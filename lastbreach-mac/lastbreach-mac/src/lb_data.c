#include "lastbreach.h"
/**
 * lb_data.c
 *
 * Module: Parsing helpers for .lbw (world) and .lbc (catalog) data files.
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */

static void skip_block(Parser *ps) {
    /* Generic balanced-brace skipper for unsupported nested DSL sections. */
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
static void skip_until_semi(Parser *ps) {
    /* Used to ignore optional clauses such as "when ..." that we do not evaluate here. */
    while (!ps_is(ps, TK_SEMI) && !ps_is(ps, TK_EOF)) lx_next_token(&ps->lx);
}
static char *parse_string_or_ident(Parser *ps, const char *what) {
    if (ps_is(ps, TK_STRING)) return ps_expect_string(ps, what);
    if (ps_is(ps, TK_IDENT)) return ps_expect_ident(ps, what);
    dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
    return NULL;
}
static int string_is_empty_marker(const char *s) {
    return !s || s[0]=='\0' || strcmp(s, "none")==0 || strcmp(s, "null")==0 || strcmp(s, "-")==0;
}
static void skip_statement_value(Parser *ps) {
    if (ps_is(ps, TK_LBRACE)) {
        skip_block(ps);
        return;
    }
    while (!ps_is(ps, TK_SEMI) && !ps_is(ps, TK_RBRACE) && !ps_is(ps, TK_EOF)) {
        lx_next_token(&ps->lx);
    }
    if (ps_is(ps, TK_SEMI)) ps_expect(ps, TK_SEMI, ";");
}
static void parse_simulation_state(Parser *ps, World *w) {
    ps_expect(ps, TK_LBRACE, "{");
    while (!ps_is(ps, TK_RBRACE) && !ps_is(ps, TK_EOF)) {
        char *k = ps_expect_ident(ps, "simulation_state key");
        ps_expect(ps, TK_COLON, ":");
        if (strcmp(k, "hydroponic_health")==0 || strcmp(k, "hydroponics_health")==0) {
            w->hydroponic_health = ps_expect_number(ps, "hydroponic health");
            ps_expect(ps, TK_SEMI, ";");
        } else if (strcmp(k, "plants_watered_today")==0) {
            w->plants_watered_today = (int)(ps_expect_number(ps, "plants watered flag")+0.5);
            ps_expect(ps, TK_SEMI, ";");
        } else if (strcmp(k, "hydroponics_maintained_today")==0 || strcmp(k, "maintained_today")==0) {
            w->hydroponics_maintained_today = (int)(ps_expect_number(ps, "hydroponics maintained flag")+0.5);
            ps_expect(ps, TK_SEMI, ";");
        } else if (strcmp(k, "cooked_food_portions")==0) {
            w->cooked_food_portions = ps_expect_number(ps, "cooked food portions");
            ps_expect(ps, TK_SEMI, ";");
        } else {
            skip_statement_value(ps);
        }
        free(k);
    }
    ps_expect(ps, TK_RBRACE, "}");
}

/** parse_world function. */
void parse_world(World *w, const char *filename, char *src) {
    Parser ps;
    ps_init(&ps, filename, src);
    /* Allow files with preamble content before the world block. */
    while (!ps_is(&ps, TK_EOF) && !ps_is_ident(&ps, "world")) lx_next_token(&ps.lx);
    if (ps_is(&ps, TK_EOF)) return;
    lx_next_token(&ps.lx);
    if (ps_is(&ps, TK_STRING)) {
        char *tmp = ps_expect_string(&ps, "world name");
        free(tmp);
    }
    ps_expect(&ps, TK_LBRACE, "{");
    while (!ps_is(&ps, TK_RBRACE) && !ps_is(&ps, TK_EOF)) {
        if (ps_is_ident(&ps, "version")) {
            lx_next_token(&ps.lx);
            (void)ps_expect_number(&ps, "version");
            ps_expect(&ps, TK_SEMI, ";");
            continue;
        }
        if (ps_is_ident(&ps, "shelter")) {
            lx_next_token(&ps.lx);
            ps_expect(&ps, TK_LBRACE, "{");
            while (!ps_is(&ps, TK_RBRACE)) {
                char *k = ps_expect_ident(&ps, "shelter key");
                ps_expect(&ps, TK_COLON, ":");
                double v = ps_expect_number(&ps, "number");
                ps_expect(&ps, TK_SEMI, ";");
                /* Apply only shelter keys the runtime currently models. */
                if (strcmp(k, "temp_c")==0) w->shelter.temp_c = v;
                else if (strcmp(k, "signature")==0) w->shelter.signature = v;
                else if (strcmp(k, "power")==0) w->shelter.power = v;
                else if (strcmp(k, "water_safe")==0) w->shelter.water_safe = v;
                else if (strcmp(k, "water_raw")==0) w->shelter.water_raw = v;
                else if (strcmp(k, "structure")==0) w->shelter.structure = v;
                else if (strcmp(k, "contamination")==0) w->shelter.contamination = v;
                free(k);
            }
            ps_expect(&ps, TK_RBRACE, "}");
            continue;
        }
        if (ps_is_ident(&ps, "inventory")) {
            lx_next_token(&ps.lx);
            ps_expect(&ps, TK_LBRACE, "{");
            while (!ps_is(&ps, TK_RBRACE)) {
                char *item = ps_expect_string(&ps, "item");
                ps_expect(&ps, TK_COLON, ":");
                if (!ps_is_ident(&ps, "qty")) dief("%s:%d: expected qty", filename, ps.lx.cur.line);
                lx_next_token(&ps.lx);
                double qty = ps_expect_number(&ps, "qty");
                double cond = 0.0;
                if (ps_is(&ps, TK_COMMA)) {
                    ps_expect(&ps, TK_COMMA, ", ");
                    if (!ps_is_ident(&ps, "cond")) dief("%s:%d: expected cond", filename, ps.lx.cur.line);
                    lx_next_token(&ps.lx);
                    cond = ps_expect_number(&ps, "cond");
                }
                ps_expect(&ps, TK_SEMI, ";");
                /* Repeated item lines accumulate quantity and best condition. */
                inv_add(&w->inv, item, qty, cond);
                free(item);
            }
            ps_expect(&ps, TK_RBRACE, "}");
            continue;
        }
        if (ps_is_ident(&ps, "events")) {
            lx_next_token(&ps.lx);
            ps_expect(&ps, TK_LBRACE, "{");
            while (!ps_is(&ps, TK_RBRACE)) {
                if (ps_is_ident(&ps, "daily")) {
                    lx_next_token(&ps.lx);
                    char *ename = ps_expect_string(&ps, "event name");
                    if (!ps_is_ident(&ps, "chance")) dief("%s:%d: expected chance", filename, ps.lx.cur.line);
                    lx_next_token(&ps.lx);
                    double ch = ps_expect_percent(&ps, "percent");
                    if (ps_is_ident(&ps, "when")) {
                        lx_next_token(&ps.lx);
                        skip_until_semi(&ps);
                    }
                    ps_expect(&ps, TK_SEMI, ";");
                    /*
                     * Only events consumed by the simulator are persisted.
                     * Unknown daily events are parsed/ignored for compatibility.
                     */
                    if (strcmp(ename, "breach")==0) w->events.breach_chance = ch;
                    free(ename);
                    continue;
                }
                if (ps_is_ident(&ps, "overnight_threat_check")) {
                    lx_next_token(&ps.lx);
                    if (!ps_is_ident(&ps, "chance")) dief("%s:%d: expected chance", filename, ps.lx.cur.line);
                    lx_next_token(&ps.lx);
                    double ch = ps_expect_percent(&ps, "percent");
                    if (ps_is_ident(&ps, "when")) {
                        lx_next_token(&ps.lx);
                        skip_until_semi(&ps);
                    }
                    ps_expect(&ps, TK_SEMI, ";");
                    w->events.overnight_chance = ch;
                    continue;
                }
                dief("%s:%d: unknown events entry", filename, ps.lx.cur.line);
            }
            ps_expect(&ps, TK_RBRACE, "}");
            continue;
        }
        if (ps_is_ident(&ps, "simulation_state")) {
            lx_next_token(&ps.lx);
            parse_simulation_state(&ps, w);
            continue;
        }
        /* Ignore other blocks (constants/weather/...) while staying token-synchronized. */
        if (ps_is(&ps, TK_IDENT)) {
            char *k = ps_expect_ident(&ps, "ident");
            if (ps_is(&ps, TK_LBRACE)) skip_block(&ps);
            else if (ps_is(&ps, TK_SEMI)) ps_expect(&ps, TK_SEMI, ";");
            else {
                while (!ps_is(&ps, TK_SEMI) && !ps_is(&ps, TK_EOF)) lx_next_token(&ps.lx);
                if (ps_is(&ps, TK_SEMI)) ps_expect(&ps, TK_SEMI, ";");
            }
            free(k);
            continue;
        }
        lx_next_token(&ps.lx);
    }
    if (ps_is(&ps, TK_RBRACE)) ps_expect(&ps, TK_RBRACE, "}");
}

static Character *find_character_override(Character *A, Character *B, const char *name) {
    if (A && A->name && strcmp(A->name, name)==0) return A;
    if (B && B->name && strcmp(B->name, name)==0) return B;
    return NULL;
}

static void parse_character_override_body(Parser *ps, Character *ch) {
    ps_expect(ps, TK_LBRACE, "{");
    while (!ps_is(ps, TK_RBRACE) && !ps_is(ps, TK_EOF)) {
        char *k = ps_expect_ident(ps, "character override key");
        ps_expect(ps, TK_COLON, ":");

        if (strcmp(k, "hunger")==0) ch->hunger = ps_expect_number(ps, "hunger");
        else if (strcmp(k, "hydration")==0) ch->hydration = ps_expect_number(ps, "hydration");
        else if (strcmp(k, "fatigue")==0) ch->fatigue = ps_expect_number(ps, "fatigue");
        else if (strcmp(k, "morale")==0) ch->morale = ps_expect_number(ps, "morale");
        else if (strcmp(k, "injury")==0) ch->injury = ps_expect_number(ps, "injury");
        else if (strcmp(k, "illness")==0) ch->illness = ps_expect_number(ps, "illness");
        else if (strcmp(k, "remaining_ticks")==0) ch->rt_remaining = (int)(ps_expect_number(ps, "remaining ticks")+0.5);
        else if (strcmp(k, "priority")==0) ch->rt_priority = ps_expect_number(ps, "priority");
        else if (strcmp(k, "defense_posture")==0) {
            char *value = parse_string_or_ident(ps, "defense posture");
            free(ch->defense_posture);
            ch->defense_posture = string_is_empty_marker(value) ? NULL : xstrdup(value);
            free(value);
        } else if (strcmp(k, "active_task")==0) {
            char *value = parse_string_or_ident(ps, "active task");
            ch->rt_task = string_is_empty_marker(value) ? NULL : xstrdup(value);
            free(value);
        } else if (strcmp(k, "station")==0) {
            char *value = parse_string_or_ident(ps, "station");
            ch->rt_station = string_is_empty_marker(value) ? NULL : xstrdup(value);
            free(value);
        } else {
            skip_statement_value(ps);
            free(k);
            continue;
        }

        ps_expect(ps, TK_SEMI, ";");
        free(k);
    }
    ps_expect(ps, TK_RBRACE, "}");
    if (!ch->rt_task) {
        ch->rt_remaining = 0;
        ch->rt_station = NULL;
        ch->rt_priority = 0;
    }
}

void parse_world_character_overrides(const char *filename, char *src, Character *A, Character *B) {
    Parser ps;
    ps_init(&ps, filename, src);
    while (!ps_is(&ps, TK_EOF) && !ps_is_ident(&ps, "world")) lx_next_token(&ps.lx);
    if (ps_is(&ps, TK_EOF)) return;
    lx_next_token(&ps.lx);
    if (ps_is(&ps, TK_STRING)) {
        char *tmp = ps_expect_string(&ps, "world name");
        free(tmp);
    }
    ps_expect(&ps, TK_LBRACE, "{");
    while (!ps_is(&ps, TK_RBRACE) && !ps_is(&ps, TK_EOF)) {
        if (ps_is_ident(&ps, "characters")) {
            lx_next_token(&ps.lx);
            ps_expect(&ps, TK_LBRACE, "{");
            while (!ps_is(&ps, TK_RBRACE) && !ps_is(&ps, TK_EOF)) {
                char *name = ps_expect_string(&ps, "character name");
                Character *ch = find_character_override(A, B, name);
                if (ch) parse_character_override_body(&ps, ch);
                else if (ps_is(&ps, TK_LBRACE)) skip_block(&ps);
                else skip_statement_value(&ps);
                free(name);
            }
            ps_expect(&ps, TK_RBRACE, "}");
            continue;
        }
        if (ps_is(&ps, TK_IDENT)) {
            char *k = ps_expect_ident(&ps, "world entry");
            if (ps_is(&ps, TK_LBRACE)) skip_block(&ps);
            else skip_statement_value(&ps);
            free(k);
            continue;
        }
        lx_next_token(&ps.lx);
    }
}

/** parse_catalog function. */
void parse_catalog(Catalog *cat, const char *filename, char *src) {
    Parser ps;
    ps_init(&ps, filename, src);
    while (!ps_is(&ps, TK_EOF)) {
        if (ps_is_ident(&ps, "taskdef")) {
            lx_next_token(&ps.lx);
            char *tname = ps_expect_string(&ps, "task name");
            TaskDef *td = cat_get_or_add_task(cat, tname);
            free(tname);
            ps_expect(&ps, TK_LBRACE, "{");
            while (!ps_is(&ps, TK_RBRACE)) {
                /* Keep taskdef parsing permissive: consume known fields, tolerate extras. */
                if (ps_is_ident(&ps, "time")) {
                    lx_next_token(&ps.lx);
                    ps_expect(&ps, TK_COLON, ":");
                    int ticks = (int)(ps_expect_number(&ps, "ticks")+0.5);
                    ps_expect(&ps, TK_SEMI, ";");
                    td->time_ticks = (ticks<=0)?1:ticks;
                    continue;
                }
                if (ps_is_ident(&ps, "station")) {
                    lx_next_token(&ps.lx);
                    ps_expect(&ps, TK_COLON, ":");
                    char *st = ps_expect_ident(&ps, "station");
                    ps_expect(&ps, TK_SEMI, ";");
                    if (td->station) free(td->station);
                    td->station = st;
                    continue;
                }
                if (ps_is(&ps, TK_IDENT)) {
                    char *k = ps_expect_ident(&ps, "field");
                    if (ps_is(&ps, TK_COLON)) {
                        ps_expect(&ps, TK_COLON, ":");
                        while (!ps_is(&ps, TK_SEMI) && !ps_is(&ps, TK_EOF)) {
                            if (ps_is(&ps, TK_LBRACE)) {
                                skip_block(&ps);
                                break;
                            }
                            lx_next_token(&ps.lx);
                        }
                        if (ps_is(&ps, TK_SEMI)) ps_expect(&ps, TK_SEMI, ";");
                    }

                    /* Nested object payload for unsupported fields. */
                    else if (ps_is(&ps, TK_LBRACE)) {
                        skip_block(&ps);
                    }

                    /* Bare marker field terminated with semicolon. */
                    else if (ps_is(&ps, TK_SEMI)) {
                        ps_expect(&ps, TK_SEMI, ";");
                    } else {
                        while (!ps_is(&ps, TK_SEMI) && !ps_is(&ps, TK_EOF)) lx_next_token(&ps.lx);
                        if (ps_is(&ps, TK_SEMI)) ps_expect(&ps, TK_SEMI, ";");
                    }
                    free(k);
                    continue;
                }
                lx_next_token(&ps.lx);
            }
            ps_expect(&ps, TK_RBRACE, "}");
            continue;
        }
        if (ps_is_ident(&ps, "itemdef")) {
            lx_next_token(&ps.lx);
            char *nm = ps_expect_string(&ps, "item name");
            free(nm);
            if (ps_is(&ps, TK_LBRACE)) skip_block(&ps);
            continue;
        }
        lx_next_token(&ps.lx);
    }
}
