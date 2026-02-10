#include "lastbreach.h"

void cat_init(Catalog *c){ VEC_INIT(c->tasks); }

TaskDef *cat_find_task(Catalog *c, const char *name){
  for(int i=0;i<c->tasks.n;i++) if(strcmp(c->tasks.v[i].name,name)==0) return &c->tasks.v[i];
  return NULL;
}
TaskDef *cat_get_or_add_task(Catalog *c, const char *name){
  TaskDef *t=cat_find_task(c,name);
  if(t) return t;
  TaskDef nt; nt.name=xstrdup(name); nt.time_ticks=1; nt.station=NULL;
  VEC_PUSH(c->tasks, nt);
  return &c->tasks.v[c->tasks.n-1];
}


