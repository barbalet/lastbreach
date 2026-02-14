#include "test_framework.h"
#include "test_support.h"

#include <fcntl.h>
#include <unistd.h>

static void test_xalloc_helpers(void) {
    char *s = xstrdup("alpha");
    void *mem = xmalloc(8);

    ASSERT_TRUE(s != NULL);
    ASSERT_STREQ("alpha", s);
    ASSERT_TRUE(mem != NULL);

    memset(mem, 0xA5, 8);
    mem = xrealloc(mem, 32);
    ASSERT_TRUE(mem != NULL);

    ASSERT_TRUE(xstrdup(NULL) == NULL);

    free(s);
    free(mem);
}

static void test_inventory_basics(void) {
    Inventory inv;
    inv_init(&inv);

    ASSERT_EQ_DBL(0.0, inv_stock(&inv, "Food"), 1e-9);
    ASSERT_EQ_INT(0, inv_has(&inv, "Food"));

    inv_add(&inv, "Food", 2.0, 50.0);
    inv_add(&inv, "Food", 1.5, 30.0);
    inv_add(&inv, "Food", 0.5, 85.0);

    ASSERT_EQ_DBL(4.0, inv_stock(&inv, "Food"), 1e-9);
    ASSERT_EQ_INT(1, inv_has(&inv, "Food"));
    ASSERT_EQ_DBL(85.0, inv_cond(&inv, "Food"), 1e-9);
    ASSERT_EQ_DBL(0.0, inv_cond(&inv, "Missing"), 1e-9);
}

static void test_catalog_basics(void) {
    Catalog cat;
    TaskDef *a;
    TaskDef *b;
    cat_init(&cat);

    a = cat_get_or_add_task(&cat, "Task A");
    ASSERT_TRUE(a != NULL);
    a->time_ticks = 3;
    a->station = xstrdup("workshop");

    b = cat_get_or_add_task(&cat, "Task A");
    ASSERT_TRUE(b != NULL);
    ASSERT_TRUE(a == b);
    ASSERT_EQ_INT(3, b->time_ticks);
    ASSERT_STREQ("workshop", b->station);
    ASSERT_TRUE(cat_find_task(&cat, "nope") == NULL);
}

static void test_world_defaults(void) {
    World w;
    world_init(&w);

    ASSERT_EQ_DBL(5.0, w.shelter.temp_c, 1e-9);
    ASSERT_EQ_DBL(10.0, w.shelter.signature, 1e-9);
    ASSERT_EQ_DBL(25.0, w.shelter.power, 1e-9);
    ASSERT_EQ_DBL(15.0, w.events.breach_chance, 1e-9);
    ASSERT_EQ_DBL(25.0, w.events.overnight_chance, 1e-9);
    ASSERT_EQ_DBL(55.0, w.hydroponic_health, 1e-9);
    ASSERT_EQ_INT(0, w.plants_watered_today);
    ASSERT_EQ_INT(0, w.hydroponics_maintained_today);
    ASSERT_EQ_DBL(0.0, w.cooked_food_portions, 1e-9);
}

static void test_io_helpers(void) {
    char path[] = "/tmp/lastbreach_test_io_XXXXXX";
    const char *payload = "hello from unit test\n";
    int fd = mkstemp(path);
    char *readback;
    ssize_t wrote;

    ASSERT_TRUE(fd >= 0);
    wrote = write(fd, payload, strlen(payload));
    ASSERT_EQ_INT((int)strlen(payload), (int)wrote);
    close(fd);

    ASSERT_EQ_INT(1, file_exists(path));
    readback = read_entire_file(path);
    ASSERT_TRUE(readback != NULL);
    ASSERT_STREQ(payload, readback);
    free(readback);

    unlink(path);
    ASSERT_EQ_INT(0, file_exists(path));
    ASSERT_TRUE(read_entire_file(path) == NULL);
}

static void test_seed_default_catalog_covers_tasks_file(void) {
    Catalog cat;
    char *tasks = read_entire_file("../../data/tasks.txt");
    char *line;
    int seen = 0;

    cat_init(&cat);
    seed_default_catalog(&cat);
    ASSERT_TRUE(tasks != NULL);

    line = strtok(tasks, "\n");
    while (line) {
        char *name = trim_ws(line);
        if (*name) {
            TaskDef *t = cat_find_task(&cat, name);
            seen++;
            ASSERT_TRUE_MSG(t != NULL, "missing seeded task: %s", name);
        }
        line = strtok(NULL, "\n");
    }

    ASSERT_TRUE(seen >= 40);
    ASSERT_EQ_INT(2, cat_find_task(&cat, "Gardening")->time_ticks);
    ASSERT_STREQ("hydroponics", cat_find_task(&cat, "Gardening")->station);
    ASSERT_EQ_INT(1, cat_find_task(&cat, "Radio communication")->time_ticks);
    ASSERT_STREQ("comms", cat_find_task(&cat, "Radio communication")->station);
    ASSERT_EQ_INT(2, cat_find_task(&cat, "Water collection")->time_ticks);
    ASSERT_STREQ("outside", cat_find_task(&cat, "Water collection")->station);

    free(tasks);
}

static void test_dsl_catalog_covers_data_lists(void) {
    char *catalog = read_entire_file("../../dsl/catalog.lbc");
    char *items = read_entire_file("../../data/items.txt");
    char *tasks = read_entire_file("../../data/tasks.txt");
    char *line;

    ASSERT_TRUE(catalog != NULL);
    ASSERT_TRUE(items != NULL);
    ASSERT_TRUE(tasks != NULL);

    line = strtok(items, "\n");
    while (line) {
        char *name = trim_ws(line);
        if (*name) {
            char pat[512];
            snprintf(pat, sizeof(pat), "itemdef \"%s\"", name);
            ASSERT_TRUE_MSG(strstr(catalog, pat) != NULL, "missing itemdef for: %s", name);
        }
        line = strtok(NULL, "\n");
    }

    line = strtok(tasks, "\n");
    while (line) {
        char *name = trim_ws(line);
        if (*name) {
            char pat[512];
            snprintf(pat, sizeof(pat), "taskdef \"%s\"", name);
            ASSERT_TRUE_MSG(strstr(catalog, pat) != NULL, "missing taskdef for: %s", name);
        }
        line = strtok(NULL, "\n");
    }

    free(catalog);
    free(items);
    free(tasks);
}

void register_core_tests(void) {
    test_run_case("xalloc helpers", test_xalloc_helpers);
    test_run_case("inventory basics", test_inventory_basics);
    test_run_case("catalog basics", test_catalog_basics);
    test_run_case("world defaults", test_world_defaults);
    test_run_case("io helpers", test_io_helpers);
    test_run_case("default catalog covers tasks file", test_seed_default_catalog_covers_tasks_file);
    test_run_case("dsl catalog covers data lists", test_dsl_catalog_covers_data_lists);
}
