/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

/**
 * @file ustream.h
 *
 * Public interface to an uncompressed stream.
 */

#ifndef _LZ77_USTREAM_H_
#define _LZ77_USTREAM_H_

#include <stdint.h>

// Forward-declare lz77_ustream since it is used by cstream.h.
struct _lz77_ustream;
typedef struct _lz77_ustream lz77_ustream;

#include <lz77ppm/cstream.h>

/**
 * Creates an input @c lz77_ustream which is backed by a memory buffer.
 * This stream is used as input by the compression algorithm.
 *
 * @param data The memory buffer from which data is read.
 * @param size The size of the buffer.
 * @param window_size The size of the sliding window used by the compression
 *        algorithm.
 * @param lookahead_size The size of the look-ahead buffer used by the
 *        compression algorithm.
 *
 * @return A pointer to the newly created @c lz77_ustream, or @c NULL in case of
 *         error. See @c errno for further information. If an invalid argument
 *         is provided, @c errno is set to @c EINVAL and an explanatory string
 *         is written to the @link lz77_log logger@endlink.
 */
lz77_ustream * lz77_ustream_from_memory(const uint8_t *data,
                                        uint32_t size,
                                        uint16_t window_size,
                                        uint16_t lookahead_size);

/**
 * Creates an input @c lz77_ustream which is backed by a file or socket
 * descriptor. This stream is used as input by the compression algorithm.
 *
 * @param fd The file or socket descriptor from which data will be read.
 * @param window_size The size of the sliding window used by the compression
 *        algorithm.
 * @param lookahead_size The size of the look-ahead buffer used by the
 *        compression algorithm.
 *
 * @return A pointer to the newly created @c lz77_ustream, or @c NULL in case of
 *         error. See @c errno for further information. If an invalid argument
 *         is provided, @c errno is set to @c EINVAL and an explanatory string
 *         is written to the @link lz77_log logger@endlink.
 */
lz77_ustream * lz77_ustream_from_descriptor(int fd,
                                            uint16_t window_size,
                                            uint16_t lookahead_size);

/**
 * Creates an output @c lz77_ustream which is backed by a memory buffer.
 * This stream is used as output by the decompression algorithm.
 *
 * @param from The input @c lz77_cstream of the decompression algorithm. It is
 *        used to match internal algorithm parameters.
 * @param buffer The memory buffer to which data will be written.
 * @param size The size of the buffer. This determines the maximum amount of
 *        bytes that can be written to the buffer.
 * @param can_realloc Determines whether the algorithm can reallocate the
 *        given buffer in order to accommodate produced data.
 *
 * @return A pointer to the newly created @c lz77_ustream, or @c NULL in case of
 *         error. See @c errno for further information. If an invalid argument
 *         is provided, @c errno is set to @c EINVAL and an explanatory string
 *         is written to the @link lz77_log logger@endlink.
 *
 * If the given @c buffer does not contain enough room for storing the output
 * and @c can_realloc is not @c 0, then the algorithm will reallocate the
 * buffer in order to accommodate the whole output. If the size of the @c buffer
 * is not sufficient and @c can_realloc is zero, the decompression algorithm
 * will fail (and @c errno will be set to @c ENOMEM).
 *
 * A @c NULL value can be passed as @c data to tell the algorithm to handle
 * the whole allocation of the buffer. In this case, the value of @c size is
 * ignored (it is treated as it would be zero) and @c can_realloc must be
 * non-zero (otherwise the decompression algorithm will fail).
 */
lz77_ustream * lz77_ustream_to_memory(lz77_cstream *from,
                                      uint8_t *buffer,
                                      uint32_t size,
                                      uint8_t can_realloc);

/**
 * Creates an output @c lz77_ustream which is backed by a file or socket
 * descriptor. This stream is used as output by the decompression algorithm.
 *
 * @param from The input @c lz77_cstream of the decompression algorithm. It is
 *        used to match internal algorithm parameters.
 * @param fd The file or socket descriptor to which data will be written.
 *
 * @return A pointer to the newly created @c lz77_ustream, or @c NULL in case of
 *         error. See @c errno for further information. If an invalid argument
 *         is provided, @c errno is set to @c EINVAL and an explanatory string
 *         is written to the @link lz77_log logger@endlink.
 */
lz77_ustream * lz77_ustream_to_descriptor(lz77_cstream *from, int fd);

/**
 * Gets the output buffer associated to an @c lz77_ustream bound to a memory
 * buffer.
 *
 * The returned pointer is valid only when the stream is an output stream backed
 * by a memory buffer. If the stream is bound to a file or socket descriptor,
 * @c NULL is returned.
 */
uint8_t * lz77_ustream_get_buffer(lz77_ustream *ustream);

/**
 * Frees all resources associated with an @c lz77_ustream.
 */
void lz77_ustream_free(lz77_ustream **ustream);

#endif
