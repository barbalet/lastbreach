#include "lastbreach.h"
/**
 * lb_defaults.c
 *
 * Module: Built-in default catalog entries used when no .lbc catalog file is supplied.
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */


/** Populates a catalog with a reasonable default task list. */
void seed_default_catalog(Catalog *cat) {
    struct {
        const char *name;
        int t;
        const char *station;
    }
    defs[] = {
        {"Reading", 1, "lounge"},
        {"Eating", 1, "kitchen"},
        {"Cooking", 2, "kitchen"},
        {"Meal prep", 2, "kitchen"},
        {"Food preservation", 2, "kitchen"},
        {"Sleeping", 4, "cot"},
        {"Resting", 2, "cot"},
        {"Socializing", 1, "lounge"},
        {"Talking", 1, "lounge"},
        {"Watching", 1, "lounge"},
        {"Computer work", 2, "comms"},
        {"Playing video games", 1, "lounge"},
        {"Playing guitar", 1, "lounge"},
        {"Knitting", 2, "craft"},
        {"Crocheting", 2, "craft"},
        {"Sewing", 2, "craft"},
        {"Crafting", 2, "workshop"},
        {"Painting", 2, "craft"},
        {"Drawing", 1, "craft"},
        {"Gardening", 2, "hydroponics"},
        {"Watering plants", 1, "hydroponics"},
        {"Hydroponics maintenance", 2, "hydroponics"},
        {"Aquarium maintenance", 2, "aquarium"},
        {"Fishing", 3, "outside"},
        {"Fish cleaning", 1, "kitchen"},
        {"Swimming", 2, "outside"},
        {"Scouting outside", 3, "outside"},
        {"Telescope use", 1, "outside"},
        {"Defensive shooting", 3, "defense"},
        {"Defensive combat", 3, "defense"},
        {"Gun smithing", 2, "workshop"},
        {"Electronics repair", 2, "workshop"},
        {"Electrical diagnostics", 2, "power"},
        {"Soldering", 2, "workshop"},
        {"Power management", 2, "power"},
        {"Radio communication", 1, "comms"},
        {"Tending a fire", 2, "heat"},
        {"Heating", 2, "heat"},
        {"General shelter chores", 2, "chores"},
        {"Maintenance chores", 2, "workshop"},
        {"Cleaning", 2, "wash"},
        {"First aid", 1, "med"},
        {"Medical treatment", 2, "med"},
        {"Water collection", 2, "outside"},
        {"Water filtration", 2, "wash"},
    }
    ;
    /* Keep this list aligned with data/tasks.txt and test expectations. */
    int n = (int)(sizeof(defs)/sizeof(defs[0]));
    for (int i = 0; i<n; i++) {
        TaskDef *t = cat_get_or_add_task(cat, defs[i].name);
        t->time_ticks = defs[i].t;
        if (defs[i].station) {
            if (t->station) free(t->station);
            t->station = xstrdup(defs[i].station);
        }
    }
}
