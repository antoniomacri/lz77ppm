/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lz77ppm/lz77.h>
#include <lz77ppm/logger.h>

#include <ustream_internal.h>
#include <cstream_internal.h>

static uint8_t number_of_bits(uint16_t value);
static void rotate_tree_array(lz77_tree v[], int size, int shift);
static void shift_tree_indices(lz77_tree v[], int size, int shift);

uint8_t * lz77_ustream_get_buffer(lz77_ustream *ustream)
{
    if (ustream == NULL) {
        lz77_log(LOG_ERROR, "Argument `ustream' must not be NULL");
        errno = EINVAL;
        return NULL;
    }

    if (ustream->fd < 0) {
        return ustream->data;
    } else {
        return NULL;
    }
}

lz77_ustream * lz77_ustream_from_memory(const uint8_t *data,
                                        uint32_t size,
                                        uint16_t window_size,
                                        uint16_t lookahead_size)
{
    if (data == NULL) {
        lz77_log(LOG_ERROR, "Argument `data' must not be NULL");
        errno = EINVAL;
        return NULL;
    }
    if (window_size < LZ77_MIN_WINDOW_SIZE) {
        lz77_log(LOG_ERROR,
                "The window size cannot be less then %d (given %d)",
                LZ77_MIN_WINDOW_SIZE, window_size);
        errno = EINVAL;
        return NULL;
    }
    if (lookahead_size < LZ77_MIN_LOOKAHEAD_SIZE) {
        lz77_log(LOG_ERROR,
                "The look-ahead buffer size cannot be less then %d (given %d)",
                LZ77_MIN_LOOKAHEAD_SIZE, lookahead_size);
        errno = EINVAL;
        return NULL;
    }

    lz77_ustream *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        object->tree = malloc((window_size + 1) * sizeof(*object->tree));
        if (object->tree == NULL) {
            free(object);
            return NULL;
        }
        object->fd = -1;
        object->cdata = data;
        object->size = size;
        object->end = size;
        object->is_input = 1;
        object->window = data;
        object->window_maxsize = window_size;
        object->window_nbits = number_of_bits(window_size - 1);
        object->lookahead = data;
        object->lookahead_maxsize = lookahead_size;
        object->length_encoder = calloc(1, sizeof(*object->length_encoder));
        if (object->length_encoder == NULL) {
            free(object->tree);
            free(object);
            return NULL;
        }
    }
    return object;
}

lz77_ustream * lz77_ustream_from_descriptor(int fd, uint16_t window_size, uint16_t lookahead_size)
{
    if (fd < 0) {
        lz77_log(LOG_ERROR, "The file descriptor is not valid");
        errno = EINVAL;
        return NULL;
    }
    if (window_size < LZ77_MIN_WINDOW_SIZE) {
        lz77_log(LOG_ERROR,
                "The window size cannot be less then %d (given %d)",
                LZ77_MIN_WINDOW_SIZE, window_size);
        errno = EINVAL;
        return NULL;
    }
    if (lookahead_size < LZ77_MIN_LOOKAHEAD_SIZE) {
        lz77_log(LOG_ERROR,
                "The look-ahead buffer size cannot be less then %d (given %d)",
                LZ77_MIN_LOOKAHEAD_SIZE, lookahead_size);
        errno = EINVAL;
        return NULL;
    }

    lz77_ustream *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        // data_size must be at least (window_size + lookahead_size).
        // If it is set exactly to (window_size + lookahead_size), data is read
        // from the descriptor few bytes at a time (even one byte at a time),
        // just as bytes are consumed by the compressing algorithm and leave
        // space in the buffer. Increase this size to achieve a better
        // performance.
        int data_size = (window_size + lookahead_size) * 10;
        uint8_t * data = malloc(data_size);
        if (data == NULL) {
            free(object);
            return NULL;
        }
        object->tree = malloc((window_size + 1) * sizeof(*object->tree));
        if (object->tree == NULL) {
            free(data);
            free(object);
            return NULL;
        }
        object->fd = fd;
        object->cdata = object->data = data;
        object->can_realloc = 1;
        object->size = data_size;
        object->is_input = 1;
        object->window = data;
        object->window_maxsize = window_size;
        object->window_nbits = number_of_bits(window_size - 1);
        object->lookahead = data;
        object->lookahead_maxsize = lookahead_size;
        object->length_encoder = calloc(1, sizeof(*object->length_encoder));
        if (object->length_encoder == NULL) {
            free(object->tree);
            free(data);
            free(object);
            return NULL;
        }
    }
    return object;
}

lz77_ustream * lz77_ustream_to_memory(lz77_cstream *from,
                                      uint8_t *data,
                                      uint32_t size,
                                      uint8_t can_realloc)
{
    if (from == NULL) {
        lz77_log(LOG_ERROR, "Argument `from' must not be NULL");
        errno = EINVAL;
        return NULL;
    }

    lz77_ustream *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        object->fd = -1;
        object->data = data;
        object->size = data == NULL ? 0 : size;
        object->can_realloc = can_realloc;
        object->window = data;
        object->from = from;
        object->length_encoder = calloc(1, sizeof(*object->length_encoder));
    }
    return object;
}

lz77_ustream * lz77_ustream_to_descriptor(lz77_cstream *from, int fd)
{
    if (from == NULL) {
        lz77_log(LOG_ERROR, "Argument `from' must not be NULL");
        errno = EINVAL;
        return NULL;
    }
    if (fd < 0) {
        lz77_log(LOG_ERROR, "The file descriptor is not valid");
        errno = EINVAL;
        return NULL;
    }

    lz77_ustream *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        object->fd = fd;
        object->can_realloc = 1;
        object->from = from;
        object->length_encoder = calloc(1, sizeof(*object->length_encoder));
    }
    return object;
}

int ustream_open(lz77_ustream *ustream)
{
    assert(ustream != NULL);
    // Check if the stream is already opened
    assert(ustream->window_currsize == 0 && ustream->lookahead_currsize == 0);
    assert(ustream->is_input || ustream->window_nbits == 0);
    if (!ustream->is_input) {
        // If the compressed stream's sizes are not valid, maybe it is not open.
        assert(ustream->from->window_maxsize >= LZ77_MIN_WINDOW_SIZE);
        assert(ustream->from->lookahead_maxsize >= LZ77_MIN_LOOKAHEAD_SIZE);
    }

    if (ustream->is_input) {
        // Fill the look-ahead buffer.
        if (ustream->fd >= 0) {
            int readcount = read(ustream->fd, ustream->data, ustream->size);
            if (readcount < 0) {
                return -1;
            }
            ustream->end = readcount;
            ustream->lookahead_currsize = ustream->end;
        } else {
            ustream->lookahead_currsize = ustream->size;
        }
        if (ustream->lookahead_currsize > ustream->lookahead_maxsize) {
            ustream->lookahead_currsize = ustream->lookahead_maxsize;
        }
        lz77_tree_init(ustream);
    }
    else {
        assert(ustream->from != NULL);
        ustream->window_maxsize = ustream->from->window_maxsize;
        ustream->window_nbits = number_of_bits(ustream->window_maxsize - 1);
        ustream->lookahead_maxsize = ustream->from->lookahead_maxsize;
        if (ustream->fd >= 0) {
            int data_size = ustream->window_maxsize * 10;
            // When changing the previous 10, update test_ustream_fill_buffer().
            uint8_t * data = malloc(data_size);
            if (data == NULL) {
                return -1;
            }
            ustream->data = data;
            ustream->size = data_size;
            ustream->window = data;
        }
    }
    int min_match_length = LZ77_TYPE_BITS + ustream->window_nbits + LZ77_TINYHUFF_MIN_CODE_BITS;
    min_match_length = (min_match_length / LZ77_SYMBOL_BITS) + 1;
    tinyhuff_init(ustream->length_encoder, min_match_length, ustream->lookahead_maxsize);

    return 0;
}

int ustream_close(lz77_ustream *ustream)
{
    assert(ustream != NULL);

    if (ustream->fd >= 0) {
        // Flush the data buffer when in output mode.
        if (ustream->is_input == 0) {
            uint8_t * data = ustream->data;
            uint32_t count = ustream->end;
            int64_t writecount = 0;
            while (writecount != count) {
                writecount = write(ustream->fd, data, count);
                if (writecount < 0) {
                    return -1;
                }
                data += writecount;
                count -= writecount;
            }
            ustream->end = 0;
        }
    }

    return 0;
}

void lz77_ustream_free(lz77_ustream **pustream)
{
    assert(pustream != NULL);

    lz77_ustream *ustream = *pustream;
    if (ustream == NULL) {
        return;
    }

    if (ustream->fd >= 0) {
        // Release the memory of the internal buffer.
        assert(ustream->can_realloc != 0);
        free(ustream->data);
        ustream->cdata = ustream->data = NULL;
    }
    free(ustream->tree);
    ustream->tree = NULL;
    free(ustream->length_encoder);
    ustream->length_encoder = NULL;
    free(ustream);

    *pustream = NULL;
}

int ustream_find_and_advance(lz77_ustream * ustream,
                             uint16_t *offset,
                             uint16_t *length,
                             uint8_t *next)
{
    assert(ustream != NULL);
    assert(offset != NULL);
    assert(length != NULL);
    assert(next != NULL);

    if (ustream->lookahead_currsize == 0) {
        // We reached EOF.
        return 0;
    }

    if (ustream->window_currsize == 0) {
        // Initialize the tree by adding the first symbol as the right child of the root.
        ustream->tree[ustream->window_maxsize].larger = 0;
        ustream->tree[0].parent = ustream->window_maxsize;
        ustream->tree[0].larger = UNUSED;
        ustream->tree[0].smaller = UNUSED;
        for (int i = 1; i < ustream->window_maxsize; i++) {
            ustream->tree[i].parent = UNUSED;
            ustream->tree[i].larger = UNUSED;
            ustream->tree[i].smaller = UNUSED;
        }
        *length = 0;
    }
    else {
        // The new node will be put in the array of nodes at position curr.
        int curr = (ustream->lookahead - ustream->cdata) % ustream->window_maxsize;
        *length = lz77_find_and_add(ustream, curr, offset);
    }

    int can_encode = tinyhuff_can_encode(ustream->length_encoder, *length);

    int count;
    if (*length == 0 || can_encode == 0) {
        count = 1;
        *length = 0;
        *offset = 0;
        *next = ustream->lookahead[0];
    } else {
        count = *length;
    }
    assert(count <= ustream->lookahead_currsize);

    for (int i = 0; i < count; i++) {
        if (i < count - 1) {
            int curr = (ustream->lookahead + 1 - ustream->cdata) % ustream->window_maxsize;
            lz77_tree_delete_node(ustream->tree, curr);
        }

        // Update the sliding window, increasing its size up to the maximum and then shifting it.
        if (ustream->window_currsize == ustream->window_maxsize) {
            ustream->window += 1;
        } else {
            ustream->window_currsize += 1;
        }
        assert(ustream->window_currsize <= ustream->window_maxsize);

        // Even after being shifted, the window always covers valid data, so it
        // always ends before (ustream->cdata + ustream->size).
        assert(ustream->window + ustream->window_currsize <= ustream->cdata + ustream->size);

        // Shift the look-ahead buffer.
        ustream->lookahead += 1;

        // Contrary to the window, the end of the look-ahead buffer may have
        // passed the end of valid data.
        const uint8_t *data_end = ustream->cdata + ustream->end;
        const uint8_t *lkah_end = ustream->lookahead + ustream->lookahead_currsize;
        if (lkah_end > data_end) {
            assert(lkah_end == data_end + 1);

            // If a valid file/socket descriptor has been provided, we must
            // keep the look-ahead buffer full.

            // Except when initialized, lookahead_currsize is always equal to
            // lookahead_maxsize. It is then reduced only at the end of the
            // compression, when not enough data is available. As a consequence,
            // we can test their equality to check whether EOF was reached
            // (thus, we avoid copying bytes with memmove).
            int eof = ustream->lookahead_currsize < ustream->lookahead_maxsize;

            // Used to test if the window has been moved from the beginning of
            // the data buffer, otherwise we can avoid using memmove.
            int canmove = ustream->window > ustream->data;

            if (ustream->fd >= 0 && !eof && canmove) {
                assert(ustream->window_currsize == ustream->window_maxsize);

                // Move the window and the look-ahead buffer to the beginning of the data buffer.
                int lookah_size = data_end - ustream->lookahead;
                int data_size = ustream->window_maxsize + lookah_size;
                memmove(ustream->data, ustream->window, data_size);

                // Try to refill the data buffer as much as possible
                uint8_t *new_lookah = ustream->data + ustream->window_maxsize;
                uint8_t *dest = new_lookah + lookah_size;
                int max_count = ustream->size - data_size;
                assert(max_count == ustream->data + ustream->size - dest);
                int readcount = read(ustream->fd, dest, max_count);
                if (readcount < 0) {
                    return -1;
                }

                // Rotate the tree array.
                int x = (ustream->window - ustream->data) % ustream->window_maxsize;
                rotate_tree_array(ustream->tree, ustream->window_maxsize, x);
                shift_tree_indices(ustream->tree, ustream->window_maxsize, x);

                // Update status variables.
                ustream->window = ustream->data;
                ustream->lookahead = new_lookah;
                ustream->end = data_size + readcount;
                ustream->lookahead_currsize = lookah_size + readcount;
                if (ustream->lookahead_currsize > ustream->lookahead_maxsize) {
                    ustream->lookahead_currsize = ustream->lookahead_maxsize;
                }
            } else {
                // Reduce the current size of the look-ahead buffer.
                ustream->lookahead_currsize -= 1;
            }
            assert(ustream->lookahead_currsize <= ustream->lookahead_maxsize);
        }

        if (i < count - 1) {
            int curr = (ustream->lookahead - ustream->cdata) % ustream->window_maxsize;
            uint16_t ignored;
            lz77_find_and_add(ustream, curr, &ignored);
        }
    }

    ustream->processed_bytes += count;

    return count;
}

int ustream_save(lz77_ustream * ustream, uint16_t offset, uint16_t length, uint8_t next)
{
    assert(ustream != NULL);
    assert(length == 0 || offset <= ustream->window_currsize);
    assert(ustream->window + ustream->window_currsize == ustream->data + ustream->end);

    int count = length == 0 ? 1 : length;
    if (ustream->size < ustream->end + count) {
        if (ustream->fd >= 0) {
            assert(ustream->window_maxsize == ustream->window_currsize);
            uint8_t *data = ustream->data;
            uint32_t count = ustream->window - ustream->data;
            int64_t writecount = 0;
            while (writecount != count) {
                writecount = write(ustream->fd, data, count);
                if (writecount < 0) {
                    return -1;
                }
                data += writecount;
                count -= writecount;
            }
            memmove(ustream->data, ustream->window, ustream->window_maxsize);
            ustream->window = ustream->data;
            ustream->end = ustream->window_maxsize;
        } else {
            if (ustream->can_realloc == 0) {
                errno = ENOMEM;
                return -1;
            }
            uint32_t new_size = ustream->end + count;
            if (new_size < 1024) {
                new_size = 1024;
            }
            if (new_size < ustream->size * 1.1) {
                new_size = ustream->size * 1.1;
            }
            ustream->size = new_size;
            uint8_t *temp = realloc(ustream->data, ustream->size);
            if (temp == NULL) {
                free(ustream->data);
                ustream->data = NULL;
                return -1;
            }
            // Update the window to point inside the new data at the same offset.
            ustream->window = temp + (ustream->window - ustream->data);
            ustream->data = temp;
        }
    }

    // Write the phrase or the unmatched symbol from the window to the buffer of original data.

    // Check that the offset is strictly inside the window (unless length is 0),
    // so that window[offset] contains valid data.
    assert(length == 0 || ustream->window + offset < ustream->data + ustream->end);

    if (length == 0) {
        ustream->data[ustream->end] = next;
        length++;
    } else if (ustream->window + offset + length <= ustream->data + ustream->end) {
        memcpy(ustream->data + ustream->end, ustream->window + offset, length);
    } else {
        // Copy one byte at a time, since source and destination buffers
        // overlap. We want exactly to exploit this to achieve a better
        // compression, allowing the matched phrase to extend beyond the right
        // boundary of the window and overrun the lookahead buffer.
        // See also comments in find_in_window().
        for (int i = 0; i < length; i++) {
            ustream->data[ustream->end + i] = ustream->window[offset + i];
        }
    }

    // Update the sliding window, increasing its size up to the maximum and then shifting it.
    if (ustream->window_currsize == ustream->window_maxsize) {
        ustream->window += length;
    } else {
        int max_increment = ustream->window_maxsize - ustream->window_currsize;
        if (length <= max_increment) {
            ustream->window_currsize += length;
        } else {
            ustream->window_currsize = ustream->window_maxsize;
            ustream->window += length - max_increment;
        }
    }
    assert(ustream->window_currsize <= ustream->window_maxsize);

    // Even after being shifted, the window always covers valid data, so it
    // always ends before (ustream->data + ustream->size).
    assert(ustream->window + ustream->window_currsize <= ustream->data + ustream->size);

    ustream->end += length;
    ustream->processed_bytes += length;

    return 0;
}

static uint8_t number_of_bits(uint16_t value)
{
    uint8_t r = 1;
    while (value >>= 1) {
        r++;
    }
    return r;
}

/**
 * Left-rotates the tree array by the given number of positions.
 */
static void rotate_tree_array(lz77_tree v[], int size, int shift)
{
    if (size <= 1 || (shift % size) == 0) {
        return;
    }
    for (int offset = 0; offset < shift; offset++) {
        lz77_tree a = v[offset];
        int i = offset;
        while (i + shift < size) {
            v[i] = v[i + shift];
            i += shift;
        }
        v[i] = a;
    }
    rotate_tree_array(v + size - shift, shift, shift - size % shift);
}

/**
 * Updates all indices of the tree nodes by a given shift.
 */
static void shift_tree_indices(lz77_tree v[], int size, int shift)
{
    for (int i = 0; i <= size; i++) {
        if (v[i].parent != UNUSED && v[i].parent != size) {
            if (v[i].parent - shift < 0) {
                v[i].parent = size + v[i].parent - shift;
            } else {
                v[i].parent = v[i].parent - shift;
            }
        }
        if (v[i].smaller != UNUSED) {
            if (v[i].smaller - shift < 0) {
                v[i].smaller = size + v[i].smaller - shift;
            } else {
                v[i].smaller = v[i].smaller - shift;
            }
        }
        if (v[i].larger != UNUSED) {
            if (v[i].larger - shift < 0) {
                v[i].larger = size + v[i].larger - shift;
            } else {
                v[i].larger = v[i].larger - shift;
            }
        }
    }
}
