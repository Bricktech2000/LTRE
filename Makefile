CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99 -fshort-enums

all: bin/ltrep bin/compl bin/equiv bin/test

bin/ltrep: ltrep.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -Wno-unused-parameter $^ -o $@

bin/compl: compl.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) $^ -o $@

bin/equiv: equiv.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) $^ -o $@

bin/synth: synth.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) $^ -o $@

bin/test: test.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) -Wno-missing-field-initializers $^ -o $@

bin/ltre.o: ltre.c ltre.h | bin/
	$(CC) $(CFLAGS) -Wno-sign-compare -Wno-parentheses -Wno-missing-field-initializers -Wno-implicit-fallthrough -c $< -o $@

bin/:
	mkdir bin/

clean:
	rm -rf bin/
