/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

/**
 * @file cstream.h
 *
 * Public interface to a compressed stream.
 */

#ifndef _LZ77_CSTREAM_H_
#define _LZ77_CSTREAM_H_

#include <stdint.h>

// Forward-declare lz77_cstream since it is used by ustream.h.
struct _lz77_cstream;
typedef struct _lz77_cstream lz77_cstream;

#include <lz77ppm/ustream.h>

/**
 * Creates an input @c lz77_cstream which is backed by a memory buffer.
 * This stream is used as input by the decompression algorithm.
 *
 * @param data The memory buffer from which data is read. Must not be @c NULL.
 * @param size The size of the buffer expressed in bytes. It is used to prevent
 *        overflows (while reading) of the buffer when the stream is corrupted
 *        and the terminating token is altered. In other words, it sets a limit
 *        to the total number of bytes processed by the decompression algorithm.
 *
 * @return A pointer to the newly created @c lz77_cstream, or @c NULL in case of
 *         error. See @c errno for further information. If an invalid argument
 *         is provided, @c errno is set to @c EINVAL and an explanatory string
 *         is written to the @link lz77_log logger@endlink.
 */
lz77_cstream * lz77_cstream_from_memory(const uint8_t *data, uint32_t size);

/**
 * Creates an input @c lz77_cstream which is backed by a file or socket
 * descriptor. This stream is used as input by the decompression algorithm.
 *
 * @param fd The file or socket descriptor from which data will be read.
 *
 * @return A pointer to the newly created @c lz77_cstream, or @c NULL in case of
 *         error. See @c errno for further information. If an invalid argument
 *         is provided, @c errno is set to @c EINVAL and an explanatory string
 *         is written to the @link lz77_log logger@endlink.
 */
lz77_cstream * lz77_cstream_from_descriptor(int fd);

/**
 * Creates an output @c lz77_cstream which is backed by a memory buffer.
 * This stream is used as output by the compression algorithm.
 *
 * @param from The input @c lz77_ustream of the compression algorithm. It is
 *        used to match internal algorithm parameters.
 * @param buffer The memory buffer to which data will be written.
 * @param size The size of the buffer (in bytes). This determines the maximum
 *        amount of bytes that can be written to the buffer.
 * @param can_realloc Determines whether the algorithm can reallocate the
 *        given buffer in order to accommodate produced data.
 *
 * @return A pointer to the newly created @c lz77_cstream, or @c NULL in case of
 *         error. See @c errno for further information. If an invalid argument
 *         is provided, @c errno is set to @c EINVAL and an explanatory string
 *         is written to the @link lz77_log logger@endlink.
 *
 * If the given @c buffer does not contain enough room for storing the output
 * and @c can_realloc is not @c 0, then the algorithm will reallocate the
 * buffer in order to accommodate the whole output. If the size of the @c buffer
 * is not sufficient and @c can_realloc is zero, the compression algorithm
 * will fail (and @c errno will be set to @c ENOMEM).
 *
 * A @c NULL value can be passed as @c data to tell the algorithm to handle
 * the whole allocation of the buffer. In this case, the value of @c size is
 * ignored (it is treated as it would be zero) and @c can_realloc must be
 * non-zero (otherwise the compression algorithm will fail).
 */
lz77_cstream * lz77_cstream_to_memory(lz77_ustream *from,
        uint8_t *buffer,
        uint32_t size,
        uint8_t can_realloc);

/**
 * Creates an output @c lz77_cstream which is backed by a file or socket
 * descriptor. This stream is used as output by the compression algorithm.
 *
 * @param from The input @c lz77_ustream of the compression algorithm. It is
 *        used to match internal algorithm parameters.
 * @param fd The file or socket descriptor to which data will be written.
 *
 * @return A pointer to the newly created @c lz77_cstream, or @c NULL in case of
 *         error. See @c errno for further information. If an invalid argument
 *         is provided, @c errno is set to @c EINVAL and an explanatory string
 *         is written to the @link lz77_log logger@endlink.
 */
lz77_cstream * lz77_cstream_to_descriptor(lz77_ustream *from, int fd);

/**
 * Gets the output buffer associated to an @c lz77_cstream bound to a memory
 * buffer.
 *
 * The returned pointer is valid only when the stream is an output stream backed
 * by a memory buffer. If the stream is bound to a file or socket descriptor,
 * @c NULL is returned.
 */
uint8_t * lz77_cstream_get_buffer(lz77_cstream *cstream);

/**
 * Gets the total number of bits processed, i.e. the number of bits consumed
 * from the stream, if opened for reading, or the number of bits written to
 * it, if opened for writing.
 */
uint64_t lz77_cstream_get_processed_bits(lz77_cstream *cstream);

/**
 * Frees all resources associated with an @c lz77_cstream.
 */
void lz77_cstream_free(lz77_cstream **cstream);

#endif
