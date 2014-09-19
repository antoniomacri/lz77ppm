/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

#include "bit.h"

uint8_t bit_get(const uint8_t *bits, int pos)
{
    return (bits[pos / 8] >> (7 - pos % 8)) & 1;
}

void bit_set(uint8_t *bits, int pos, uint8_t state)
{
    if (state)
        bits[pos / 8] |= 0x80 >> (pos % 8);
    else
        bits[pos / 8] &= ~(0x80 >> (pos % 8));
}
