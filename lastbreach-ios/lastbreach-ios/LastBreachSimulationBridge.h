#ifndef LASTBREACH_SIMULATION_BRIDGE_H
#define LASTBREACH_SIMULATION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

char *lb_ios_run_simulation_json(
    const char *world_source,
    const char *catalog_source,
    const char *joel_source,
    const char *mara_source,
    int days,
    unsigned int seed
);

void lb_ios_free_string(char *string);

#ifdef __cplusplus
}
#endif

#endif
