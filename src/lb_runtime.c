#include "lastbreach.h"

typedef struct {
  Character *ch;
  World *w;
  int tick, day;
  int breach_level;
  int ev_breach, ev_overnight;
  VecStr keys;
  VecDbl vals;
} EvalCtx;

static void ectx_init(EvalCtx *c){ VEC_INIT(c->keys); VEC_INIT(c->vals); }
static void ectx_clear(EvalCtx *c){
  for(int i=0;i<c->keys.n;i++) free(c->keys.v[i]);
  VEC_FREE(c->keys); VEC_FREE(c->vals);
  VEC_INIT(c->keys); VEC_INIT(c->vals);
}
static void ectx_set(EvalCtx *c, const char *k, double v){
  for(int i=0;i<c->keys.n;i++) if(strcmp(c->keys.v[i],k)==0){ c->vals.v[i]=v; return; }
  VEC_PUSH(c->keys, xstrdup(k));
  VEC_PUSH(c->vals, v);
}
static int ectx_get(EvalCtx *c, const char *k, double *out){
  for(int i=0;i<c->keys.n;i++) if(strcmp(c->keys.v[i],k)==0){ *out=c->vals.v[i]; return 1; }
  return 0;
}
static int truthy(double v){ return v!=0.0; }

static double eval_expr(EvalCtx *ctx, Expr *e);

static double eval_call(EvalCtx *ctx, CallExpr *c){
  const char *name=c->name;
  if((strcmp(name,"stock")==0)||(strcmp(name,"has")==0)||(strcmp(name,"cond")==0)||(strcmp(name,"event")==0)){
    if(c->args.n<1) return 0.0;
    Expr *a0=c->args.v[0];
    if(a0->kind!=EX_STRING) return 0.0;
    const char *s=a0->u.str;
    if(strcmp(name,"stock")==0) return inv_stock(&ctx->w->inv,s);
    if(strcmp(name,"has")==0) return inv_has(&ctx->w->inv,s)?1.0:0.0;
    if(strcmp(name,"cond")==0) return inv_cond(&ctx->w->inv,s);
    if(strcmp(name,"event")==0){
      if(strcmp(s,"breach")==0) return ctx->ev_breach?1.0:0.0;
      if(strcmp(s,"overnight_threat_check")==0) return ctx->ev_overnight?1.0:0.0;
      return 0.0;
    }
  }
  return 0.0;
}

static double eval_var(EvalCtx *ctx, const char *v){
  double out=0;
  if(ectx_get(ctx,v,&out)) return out;

  if(strcmp(v,"tick")==0) return (double)ctx->tick;
  if(strcmp(v,"day")==0) return (double)ctx->day;
  if(strcmp(v,"breach.level")==0) return (double)ctx->breach_level;

  if(strcmp(v,"char.hunger")==0) return ctx->ch->hunger;
  if(strcmp(v,"char.hydration")==0) return ctx->ch->hydration;
  if(strcmp(v,"char.fatigue")==0) return ctx->ch->fatigue;
  if(strcmp(v,"char.morale")==0) return ctx->ch->morale;
  if(strcmp(v,"char.injury")==0) return ctx->ch->injury;
  if(strcmp(v,"char.illness")==0) return ctx->ch->illness;

  if(strcmp(v,"shelter.temp_c")==0) return ctx->w->shelter.temp_c;
  if(strcmp(v,"shelter.signature")==0) return ctx->w->shelter.signature;
  if(strcmp(v,"shelter.power")==0) return ctx->w->shelter.power;
  if(strcmp(v,"shelter.water_safe")==0) return ctx->w->shelter.water_safe;
  if(strcmp(v,"shelter.water_raw")==0) return ctx->w->shelter.water_raw;
  if(strcmp(v,"shelter.structure")==0) return ctx->w->shelter.structure;
  if(strcmp(v,"shelter.contamination")==0) return ctx->w->shelter.contamination;

  return 0.0;
}

static double eval_expr(EvalCtx *ctx, Expr *e){
  switch(e->kind){
    case EX_NUM: return e->u.num;
    case EX_BOOL: return e->u.boolean?1.0:0.0;
    case EX_STRING: return 0.0;
    case EX_VAR: return eval_var(ctx,e->u.var);
    case EX_CALL: return eval_call(ctx,&e->u.call);
    case EX_UNARY: {
      double a=eval_expr(ctx,e->u.un.a);
      if(e->u.un.op==OP_NEG) return -a;
      if(e->u.un.op==OP_NOT) return truthy(a)?0.0:1.0;
      return 0.0;
    }
    case EX_BINARY: {
      double a=eval_expr(ctx,e->u.bin.a);
      double b=eval_expr(ctx,e->u.bin.b);
      switch(e->u.bin.op){
        case OP_ADD: return a+b;
        case OP_SUB: return a-b;
        case OP_MUL: return a*b;
        case OP_DIV: return (b==0)?0:(a/b);
        case OP_EQ: return (a==b)?1.0:0.0;
        case OP_NEQ: return (a!=b)?1.0:0.0;
        case OP_LT: return (a<b)?1.0:0.0;
        case OP_LTE: return (a<=b)?1.0:0.0;
        case OP_GT: return (a>b)?1.0:0.0;
        case OP_GTE: return (a>=b)?1.0:0.0;
        case OP_AND: return (truthy(a)&&truthy(b))?1.0:0.0;
        case OP_OR: return (truthy(a)||truthy(b))?1.0:0.0;
        default: return 0.0;
      }
    }
    default: return 0.0;
  }
}


typedef struct {
  int kind; /* 0 none/idle, 1 task, 3 yield */
  const char *task_name;
  int ticks;
  double priority;
  const char *station;
  int stop_block;
} Candidate;

static void cand_reset(Candidate *c){
  memset(c,0,sizeof(*c));
  c->kind=0;
  c->priority=-1e9;
}

static void cand_consider_task(Candidate *best, const char *name, int ticks, double pr, const char *station){
  if(pr > best->priority){
    best->kind=1; best->priority=pr;
    best->task_name=name; best->ticks=ticks; best->station=station;
  }
}

static int exec_stmt_list_select(EvalCtx *ctx, Catalog *cat, const VecStmtPtr *list, double base_priority, Candidate *best){
  for(int i=0;i<list->n;i++){
    Stmt *s=list->v[i];
    switch(s->kind){
      case ST_LET: {
        double v=eval_expr(ctx,s->u.let_.value);
        ectx_set(ctx,s->u.let_.name,v);
        break;
      }
      case ST_SET: {
        if(strcmp(s->u.set_.lhs,"defaults.defense_posture")==0){
          Expr *rhs=s->u.set_.rhs;
          if(rhs->kind==EX_STRING){
            free(ctx->ch->defense_posture);
            ctx->ch->defense_posture=xstrdup(rhs->u.str);
          } else {
            double v=eval_expr(ctx,rhs);
            free(ctx->ch->defense_posture);
            ctx->ch->defense_posture=xstrdup(v>=0.5?"loud":"quiet");
          }
        }
        break;
      }
      case ST_TASK: {
        int ticks=1;
        double pr=base_priority;
        if(s->u.task.for_ticks) ticks=(int)(eval_expr(ctx,s->u.task.for_ticks)+0.5);
        TaskDef *td=cat_find_task(cat,s->u.task.task_name);
        const char *station = td?td->station:NULL;
        if(!s->u.task.for_ticks && td) ticks=td->time_ticks;
        if(s->u.task.priority) pr=eval_expr(ctx,s->u.task.priority);
        if(ticks<=0) ticks=1;
        cand_consider_task(best, s->u.task.task_name, ticks, pr, station);
        break;
      }
      case ST_IF: {
        double ok=eval_expr(ctx,s->u.if_.cond);
        if(truthy(ok)){
          if(exec_stmt_list_select(ctx,cat,&s->u.if_.then_stmts,base_priority,best)) return 1;
        } else {
          if(exec_stmt_list_select(ctx,cat,&s->u.if_.else_stmts,base_priority,best)) return 1;
        }
        break;
      }
      case ST_YIELD: {
        if(0 > best->priority){ best->kind=3; best->priority=0; }
        break;
      }
      case ST_STOP: {
        best->stop_block=1;
        return 1;
      }
      default: break;
    }
  }
  return 0;
}

static Candidate choose_action(Character *ch, World *w, Catalog *cat, int day, int tick, int breach_level, int ev_breach, int ev_overnight){
  EvalCtx ctx; memset(&ctx,0,sizeof(ctx));
  ctx.ch=ch; ctx.w=w; ctx.day=day; ctx.tick=tick;
  ctx.breach_level=breach_level;
  ctx.ev_breach=ev_breach; ctx.ev_overnight=ev_overnight;
  ectx_init(&ctx);

  Candidate best; cand_reset(&best);

  /* 1) on breach */
  if(ev_breach){
    for(int i=0;i<ch->on_events.n;i++){
      OnEventRule *r=&ch->on_events.v[i];
      if(strcmp(r->event_name,"breach")!=0) continue;
      if(r->when_cond){
        double ok=eval_expr(&ctx,r->when_cond);
        if(!truthy(ok)) continue;
      }
      Candidate tmp; cand_reset(&tmp);
      (void)exec_stmt_list_select(&ctx,cat,&r->stmts,r->priority,&tmp);
      if(tmp.kind==1) cand_consider_task(&best,tmp.task_name,tmp.ticks,tmp.priority,tmp.station);
    }
    if(best.kind==1){ ectx_clear(&ctx); return best; }
  }

  /* 2) thresholds */
  for(int i=0;i<ch->thresholds.n;i++){
    ThresholdRule *tr=&ch->thresholds.v[i];
    double ok=eval_expr(&ctx,tr->cond);
    if(!truthy(ok)) continue;
    VecStmtPtr one; VEC_INIT(one); VEC_PUSH(one,tr->action);
    Candidate tmp; cand_reset(&tmp);
    (void)exec_stmt_list_select(&ctx,cat,&one,0.0,&tmp);
    VEC_FREE(one);
    if(tmp.kind==1) cand_consider_task(&best,tmp.task_name,tmp.ticks,tmp.priority,tmp.station);
  }
  if(best.kind==1){ ectx_clear(&ctx); return best; }

  /* 3) plan blocks */
  for(int i=0;i<ch->blocks.n;i++){
    BlockRule *b=&ch->blocks.v[i];
    if(tick < b->start_tick || tick >= b->end_tick) continue;
    Candidate tmp; cand_reset(&tmp);
    (void)exec_stmt_list_select(&ctx,cat,&b->stmts,0.0,&tmp);
    if(tmp.kind==1) cand_consider_task(&best,tmp.task_name,tmp.ticks,tmp.priority,tmp.station);
    if(tmp.stop_block) break;
  }

  /* 4) rules */
  for(int i=0;i<ch->rules.n;i++){
    GenericRule *r=&ch->rules.v[i];
    Candidate tmp; cand_reset(&tmp);
    (void)exec_stmt_list_select(&ctx,cat,&r->stmts,r->priority,&tmp);
    if(tmp.kind==1) cand_consider_task(&best,tmp.task_name,tmp.ticks,tmp.priority,tmp.station);
  }

  if(best.kind==0){ best.kind=3; best.priority=0; }
  ectx_clear(&ctx);
  return best;
}


static int rand_percent(void){ return rand()%100; }

typedef struct { int breach_tick; int breach_level; } DayEvents;

static void plan_day_events(World *w, DayEvents *ev){
  ev->breach_tick=-1; ev->breach_level=0;
  if(rand_percent() < (int)(w->events.breach_chance+0.5)){
    int t = 6 + (rand()%16); /* 6..21 */
    ev->breach_tick=t;
    double s=w->shelter.signature, st=w->shelter.structure;
    int lvl=1;
    if(st<70 || s>15) lvl=2;
    if(st<55 || s>25) lvl=3;
    if((rand()%100)<25 && lvl<3) lvl++;
    ev->breach_level=lvl;
  }
}

static void clamp01_100(double *v){ if(*v<0) *v=0; if(*v>100) *v=100; }

static void tick_decay(Character *ch){
  ch->hunger -= 0.8;
  ch->hydration -= 1.0;
  ch->morale -= 0.1;
  clamp01_100(&ch->hunger); clamp01_100(&ch->hydration); clamp01_100(&ch->morale);
}

/*
  Fatigue model ("fatigue" == tiredness, 0..100):
  - increases while awake (idle or working)
  - decreases continuously while Resting/Sleeping

  This prevents the common lock-up where a character repeatedly selects
  Resting/Sleeping but never recovers enough to resume the plan.
*/
static void fatigue_tick(Character *ch){
  double df = 0.0;

  if(ch->rt_task){
    if(strcmp(ch->rt_task,"Sleeping")==0) df = -6.0;
    else if(strcmp(ch->rt_task,"Resting")==0) df = -3.0;
    else df = +1.0; /* any other task tires you */
  } else {
    df = +0.5; /* being awake but idle still costs something */
  }

  ch->fatigue += df;
  clamp01_100(&ch->fatigue);
}

static void apply_task_effects(World *w, Character *ch, const char *task){
  (void)w;
  /* fatigue is handled per-tick in fatigue_tick() */
  if(strcmp(task,"Sleeping")==0){ ch->morale += 2; }
  else if(strcmp(task,"Resting")==0){ ch->morale += 1; }
  else if(strcmp(task,"Eating")==0){ ch->hunger += 15; ch->hydration += 8; ch->morale += 1; }
  else if(strcmp(task,"Defensive shooting")==0){ ch->morale -= 1; }
  else if(strcmp(task,"Defensive combat")==0){ ch->injury += 2; }
  clamp01_100(&ch->morale); clamp01_100(&ch->injury); clamp01_100(&ch->hunger); clamp01_100(&ch->hydration);
}

static void print_status(Character *ch){
  printf("    %s stats: hunger=%.0f hyd=%.0f fatigue=%.0f morale=%.0f injury=%.0f illness=%.0f posture=%s\n",
    ch->name, ch->hunger, ch->hydration, ch->fatigue, ch->morale, ch->injury, ch->illness, ch->defense_posture);
}

void run_sim(World *w, Catalog *cat, Character *A, Character *B, int days){
  for(int day=0; day<days; day++){
    DayEvents ev; plan_day_events(w,&ev);

    printf("\n=== DAY %d === shelter(structure=%.0f temp=%.1f power=%.0f sig=%.0f water_safe=%.0f) breach_chance=%.0f%%\n",
      day, w->shelter.structure, w->shelter.temp_c, w->shelter.power, w->shelter.signature, w->shelter.water_safe, w->events.breach_chance);

    for(int tick=0; tick<DAY_TICKS; tick++){
      int ev_breach=(ev.breach_tick==tick);
      int breach_level=ev_breach?ev.breach_level:0;
      int ev_overnight=(tick==DAY_TICKS-1);

      printf("\n  [day %d tick %02d] ", day, tick);
      if(ev_breach) printf("EVENT: BREACH level=%d! ", breach_level);
      if(ev_overnight) printf("EVENT: overnight_threat_check ");
      printf("\n");

      tick_decay(A); tick_decay(B);
      fatigue_tick(A); fatigue_tick(B);

      /* progress ongoing tasks */
      if(A->rt_remaining>0){
        A->rt_remaining--;
        if(A->rt_remaining==0 && A->rt_task){
          printf("    %s completed: %s\n", A->name, A->rt_task);
          apply_task_effects(w,A,A->rt_task);
          A->rt_task=NULL; A->rt_station=NULL; A->rt_priority=0;
        }
      }
      if(B->rt_remaining>0){
        B->rt_remaining--;
        if(B->rt_remaining==0 && B->rt_task){
          printf("    %s completed: %s\n", B->name, B->rt_task);
          apply_task_effects(w,B,B->rt_task);
          B->rt_task=NULL; B->rt_station=NULL; B->rt_priority=0;
        }
      }

      Candidate ca, cb; cand_reset(&ca); cand_reset(&cb);

      if(A->rt_remaining==0) ca=choose_action(A,w,cat,day,tick,breach_level,ev_breach,ev_overnight);
      if(B->rt_remaining==0) cb=choose_action(B,w,cat,day,tick,breach_level,ev_breach,ev_overnight);

      /* station conflict */
      if(A->rt_remaining==0 && B->rt_remaining==0 && ca.kind==1 && cb.kind==1){
        if(ca.station && cb.station && strcmp(ca.station,cb.station)==0){
          int a_wins = (ca.priority > cb.priority) || (ca.priority==cb.priority && strcmp(A->name,B->name)<=0);
          if(a_wins){
            printf("    CONFLICT: station '%s' claimed by %s (priority %.1f); %s yields\n", ca.station, A->name, ca.priority, B->name);
            cb.kind=3;
          } else {
            printf("    CONFLICT: station '%s' claimed by %s (priority %.1f); %s yields\n", cb.station, B->name, cb.priority, A->name);
            ca.kind=3;
          }
        }
      }

      /* start/continue */
      if(A->rt_remaining==0){
        if(ca.kind==1){
          A->rt_task=ca.task_name; A->rt_station=ca.station; A->rt_remaining=ca.ticks; A->rt_priority=ca.priority;
          printf("    %s starts: %s (%dt) station=%s priority=%.1f\n", A->name, ca.task_name, ca.ticks, ca.station?ca.station:"-", ca.priority);
        } else {
          printf("    %s idle\n", A->name);
        }
      } else {
        printf("    %s continues: %s (remaining %dt)\n", A->name, A->rt_task?A->rt_task:"(none)", A->rt_remaining);
      }

      if(B->rt_remaining==0){
        if(cb.kind==1){
          B->rt_task=cb.task_name; B->rt_station=cb.station; B->rt_remaining=cb.ticks; B->rt_priority=cb.priority;
          printf("    %s starts: %s (%dt) station=%s priority=%.1f\n", B->name, cb.task_name, cb.ticks, cb.station?cb.station:"-", cb.priority);
        } else {
          printf("    %s idle\n", B->name);
        }
      } else {
        printf("    %s continues: %s (remaining %dt)\n", B->name, B->rt_task?B->rt_task:"(none)", B->rt_remaining);
      }

      /* breach consequence */
      if(ev_breach){
        int defended=0;
        if(A->rt_task && strstr(A->rt_task,"Defensive")!=NULL) defended=1;
        if(B->rt_task && strstr(B->rt_task,"Defensive")!=NULL) defended=1;
        if(!defended){
          double dmg=4.0*breach_level;
          w->shelter.structure -= dmg;
          if(w->shelter.structure<0) w->shelter.structure=0;
          printf("    BREACH impact: structure -%.0f (now %.0f)\n", dmg, w->shelter.structure);
        } else {
          printf("    BREACH defended: minimal structure loss\n");
          w->shelter.structure -= (breach_level==3?1.0:0.5);
          if(w->shelter.structure<0) w->shelter.structure=0;
        }
      }

      print_status(A);
      print_status(B);

      if(ev_overnight){
        int roll=rand_percent();
        if(roll < (int)(w->events.overnight_chance+0.5)){
          printf("    overnight_threat_check: contact outside (roll=%d < %.0f%%)\n", roll, w->events.overnight_chance);
          w->shelter.signature += 1.0;
        } else {
          printf("    overnight_threat_check: quiet night (roll=%d)\n", roll);
          if(w->shelter.signature>0) w->shelter.signature -= 0.5;
          if(w->shelter.signature<0) w->shelter.signature=0;
        }
      }
    }
  }
}


