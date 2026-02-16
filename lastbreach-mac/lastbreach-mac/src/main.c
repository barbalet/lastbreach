#include "lastbreach.h"
/**
 * main.c
 *
 * Module: Program entry point and CLI argument parsing for the DSL runner.
 *
 * This file is part of the modularized LastBreach DSL runner (C99, no third-party
 * libraries). The goal here is readability: small functions, clear names, and
 * comments that explain *why* a piece of logic exists.
 */

static void usage(void) {
    fprintf(stderr,
            "usage: lastbreach <a.lbp> <b.lbp> [--days N] [--seed N] [--world file.lbw] [--catalog file.lbc]\n"
            "notes:\n"
            "  - if --world omitted and ./world.lbw exists, it will be loaded\n"
            "  - if --catalog omitted and ./catalog.lbc exists, it will be loaded\n"
           );
    exit(2);
}

/** main function. */
int main(int argc, char **argv) {
    if (argc < 3) usage();
    const char *a_path = argv[1];
    const char *b_path = argv[2];
    const char *world_path = NULL;
    const char *catalog_path = NULL;
    int days = 1;
    unsigned int seed = (unsigned int)time(NULL);
    for (int i = 3; i<argc; i++) {
        if (strcmp(argv[i], "--days")==0 && i+1<argc) {
            days = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--seed")==0 && i+1<argc) {
            seed = (unsigned int)strtoul(argv[++i], NULL, 10);
            continue;
        }
        if (strcmp(argv[i], "--world")==0 && i+1<argc) {
            world_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--catalog")==0 && i+1<argc) {
            catalog_path = argv[++i];
            continue;
        }
        usage();
    }
    srand(seed);
    World world;
    world_init(&world);
    Catalog cat;
    cat_init(&cat);
    seed_default_catalog(&cat);
    /* Auto-discover local data files for convenience in developer workflows. */
    if (!world_path && file_exists("world.lbw")) world_path = "world.lbw";
    if (!catalog_path && file_exists("catalog.lbc")) catalog_path = "catalog.lbc";
    if (catalog_path) {
        char *src = read_entire_file(catalog_path);
        if (!src) dief("failed to read catalog file: %s", catalog_path);
        parse_catalog(&cat, catalog_path, src);
        free(src);
        printf("Loaded catalog: %s\n", catalog_path);
    }
    if (world_path) {
        char *src = read_entire_file(world_path);
        if (!src) dief("failed to read world file: %s", world_path);
        parse_world(&world, world_path, src);
        free(src);
        printf("Loaded world: %s\n", world_path);
    }
    char *a_src = read_entire_file(a_path);
    char *b_src = read_entire_file(b_path);
    if (!a_src) dief("failed to read %s", a_path);
    if (!b_src) dief("failed to read %s", b_path);
    Parser pa;
    ps_init(&pa, a_path, a_src);
    Parser pb;
    ps_init(&pb, b_path, b_src);
    /* Skip any DSL preamble until the first `character` block in each file. */
    while (!ps_is_ident(&pa, "character") && !ps_is(&pa, TK_EOF)) lx_next_token(&pa.lx);
    while (!ps_is_ident(&pb, "character") && !ps_is(&pb, TK_EOF)) lx_next_token(&pb.lx);
    if (ps_is(&pa, TK_EOF)) dief("%s: no character block found", a_path);
    if (ps_is(&pb, TK_EOF)) dief("%s: no character block found", b_path);
    Character A, B;
    parse_character(&pa, &A);
    parse_character(&pb, &B);
    printf("Loaded characters: %s and %s\n", A.name, B.name);
    printf("Seed=%u days=%d\n", seed, days);
    run_sim(&world, &cat, &A, &B, days);
    free(a_src);
    free(b_src);
    return 0;
}
