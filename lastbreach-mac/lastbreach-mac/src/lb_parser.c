#include "lb_parser_internal.h"
/**
 * lb_parser.c
 *
 * Module: Shared parser primitives (initialization/token expectations) used by
 * the parser split across expression/statement/section translation units.
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */

void ps_init(Parser *ps, const char *filename, char *src) {
    ps->filename = filename;
    ps->lx.src = src;
    ps->lx.len = strlen(src);
    ps->lx.pos = 0;
    ps->lx.line = 1;
    lx_next_token(&ps->lx);
}
int ps_is(Parser *ps, TokenKind k) {
    return ps->lx.cur.kind==k;
}
int ps_is_ident(Parser *ps, const char *s) {
    if (!ps_is(ps, TK_IDENT)) return 0;
    Token *t = &ps->lx.cur;
    int n = (int)strlen(s);
    return (t->len==n && strncmp(t->start, s, (size_t)n)==0);
}
void ps_expect(Parser *ps, TokenKind k, const char *what) {
    if (!ps_is(ps, k)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
    lx_next_token(&ps->lx);
}
char *ps_expect_ident(Parser *ps, const char *what) {
    if (!ps_is(ps, TK_IDENT)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
    char *s = tk_cstr(&ps->lx.cur);
    lx_next_token(&ps->lx);
    return s;
}
char *ps_expect_string(Parser *ps, const char *what) {
    if (!ps_is(ps, TK_STRING)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
    char *s = tk_cstr(&ps->lx.cur);
    lx_next_token(&ps->lx);
    return s;
}
double ps_expect_number(Parser *ps, const char *what) {
    Token *t = &ps->lx.cur;
    if (ps_is(ps, TK_NUMBER)) {
        double v = t->num;
        lx_next_token(&ps->lx);
        return v;
    }
    if (ps_is(ps, TK_DURATION)) {
        double v = (double)t->iticks;
        lx_next_token(&ps->lx);
        return v;
    }
    dief("%s:%d: expected %s", ps->filename, t->line, what);
    return 0;
}
double ps_expect_percent(Parser *ps, const char *what) {
    if (!ps_is(ps, TK_PERCENT)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
    double v = ps->lx.cur.num;
    lx_next_token(&ps->lx);
    return v;
}
