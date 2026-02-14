#include "test_framework.h"

static const char *g_suite_name = NULL;
static int g_run = 0;
static int g_failed = 0;

void test_begin_suite(const char *name) {
    g_suite_name = name;
    g_run = 0;
    g_failed = 0;
    printf("== %s ==\n", g_suite_name ? g_suite_name : "tests");
}

void test_fail_impl(const char *file, int line, const char *expr, const char *fmt, ...) {
    va_list ap;
    g_failed++;
    printf("  failure at %s:%d (%s): ", file, line, expr);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void test_run_case(const char *name, test_fn_t fn) {
    int failed_before = g_failed;
    g_run++;
    fn();
    if (g_failed == failed_before) {
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s\n", name);
    }
}

int test_end_suite(void) {
    int failed_cases = g_failed;
    int passed_cases = g_run - failed_cases;
    if (passed_cases < 0) passed_cases = 0;
    printf("-- result: %d passed, %d failed, %d total --\n", passed_cases, failed_cases, g_run);
    return failed_cases == 0 ? 0 : 1;
}
