#include "lb_parser_internal.h"
/**
 * lb_parser_sections.c
 *
 * Module: Character-section parsing (skills/traits/defaults/thresholds/plan/on).
 */

/* ---- Character sections ---- */

static void parse_skills(Parser *ps, Character *ch) {
    /* skills { name: number; ... } */
    ps_expect(ps, TK_LBRACE, "{");
    while (!ps_is(ps, TK_RBRACE)) {
        char *k = ps_expect_ident(ps, "skill name");
        ps_expect(ps, TK_COLON, ":");
        double v = ps_expect_number(ps, "number");
        ps_expect(ps, TK_SEMI, ";");
        VEC_PUSH(ch->skill_keys, k);
        VEC_PUSH(ch->skill_vals, v);
    }
    ps_expect(ps, TK_RBRACE, "}");
}
static void parse_traits(Parser *ps, Character *ch) {
    /* traits: ["trait", ...]; */
    ps_expect(ps, TK_COLON, ":");
    ps_expect(ps, TK_LBRACK, "[");
    if (!ps_is(ps, TK_RBRACK)) {
        for (;;) {
            char *s = ps_expect_string(ps, "trait");
            VEC_PUSH(ch->traits, s);
            if (ps_is(ps, TK_COMMA)) {
                ps_expect(ps, TK_COMMA, ", ");
                continue;
            }
            break;
        }
    }
    ps_expect(ps, TK_RBRACK, "]");
    ps_expect(ps, TK_SEMI, ";");
}
static void parse_defaults(Parser *ps, Character *ch) {
    /*
     * We currently apply only fields that affect runtime behavior directly.
     * Unknown/default-only values are consumed to keep the parser forward-compatible.
     */
    ps_expect(ps, TK_LBRACE, "{");
    while (!ps_is(ps, TK_RBRACE)) {
        char *k = ps_expect_ident(ps, "defaults key");
        ps_expect(ps, TK_COLON, ":");
        if (strcmp(k, "defense_posture")==0) {
            char *v = ps_expect_string(ps, "posture");
            free(ch->defense_posture);
            ch->defense_posture = v;
            ps_expect(ps, TK_SEMI, ";");
            free(k);
            continue;
        }
        if (ps_is(ps, TK_STRING)) {
            char *tmp = ps_expect_string(ps, "string");
            free(tmp);
        } else {
            (void)ps_expect_number(ps, "number");
        }
        ps_expect(ps, TK_SEMI, ";");
        free(k);
    }
    ps_expect(ps, TK_RBRACE, "}");
}
static void parse_thresholds(Parser *ps, Character *ch) {
    /* thresholds { when <expr> do <action>; ... } */
    ps_expect(ps, TK_LBRACE, "{");
    while (!ps_is(ps, TK_RBRACE)) {
        if (!ps_is_ident(ps, "when")) dief("%s:%d: expected when", ps->filename, ps->lx.cur.line);
        lx_next_token(&ps->lx);
        Expr *cond = parse_expr(ps);
        if (!ps_is_ident(ps, "do")) dief("%s:%d: expected do", ps->filename, ps->lx.cur.line);
        lx_next_token(&ps->lx);
        Stmt *action = parse_action_stmt(ps);
        ps_expect(ps, TK_SEMI, ";");
        ThresholdRule tr;
        tr.cond = cond;
        tr.action = action;
        VEC_PUSH(ch->thresholds, tr);
    }
    ps_expect(ps, TK_RBRACE, "}");
}
static int parse_int_lit(Parser *ps) {
    /* Plan ranges are integer-like and accept either NUMBER or DURATION tokens. */
    Token *t = &ps->lx.cur;
    if (ps_is(ps, TK_NUMBER)) {
        int v = (int)(t->num+0.5);
        lx_next_token(&ps->lx);
        return v;
    }
    if (ps_is(ps, TK_DURATION)) {
        int v = t->iticks;
        lx_next_token(&ps->lx);
        return v;
    }
    dief("%s:%d: expected int literal", ps->filename, t->line);
    return 0;
}
static void parse_plan(Parser *ps, Character *ch) {
    /*
     * plan {
     *   block <name> <start>..<end> { ... }
     *   rule ["label"] priority <num> { ... }
     * }
     */
    ps_expect(ps, TK_LBRACE, "{");
    while (!ps_is(ps, TK_RBRACE)) {
        if (ps_is_ident(ps, "block")) {
            lx_next_token(&ps->lx);
            char *bname = ps_expect_ident(ps, "block name");
            int start = parse_int_lit(ps);
            if (ps_is(ps, TK_DOTDOT)) {
                ps_expect(ps, TK_DOTDOT, "..");
            } else {
                /* tolerate lexer producing . . */
                ps_expect(ps, TK_DOT, ".");
                ps_expect(ps, TK_DOT, ".");
            }
            int end = parse_int_lit(ps);
            ps_expect(ps, TK_LBRACE, "{");
            VecStmtPtr stmts;
            VEC_INIT(stmts);
            parse_stmt_list(ps, &stmts);
            ps_expect(ps, TK_RBRACE, "}");
            BlockRule br;
            br.name = bname;
            br.start_tick = start;
            br.end_tick = end;
            br.stmts = stmts;
            VEC_PUSH(ch->blocks, br);
            continue;
        }
        if (ps_is_ident(ps, "rule")) {
            lx_next_token(&ps->lx);
            char *label = NULL;
            if (ps_is(ps, TK_STRING)) label = ps_expect_string(ps, "label");
            if (!ps_is_ident(ps, "priority")) dief("%s:%d: expected priority", ps->filename, ps->lx.cur.line);
            lx_next_token(&ps->lx);
            double pr = ps_expect_number(ps, "priority number");
            ps_expect(ps, TK_LBRACE, "{");
            VecStmtPtr stmts;
            VEC_INIT(stmts);
            parse_stmt_list(ps, &stmts);
            ps_expect(ps, TK_RBRACE, "}");
            GenericRule gr;
            gr.label = label;
            gr.priority = pr;
            gr.stmts = stmts;
            VEC_PUSH(ch->rules, gr);
            continue;
        }
        dief("%s:%d: expected block or rule in plan", ps->filename, ps->lx.cur.line);
    }
    ps_expect(ps, TK_RBRACE, "}");
}
static void parse_on(Parser *ps, Character *ch) {
    /* on "breach" (when expr)? priority <num> { ... } */
    if (!ps_is_ident(ps, "on")) dief("%s:%d: expected on", ps->filename, ps->lx.cur.line);
    lx_next_token(&ps->lx);
    char *ename = ps_expect_string(ps, "event");
    Expr *when_cond = NULL;
    if (ps_is_ident(ps, "when")) {
        lx_next_token(&ps->lx);
        when_cond = parse_expr(ps);
    }
    if (!ps_is_ident(ps, "priority")) dief("%s:%d: expected priority", ps->filename, ps->lx.cur.line);
    lx_next_token(&ps->lx);
    double pr = ps_expect_number(ps, "priority number");
    ps_expect(ps, TK_LBRACE, "{");
    VecStmtPtr stmts;
    VEC_INIT(stmts);
    parse_stmt_list(ps, &stmts);
    ps_expect(ps, TK_RBRACE, "}");
    OnEventRule r;
    r.event_name = ename;
    r.priority = pr;
    r.when_cond = when_cond;
    r.stmts = stmts;
    VEC_PUSH(ch->on_events, r);
}
void parse_character(Parser *ps, Character *out) {
    if (!ps_is_ident(ps, "character")) dief("%s:%d: expected character", ps->filename, ps->lx.cur.line);
    lx_next_token(&ps->lx);
    char *name = ps_expect_string(ps, "character name");
    character_init(out);
    out->name = name;
    ps_expect(ps, TK_LBRACE, "{");
    while (!ps_is(ps, TK_RBRACE)) {
        /* Parse sections in any order; unknown sections are treated as errors. */
        if (ps_is_ident(ps, "version")) {
            lx_next_token(&ps->lx);
            (void)ps_expect_number(ps, "version");
            ps_expect(ps, TK_SEMI, ";");
            continue;
        }
        if (ps_is_ident(ps, "skills")) {
            lx_next_token(&ps->lx);
            parse_skills(ps, out);
            continue;
        }
        if (ps_is_ident(ps, "traits")) {
            lx_next_token(&ps->lx);
            parse_traits(ps, out);
            continue;
        }
        if (ps_is_ident(ps, "defaults")) {
            lx_next_token(&ps->lx);
            parse_defaults(ps, out);
            continue;
        }
        if (ps_is_ident(ps, "thresholds")) {
            lx_next_token(&ps->lx);
            parse_thresholds(ps, out);
            continue;
        }
        if (ps_is_ident(ps, "plan")) {
            lx_next_token(&ps->lx);
            parse_plan(ps, out);
            continue;
        }
        if (ps_is_ident(ps, "on")) {
            parse_on(ps, out);
            continue;
        }
        dief("%s:%d: unexpected token in character block", ps->filename, ps->lx.cur.line);
    }
    ps_expect(ps, TK_RBRACE, "}");
}
