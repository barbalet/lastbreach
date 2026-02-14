#include "lb_runtime_internal.h"
/**
 * lb_sim.c
 *
 * Module: Tick/day simulation loop, world events, and task progression/output.
 */

static int rand_percent(void) {
    return rand()%100;
}

typedef struct {
    int breach_tick;
    int breach_level;
} DayEvents;

typedef struct {
    const char *name;
    double hunger;
    double hydration;
    double morale;
    double injury;
    double illness;
    double temp_c;
    double power;
    double water_safe;
    double water_raw;
    double structure;
    double contamination;
    double signature;
} TaskDelta;

static const TaskDelta kTaskDeltas[] = {
    {"Reading", 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Eating", 12, 5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Cooking", 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Meal prep", 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Food preservation", 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, -0.5, 0},
    {"Sleeping", 0, 0, 2, -1, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Resting", 0, 0, 1, -0.5, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Socializing", 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Talking", 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Watching", 0, 0, 2, 0, 0, 0, -0.5, 0, 0, 0, 0, 0},
    {"Computer work", 0, 0, 1, 0, 0, 0, -1.5, 0, 0, 0, 0, 0.2},
    {"Playing video games", 0, 0, 4, 0, 0, 0, -1, 0, 0, 0, 0, 0.2},
    {"Playing guitar", 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0.3},
    {"Knitting", 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Crocheting", 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Sewing", 0, 0, 2, 0, 0, 0, 0, 0, 0, 0.3, 0, 0},
    {"Crafting", 0, 0, 2, 0, 0, 0, -0.5, 0, 0, 0.4, 0, 0.2},
    {"Painting", 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Drawing", 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Gardening", 0, 0, 2, 0, 0, 0, 0, 0, 0, 0.2, -0.5, 0},
    {"Watering plants", 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, -0.5, 0},
    {"Hydroponics maintenance", 0, 0, 1, 0, 0, 0, -0.3, 0, 0, 0.5, -1, 0},
    {"Aquarium maintenance", 0, 0, 1, 0, 0, 0, -0.2, 0, 0, 0, -0.8, 0},
    {"Fishing", 0, 0, 1, 0, 0, 0, -0.4, 0, 0, 0, 0, 0.8},
    {"Fish cleaning", 0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, 0.3, 0},
    {"Swimming", 0, -2, 2, -1, 0, 0, 0, 0, 0, 0, 0, 0.5},
    {"Scouting outside", 0, 0, -1, 1, 0, 0, -0.8, 0, 0, 0, 0, 1.2},
    {"Telescope use", 0, 0, 1, 0, 0, 0, -0.2, 0, 0, 0, 0, 0.6},
    {"Defensive shooting", 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0.8},
    {"Defensive combat", 0, 0, -1, 2, 0, 0, 0, 0, 0, -0.5, 0, 1.0},
    {"Gun smithing", 0, 0, 1, 0, 0, 0, 0, 0, 0, 0.4, 0, 0.2},
    {"Electronics repair", 0, 0, 1, 0, 0, 0, 0.4, 0, 0, 0, 0, 0},
    {"Electrical diagnostics", 0, 0, 0, 0, 0, 0, 0.2, 0, 0, 0, 0, 0},
    {"Soldering", 0, 0, 0, 0, 0, 0, 0.2, 0, 0, 0, 0, 0},
    {"Power management", 0, 0, 0.5, 0, 0, 0, 1.5, 0, 0, 0, 0, 0},
    {"Radio communication", 0, 0, 0, 0, 0, 0, -0.4, 0, 0, 0, 0, 1.5},
    {"Tending a fire", 0, 0, 0.5, 0, 0, 1.2, 0, 0, 0, 0, 0, 0.4},
    {"Heating", 0, 0, 0, 0, 0, 2.0, 0, 0, 0, 0, 0, 0},
    {"General shelter chores", 0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0.5, -0.8, 0},
    {"Maintenance chores", 0, 0, 0.5, 0, 0, 0, 0.2, 0, 0, 1.0, -0.2, 0},
    {"Cleaning", 0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, -2.0, 0},
    {"First aid", 0, 0, 1, -12, 0, 0, 0, 0, 0, 0, -0.5, 0},
    {"Medical treatment", 0, 0, 1, 0, -12, 0, -0.2, 0, 0, 0, -1.0, 0},
    {"Water collection", 0, 0, 0, 0, 0, 0, 0, 0, 2.5, 0, -0.4, 0.4},
    {"Water filtration", 0, 0, 0, 0, 0, 0, -0.2, 2.0, -2.0, 0, -1.0, 0}
};

static const char *kPlantProduce[] = {
    "Tomato",
    "Green bean",
    "Chili",
    "Garlic"
};

static void plan_day_events(World *w, DayEvents *ev) {
    ev->breach_tick = -1;
    ev->breach_level = 0;
    if (rand_percent() < (int)(w->events.breach_chance+0.5)) {
        int t = 6 + (rand()%16);
        /* 6..21 */
        ev->breach_tick = t;
        double s = w->shelter.signature, st = w->shelter.structure;
        int lvl = 1;
        if (st<70 || s>15) lvl = 2;
        if (st<55 || s>25) lvl = 3;
        if ((rand()%100)<25 && lvl<3) lvl++;
        ev->breach_level = lvl;
    }
}

static void clamp01_100(double *v) {
    if (*v<0) *v = 0;
    if (*v>100) *v = 100;
}

static void clamp_world(World *w) {
    if (w->shelter.power < 0) w->shelter.power = 0;
    if (w->shelter.power > 100) w->shelter.power = 100;
    if (w->shelter.water_safe < 0) w->shelter.water_safe = 0;
    if (w->shelter.water_safe > 100) w->shelter.water_safe = 100;
    if (w->shelter.water_raw < 0) w->shelter.water_raw = 0;
    if (w->shelter.water_raw > 100) w->shelter.water_raw = 100;
    if (w->shelter.structure < 0) w->shelter.structure = 0;
    if (w->shelter.structure > 100) w->shelter.structure = 100;
    if (w->shelter.contamination < 0) w->shelter.contamination = 0;
    if (w->shelter.contamination > 100) w->shelter.contamination = 100;
    if (w->shelter.signature < 0) w->shelter.signature = 0;
    if (w->shelter.signature > 100) w->shelter.signature = 100;
    if (w->shelter.temp_c < -30) w->shelter.temp_c = -30;
    if (w->shelter.temp_c > 60) w->shelter.temp_c = 60;
    clamp01_100(&w->hydroponic_health);
    if (w->cooked_food_portions < 0) w->cooked_food_portions = 0;
}

static const TaskDelta *find_task_delta(const char *task) {
    int n = (int)(sizeof(kTaskDeltas)/sizeof(kTaskDeltas[0]));
    for (int i = 0; i<n; i++) {
        if (strcmp(kTaskDeltas[i].name, task)==0) return &kTaskDeltas[i];
    }
    return NULL;
}

static double inv_consume(Inventory *inv, const char *key, double qty) {
    if (qty <= 0) return 0.0;
    ItemEntry *e = inv_find(inv, key);
    if (!e || e->qty <= 0) return 0.0;
    if (qty > e->qty) qty = e->qty;
    e->qty -= qty;
    if (e->qty < 0) e->qty = 0;
    return qty;
}

static double consume_world_water(World *w, double amount) {
    double used = 0.0;
    if (amount <= 0) return 0.0;

    if (w->shelter.water_safe > 0) {
        double take = amount;
        if (take > w->shelter.water_safe) take = w->shelter.water_safe;
        w->shelter.water_safe -= take;
        amount -= take;
        used += take;
    }
    if (amount > 0) {
        double take = inv_consume(&w->inv, "Water", amount);
        amount -= take;
        used += take;
    }
    if (amount > 0 && w->shelter.water_raw > 0) {
        double take = amount;
        if (take > w->shelter.water_raw) take = w->shelter.water_raw;
        w->shelter.water_raw -= take;
        amount -= take;
        used += take;
    }
    return used;
}

static int consume_meal(World *w, double *hunger_gain, double *hydration_gain) {
    struct {
        const char *name;
        double qty;
        double hunger;
        double hydration;
    } foods[] = {
        {"Food", 1.0, 12.0, 5.0},
        {"Fish", 1.0, 10.0, 2.0},
        {"Tomato", 1.0, 5.0, 2.0},
        {"Green bean", 1.0, 4.0, 1.0},
        {"Chili", 0.5, 2.0, 0.0},
        {"Garlic", 0.5, 1.5, 0.0},
        {"Ramen", 1.0, 8.0, -1.0},
        {"Canned spam", 1.0, 9.0, -0.5},
        {"Canned tomato", 1.0, 6.0, 1.0},
        {"Canned beans", 1.0, 7.0, 0.5},
        {"Canned corn", 1.0, 6.0, 0.5},
        {"Canned tuna", 1.0, 8.0, 0.0}
    };
    int n = (int)(sizeof(foods)/sizeof(foods[0]));
    for (int i = 0; i<n; i++) {
        double eaten = inv_consume(&w->inv, foods[i].name, foods[i].qty);
        if (eaten > 0.0) {
            double scale = eaten/foods[i].qty;
            *hunger_gain = foods[i].hunger * scale;
            *hydration_gain = foods[i].hydration * scale;

            /*
              Food produced by Cooking/Meal prep is tracked as "cooked portions".
              Eating those portions gives a higher nutritional payoff than raw produce.
            */
            if (strcmp(foods[i].name, "Food")==0 && w->cooked_food_portions > 0.0) {
                double cooked_used = eaten;
                if (cooked_used > w->cooked_food_portions) cooked_used = w->cooked_food_portions;
                *hunger_gain += 6.0 * cooked_used;
                *hydration_gain += 3.0 * cooked_used;
                w->cooked_food_portions -= cooked_used;
            }
            return 1;
        }
    }
    *hunger_gain = 0.0;
    *hydration_gain = 0.0;
    return 0;
}

static void apply_task_delta(World *w, Character *ch, const TaskDelta *d) {
    if (!d) return;
    ch->hunger += d->hunger;
    ch->hydration += d->hydration;
    ch->morale += d->morale;
    ch->injury += d->injury;
    ch->illness += d->illness;

    w->shelter.temp_c += d->temp_c;
    w->shelter.power += d->power;
    w->shelter.water_safe += d->water_safe;
    w->shelter.water_raw += d->water_raw;
    w->shelter.structure += d->structure;
    w->shelter.contamination += d->contamination;
    w->shelter.signature += d->signature;
}

static void overnight_plant_tick(World *w) {
    double plants = inv_stock(&w->inv, "Plant");

    if (inv_stock(&w->inv, "Hydroponic planter") > 0.0) w->hydroponic_health += 1.0;
    else w->hydroponic_health -= 6.0;

    if (w->plants_watered_today) w->hydroponic_health += 4.0;
    else w->hydroponic_health -= 8.0;

    if (w->hydroponics_maintained_today) w->hydroponic_health += 3.0;

    if (w->shelter.temp_c < 2.0 || w->shelter.temp_c > 34.0) w->hydroponic_health -= 5.0;
    else w->hydroponic_health += 1.0;

    clamp01_100(&w->hydroponic_health);

    if (plants <= 0.0 && w->hydroponic_health > 45.0 && inv_stock(&w->inv, "Seeds") > 0.2 && inv_stock(&w->inv, "Soil") > 0.1) {
        if (inv_consume(&w->inv, "Seeds", 0.2) > 0.0 && inv_consume(&w->inv, "Soil", 0.1) > 0.0) {
            inv_add(&w->inv, "Plant", 0.6, 100.0);
            plants = inv_stock(&w->inv, "Plant");
            printf("    hydroponics: seeds germinated into starter plants\n");
        }
    }

    if (plants > 0.0) {
        double growth = (w->hydroponic_health - 50.0)/70.0;
        if (w->plants_watered_today) growth += 0.3;
        if (w->hydroponics_maintained_today) growth += 0.2;
        if (growth >= 0.0) inv_add(&w->inv, "Plant", growth, 100.0);
        else inv_consume(&w->inv, "Plant", -growth);

        plants = inv_stock(&w->inv, "Plant");
        int attempts = (int)(plants/1.2);
        if (attempts < 1) attempts = 1;
        if (attempts > 5) attempts = 5;

        int produce_counts[4] = {0, 0, 0, 0};
        int harvests = 0;
        for (int i = 0; i<attempts; i++) {
            int chance = (int)(w->hydroponic_health*0.6 + plants*12.0);
            if (chance > 90) chance = 90;
            if (rand_percent() < chance) {
                int kind = rand()%4;
                inv_add(&w->inv, kPlantProduce[kind], 1.0, 95.0);
                inv_consume(&w->inv, "Plant", 0.12);
                produce_counts[kind]++;
                harvests++;
            }
        }

        if (harvests > 0) {
            printf("    hydroponics harvest:");
            for (int i = 0; i<4; i++) {
                if (produce_counts[i] > 0) printf(" %s x%d", kPlantProduce[i], produce_counts[i]);
            }
            printf("\n");
        }
    }

    w->plants_watered_today = 0;
    w->hydroponics_maintained_today = 0;
    clamp_world(w);
}

static void tick_decay(Character *ch) {
    ch->hunger -= 0.8;
    ch->hydration -= 1.0;
    ch->morale -= 0.1;
    clamp01_100(&ch->hunger);
    clamp01_100(&ch->hydration);
    clamp01_100(&ch->morale);
}

/*
  Fatigue model ("fatigue" == tiredness, 0..100):
  - increases while awake (idle or working)
  - decreases continuously while Resting/Sleeping

  This prevents the common lock-up where a character repeatedly selects
  Resting/Sleeping but never recovers enough to resume the plan.
*/
static void fatigue_tick(Character *ch) {
    double df = 0.0;
    if (ch->rt_task) {
        if (strcmp(ch->rt_task, "Sleeping")==0) df = -6.0;
        else if (strcmp(ch->rt_task, "Resting")==0) df = -3.0;
        else df = +1.0;
        /* any other task tires you */
    } else {
        df = +0.5;
        /* being awake but idle still costs something */
    }
    ch->fatigue += df;
    clamp01_100(&ch->fatigue);
}

static void apply_task_effects(World *w, Character *ch, const char *task) {
    /* fatigue is handled per-tick in fatigue_tick() */
    const TaskDelta *d = find_task_delta(task);
    apply_task_delta(w, ch, d);

    if (strcmp(task, "Eating")==0) {
        double h = 0.0;
        double hy = 0.0;
        if (consume_meal(w, &h, &hy)) {
            ch->hunger += h;
            ch->hydration += hy;
        } else {
            ch->morale -= 2.0;
            ch->illness += 1.0;
        }
    } else if (strcmp(task, "Meal prep")==0 || strcmp(task, "Cooking")==0) {
        double meal_parts = 0.0;
        meal_parts += inv_consume(&w->inv, "Fish", 0.5)*1.2;
        meal_parts += inv_consume(&w->inv, "Tomato", 0.5);
        meal_parts += inv_consume(&w->inv, "Green bean", 0.5);
        meal_parts += inv_consume(&w->inv, "Chili", 0.25);
        meal_parts += inv_consume(&w->inv, "Garlic", 0.25);
        if (meal_parts > 0.0) {
            inv_add(&w->inv, "Food", meal_parts, 100.0);
            w->cooked_food_portions += meal_parts;
        }
    } else if (strcmp(task, "Food preservation")==0) {
        double preserved = inv_consume(&w->inv, "Food", 1.5);
        if (preserved > 0.0) {
            if (w->cooked_food_portions > 0.0) {
                double taken = preserved;
                if (taken > w->cooked_food_portions) taken = w->cooked_food_portions;
                w->cooked_food_portions -= taken;
            }
            const char *canned[] = {
                "Canned tomato",
                "Canned corn",
                "Canned beans",
                "Canned tuna",
                "Canned spam"
            };
            inv_add(&w->inv, canned[rand()%5], 1.0, 95.0);
        }
    } else if (strcmp(task, "Gardening")==0) {
        int has_planter = inv_stock(&w->inv, "Hydroponic planter") > 0.0;
        double water_used = consume_world_water(w, 0.5);
        if (has_planter && water_used > 0.0 && inv_consume(&w->inv, "Seeds", 0.3) > 0.0 && inv_consume(&w->inv, "Soil", 0.2) > 0.0) {
            inv_add(&w->inv, "Plant", 1.0, 100.0);
            w->hydroponic_health += 6.0;
            printf("    gardening: planted seeds (Plant +1.0)\n");
        }
    } else if (strcmp(task, "Watering plants")==0) {
        double used = consume_world_water(w, 1.0);
        if (used > 0.0) {
            w->plants_watered_today = 1;
            w->hydroponic_health += 4.0*used;
            if (inv_stock(&w->inv, "Plant") > 0.0) inv_add(&w->inv, "Plant", 0.25*used, 100.0);
        } else {
            w->hydroponic_health -= 4.0;
        }
    } else if (strcmp(task, "Hydroponics maintenance")==0) {
        w->hydroponics_maintained_today = 1;
        if (inv_consume(&w->inv, "Fertilizer", 0.25) > 0.0) w->hydroponic_health += 6.0;
        else w->hydroponic_health += 3.0;
    } else if (strcmp(task, "Aquarium maintenance")==0) {
        int has_tank = (inv_stock(&w->inv, "Aquarium") > 0.0) || (inv_stock(&w->inv, "Fish tank") > 0.0);
        if (has_tank && inv_stock(&w->inv, "Fish") > 0.0) ch->morale += 1.0;
        if (!has_tank) ch->morale -= 1.0;
    } else if (strcmp(task, "Fishing")==0) {
        double bait = inv_consume(&w->inv, "Bait", 0.3);
        double hooks = inv_consume(&w->inv, "Fishing hooks", 0.1);
        double catch_qty = 0.2;
        if (inv_stock(&w->inv, "Fishing rod") > 0.0) catch_qty += 0.5;
        catch_qty += bait*1.8;
        catch_qty += hooks*2.0;
        inv_add(&w->inv, "Fish", catch_qty, 80.0);
    } else if (strcmp(task, "Fish cleaning")==0) {
        double fish = inv_consume(&w->inv, "Fish", 1.0);
        if (fish > 0.0) inv_add(&w->inv, "Food", fish*1.1, 100.0);
    } else if (strcmp(task, "Soldering")==0 || strcmp(task, "Electronics repair")==0) {
        inv_consume(&w->inv, "Solder wire", 0.2);
    } else if (strcmp(task, "Defensive shooting")==0) {
        if (inv_consume(&w->inv, "Ammunition", 2.0) < 1.0) ch->morale -= 2.0;
    } else if (strcmp(task, "Tending a fire")==0 || strcmp(task, "Heating")==0) {
        if (inv_consume(&w->inv, "Firewood", 1.0) <= 0.0) {
            if (inv_consume(&w->inv, "Fuel can", 0.4) <= 0.0) {
                w->shelter.temp_c -= 1.0;
                ch->morale -= 1.0;
            }
        }
    } else if (strcmp(task, "Power management")==0) {
        if (inv_stock(&w->inv, "Solar panel") > 0.0) w->shelter.power += 1.5;
        if (inv_stock(&w->inv, "Generator") > 0.0 && inv_consume(&w->inv, "Fuel can", 0.3) > 0.0) {
            w->shelter.power += 4.0;
            w->shelter.signature += 0.6;
        }
    } else if (strcmp(task, "Radio communication")==0) {
        int has_radio = (inv_stock(&w->inv, "Radio") > 0.0) || (inv_stock(&w->inv, "Antenna") > 0.0) || (inv_stock(&w->inv, "Satellite dish") > 0.0);
        if (!has_radio) ch->morale -= 1.0;
    } else if (strcmp(task, "Water collection")==0) {
        double gain = 1.0;
        if (inv_stock(&w->inv, "Bucket") > 0.0) gain += 1.0;
        if (inv_stock(&w->inv, "Watering can") > 0.0) gain += 0.5;
        if (inv_stock(&w->inv, "Water tank") > 0.0 || inv_stock(&w->inv, "Water barrel") > 0.0) gain += 0.5;
        w->shelter.water_raw += gain;
    } else if (strcmp(task, "Water filtration")==0) {
        double filter_capacity = 2.0;
        if (inv_stock(&w->inv, "Water filter") <= 0.0) filter_capacity = 0.5;
        if (w->shelter.water_raw > 0.0) {
            double moved = w->shelter.water_raw;
            if (moved > filter_capacity) moved = filter_capacity;
            w->shelter.water_raw -= moved;
            w->shelter.water_safe += moved*0.9;
        }
    } else if (strcmp(task, "First aid")==0) {
        inv_consume(&w->inv, "First-aid box", 0.05);
    } else if (strcmp(task, "Medical treatment")==0) {
        inv_consume(&w->inv, "Medical box", 0.05);
    }

    clamp01_100(&ch->morale);
    clamp01_100(&ch->injury);
    clamp01_100(&ch->hunger);
    clamp01_100(&ch->hydration);
    clamp01_100(&ch->illness);
    clamp_world(w);
}

static void print_status(Character *ch) {
    printf("    %s stats: hunger=%.0f hyd=%.0f fatigue=%.0f morale=%.0f injury=%.0f illness=%.0f posture=%s\n",
           ch->name, ch->hunger, ch->hydration, ch->fatigue, ch->morale, ch->injury, ch->illness, ch->defense_posture);
}

void run_sim(World *w, Catalog *cat, Character *A, Character *B, int days) {
    for (int day = 0; day<days; day++) {
        DayEvents ev;
        plan_day_events(w, &ev);
        w->plants_watered_today = 0;
        w->hydroponics_maintained_today = 0;

        printf("\n=== DAY %d === shelter(structure=%.0f temp=%.1f power=%.0f sig=%.0f water_safe=%.0f hydro=%.0f plants=%.1f cooked=%.1f) breach_chance=%.0f%%\n",
               day,
               w->shelter.structure,
               w->shelter.temp_c,
               w->shelter.power,
               w->shelter.signature,
               w->shelter.water_safe,
               w->hydroponic_health,
               inv_stock(&w->inv, "Plant"),
               w->cooked_food_portions,
               w->events.breach_chance);

        for (int tick = 0; tick<DAY_TICKS; tick++) {
            int ev_breach = (ev.breach_tick==tick);
            int breach_level = ev_breach?ev.breach_level:0;
            int ev_overnight = (tick==DAY_TICKS-1);

            printf("\n  [day %d tick %02d] ", day, tick);
            if (ev_breach) printf("EVENT: BREACH level=%d! ", breach_level);
            if (ev_overnight) printf("EVENT: overnight_threat_check ");
            printf("\n");

            tick_decay(A);
            tick_decay(B);
            fatigue_tick(A);
            fatigue_tick(B);

            /* progress ongoing tasks */
            if (A->rt_remaining>0) {
                A->rt_remaining--;
                if (A->rt_remaining==0 && A->rt_task) {
                    printf("    %s completed: %s\n", A->name, A->rt_task);
                    apply_task_effects(w, A, A->rt_task);
                    A->rt_task = NULL;
                    A->rt_station = NULL;
                    A->rt_priority = 0;
                }
            }
            if (B->rt_remaining>0) {
                B->rt_remaining--;
                if (B->rt_remaining==0 && B->rt_task) {
                    printf("    %s completed: %s\n", B->name, B->rt_task);
                    apply_task_effects(w, B, B->rt_task);
                    B->rt_task = NULL;
                    B->rt_station = NULL;
                    B->rt_priority = 0;
                }
            }

            Candidate ca, cb;
            cand_reset(&ca);
            cand_reset(&cb);
            if (A->rt_remaining==0) ca = choose_action(A, w, cat, day, tick, breach_level, ev_breach, ev_overnight);
            if (B->rt_remaining==0) cb = choose_action(B, w, cat, day, tick, breach_level, ev_breach, ev_overnight);

            /* station conflict */
            if (A->rt_remaining==0 && B->rt_remaining==0 && ca.kind==1 && cb.kind==1) {
                if (ca.station && cb.station && strcmp(ca.station, cb.station)==0) {
                    int a_wins = (ca.priority > cb.priority) || (ca.priority==cb.priority && strcmp(A->name, B->name)<=0);
                    if (a_wins) {
                        printf("    CONFLICT: station '%s' claimed by %s (priority %.1f); %s yields\n", ca.station, A->name, ca.priority, B->name);
                        cb.kind = 3;
                    } else {
                        printf("    CONFLICT: station '%s' claimed by %s (priority %.1f); %s yields\n", cb.station, B->name, cb.priority, A->name);
                        ca.kind = 3;
                    }
                }
            }

            /* start/continue */
            if (A->rt_remaining==0) {
                if (ca.kind==1) {
                    A->rt_task = ca.task_name;
                    A->rt_station = ca.station;
                    A->rt_remaining = ca.ticks;
                    A->rt_priority = ca.priority;
                    printf("    %s starts: %s (%dt) station=%s priority=%.1f\n", A->name, ca.task_name, ca.ticks, ca.station?ca.station:"-", ca.priority);
                } else {
                    printf("    %s idle\n", A->name);
                }
            } else {
                printf("    %s continues: %s (remaining %dt)\n", A->name, A->rt_task?A->rt_task:"(none)", A->rt_remaining);
            }

            if (B->rt_remaining==0) {
                if (cb.kind==1) {
                    B->rt_task = cb.task_name;
                    B->rt_station = cb.station;
                    B->rt_remaining = cb.ticks;
                    B->rt_priority = cb.priority;
                    printf("    %s starts: %s (%dt) station=%s priority=%.1f\n", B->name, cb.task_name, cb.ticks, cb.station?cb.station:"-", cb.priority);
                } else {
                    printf("    %s idle\n", B->name);
                }
            } else {
                printf("    %s continues: %s (remaining %dt)\n", B->name, B->rt_task?B->rt_task:"(none)", B->rt_remaining);
            }

            /* breach consequence */
            if (ev_breach) {
                int defended = 0;
                if (A->rt_task && strstr(A->rt_task, "Defensive")!=NULL) defended = 1;
                if (B->rt_task && strstr(B->rt_task, "Defensive")!=NULL) defended = 1;
                if (!defended) {
                    double dmg = 4.0*breach_level;
                    w->shelter.structure -= dmg;
                    if (w->shelter.structure<0) w->shelter.structure = 0;
                    printf("    BREACH impact: structure -%.0f (now %.0f)\n", dmg, w->shelter.structure);
                } else {
                    printf("    BREACH defended: minimal structure loss\n");
                    w->shelter.structure -= (breach_level==3?1.0:0.5);
                    if (w->shelter.structure<0) w->shelter.structure = 0;
                }
            }

            print_status(A);
            print_status(B);

            if (ev_overnight) {
                int roll = rand_percent();
                if (roll < (int)(w->events.overnight_chance+0.5)) {
                    printf("    overnight_threat_check: contact outside (roll=%d < %.0f%%)\n", roll, w->events.overnight_chance);
                    w->shelter.signature += 1.0;
                } else {
                    printf("    overnight_threat_check: quiet night (roll=%d)\n", roll);
                    if (w->shelter.signature>0) w->shelter.signature -= 0.5;
                    if (w->shelter.signature<0) w->shelter.signature = 0;
                }

                overnight_plant_tick(w);
                printf("    hydroponics: health=%.0f plants=%.1f tomato=%.0f green_bean=%.0f chili=%.0f garlic=%.0f\n",
                       w->hydroponic_health,
                       inv_stock(&w->inv, "Plant"),
                       inv_stock(&w->inv, "Tomato"),
                       inv_stock(&w->inv, "Green bean"),
                       inv_stock(&w->inv, "Chili"),
                       inv_stock(&w->inv, "Garlic"));
            }
        }
    }
}
