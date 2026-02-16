#include "test_framework.h"
#include "test_support.h"

static void test_lexer_tokens(void) {
    /* Covers literal/token edge cases, comment skipping, and operator lexing. */
    char *src = xstrdup("alpha 12 3t 45% /* skip */ \"hello\" .. <= >= != == # done\n");
    Lexer lx;
    char *tok;

    memset(&lx, 0, sizeof(lx));
    lx.src = src;
    lx.len = strlen(src);
    lx.pos = 0;
    lx.line = 1;

    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_IDENT, lx.cur.kind);
    tok = tk_cstr(&lx.cur);
    ASSERT_STREQ("alpha", tok);
    free(tok);

    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_NUMBER, lx.cur.kind);
    ASSERT_EQ_DBL(12.0, lx.cur.num, 1e-9);

    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_DURATION, lx.cur.kind);
    ASSERT_EQ_INT(3, lx.cur.iticks);

    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_PERCENT, lx.cur.kind);
    ASSERT_EQ_DBL(45.0, lx.cur.num, 1e-9);

    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_STRING, lx.cur.kind);
    tok = tk_cstr(&lx.cur);
    ASSERT_STREQ("hello", tok);
    free(tok);

    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_DOTDOT, lx.cur.kind);
    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_LTE, lx.cur.kind);
    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_GTE, lx.cur.kind);
    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_NEQ, lx.cur.kind);
    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_EQ, lx.cur.kind);
    lx_next_token(&lx);
    ASSERT_EQ_INT(TK_EOF, lx.cur.kind);

    free(src);
}

static void test_parse_world_and_catalog(void) {
    /* Ensures parser consumes known fields and safely ignores unknown ones. */
    const char *catalog_src =
        "taskdef \"Custom\" { time: 3t; station: bench; ignores: { x: 1; }; }\n"
        "taskdef \"Minimal\" { station: lab; }\n"
        "itemdef \"Food\" { stackable: true; }\n";
    const char *world_src =
        "world \"Unit\" {\n"
        "  shelter { temp_c: 7; signature: 9; power: 12; water_safe: 8; water_raw: 5; structure: 80; contamination: 11; }\n"
        "  inventory { \"Food\": qty 4; \"Rifle\": qty 1, cond 70; }\n"
        "  events { daily \"breach\" chance 12%; overnight_threat_check chance 33%; }\n"
        "  constants { DAY_TICKS: 24; }\n"
        "}\n";
    Catalog cat;
    World w;
    TaskDef *custom;
    TaskDef *minimal;

    cat_init(&cat);
    parse_catalog_text("catalog_test", catalog_src, &cat);
    custom = cat_find_task(&cat, "Custom");
    minimal = cat_find_task(&cat, "Minimal");

    ASSERT_TRUE(custom != NULL);
    ASSERT_EQ_INT(3, custom->time_ticks);
    ASSERT_STREQ("bench", custom->station);
    ASSERT_TRUE(minimal != NULL);
    ASSERT_EQ_INT(1, minimal->time_ticks);
    ASSERT_STREQ("lab", minimal->station);

    world_init(&w);
    parse_world_text("world_test", world_src, &w);
    ASSERT_EQ_DBL(7.0, w.shelter.temp_c, 1e-9);
    ASSERT_EQ_DBL(9.0, w.shelter.signature, 1e-9);
    ASSERT_EQ_DBL(12.0, w.shelter.power, 1e-9);
    ASSERT_EQ_DBL(8.0, w.shelter.water_safe, 1e-9);
    ASSERT_EQ_DBL(5.0, w.shelter.water_raw, 1e-9);
    ASSERT_EQ_DBL(80.0, w.shelter.structure, 1e-9);
    ASSERT_EQ_DBL(11.0, w.shelter.contamination, 1e-9);
    ASSERT_EQ_DBL(4.0, inv_stock(&w.inv, "Food"), 1e-9);
    ASSERT_EQ_DBL(1.0, inv_stock(&w.inv, "Rifle"), 1e-9);
    ASSERT_EQ_DBL(70.0, inv_cond(&w.inv, "Rifle"), 1e-9);
    ASSERT_EQ_DBL(12.0, w.events.breach_chance, 1e-9);
    ASSERT_EQ_DBL(33.0, w.events.overnight_chance, 1e-9);
}

static void test_parse_character_sections(void) {
    /* Smoke-tests all major character sections in a single DSL fixture. */
    const char *ch_src =
        "character \"Unit\" {\n"
        "  version 1;\n"
        "  skills { gardening: 2; cooking: 3; }\n"
        "  traits: [\"calm\", \"focused\"];\n"
        "  defaults { defense_posture: \"loud\"; ignored_num: 1; ignored_str: \"x\"; }\n"
        "  thresholds { when char.hunger < 40 do task \"Eating\" for 1t priority 99; }\n"
        "  plan {\n"
        "    block day 0..12 {\n"
        "      let x = 1 + 2 * 3;\n"
        "      if x > 6 { task \"Reading\" for 1t priority 60; } else { task \"Talking\" for 1t priority 50; }\n"
        "      yield_tick;\n"
        "    }\n"
        "    rule \"fallback\" priority 5 { task \"Resting\"; }\n"
        "  }\n"
        "  on \"breach\" when breach.level > 1 priority 88 { task \"Defensive combat\" for 2t; }\n"
        "}\n";
    Character ch;

    parse_character_text("character_test", ch_src, &ch);
    ASSERT_STREQ("Unit", ch.name);
    ASSERT_EQ_INT(2, ch.skill_keys.n);
    ASSERT_STREQ("gardening", ch.skill_keys.v[0]);
    ASSERT_EQ_DBL(2.0, ch.skill_vals.v[0], 1e-9);
    ASSERT_EQ_INT(2, ch.traits.n);
    ASSERT_STREQ("calm", ch.traits.v[0]);
    ASSERT_STREQ("loud", ch.defense_posture);

    ASSERT_EQ_INT(1, ch.thresholds.n);
    ASSERT_EQ_INT(ST_TASK, ch.thresholds.v[0].action->kind);
    ASSERT_STREQ("Eating", ch.thresholds.v[0].action->u.task.task_name);

    ASSERT_EQ_INT(1, ch.blocks.n);
    ASSERT_STREQ("day", ch.blocks.v[0].name);
    ASSERT_EQ_INT(0, ch.blocks.v[0].start_tick);
    ASSERT_EQ_INT(12, ch.blocks.v[0].end_tick);

    ASSERT_EQ_INT(1, ch.rules.n);
    ASSERT_STREQ("fallback", ch.rules.v[0].label);
    ASSERT_EQ_DBL(5.0, ch.rules.v[0].priority, 1e-9);

    ASSERT_EQ_INT(1, ch.on_events.n);
    ASSERT_STREQ("breach", ch.on_events.v[0].event_name);
    ASSERT_EQ_DBL(88.0, ch.on_events.v[0].priority, 1e-9);
}

static void test_eval_expressions(void) {
    /* Validates arithmetic, built-ins, booleans, and runtime variable access. */
    World w;
    Character ch;
    EvalCtx ctx;
    char *src = NULL;
    Expr *expr;
    double out;

    world_init(&w);
    character_init(&ch);
    inv_add(&w.inv, "Food", 3.0, 0.0);
    inv_add(&w.inv, "Rifle", 1.0, 80.0);
    w.shelter.power = 33.0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.ch = &ch;
    ctx.w = &w;
    ctx.tick = 7;
    ctx.day = 2;
    ctx.breach_level = 3;
    ctx.ev_breach = 1;
    ctx.ev_overnight = 0;
    ectx_init(&ctx);

    expr = parse_expr_text("expr1", "1 + 2 * 3", &src);
    out = eval_expr(&ctx, expr);
    ASSERT_EQ_DBL(7.0, out, 1e-9);
    free(src);

    expr = parse_expr_text("expr2", "stock(\"Food\") + cond(\"Rifle\")", &src);
    out = eval_expr(&ctx, expr);
    ASSERT_EQ_DBL(83.0, out, 1e-9);
    free(src);

    expr = parse_expr_text("expr3", "has(\"Rifle\") and cond(\"Rifle\") > 70", &src);
    out = eval_expr(&ctx, expr);
    ASSERT_EQ_DBL(1.0, out, 1e-9);
    free(src);

    expr = parse_expr_text("expr4", "event(\"breach\") + event(\"overnight_threat_check\")", &src);
    out = eval_expr(&ctx, expr);
    ASSERT_EQ_DBL(1.0, out, 1e-9);
    free(src);

    expr = parse_expr_text("expr5", "tick + day + breach.level + shelter.power", &src);
    out = eval_expr(&ctx, expr);
    ASSERT_EQ_DBL(45.0, out, 1e-9);
    free(src);

    ectx_clear(&ctx);
}

static void test_parse_task_optional_clauses(void) {
    /* Optional task clauses should parse without changing core task fields. */
    const char *src =
        "character \"Clauses\" {\n"
        "  version 1;\n"
        "  plan {\n"
        "    block day 0..24 {\n"
        "      task \"Water filtration\" for 2t using { \"Water filter\": 1; } requires [\"Bucket\"] consumes { \"Water\": 1; } when shelter.water_raw > 0 priority 70;\n"
        "    }\n"
        "  }\n"
        "}\n";
    Character ch;
    World w;
    EvalCtx ctx;
    Stmt *s;
    double ticks;
    double priority;

    parse_character_text("optional_clauses", src, &ch);
    ASSERT_EQ_INT(1, ch.blocks.n);
    ASSERT_EQ_INT(1, ch.blocks.v[0].stmts.n);
    s = ch.blocks.v[0].stmts.v[0];

    ASSERT_EQ_INT(ST_TASK, s->kind);
    ASSERT_STREQ("Water filtration", s->u.task.task_name);
    ASSERT_TRUE(s->u.task.for_ticks != NULL);
    ASSERT_TRUE(s->u.task.priority != NULL);

    world_init(&w);
    memset(&ctx, 0, sizeof(ctx));
    ctx.ch = &ch;
    ctx.w = &w;
    ectx_init(&ctx);
    ticks = eval_expr(&ctx, s->u.task.for_ticks);
    priority = eval_expr(&ctx, s->u.task.priority);
    ASSERT_EQ_DBL(2.0, ticks, 1e-9);
    ASSERT_EQ_DBL(70.0, priority, 1e-9);
    ectx_clear(&ctx);
}

void register_parser_eval_tests(void) {
    /* Keep registration order aligned with parser/eval workflow complexity. */
    test_run_case("lexer tokens", test_lexer_tokens);
    test_run_case("parse world/catalog", test_parse_world_and_catalog);
    test_run_case("parse character sections", test_parse_character_sections);
    test_run_case("eval expressions", test_eval_expressions);
    test_run_case("parse task optional clauses", test_parse_task_optional_clauses);
}
