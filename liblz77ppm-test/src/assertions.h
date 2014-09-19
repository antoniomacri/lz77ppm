#include <stdint.h>

static char buffer[1000];

void log_and_abort(const char *file,
                   unsigned int line,
                   const char *function,
                   const char *reason,
                   const char *extrainfo)
{
    printf("Assertion failed:\n");
    printf("  Location: %s:%d (%s)\n", file, line, function);
    printf("  Reason: %s\n", reason);
    if (extrainfo != NULL) {
        printf("  Extra info: %s\n", extrainfo);
    }
    printf("\nAborting.");
    abort();
}

void _assert_true(int condition,
                  const char *file,
                  unsigned int line,
                  const char *function,
                  const char *extrainfo)
{
    if (!condition) {
        sprintf(buffer, "Condition not verified");
        log_and_abort(file, line, function, buffer, extrainfo);
    }
}

void _assert_int_equal(int expected, int actual,
                       const char *file,
                       unsigned int line,
                       const char *function,
                       const char *extrainfo)
{
    if (expected != actual) {
        sprintf(buffer, "Expected %d but was %d", expected, actual);
        log_and_abort(file, line, function, buffer, extrainfo);
    }
}

void _assert_n_array_equal(uint8_t *expected, uint8_t *actual, uint32_t n,
                           const char *file,
                           unsigned int line,
                           const char *function,
                           const char *extrainfo)
{
    for (uint32_t _i = 0; _i < n; _i++) {
        if (actual[_i] != expected[_i]) {
            sprintf(buffer, "Expected %d to be %d at position %d",
                    (int)actual[_i], (int)expected[_i], (int)_i);
            log_and_abort(file, line, function, buffer, extrainfo);
        }
    }
}

#define assert_true(condition, ei) \
    _assert_true(condition, __FILE__, __LINE__, __FUNCTION__, ei)

#define assert_int_equal(expected, actual, ei) \
    _assert_int_equal(expected, actual, __FILE__, __LINE__, __FUNCTION__, ei)

#define assert_n_array_equal(expected, actual, n, ei) \
    _assert_n_array_equal(expected, actual, n, __FILE__, __LINE__, __FUNCTION__, ei)
