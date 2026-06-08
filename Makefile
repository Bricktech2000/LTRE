.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99 -fshort-enums

all: bin/ltrep bin/compl bin/equiv bin/test
bin/:; mkdir bin/
clean:; rm -rf bin/

bin/ltrep:  bin/ltre.o ltrep/ltrep.c;    $(CC) $(CFLAGS) -o $@ bin/ltre.o -I./ ltrep/ltrep.c -Wno-parentheses -Wno-unused-value -Wno-unused-parameter
bin/compl:  bin/ltre.o examples/compl.c; $(CC) $(CFLAGS) -o $@ bin/ltre.o -I./ examples/compl.c -Wno-parentheses
bin/equiv:  bin/ltre.o examples/equiv.c; $(CC) $(CFLAGS) -o $@ bin/ltre.o -I./ examples/equiv.c -Wno-parentheses
bin/synth:  bin/ltre.o examples/synth.c; $(CC) $(CFLAGS) -o $@ bin/ltre.o -I./ examples/synth.c -Wno-parentheses
bin/test:   bin/ltre.o test.c;           $(CC) $(CFLAGS) -o $@ bin/ltre.o test.c -Wno-parentheses -Wno-missing-field-initializers
bin/ltre.o: bin/ ltre.h ltre.c;          $(CC) $(CFLAGS) -o $@ -c ltre.c -Wno-parentheses -Wno-missing-field-initializers -Wno-sign-compare -Wno-implicit-fallthrough -Wno-bool-operation
