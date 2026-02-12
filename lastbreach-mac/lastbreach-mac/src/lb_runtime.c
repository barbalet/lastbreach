#include "lb_runtime_internal.h"
/**
 * lb_runtime.c
 *
 * Module: Runtime façade.
 *
 * Runtime implementation has been split by responsibility:
 *  - lb_eval.c      : expression evaluation
 *  - lb_scheduler.c : action selection
 *  - lb_sim.c       : simulation loop
 */
