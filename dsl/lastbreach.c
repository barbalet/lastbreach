/* lastbreach.c - Standalone DSL runner (C99, no third-party libs)
   Parses:
     - 2x .lbp character plans (required)
     - .lbw world file (optional)
     - .lbc catalog file (optional)

   Builds a single executable "lastbreach" (see Makefile).

   This is a pragmatic runner: it supports the core constructs used by the example files:
     - character "Name" { skills{...} traits:[...] defaults{...} thresholds{...} plan{...} on "event"... }
     - statements: let, if/else, task "...", set defaults.defense_posture = "...", yield_tick, stop_block
     - expressions: numbers, strings (as call args), variables (tick/day/char.* and shelter.*), comparisons, and/or/not
     - calls: stock("Item"), has("Item"), cond("Item"), event("breach")

   Simulation:
     - 24 ticks per day (configurable in code)
     - basic stat drift and simple task completion effects
     - daily breach chance from world.lbw; breach at random tick with severity 1..3
     - station conflicts: tasks can have a station (from catalog taskdef.station); if both start same station, higher priority wins
*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>

#define DAY_TICKS 24

static void dief(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}
static void *xmalloc(size_t n){ void *p=malloc(n); if(!p) dief("out of memory"); return p; }
static void *xrealloc(void *p,size_t n){ void*q=realloc(p,n); if(!q) dief("out of memory"); return q; }
static char *xstrdup(const char *s){ if(!s) return NULL; size_t n=strlen(s); char *p=(char*)xmalloc(n+1); memcpy(p,s,n+1); return p; }

/* ------------------------------ Typed vectors ------------------------------ */
#define VEC_DECL(name, T) typedef struct { T *v; int n, cap; } name
#define VEC_INIT(a) do{ (a).v=NULL; (a).n=0; (a).cap=0; }while(0)
#define VEC_FREE(a) do{ free((a).v); (a).v=NULL; (a).n=(a).cap=0; }while(0)
#define VEC_PUSH(a, x) do{ \
  if ((a).n == (a).cap) { \
    (a).cap = (a).cap ? (a).cap*2 : 8; \
    (a).v = xrealloc((a).v, (size_t)(a).cap * sizeof(*(a).v)); \
  } \
  (a).v[(a).n++] = (x); \
}while(0)

/* Forward types needed for vector decls */
typedef struct Expr Expr;
typedef struct Stmt Stmt;

VEC_DECL(VecStr, char*);
VEC_DECL(VecDbl, double);
VEC_DECL(VecExprPtr, Expr*);
VEC_DECL(VecStmtPtr, Stmt*);

/* ------------------------------ Inventory / Catalog / World ------------------------------ */

typedef struct { char *key; double qty; double best_cond; } ItemEntry;
VEC_DECL(VecItemEntry, ItemEntry);

typedef struct { VecItemEntry items; } Inventory;

static void inv_init(Inventory *inv){ VEC_INIT(inv->items); }

static ItemEntry *inv_find(Inventory *inv, const char *key){
  for(int i=0;i<inv->items.n;i++) if(strcmp(inv->items.v[i].key,key)==0) return &inv->items.v[i];
  return NULL;
}
static void inv_add(Inventory *inv, const char *key, double qty, double cond){
  ItemEntry *e = inv_find(inv,key);
  if(!e){
    ItemEntry ne; ne.key=xstrdup(key); ne.qty=qty; ne.best_cond=cond;
    VEC_PUSH(inv->items, ne);
  } else {
    e->qty += qty;
    if (cond > e->best_cond) e->best_cond = cond;
  }
}
static double inv_stock(Inventory *inv, const char *key){ ItemEntry *e=inv_find(inv,key); return e?e->qty:0.0; }
static int inv_has(Inventory *inv, const char *key){ return inv_stock(inv,key) > 0.0; }
static double inv_cond(Inventory *inv, const char *key){ ItemEntry *e=inv_find(inv,key); return e?e->best_cond:0.0; }

typedef struct { char *name; int time_ticks; char *station; } TaskDef;
VEC_DECL(VecTaskDef, TaskDef);

typedef struct { VecTaskDef tasks; } Catalog;

static void cat_init(Catalog *c){ VEC_INIT(c->tasks); }

static TaskDef *cat_find_task(Catalog *c, const char *name){
  for(int i=0;i<c->tasks.n;i++) if(strcmp(c->tasks.v[i].name,name)==0) return &c->tasks.v[i];
  return NULL;
}
static TaskDef *cat_get_or_add_task(Catalog *c, const char *name){
  TaskDef *t=cat_find_task(c,name);
  if(t) return t;
  TaskDef nt; nt.name=xstrdup(name); nt.time_ticks=1; nt.station=NULL;
  VEC_PUSH(c->tasks, nt);
  return &c->tasks.v[c->tasks.n-1];
}

typedef struct {
  double temp_c, signature, power, water_safe, water_raw, structure, contamination;
} Shelter;

typedef struct { double breach_chance, overnight_chance; } WorldEvents;

typedef struct { Shelter shelter; Inventory inv; WorldEvents events; } World;

static void world_init(World *w){
  memset(w,0,sizeof(*w));
  w->shelter.temp_c=5.0;
  w->shelter.signature=10.0;
  w->shelter.power=25.0;
  w->shelter.water_safe=5.0;
  w->shelter.water_raw=10.0;
  w->shelter.structure=75.0;
  w->shelter.contamination=10.0;
  inv_init(&w->inv);
  w->events.breach_chance=15.0;
  w->events.overnight_chance=25.0;
}

/* ------------------------------ Lexer ------------------------------ */

typedef enum {
  TK_EOF=0, TK_IDENT, TK_STRING, TK_NUMBER, TK_PERCENT, TK_DURATION,
  TK_LBRACE, TK_RBRACE, TK_LPAREN, TK_RPAREN, TK_LBRACK, TK_RBRACK,
  TK_COLON, TK_SEMI, TK_COMMA, TK_DOT, TK_DOTDOT, TK_ASSIGN,
  TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_EQ, TK_NEQ, TK_LT, TK_LTE, TK_GT, TK_GTE
} TokenKind;

typedef struct {
  TokenKind kind;
  int line;
  const char *start;
  int len;
  double num;
  int iticks;
} Token;

typedef struct { char *src; size_t len,pos; int line; Token cur; } Lexer;

static int lx_peek(Lexer *lx){ return (lx->pos>=lx->len)?0:(unsigned char)lx->src[lx->pos]; }
static int lx_next(Lexer *lx){ if(lx->pos>=lx->len) return 0; int c=(unsigned char)lx->src[lx->pos++]; if(c=='\n') lx->line++; return c; }
static int lx_match(Lexer *lx,int c){ if(lx_peek(lx)==c){ lx_next(lx); return 1; } return 0; }

static void lx_skip(Lexer *lx){
  for(;;){
    int c=lx_peek(lx);
    if(c==0) return;
    if(isspace(c)){ lx_next(lx); continue; }
    if(c=='#'){ while((c=lx_next(lx))!=0 && c!='\n'){} continue; }
    if(c=='/' && lx->pos+1<lx->len && lx->src[lx->pos+1]=='/'){
      lx_next(lx); lx_next(lx); while((c=lx_next(lx))!=0 && c!='\n'){} continue;
    }
    if(c=='/' && lx->pos+1<lx->len && lx->src[lx->pos+1]=='*'){
      lx_next(lx); lx_next(lx);
      for(;;){
        c=lx_next(lx);
        if(c==0) dief("unterminated block comment");
        if(c=='*' && lx_peek(lx)=='/'){ lx_next(lx); break; }
      }
      continue;
    }
    return;
  }
}

static Token tk_make(Lexer *lx, TokenKind k, const char *s, int n){
  Token t; memset(&t,0,sizeof(t));
  t.kind=k; t.start=s; t.len=n; t.line=lx->line;
  return t;
}
static char *tk_cstr(const Token *t){
  char *s=(char*)xmalloc((size_t)t->len+1);
  memcpy(s,t->start,(size_t)t->len);
  s[t->len]=0;
  return s;
}

static void lx_read_string(Lexer *lx){
  const char *start=&lx->src[lx->pos];
  int line0=lx->line;
  size_t p0=lx->pos;
  for(;;){
    int c=lx_next(lx);
    if(c==0) dief("unterminated string at line %d", line0);
    if(c=='"') break;
    if(c=='\\'){ int d=lx_next(lx); if(d==0) dief("unterminated escape at line %d", line0); }
  }
  size_t p1=lx->pos-1;
  lx->cur=tk_make(lx,TK_STRING,start,(int)(p1-p0));
}

static void lx_read_number(Lexer *lx, int first){
  char buf[128]; int bi=0;
  buf[bi++]=(char)first;
  int c; int seen_dot=(first=='.');
  while((c=lx_peek(lx))!=0){
    if(isdigit(c)){ if(bi<120) buf[bi++]=(char)lx_next(lx); else lx_next(lx); continue; }
    if(c=='.' && !seen_dot){
      /* only treat as decimal point if followed by a digit, otherwise it may be a range operator like .. */
      int nextc = 0;
      if(lx->pos+1 < lx->len) nextc = (unsigned char)lx->src[lx->pos+1];
      if(!isdigit(nextc)) break;
      seen_dot=1;
      if(bi<120) buf[bi++]=(char)lx_next(lx); else lx_next(lx);
      continue;
    }
    break;
  }
  buf[bi]=0;
  if(lx_peek(lx)=='%'){ lx_next(lx); lx->cur=tk_make(lx,TK_PERCENT,NULL,0); lx->cur.num=atof(buf); return; }
  if(lx_peek(lx)=='t'){ lx_next(lx); lx->cur=tk_make(lx,TK_DURATION,NULL,0); lx->cur.iticks=(int)(atof(buf)+0.5); return; }
  lx->cur=tk_make(lx,TK_NUMBER,NULL,0); lx->cur.num=atof(buf);
}

static int is_ident_start(int c){ return isalpha(c)||c=='_'; }
static int is_ident_part(int c){ return isalnum(c)||c=='_'; }

static void lx_next_token(Lexer *lx){
  lx_skip(lx);
  int c=lx_next(lx);
  if(c==0){ lx->cur=tk_make(lx,TK_EOF,NULL,0); return; }
  const char *s=&lx->src[lx->pos-1];
  switch(c){
    case '{': lx->cur=tk_make(lx,TK_LBRACE,s,1); return;
    case '}': lx->cur=tk_make(lx,TK_RBRACE,s,1); return;
    case '(': lx->cur=tk_make(lx,TK_LPAREN,s,1); return;
    case ')': lx->cur=tk_make(lx,TK_RPAREN,s,1); return;
    case '[': lx->cur=tk_make(lx,TK_LBRACK,s,1); return;
    case ']': lx->cur=tk_make(lx,TK_RBRACK,s,1); return;
    case ':': lx->cur=tk_make(lx,TK_COLON,s,1); return;
    case ';': lx->cur=tk_make(lx,TK_SEMI,s,1); return;
    case ',': lx->cur=tk_make(lx,TK_COMMA,s,1); return;
    case '.': if(lx_match(lx,'.')){ lx->cur=tk_make(lx,TK_DOTDOT,s,2); return; } lx->cur=tk_make(lx,TK_DOT,s,1); return;
    case '+': lx->cur=tk_make(lx,TK_PLUS,s,1); return;
    case '-': lx->cur=tk_make(lx,TK_MINUS,s,1); return;
    case '*': lx->cur=tk_make(lx,TK_STAR,s,1); return;
    case '/': lx->cur=tk_make(lx,TK_SLASH,s,1); return;
    case '=': if(lx_match(lx,'=')){ lx->cur=tk_make(lx,TK_EQ,s,2); return; } lx->cur=tk_make(lx,TK_ASSIGN,s,1); return;
    case '!': if(lx_match(lx,'=')){ lx->cur=tk_make(lx,TK_NEQ,s,2); return; } dief("unexpected '!' at line %d", lx->line);
    case '<': if(lx_match(lx,'=')){ lx->cur=tk_make(lx,TK_LTE,s,2); return; } lx->cur=tk_make(lx,TK_LT,s,1); return;
    case '>': if(lx_match(lx,'=')){ lx->cur=tk_make(lx,TK_GTE,s,2); return; } lx->cur=tk_make(lx,TK_GT,s,1); return;
    case '"': lx_read_string(lx); return;
    default: break;
  }
  if(isdigit(c) || (c=='.' && isdigit(lx_peek(lx)))){ lx_read_number(lx,c); return; }
  if(is_ident_start(c)){
    size_t p0=lx->pos-1;
    while(is_ident_part(lx_peek(lx))) lx_next(lx);
    size_t p1=lx->pos;
    lx->cur=tk_make(lx,TK_IDENT,&lx->src[p0],(int)(p1-p0));
    return;
  }
  dief("unexpected character '%c' at line %d", c, lx->line);
}

/* ------------------------------ AST ------------------------------ */

typedef enum { EX_NUM, EX_BOOL, EX_STRING, EX_VAR, EX_CALL, EX_UNARY, EX_BINARY } ExprKind;
typedef enum {
  OP_ADD, OP_SUB, OP_MUL, OP_DIV,
  OP_EQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE,
  OP_AND, OP_OR, OP_NEG, OP_NOT
} OpKind;

typedef struct { char *name; VecExprPtr args; } CallExpr;

struct Expr {
  ExprKind kind;
  int line;
  union {
    double num;
    int boolean;
    char *str;
    char *var;
    CallExpr call;
    struct { OpKind op; Expr *a; } un;
    struct { OpKind op; Expr *a; Expr *b; } bin;
  } u;
};

typedef enum { ST_LET, ST_IF, ST_TASK, ST_SET, ST_YIELD, ST_STOP } StmtKind;

typedef struct { char *name; Expr *value; } LetStmt;
typedef struct { Expr *cond; VecStmtPtr then_stmts; VecStmtPtr else_stmts; } IfStmt;
typedef struct { char *task_name; Expr *for_ticks; Expr *priority; } TaskStmt;
typedef struct { char *lhs; Expr *rhs; } SetStmt;

struct Stmt {
  StmtKind kind;
  int line;
  union { LetStmt let_; IfStmt if_; TaskStmt task; SetStmt set_; } u;
};

typedef struct { Expr *cond; Stmt *action; } ThresholdRule;
VEC_DECL(VecThreshold, ThresholdRule);

typedef struct { char *name; int start_tick,end_tick; VecStmtPtr stmts; } BlockRule;
VEC_DECL(VecBlockRule, BlockRule);

typedef struct { char *label; double priority; VecStmtPtr stmts; } GenericRule;
VEC_DECL(VecGenericRule, GenericRule);

typedef struct { char *event_name; double priority; Expr *when_cond; VecStmtPtr stmts; } OnEventRule;
VEC_DECL(VecOnEventRule, OnEventRule);

typedef struct {
  char *name;
  double hunger, hydration, fatigue, morale, injury, illness;
  char *defense_posture;
  VecStr skill_keys;
  VecDbl skill_vals;
  VecStr traits;
  VecThreshold thresholds;
  VecBlockRule blocks;
  VecGenericRule rules;
  VecOnEventRule on_events;

  /* runtime */
  const char *rt_task;
  const char *rt_station;
  int rt_remaining;
  double rt_priority;
} Character;

static void character_init(Character *c){
  memset(c,0,sizeof(*c));
  c->hunger=75; c->hydration=75; c->fatigue=20; c->morale=55; c->injury=0; c->illness=0;
  c->defense_posture=xstrdup("quiet");
  VEC_INIT(c->skill_keys); VEC_INIT(c->skill_vals);
  VEC_INIT(c->traits);
  VEC_INIT(c->thresholds);
  VEC_INIT(c->blocks);
  VEC_INIT(c->rules);
  VEC_INIT(c->on_events);
  c->rt_task=NULL; c->rt_station=NULL; c->rt_remaining=0; c->rt_priority=0;
}

/* ------------------------------ Parser ------------------------------ */

typedef struct { const char *filename; Lexer lx; } Parser;

static void ps_init(Parser *ps, const char *filename, char *src){
  ps->filename=filename;
  ps->lx.src=src; ps->lx.len=strlen(src); ps->lx.pos=0; ps->lx.line=1;
  lx_next_token(&ps->lx);
}
static int ps_is(Parser *ps, TokenKind k){ return ps->lx.cur.kind==k; }
static int ps_is_ident(Parser *ps, const char *s){
  if(!ps_is(ps,TK_IDENT)) return 0;
  Token *t=&ps->lx.cur;
  int n=(int)strlen(s);
  return (t->len==n && strncmp(t->start,s,(size_t)n)==0);
}
static void ps_expect(Parser *ps, TokenKind k, const char *what){
  if(!ps_is(ps,k)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
  lx_next_token(&ps->lx);
}
static char *ps_expect_ident(Parser *ps, const char *what){
  if(!ps_is(ps,TK_IDENT)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
  char *s=tk_cstr(&ps->lx.cur);
  lx_next_token(&ps->lx);
  return s;
}
static char *ps_expect_string(Parser *ps, const char *what){
  if(!ps_is(ps,TK_STRING)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
  char *s=tk_cstr(&ps->lx.cur);
  lx_next_token(&ps->lx);
  return s;
}
static double ps_expect_number(Parser *ps, const char *what){
  Token *t=&ps->lx.cur;
  if(ps_is(ps,TK_NUMBER)){ double v=t->num; lx_next_token(&ps->lx); return v; }
  if(ps_is(ps,TK_DURATION)){ double v=(double)t->iticks; lx_next_token(&ps->lx); return v; }
  dief("%s:%d: expected %s", ps->filename, t->line, what);
  return 0;
}
static double ps_expect_percent(Parser *ps, const char *what){
  if(!ps_is(ps,TK_PERCENT)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
  double v=ps->lx.cur.num;
  lx_next_token(&ps->lx);
  return v;
}

/* Expr constructors */
static Expr *ex_new(ExprKind k, int line){ Expr *e=(Expr*)xmalloc(sizeof(Expr)); memset(e,0,sizeof(*e)); e->kind=k; e->line=line; return e; }
static Expr *ex_num(double v,int line){ Expr*e=ex_new(EX_NUM,line); e->u.num=v; return e; }
static Expr *ex_bool(int b,int line){ Expr*e=ex_new(EX_BOOL,line); e->u.boolean=b; return e; }
static Expr *ex_string(char*s,int line){ Expr*e=ex_new(EX_STRING,line); e->u.str=s; return e; }
static Expr *ex_var(char*v,int line){ Expr*e=ex_new(EX_VAR,line); e->u.var=v; return e; }
static Expr *ex_un(OpKind op, Expr*a,int line){ Expr*e=ex_new(EX_UNARY,line); e->u.un.op=op; e->u.un.a=a; return e; }
static Expr *ex_bin(OpKind op, Expr*a,Expr*b,int line){ Expr*e=ex_new(EX_BINARY,line); e->u.bin.op=op; e->u.bin.a=a; e->u.bin.b=b; return e; }
static Expr *ex_call(char *name, VecExprPtr args,int line){ Expr*e=ex_new(EX_CALL,line); e->u.call.name=name; e->u.call.args=args; return e; }

/* Forward expr parse */
static Expr *parse_expr(Parser *ps);

static Expr *parse_primary(Parser *ps){
  Token *t=&ps->lx.cur;
  if(ps_is(ps,TK_NUMBER)){ double v=t->num; int line=t->line; lx_next_token(&ps->lx); return ex_num(v,line); }
  if(ps_is(ps,TK_DURATION)){ double v=(double)t->iticks; int line=t->line; lx_next_token(&ps->lx); return ex_num(v,line); }
  if(ps_is(ps,TK_PERCENT)){ double v=t->num; int line=t->line; lx_next_token(&ps->lx); return ex_num(v,line); }
  if(ps_is(ps,TK_STRING)){ char *s=tk_cstr(t); int line=t->line; lx_next_token(&ps->lx); return ex_string(s,line); }
  if(ps_is(ps,TK_IDENT)){
    char *base=tk_cstr(t); int line=t->line; lx_next_token(&ps->lx);

    if(ps_is(ps,TK_LPAREN)){
      ps_expect(ps,TK_LPAREN,"(");
      VecExprPtr args; VEC_INIT(args);
      if(!ps_is(ps,TK_RPAREN)){
        for(;;){
          Expr *a=parse_expr(ps);
          VEC_PUSH(args,a);
          if(ps_is(ps,TK_COMMA)){ ps_expect(ps,TK_COMMA,","); continue; }
          break;
        }
      }
      ps_expect(ps,TK_RPAREN,")");
      return ex_call(base,args,line);
    }

    if(ps_is(ps,TK_DOT)){
      size_t cap=strlen(base)+32;
      char *buf=(char*)xmalloc(cap);
      strcpy(buf,base); free(base);
      while(ps_is(ps,TK_DOT)){
        ps_expect(ps,TK_DOT,".");
        char *part=ps_expect_ident(ps,"identifier");
        size_t need=strlen(buf)+1+strlen(part)+1;
        if(need>cap){ cap=need*2; buf=(char*)xrealloc(buf,cap); }
        strcat(buf,"."); strcat(buf,part);
        free(part);
      }
      return ex_var(buf,line);
    }

    return ex_var(base,line);
  }
  if(ps_is(ps,TK_LPAREN)){
    ps_expect(ps,TK_LPAREN,"(");
    Expr *e=parse_expr(ps);
    ps_expect(ps,TK_RPAREN,")");
    return e;
  }
  dief("%s:%d: expected expression", ps->filename, t->line);
  return NULL;
}

static Expr *parse_unary(Parser *ps){
  if(ps_is_ident(ps,"not")){ int line=ps->lx.cur.line; lx_next_token(&ps->lx); return ex_un(OP_NOT, parse_unary(ps), line); }
  if(ps_is(ps,TK_MINUS)){ int line=ps->lx.cur.line; ps_expect(ps,TK_MINUS,"-"); return ex_un(OP_NEG, parse_unary(ps), line); }
  if(ps_is_ident(ps,"true")){ int line=ps->lx.cur.line; lx_next_token(&ps->lx); return ex_bool(1,line); }
  if(ps_is_ident(ps,"false")){ int line=ps->lx.cur.line; lx_next_token(&ps->lx); return ex_bool(0,line); }
  return parse_primary(ps);
}
static Expr *parse_mul(Parser *ps){
  Expr *e=parse_unary(ps);
  for(;;){
    if(ps_is(ps,TK_STAR)){ int line=ps->lx.cur.line; ps_expect(ps,TK_STAR,"*"); e=ex_bin(OP_MUL,e,parse_unary(ps),line); }
    else if(ps_is(ps,TK_SLASH)){ int line=ps->lx.cur.line; ps_expect(ps,TK_SLASH,"/"); e=ex_bin(OP_DIV,e,parse_unary(ps),line); }
    else break;
  }
  return e;
}
static Expr *parse_add(Parser *ps){
  Expr *e=parse_mul(ps);
  for(;;){
    if(ps_is(ps,TK_PLUS)){ int line=ps->lx.cur.line; ps_expect(ps,TK_PLUS,"+"); e=ex_bin(OP_ADD,e,parse_mul(ps),line); }
    else if(ps_is(ps,TK_MINUS)){ int line=ps->lx.cur.line; ps_expect(ps,TK_MINUS,"-"); e=ex_bin(OP_SUB,e,parse_mul(ps),line); }
    else break;
  }
  return e;
}
static Expr *parse_cmp(Parser *ps){
  Expr *e=parse_add(ps);
  for(;;){
    TokenKind k=ps->lx.cur.kind; int line=ps->lx.cur.line;
    if(k==TK_EQ){ ps_expect(ps,TK_EQ,"=="); e=ex_bin(OP_EQ,e,parse_add(ps),line); }
    else if(k==TK_NEQ){ ps_expect(ps,TK_NEQ,"!="); e=ex_bin(OP_NEQ,e,parse_add(ps),line); }
    else if(k==TK_LT){ ps_expect(ps,TK_LT,"<"); e=ex_bin(OP_LT,e,parse_add(ps),line); }
    else if(k==TK_LTE){ ps_expect(ps,TK_LTE,"<="); e=ex_bin(OP_LTE,e,parse_add(ps),line); }
    else if(k==TK_GT){ ps_expect(ps,TK_GT,">"); e=ex_bin(OP_GT,e,parse_add(ps),line); }
    else if(k==TK_GTE){ ps_expect(ps,TK_GTE,">="); e=ex_bin(OP_GTE,e,parse_add(ps),line); }
    else break;
  }
  return e;
}
static Expr *parse_and(Parser *ps){
  Expr *e=parse_cmp(ps);
  while(ps_is_ident(ps,"and")){ int line=ps->lx.cur.line; lx_next_token(&ps->lx); e=ex_bin(OP_AND,e,parse_cmp(ps),line); }
  return e;
}
static Expr *parse_or(Parser *ps){
  Expr *e=parse_and(ps);
  while(ps_is_ident(ps,"or")){ int line=ps->lx.cur.line; lx_next_token(&ps->lx); e=ex_bin(OP_OR,e,parse_and(ps),line); }
  return e;
}
static Expr *parse_expr(Parser *ps){ return parse_or(ps); }

/* ---- Stmts ---- */
static Stmt *st_new(StmtKind k, int line){ Stmt *s=(Stmt*)xmalloc(sizeof(Stmt)); memset(s,0,sizeof(*s)); s->kind=k; s->line=line; return s; }

static void parse_stmt_list(Parser *ps, VecStmtPtr *out);

static void skip_block(Parser *ps);

static Stmt *parse_action_stmt(Parser *ps){
  Token *t=&ps->lx.cur;

  if(ps_is_ident(ps,"task")){
    int line=t->line; lx_next_token(&ps->lx);
    char *tn=ps_expect_string(ps,"task name");
    Stmt *s=st_new(ST_TASK,line);
    s->u.task.task_name=tn;
    s->u.task.for_ticks=NULL;
    s->u.task.priority=NULL;
    for(;;){
      if(ps_is_ident(ps,"for")){ lx_next_token(&ps->lx); s->u.task.for_ticks=parse_expr(ps); continue; }
      if(ps_is_ident(ps,"priority")){ lx_next_token(&ps->lx); s->u.task.priority=parse_expr(ps); continue; }

      /* tolerate optional DSL clauses we don't simulate in detail (using/requires/consumes/produces/when/etc.) */
      if(ps_is(ps,TK_IDENT)){
        if(ps_is_ident(ps,"using") || ps_is_ident(ps,"requires") || ps_is_ident(ps,"consumes") || ps_is_ident(ps,"produces")){
          lx_next_token(&ps->lx);
          if(ps_is(ps,TK_LBRACE)){ skip_block(ps); continue; }
          /* sometimes a list follows */
          if(ps_is(ps,TK_LBRACK)){
            /* skip bracket list */
            int depth=0; ps_expect(ps,TK_LBRACK,"["); depth=1;
            while(depth>0 && !ps_is(ps,TK_EOF)){
              if(ps_is(ps,TK_LBRACK)){ ps_expect(ps,TK_LBRACK,"["); depth++; continue; }
              if(ps_is(ps,TK_RBRACK)){ ps_expect(ps,TK_RBRACK,"]"); depth--; continue; }
              lx_next_token(&ps->lx);
            }
            continue;
          }
          /* fallthrough: consume one expression if present */
          if(!ps_is(ps,TK_SEMI)) (void)parse_expr(ps);
          continue;
        }
        if(ps_is_ident(ps,"when")){
          lx_next_token(&ps->lx);
          (void)parse_expr(ps);
          continue;
        }
      }
      break;
    }
    return s;
  }

  if(ps_is_ident(ps,"set")){
    int line=t->line; lx_next_token(&ps->lx);
    char *lhs=ps_expect_ident(ps,"lvalue");
    size_t cap=strlen(lhs)+32; char *buf=(char*)xmalloc(cap); strcpy(buf,lhs); free(lhs);
    while(ps_is(ps,TK_DOT)){
      ps_expect(ps,TK_DOT,".");
      char *part=ps_expect_ident(ps,"identifier");
      size_t need=strlen(buf)+1+strlen(part)+1;
      if(need>cap){ cap=need*2; buf=(char*)xrealloc(buf,cap); }
      strcat(buf,"."); strcat(buf,part);
      free(part);
    }
    ps_expect(ps,TK_ASSIGN,"=");
    Expr *rhs=parse_expr(ps);
    Stmt *s=st_new(ST_SET,line);
    s->u.set_.lhs=buf;
    s->u.set_.rhs=rhs;
    return s;
  }

  if(ps_is_ident(ps,"yield_tick")){ int line=t->line; lx_next_token(&ps->lx); return st_new(ST_YIELD,line); }
  if(ps_is_ident(ps,"stop_block")){ int line=t->line; lx_next_token(&ps->lx); return st_new(ST_STOP,line); }

  dief("%s:%d: expected action stmt", ps->filename, t->line);
  return NULL;
}

static Stmt *parse_stmt(Parser *ps){
  Token *t=&ps->lx.cur;

  if(ps_is_ident(ps,"let")){
    int line=t->line; lx_next_token(&ps->lx);
    char *name=ps_expect_ident(ps,"let name");
    ps_expect(ps,TK_ASSIGN,"=");
    Expr *val=parse_expr(ps);
    ps_expect(ps,TK_SEMI,";");
    Stmt *s=st_new(ST_LET,line);
    s->u.let_.name=name; s->u.let_.value=val;
    return s;
  }

  if(ps_is_ident(ps,"if")){
    int line=t->line; lx_next_token(&ps->lx);
    Expr *cond=parse_expr(ps);
    ps_expect(ps,TK_LBRACE,"{");
    VecStmtPtr then_stmts; VEC_INIT(then_stmts);
    parse_stmt_list(ps,&then_stmts);
    ps_expect(ps,TK_RBRACE,"}");

    VecStmtPtr else_stmts; VEC_INIT(else_stmts);
    if(ps_is_ident(ps,"else")){
      lx_next_token(&ps->lx);
      if(ps_is_ident(ps,"if")){
        /* else-if: parse nested if as a single statement in else block */
        Stmt *nested = parse_stmt(ps);
        VEC_PUSH(else_stmts, nested);
      } else {
        ps_expect(ps,TK_LBRACE,"{");
        parse_stmt_list(ps,&else_stmts);
        ps_expect(ps,TK_RBRACE,"}");
      }
    }

    Stmt *s=st_new(ST_IF,line);
    s->u.if_.cond=cond;
    s->u.if_.then_stmts=then_stmts;
    s->u.if_.else_stmts=else_stmts;
    return s;
  }

  Stmt *a=parse_action_stmt(ps);
  ps_expect(ps,TK_SEMI,";");
  return a;
}

static void parse_stmt_list(Parser *ps, VecStmtPtr *out){
  while(!ps_is(ps,TK_RBRACE) && !ps_is(ps,TK_EOF)){
    Stmt *s=parse_stmt(ps);
    VEC_PUSH(*out,s);
  }
}

/* ---- Character sections ---- */

static void parse_skills(Parser *ps, Character *ch){
  ps_expect(ps,TK_LBRACE,"{");
  while(!ps_is(ps,TK_RBRACE)){
    char *k=ps_expect_ident(ps,"skill name");
    ps_expect(ps,TK_COLON,":");
    double v=ps_expect_number(ps,"number");
    ps_expect(ps,TK_SEMI,";");
    VEC_PUSH(ch->skill_keys,k);
    VEC_PUSH(ch->skill_vals,v);
  }
  ps_expect(ps,TK_RBRACE,"}");
}
static void parse_traits(Parser *ps, Character *ch){
  ps_expect(ps,TK_COLON,":");
  ps_expect(ps,TK_LBRACK,"[");
  if(!ps_is(ps,TK_RBRACK)){
    for(;;){
      char *s=ps_expect_string(ps,"trait");
      VEC_PUSH(ch->traits,s);
      if(ps_is(ps,TK_COMMA)){ ps_expect(ps,TK_COMMA,","); continue; }
      break;
    }
  }
  ps_expect(ps,TK_RBRACK,"]");
  ps_expect(ps,TK_SEMI,";");
}
static void parse_defaults(Parser *ps, Character *ch){
  ps_expect(ps,TK_LBRACE,"{");
  while(!ps_is(ps,TK_RBRACE)){
    char *k=ps_expect_ident(ps,"defaults key");
    ps_expect(ps,TK_COLON,":");
    if(strcmp(k,"defense_posture")==0){
      char *v=ps_expect_string(ps,"posture");
      free(ch->defense_posture);
      ch->defense_posture=v;
      ps_expect(ps,TK_SEMI,";");
      free(k);
      continue;
    }
    if(ps_is(ps,TK_STRING)){ char *tmp=ps_expect_string(ps,"string"); free(tmp); }
    else { (void)ps_expect_number(ps,"number"); }
    ps_expect(ps,TK_SEMI,";");
    free(k);
  }
  ps_expect(ps,TK_RBRACE,"}");
}

static void parse_thresholds(Parser *ps, Character *ch){
  ps_expect(ps,TK_LBRACE,"{");
  while(!ps_is(ps,TK_RBRACE)){
    if(!ps_is_ident(ps,"when")) dief("%s:%d: expected when", ps->filename, ps->lx.cur.line);
    lx_next_token(&ps->lx);
    Expr *cond=parse_expr(ps);
    if(!ps_is_ident(ps,"do")) dief("%s:%d: expected do", ps->filename, ps->lx.cur.line);
    lx_next_token(&ps->lx);
    Stmt *action=parse_action_stmt(ps);
    ps_expect(ps,TK_SEMI,";");
    ThresholdRule tr; tr.cond=cond; tr.action=action;
    VEC_PUSH(ch->thresholds,tr);
  }
  ps_expect(ps,TK_RBRACE,"}");
}

static int parse_int_lit(Parser *ps){
  Token *t=&ps->lx.cur;
  if(ps_is(ps,TK_NUMBER)){ int v=(int)(t->num+0.5); lx_next_token(&ps->lx); return v; }
  if(ps_is(ps,TK_DURATION)){ int v=t->iticks; lx_next_token(&ps->lx); return v; }
  dief("%s:%d: expected int literal", ps->filename, t->line);
  return 0;
}

static void parse_plan(Parser *ps, Character *ch){
  ps_expect(ps,TK_LBRACE,"{");
  while(!ps_is(ps,TK_RBRACE)){
    if(ps_is_ident(ps,"block")){
      lx_next_token(&ps->lx);
      char *bname=ps_expect_ident(ps,"block name");
      int start=parse_int_lit(ps);
      if(ps_is(ps,TK_DOTDOT)){
      ps_expect(ps,TK_DOTDOT,"..");
    } else {
      /* tolerate lexer producing . . */
      ps_expect(ps,TK_DOT,".");
      ps_expect(ps,TK_DOT,".");
    }
      int end=parse_int_lit(ps);

      ps_expect(ps,TK_LBRACE,"{");
      VecStmtPtr stmts; VEC_INIT(stmts);
      parse_stmt_list(ps,&stmts);
      ps_expect(ps,TK_RBRACE,"}");

      BlockRule br; br.name=bname; br.start_tick=start; br.end_tick=end; br.stmts=stmts;
      VEC_PUSH(ch->blocks, br);
      continue;
    }
    if(ps_is_ident(ps,"rule")){
      lx_next_token(&ps->lx);
      char *label=NULL;
      if(ps_is(ps,TK_STRING)) label=ps_expect_string(ps,"label");
      if(!ps_is_ident(ps,"priority")) dief("%s:%d: expected priority", ps->filename, ps->lx.cur.line);
      lx_next_token(&ps->lx);
      double pr=ps_expect_number(ps,"priority number");

      ps_expect(ps,TK_LBRACE,"{");
      VecStmtPtr stmts; VEC_INIT(stmts);
      parse_stmt_list(ps,&stmts);
      ps_expect(ps,TK_RBRACE,"}");

      GenericRule gr; gr.label=label; gr.priority=pr; gr.stmts=stmts;
      VEC_PUSH(ch->rules, gr);
      continue;
    }
    dief("%s:%d: expected block or rule in plan", ps->filename, ps->lx.cur.line);
  }
  ps_expect(ps,TK_RBRACE,"}");
}

static void parse_on(Parser *ps, Character *ch){
  /* on "breach" (when expr)? priority <num> { ... } */
  if(!ps_is_ident(ps,"on")) dief("%s:%d: expected on", ps->filename, ps->lx.cur.line);
  lx_next_token(&ps->lx);
  char *ename=ps_expect_string(ps,"event");
  Expr *when_cond=NULL;
  if(ps_is_ident(ps,"when")){ lx_next_token(&ps->lx); when_cond=parse_expr(ps); }
  if(!ps_is_ident(ps,"priority")) dief("%s:%d: expected priority", ps->filename, ps->lx.cur.line);
  lx_next_token(&ps->lx);
  double pr=ps_expect_number(ps,"priority number");
  ps_expect(ps,TK_LBRACE,"{");
  VecStmtPtr stmts; VEC_INIT(stmts);
  parse_stmt_list(ps,&stmts);
  ps_expect(ps,TK_RBRACE,"}");
  OnEventRule r; r.event_name=ename; r.priority=pr; r.when_cond=when_cond; r.stmts=stmts;
  VEC_PUSH(ch->on_events, r);
}

static void parse_character(Parser *ps, Character *out){
  if(!ps_is_ident(ps,"character")) dief("%s:%d: expected character", ps->filename, ps->lx.cur.line);
  lx_next_token(&ps->lx);
  char *name=ps_expect_string(ps,"character name");
  character_init(out);
  out->name=name;

  ps_expect(ps,TK_LBRACE,"{");
  while(!ps_is(ps,TK_RBRACE)){
    if(ps_is_ident(ps,"version")){ lx_next_token(&ps->lx); (void)ps_expect_number(ps,"version"); ps_expect(ps,TK_SEMI,";"); continue; }
    if(ps_is_ident(ps,"skills")){ lx_next_token(&ps->lx); parse_skills(ps,out); continue; }
    if(ps_is_ident(ps,"traits")){ lx_next_token(&ps->lx); parse_traits(ps,out); continue; }
    if(ps_is_ident(ps,"defaults")){ lx_next_token(&ps->lx); parse_defaults(ps,out); continue; }
    if(ps_is_ident(ps,"thresholds")){ lx_next_token(&ps->lx); parse_thresholds(ps,out); continue; }
    if(ps_is_ident(ps,"plan")){ lx_next_token(&ps->lx); parse_plan(ps,out); continue; }
    if(ps_is_ident(ps,"on")){ parse_on(ps,out); continue; }
    dief("%s:%d: unexpected token in character block", ps->filename, ps->lx.cur.line);
  }
  ps_expect(ps,TK_RBRACE,"}");
}

/* ------------------------------ World/Catalog parsing (subset) ------------------------------ */

static void skip_block(Parser *ps){
  int depth=0;
  if(ps_is(ps,TK_LBRACE)){ ps_expect(ps,TK_LBRACE,"{"); depth=1; }
  while(depth>0 && !ps_is(ps,TK_EOF)){
    if(ps_is(ps,TK_LBRACE)){ ps_expect(ps,TK_LBRACE,"{"); depth++; continue; }
    if(ps_is(ps,TK_RBRACE)){ ps_expect(ps,TK_RBRACE,"}"); depth--; continue; }
    lx_next_token(&ps->lx);
  }
}

static void parse_world(World *w, const char *filename, char *src){
  Parser ps; ps_init(&ps, filename, src);
  while(!ps_is(&ps,TK_EOF) && !ps_is_ident(&ps,"world")) lx_next_token(&ps.lx);
  if(ps_is(&ps,TK_EOF)) return;
  lx_next_token(&ps.lx);
  if(ps_is(&ps,TK_STRING)){ char *tmp=ps_expect_string(&ps,"world name"); free(tmp); }
  ps_expect(&ps,TK_LBRACE,"{");
  while(!ps_is(&ps,TK_RBRACE) && !ps_is(&ps,TK_EOF)){
    if(ps_is_ident(&ps,"version")){ lx_next_token(&ps.lx); (void)ps_expect_number(&ps,"version"); ps_expect(&ps,TK_SEMI,";"); continue; }

    if(ps_is_ident(&ps,"shelter")){
      lx_next_token(&ps.lx);
      ps_expect(&ps,TK_LBRACE,"{");
      while(!ps_is(&ps,TK_RBRACE)){
        char *k=ps_expect_ident(&ps,"shelter key");
        ps_expect(&ps,TK_COLON,":");
        double v=ps_expect_number(&ps,"number");
        ps_expect(&ps,TK_SEMI,";");
        if(strcmp(k,"temp_c")==0) w->shelter.temp_c=v;
        else if(strcmp(k,"signature")==0) w->shelter.signature=v;
        else if(strcmp(k,"power")==0) w->shelter.power=v;
        else if(strcmp(k,"water_safe")==0) w->shelter.water_safe=v;
        else if(strcmp(k,"water_raw")==0) w->shelter.water_raw=v;
        else if(strcmp(k,"structure")==0) w->shelter.structure=v;
        else if(strcmp(k,"contamination")==0) w->shelter.contamination=v;
        free(k);
      }
      ps_expect(&ps,TK_RBRACE,"}");
      continue;
    }

    if(ps_is_ident(&ps,"inventory")){
      lx_next_token(&ps.lx);
      ps_expect(&ps,TK_LBRACE,"{");
      while(!ps_is(&ps,TK_RBRACE)){
        char *item=ps_expect_string(&ps,"item");
        ps_expect(&ps,TK_COLON,":");
        if(!ps_is_ident(&ps,"qty")) dief("%s:%d: expected qty", filename, ps.lx.cur.line);
        lx_next_token(&ps.lx);
        double qty=ps_expect_number(&ps,"qty");
        double cond=0.0;
        if(ps_is(&ps,TK_COMMA)){
          ps_expect(&ps,TK_COMMA,",");
          if(!ps_is_ident(&ps,"cond")) dief("%s:%d: expected cond", filename, ps.lx.cur.line);
          lx_next_token(&ps.lx);
          cond=ps_expect_number(&ps,"cond");
        }
        ps_expect(&ps,TK_SEMI,";");
        inv_add(&w->inv,item,qty,cond);
        free(item);
      }
      ps_expect(&ps,TK_RBRACE,"}");
      continue;
    }

    if(ps_is_ident(&ps,"events")){
      lx_next_token(&ps.lx);
      ps_expect(&ps,TK_LBRACE,"{");
      while(!ps_is(&ps,TK_RBRACE)){
        if(ps_is_ident(&ps,"daily")){
          lx_next_token(&ps.lx);
          char *ename=ps_expect_string(&ps,"event name");
          if(!ps_is_ident(&ps,"chance")) dief("%s:%d: expected chance", filename, ps.lx.cur.line);
          lx_next_token(&ps.lx);
          double ch=ps_expect_percent(&ps,"percent");
          if(ps_is_ident(&ps,"when")){ lx_next_token(&ps.lx); (void)parse_expr(&ps); }
          ps_expect(&ps,TK_SEMI,";");
          if(strcmp(ename,"breach")==0) w->events.breach_chance=ch;
          free(ename);
          continue;
        }
        if(ps_is_ident(&ps,"overnight_threat_check")){
          lx_next_token(&ps.lx);
          if(!ps_is_ident(&ps,"chance")) dief("%s:%d: expected chance", filename, ps.lx.cur.line);
          lx_next_token(&ps.lx);
          double ch=ps_expect_percent(&ps,"percent");
          if(ps_is_ident(&ps,"when")){ lx_next_token(&ps.lx); (void)parse_expr(&ps); }
          ps_expect(&ps,TK_SEMI,";");
          w->events.overnight_chance=ch;
          continue;
        }
        dief("%s:%d: unknown events entry", filename, ps.lx.cur.line);
      }
      ps_expect(&ps,TK_RBRACE,"}");
      continue;
    }

    /* ignore other blocks (constants/weather/...) */
    if(ps_is(&ps,TK_IDENT)){
      char *k=ps_expect_ident(&ps,"ident");
      if(ps_is(&ps,TK_LBRACE)) skip_block(&ps);
      else if(ps_is(&ps,TK_SEMI)) ps_expect(&ps,TK_SEMI,";");
      else {
        while(!ps_is(&ps,TK_SEMI) && !ps_is(&ps,TK_EOF)) lx_next_token(&ps.lx);
        if(ps_is(&ps,TK_SEMI)) ps_expect(&ps,TK_SEMI,";");
      }
      free(k);
      continue;
    }
    lx_next_token(&ps.lx);
  }
  if(ps_is(&ps,TK_RBRACE)) ps_expect(&ps,TK_RBRACE,"}");
}

static void parse_catalog(Catalog *cat, const char *filename, char *src){
  Parser ps; ps_init(&ps, filename, src);
  while(!ps_is(&ps,TK_EOF)){
    if(ps_is_ident(&ps,"taskdef")){
      lx_next_token(&ps.lx);
      char *tname=ps_expect_string(&ps,"task name");
      TaskDef *td=cat_get_or_add_task(cat,tname);
      free(tname);

      ps_expect(&ps,TK_LBRACE,"{");
      while(!ps_is(&ps,TK_RBRACE)){
        if(ps_is_ident(&ps,"time")){
          lx_next_token(&ps.lx); ps_expect(&ps,TK_COLON,":");
          int ticks=(int)(ps_expect_number(&ps,"ticks")+0.5);
          ps_expect(&ps,TK_SEMI,";");
          td->time_ticks = (ticks<=0)?1:ticks;
          continue;
        }
        if(ps_is_ident(&ps,"station")){
          lx_next_token(&ps.lx); ps_expect(&ps,TK_COLON,":");
          char *st=ps_expect_ident(&ps,"station");
          ps_expect(&ps,TK_SEMI,";");
          if(td->station) free(td->station);
          td->station=st;
          continue;
        }

        if(ps_is(&ps,TK_IDENT)){
          char *k=ps_expect_ident(&ps,"field");
          if(ps_is(&ps,TK_COLON)){
            ps_expect(&ps,TK_COLON,":");
            while(!ps_is(&ps,TK_SEMI) && !ps_is(&ps,TK_EOF)){
              if(ps_is(&ps,TK_LBRACE)){ skip_block(&ps); break; }
              lx_next_token(&ps.lx);
            }
            if(ps_is(&ps,TK_SEMI)) ps_expect(&ps,TK_SEMI,";");
          } else if(ps_is(&ps,TK_LBRACE)){
            skip_block(&ps);
          } else if(ps_is(&ps,TK_SEMI)){
            ps_expect(&ps,TK_SEMI,";");
          } else {
            while(!ps_is(&ps,TK_SEMI) && !ps_is(&ps,TK_EOF)) lx_next_token(&ps.lx);
            if(ps_is(&ps,TK_SEMI)) ps_expect(&ps,TK_SEMI,";");
          }
          free(k);
          continue;
        }
        lx_next_token(&ps.lx);
      }
      ps_expect(&ps,TK_RBRACE,"}");
      continue;
    }

    if(ps_is_ident(&ps,"itemdef")){
      lx_next_token(&ps.lx);
      char *nm=ps_expect_string(&ps,"item name");
      free(nm);
      if(ps_is(&ps,TK_LBRACE)) skip_block(&ps);
      continue;
    }

    lx_next_token(&ps.lx);
  }
}

/* ------------------------------ Evaluator ------------------------------ */

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

/* ------------------------------ Action selection ------------------------------ */

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

/* ------------------------------ Simulation ------------------------------ */

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

static void apply_task_effects(World *w, Character *ch, const char *task){
  (void)w;
  if(strcmp(task,"Sleeping")==0){ ch->fatigue -= 20; ch->morale += 2; }
  else if(strcmp(task,"Resting")==0){ ch->fatigue -= 10; ch->morale += 1; }
  else if(strcmp(task,"Eating")==0){ ch->hunger += 15; ch->hydration += 8; ch->morale += 1; }
  else if(strcmp(task,"Defensive shooting")==0){ ch->fatigue += 5; ch->morale -= 1; }
  else if(strcmp(task,"Defensive combat")==0){ ch->fatigue += 6; ch->injury += 2; }
  else { ch->fatigue += 2; }
  clamp01_100(&ch->fatigue); clamp01_100(&ch->morale); clamp01_100(&ch->injury); clamp01_100(&ch->hunger); clamp01_100(&ch->hydration);
}

static void print_status(Character *ch){
  printf("    %s stats: hunger=%.0f hyd=%.0f fatigue=%.0f morale=%.0f injury=%.0f illness=%.0f posture=%s\n",
    ch->name, ch->hunger, ch->hydration, ch->fatigue, ch->morale, ch->injury, ch->illness, ch->defense_posture);
}

static void run_sim(World *w, Catalog *cat, Character *A, Character *B, int days){
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

/* ------------------------------ File I/O ------------------------------ */

static char *read_entire_file(const char *path){
  FILE *f=fopen(path,"rb");
  if(!f) return NULL;
  fseek(f,0,SEEK_END);
  long n=ftell(f);
  fseek(f,0,SEEK_SET);
  if(n<0){ fclose(f); return NULL; }
  char *buf=(char*)xmalloc((size_t)n+1);
  size_t rd=fread(buf,1,(size_t)n,f);
  fclose(f);
  buf[rd]=0;
  return buf;
}
static int file_exists(const char *path){
  FILE *f=fopen(path,"rb");
  if(!f) return 0;
  fclose(f);
  return 1;
}

/* ------------------------------ Defaults ------------------------------ */

static void seed_default_catalog(Catalog *cat){
  struct { const char *name; int t; const char *station; } defs[] = {
    {"Eating",1,"kitchen"},
    {"Resting",2,"cot"},
    {"Sleeping",4,"cot"},
    {"Water filtration",2,"wash"},
    {"Meal prep",2,"kitchen"},
    {"Cooking",2,"kitchen"},
    {"Cleaning",2,"wash"},
    {"General shelter chores",2,"chores"},
    {"Maintenance chores",2,"workshop"},
    {"Gun smithing",2,"workshop"},
    {"Power management",2,"power"},
    {"Electrical diagnostics",2,"power"},
    {"Electronics repair",2,"workshop"},
    {"Watering plants",1,"hydroponics"},
    {"Hydroponics maintenance",2,"hydroponics"},
    {"Reading",1,"lounge"},
    {"Talking",1,"lounge"},
    {"Socializing",1,"lounge"},
    {"Food preservation",2,"kitchen"},
    {"Heating",2,"heat"},
    {"Tending a fire",2,"heat"},
    {"Scouting outside",3,"outside"},
    {"Fishing",3,"outside"},
    {"Fish cleaning",1,"kitchen"},
    {"Defensive shooting",3,"defense"},
    {"Defensive combat",3,"defense"},
    {"First aid",1,"med"},
    {"Medical treatment",2,"med"},
  };
  int n=(int)(sizeof(defs)/sizeof(defs[0]));
  for(int i=0;i<n;i++){
    TaskDef *t=cat_get_or_add_task(cat, defs[i].name);
    t->time_ticks=defs[i].t;
    if(defs[i].station){
      if(t->station) free(t->station);
      t->station=xstrdup(defs[i].station);
    }
  }
}

/* ------------------------------ Main ------------------------------ */

static void usage(void){
  fprintf(stderr,
    "usage: lastbreach <a.lbp> <b.lbp> [--days N] [--seed N] [--world file.lbw] [--catalog file.lbc]\n"
    "notes:\n"
    "  - if --world omitted and ./world.lbw exists, it will be loaded\n"
    "  - if --catalog omitted and ./catalog.lbc exists, it will be loaded\n"
  );
  exit(2);
}

int main(int argc, char **argv){
  if(argc < 3) usage();
  const char *a_path=argv[1];
  const char *b_path=argv[2];

  const char *world_path=NULL;
  const char *catalog_path=NULL;
  int days=1;
  unsigned int seed=(unsigned int)time(NULL);

  for(int i=3;i<argc;i++){
    if(strcmp(argv[i],"--days")==0 && i+1<argc){ days=atoi(argv[++i]); continue; }
    if(strcmp(argv[i],"--seed")==0 && i+1<argc){ seed=(unsigned int)strtoul(argv[++i],NULL,10); continue; }
    if(strcmp(argv[i],"--world")==0 && i+1<argc){ world_path=argv[++i]; continue; }
    if(strcmp(argv[i],"--catalog")==0 && i+1<argc){ catalog_path=argv[++i]; continue; }
    usage();
  }

  srand(seed);

  World world; world_init(&world);
  Catalog cat; cat_init(&cat);
  seed_default_catalog(&cat);

  if(!world_path && file_exists("world.lbw")) world_path="world.lbw";
  if(!catalog_path && file_exists("catalog.lbc")) catalog_path="catalog.lbc";

  if(catalog_path){
    char *src=read_entire_file(catalog_path);
    if(!src) dief("failed to read catalog file: %s", catalog_path);
    parse_catalog(&cat, catalog_path, src);
    free(src);
    printf("Loaded catalog: %s\n", catalog_path);
  }
  if(world_path){
    char *src=read_entire_file(world_path);
    if(!src) dief("failed to read world file: %s", world_path);
    parse_world(&world, world_path, src);
    free(src);
    printf("Loaded world: %s\n", world_path);
  }

  char *a_src=read_entire_file(a_path);
  char *b_src=read_entire_file(b_path);
  if(!a_src) dief("failed to read %s", a_path);
  if(!b_src) dief("failed to read %s", b_path);

  Parser pa; ps_init(&pa,a_path,a_src);
  Parser pb; ps_init(&pb,b_path,b_src);

  while(!ps_is_ident(&pa,"character") && !ps_is(&pa,TK_EOF)) lx_next_token(&pa.lx);
  while(!ps_is_ident(&pb,"character") && !ps_is(&pb,TK_EOF)) lx_next_token(&pb.lx);
  if(ps_is(&pa,TK_EOF)) dief("%s: no character block found", a_path);
  if(ps_is(&pb,TK_EOF)) dief("%s: no character block found", b_path);

  Character A,B;
  parse_character(&pa,&A);
  parse_character(&pb,&B);

  printf("Loaded characters: %s and %s\n", A.name, B.name);
  printf("Seed=%u days=%d\n", seed, days);

  run_sim(&world,&cat,&A,&B,days);

  free(a_src); free(b_src);
  return 0;
}
