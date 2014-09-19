/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

/**
 * @file logger.h
 *
 * A very simple logging mechanism.
 */

#ifndef _LZ77_LOGGER_H_
#define _LZ77_LOGGER_H_

/** Debug messages. */
#define LOG_DEBUG 0

/** Generic information. */
#define LOG_INFO 1

/** Warnings. */
#define LOG_WARN 2

/** Error conditions. */
#define LOG_ERROR 3


/**
 * A function pointer to the logging function.
 *
 * Assign to this pointer the desired logging function. The default logger
 * prints a line in the format:
 * @verbatim [<log level>] [<time>] message<newline> @endverbatim
 */
extern void (*lz77_log)(int level, const char *message, ...);

#endif
