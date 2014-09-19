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
#include <sys/stat.h>

#include <lz77ppm/lz77.h>

#include <ustream_internal.h>
#include <cstream_internal.h>
#include <tinyhuff.h>

/*
 * Use int64_t: this function would return an uint64_t (to indicate the size in
 * bytes of the compressed stream) or -1 in case of error. Since the size is
 * internally stored as number of bits, the maximum size of the compressed file
 * is (2^64 - 8)/8 = 2^61 - 1 bytes.
 */
int64_t lz77_compress(lz77_ustream *original, lz77_cstream *compressed)
{
    assert(original != NULL);
    assert(compressed != NULL);

    if (ustream_open(original) < 0 || cstream_open(compressed) < 0) {
        return -1;
    }

    int winoff_bits = original->window_nbits;
    lz77_tinyhuff *length_encoder = original->length_encoder;

    uint64_t input_size = 0;
    if (report_progress) {
        if (original->fd >= 0) {
            struct stat st;
            if (fstat(original->fd, &st) == 0) {
                input_size = st.st_size;
            }
        }
        else {
            input_size = original->end;
        }
    }

    uint16_t offset, length;
    uint8_t next;
    while (ustream_find_and_advance(original, &offset, &length, &next) > 0)
    {
        uint64_t token;
        uint16_t tbits;
        if (length != 0) {
            // Encode a phrase token.
            token = 0x00000001;
            token = (token << winoff_bits) | offset;
            tbits = tinyhuff_encode(length_encoder, length, &length);
            token = (token << tbits) | length;
            tbits = LZ77_TYPE_BITS + winoff_bits + tbits;
        }
        else {
            // Encode a symbol token.
            token = 0x00000000;
            token = (token << LZ77_NEXT_BITS) | next;
            tbits = LZ77_SYMBOL_BITS;
        }

        // Write the token to the buffer of compressed data.
        uint16_t startbit = (sizeof(token) * 8) - tbits;
        if (cstream_write_bits(compressed, &token, startbit, tbits) < 0) {
            return -1;
        }

        if (report_progress) {
            float percent = 0;
            if (input_size > 0) {
                percent = 100.0 * original->processed_bytes / input_size;
            }
            report_progress(original, compressed, percent);
        }
    }

    // Encode the terminating token.
    uint64_t token = 0x00000001 << winoff_bits;
    uint16_t tbits = tinyhuff_encode(length_encoder, 0, &length);
    token = (token << tbits) | length;
    tbits = LZ77_TYPE_BITS + winoff_bits + tbits;

    uint32_t startbit = (sizeof(token) * 8) - tbits;
    if (cstream_write_bits(compressed, &token, startbit, tbits) < 0) {
        return -1;
    }

    ustream_close(original);
    cstream_close(compressed);

    return (lz77_cstream_get_processed_bits(compressed) + 7) / 8;
}

/*
 * Use int64_t: this function would return an uint64_t (to indicate the size in
 * bytes of the compressed stream) or -1 in case of error. Consequently, the
 * maximum size of the uncompressed file is 2^(64-1) - 1 bytes.
 */
int64_t lz77_decompress(lz77_cstream *compressed, lz77_ustream *original)
{
    assert(compressed != NULL);
    assert(original != NULL);

    if (cstream_open(compressed) < 0 || ustream_open(original) < 0) {
        return -1;
    }

    int winoff_bits = original->window_nbits;
    lz77_tinyhuff *length_encoder = original->length_encoder;

    uint64_t input_size = 0;
    if (report_progress) {
        if (compressed->fd >= 0) {
            struct stat st;
            if (fstat(compressed->fd, &st) == 0) {
                input_size = st.st_size;
            }
        }
        else {
            input_size = compressed->end;
        }
    }

    while (1)
    {
        // Get the next bit from the compressed data to determine if there is
        // a phrase or a symbol token.
        int state = 0;
        cstream_read(compressed, &state, 0, LZ77_TYPE_BITS);

        uint16_t offset = 0, length = 0;
        uint8_t next = 0;

        if (state) {
            unsigned tpos;
            tpos = (sizeof(offset) * 8) - winoff_bits;
            cstream_read(compressed, &offset, tpos, winoff_bits);

            while (1) {
                uint16_t peek = 0;
                int c, p = cstream_peek(compressed, &peek, 0, sizeof(peek) * 8);
                peek = htons(peek);
                c = tinyhuff_decode(length_encoder, &peek, p, &length);
                if (c > 0) {
                    cstream_consume(compressed, c);
                    break;
                }
            }

            // Ensure that the offset has the correct byte ordering for the
            // system (the decoded length is already byte-ordered).
            offset = ntohs(offset);

            if (length == 0) {
                // We just read the terminating token.
                break;
            }
        }
        else {
            unsigned tpos = (sizeof(next) * 8) - LZ77_NEXT_BITS;
            if (cstream_read(compressed, &next, tpos, LZ77_NEXT_BITS) != LZ77_NEXT_BITS) {
                return -1;
            }
        }

        // Write the phrase from the window to the output stream.
        if (ustream_save(original, offset, length, next) < 0) {
            return -1;
        }

        if (report_progress) {
            float percent = 0;
            if (input_size > 0) {
                percent = 100.0 * (compressed->processed_bits / 8) / input_size;
            }
            report_progress(original, compressed, percent);
        }
    }

    cstream_close(compressed);
    ustream_close(original);

    return original->processed_bytes;
}

void (*report_progress)(lz77_ustream *ustream, lz77_cstream *cstream, float percent);

