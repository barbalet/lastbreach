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
    /* -1 means no breach that day. */
    int breach_tick;
    int breach_level;
} DayEvents;

typedef struct {
    /* Per-task completion counter used for end-of-run diagnostics. */
    char *task_name;
    int count;
} TaskCount;

typedef struct {
    TaskCount *tasks;
    int n;
    int cap;
    /* These counters make idle/conflict behavior visible in output summaries. */
    int idle_ticks;
    int conflict_yields;
} AgentDiagnostics;

typedef struct {
    /* Canonical task name in catalog/scripts. */
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

/*
 * Baseline per-completion impact table.
 * Task-specific logic in apply_task_effects() can further modify outcomes
 * (inventory usage, conversions, conditional bonuses/penalties, etc.).
 */
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

static void diag_init(AgentDiagnostics *d) {
    memset(d, 0, sizeof(*d));
}

static void diag_free(AgentDiagnostics *d) {
    /* `task_name` strings are owned by diagnostics and must be released. */
    for (int i = 0; i<d->n; i++) free(d->tasks[i].task_name);
    free(d->tasks);
    memset(d, 0, sizeof(*d));
}

static int diag_index_of(const AgentDiagnostics *d, const char *task_name) {
    for (int i = 0; i<d->n; i++) {
        if (strcmp(d->tasks[i].task_name, task_name)==0) return i;
    }
    return -1;
}

static void diag_record_completion(AgentDiagnostics *d, const char *task_name) {
    if (!task_name) return;
    int idx = diag_index_of(d, task_name);
    if (idx >= 0) {
        d->tasks[idx].count++;
        return;
    }
    if (d->n == d->cap) {
        /* Standard doubling growth keeps amortized append cost O(1). */
        d->cap = d->cap ? d->cap*2 : 8;
        d->tasks = xrealloc(d->tasks, (size_t)d->cap*sizeof(*d->tasks));
    }
    d->tasks[d->n].task_name = xstrdup(task_name);
    d->tasks[d->n].count = 1;
    d->n++;
}

static int diag_task_count(const AgentDiagnostics *d, const char *task_name) {
    int idx = diag_index_of(d, task_name);
    if (idx < 0) return 0;
    return d->tasks[idx].count;
}

static int diag_total_completions(const AgentDiagnostics *d) {
    int total = 0;
    for (int i = 0; i<d->n; i++) total += d->tasks[i].count;
    return total;
}

static int vecstr_contains(const VecStr *v, const char *s) {
    for (int i = 0; i<v->n; i++) {
        if (strcmp(v->v[i], s)==0) return 1;
    }
    return 0;
}

static void vecstr_push_unique(VecStr *v, const char *s) {
    if (!s || vecstr_contains(v, s)) return;
    VEC_PUSH(*v, xstrdup(s));
}

static void collect_stmt_tasks(Stmt *s, VecStr *out);

static void collect_stmt_list_tasks(const VecStmtPtr *list, VecStr *out) {
    for (int i = 0; i<list->n; i++) collect_stmt_tasks(list->v[i], out);
}

static void collect_stmt_tasks(Stmt *s, VecStr *out) {
    if (!s) return;
    if (s->kind == ST_TASK) {
        vecstr_push_unique(out, s->u.task.task_name);
        return;
    }
    if (s->kind == ST_IF) {
        collect_stmt_list_tasks(&s->u.if_.then_stmts, out);
        collect_stmt_list_tasks(&s->u.if_.else_stmts, out);
    }
}

static void collect_character_tasks(Character *ch, VecStr *out) {
    /* Flatten all rule sources into one unique task-name list for reporting. */
    VEC_INIT(*out);
    for (int i = 0; i<ch->thresholds.n; i++) collect_stmt_tasks(ch->thresholds.v[i].action, out);
    for (int i = 0; i<ch->blocks.n; i++) collect_stmt_list_tasks(&ch->blocks.v[i].stmts, out);
    for (int i = 0; i<ch->rules.n; i++) collect_stmt_list_tasks(&ch->rules.v[i].stmts, out);
    for (int i = 0; i<ch->on_events.n; i++) collect_stmt_list_tasks(&ch->on_events.v[i].stmts, out);
}

static int group_completed_count(const AgentDiagnostics *d, const char *const *tasks, int ntasks) {
    int total = 0;
    for (int i = 0; i<ntasks; i++) total += diag_task_count(d, tasks[i]);
    return total;
}

static int group_in_plan(const VecStr *planned, const char *const *tasks, int ntasks) {
    for (int i = 0; i<ntasks; i++) {
        if (vecstr_contains(planned, tasks[i])) return 1;
    }
    return 0;
}

static void print_need_line(
    const char *name,
    const char *state,
    double metric,
    const char *metric_name,
    int completed_support,
    int in_plan,
    int in_progress
) {
    printf("      %s: %s (%s=%.0f) support_tasks_completed=%d support_task_in_progress=%s support_tasks_in_plan=%s\n",
           name, state, metric_name, metric, completed_support, in_progress?"yes":"no", in_plan?"yes":"no");
}

static const char *low_is_bad_state(double v, double critical, double low) {
    if (v <= critical) return "CRITICAL";
    if (v <= low) return "LOW";
    return "OK";
}

static const char *high_is_bad_state(double v, double low, double critical) {
    if (v >= critical) return "CRITICAL";
    if (v >= low) return "LOW";
    return "OK";
}

static double edible_stock(World *w) {
    return inv_stock(&w->inv, "Food")
           + inv_stock(&w->inv, "Fish")
           + inv_stock(&w->inv, "Tomato")
           + inv_stock(&w->inv, "Green bean")
           + inv_stock(&w->inv, "Chili")
           + inv_stock(&w->inv, "Garlic")
           + inv_stock(&w->inv, "Ramen")
           + inv_stock(&w->inv, "Canned spam")
           + inv_stock(&w->inv, "Canned tomato")
           + inv_stock(&w->inv, "Canned beans")
           + inv_stock(&w->inv, "Canned corn")
           + inv_stock(&w->inv, "Canned tuna")
           + w->cooked_food_portions;
}

static double total_water_stock(World *w) {
    return w->shelter.water_safe + w->shelter.water_raw + inv_stock(&w->inv, "Water");
}

static int group_in_progress(Character *ch, const char *const *tasks, int ntasks) {
    if (!ch->rt_task || ch->rt_remaining <= 0) return 0;
    for (int i = 0; i<ntasks; i++) {
        if (strcmp(ch->rt_task, tasks[i])==0) return 1;
    }
    return 0;
}

static void print_need_diagnostics(Character *ch, World *w, const AgentDiagnostics *d, const VecStr *planned) {
    /*
     * Curated task groups map core "needs" to concrete actions.
     * This is intentionally heuristic and diagnostic-only.
     */
    static const char *kNourishTasks[] = {"Eating", "Meal prep", "Cooking", "Fish cleaning", "Food preservation"};
    static const char *kHydrationTasks[] = {"Water collection", "Water filtration", "Eating"};
    static const char *kRestTasks[] = {"Sleeping", "Resting"};
    static const char *kMoraleTasks[] = {"Socializing", "Talking", "Reading", "Playing video games", "Playing guitar", "Painting", "Drawing"};
    static const char *kInjuryTasks[] = {"First aid", "Resting", "Sleeping"};
    static const char *kIllnessTasks[] = {"Medical treatment", "Water filtration", "Cleaning", "Resting", "Sleeping"};

    int nourish_done = group_completed_count(d, kNourishTasks, (int)(sizeof(kNourishTasks)/sizeof(kNourishTasks[0])));
    int hydration_done = group_completed_count(d, kHydrationTasks, (int)(sizeof(kHydrationTasks)/sizeof(kHydrationTasks[0])));
    int rest_done = group_completed_count(d, kRestTasks, (int)(sizeof(kRestTasks)/sizeof(kRestTasks[0])));
    int morale_done = group_completed_count(d, kMoraleTasks, (int)(sizeof(kMoraleTasks)/sizeof(kMoraleTasks[0])));
    int injury_done = group_completed_count(d, kInjuryTasks, (int)(sizeof(kInjuryTasks)/sizeof(kInjuryTasks[0])));
    int illness_done = group_completed_count(d, kIllnessTasks, (int)(sizeof(kIllnessTasks)/sizeof(kIllnessTasks[0])));

    int nourish_in_plan = group_in_plan(planned, kNourishTasks, (int)(sizeof(kNourishTasks)/sizeof(kNourishTasks[0])));
    int hydration_in_plan = group_in_plan(planned, kHydrationTasks, (int)(sizeof(kHydrationTasks)/sizeof(kHydrationTasks[0])));
    int rest_in_plan = group_in_plan(planned, kRestTasks, (int)(sizeof(kRestTasks)/sizeof(kRestTasks[0])));
    int morale_in_plan = group_in_plan(planned, kMoraleTasks, (int)(sizeof(kMoraleTasks)/sizeof(kMoraleTasks[0])));
    int injury_in_plan = group_in_plan(planned, kInjuryTasks, (int)(sizeof(kInjuryTasks)/sizeof(kInjuryTasks[0])));
    int illness_in_plan = group_in_plan(planned, kIllnessTasks, (int)(sizeof(kIllnessTasks)/sizeof(kIllnessTasks[0])));

    int nourish_in_progress = group_in_progress(ch, kNourishTasks, (int)(sizeof(kNourishTasks)/sizeof(kNourishTasks[0])));
    int hydration_in_progress = group_in_progress(ch, kHydrationTasks, (int)(sizeof(kHydrationTasks)/sizeof(kHydrationTasks[0])));
    int rest_in_progress = group_in_progress(ch, kRestTasks, (int)(sizeof(kRestTasks)/sizeof(kRestTasks[0])));
    int morale_in_progress = group_in_progress(ch, kMoraleTasks, (int)(sizeof(kMoraleTasks)/sizeof(kMoraleTasks[0])));
    int injury_in_progress = group_in_progress(ch, kInjuryTasks, (int)(sizeof(kInjuryTasks)/sizeof(kInjuryTasks[0])));
    int illness_in_progress = group_in_progress(ch, kIllnessTasks, (int)(sizeof(kIllnessTasks)/sizeof(kIllnessTasks[0])));

    printf("    life-gaps:\n");
    print_need_line("nourishment", low_is_bad_state(ch->hunger, 20.0, 45.0), ch->hunger, "hunger", nourish_done, nourish_in_plan, nourish_in_progress);
    if (ch->hunger <= 45.0 && nourish_done == 0) printf("        gap: recovery tasks for food were never completed.\n");
    if (ch->hunger <= 45.0 && !nourish_in_plan) printf("        gap: no food-recovery task is present in this character's policy.\n");
    if (ch->hunger <= 45.0 && edible_stock(w) < 1.0) printf("        gap: edible stock is near zero (edible_total=%.1f).\n", edible_stock(w));

    print_need_line("hydration", low_is_bad_state(ch->hydration, 20.0, 45.0), ch->hydration, "hydration", hydration_done, hydration_in_plan, hydration_in_progress);
    if (ch->hydration <= 45.0 && hydration_done == 0) printf("        gap: water-related tasks were never completed.\n");
    if (ch->hydration <= 45.0 && !hydration_in_plan) printf("        gap: no water-supply task is present in this character's policy.\n");
    if (ch->hydration <= 45.0 && total_water_stock(w) < 1.0) printf("        gap: available water is near zero (water_total=%.1f).\n", total_water_stock(w));

    print_need_line("rest", high_is_bad_state(ch->fatigue, 65.0, 85.0), ch->fatigue, "fatigue", rest_done, rest_in_plan, rest_in_progress);
    if (ch->fatigue >= 65.0 && rest_done == 0) printf("        gap: no Sleeping/Resting tasks were completed.\n");
    if (ch->fatigue >= 65.0 && !rest_in_plan) printf("        gap: no Sleeping/Resting task exists in this character's policy.\n");

    print_need_line("social/emotional", low_is_bad_state(ch->morale, 25.0, 45.0), ch->morale, "morale", morale_done, morale_in_plan, morale_in_progress);
    if (ch->morale <= 45.0 && morale_done == 0) printf("        gap: morale-support tasks were never completed.\n");
    if (ch->morale <= 45.0 && !morale_in_plan) printf("        gap: no morale-support task exists in this character's policy.\n");

    print_need_line("injury-care", high_is_bad_state(ch->injury, 25.0, 50.0), ch->injury, "injury", injury_done, injury_in_plan, injury_in_progress);
    if (ch->injury >= 25.0 && injury_done == 0) printf("        gap: injury-mitigation tasks were never completed.\n");
    if (ch->injury >= 25.0 && !injury_in_plan) printf("        gap: no injury-mitigation task exists in this character's policy.\n");
    if (ch->injury >= 25.0 && inv_stock(&w->inv, "First-aid box") <= 0.0) printf("        gap: no First-aid box remains in inventory.\n");

    print_need_line("illness-care", high_is_bad_state(ch->illness, 25.0, 50.0), ch->illness, "illness", illness_done, illness_in_plan, illness_in_progress);
    if (ch->illness >= 25.0 && illness_done == 0) printf("        gap: illness-mitigation tasks were never completed.\n");
    if (ch->illness >= 25.0 && !illness_in_plan) printf("        gap: no illness-mitigation task exists in this character's policy.\n");
    if (ch->illness >= 25.0 && inv_stock(&w->inv, "Medical box") <= 0.0) printf("        gap: no Medical box remains in inventory.\n");
}

static void print_agent_diagnostics(Character *ch, Catalog *cat, World *w, const AgentDiagnostics *d) {
    VecStr planned;
    collect_character_tasks(ch, &planned);

    printf("\n  agent: %s\n", ch->name);
    printf("    snapshot: hunger=%.0f hyd=%.0f fatigue=%.0f morale=%.0f injury=%.0f illness=%.0f posture=%s\n",
           ch->hunger, ch->hydration, ch->fatigue, ch->morale, ch->injury, ch->illness, ch->defense_posture);
    printf("    runtime: active_task=%s remaining=%d\n",
           ch->rt_task ? ch->rt_task : "(none)", ch->rt_remaining);
    printf("    activity: total_completed=%d unique_completed=%d idle_ticks=%d conflict_yields=%d\n",
           diag_total_completions(d), d->n, d->idle_ticks, d->conflict_yields);

    if (d->n == 0) {
        printf("    completed_tasks: (none)\n");
    } else {
        printf("    completed_tasks:\n");
        for (int i = 0; i<d->n; i++) {
            printf("      - %s x%d\n", d->tasks[i].task_name, d->tasks[i].count);
        }
    }

    /* Tasks present in policy but never completed often reveal scheduler gaps. */
    int planned_not_done = 0;
    for (int i = 0; i<planned.n; i++) {
        if (diag_task_count(d, planned.v[i]) == 0) planned_not_done++;
    }
    if (planned_not_done == 0) {
        printf("    planned_but_not_completed: (none)\n");
    } else {
        printf("    planned_but_not_completed (%d):\n", planned_not_done);
        for (int i = 0; i<planned.n; i++) {
            if (diag_task_count(d, planned.v[i]) == 0) {
                TaskDef *td = cat_find_task(cat, planned.v[i]);
                int in_progress = (ch->rt_task && ch->rt_remaining > 0 && strcmp(ch->rt_task, planned.v[i])==0);
                printf("      - %s (catalog=%s in_progress=%s)\n", planned.v[i], td?"yes":"no", in_progress?"yes":"no");
            }
        }
    }

    print_need_diagnostics(ch, w, d, &planned);

    for (int i = 0; i<planned.n; i++) free(planned.v[i]);
    VEC_FREE(planned);
}

static void print_world_diagnostics(World *w) {
    printf("  world snapshot: structure=%.0f temp=%.1f power=%.0f sig=%.0f contamination=%.0f water_safe=%.0f water_raw=%.0f hydro=%.0f\n",
           w->shelter.structure,
           w->shelter.temp_c,
           w->shelter.power,
           w->shelter.signature,
           w->shelter.contamination,
           w->shelter.water_safe,
           w->shelter.water_raw,
           w->hydroponic_health);
    printf("  world stock: edible_total=%.1f cooked=%.1f water_total=%.1f first_aid=%.1f medical=%.1f plants=%.1f seeds=%.1f soil=%.1f\n",
           edible_stock(w),
           w->cooked_food_portions,
           total_water_stock(w),
           inv_stock(&w->inv, "First-aid box"),
           inv_stock(&w->inv, "Medical box"),
           inv_stock(&w->inv, "Plant"),
           inv_stock(&w->inv, "Seeds"),
           inv_stock(&w->inv, "Soil"));
}

static void plan_day_events(World *w, DayEvents *ev) {
    ev->breach_tick = -1;
    ev->breach_level = 0;
    if (rand_percent() < (int)(w->events.breach_chance+0.5)) {
        int t = 6 + (rand()%16);
        /* 6..21 */
        ev->breach_tick = t;
        double s = w->shelter.signature, st = w->shelter.structure;
        /*
         * Breach severity increases when the shelter is weak or the signature
         * is loud; a small random bump keeps repeated days less deterministic.
         */
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
    /* Keep world values inside sensible simulation bounds after each mutation. */
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
    /* Return actual consumed amount so callers can scale downstream effects. */
    e->qty -= qty;
    if (e->qty < 0) e->qty = 0;
    return qty;
}

static double consume_world_water(World *w, double amount) {
    double used = 0.0;
    if (amount <= 0) return 0.0;

    /*
     * Preferred order:
     * 1) safe tank water
     * 2) bottled/packaged water inventory
     * 3) raw water (fallback)
     */
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
    /* First available food entry wins; table order encodes preference. */
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
    /*
     * Nightly hydroponics pass:
     * 1) update hydroponic health from actions/environment
     * 2) optionally germinate seeds when empty
     * 3) grow or decay plants
     * 4) probabilistically harvest produce
     */
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
    /* Passive per-tick drift while awake in shelter conditions. */
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

    /*
     * Task-specific branches model inventory/equipment interactions that cannot
     * be represented as simple additive deltas.
     */
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
    AgentDiagnostics da, db;
    diag_init(&da);
    diag_init(&db);

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

            /* Phase 1: passive per-tick decay/fatigue updates. */
            tick_decay(A);
            tick_decay(B);
            fatigue_tick(A);
            fatigue_tick(B);

            /* progress ongoing tasks */
            if (A->rt_remaining>0) {
                A->rt_remaining--;
                if (A->rt_remaining==0 && A->rt_task) {
                    printf("    %s completed: %s\n", A->name, A->rt_task);
                    diag_record_completion(&da, A->rt_task);
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
                    diag_record_completion(&db, B->rt_task);
                    apply_task_effects(w, B, B->rt_task);
                    B->rt_task = NULL;
                    B->rt_station = NULL;
                    B->rt_priority = 0;
                }
            }

            /* Phase 2: ask scheduler for a new action when agent is idle. */
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
                        db.conflict_yields++;
                        cb.kind = 3;
                    } else {
                        printf("    CONFLICT: station '%s' claimed by %s (priority %.1f); %s yields\n", cb.station, B->name, cb.priority, A->name);
                        da.conflict_yields++;
                        ca.kind = 3;
                    }
                }
            }

            /* Phase 3: start chosen tasks or report continuation/idle state. */
            if (A->rt_remaining==0) {
                if (ca.kind==1) {
                    A->rt_task = ca.task_name;
                    A->rt_station = ca.station;
                    A->rt_remaining = ca.ticks;
                    A->rt_priority = ca.priority;
                    printf("    %s starts: %s (%dt) station=%s priority=%.1f\n", A->name, ca.task_name, ca.ticks, ca.station?ca.station:"-", ca.priority);
                } else {
                    da.idle_ticks++;
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
                    db.idle_ticks++;
                    printf("    %s idle\n", B->name);
                }
            } else {
                printf("    %s continues: %s (remaining %dt)\n", B->name, B->rt_task?B->rt_task:"(none)", B->rt_remaining);
            }

            /* Phase 4: resolve event consequences after action assignment. */
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
                /* Phase 5 (last tick only): overnight encounter + plant cycle. */
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

    printf("\n=== SIMULATION COMPLETE ===\n");
    print_world_diagnostics(w);
    print_agent_diagnostics(A, cat, w, &da);
    print_agent_diagnostics(B, cat, w, &db);

    diag_free(&da);
    diag_free(&db);
}
