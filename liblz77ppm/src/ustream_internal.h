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
 * Structures and functions to handle uncompressed streams.
 */

#ifndef _LZ77_USTREAM_INTERNAL_H_
#define _LZ77_USTREAM_INTERNAL_H_

#include <lz77ppm/ustream.h>

#include <tinyhuff.h>
#include <tree.h>

/**
 * Represents a stream containing uncompressed data.
 *
 * It is used to read the data to be compressed or to write the output of the
 * decompression. An @c lz77_ustream object can be backed by a memory buffer or
 * by a file/socket descriptor.
 *
 * Notice the two variables @c data and @c cdata -- they are declared just to
 * check constness.
 *
 * @see #lz77_ustream_from_memory
 * @see #lz77_ustream_to_memory
 * @see #lz77_ustream_from_descriptor
 * @see #lz77_ustream_to_descriptor
 */
struct _lz77_ustream {
    /**
     * A descriptor to a file or a socket used for reading or writing the
     * uncompressed data.
     */
    int fd;
    /**
     * The pointer to an input buffer. @see data
     */
    const uint8_t *cdata;
    /**
     * The pointer to an output buffer. @see cdata
     */
    uint8_t *data;
    /**
     * The size of the buffer pointed to by @c data (or @c cdata). During a
     * compression from memory, it is assumed equal to the whole amount of data
     * to be compressed. When compressing from a file, it gives the maximum
     * amount of data that the data buffer can hold, that is the maximum size
     * of each buffered chunk read from the file. During a decompression, it is
     * used to check whether the buffer needs to be reallocated in order to
     * accommodate new output bytes.
     */
    uint32_t size;
    /**
     * An index indicating the end of valid data inside the buffer pointed to
     * by @c data (or @c cdata). When compressing from a memory stream, it
     * remains equal to @c size, so it is mostly useful when compressing from
     * a file/socket stream, in which case the allocated buffer may be greater
     * that the amount of data it carries. When decompressing, it indicates the
     * position of the next byte in the output buffer.
     */
    uint32_t end;
    /**
     * A boolean value used to determine whether the provided buffer can be
     * reallocated by the algorithm in order to accommodate new data.
     *
     * @see #lz77_ustream_to_memory
     */
    uint8_t can_realloc;
    /**
     * A boolean value indicating whether the stream is opened for reading
     * (and thus used for input by the compression algorithm).
     */
    uint8_t is_input;
    /**
     * A pointer to the sliding window inside the array pointed to by the
     * @c data or @c cdata field.
     */
    const uint8_t *window;
    /**
     * The maximum size of the sliding window.
     */
    uint16_t window_maxsize;
    /**
     * The current size of the sliding window. When the algorithm starts, the
     * sliding window has a size of 0 bytes. It is then increased as bytes are
     * consumed. After @c window_maxsize bytes have been consumed, the window
     * size remains equal to the maximum size.
     */
    uint16_t window_currsize;
    /**
     * Number of bits needed to represent an offset inside the sliding window.
     */
    uint8_t window_nbits;
    /**
     * A pointer to the look-ahead buffer inside the array pointed to by the
     * @c data or @c cdata field.
     */
    const uint8_t *lookahead;
    /**
     * The maximum size of the look-ahead buffer.
     */
    uint16_t lookahead_maxsize;
    /**
     * The current size of the look-ahead buffer. The look-ahead buffer is
     * initialized with a size of 0 bytes. It is then increased as bytes are
     * read, up to a maximum of @c lookahead_maxsize bytes. When the amount of
     * bytes from the input stream is not enough (this happens when we are
     * reaching the EOF), the @c lookahead_currsize is decreased again, until
     * it is set to zero, which means the EOF has been reached and no more data
     * remains to be processed.
     */
    uint16_t lookahead_currsize;
    /**
     * The binary search tree used to quickly find a match in the window.
     * It is allocated with a size of @c window_maxsize+1. The last element
     * (at position @c window_size) is the root of the tree.
     */
    lz77_tree *tree;
    /**
     * The compressor used to encode the length of a match.
     */
    lz77_tinyhuff *length_encoder;
    /**
     * The total number of bytes processed, i.e. the number of bytes consumed
     * from the stream, if opened for reading, or the number of bytes written to
     * it, if opened for writing.
     */
    uint64_t processed_bytes;
    /**
     * The input @c lz77_cstream of the decompression algorithm. It is used only
     * if #is_input is false (i.e., the @c lz77_ustream is used to decompress
     * data from a @c lz77_cstream).
     */
    lz77_cstream *from;
};

/**
 * Opens an @c lz77_ustream, initializing its internal data structures.
 *
 * Call this function just once, after the stream has been created and before
 * it is used.
 *
 * @return 0 in case of success, or a negative value if an error occurred. See
 *         @c errno for further information. If an invalid argument is provided,
 *         @c errno is set to @c EINVAL and an explanatory string is written to
 *         the @link lz77_log logger@endlink.
 */
int ustream_open(lz77_ustream *ustream);

/**
 * Closes an @c lz77_ustream, releasing internal resources.
 *
 * If the stream is backed by a memory buffer, the memory buffer itself is
 * @em not freed. If the stream is backed by a file or socket descriptor,
 * possibly buffered data is flushed, but the descriptor itself is @em not
 * closed.
 *
 * @return 0 in case of success, or a negative value if an error occurred. See
 *         @c errno for further information. If an invalid argument is provided,
 *         @c errno is set to @c EINVAL and an explanatory string is written to
 *         the @link lz77_log logger@endlink.
 */
int ustream_close(lz77_ustream *ustream);

/**
 * Reads data from an @c lz77_ustream producing the parameters @c offset,
 * @c length and @c next of an LZ77 token. The parameters can represent either
 * a phrase or a symbol token. This function also updates the sliding window
 * and the look-ahead buffer by the amount of bytes consumed.
 *
 * @param offset Must point to an integer that will be set to the offset of the
 *        phrase token inside the sliding window. If a symbol token is
 *        encountered, then @c *offset will be left untouched.
 * @param length Must point to an integer that will be set to the length of the
 *        phrase found in the sliding window. It will be zero if a symbol token
 *        is encountered.
 * @param next Must point to a byte that will be set to the next unmatched
 *        symbol. If a phrase token is encountered, it will be left untouched.
 *
 * @return The total number of bytes consumed (that is, @c length for a phrase
 *         token or 1 for a symbol token), zero if EOF was reached, or a
 *         negative value in case of error. See @c errno for further
 *         information. If an invalid argument is provided, @c errno is set to
 *         @c EINVAL and an explanatory string is written to the @link lz77_log
 *         logger@endlink.
 */
int ustream_find_and_advance(lz77_ustream *ustream,
                             uint16_t *offset,
                             uint16_t *length,
                             uint8_t *next);

/**
 * Writes data to an @c lz77_ustream from the given parameters of an LZ77 token.
 *
 * @param offset The offset of the phrase in the sliding window. It is ignored
 *        if a symbol token is being written.
 * @param length The length of the phrase in the sliding window. Must be zero
 *        if a symbol token is to be written.
 * @param next The unmatched symbol. It is ignored if a phrase token is being
 *        written.
 *
 * @return 0 in case of success, or a negative value if an error occurred.
 *         See @c errno for further information. If an invalid argument is
 *         provided, @c errno is set to @c EINVAL and an explanatory string is
 *         written to the @link lz77_log logger@endlink.
 */
int ustream_save(lz77_ustream *ustream, uint16_t offset, uint16_t length, uint8_t next);

#endif
