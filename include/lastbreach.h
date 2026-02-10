#ifndef LASTBREACH_H
#define LASTBREACH_H

/* Modularized LastBreach DSL runner
   (split from the original monolithic lastbreach.c)
   C99, no third-party libraries.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>

#define DAY_TICKS 24

/* ------------------------------ Common helpers ------------------------------ */
void dief(const char *fmt, ...);
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

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

void inv_init(Inventory *inv);
ItemEntry *inv_find(Inventory *inv, const char *key);
void inv_add(Inventory *inv, const char *key, double qty, double cond);
double inv_stock(Inventory *inv, const char *key);
int inv_has(Inventory *inv, const char *key);
double inv_cond(Inventory *inv, const char *key);

typedef struct { char *name; int time_ticks; char *station; } TaskDef;
VEC_DECL(VecTaskDef, TaskDef);

typedef struct { VecTaskDef tasks; } Catalog;

void cat_init(Catalog *c);
TaskDef *cat_find_task(Catalog *c, const char *name);
TaskDef *cat_get_or_add_task(Catalog *c, const char *name);

typedef struct {
  double temp_c, signature, power, water_safe, water_raw, structure, contamination;
} Shelter;

typedef struct { double breach_chance, overnight_chance; } WorldEvents;

typedef struct { Shelter shelter; Inventory inv; WorldEvents events; } World;

void world_init(World *w);

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

void lx_next_token(Lexer *lx);
char *tk_cstr(const Token *t);

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

void character_init(Character *c);

/* ------------------------------ Parser ------------------------------ */

typedef struct { const char *filename; Lexer lx; } Parser;

void ps_init(Parser *ps, const char *filename, char *src);
int ps_is(Parser *ps, TokenKind k);
int ps_is_ident(Parser *ps, const char *s);

void ps_expect(Parser *ps, TokenKind k, const char *what);
char *ps_expect_ident(Parser *ps, const char *what);
char *ps_expect_string(Parser *ps, const char *what);
double ps_expect_number(Parser *ps, const char *what);
double ps_expect_percent(Parser *ps, const char *what);

void parse_character(Parser *ps, Character *out);

/* ------------------------------ World/Catalog parsing ------------------------------ */
void parse_catalog(Catalog *cat, const char *filename, char *src);
void parse_world(World *w, const char *filename, char *src);

/* ------------------------------ File I/O ------------------------------ */
int file_exists(const char *path);
char *read_entire_file(const char *path);

/* ------------------------------ Defaults ------------------------------ */
void seed_default_catalog(Catalog *cat);

/* ------------------------------ Simulation ------------------------------ */
void run_sim(World *w, Catalog *cat, Character *A, Character *B, int days);

#endif /* LASTBREACH_H */
