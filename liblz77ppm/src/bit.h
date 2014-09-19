/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

/**
 * @file bit.h
 *
 * A few routines to get and set bits from buffers.
 */

#ifndef _BIT_H_
#define _BIT_H_

#include <stdint.h>

/**
 * Gets a specific bit from a given buffer.
 *
 * @param bits An array of bytes which is supposed to contain a stream of bits.
 * @param pos The position of the bit inside the given buffer.
 *
 * @return The value of the specified bit.
 *
 * Bits inside a byte are counted from the left-most to the right-most one.
 * For instance, the most significant bit of the first byte (the one pointed to
 * by @c bits) is considered at position 0, while its least significant bit is
 * at position 7. If @c pos is greater than 7, then subsequent bytes are
 * selected according to the position specified.
 */
uint8_t bit_get(const uint8_t *bits, int pos);

/**
 * Sets the bit of a given buffer at a specified position.
 *
 * @param bits An array of bytes which will contain a stream of bits.
 * @param pos The position of the bit to be set inside the given buffer.
 * @param state The new value of the bit. A value of 0 will reset the bit,
 *        while any other value will set the bit.
 *
 * Bits inside a byte are counted from the left-most to the right-most one.
 * For instance, the most significant bit of the first byte (the one pointed to
 * by @c bits) is considered at position 0, while its least significant bit is
 * at position 7. If @c pos is greater than 7, then subsequent bytes are
 * selected according to the position specified.
 */
void bit_set(uint8_t *bits, int pos, uint8_t state);

#endif
