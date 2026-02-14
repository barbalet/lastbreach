#ifndef LASTBREACH_TEST_SUPPORT_H
#define LASTBREACH_TEST_SUPPORT_H

#include "lastbreach.h"
#include "lb_parser_internal.h"
#include "lb_runtime_internal.h"

void parse_character_text(const char *filename, const char *src, Character *out);
void parse_world_text(const char *filename, const char *src, World *out);
void parse_catalog_text(const char *filename, const char *src, Catalog *out);
Expr *parse_expr_text(const char *filename, const char *src, char **storage);
void run_sim_quiet(World *w, Catalog *cat, Character *a, Character *b, int days);
char *trim_ws(char *s);
double produce_total(World *w);

#endif
