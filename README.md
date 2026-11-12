# LTRE

_Finite automaton regular expression engine_

## Overview

LTRE is a regular expression library written in C99 that has no dependencies but the C standard library. It compiles patterns down to minimal DFAs for linear-time matching and provides facilities for manipulating regular expressions, for lazily constructing DFAs and for decompiling DFAs back into regular expressions.

```
                  regex_ignorecase _ regex_reverse      dfa_mark _
              regex_differentiate | | regex_cmp    dfa_minimize | | dfa_equivalent
                                  | V                           | V
(pattern)-------ltre_parse----->(regex)------ltre_compile----->(dfa)--->ltre_serialize--->(image)
         <----ltre_stringify----   |   <----ltre_decompile-----  |  <--ltre_deserialize---
         ---ltre_fixed_string-->   |   ----ltre_determinize--->  |
                                   V                             V
                           ltre_matches_lazy                ltre_matches
```

For sample regular expressions, see the test suite [test.c](test.c). For a more realistic use-case, see the small command-line search tool [ltrep_/ltrep.c](ltrep_/ltrep.c). For demos of of DFA decompilation and equivalence, see the regex complementation tool [examples/compl.c](examples/compl.c) and the regex equivalence tool [examples/equiv.c](examples/equiv.c). For generating matching strings from a regular expression, see the string synthesis tool [examples/synth.c](examples/synth.c).

See [patterns.md](patterns.md) and [grammar.bnf](grammar.bnf) for documentation on regular expression strings.

## Usage

To build and run the test suite:

```sh
make test
./test # should have no output
```

To build and run the command-line search tool:

<!-- keep in sync with ltrep_/ltrep.1 -->

```sh
make ltrep
sh -c 'cd ltrep_/ && sh test.sh ../ltrep' # no output
./ltrep -h # displays help
man -l ltrep_/ltrep.1 # displays man page
./ltrep -Hnko '"(~[\\"]|\\.)*"' ltrep_/ltrep.c ltre.c
./ltrep -bz '[\p\s]{4,}' ltrep | tr '\0\n' '\n\0'
./ltrep -1l "$(cat ltrep_/yara.ltre)" * # ltrep
./ltrep -H1oc '\n'   ltrep_/ltrep.c ltre.c # wc -l
./ltrep -H1oc '\S\s' ltrep_/ltrep.c ltre.c # wc -w
./ltrep -H1oc '.'    ltrep_/ltrep.c ltre.c # wc -c
```

To build and run the regex complementation tool:

```sh
make compl
echo 'abc' | ./compl # |a|ab|(~a|a~b|ab(~c|c.))%
```

To build and run the regex equivalence tool:

```sh
make equiv
echo -e '0-9+&!0.+\t0|1-90-9*' | ./equiv # equivalent
echo -e '(a+b*)*\t(a*b+)*' | ./equiv # not equivalent
```

To build and run the string synthesis tool:

```sh
make synth
echo '0' | ./synth '0|1+' # 0
echo '1' | ./synth '0|1+' | head -c 256 # 111...
./synth '((0{2}!1){2}!2){2}!3' # 010201030102010
echo '0000a0000' | ./synth "$(cat examples/tm.ltre)"
cat examples/3x5\ ocr.txt | ./synth "$(cat examples/3x5\ ocr.ltre)"
# use `stty -icanon -echo -nl` for interactive use
```
