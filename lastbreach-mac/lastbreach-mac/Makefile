CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic

INCLUDES = -Iinclude
TEST_INCLUDES = -Iinclude -Isrc -Itest
SRCS = \
  src/main.c \
  src/lb_common.c \
  src/lb_inventory.c \
  src/lb_catalog.c \
  src/lb_world.c \
  src/lb_lexer.c \
  src/lb_ast.c \
  src/lb_parser.c \
  src/lb_parser_expr.c \
  src/lb_parser_stmt.c \
  src/lb_parser_sections.c \
  src/lb_data.c \
  src/lb_runtime.c \
  src/lb_eval.c \
  src/lb_scheduler.c \
  src/lb_sim.c \
  src/lb_io.c \
  src/lb_defaults.c

OBJS = $(SRCS:.c=.o)
APP_SRCS = $(filter-out src/main.c,$(SRCS))
APP_OBJS = $(APP_SRCS:.c=.o)

TEST_SRCS = \
  test/test_main.c \
  test/test_framework.c \
  test/test_support.c \
  test/test_core.c \
  test/test_parser_eval.c \
  test/test_scheduler_sim.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_BIN = lastbreach_tests

all: lastbreach

lastbreach: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c include/lastbreach.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

test/%.o: test/%.c include/lastbreach.h test/test_framework.h test/test_support.h
	$(CC) $(CFLAGS) $(TEST_INCLUDES) -c -o $@ $<

$(TEST_BIN): $(APP_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(APP_OBJS) $(TEST_OBJS)

test: $(TEST_BIN)
	./$(TEST_BIN)

src/lb_parser.o src/lb_parser_expr.o src/lb_parser_stmt.o src/lb_parser_sections.o: src/lb_parser_internal.h
src/lb_runtime.o src/lb_eval.o src/lb_scheduler.o src/lb_sim.o: src/lb_runtime_internal.h

clean:
	rm -f $(OBJS) $(TEST_OBJS) lastbreach $(TEST_BIN)

.PHONY: all clean test
