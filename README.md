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

For sample regular expressions, see the test suite [test.c](test.c). For a more realistic use-case, see the small command-line search tool [ltrep/ltrep.c](ltrep/ltrep.c). For demos of of DFA decompilation and equivalence, see the regex complementation tool [examples/compl.c](examples/compl.c) and the regex equivalence tool [examples/equiv.c](examples/equiv.c). For generating matching strings from a regular expression, see the string synthesis tool [examples/synth.c](examples/synth.c).

See [patterns.md](patterns.md) and [grammar.bnf](grammar.bnf) for documentation on regular expression strings.

## Usage

To build and run the test suite:

```sh
make bin/test
bin/test # should have no output
```

To build and run the command-line search tool:

<!-- keep in sync with ltrep/ltrep.1 -->

```sh
make bin/ltrep
sh -c 'cd ltrep/ && sh test.sh ../bin/ltrep' # no output
bin/ltrep -h # displays help
man -l ltrep/ltrep.1 # displays man page
bin/ltrep -Hnko '"(~[\\"]|\\.)*"' ltrep/ltrep.c ltre.c
bin/ltrep -bz '[\p\s]{4,}' bin/ltrep | tr '\0\n' '\n\0'
bin/ltrep -1l "$(cat ltrep/yara.ltre)" bin/* # bin/ltrep
```

To build and run the regex complementation tool:

```sh
make bin/compl
echo 'abc' | bin/compl # |a|ab|(~a|a~b|ab(~c|c.))%
```

To build and run the regex equivalence tool:

```sh
make bin/equiv
echo -e '0-9+&!0.+\t0|1-90-9*' | bin/equiv # equivalent
echo -e '(a+b*)*\t(a*b+)*' | bin/equiv # not equivalent
```

To build and run the string synthesis tool:

```sh
make bin/synth
echo '0' | bin/synth '0|1+' # 0
echo '1' | bin/synth '0|1+' | head -c 256 # 111...
bin/synth '((0{2}!1){2}!2){2}!3' # 010201030102010
echo '0000a0000' | bin/synth "$(cat examples/tm.ltre)"
cat examples/3x5\ ocr.txt | bin/synth "$(cat examples/3x5\ ocr.ltre)"
# use `stty -icanon -echo -nl` for interactive use
```
