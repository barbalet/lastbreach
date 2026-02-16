#include "lb_runtime_internal.h"
/**
 * lb_scheduler.c
 *
 * Module: Action selection from parsed rules (thresholds/blocks/on-events).
 */

void cand_reset(Candidate *c) {
    memset(c, 0, sizeof(*c));
    c->kind = 0;
    c->priority = -1e9;
}
static void cand_consider_task(Candidate *best, const char *name, int ticks, double pr, const char *station) {
    /* Keep only the highest-priority candidate found so far. */
    if (pr > best->priority) {
        best->kind = 1;
        best->priority = pr;
        best->task_name = name;
        best->ticks = ticks;
        best->station = station;
    }
}
static int exec_stmt_list_select(EvalCtx *ctx, Catalog *cat, const VecStmtPtr *list, double base_priority, Candidate *best) {
    /*
     * Execute scheduler statements in order and mutate `best` as directives are
     * encountered. This is evaluation for selection, not full simulation.
     */
    for (int i = 0; i<list->n; i++) {
        Stmt *s = list->v[i];
        switch (s->kind) {
        case ST_LET: {
            /* let bindings are local to this selection pass. */
            double v = eval_expr(ctx, s->u.let_.value);
            ectx_set(ctx, s->u.let_.name, v);
            break;
        }
        case ST_SET: {
            /*
             * Only runtime-mutable defaults are currently implemented.
             * Unknown `set` targets are intentionally ignored.
             */
            if (strcmp(s->u.set_.lhs, "defaults.defense_posture")==0) {
                Expr *rhs = s->u.set_.rhs;
                if (rhs->kind==EX_STRING) {
                    free(ctx->ch->defense_posture);
                    ctx->ch->defense_posture = xstrdup(rhs->u.str);
                } else {
                    double v = eval_expr(ctx, rhs);
                    free(ctx->ch->defense_posture);
                    ctx->ch->defense_posture = xstrdup(v>=0.5?"loud":"quiet");
                }
            }
            break;
        }
        case ST_TASK: {
            int ticks = 1;
            double pr = base_priority;
            if (s->u.task.for_ticks) ticks = (int)(eval_expr(ctx, s->u.task.for_ticks)+0.5);
            TaskDef *td = cat_find_task(cat, s->u.task.task_name);
            const char *station = td?td->station:NULL;
            if (!s->u.task.for_ticks && td) ticks = td->time_ticks;
            if (s->u.task.priority) pr = eval_expr(ctx, s->u.task.priority);
            if (ticks<=0) ticks = 1;
            cand_consider_task(best, s->u.task.task_name, ticks, pr, station);
            break;
        }
        case ST_IF: {
            /* Branching can recursively discover task candidates. */
            double ok = eval_expr(ctx, s->u.if_.cond);
            if (truthy(ok)) {
                if (exec_stmt_list_select(ctx, cat, &s->u.if_.then_stmts, base_priority, best)) return 1;
            } else {
                if (exec_stmt_list_select(ctx, cat, &s->u.if_.else_stmts, base_priority, best)) return 1;
            }
            break;
        }
        case ST_YIELD: {
            /* Yield means "prefer idle" unless a stronger task is already chosen. */
            if (0 > best->priority) {
                best->kind = 3;
                best->priority = 0;
            }
            break;
        }
        case ST_STOP: {
            /* stop_block short-circuits the current block/rule body. */
            best->stop_block = 1;
            return 1;
        }
        default:
            break;
        }
    }
    return 0;
}
Candidate choose_action(Character *ch, World *w, Catalog *cat, int day, int tick, int breach_level, int ev_breach, int ev_overnight) {
    EvalCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.ch = ch;
    ctx.w = w;
    ctx.day = day;
    ctx.tick = tick;
    ctx.breach_level = breach_level;
    ctx.ev_breach = ev_breach;
    ctx.ev_overnight = ev_overnight;
    ectx_init(&ctx);
    Candidate best;
    cand_reset(&best);
    /*
     * Precedence is intentional and mirrors game design:
     * 1) on-event handlers
     * 2) threshold safety checks
     * 3) plan blocks
     * 4) generic fallback rules
     */
    /* 1) on breach */
    if (ev_breach) {
        for (int i = 0; i<ch->on_events.n; i++) {
            OnEventRule *r = &ch->on_events.v[i];
            if (strcmp(r->event_name, "breach")!=0) continue;
            if (r->when_cond) {
                double ok = eval_expr(&ctx, r->when_cond);
                if (!truthy(ok)) continue;
            }
            Candidate tmp;
            cand_reset(&tmp);
            (void)exec_stmt_list_select(&ctx, cat, &r->stmts, r->priority, &tmp);
            if (tmp.kind==1) cand_consider_task(&best, tmp.task_name, tmp.ticks, tmp.priority, tmp.station);
        }
        if (best.kind==1) {
            ectx_clear(&ctx);
            return best;
        }
    }
    /* 2) thresholds */
    for (int i = 0; i<ch->thresholds.n; i++) {
        ThresholdRule *tr = &ch->thresholds.v[i];
        double ok = eval_expr(&ctx, tr->cond);
        if (!truthy(ok)) continue;
        VecStmtPtr one;
        VEC_INIT(one);
        VEC_PUSH(one, tr->action);
        Candidate tmp;
        cand_reset(&tmp);
        (void)exec_stmt_list_select(&ctx, cat, &one, 0.0, &tmp);
        VEC_FREE(one);
        if (tmp.kind==1) cand_consider_task(&best, tmp.task_name, tmp.ticks, tmp.priority, tmp.station);
    }
    if (best.kind==1) {
        ectx_clear(&ctx);
        return best;
    }
    /* 3) plan blocks */
    for (int i = 0; i<ch->blocks.n; i++) {
        BlockRule *b = &ch->blocks.v[i];
        if (tick < b->start_tick || tick >= b->end_tick) continue;
        Candidate tmp;
        cand_reset(&tmp);
        (void)exec_stmt_list_select(&ctx, cat, &b->stmts, 0.0, &tmp);
        if (tmp.kind==1) cand_consider_task(&best, tmp.task_name, tmp.ticks, tmp.priority, tmp.station);
        if (tmp.stop_block) break;
    }
    /* 4) rules */
    for (int i = 0; i<ch->rules.n; i++) {
        GenericRule *r = &ch->rules.v[i];
        Candidate tmp;
        cand_reset(&tmp);
        (void)exec_stmt_list_select(&ctx, cat, &r->stmts, r->priority, &tmp);
        if (tmp.kind==1) cand_consider_task(&best, tmp.task_name, tmp.ticks, tmp.priority, tmp.station);
    }
    if (best.kind==0) {
        /* Scheduler always returns an explicit action; idle is encoded as yield. */
        best.kind = 3;
        best.priority = 0;
    }
    ectx_clear(&ctx);
    return best;
}
