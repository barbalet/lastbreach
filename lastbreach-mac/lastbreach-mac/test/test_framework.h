#ifndef LASTBREACH_TEST_FRAMEWORK_H
#define LASTBREACH_TEST_FRAMEWORK_H

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*test_fn_t)(void);

void test_begin_suite(const char *name);
void test_run_case(const char *name, test_fn_t fn);
int test_end_suite(void);
void test_fail_impl(const char *file, int line, const char *expr, const char *fmt, ...);

#define ASSERT_TRUE(expr)                                                          \
    do {                                                                           \
        if (!(expr)) {                                                             \
            test_fail_impl(__FILE__, __LINE__, #expr, "assertion failed");        \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_TRUE_MSG(expr, fmt, ...)                                            \
    do {                                                                           \
        if (!(expr)) {                                                             \
            test_fail_impl(__FILE__, __LINE__, #expr, fmt, __VA_ARGS__);          \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_EQ_INT(expected, actual)                                            \
    do {                                                                           \
        int _exp = (expected);                                                     \
        int _act = (actual);                                                       \
        if (_exp != _act) {                                                        \
            test_fail_impl(__FILE__, __LINE__, #actual,                           \
                           "expected %d but got %d", _exp, _act);                 \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_EQ_DBL(expected, actual, eps)                                       \
    do {                                                                           \
        double _exp = (expected);                                                  \
        double _act = (actual);                                                    \
        double _eps = (eps);                                                       \
        if (fabs(_exp - _act) > _eps) {                                            \
            test_fail_impl(__FILE__, __LINE__, #actual,                           \
                           "expected %.6f but got %.6f (eps=%.6f)",               \
                           _exp, _act, _eps);                                      \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_STREQ(expected, actual)                                             \
    do {                                                                           \
        const char *_exp = (expected);                                             \
        const char *_act = (actual);                                               \
        if ((_exp == NULL) != (_act == NULL) ||                                    \
            (_exp && _act && strcmp(_exp, _act) != 0)) {                          \
            test_fail_impl(__FILE__, __LINE__, #actual,                           \
                           "expected \"%s\" but got \"%s\"",                      \
                           _exp ? _exp : "(null)", _act ? _act : "(null)");       \
            return;                                                                \
        }                                                                          \
    } while (0)

#endif
