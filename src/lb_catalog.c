#include "lastbreach.h"
/**
 * lb_catalog.c
 *
 * Module: Task catalog container; stores task definitions referenced by characters and rules.
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */


/** Initializes a catalog (empty task list). */
void cat_init(Catalog *c) {
  VEC_INIT(c->tasks);
}
TaskDef *cat_find_task(Catalog *c, const char *name) {
  for (int i = 0;i<c->tasks.n;i++) if (strcmp(c->tasks.v[i].name, name)==0) return &c->tasks.v[i];
  return NULL;
}
TaskDef *cat_get_or_add_task(Catalog *c, const char *name) {
  TaskDef *t = cat_find_task(c, name);
  if (t) return t;
  TaskDef nt;
  nt.name = xstrdup(name);
  nt.time_ticks = 1;
  nt.station = NULL;
  VEC_PUSH(c->tasks, nt);
  return &c->tasks.v[c->tasks.n-1];
}
