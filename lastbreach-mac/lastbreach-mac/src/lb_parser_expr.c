#include "lb_parser_internal.h"
/**
 * lb_parser_expr.c
 *
 * Module: Expression AST construction and recursive-descent expression parsing.
 */

/*
 * These tiny constructors centralize allocation/initialization so the parser
 * can build AST nodes with one line per production.
 */
static Expr *ex_new(ExprKind k, int line) {
    Expr *e = (Expr*)xmalloc(sizeof(Expr));
    memset(e, 0, sizeof(*e));
    e->kind = k;
    e->line = line;
    return e;
}
static Expr *ex_num(double v, int line) {
    Expr*e = ex_new(EX_NUM, line);
    e->u.num = v;
    return e;
}
static Expr *ex_bool(int b, int line) {
    Expr*e = ex_new(EX_BOOL, line);
    e->u.boolean = b;
    return e;
}
static Expr *ex_string(char*s, int line) {
    Expr*e = ex_new(EX_STRING, line);
    e->u.str = s;
    return e;
}
static Expr *ex_var(char*v, int line) {
    Expr*e = ex_new(EX_VAR, line);
    e->u.var = v;
    return e;
}
static Expr *ex_un(OpKind op, Expr*a, int line) {
    Expr*e = ex_new(EX_UNARY, line);
    e->u.un.op = op;
    e->u.un.a = a;
    return e;
}
static Expr *ex_bin(OpKind op, Expr*a, Expr*b, int line) {
    Expr*e = ex_new(EX_BINARY, line);
    e->u.bin.op = op;
    e->u.bin.a = a;
    e->u.bin.b = b;
    return e;
}
static Expr *ex_call(char *name, VecExprPtr args, int line) {
    Expr*e = ex_new(EX_CALL, line);
    e->u.call.name = name;
    e->u.call.args = args;
    return e;
}
static Expr *parse_primary(Parser *ps) {
    Token *t = &ps->lx.cur;

    /* Numeric-like literals all collapse to EX_NUM for runtime simplicity. */
    if (ps_is(ps, TK_NUMBER)) {
        double v = t->num;
        int line = t->line;
        lx_next_token(&ps->lx);
        return ex_num(v, line);
    }
    if (ps_is(ps, TK_DURATION)) {
        double v = (double)t->iticks;
        int line = t->line;
        lx_next_token(&ps->lx);
        return ex_num(v, line);
    }
    if (ps_is(ps, TK_PERCENT)) {
        double v = t->num;
        int line = t->line;
        lx_next_token(&ps->lx);
        return ex_num(v, line);
    }
    if (ps_is(ps, TK_STRING)) {
        char *s = tk_cstr(t);
        int line = t->line;
        lx_next_token(&ps->lx);
        return ex_string(s, line);
    }
    if (ps_is(ps, TK_IDENT)) {
        char *base = tk_cstr(t);
        int line = t->line;
        lx_next_token(&ps->lx);

        /* identifier(...) => call expression with comma-separated arguments */
        if (ps_is(ps, TK_LPAREN)) {
            ps_expect(ps, TK_LPAREN, "(");
            VecExprPtr args;
            VEC_INIT(args);
            if (!ps_is(ps, TK_RPAREN)) {
                for (;;) {
                    Expr *a = parse_expr(ps);
                    VEC_PUSH(args, a);
                    if (ps_is(ps, TK_COMMA)) {
                        ps_expect(ps, TK_COMMA, ", ");
                        continue;
                    }
                    break;
                }
            }
            ps_expect(ps, TK_RPAREN, ")");
            return ex_call(base, args, line);
        }
        if (ps_is(ps, TK_DOT)) {
            /*
             * Keep dotted lookups as a single variable token ("char.hunger")
             * so runtime lookup stays table-driven and compact.
             */
            size_t cap = strlen(base)+32;
            char *buf = (char*)xmalloc(cap);
            strcpy(buf, base);
            free(base);
            while (ps_is(ps, TK_DOT)) {
                ps_expect(ps, TK_DOT, ".");
                char *part = ps_expect_ident(ps, "identifier");
                size_t need = strlen(buf)+1+strlen(part)+1;
                if (need>cap) {
                    cap = need*2;
                    buf = (char*)xrealloc(buf, cap);
                }
                strcat(buf, ".");
                strcat(buf, part);
                free(part);
            }
            return ex_var(buf, line);
        }
        return ex_var(base, line);
    }
    if (ps_is(ps, TK_LPAREN)) {
        ps_expect(ps, TK_LPAREN, "(");
        Expr *e = parse_expr(ps);
        ps_expect(ps, TK_RPAREN, ")");
        return e;
    }
    dief("%s:%d: expected expression", ps->filename, t->line);
    return NULL;
}
static Expr *parse_unary(Parser *ps) {
    /* Unary operators are right-associative: "- -x" parses as "-(-x)". */
    if (ps_is_ident(ps, "not")) {
        int line = ps->lx.cur.line;
        lx_next_token(&ps->lx);
        return ex_un(OP_NOT, parse_unary(ps), line);
    }
    if (ps_is(ps, TK_MINUS)) {
        int line = ps->lx.cur.line;
        ps_expect(ps, TK_MINUS, "-");
        return ex_un(OP_NEG, parse_unary(ps), line);
    }
    if (ps_is_ident(ps, "true")) {
        int line = ps->lx.cur.line;
        lx_next_token(&ps->lx);
        return ex_bool(1, line);
    }
    if (ps_is_ident(ps, "false")) {
        int line = ps->lx.cur.line;
        lx_next_token(&ps->lx);
        return ex_bool(0, line);
    }
    return parse_primary(ps);
}
static Expr *parse_mul(Parser *ps) {
    /* Standard precedence ladder: unary > mul/div > add/sub > compare > and > or. */
    Expr *e = parse_unary(ps);
    for (;;) {
        if (ps_is(ps, TK_STAR)) {
            int line = ps->lx.cur.line;
            ps_expect(ps, TK_STAR, "*");
            e = ex_bin(OP_MUL, e, parse_unary(ps), line);
        } else if (ps_is(ps, TK_SLASH)) {
            int line = ps->lx.cur.line;
            ps_expect(ps, TK_SLASH, "/");
            e = ex_bin(OP_DIV, e, parse_unary(ps), line);
        } else break;
    }
    return e;
}
static Expr *parse_add(Parser *ps) {
    Expr *e = parse_mul(ps);
    for (;;) {
        if (ps_is(ps, TK_PLUS)) {
            int line = ps->lx.cur.line;
            ps_expect(ps, TK_PLUS, "+");
            e = ex_bin(OP_ADD, e, parse_mul(ps), line);
        } else if (ps_is(ps, TK_MINUS)) {
            int line = ps->lx.cur.line;
            ps_expect(ps, TK_MINUS, "-");
            e = ex_bin(OP_SUB, e, parse_mul(ps), line);
        } else break;
    }
    return e;
}
static Expr *parse_cmp(Parser *ps) {
    Expr *e = parse_add(ps);
    for (;;) {
        TokenKind k = ps->lx.cur.kind;
        int line = ps->lx.cur.line;
        /* Comparison operators are parsed left-to-right. */
        if (k==TK_EQ) {
            ps_expect(ps, TK_EQ, "==");
            e = ex_bin(OP_EQ, e, parse_add(ps), line);
        } else if (k==TK_NEQ) {
            ps_expect(ps, TK_NEQ, "!=");
            e = ex_bin(OP_NEQ, e, parse_add(ps), line);
        } else if (k==TK_LT) {
            ps_expect(ps, TK_LT, "<");
            e = ex_bin(OP_LT, e, parse_add(ps), line);
        } else if (k==TK_LTE) {
            ps_expect(ps, TK_LTE, "<=");
            e = ex_bin(OP_LTE, e, parse_add(ps), line);
        } else if (k==TK_GT) {
            ps_expect(ps, TK_GT, ">");
            e = ex_bin(OP_GT, e, parse_add(ps), line);
        } else if (k==TK_GTE) {
            ps_expect(ps, TK_GTE, ">=");
            e = ex_bin(OP_GTE, e, parse_add(ps), line);
        } else break;
    }
    return e;
}
static Expr *parse_and(Parser *ps) {
    Expr *e = parse_cmp(ps);
    while (ps_is_ident(ps, "and")) {
        int line = ps->lx.cur.line;
        lx_next_token(&ps->lx);
        e = ex_bin(OP_AND, e, parse_cmp(ps), line);
    }
    return e;
}
static Expr *parse_or(Parser *ps) {
    Expr *e = parse_and(ps);
    while (ps_is_ident(ps, "or")) {
        int line = ps->lx.cur.line;
        lx_next_token(&ps->lx);
        e = ex_bin(OP_OR, e, parse_and(ps), line);
    }
    return e;
}
Expr *parse_expr(Parser *ps) {
    /* Entry point intentionally returns the lowest-precedence parser level. */
    return parse_or(ps);
}
