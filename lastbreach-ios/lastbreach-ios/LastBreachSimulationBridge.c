#include "LastBreachSimulationBridge.h"

#include "../../lastbreach-mac/lastbreach-mac/include/lastbreach.h"

static char *bridge_strdup(const char *s) {
    size_t n = strlen(s);
    char *copy = (char *)malloc(n + 1);
    if (!copy) return NULL;
    memcpy(copy, s, n + 1);
    return copy;
}

static char *bridge_error_json(const char *message) {
    const char *prefix = "{\"type\":\"bridge_error\",\"message\":\"";
    const char *suffix = "\"}\n";
    size_t n = strlen(prefix) + strlen(message) + strlen(suffix);
    char *json = (char *)malloc(n + 1);
    if (!json) return NULL;
    snprintf(json, n + 1, "%s%s%s", prefix, message, suffix);
    return json;
}

static void parse_character_source(Character *out, const char *filename, const char *source) {
    char *copy = xstrdup(source);
    Parser parser;
    ps_init(&parser, filename, copy);

    while (!ps_is_ident(&parser, "character") && !ps_is(&parser, TK_EOF)) {
        lx_next_token(&parser.lx);
    }
    if (ps_is(&parser, TK_EOF)) {
        dief("%s: no character block found", filename);
    }

    parse_character(&parser, out);
    free(copy);
}

static char *read_tmpfile_contents(FILE *file) {
    long length;
    char *buffer;
    size_t read_count;

    if (fflush(file) != 0) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) return NULL;
    length = ftell(file);
    if (length < 0) return NULL;
    if (fseek(file, 0, SEEK_SET) != 0) return NULL;

    buffer = (char *)malloc((size_t)length + 1);
    if (!buffer) return NULL;

    read_count = fread(buffer, 1, (size_t)length, file);
    buffer[read_count] = '\0';
    return buffer;
}

char *lb_ios_run_simulation_json(
    const char *world_source,
    const char *catalog_source,
    const char *joel_source,
    const char *mara_source,
    int days,
    unsigned int seed
) {
    World world;
    Catalog catalog;
    Character joel;
    Character mara;
    SimOptions options;
    FILE *output;
    char *world_copy;
    char *catalog_copy;
    char *json;

    if (!world_source || !catalog_source || !joel_source || !mara_source) {
        return bridge_error_json("missing scenario source");
    }
    if (days <= 0) days = 1;

    world_init(&world);
    cat_init(&catalog);
    seed_default_catalog(&catalog);

    catalog_copy = xstrdup(catalog_source);
    parse_catalog(&catalog, "catalog.lbc", catalog_copy);
    free(catalog_copy);

    world_copy = xstrdup(world_source);
    parse_world(&world, "world.lbw", world_copy);
    free(world_copy);

    parse_character_source(&joel, "joel.lbp", joel_source);
    parse_character_source(&mara, "mara.lbp", mara_source);

    output = tmpfile();
    if (!output) {
        return bridge_error_json("failed to open simulation output buffer");
    }

    srand(seed);
    options.mode = LB_SIM_OUTPUT_JSONL;
    options.out = output;
    options.seed = seed;
    run_sim_with_options(&world, &catalog, &joel, &mara, days, &options);

    json = read_tmpfile_contents(output);
    fclose(output);
    if (!json) {
        return bridge_error_json("failed to read simulation output");
    }
    return json;
}

void lb_ios_free_string(char *string) {
    free(string);
}
