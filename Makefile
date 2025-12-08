CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99 -fshort-enums

all: bin/ltrep bin/compl bin/equiv bin/test

bin/ltrep: ltrep/ltrep.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -Wno-unused-value -Wno-unused-parameter -I./ $^ -o $@

bin/compl: examples/compl.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -I./ $^ -o $@

bin/equiv: examples/equiv.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -I./ $^ -o $@

bin/synth: examples/synth.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -I./ $^ -o $@

bin/test: test.c bin/ltre.o | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -Wno-missing-field-initializers $^ -o $@

bin/ltre.o: ltre.c ltre.h | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -Wno-sign-compare -Wno-missing-field-initializers -Wno-implicit-fallthrough -Wno-bool-operation -c $< -o $@

bin/:
	mkdir bin/

clean:
	rm -rf bin/
