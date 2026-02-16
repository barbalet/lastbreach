#include "test_support.h"

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

void parse_character_text(const char *filename, const char *src, Character *out) {
    /* Tests pass stack strings; parser expects mutable storage. */
    char *buf = xstrdup(src);
    Parser ps;
    ps_init(&ps, filename, buf);
    parse_character(&ps, out);
    free(buf);
}

void parse_world_text(const char *filename, const char *src, World *out) {
    char *buf = xstrdup(src);
    parse_world(out, filename, buf);
    free(buf);
}

void parse_catalog_text(const char *filename, const char *src, Catalog *out) {
    char *buf = xstrdup(src);
    parse_catalog(out, filename, buf);
    free(buf);
}

Expr *parse_expr_text(const char *filename, const char *src, char **storage) {
    Parser ps;
    *storage = xstrdup(src);
    ps_init(&ps, filename, *storage);
    return parse_expr(&ps);
}

void run_sim_quiet(World *w, Catalog *cat, Character *a, Character *b, int days) {
    /* Redirect stdout so tests can assert on state without log noise. */
    int stdout_fd = dup(fileno(stdout));
    int devnull_fd = open("/dev/null", O_WRONLY);

    if (stdout_fd < 0 || devnull_fd < 0) {
        if (stdout_fd >= 0) close(stdout_fd);
        if (devnull_fd >= 0) close(devnull_fd);
        run_sim(w, cat, a, b, days);
        return;
    }

    fflush(stdout);
    (void)dup2(devnull_fd, fileno(stdout));
    close(devnull_fd);

    run_sim(w, cat, a, b, days);

    fflush(stdout);
    (void)dup2(stdout_fd, fileno(stdout));
    close(stdout_fd);
}

char *trim_ws(char *s) {
    /* In-place trim helper used when scanning fixture files line-by-line. */
    char *end;
    while (*s && isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

double produce_total(World *w) {
    return inv_stock(&w->inv, "Tomato")
           + inv_stock(&w->inv, "Green bean")
           + inv_stock(&w->inv, "Chili")
           + inv_stock(&w->inv, "Garlic");
}
