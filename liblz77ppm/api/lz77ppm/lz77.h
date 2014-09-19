/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

/**
 * @file lz77.h
 *
 * The main header file of the library.
 */

#ifndef _LZ77PPM_H_
#define _LZ77PPM_H_

#include <stdint.h>

#include <lz77ppm/cstream.h>
#include <lz77ppm/ustream.h>

#define LZ77PPM_VERSION 0x10

/**
 * Number of bits used to identify the type of an LZ77 token.
 */
#define LZ77_TYPE_BITS 1

/**
 * Number of bits used to embed the next symbol in a token.
 */
#define LZ77_NEXT_BITS 8

/**
 * Number of bits for an LZ77 symbol token.
 */
#define LZ77_SYMBOL_BITS (LZ77_TYPE_BITS + LZ77_NEXT_BITS)

/**
 * The minimum accepted window size.
 */
#define LZ77_MIN_WINDOW_SIZE 4

/**
 * The minimum accepted look-ahead buffer size.
 */
#define LZ77_MIN_LOOKAHEAD_SIZE 2

/**
 * Compresses a sequence of bytes using the LZ77 algorithm.
 *
 * @param original The stream containing the data to be compressed.
 * @param compressed The stream that will contain the compressed data.
 *
 * @return Number of bytes written in the compressed stream, or @c -1 in case
 *         of failure.
 *
 * If the function fails, use @c errno to get additional information about the
 * reason.
 */
int64_t lz77_compress(lz77_ustream *original, lz77_cstream *compressed);

/**
 * Reconstruct the original data from a stream compressed using the LZ77
 * algorithm.
 *
 * @param compressed The stream containing the data to be decompressed.
 * @param original The stream that will contain the decompressed data.
 *
 * @return Number of bytes written in the decompressed stream, or @c -1 in case
 *         of failure.
 *
 * If the function fails, use @c errno to get additional information about the
 * reason.
 */
int64_t lz77_decompress(lz77_cstream *compressed, lz77_ustream *original);

/**
 * Reports progress about the compression/decompression.
 *
 * Assign to this variable the desired progress reporting function, in order to
 * be notified of progresses about the operation being performed. Set it to
 * @c NULL to disable any report.
 *
 * @param ustream The uncompressed stream (input for a compression operation).
 * @param cstream The compressed stream (input for a decompression operation).
 * @param percent The percentage of completion of the operation (from 0 to
 *        100.0), or zero if it cannot be determined (for instance, when the
 *        input stream is from a socket). This percentage is determined as the
 *        ratio between the total number of bytes processed from the input
 *        stream and the whole size of the input stream.
 */
extern void (*report_progress)(lz77_ustream *ustream, lz77_cstream *cstream, float percent);

#endif
