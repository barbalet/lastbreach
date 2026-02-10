#include "lastbreach.h"

void inv_init(Inventory *inv){ VEC_INIT(inv->items); }

ItemEntry *inv_find(Inventory *inv, const char *key){
  for(int i=0;i<inv->items.n;i++) if(strcmp(inv->items.v[i].key,key)==0) return &inv->items.v[i];
  return NULL;
}
void inv_add(Inventory *inv, const char *key, double qty, double cond){
  ItemEntry *e = inv_find(inv,key);
  if(!e){
    ItemEntry ne; ne.key=xstrdup(key); ne.qty=qty; ne.best_cond=cond;
    VEC_PUSH(inv->items, ne);
  } else {
    e->qty += qty;
    if (cond > e->best_cond) e->best_cond = cond;
  }
}
double inv_stock(Inventory *inv, const char *key){ ItemEntry *e=inv_find(inv,key); return e?e->qty:0.0; }
int inv_has(Inventory *inv, const char *key){ return inv_stock(inv,key) > 0.0; }
double inv_cond(Inventory *inv, const char *key){ ItemEntry *e=inv_find(inv,key); return e?e->best_cond:0.0; }


