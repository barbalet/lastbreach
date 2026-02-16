#include "test_framework.h"
#include "test_support.h"

static const char *kSchedCharacterSrc =
    "character \"Sched\" {\n"
    "  version 1;\n"
    "  thresholds { when char.hunger < 50 do task \"Eating\" for 1t priority 90; }\n"
    "  plan {\n"
    "    block day 0..24 { task \"Resting\" for 1t priority 10; }\n"
    "    rule \"fallback\" priority 20 { task \"Talking\" for 1t; }\n"
    "  }\n"
    "  on \"breach\" priority 80 { task \"Defensive combat\" for 2t; }\n"
    "}\n";

static const char *kEatAtTickZeroSrc =
    "character \"A\" {\n"
    "  version 1;\n"
    "  plan {\n"
    "    block day 0..24 {\n"
    "      if tick == 0 {\n"
    "        task \"Eating\" for 1t priority 100;\n"
    "      } else {\n"
    "        task \"Resting\" for 1t priority 10;\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n";

static const char *kAlwaysRestSrc =
    "character \"B\" {\n"
    "  version 1;\n"
    "  plan {\n"
    "    block day 0..24 {\n"
    "      task \"Resting\" for 1t priority 10;\n"
    "    }\n"
    "  }\n"
    "}\n";

static const char *kGrowerSrc =
    "character \"Grower\" {\n"
    "  version 1;\n"
    "  plan {\n"
    "    block day 0..24 {\n"
    "      if tick < 12 {\n"
    "        task \"Watering plants\" for 1t priority 90;\n"
    "      } else {\n"
    "        task \"Hydroponics maintenance\" for 1t priority 80;\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n";

static void seed_world_and_catalog(World *w, Catalog *cat) {
    /* Neutralize random event pressure so tests remain deterministic. */
    world_init(w);
    cat_init(cat);
    seed_default_catalog(cat);
    w->events.breach_chance = 0.0;
    w->events.overnight_chance = 0.0;
}

static void test_choose_action_precedence(void) {
    /*
     * Priority order under test:
     * breach handler > threshold rule > plan/rule fallback.
     */
    Character ch;
    World w;
    Catalog cat;
    Candidate cand;

    parse_character_text("sched_char", kSchedCharacterSrc, &ch);
    seed_world_and_catalog(&w, &cat);

    ch.hunger = 40.0;
    cand = choose_action(&ch, &w, &cat, 0, 5, 2, 1, 0);
    ASSERT_EQ_INT(1, cand.kind);
    ASSERT_STREQ("Defensive combat", cand.task_name);
    ASSERT_EQ_INT(2, cand.ticks);

    ch.hunger = 40.0;
    cand = choose_action(&ch, &w, &cat, 0, 5, 0, 0, 0);
    ASSERT_EQ_INT(1, cand.kind);
    ASSERT_STREQ("Eating", cand.task_name);
    ASSERT_EQ_INT(1, cand.ticks);

    ch.hunger = 80.0;
    cand = choose_action(&ch, &w, &cat, 0, 5, 0, 0, 0);
    ASSERT_EQ_INT(1, cand.kind);
    ASSERT_STREQ("Talking", cand.task_name);
}

static void test_run_sim_cooked_food_bonus(void) {
    /* Cooked portions should provide stronger nutrition than equivalent raw food. */
    World w_raw, w_cooked;
    Catalog cat_raw, cat_cooked;
    Character a_raw, b_raw, a_cooked, b_cooked;
    double hunger_raw, hyd_raw;
    double hunger_cooked, hyd_cooked;

    parse_character_text("eat_raw_a", kEatAtTickZeroSrc, &a_raw);
    parse_character_text("eat_raw_b", kAlwaysRestSrc, &b_raw);
    seed_world_and_catalog(&w_raw, &cat_raw);
    inv_add(&w_raw.inv, "Food", 4.0, 100.0);
    a_raw.hunger = 30.0;
    a_raw.hydration = 30.0;

    srand(9);
    run_sim_quiet(&w_raw, &cat_raw, &a_raw, &b_raw, 1);
    hunger_raw = a_raw.hunger;
    hyd_raw = a_raw.hydration;

    parse_character_text("eat_cooked_a", kEatAtTickZeroSrc, &a_cooked);
    parse_character_text("eat_cooked_b", kAlwaysRestSrc, &b_cooked);
    seed_world_and_catalog(&w_cooked, &cat_cooked);
    inv_add(&w_cooked.inv, "Food", 4.0, 100.0);
    w_cooked.cooked_food_portions = 4.0;
    a_cooked.hunger = 30.0;
    a_cooked.hydration = 30.0;

    srand(9);
    run_sim_quiet(&w_cooked, &cat_cooked, &a_cooked, &b_cooked, 1);
    hunger_cooked = a_cooked.hunger;
    hyd_cooked = a_cooked.hydration;

    ASSERT_TRUE_MSG(hunger_cooked > hunger_raw + 4.0,
                    "expected cooked hunger gain > raw (raw=%.2f cooked=%.2f)",
                    hunger_raw, hunger_cooked);
    ASSERT_TRUE_MSG(hyd_cooked > hyd_raw + 2.0,
                    "expected cooked hydration gain > raw (raw=%.2f cooked=%.2f)",
                    hyd_raw, hyd_cooked);
    ASSERT_TRUE(w_cooked.cooked_food_portions < 4.0);
}

static void test_run_sim_hydroponics_produce(void) {
    /* A watered/maintained setup should yield at least some produce in one day. */
    World w;
    Catalog cat;
    Character grower, helper;
    double produce_before;
    double produce_after;

    parse_character_text("grower", kGrowerSrc, &grower);
    parse_character_text("helper", kAlwaysRestSrc, &helper);
    seed_world_and_catalog(&w, &cat);

    inv_add(&w.inv, "Hydroponic planter", 1.0, 100.0);
    inv_add(&w.inv, "Plant", 4.0, 100.0);
    inv_add(&w.inv, "Fertilizer", 8.0, 100.0);
    inv_add(&w.inv, "Water", 20.0, 100.0);
    w.shelter.water_safe = 20.0;
    w.hydroponic_health = 80.0;

    produce_before = produce_total(&w);
    srand(123);
    run_sim_quiet(&w, &cat, &grower, &helper, 1);
    produce_after = produce_total(&w);

    ASSERT_TRUE_MSG(produce_after > produce_before,
                    "expected produce growth (before=%.2f after=%.2f)",
                    produce_before, produce_after);
    ASSERT_TRUE(w.hydroponic_health >= 70.0);
    ASSERT_TRUE(inv_stock(&w.inv, "Plant") > 0.0);
}

void register_scheduler_sim_tests(void) {
    test_run_case("scheduler precedence", test_choose_action_precedence);
    test_run_case("sim cooked-food bonus", test_run_sim_cooked_food_bonus);
    test_run_case("sim hydroponics produce", test_run_sim_hydroponics_produce);
}
