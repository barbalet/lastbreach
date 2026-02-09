CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic

all: lastbreach

lastbreach: lastbreach.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f lastbreach
