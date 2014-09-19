/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <lz77ppm/logger.h>

static void lz77_log_print_header(int level) {
    time_t now = time(NULL);
    char *time_string = ctime(&now);
    const char *level_name;
    switch (level) {
        case LOG_DEBUG:
            level_name = "debug";
            break;
        case LOG_INFO:
            level_name = "info";
            break;
        case LOG_WARN:
            level_name = "warning";
            break;
        case LOG_ERROR:
            level_name = "error";
            break;
    }
    time_string[strlen(time_string) - 1] = '\0';
    fprintf(stderr, "[%s] [%s] ", time_string, level_name);
}

void lz77_log_default(int level, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    lz77_log_print_header(level);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void (*lz77_log)(int level, const char *message, ...) = lz77_log_default;
