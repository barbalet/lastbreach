#include "lastbreach.h"
/**
 * lb_common.c
 *
 * Module: Common utilities: fatal error reporting and checked allocation helpers.
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */


/** Prints a formatted fatal error message to stderr and terminates the program. */
void dief(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
void *xmalloc(size_t n) {
    /* Centralized OOM handling keeps call sites uncluttered. */
    void *p = malloc(n);
    if (!p) dief("out of memory");
    return p;
}
void *xrealloc(void *p, size_t n) {
    void*q = realloc(p, n);
    if (!q) dief("out of memory");
    return q;
}
char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char*)xmalloc(n+1);
    memcpy(p, s, n+1);
    return p;
}
