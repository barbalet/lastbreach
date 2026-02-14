#include "test_framework.h"

void register_core_tests(void);
void register_parser_eval_tests(void);
void register_scheduler_sim_tests(void);

int main(void) {
    test_begin_suite("lastbreach unit tests");
    register_core_tests();
    register_parser_eval_tests();
    register_scheduler_sim_tests();
    return test_end_suite();
}
