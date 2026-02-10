CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic

INCLUDES = -Iinclude
SRCS = \
  src/main.c \
  src/lb_common.c \
  src/lb_inventory.c \
  src/lb_catalog.c \
  src/lb_world.c \
  src/lb_lexer.c \
  src/lb_ast.c \
  src/lb_parser.c \
  src/lb_data.c \
  src/lb_runtime.c \
  src/lb_io.c \
  src/lb_defaults.c

OBJS = $(SRCS:.c=.o)

all: lastbreach

lastbreach: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c include/lastbreach.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(OBJS) lastbreach

.PHONY: all clean
