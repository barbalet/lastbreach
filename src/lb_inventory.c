#include "lastbreach.h"
/**
 * lb_inventory.c
 *
 * Module: Inventory container used by the simulation; supports quantity tracking and best condition.
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */


/** Initializes an inventory (empty item list). */
void inv_init(Inventory *inv) {
  VEC_INIT(inv->items);
}
ItemEntry *inv_find(Inventory *inv, const char *key) {
  for (int i = 0;i<inv->items.n;i++) if (strcmp(inv->items.v[i].key, key)==0) return &inv->items.v[i];
  return NULL;
}

/** Adds quantity for an item key; tracks best (max) condition seen. */
void inv_add(Inventory *inv, const char *key, double qty, double cond) {
  ItemEntry *e = inv_find(inv, key);
  if (!e) {
    ItemEntry ne;
    ne.key = xstrdup(key);
    ne.qty = qty;
    ne.best_cond = cond;
    VEC_PUSH(inv->items, ne);
  }
  else {
    e->qty += qty;
    if (cond > e->best_cond) e->best_cond = cond;
  }
}

/** Returns quantity in stock for a key. */
double inv_stock(Inventory *inv, const char *key) {
  ItemEntry *e = inv_find(inv, key);
  return e?e->qty:0.0;
}

/** Returns non-zero if any quantity exists for a key. */
int inv_has(Inventory *inv, const char *key) {
  return inv_stock(inv, key) > 0.0;
}

/** Returns the best condition observed for a key (0 if missing). */
double inv_cond(Inventory *inv, const char *key) {
  ItemEntry *e = inv_find(inv, key);
  return e?e->best_cond:0.0;
}
