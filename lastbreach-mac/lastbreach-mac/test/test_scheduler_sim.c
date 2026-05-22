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

static const char *kJsonOpsSrc =
    "character \"Ops\" {\n"
    "  version 1;\n"
    "  plan {\n"
    "    block day 0..24 {\n"
    "      if tick == 0 {\n"
    "        task \"Eating\" for 1t priority 100;\n"
    "      } else if tick == 1 {\n"
    "        task \"Gun smithing\" for 1t priority 90;\n"
    "      } else if tick >= 20 {\n"
    "        task \"Sleeping\" for 1t priority 80;\n"
    "      } else {\n"
    "        task \"Resting\" for 1t priority 10;\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "  on \"breach\" priority 120 {\n"
    "    task \"Defensive shooting\" for 1t priority 120;\n"
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

static char *capture_json_run(World *w, Catalog *cat, Character *a, Character *b, int days, unsigned int seed) {
    FILE *tmp = tmpfile();
    SimOptions options;
    long len;
    char *buf;
    size_t read_len;

    if (!tmp) dief("tmpfile failed while capturing JSON output");
    options.mode = LB_SIM_OUTPUT_JSONL;
    options.out = tmp;
    options.seed = seed;

    srand(seed);
    run_sim_with_options(w, cat, a, b, days, &options);

    fflush(tmp);
    if (fseek(tmp, 0, SEEK_END) != 0) dief("failed to seek JSON capture");
    len = ftell(tmp);
    if (len < 0) dief("failed to measure JSON capture");
    if (fseek(tmp, 0, SEEK_SET) != 0) dief("failed to rewind JSON capture");

    buf = xmalloc((size_t)len + 1);
    read_len = fread(buf, 1, (size_t)len, tmp);
    buf[read_len] = '\0';
    fclose(tmp);
    return buf;
}

static int contains_text(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

static void seed_json_contract_world(World *w, Catalog *cat) {
    seed_world_and_catalog(w, cat);
    w->events.breach_chance = 100.0;
    w->events.overnight_chance = 100.0;
    w->hydroponic_health = 95.0;
    w->shelter.water_safe = 30.0;
    inv_add(&w->inv, "Food", 4.0, 100.0);
    inv_add(&w->inv, "Gun cleaning kit", 1.0, 100.0);
    inv_add(&w->inv, "Rifle", 1.0, 100.0);
    inv_add(&w->inv, "Ammunition", 12.0, 100.0);
    inv_add(&w->inv, "Hydroponic planter", 1.0, 100.0);
    inv_add(&w->inv, "Plant", 6.0, 100.0);
    inv_add(&w->inv, "Fertilizer", 10.0, 100.0);
    inv_add(&w->inv, "Water", 20.0, 100.0);
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

static void test_run_sim_json_event_contract(void) {
    World w;
    Catalog cat;
    Character ops, grower;
    char *json;

    parse_character_text("json_ops", kJsonOpsSrc, &ops);
    parse_character_text("json_grower", kGrowerSrc, &grower);
    seed_json_contract_world(&w, &cat);

    json = capture_json_run(&w, &cat, &ops, &grower, 1, 77);

    ASSERT_TRUE(contains_text(json, "\"type\":\"run_start\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"initial_state\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"tick_snapshot\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"task_started\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"task_completed\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"inventory_changed\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"breach\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"breach_impact\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"overnight_threat_check\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"harvest\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"final_state\""));
    ASSERT_TRUE(contains_text(json, "\"type\":\"simulation_complete\""));

    ASSERT_TRUE(contains_text(json, "\"task\":\"Eating\""));
    ASSERT_TRUE(contains_text(json, "\"task\":\"Gun smithing\""));
    ASSERT_TRUE(contains_text(json, "\"task\":\"Watering plants\""));
    ASSERT_TRUE(contains_text(json, "\"task\":\"Hydroponics maintenance\""));
    ASSERT_TRUE(contains_text(json, "\"task\":\"Defensive shooting\""));
    ASSERT_TRUE(contains_text(json, "\"task\":\"Sleeping\""));
    ASSERT_TRUE(contains_text(json, "\"character_id\":\"ops\""));
    ASSERT_TRUE(contains_text(json, "\"station_id\":\"hydroponics\""));
    ASSERT_TRUE(contains_text(json, "\"item_id\":\"ammunition\""));

    free(json);
}

static void test_run_sim_json_deterministic_output(void) {
    World w1, w2;
    Catalog cat1, cat2;
    Character ops1, grower1, ops2, grower2;
    char *json1;
    char *json2;

    parse_character_text("json_ops_1", kJsonOpsSrc, &ops1);
    parse_character_text("json_grower_1", kGrowerSrc, &grower1);
    seed_json_contract_world(&w1, &cat1);

    parse_character_text("json_ops_2", kJsonOpsSrc, &ops2);
    parse_character_text("json_grower_2", kGrowerSrc, &grower2);
    seed_json_contract_world(&w2, &cat2);

    json1 = capture_json_run(&w1, &cat1, &ops1, &grower1, 1, 77);
    json2 = capture_json_run(&w2, &cat2, &ops2, &grower2, 1, 77);

    ASSERT_STREQ(json1, json2);
    ASSERT_TRUE(contains_text(json1, "\"schema_version\":1"));
    ASSERT_TRUE(contains_text(json1, "\"seed\":77"));

    free(json1);
    free(json2);
}

void register_scheduler_sim_tests(void) {
    test_run_case("scheduler precedence", test_choose_action_precedence);
    test_run_case("sim cooked-food bonus", test_run_sim_cooked_food_bonus);
    test_run_case("sim hydroponics produce", test_run_sim_hydroponics_produce);
    test_run_case("sim json event contract", test_run_sim_json_event_contract);
    test_run_case("sim json deterministic output", test_run_sim_json_deterministic_output);
}
