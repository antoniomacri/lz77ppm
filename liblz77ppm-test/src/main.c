#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <lz77ppm/lz77.h>

#include "assertions.h"

//#define VERBOSE

int WINDOW_SIZE = 1 << 9;
int BUFFER_SIZE = 1 << 5;

/*
 * Testing with an input size up to the size of window plus that of the
 * look-ahead buffer should be sufficient to cover the most interesting cases.
 */
#ifndef TEST_MAX_INPUT_SIZE
#define TEST_MAX_INPUT_SIZE (WINDOW_SIZE + 2*BUFFER_SIZE + 2)
#endif

clock_t start;
unsigned long total_time_compression = 0;
unsigned long total_time_decompression = 0;
unsigned long test_time_compression = 0;
unsigned long test_time_decompression = 0;

unsigned long total_size_compressed = 0;
unsigned long total_size_decompressed = 0;
unsigned long test_size_compressed = 0;
unsigned long test_size_decompressed = 0;

int do_compress(lz77_ustream *original, lz77_cstream *compressed)
{
    start = clock();
    int compressed_size = lz77_compress(original, compressed);
    test_time_compression += clock() - start;
    test_size_compressed += compressed_size;
    return compressed_size;
}

int do_decompress(lz77_cstream *compressed, lz77_ustream *decompressed)
{
    start = clock();
    int decompressed_size = lz77_decompress(compressed, decompressed);
    test_time_decompression += clock() - start;
    test_size_decompressed += decompressed_size;
    return decompressed_size;
}

uint8_t get_zero(int i) {
    (void)(i);
    return 0;
}

uint8_t get_value(int i) {
    (void)(i);
    return 'a';
}

uint8_t get_random(int i) {
    (void)(i);
    return rand();
}

static int triangle = 0;

uint8_t get_triangle(int i) {
    (void)(i);
    static int cc = 1;
    return (cc <= triangle) ? cc++, 'A' + triangle : (cc = 1, 'A' + triangle++);
}

static const char *get_char_input = NULL;

uint8_t get_char(int i) {
    return get_char_input[i];
}

void test_variable_length_i(const int original_size, uint8_t (*initializer)(int))
{
    uint8_t *original = malloc(original_size);
    if (original == NULL) {
        printf("Cannot allocate %d bytes of memory.\n", original_size);
        printf("Aborting.");
        exit(-2);
    }

    for (int i = 0; i < original_size; i++) {
        original[i] = initializer(i);
    }

    char extrainfo[100];
    sprintf(extrainfo, "Original size is %d bytes", original_size);

    // Compress.

    lz77_ustream * original_stream = lz77_ustream_from_memory(
            original,
            original_size,
            WINDOW_SIZE,
            BUFFER_SIZE);

    lz77_cstream * compressed_stream = lz77_cstream_to_memory(
            original_stream,
            NULL,
            0,
            1); // can_realloc = true

    int compressed_size = do_compress(original_stream, compressed_stream);
    uint8_t *compressed = lz77_cstream_get_buffer(compressed_stream);

    // The compressed stream must contain at least the terminating token.
    assert_true(compressed_size > 0, extrainfo);
    assert_true(compressed != NULL, extrainfo);

    lz77_ustream_free(&original_stream);
    lz77_cstream_free(&compressed_stream);

    // Decompress.

    compressed_stream = lz77_cstream_from_memory(compressed, compressed_size);

    lz77_ustream * decompressed_stream = lz77_ustream_to_memory(
            compressed_stream,
            NULL,
            0,
            1); // can_realloc = true

    int decompressed_size = do_decompress(compressed_stream, decompressed_stream);
    uint8_t * decompressed = lz77_ustream_get_buffer(decompressed_stream);

    assert_true(decompressed_size >= 0, extrainfo);
    assert_true(decompressed_size == 0 || decompressed != NULL, extrainfo);

    lz77_cstream_free(&compressed_stream);
    lz77_ustream_free(&decompressed_stream);

    // Check results.
    assert_int_equal(original_size, decompressed_size, extrainfo);
    assert_n_array_equal(original, decompressed, original_size, extrainfo);

    // Cleanup.
    free(original);
    free(compressed);
    free(decompressed);
}

void test_variable_length(uint8_t (*initializer)(), const char *name)
{
    const int max_original_size = TEST_MAX_INPUT_SIZE;

    printf("\nTesting with variable length using initializer '%s' (up to %d bytes)...\n",
            name, max_original_size);

    int percent = -1;
    for (int i = 0; i <= max_original_size; i++) {
        test_variable_length_i(i, initializer);

        int p = i * 100 / max_original_size;
        if (p % 10 == 0 && p > percent) {
            percent = p;
            printf(" %d%%...\n", percent);
        }
    }
}

void test_variable_length_zero()
{
    test_variable_length(get_zero, "get_zero");
}

void test_variable_length_value()
{
    test_variable_length(get_value, "get_value");
}

void test_variable_length_random()
{
    test_variable_length(get_random, "get_random");
}

void test_static_alloc_i(const int original_size, const int decompressed_maxsize)
{
    uint8_t *original = malloc(original_size);
    if (original == NULL) {
        printf("Cannot allocate %d bytes of memory.\n", original_size);
        printf("Aborting.");
        exit(-2);
    }

    for (int i = 0; i < original_size; i++) {
        original[i] = get_random(i);
    }

    char extrainfo[100];
    sprintf(extrainfo, "Original size is %d bytes", original_size);

    // Compress.

    lz77_ustream * original_stream = lz77_ustream_from_memory(
            original,
            original_size,
            WINDOW_SIZE,
            BUFFER_SIZE);

    lz77_cstream * compressed_stream = lz77_cstream_to_memory(
            original_stream,
            NULL,
            0,
            1); // can_realloc = true

    int compressed_size = do_compress(original_stream, compressed_stream);
    uint8_t *compressed = lz77_cstream_get_buffer(compressed_stream);

    // The compressed stream must contain at least the terminating token.
    assert_true(compressed_size > 0, extrainfo);
    assert_true(compressed != NULL, extrainfo);

    lz77_ustream_free(&original_stream);
    lz77_cstream_free(&compressed_stream);

    // Decompress.

    uint8_t decompressed[decompressed_maxsize];

    compressed_stream = lz77_cstream_from_memory(compressed, compressed_size);

    lz77_ustream * decompressed_stream = lz77_ustream_to_memory(
            compressed_stream,
            decompressed,
            decompressed_maxsize,
            0); // can_realloc = false

    int decompressed_size = do_decompress(compressed_stream, decompressed_stream);

    lz77_cstream_free(&compressed_stream);
    lz77_ustream_free(&decompressed_stream);

    // Check results.
    if (decompressed_size < 0) {
        assert_true(errno == ENOMEM, extrainfo);
    }
    else {
        // Check no overflow occurred.
        assert_true(decompressed_size <= decompressed_maxsize, extrainfo);

        assert_int_equal(original_size, decompressed_size, extrainfo);
        assert_n_array_equal(original, decompressed, original_size, extrainfo);
    }

    // Cleanup.
    free(original);
    free(compressed);
}

void test_static_alloc()
{
    const int max_original_size = TEST_MAX_INPUT_SIZE;

    printf("\nTesting with static allocation (up to %d bytes)...\n", max_original_size);

    int percent = -1;
    for (int i = 0; i <= max_original_size; i++) {
        test_static_alloc_i(i, max_original_size / 2);

        int p = i * 100 / max_original_size;
        if (p % 10 == 0 && p > percent) {
            percent = p;
            printf(" %d%%...\n", percent);
        }
    }
}

void test_compress_from_file_i(const int original_size)
{
    uint8_t *original = malloc(original_size);
    if (original == NULL) {
        printf("Cannot allocate %d bytes of memory.\n", original_size);
        printf("Aborting.");
        exit(-2);
    }

    for (int i = 0; i < original_size; i++) {
        original[i] = get_random(i);
    }

    char extrainfo[100];
    sprintf(extrainfo, "Original size is %d bytes", original_size);

    // Compress.

    int fd_input = open("/tmp/temp-input.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd_input < 0) {
        perror("Cannot create input file");
        exit(-2);
    }
    if (write(fd_input, original, original_size) != original_size) {
        perror("Cannot write data to input file");
        close(fd_input);
        exit(-2);
    }
    if (lseek(fd_input, SEEK_SET, 0) != 0) {
        perror("Cannot seek at the beginning of the input file");
        close(fd_input);
        exit(-2);
    }

    lz77_ustream * original_stream = lz77_ustream_from_descriptor(
            fd_input,
            WINDOW_SIZE,
            BUFFER_SIZE);

    lz77_cstream * compressed_stream = lz77_cstream_to_memory(
            original_stream,
            NULL,
            0,
            1); // can_realloc = true

    int compressed_size = do_compress(original_stream, compressed_stream);
    uint8_t *compressed = lz77_cstream_get_buffer(compressed_stream);

    // The compressed stream must contain at least the terminating token.
    assert_true(compressed_size > 0, extrainfo);
    assert_true(compressed != NULL, extrainfo);

    lz77_ustream_free(&original_stream);
    lz77_cstream_free(&compressed_stream);

    close(fd_input);

    // Decompress.

    compressed_stream = lz77_cstream_from_memory(compressed, compressed_size);

    lz77_ustream * decompressed_stream = lz77_ustream_to_memory(
            compressed_stream,
            NULL,
            0,
            1); // can_realloc = true

    int decompressed_size = do_decompress(compressed_stream, decompressed_stream);
    uint8_t * decompressed = lz77_ustream_get_buffer(decompressed_stream);

    assert_true(decompressed_size >= 0, extrainfo);
    assert_true(decompressed_size == 0 || decompressed != NULL, extrainfo);

    lz77_cstream_free(&compressed_stream);
    lz77_ustream_free(&decompressed_stream);

    // Check results.
    assert_int_equal(original_size, decompressed_size, extrainfo);
    assert_n_array_equal(original, decompressed, original_size, extrainfo);

    // Cleanup.
    free(original);
    free(compressed);
    free(decompressed);
}

void test_compress_from_file()
{
    const int max_original_size = TEST_MAX_INPUT_SIZE;

    printf("\nTesting with compression input from file (up to %d bytes)...\n", max_original_size);

    int percent = -1;
    for (int i = 0; i <= max_original_size; i++) {
        test_compress_from_file_i(i);

        int p = i * 100 / max_original_size;
        if (p % 10 == 0 && p > percent) {
            percent = p;
            printf(" %d%%...\n", percent);
        }
    }
}

void test_decompress_to_file_i(const int original_size)
{
    uint8_t *original = malloc(original_size);
    if (original == NULL) {
        printf("Cannot allocate %d bytes of memory.\n", original_size);
        printf("Aborting.");
        exit(-2);
    }

    for (int i = 0; i < original_size; i++) {
        original[i] = get_random(i);
    }

    char extrainfo[100];
    sprintf(extrainfo, "Original size is %d bytes", original_size);

    // Compress.

    lz77_ustream * original_stream = lz77_ustream_from_memory(
            original,
            original_size,
            WINDOW_SIZE,
            BUFFER_SIZE);

    lz77_cstream * compressed_stream = lz77_cstream_to_memory(
            original_stream,
            NULL,
            0,
            1); // can_realloc = true

    int compressed_size = do_compress(original_stream, compressed_stream);
    uint8_t *compressed = lz77_cstream_get_buffer(compressed_stream);

    // The compressed stream must contain at least the terminating token.
    assert_true(compressed_size > 0, extrainfo);
    assert_true(compressed != NULL, extrainfo);

    lz77_ustream_free(&original_stream);
    lz77_cstream_free(&compressed_stream);

    // Decompress.

    compressed_stream = lz77_cstream_from_memory(compressed, compressed_size);

    int fd_decompressed = open("/tmp/temp-decompressed.txt",
            0 | O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd_decompressed < 0) {
        perror("Cannot create decompressed file");
        exit(-2);
    }

    lz77_ustream * decompressed_stream = lz77_ustream_to_descriptor(
            compressed_stream, fd_decompressed);

    int decompressed_size = do_decompress(compressed_stream, decompressed_stream);

    assert_true(decompressed_size >= 0, extrainfo);

    lz77_cstream_free(&compressed_stream);
    lz77_ustream_free(&decompressed_stream);

    // Check results.
    assert_int_equal(original_size, decompressed_size, extrainfo);

    // Check result is the same as the input.
    if (lseek(fd_decompressed, SEEK_SET, 0) != 0) {
        perror("Cannot seek at the beginning of the decompressed file");
        exit(-2);
    }
    int pos = 0;
    for (uint8_t c; read(fd_decompressed, &c, 1) > 0; pos++) {
        assert_true(c == original[pos], extrainfo);
    }
    assert_int_equal(pos, decompressed_size, extrainfo);

    // Cleanup.
    free(original);
    free(compressed);
    // Close the file after having compared original and decompressed streams.
    close(fd_decompressed);
}

void test_decompress_to_file()
{
    const int max_original_size = TEST_MAX_INPUT_SIZE;

    printf("\nTesting with decompression output to file (up to %d bytes)...\n", max_original_size);

    int percent = -1;
    for (int i = 0; i <= max_original_size; i++) {
        test_decompress_to_file_i(i);

        int p = i * 100 / max_original_size;
        if (p % 10 == 0 && p > percent) {
            percent = p;
            printf(" %d%%...\n", percent);
        }
    }
}

void test_compress_to_file_i(const int original_size)
{
    uint8_t *original = malloc(original_size);
    if (original == NULL) {
        printf("Cannot allocate %d bytes of memory.\n", original_size);
        printf("Aborting.");
        exit(-2);
    }

    for (int i = 0; i < original_size; i++) {
        original[i] = get_random(i);
    }

    char extrainfo[100];
    sprintf(extrainfo, "Original size is %d bytes", original_size);

    // Compress.

    lz77_ustream * original_stream = lz77_ustream_from_memory(
            original,
            original_size,
            WINDOW_SIZE,
            BUFFER_SIZE);

    int fd_compressed = open("/tmp/temp-compressed.txt",
            0 | O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd_compressed < 0) {
        perror("Cannot create compressed file");
        exit(-2);
    }

    lz77_cstream * compressed_stream = lz77_cstream_to_descriptor(
            original_stream,
            fd_compressed);

    int compressed_size = do_compress(original_stream, compressed_stream);

    lz77_ustream_free(&original_stream);
    lz77_cstream_free(&compressed_stream);

    // Check that the output size equals the file's size.
    struct stat stat;
    fstat(fd_compressed, &stat);
    assert_int_equal(stat.st_size, compressed_size, extrainfo);

    // Decompress.

    if (lseek(fd_compressed, SEEK_SET, 0) != 0) {
        perror("Cannot seek at the beginning of the compressed file");
        exit(-2);
    }

    compressed_stream = lz77_cstream_from_descriptor(fd_compressed);

    lz77_ustream * decompressed_stream = lz77_ustream_to_memory(
            compressed_stream,
            NULL,
            0,
            1); // can_realloc = true

    int decompressed_size = do_decompress(compressed_stream, decompressed_stream);
    uint8_t * decompressed = lz77_ustream_get_buffer(decompressed_stream);

    assert_true(decompressed_size >= 0, extrainfo);
    assert_true(decompressed_size == 0 || decompressed != NULL, extrainfo);

    lz77_cstream_free(&compressed_stream);
    lz77_ustream_free(&decompressed_stream);

    // Check results.
    assert_int_equal(original_size, decompressed_size, extrainfo);
    assert_n_array_equal(original, decompressed, original_size, extrainfo);

    // Cleanup.
    free(original);
    free(decompressed);
    close(fd_compressed);
}

void test_compress_to_file()
{
    const int max_original_size = TEST_MAX_INPUT_SIZE;

    printf("\nTesting with compression output to file (up to %d bytes)...\n", max_original_size);

    int percent = -1;
    for (int i = 0; i <= max_original_size; i++) {
        test_compress_to_file_i(i);

        int p = i * 100 / max_original_size;
        if (p % 10 == 0 && p > percent) {
            percent = p;
            printf(" %d%%...\n", percent);
        }
    }
}

void test_decompress_from_file_i(const int original_size)
{
    uint8_t *original = malloc(original_size);
    if (original == NULL) {
        printf("Cannot allocate %d bytes of memory.\n", original_size);
        printf("Aborting.");
        exit(-2);
    }

    for (int i = 0; i < original_size; i++) {
        original[i] = get_random(i);
    }

    char extrainfo[100];
    sprintf(extrainfo, "Original size is %d bytes", original_size);

    // Compress.

    lz77_ustream * original_stream = lz77_ustream_from_memory(
            original,
            original_size,
            WINDOW_SIZE,
            BUFFER_SIZE);

    int fd_compressed = open("/tmp/temp-compressed.txt",
            0 | O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd_compressed < 0) {
        perror("Cannot create compressed file");
        exit(-2);
    }

    lz77_cstream * compressed_stream = lz77_cstream_to_descriptor(
            original_stream,
            fd_compressed);

    int compressed_size = do_compress(original_stream, compressed_stream);

    lz77_ustream_free(&original_stream);
    lz77_cstream_free(&compressed_stream);

    // Check that the output size equals the file's size.
    struct stat stat;
    fstat(fd_compressed, &stat);
    assert_int_equal(stat.st_size, compressed_size, extrainfo);

    // Decompress.

    if (lseek(fd_compressed, SEEK_SET, 0) != 0) {
        perror("Cannot seek at the beginning of the compressed file");
        exit(-2);
    }

    compressed_stream = lz77_cstream_from_descriptor(fd_compressed);

    lz77_ustream * decompressed_stream = lz77_ustream_to_memory(
            compressed_stream,
            NULL,
            0,
            1); // can_realloc = true

    int decompressed_size = do_decompress(compressed_stream, decompressed_stream);
    uint8_t * decompressed = lz77_ustream_get_buffer(decompressed_stream);

    assert_true(decompressed_size >= 0, extrainfo);
    assert_true(decompressed_size == 0 || decompressed != NULL, extrainfo);

    lz77_cstream_free(&compressed_stream);
    lz77_ustream_free(&decompressed_stream);

    // Check results.
    assert_int_equal(original_size, decompressed_size, extrainfo);
    assert_n_array_equal(original, decompressed, original_size, extrainfo);

    // Cleanup.
    free(original);
    free(decompressed);
    close(fd_compressed);
}

void test_decompress_from_file()
{
    const int max_original_size = TEST_MAX_INPUT_SIZE;

    printf("\nTesting with decompression input from file (up to %d bytes)...\n", max_original_size);

    int percent = -1;
    for (int i = 0; i <= max_original_size; i++) {
        test_decompress_from_file_i(i);

        int p = i * 100 / max_original_size;
        if (p % 10 == 0 && p > percent) {
            percent = p;
            printf(" %d%%...\n", percent);
        }
    }
}

void test_ustream_fill_buffer()
{
    const int half_count = BUFFER_SIZE + 1;
    // This 10 is the factor used by ustream to allocate the internal buffer.
    const int min_original_size = WINDOW_SIZE * 10 - half_count;

    printf("\nTest filling ustream's buffer (from %d to %d bytes of data)...\n",
            min_original_size, min_original_size + 2 * half_count);

    int percent = -1;
    for (int i = 0; i <= 2 * half_count; i++) {
        test_decompress_to_file_i(min_original_size + i);

        int p = i * 100 / (2 * half_count);
        if (p % 10 == 0 && p > percent) {
            percent = p;
            printf(" %d%%...\n", percent);
        }
    }
}

void test_variable_lookahead()
{
    const int max_original_size = TEST_MAX_INPUT_SIZE;

    printf("\nTesting with look-ahead of size %d bytes (up to %d bytes of data)...\n",
            BUFFER_SIZE, max_original_size);

    int percent = -1;
    for (int i = 0; i <= max_original_size; i++) {
        triangle = 0;
        test_variable_length_i(i, get_triangle);

        int p = i * 100 / max_original_size;
        if (p % 10 == 0 && p > percent) {
            percent = p;
            printf(" %d%%...\n", percent);
        }
    }
}

void run_test(void (*test)(void))
{
    test_size_compressed = test_size_decompressed = 0;
    test_time_compression = test_time_decompression = 0;

    test();

    printf("Compression ratio (greater is better):  %.3lf\n",
            test_size_decompressed / (double)test_size_compressed);
    printf("Seconds taken by compression:           %.3lf\n",
            test_time_compression / (double)CLOCKS_PER_SEC);
    printf("Seconds taken by decompression:         %.3lf\n",
            test_time_decompression / (double)CLOCKS_PER_SEC);

    total_time_compression += test_time_compression;
    total_time_decompression += test_time_decompression;
    total_size_compressed += test_size_compressed;
    total_size_decompressed += test_size_decompressed;
}

void test_explicit_i(const char *input, int window_size, int buffer_size)
{
    int WINDOW_SIZE_saved = WINDOW_SIZE;
    int BUFFER_SIZE_saved = BUFFER_SIZE;
    WINDOW_SIZE = window_size;
    BUFFER_SIZE = buffer_size;

    printf(" %s (window: %d, lookahead: %d)\n", input, window_size, buffer_size);
    test_variable_length_i(strlen(get_char_input = input), get_char);

    WINDOW_SIZE = WINDOW_SIZE_saved;
    BUFFER_SIZE = BUFFER_SIZE_saved;
}

void test_explicit()
{
    printf("\nTesting with with explicit inputs...\n");

    test_explicit_i("BBAAABBC", 4, 2);
    test_explicit_i("BAAABBCA", 4, 2);
    test_explicit_i("AAABBCAB", 4, 2);
    test_explicit_i("YAZABCDEFGHI", 8, 4);
}

int main()
{
    run_test(test_variable_length_zero);

    run_test(test_variable_length_value);

    run_test(test_variable_length_random);

    run_test(test_static_alloc);

    run_test(test_compress_from_file);

    run_test(test_decompress_to_file);

    run_test(test_compress_to_file);

    run_test(test_decompress_from_file);

    run_test(test_ustream_fill_buffer);

    int BUFFER_SIZE_saved = BUFFER_SIZE;
    int WINDOW_SIZE_saved = WINDOW_SIZE;

    for (int i = LZ77_MIN_LOOKAHEAD_SIZE; i <= 24; i++) {
        BUFFER_SIZE = i;
        run_test(test_variable_lookahead);
    }

    BUFFER_SIZE = LZ77_MIN_LOOKAHEAD_SIZE;
    for (int i = 0; i < 16; i++) {
        WINDOW_SIZE = BUFFER_SIZE + i;
        if (WINDOW_SIZE < LZ77_MIN_WINDOW_SIZE) {
            WINDOW_SIZE = LZ77_MIN_WINDOW_SIZE;
        }
        run_test(test_variable_lookahead);
    }

    BUFFER_SIZE = BUFFER_SIZE_saved;
    WINDOW_SIZE = WINDOW_SIZE_saved;

    run_test(test_explicit);

    printf("\n\n");
    printf("Total seconds taken by compression:   %.3lf\n",
            total_time_compression / (double)CLOCKS_PER_SEC);
    printf("Total seconds taken by decompression: %.3lf\n",
            total_time_decompression / (double)CLOCKS_PER_SEC);

    return 0;
}

