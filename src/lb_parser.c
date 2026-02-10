#include "lastbreach.h"

void ps_init(Parser *ps, const char *filename, char *src){
  ps->filename=filename;
  ps->lx.src=src; ps->lx.len=strlen(src); ps->lx.pos=0; ps->lx.line=1;
  lx_next_token(&ps->lx);
}
int ps_is(Parser *ps, TokenKind k){ return ps->lx.cur.kind==k; }
int ps_is_ident(Parser *ps, const char *s){
  if(!ps_is(ps,TK_IDENT)) return 0;
  Token *t=&ps->lx.cur;
  int n=(int)strlen(s);
  return (t->len==n && strncmp(t->start,s,(size_t)n)==0);
}
void ps_expect(Parser *ps, TokenKind k, const char *what){
  if(!ps_is(ps,k)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
  lx_next_token(&ps->lx);
}
char *ps_expect_ident(Parser *ps, const char *what){
  if(!ps_is(ps,TK_IDENT)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
  char *s=tk_cstr(&ps->lx.cur);
  lx_next_token(&ps->lx);
  return s;
}
char *ps_expect_string(Parser *ps, const char *what){
  if(!ps_is(ps,TK_STRING)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
  char *s=tk_cstr(&ps->lx.cur);
  lx_next_token(&ps->lx);
  return s;
}
double ps_expect_number(Parser *ps, const char *what){
  Token *t=&ps->lx.cur;
  if(ps_is(ps,TK_NUMBER)){ double v=t->num; lx_next_token(&ps->lx); return v; }
  if(ps_is(ps,TK_DURATION)){ double v=(double)t->iticks; lx_next_token(&ps->lx); return v; }
  dief("%s:%d: expected %s", ps->filename, t->line, what);
  return 0;
}
double ps_expect_percent(Parser *ps, const char *what){
  if(!ps_is(ps,TK_PERCENT)) dief("%s:%d: expected %s", ps->filename, ps->lx.cur.line, what);
  double v=ps->lx.cur.num;
  lx_next_token(&ps->lx);
  return v;
}

static void skip_block(Parser *ps);

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

void parse_character(Parser *ps, Character *out){
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



/* Shared helper: skip an arbitrary {...} block (used by parser for unknown/ignored blocks) */
static void skip_block(Parser *ps){
  int depth=0;
  if(ps_is(ps,TK_LBRACE)){ ps_expect(ps,TK_LBRACE,"{"); depth=1; }
  while(depth>0 && !ps_is(ps,TK_EOF)){
    if(ps_is(ps,TK_LBRACE)){ ps_expect(ps,TK_LBRACE,"{"); depth++; continue; }
    if(ps_is(ps,TK_RBRACE)){ ps_expect(ps,TK_RBRACE,"}"); depth--; continue; }
    lx_next_token(&ps->lx);
  }
}
