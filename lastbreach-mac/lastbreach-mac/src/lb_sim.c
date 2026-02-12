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
}
DayEvents;
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
    (void)w;
    /* fatigue is handled per-tick in fatigue_tick() */
    if (strcmp(task, "Sleeping")==0) {
        ch->morale += 2;
    } else if (strcmp(task, "Resting")==0) {
        ch->morale += 1;
    } else if (strcmp(task, "Eating")==0) {
        ch->hunger += 15;
        ch->hydration += 8;
        ch->morale += 1;
    } else if (strcmp(task, "Defensive shooting")==0) {
        ch->morale -= 1;
    } else if (strcmp(task, "Defensive combat")==0) {
        ch->injury += 2;
    }
    clamp01_100(&ch->morale);
    clamp01_100(&ch->injury);
    clamp01_100(&ch->hunger);
    clamp01_100(&ch->hydration);
}
static void print_status(Character *ch) {
    printf("    %s stats: hunger=%.0f hyd=%.0f fatigue=%.0f morale=%.0f injury=%.0f illness=%.0f posture=%s\n",
           ch->name, ch->hunger, ch->hydration, ch->fatigue, ch->morale, ch->injury, ch->illness, ch->defense_posture);
}
void run_sim(World *w, Catalog *cat, Character *A, Character *B, int days) {
    for (int day = 0; day<days; day++) {
        DayEvents ev;
        plan_day_events(w, &ev);
        printf("\n=== DAY %d === shelter(structure=%.0f temp=%.1f power=%.0f sig=%.0f water_safe=%.0f) breach_chance=%.0f%%\n",
               day, w->shelter.structure, w->shelter.temp_c, w->shelter.power, w->shelter.signature, w->shelter.water_safe, w->events.breach_chance);
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
            }
        }
    }
}
