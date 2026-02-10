#include "lastbreach.h"
/**
 * lb_lexer.c
 *
 * Module: Lexer/tokenizer for the LastBreach DSL (identifiers, numbers, strings, punctuation).
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */

static int lx_peek(Lexer *lx) {
    return (lx->pos>=lx->len)?0:(unsigned char)lx->src[lx->pos];
}
static int lx_next(Lexer *lx) {
    if (lx->pos>=lx->len) return 0;
    int c = (unsigned char)lx->src[lx->pos++];
    if (c=='\n') lx->line++;
    return c;
}
static int lx_match(Lexer *lx, int c) {
    if (lx_peek(lx)==c) {
        lx_next(lx);
        return 1;
    }
    return 0;
}
static void lx_skip(Lexer *lx) {
    for (;;) {
        int c = lx_peek(lx);
        if (c==0) return;
        if (isspace(c)) {
            lx_next(lx);
            continue;
        }
        if (c=='#') {
            while ((c = lx_next(lx))!=0 && c!='\n') {
            }
            continue;
        }
        if (c=='/' && lx->pos+1<lx->len && lx->src[lx->pos+1]=='/') {
            lx_next(lx);
            lx_next(lx);
            while ((c = lx_next(lx))!=0 && c!='\n') {
            }
            continue;
        }
        if (c=='/' && lx->pos+1<lx->len && lx->src[lx->pos+1]=='*') {
            lx_next(lx);
            lx_next(lx);
            for (;;) {
                c = lx_next(lx);
                if (c==0) dief("unterminated block comment");
                if (c=='*' && lx_peek(lx)=='/') {
                    lx_next(lx);
                    break;
                }
            }
            continue;
        }
        return;
    }
}
static Token tk_make(Lexer *lx, TokenKind k, const char *s, int n) {
    Token t;
    memset(&t, 0, sizeof(t));
    t.kind = k;
    t.start = s;
    t.len = n;
    t.line = lx->line;
    return t;
}
char *tk_cstr(const Token *t) {
    char *s = (char*)xmalloc((size_t)t->len+1);
    memcpy(s, t->start, (size_t)t->len);
    s[t->len] = 0;
    return s;
}
static void lx_read_string(Lexer *lx) {
    const char *start = &lx->src[lx->pos];
    int line0 = lx->line;
    size_t p0 = lx->pos;
    for (;;) {
        int c = lx_next(lx);
        if (c==0) dief("unterminated string at line %d", line0);
        if (c=='"') break;
        if (c=='\\') {
            int d = lx_next(lx);
            if (d==0) dief("unterminated escape at line %d", line0);
        }
    }
    size_t p1 = lx->pos-1;
    lx->cur = tk_make(lx, TK_STRING, start, (int)(p1-p0));
}
static void lx_read_number(Lexer *lx, int first) {
    char buf[128];
    int bi = 0;
    buf[bi++] = (char)first;
    int c;
    int seen_dot = (first=='.');
    while ((c = lx_peek(lx))!=0) {
        if (isdigit(c)) {
            if (bi<120) buf[bi++] = (char)lx_next(lx);
            else lx_next(lx);
            continue;
        }
        if (c=='.' && !seen_dot) {
            /* only treat as decimal point if followed by a digit, otherwise it may be a range operator like .. */
            int nextc = 0;
            if (lx->pos+1 < lx->len) nextc = (unsigned char)lx->src[lx->pos+1];
            if (!isdigit(nextc)) break;
            seen_dot = 1;
            if (bi<120) buf[bi++] = (char)lx_next(lx);
            else lx_next(lx);
            continue;
        }
        break;
    }
    buf[bi] = 0;
    if (lx_peek(lx)=='%') {
        lx_next(lx);
        lx->cur = tk_make(lx, TK_PERCENT, NULL, 0);
        lx->cur.num = atof(buf);
        return;
    }
    if (lx_peek(lx)=='t') {
        lx_next(lx);
        lx->cur = tk_make(lx, TK_DURATION, NULL, 0);
        lx->cur.iticks = (int)(atof(buf)+0.5);
        return;
    }
    lx->cur = tk_make(lx, TK_NUMBER, NULL, 0);
    lx->cur.num = atof(buf);
}
static int is_ident_start(int c) {
    return isalpha(c)||c=='_';
}
static int is_ident_part(int c) {
    return isalnum(c)||c=='_';
}

/** lx_next_token function. */
void lx_next_token(Lexer *lx) {
    lx_skip(lx);
    int c = lx_next(lx);
    if (c==0) {
        lx->cur = tk_make(lx, TK_EOF, NULL, 0);
        return;
    }
    const char *s = &lx->src[lx->pos-1];
    switch (c) {
    case '{':
        lx->cur = tk_make(lx, TK_LBRACE, s, 1);
        return;
    case '}':
        lx->cur = tk_make(lx, TK_RBRACE, s, 1);
        return;
    case '(':
        lx->cur = tk_make(lx, TK_LPAREN, s, 1);
        return;
    case ')':
        lx->cur = tk_make(lx, TK_RPAREN, s, 1);
        return;
    case '[':
        lx->cur = tk_make(lx, TK_LBRACK, s, 1);
        return;
    case ']':
        lx->cur = tk_make(lx, TK_RBRACK, s, 1);
        return;
    case ':':
        lx->cur = tk_make(lx, TK_COLON, s, 1);
        return;
    case ';':
        lx->cur = tk_make(lx, TK_SEMI, s, 1);
        return;
    case ',':
        lx->cur = tk_make(lx, TK_COMMA, s, 1);
        return;
    case '.':
        if (lx_match(lx, '.')) {
            lx->cur = tk_make(lx, TK_DOTDOT, s, 2);
            return;
        }
        lx->cur = tk_make(lx, TK_DOT, s, 1);
        return;
    case '+':
        lx->cur = tk_make(lx, TK_PLUS, s, 1);
        return;
    case '-':
        lx->cur = tk_make(lx, TK_MINUS, s, 1);
        return;
    case '*':
        lx->cur = tk_make(lx, TK_STAR, s, 1);
        return;
    case '/':
        lx->cur = tk_make(lx, TK_SLASH, s, 1);
        return;
    case '=':
        if (lx_match(lx, '=')) {
            lx->cur = tk_make(lx, TK_EQ, s, 2);
            return;
        }
        lx->cur = tk_make(lx, TK_ASSIGN, s, 1);
        return;
    case '!':
        if (lx_match(lx, '=')) {
            lx->cur = tk_make(lx, TK_NEQ, s, 2);
            return;
        }
        dief("unexpected '!' at line %d", lx->line);
        return; /* dief() exits, but keep the compiler happy */
    case '<':
        if (lx_match(lx, '=')) {
            lx->cur = tk_make(lx, TK_LTE, s, 2);
            return;
        }
        lx->cur = tk_make(lx, TK_LT, s, 1);
        return;
    case '>':
        if (lx_match(lx, '=')) {
            lx->cur = tk_make(lx, TK_GTE, s, 2);
            return;
        }
        lx->cur = tk_make(lx, TK_GT, s, 1);
        return;
    case '"':
        lx_read_string(lx);
        return;
    default:
        break;
    }
    if (isdigit(c) || (c=='.' && isdigit(lx_peek(lx)))) {
        lx_read_number(lx, c);
        return;
    }
    if (is_ident_start(c)) {
        size_t p0 = lx->pos-1;
        while (is_ident_part(lx_peek(lx))) lx_next(lx);
        size_t p1 = lx->pos;
        lx->cur = tk_make(lx, TK_IDENT, &lx->src[p0], (int)(p1-p0));
        return;
    }
    dief("unexpected character '%c' at line %d", c, lx->line);
}
