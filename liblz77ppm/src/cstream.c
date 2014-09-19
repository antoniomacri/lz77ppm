/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

#define _BSD_SOURCE  // Required on Linux for htobe64()
#ifdef __FreeBSD__
#  include <sys/endian.h>
#else
#  include <endian.h>
#endif

#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lz77ppm/lz77.h>
#include <lz77ppm/logger.h>

#include <cstream_internal.h>
#include <ustream_internal.h>
#include <bit.h>

uint8_t * lz77_cstream_get_buffer(lz77_cstream *cstream)
{
    if (cstream == NULL) {
        lz77_log(LOG_ERROR, "Argument `cstream' must not be NULL");
        errno = EINVAL;
        return NULL;
    }

    if (cstream->fd < 0) {
        return cstream->data;
    } else {
        return NULL;
    }
}

uint64_t lz77_cstream_get_processed_bits(lz77_cstream *cstream)
{
    if (cstream == NULL) {
        lz77_log(LOG_ERROR, "Argument `cstream' must not be NULL");
        errno = EINVAL;
        return 0;
    }

    return cstream->processed_bits + cstream->cached_nbits;
}

lz77_cstream * lz77_cstream_from_memory(const uint8_t *data, uint32_t size)
{
    if (data == NULL) {
        lz77_log(LOG_ERROR, "Argument `data' must not be NULL");
        errno = EINVAL;
        return NULL;
    }

    lz77_cstream *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        object->fd = -1;
        object->cdata = data;
        object->size = size;
        object->end = size * (uint64_t)8;
        object->is_input = 1;
    }
    return object;
}

lz77_cstream * lz77_cstream_from_descriptor(int fd)
{
    if (fd < 0) {
        lz77_log(LOG_ERROR, "The file descriptor is not valid");
        errno = EINVAL;
        return NULL;
    }

    lz77_cstream *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        int data_size = 1024; // 1 KB
        uint8_t * data = malloc(data_size);
        if (data == NULL) {
            free(object);
            return NULL;
        }
        object->fd = fd;
        object->cdata = object->data = data;
        object->can_realloc = 1;
        object->size = data_size;
        object->is_input = 1;
    }
    return object;
}

lz77_cstream * lz77_cstream_to_descriptor(lz77_ustream *from, int fd)
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

    lz77_cstream *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        int data_size = 1024; // 1 KB
        uint8_t * data = malloc(data_size);
        if (data == NULL) {
            free(object);
            return NULL;
        }
        object->fd = fd;
        object->data = data;
        object->can_realloc = 1;
        object->size = data_size;
        object->window_maxsize = from->window_maxsize;
        object->lookahead_maxsize = from->lookahead_maxsize;
    }
    return object;
}

lz77_cstream * lz77_cstream_to_memory(lz77_ustream *from,
                                      uint8_t *data,
                                      uint32_t size,
                                      uint8_t can_realloc)
{
    if (from == NULL) {
        lz77_log(LOG_ERROR, "Argument `from' must not be NULL");
        errno = EINVAL;
        return NULL;
    }

    lz77_cstream *object = calloc(1, sizeof(*object));
    if (object != NULL) {
        object->fd = -1;
        object->data = data;
        object->size = data == NULL ? 0 : size;
        object->can_realloc = can_realloc;
        object->window_maxsize = from->window_maxsize;
        object->lookahead_maxsize = from->lookahead_maxsize;
    }
    return object;
}

int cstream_open(lz77_cstream *cstream)
{
    assert(cstream != NULL);
    assert(cstream->pos == 0);
    assert(!cstream->is_input || (cstream->window_maxsize == 0 && cstream->lookahead_maxsize == 0));

    cstream_header header;
    // memset to zero, since cstream_read requires the buffer to be zeroed.
    memset(&header, 0, sizeof(header));
    if (cstream->is_input) {
        if (cstream_read(cstream, &header, 0, sizeof(header) * 8) != sizeof(header) * 8) {
            lz77_log(LOG_ERROR, "Cannot read from stream");
            return -1;
        }
        if (memcmp(header.magic, "LZ77", 4) != 0) {
            lz77_log(LOG_ERROR, "Invalid file type");
            errno = 0;
            return -1;
        }
        if (header.version != LZ77PPM_VERSION) {
            lz77_log(LOG_ERROR, "File compressed with an unsupported program version");
            errno = 0;
            return -1;
        }
        cstream->window_maxsize = ntohs(header.window_size);
        if (cstream->window_maxsize < LZ77_MIN_WINDOW_SIZE) {
            lz77_log(LOG_ERROR, "The compressed file specifies an invalid window size");
            errno = 0;
            return -1;
        }
        cstream->lookahead_maxsize = ntohs(header.lookahead_size);
        if (cstream->lookahead_maxsize < LZ77_MIN_LOOKAHEAD_SIZE) {
            lz77_log(LOG_ERROR, "The compressed file specifies an invalid look-ahead size");
            errno = 0;
            return -1;
        }
        if (cstream->lookahead_maxsize > cstream->window_maxsize) {
            lz77_log(LOG_ERROR,
                    "The compressed file specifies a look-ahead bigger than the window");
            errno = 0;
            return -1;
        }
    }
    else {
        memcpy(header.magic, "LZ77", 4);
        header.version = LZ77PPM_VERSION;
        header.window_size = htons(cstream->window_maxsize);
        header.lookahead_size = htons(cstream->lookahead_maxsize);
        if (cstream_write(cstream, &header, sizeof(header)) < 0) {
            lz77_log(LOG_ERROR, "Cannot write to stream");
            return -1;
        }
    }

    return 0;
}

int cstream_close(lz77_cstream *cstream)
{
    assert(cstream != NULL);

    if (cstream->cached_nbits > 0) {
        uint64_t cached_ordered = htobe64(cstream->cached);
        int nbytes = (cstream->cached_nbits + 7) / 8;
        cstream_write(cstream, &cached_ordered, nbytes);
        cstream->cached_nbits = 0;
    }

    if (cstream->fd >= 0) {
        // Flush the data buffer when in output mode.
        if (cstream->is_input == 0) {
            uint8_t *data = cstream->data;
            uint32_t count = (cstream->end + 7) / 8;
            int64_t writecount = 0;
            while (writecount != count) {
                writecount = write(cstream->fd, data, count);
                if (writecount < 0) {
                    return -1;
                }
                data += writecount;
                count -= writecount;
            }
            cstream->end = 0;
        }
    }

    return 0;
}

void lz77_cstream_free(lz77_cstream **pcstream)
{
    assert(pcstream != NULL);

    lz77_cstream *cstream = *pcstream;
    if (cstream == NULL) {
        return;
    }

    if (cstream->fd >= 0) {
        // Release the memory of the internal buffer.
        assert(cstream->can_realloc != 0);
        free(cstream->data);
        cstream->cdata = cstream->data = NULL;
    }
    free(cstream);

    *pcstream = NULL;
}

int cstream_read(lz77_cstream *cstream, void *buffer, uint16_t startbit, uint16_t nbits)
{
    assert(cstream != NULL);
    assert(buffer != NULL);
    assert(cstream->is_input);

    int i = 0;
    while (1) {
        int ii = cstream_peek(cstream, buffer, startbit, nbits);
        if (ii <= 0) {
            return ii;  // Error or EOF.
        }
        if (ii == i || ii == nbits) { // If ii == i, no more bits read => EOF.
            cstream_consume(cstream, ii);
            return ii;
        }
        i = ii;
    }
}

int cstream_peek(lz77_cstream *cstream, void *buffer, uint16_t startbit, uint16_t nbits)
{
    assert(cstream != NULL);
    assert(buffer != NULL);
    assert(cstream->is_input);

    if (cstream->pos + nbits > cstream->end) {
        if (cstream->fd >= 0) {
            // Move bytes between pos and end at the beginning of the buffer.
            int pos_byte = cstream->pos / 8;
            int end_byte = (cstream->end + 7) / 8;
            uint8_t *pos_data = cstream->data + pos_byte;
            memmove(cstream->data, pos_data, end_byte - pos_byte);
            cstream->pos -= pos_byte * 8;
            cstream->end -= pos_byte * 8;

            // Try to refill the data buffer.
            end_byte = (cstream->end + 7) / 8;
            int max_count = cstream->size - end_byte;
            int count = read(cstream->fd, cstream->data + end_byte, max_count);
            if (count < 0) {
                return count;
            }
            cstream->end += count * 8;
        }
    }

    int i = 0;
    for (; i < nbits && cstream->pos + i < cstream->end; i++) {
        if (bit_get(cstream->cdata, cstream->pos + i)) {
            bit_set(buffer, startbit + i, 1);
        } else {
            assert(bit_get(buffer, startbit + i) == 0);
        }
    }
    return i;
}

int cstream_consume(lz77_cstream *cstream, uint16_t nbits)
{
    assert(cstream != NULL);
    assert(cstream->is_input);

    // Leave the assertion here but cancel out any implications in case it is
    // removed (NDEBUG).
    assert(cstream->pos + nbits <= cstream->end);
    if (nbits > cstream->end - cstream->pos) {
        nbits = cstream->end - cstream->pos;
    }

    cstream->pos += nbits;
    cstream->processed_bits += nbits;
    return nbits;
}

int cstream_write_bits(lz77_cstream *cstream,
                       const uint64_t *reg,
                       uint16_t startbit,
                       uint16_t nbits)
{
    assert(cstream != NULL);
    assert(reg!= NULL);
    assert(startbit + nbits <= sizeof(*reg) * 8);
    assert(!cstream->is_input);

    int result = 0;

    if (cstream->cached_nbits + nbits > sizeof(cstream->cached) * 8) {
        // Be sure we will write at least one byte.
        assert(cstream->cached_nbits / 8 > 1);

        uint64_t cached_ordered = htobe64(cstream->cached);
        int count = cstream->cached_nbits / 8;
        result = cstream_write(cstream, &cached_ordered, count);

        // Force clearing of register also when it is shifted by 64 bits.
        cstream->cached <<= 1;
        cstream->cached <<= (cstream->cached_nbits / 8) * 8 - 1;
        cstream->cached_nbits = cstream->cached_nbits % 8;
    }

    uint64_t value = *reg;
    value = value >> (sizeof(value) * 8 - startbit - nbits);
    value = value & ((1 << nbits) - 1);
    value = value << (sizeof(value) * 8 - nbits - cstream->cached_nbits);
    cstream->cached |= value;
    cstream->cached_nbits += nbits;

    return result;
}

int cstream_write(lz77_cstream *cstream, const void *buffer, uint32_t nbytes)
{
    assert(cstream != NULL);
    assert(buffer != NULL);
    assert(!cstream->is_input);

    // We should have written multiple of bytes to the output stream.
    assert(cstream->end % 8 == 0);

    if (cstream->end / 8 + nbytes > cstream->size) {
        if (cstream->fd >= 0) {
            uint8_t *data = cstream->data;
            uint32_t count = cstream->end / 8;
            int64_t writecount = 0;
            while (writecount != count) {
                writecount = write(cstream->fd, data, count);
                if (writecount < 0) {
                    return -1;
                }
                data += writecount;
                count -= writecount;
            }
            cstream->end = 0;
        }
        else {
            if (cstream->can_realloc == 0) {
                errno = ENOMEM;
                return -1;
            }
            uint32_t new_size = cstream->end / 8 + nbytes;
            if (new_size < 1024) {
                new_size = 1024;
            }
            if (new_size < cstream->size * 1.1) {
                new_size = cstream->size * 1.1;
            }
            uint8_t *temp = realloc(cstream->data, new_size);
            if (temp == NULL) {
                free(cstream->data);
                cstream->data = NULL;
                return -1;
            }
            cstream->size = new_size;
            cstream->data = temp;
        }
    }
    assert(cstream->end / 8 + nbytes <= cstream->size);

    memcpy(cstream->data + cstream->end / 8, buffer, nbytes);

    cstream->end += nbytes * 8;
    cstream->processed_bits += nbytes * 8;

    return 0;
}
