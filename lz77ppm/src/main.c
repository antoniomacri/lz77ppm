/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

/**
 * @file main.c
 *
 * A command line interface to the lz77ppm library.
 *
 * @author Antonio Macrì
 */

#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <lz77ppm/lz77.h>

#define PROGRAM_VERSION "1.0"

#define DEFAULT_WINDOW_SIZE    4096
#define DEFAULT_LOOKAHEAD_SIZE 32

#define XSTR(a) STR(a)
#define STR(a) #a

static struct option long_options[] = {
    { "compress", no_argument, 0, 'c' },
    { "decompress", no_argument, 0, 'd' },
    { "window-size", required_argument, 0, 'w' },
    { "lookahead-size", required_argument, 0, 'l' },
    { "output", required_argument, 0, 'o' },
    { "force", no_argument, 0, 'f' },
    { "summary", no_argument, 0, 's' },
    { "stats", no_argument, 0, 't' },
    { "help", no_argument, 0, 'h' },
    { "version", no_argument, 0, 'V' },
    { 0, 0, 0, 0 }
};

static struct {
    const char *desc;
    const char *deflt;
} long_options_ex[] = {
    { "Compress a file", NULL },
    { "Decompress a file", NULL },
    { "Specify the size of the window", XSTR(DEFAULT_WINDOW_SIZE) },
    { "Specify the size of the look-ahead buffer", XSTR(DEFAULT_LOOKAHEAD_SIZE) },
    { "Specify the filename of the output file", NULL },
    { "Force overwrite of the output file if it already exists", NULL },
    { "Show a summary of the operation that will be performed", NULL },
    { "Show statistics after the operation is completed", NULL },
    { "Show this help", NULL },
    { "Show the version", NULL },
};

static void show_version(const char *program)
{
    printf("%s: v%s (library %d.%d)\n",
            program, PROGRAM_VERSION, (LZ77PPM_VERSION >> 4) & 0xF, LZ77PPM_VERSION & 0xF);
    printf("Written by Antonio Macrì <ing.antonio.macri@gmail.com>.\n");
}

static void usage(const char *program)
{
    printf("Compress or decompress a file using the LZ77 algorithm.\n\n");
    printf("Usage:\n");
    printf("  %s [-c | -d] [options] [-o output-file] INPUTFILE\n", program);
    printf("\nIf the -o option is not used, the result is sent to the standard output.\n"
            "If the input file is not specified, the standard input is used.\n");
    printf("\nOptions:\n");
    int width = 0;
    for (unsigned i = 0; long_options[i].name != NULL; i++) {
        int len = strlen(long_options[i].name);
        if (width < len) {
            width = len;
        }
    }
    for (unsigned i = 0; long_options[i].name != NULL; i++) {
        struct option *o = &long_options[i];
        printf("  -%c, --%-*s  %s", o->val, width, o->name, long_options_ex[i].desc);
        if (long_options_ex[i].deflt != NULL) {
            printf(" (default %s)", long_options_ex[i].deflt);
        }
        printf("\n");
    }
    printf("\nExamples:\n");
    printf("  %s text.txt\n", program);
    printf("    Compress the file text.txt to stdout\n");
    printf("  %s -sc input.txt > output.lz\n", program);
    printf("    Compress the file input.txt to output.lz\n");
    printf("  %s -c input.txt -w 1024 -l 64 -o output.lz\n", program);
    printf("    Compress the file input.txt to output.lz using the given "
            "window and look-ahead buffer sizes\n");

    printf("\n");
    show_version(program);
}

static const char *print_size(int64_t bytes)
{
    static char str[100];
    double unit = 1024;
    static const char prefix[] = "KMGTPE";
    if (bytes < unit) {
        sprintf(str, "%d B", (int)bytes);
    } else {
        unsigned int exp = (unsigned int)(log10((double)bytes) / log10(unit));
        if (exp >= sizeof(prefix)) {
            exp = sizeof(prefix) - 1;
        }
        sprintf(str, "%.1lf %ciB", bytes / pow(unit, exp), prefix[exp - 1]);
    }
    return str;
}

static const char *print_time(double seconds)
{
    static char str[100];
    if (seconds <= 60) {
        sprintf(str, "%.2lfs", seconds);
    } else {
        sprintf(str, "%dm %ds", (int)seconds / 60, (int)seconds % 60);
    }
    return str;
}

static unsigned long int timeval_millis(const struct timeval *start, const struct timeval *end)
{
    return (end->tv_usec / 1000 + 1000 * end->tv_sec)
            - (start->tv_usec / 1000 + 1000 * start->tv_sec);
}

int64_t do_compress(const char *input_filename,
                    const char *output_filename,
                    int window_size,
                    int lookahead_size,
                    int overwrite_output)
{
    int fd_input;
    if (input_filename == NULL) {
        fd_input = STDIN_FILENO;
    } else {
        fd_input = open(input_filename, O_RDONLY, S_IRUSR);
    }

    if (fd_input < 0) {
        perror("Cannot open input file");
        exit(-2);
    }

    int fd_output;
    if (output_filename == NULL) {
        fd_output = STDOUT_FILENO;
    } else {
        int oflag = O_WRONLY | O_CREAT | (overwrite_output ? O_TRUNC : O_EXCL);
        fd_output = open(output_filename, oflag, 0644);
    }

    if (fd_output < 0) {
        perror("Cannot open output file");
        close(fd_input);
        exit(-2);
    }

    lz77_ustream * original_stream = lz77_ustream_from_descriptor(
            fd_input, window_size, lookahead_size);
    if (original_stream == NULL) {
        close(fd_input);
        close(fd_output);
        return -1;
    }

    lz77_cstream * compressed_stream = lz77_cstream_to_descriptor(original_stream, fd_output);
    if (compressed_stream == NULL) {
        lz77_ustream_free(&original_stream);
        close(fd_input);
        close(fd_output);
        return -1;
    }

    int64_t result_size = lz77_compress(original_stream, compressed_stream);

    lz77_ustream_free(&original_stream);
    lz77_cstream_free(&compressed_stream);
    close(fd_input);
    close(fd_output);

    return result_size;
}

int64_t do_decompress(const char *input_filename, const char *output_filename, int overwrite_output)
{
    int fd_input;
    if (input_filename == NULL) {
        fd_input = STDIN_FILENO;
    } else {
        fd_input = open(input_filename, O_RDONLY, S_IRUSR);
    }

    if (fd_input < 0) {
        perror("Cannot open input file");
        exit(-2);
    }

    int fd_output;
    if (output_filename == NULL) {
        fd_output = STDOUT_FILENO;
    } else {
        int oflag = O_WRONLY | O_CREAT | (overwrite_output ? O_TRUNC : O_EXCL);
        fd_output = open(output_filename, oflag, 0644);
    }

    if (fd_output < 0) {
        perror("Cannot open output file");
        close(fd_input);
        exit(-2);
    }

    lz77_cstream * compressed_stream = lz77_cstream_from_descriptor(fd_input);
    if (compressed_stream == NULL) {
        close(fd_input);
        close(fd_output);
        return -1;
    }

    lz77_ustream * decompressed_stream = lz77_ustream_to_descriptor(compressed_stream, fd_output);
    if (decompressed_stream == NULL) {
        lz77_cstream_free(&compressed_stream);
        close(fd_input);
        close(fd_output);
        return -1;
    }

    int64_t result_size = lz77_decompress(compressed_stream, decompressed_stream);

    lz77_cstream_free(&compressed_stream);
    lz77_ustream_free(&decompressed_stream);
    close(fd_input);
    close(fd_output);

    return result_size;
}

static struct timeval start;

static void cli_report_progress(lz77_ustream *ustream, lz77_cstream *cstream, float percent)
{
    static int last = -1;

    // Avoid unused-parameter warning.
    (void)(ustream);
    (void)(cstream);

    if ((int)percent != last) {
        last = percent;

        struct timeval end;
        gettimeofday(&end, NULL);
        double elapsed_time = timeval_millis(&start, &end) / 1000.0;
        double remaining_time = (100.0 - percent) / percent * elapsed_time;

        fprintf(stderr, "\rProgress %d%% (remaining %s)...    \b\b\b",
                last, last <= 0 ? "unknown" : print_time(remaining_time));
    }
}

int main(int argc, char* argv[])
{
    int decompress = 0;
    const char *input_filename = NULL;
    const char *output_filename = NULL;
    uint16_t window_size = DEFAULT_WINDOW_SIZE;
    uint16_t lookahead_size = DEFAULT_LOOKAHEAD_SIZE;
    int force_overwrite = 0;
    int show_summary = 0;
    int show_statistics = 0;

    int c;
    while ((c = getopt_long(argc, argv, "cdw:l:o:fsthV", long_options, 0)) >= 0)
    {
        switch (c) {
            case 'c':
                decompress = 0;
                break;
            case 'd':
                decompress = 1;
                break;
            case 'w': {
                unsigned long int w = strtoul(optarg, NULL, 10);
                if (w >= (1 << sizeof(window_size) * 8)) {
                    fprintf(stderr, "Window size too large (%lu)!\n", w);
                    return -1;
                }
                window_size = w;
                break;
            }
            case 'l': {
                unsigned long int l = strtoul(optarg, NULL, 10);
                if (l >= (1 << sizeof(window_size) * 8)) {
                    fprintf(stderr, "Look-ahead size too large (%lu)!\n", l);
                    return -1;
                }
                lookahead_size = l;
                break;
            }
            case 'o':
                output_filename = optarg;
                break;
            case 'f':
                force_overwrite = 1;
                break;
            case 's':
                show_summary = 1;
                report_progress = cli_report_progress;
                break;
            case 't':
                show_statistics = 1;
                report_progress = cli_report_progress;
                break;
            case 'h':
                usage(argv[0]);
                return -1;
            case 'V':
                show_version(argv[0]);
                return -1;
            case '?':
                fprintf(stderr, "Use option -h to show help.\n");
                return -1;
        }
    }
    if (optind < argc - 1) {
        fprintf(stderr, "Too many files specified!\n");
        return -1;
    }
    if (optind == argc - 1) {
        input_filename = argv[optind];
    }

    int64_t output_size;
    if (!decompress) {
        if (show_summary) {
            fprintf(stderr, "Compression:\n");
            fprintf(stderr, "  Input file:      %s\n",
                    input_filename ? input_filename : "(standard input)");
            fprintf(stderr, "  Output file:     %s\n",
                    output_filename ? output_filename : "(standard output)");
            fprintf(stderr, "  Window size:     %d bytes\n", window_size);
            fprintf(stderr, "  Look-ahead size: %d bytes\n", lookahead_size);
        }

        struct timeval end;
        gettimeofday(&start, NULL);
        output_size = do_compress(input_filename, output_filename,
                window_size, lookahead_size, force_overwrite);
        gettimeofday(&end, NULL);

        if (show_summary) {
            fprintf(stderr, "Compression %s.\n", output_size <= 0 ? "failed" : "done");
        }
        if (output_size > 0 && show_statistics) {
            struct stat st;
            stat(input_filename, &st);
            uint64_t input_size = st.st_size;
            double compression_ratio = input_size / (double)output_size;
            double elapsed_time = timeval_millis(&start, &end) / 1000.0;
            fprintf(stderr, "\nStatistics:\n");
            fprintf(stderr, "  Input file size:   %s\n", print_size(input_size));
            fprintf(stderr, "  Output file size:  %s\n", print_size(output_size));
            fprintf(stderr, "  Compression ratio: %.2lf (%.1lf%%)\n",
                    compression_ratio, 100 / compression_ratio);
            fprintf(stderr, "  Elapsed time:      %s\n", print_time(elapsed_time));
            fprintf(stderr, "  Data rate:         %s/s\n", print_size(input_size / elapsed_time));
        }
    }
    else {
        if (show_summary) {
            fprintf(stderr, "Decompression:\n");
            fprintf(stderr, "  Input file:  %s\n",
                    input_filename ? input_filename : "(standard input)");
            fprintf(stderr, "  Output file: %s\n",
                    output_filename ? output_filename : "(standard output)");
        }

        struct timeval end;
        gettimeofday(&start, NULL);
        output_size = do_decompress(input_filename, output_filename, force_overwrite);
        gettimeofday(&end, NULL);

        if (show_summary) {
            fprintf(stderr, "Decompression %s.\n", output_size <= 0 ? "failed" : "done");
        }
        if (output_size > 0 && show_statistics) {
            struct stat st;
            stat(input_filename, &st);
            uint64_t input_size = st.st_size;
            double compression_ratio = output_size / (double)input_size;
            double elapsed_time = timeval_millis(&start, &end) / 1000.0;
            fprintf(stderr, "\nStatistics:\n");
            fprintf(stderr, "  Input file size:   %s\n", print_size(input_size));
            fprintf(stderr, "  Output file size:  %s\n", print_size(output_size));
            fprintf(stderr, "  Compression ratio: %.2lf (%.1lf%%)\n",
                    compression_ratio, 100 / compression_ratio);
            fprintf(stderr, "  Elapsed time:      %s\n", print_time(elapsed_time));
            fprintf(stderr, "  Data rate:         %s/s\n", print_size(input_size / elapsed_time));
        }
    }

    return output_size > 0 ? 0 : output_size;
}
