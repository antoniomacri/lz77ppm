/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

/**
 * @file tinyhuff.h
 *
 * Huffman compression for the length of a match.
 */

#ifndef _LZ77_TINYHUFF_H_
#define _LZ77_TINYHUFF_H_

#include <stdint.h>

/**
 * A tiny data structure to support encoding of small values (i.e., the length
 * of a match) using a static Huffman table.
 */
typedef struct _lz77_tinyhuff {
    /**
     * The minimum value that will be encoded.
     */
    uint16_t min_value;
    /**
     * The maximum value that will be encoded.
     */
    uint16_t max_value;
    /**
     * The maximum value with an associated Huffman code.
     * It is an internal parameter just cached here.
     */
    uint16_t max_encoded_value;
    /**
     * The number of bits used to encode the diff part of a code.
     * It is an internal parameter just cached here.
     */
    uint8_t diff_nbits;
} lz77_tinyhuff;

/**
 * The minimum length of a code produced by #lz77_tinyhuff_encode.
 * This is used to choose whether to encode a symbol token or a phrase token
 * based on the their respective bit-lengths.
 */
#define LZ77_TINYHUFF_MIN_CODE_BITS 2

/**
 * Initializes an instance of #lz77_tinyhuff.
 *
 * @param min_value The minimum value that will be encoded.
 * @param max_value The maximum value that will be encoded.
 */
void tinyhuff_init(lz77_tinyhuff *encoder, uint16_t min_value, uint16_t max_value);

/**
 * Encodes a given value.
 *
 * @param value The value to be encoded.
 * @param code A pointer to an integer that will be set to the Huffman code
 *        produced. The code will be right-aligned inside the 16-bits field.
 *
 * @return The length in bits of the code produced.
 */
uint8_t tinyhuff_encode(lz77_tinyhuff *enc, uint16_t value, uint16_t *code);

/**
 * Gets a value indicating whether the given number can be encoded.
 *
 * @param value The value that should be encoded.
 *
 * @return A non-zero value if the given number can be encoded (it is greater
 *         than or equal to the maximum accepted value and lesser than or equal
 *         to the maximum accepted value), or zero otherwise.
 */
uint8_t tinyhuff_can_encode(lz77_tinyhuff *enc, uint16_t value);

/**
 * Decodes a given value.
 *
 * @param peeked_data A pointer to a 16-bit unsigned integer that contains
 *        (starting from the left-most bit) data peeked from the input buffer.
 * @param peeked_length Number of bits actually peaked from the input buffer and
 *        stored into @c peeked_data.
 * @param value A pointer to an integer that will be set to the decoded
 *        value.
 *
 * @return The number of bits consumed from the peeked data or 0 in case of
 *         error.
 *
 * The size of an encoded value is variable, but it is guaranteed not to exceed
 * the size of @c *peeked_data (that is, 16 bits in the current implementation).
 * Notice that the function gets a pointer to the peeked data (even if it is a
 * simple @c uint16_t) instead of the data itself, since the compiler should
 * issue a warning of "incompatible pointer types" if an integer of different
 * size is passed, which could cause subtle bugs.
 *
 * This function checks whether the specified @c peeked_length is sufficient to
 * extract an encoded value, and if not it returns an error. Usually, the caller
 * will try to peek data as much as required to fill @c *peeked_data (that is,
 * <tt>sizeof(*peeked_data)*8</tt> bits) and put into @c peeked_length the
 * actual number of bits read.
 */
uint8_t tinyhuff_decode(lz77_tinyhuff *enc,
                        const uint16_t *peeked_data,
                        uint16_t peeked_length,
                        uint16_t *value);

#endif
