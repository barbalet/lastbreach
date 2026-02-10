#include "lastbreach.h"
/**
 * lb_world.c
 *
 * Module: World state initialization (shelter stats, event chances, inventory).
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */


/** Initializes default world/shelter stats and event chances. */
void world_init(World *w) {
  memset(w, 0, sizeof(*w));
  w->shelter.temp_c = 5.0;
  w->shelter.signature = 10.0;
  w->shelter.power = 25.0;
  w->shelter.water_safe = 5.0;
  w->shelter.water_raw = 10.0;
  w->shelter.structure = 75.0;
  w->shelter.contamination = 10.0;
  inv_init(&w->inv);
  w->events.breach_chance = 15.0;
  w->events.overnight_chance = 25.0;
}
