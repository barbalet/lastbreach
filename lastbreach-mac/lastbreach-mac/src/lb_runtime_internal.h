#ifndef LB_RUNTIME_INTERNAL_H
#define LB_RUNTIME_INTERNAL_H

#include "lastbreach.h"

/*
 * Private runtime interfaces shared by split runtime modules.
 *
 * Ownership:
 * - lb_eval.c      : EvalCtx helpers + eval_expr
 * - lb_scheduler.c : Candidate helpers + choose_action
 * - lb_sim.c       : simulation loop consuming Candidate/choose_action
 *
 * This header is internal to src/ runtime modules and intentionally not exposed
 * in include/lastbreach.h.
 */

typedef struct {
    Character *ch;
    World *w;
    int tick, day;
    int breach_level;
    int ev_breach, ev_overnight;
    VecStr keys;
    VecDbl vals;
} EvalCtx;

typedef struct {
    int kind;
    /* 0 none/idle, 1 task, 3 yield */
    const char *task_name;
    int ticks;
    double priority;
    const char *station;
    int stop_block;
} Candidate;

void ectx_init(EvalCtx *c);
void ectx_clear(EvalCtx *c);
void ectx_set(EvalCtx *c, const char *k, double v);
int ectx_get(EvalCtx *c, const char *k, double *out);
int truthy(double v);
double eval_expr(EvalCtx *ctx, Expr *e);

void cand_reset(Candidate *c);
Candidate choose_action(Character *ch, World *w, Catalog *cat, int day, int tick, int breach_level, int ev_breach, int ev_overnight);

#endif
