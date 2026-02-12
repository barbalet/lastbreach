#include "lb_parser_internal.h"
/**
 * lb_parser_stmt.c
 *
 * Module: Statement parsing for executable rule/block bodies.
 */

static void skip_block(Parser *ps);

/* ---- Stmts ---- */
static Stmt *st_new(StmtKind k, int line) {
    Stmt *s = (Stmt*)xmalloc(sizeof(Stmt));
    memset(s, 0, sizeof(*s));
    s->kind = k;
    s->line = line;
    return s;
}
Stmt *parse_action_stmt(Parser *ps) {
    Token *t = &ps->lx.cur;
    if (ps_is_ident(ps, "task")) {
        int line = t->line;
        lx_next_token(&ps->lx);
        char *tn = ps_expect_string(ps, "task name");
        Stmt *s = st_new(ST_TASK, line);
        s->u.task.task_name = tn;
        s->u.task.for_ticks = NULL;
        s->u.task.priority = NULL;
        for (;;) {
            if (ps_is_ident(ps, "for")) {
                lx_next_token(&ps->lx);
                s->u.task.for_ticks = parse_expr(ps);
                continue;
            }
            if (ps_is_ident(ps, "priority")) {
                lx_next_token(&ps->lx);
                s->u.task.priority = parse_expr(ps);
                continue;
            }
            /* tolerate optional DSL clauses we don't simulate in detail (using/requires/consumes/produces/when/etc.) */
            if (ps_is(ps, TK_IDENT)) {
                if (ps_is_ident(ps, "using") || ps_is_ident(ps, "requires") || ps_is_ident(ps, "consumes") || ps_is_ident(ps, "produces")) {
                    lx_next_token(&ps->lx);
                    if (ps_is(ps, TK_LBRACE)) {
                        skip_block(ps);
                        continue;
                    }
                    /* sometimes a list follows */
                    if (ps_is(ps, TK_LBRACK)) {
                        /* skip bracket list */
                        int depth = 0;
                        ps_expect(ps, TK_LBRACK, "[");
                        depth = 1;
                        while (depth>0 && !ps_is(ps, TK_EOF)) {
                            if (ps_is(ps, TK_LBRACK)) {
                                ps_expect(ps, TK_LBRACK, "[");
                                depth++;
                                continue;
                            }
                            if (ps_is(ps, TK_RBRACK)) {
                                ps_expect(ps, TK_RBRACK, "]");
                                depth--;
                                continue;
                            }
                            lx_next_token(&ps->lx);
                        }
                        continue;
                    }
                    /* fallthrough: consume one expression if present */
                    if (!ps_is(ps, TK_SEMI)) (void)parse_expr(ps);
                    continue;
                }
                if (ps_is_ident(ps, "when")) {
                    lx_next_token(&ps->lx);
                    (void)parse_expr(ps);
                    continue;
                }
            }
            break;
        }
        return s;
    }
    if (ps_is_ident(ps, "set")) {
        int line = t->line;
        lx_next_token(&ps->lx);
        char *lhs = ps_expect_ident(ps, "lvalue");
        size_t cap = strlen(lhs)+32;
        char *buf = (char*)xmalloc(cap);
        strcpy(buf, lhs);
        free(lhs);
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
        ps_expect(ps, TK_ASSIGN, "=");
        Expr *rhs = parse_expr(ps);
        Stmt *s = st_new(ST_SET, line);
        s->u.set_.lhs = buf;
        s->u.set_.rhs = rhs;
        return s;
    }
    if (ps_is_ident(ps, "yield_tick")) {
        int line = t->line;
        lx_next_token(&ps->lx);
        return st_new(ST_YIELD, line);
    }
    if (ps_is_ident(ps, "stop_block")) {
        int line = t->line;
        lx_next_token(&ps->lx);
        return st_new(ST_STOP, line);
    }
    dief("%s:%d: expected action stmt", ps->filename, t->line);
    return NULL;
}
static Stmt *parse_stmt(Parser *ps) {
    Token *t = &ps->lx.cur;
    if (ps_is_ident(ps, "let")) {
        int line = t->line;
        lx_next_token(&ps->lx);
        char *name = ps_expect_ident(ps, "let name");
        ps_expect(ps, TK_ASSIGN, "=");
        Expr *val = parse_expr(ps);
        ps_expect(ps, TK_SEMI, ";");
        Stmt *s = st_new(ST_LET, line);
        s->u.let_.name = name;
        s->u.let_.value = val;
        return s;
    }
    if (ps_is_ident(ps, "if")) {
        int line = t->line;
        lx_next_token(&ps->lx);
        Expr *cond = parse_expr(ps);
        ps_expect(ps, TK_LBRACE, "{");
        VecStmtPtr then_stmts;
        VEC_INIT(then_stmts);
        parse_stmt_list(ps, &then_stmts);
        ps_expect(ps, TK_RBRACE, "}");
        VecStmtPtr else_stmts;
        VEC_INIT(else_stmts);
        if (ps_is_ident(ps, "else")) {
            lx_next_token(&ps->lx);
            if (ps_is_ident(ps, "if")) {
                /* else-if: parse nested if as a single statement in else block */
                Stmt *nested = parse_stmt(ps);
                VEC_PUSH(else_stmts, nested);
            } else {
                ps_expect(ps, TK_LBRACE, "{");
                parse_stmt_list(ps, &else_stmts);
                ps_expect(ps, TK_RBRACE, "}");
            }
        }
        Stmt *s = st_new(ST_IF, line);
        s->u.if_.cond = cond;
        s->u.if_.then_stmts = then_stmts;
        s->u.if_.else_stmts = else_stmts;
        return s;
    }
    Stmt *a = parse_action_stmt(ps);
    ps_expect(ps, TK_SEMI, ";");
    return a;
}
void parse_stmt_list(Parser *ps, VecStmtPtr *out) {
    while (!ps_is(ps, TK_RBRACE) && !ps_is(ps, TK_EOF)) {
        Stmt *s = parse_stmt(ps);
        VEC_PUSH(*out, s);
    }
}

/* Shared helper: skip an arbitrary {...} block (used by parser for unknown/ignored blocks) */
static void skip_block(Parser *ps) {
    int depth = 0;
    if (ps_is(ps, TK_LBRACE)) {
        ps_expect(ps, TK_LBRACE, "{");
        depth = 1;
    }
    while (depth>0 && !ps_is(ps, TK_EOF)) {
        if (ps_is(ps, TK_LBRACE)) {
            ps_expect(ps, TK_LBRACE, "{");
            depth++;
            continue;
        }
        if (ps_is(ps, TK_RBRACE)) {
            ps_expect(ps, TK_RBRACE, "}");
            depth--;
            continue;
        }
        lx_next_token(&ps->lx);
    }
}
