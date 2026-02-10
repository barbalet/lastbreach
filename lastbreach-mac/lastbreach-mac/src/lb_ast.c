#include "lastbreach.h"
/**
 * lb_ast.c
 *
 * Module: AST/Character initialization helpers and small constructors.
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */


/** Initializes a character with baseline vitals and empty rule lists. */
void character_init(Character *c) {
    memset(c, 0, sizeof(*c));
    c->hunger = 75;
    c->hydration = 75;
    c->fatigue = 20;
    c->morale = 55;
    c->injury = 0;
    c->illness = 0;
    c->defense_posture = xstrdup("quiet");
    VEC_INIT(c->skill_keys);
    VEC_INIT(c->skill_vals);
    VEC_INIT(c->traits);
    VEC_INIT(c->thresholds);
    VEC_INIT(c->blocks);
    VEC_INIT(c->rules);
    VEC_INIT(c->on_events);
    c->rt_task = NULL;
    c->rt_station = NULL;
    c->rt_remaining = 0;
    c->rt_priority = 0;
}
