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
 * Structures and functions to handle compressed streams.
 */

#ifndef _LZ77_CSTREAM_INTERNAL_H_
#define _LZ77_CSTREAM_INTERNAL_H_

#include <lz77ppm/cstream.h>

/**
 * Represents a stream containing compressed data.
 *
 * It is used to write the compressed data or to read the result of a previous
 * compression. An @c lz77_cstream object can be backed by a memory buffer or
 * by a file/socket descriptor.
 *
 * Notice the two variables @c data and @c cdata -- they are declared just to
 * check constness.
 *
 * @see #lz77_cstream_from_memory
 * @see #lz77_cstream_to_memory
 * @see #lz77_cstream_from_descriptor
 * @see #lz77_cstream_to_descriptor
 */
struct _lz77_cstream {
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
     * The size (in bytes) of the buffer pointed to by @c data (or @c cdata).
     * During a decompression from memory, it is used to prevent overflows
     * (while reading) of the buffer when the stream is corrupted and the
     * terminating token is altered; in other words, @c size is used to limit
     * the total number of bytes processed by the decompression algorithm. When
     * de/compressing from a file, it gives the maximum amount of data that the
     * internal buffer can hold, that is, it is used as an internal parameter
     * which represents the maximum size of each buffered chunk read from or
     * written to the file. During a compression from memory, it is used to
     * check whether the buffer needs to be reallocated in order to accommodate
     * new output bytes.
     */
    uint32_t size;
    /**
     * An index indicating the position of the next @em bit to be read in the
     * buffer pointed to by @c data (or @c cdata). If @c pos equals to @c end,
     * there are no bits left to be read in the buffer.
     *
     * @see #end
     */
    uint64_t pos;
    /**
     * An index indicating the number of valid @em bits in the buffer pointed
     * to by @c data (or @c cdata). It is used to determine how many bits are
     * to be read in the buffer or the position of the next bit to be written.
     * When initialized, @c end is always a multiple of 8 bits, so it is up to
     * the algorithm to found out the exact number of user bits and distinguish
     * them from padding.
     *
     * @see #pos
     */
    uint64_t end;
    /**
     * A small write cache.
     */
    uint64_t cached;
    /**
     * The number of bits stored in @c cached.
     */
    uint8_t cached_nbits;
    /**
     * A boolean value used to determine whether the provided buffer can be
     * reallocated by the algorithm in order to accommodate new data.
     *
     * @see #lz77_cstream_to_memory
     */
    uint8_t can_realloc;
    /**
     * A boolean value indicating whether the stream is opened for reading
     * (and thus used for input by the decompression algorithm).
     */
    uint8_t is_input;
    /**
     * The maximum size of the sliding window.
     */
    uint16_t window_maxsize;
    /**
     * The maximum size of the look-ahead buffer.
     */
    uint16_t lookahead_maxsize;
    /**
     * The total number of bits processed, i.e. the number of bits consumed
     * from the stream, if opened for reading, or the number of bits written to
     * it, if opened for writing.
     *
     * Notice that, when the stream is written to, some of the out-put bits may
     * be @c cached and their count is stored into @c cached_bits, not in @c
     * processed_bits).
     */
    uint64_t processed_bits;
};

/**
 * Contains the header written to the compressed output file.
 */
typedef struct {
    /** A "magic" identifier, always set to the sequence 'L', 'Z', '7', '7'. */
    uint8_t magic[4];
    /**
     * The version of the program used to compress the file. The high and the
     * low nibbles contain respectively the major and the minor version.
     */
    uint8_t version;
    /** Reserved for future uses. */
    uint8_t reserved[3];
    /** The size of the window. */
    uint16_t window_size;
    /** The size of the look-ahead buffer. */
    uint16_t lookahead_size;
} cstream_header;

/**
 * Opens an @c lz77_cstream, initializing its internal data structures.
 *
 * Call this function just once, after the stream has been created and before
 * it is used.
 *
 * @return 0 in case of success, or a negative value if an error occurred. See
 *         @c errno for further information. If an invalid argument is provided,
 *         @c errno is set to @c EINVAL and an explanatory string is written to
 *         the @link lz77_log logger@endlink.
 */
int cstream_open(lz77_cstream *cstream);

/**
 * Closes an @c lz77_cstream, releasing internal resources.
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
int cstream_close(lz77_cstream *cstream);

/**
 * Reads a given number of bits from an @c lz77_cstream.
 *
 * @param buffer A pointer to the buffer into which data will be stored.
 * @param startbit The position of the first bit of @c buffer that will be set.
 * @param nbits The number of bits to read and save into @c buffer.
 *
 * @return The actual number of bits read and stored into the @c buffer, or zero
 *         if EOF was reached, or a negative value in case of error. See @c
 *         errno for further information. If an invalid argument is provided,
 *         @c errno is set to @c EINVAL and an explanatory string is written to
 *         the @link lz77_log logger@endlink.
 */
int cstream_read(lz77_cstream *cstream, void *buffer, uint16_t startbit, uint16_t nbits);

/**
 * Peeks a given number of bits from an @c lz77_cstream, without consuming them.
 *
 * @param buffer A pointer to the buffer into which data will be stored.
 * @param startbit The position of the first bit of @c buffer that will be set.
 * @param nbits The number of bits to peek and save into @c buffer.
 *
 * @return The actual number of bits read and stored into the @c buffer, or zero
 *         if EOF was reached, or a negative value in case of error. See @c
 *         errno for further information. If an invalid argument is provided,
 *         @c errno is set to @c EINVAL and an explanatory string is written to
 *         the @link lz77_log logger@endlink.
 */
int cstream_peek(lz77_cstream *cstream, void *buffer, uint16_t startbit, uint16_t nbits);

/**
 * Consumes the given number of bits from an @c lz77_cstream.
 *
 * @param nbits The number of bits to consume, removing them from the stream.
 *
 * @return The actual number of bits consumed, or zero if EOF was reached, or a
 *         negative value in case of error. See @c errno for further
 *         information. If an invalid argument is provided, @c errno is set to
 *         @c EINVAL and an explanatory string is written to the @link lz77_log
 *         logger@endlink.
 */
int cstream_consume(lz77_cstream *cstream, uint16_t nbits);

/**
 * Writes bits to an @c lz77_cstream from a register.
 *
 * @param reg A pointer to an integer containing bits that will be written.
 * @param startbit The position of the first bit of @c reg to write. The most
 *        significant bit is considered at position 0.
 * @param nbits The number of bits to write to the output stream.
 *
 * @return 0 in case of success, or a negative value if an error occurred.
 *         See @c errno for further information. If an invalid argument is
 *         provided, @c errno is set to @c EINVAL and an explanatory string is
 *         written to the @link lz77_log logger@endlink.
 */
int cstream_write_bits(lz77_cstream *cstream,
                       const uint64_t *reg,
                       uint16_t startbit,
                       uint16_t nbits);

/**
 * Writes bytes to an @c lz77_cstream from a buffer.
 *
 * @param buffer A pointer to the buffer containing bytes that will be written.
 * @param nbytes The number of bytes to write to the output stream.
 *
 * @return 0 in case of success, or a negative value if an error occurred.
 *         See @c errno for further information. If an invalid argument is
 *         provided, @c errno is set to @c EINVAL and an explanatory string is
 *         written to the @link lz77_log logger@endlink.
 */
int cstream_write(lz77_cstream *cstream, const void *buffer, uint32_t nbytes);

#endif
