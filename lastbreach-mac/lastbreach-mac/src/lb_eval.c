#include "lb_runtime_internal.h"
/**
 * lb_eval.c
 *
 * Module: Runtime expression-evaluation context and AST evaluation.
 */

void ectx_init(EvalCtx *c) {
    VEC_INIT(c->keys);
    VEC_INIT(c->vals);
}
void ectx_clear(EvalCtx *c) {
    for (int i = 0; i<c->keys.n; i++) free(c->keys.v[i]);
    VEC_FREE(c->keys);
    VEC_FREE(c->vals);
    VEC_INIT(c->keys);
    VEC_INIT(c->vals);
}
void ectx_set(EvalCtx *c, const char *k, double v) {
    for (int i = 0; i<c->keys.n; i++) if (strcmp(c->keys.v[i], k)==0) {
            c->vals.v[i] = v;
            return;
        }
    VEC_PUSH(c->keys, xstrdup(k));
    VEC_PUSH(c->vals, v);
}
int ectx_get(EvalCtx *c, const char *k, double *out) {
    for (int i = 0; i<c->keys.n; i++) if (strcmp(c->keys.v[i], k)==0) {
            *out = c->vals.v[i];
            return 1;
        }
    return 0;
}
int truthy(double v) {
    return v!=0.0;
}
double eval_expr(EvalCtx *ctx, Expr *e);
static double eval_call(EvalCtx *ctx, CallExpr *c) {
    const char *name = c->name;
    if ((strcmp(name, "stock")==0)||(strcmp(name, "has")==0)||(strcmp(name, "cond")==0)||(strcmp(name, "event")==0)) {
        if (c->args.n<1) return 0.0;
        Expr *a0 = c->args.v[0];
        if (a0->kind!=EX_STRING) return 0.0;
        const char *s = a0->u.str;
        if (strcmp(name, "stock")==0) return inv_stock(&ctx->w->inv, s);
        if (strcmp(name, "has")==0) return inv_has(&ctx->w->inv, s)?1.0:0.0;
        if (strcmp(name, "cond")==0) return inv_cond(&ctx->w->inv, s);
        if (strcmp(name, "event")==0) {
            if (strcmp(s, "breach")==0) return ctx->ev_breach?1.0:0.0;
            if (strcmp(s, "overnight_threat_check")==0) return ctx->ev_overnight?1.0:0.0;
            return 0.0;
        }
    }
    return 0.0;
}
static double eval_var(EvalCtx *ctx, const char *v) {
    double out = 0;
    if (ectx_get(ctx, v, &out)) return out;
    if (strcmp(v, "tick")==0) return (double)ctx->tick;
    if (strcmp(v, "day")==0) return (double)ctx->day;
    if (strcmp(v, "breach.level")==0) return (double)ctx->breach_level;
    if (strcmp(v, "char.hunger")==0) return ctx->ch->hunger;
    if (strcmp(v, "char.hydration")==0) return ctx->ch->hydration;
    if (strcmp(v, "char.fatigue")==0) return ctx->ch->fatigue;
    if (strcmp(v, "char.morale")==0) return ctx->ch->morale;
    if (strcmp(v, "char.injury")==0) return ctx->ch->injury;
    if (strcmp(v, "char.illness")==0) return ctx->ch->illness;
    if (strcmp(v, "shelter.temp_c")==0) return ctx->w->shelter.temp_c;
    if (strcmp(v, "shelter.signature")==0) return ctx->w->shelter.signature;
    if (strcmp(v, "shelter.power")==0) return ctx->w->shelter.power;
    if (strcmp(v, "shelter.water_safe")==0) return ctx->w->shelter.water_safe;
    if (strcmp(v, "shelter.water_raw")==0) return ctx->w->shelter.water_raw;
    if (strcmp(v, "shelter.structure")==0) return ctx->w->shelter.structure;
    if (strcmp(v, "shelter.contamination")==0) return ctx->w->shelter.contamination;
    return 0.0;
}
double eval_expr(EvalCtx *ctx, Expr *e) {
    switch (e->kind) {
    case EX_NUM:
        return e->u.num;
    case EX_BOOL:
        return e->u.boolean?1.0:0.0;
    case EX_STRING:
        return 0.0;
    case EX_VAR:
        return eval_var(ctx, e->u.var);
    case EX_CALL:
        return eval_call(ctx, &e->u.call);
    case EX_UNARY: {
        double a = eval_expr(ctx, e->u.un.a);
        if (e->u.un.op==OP_NEG) return -a;
        if (e->u.un.op==OP_NOT) return truthy(a)?0.0:1.0;
        return 0.0;
    }
    case EX_BINARY: {
        double a = eval_expr(ctx, e->u.bin.a);
        double b = eval_expr(ctx, e->u.bin.b);
        switch (e->u.bin.op) {
        case OP_ADD:
            return a+b;
        case OP_SUB:
            return a-b;
        case OP_MUL:
            return a*b;
        case OP_DIV:
            return (b==0)?0:(a/b);
        case OP_EQ:
            return (a==b)?1.0:0.0;
        case OP_NEQ:
            return (a!=b)?1.0:0.0;
        case OP_LT:
            return (a<b)?1.0:0.0;
        case OP_LTE:
            return (a<=b)?1.0:0.0;
        case OP_GT:
            return (a>b)?1.0:0.0;
        case OP_GTE:
            return (a>=b)?1.0:0.0;
        case OP_AND:
            return (truthy(a)&&truthy(b))?1.0:0.0;
        case OP_OR:
            return (truthy(a)||truthy(b))?1.0:0.0;
        default:
            return 0.0;
        }
    }
    default:
        return 0.0;
    }
}
