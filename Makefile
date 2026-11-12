.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99 -fshort-enums

all: ltrep compl equiv test
clean:; rm -f ltrep compl equiv synth test ltre.o

ltrep:  ltre.o ltrep_/ltrep.c;   $(CC) $(CFLAGS) -o $@ ltre.o -I./ ltrep_/ltrep.c -Wno-parentheses -Wno-unused-value -Wno-unused-parameter
compl:  ltre.o examples/compl.c; $(CC) $(CFLAGS) -o $@ ltre.o -I./ examples/compl.c -Wno-parentheses
equiv:  ltre.o examples/equiv.c; $(CC) $(CFLAGS) -o $@ ltre.o -I./ examples/equiv.c -Wno-parentheses
synth:  ltre.o examples/synth.c; $(CC) $(CFLAGS) -o $@ ltre.o -I./ examples/synth.c -Wno-parentheses
test:   ltre.o test.c;           $(CC) $(CFLAGS) -o $@ ltre.o test.c -Wno-parentheses -Wno-missing-field-initializers
ltre.o: ltre.h ltre.c;           $(CC) $(CFLAGS) -o $@ -c ltre.c -Wno-parentheses -Wno-missing-field-initializers -Wno-sign-compare -Wno-implicit-fallthrough -Wno-bool-operation
