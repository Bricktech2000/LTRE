# LTRE

_Finite automaton regular expression engine_

## Overview

LTRE is a regular expression library written in C99 that has no dependencies but the C standard library. It compiles patterns down to minimal DFAs for linear-time matching and provides facilities for manipulating regular expressions, for lazily constructing DFAs and for decompiling DFAs back into regular expressions.

```
                  regex_ignorecase _ regex_reverse  dfa_optimize _
              regex_differentiate | | regex_cmp    dfa_minimize | | dfa_equivalent
                                  | V                           | V
(pattern)-------ltre_parse----->(regex)------ltre_compile----->(dfa)--->ltre_serialize--->(image)
         <----ltre_stringify----   |   <----ltre_decompile-----  |  <--ltre_deserialize---
         ---ltre_fixed_string-->   |   ----ltre_determinize--->  |
                                   V                             V
                           ltre_matches_lazy                ltre_matches
```

For sample regular expressions, see the test suite [test.c](test.c). For a more realistic use-case, see the small command-line search tool [ltrep.c](ltrep.c). For demos of of DFA decompilation and equivalence, see the regex complementation tool [compl.c](compl.c) and the regex equivalence tool [equiv.c](equiv.c). For generating matching strings from a regular expression, see the string synthesis tool [synth.c](synth.c).

## Usage

To build and run the test suite:

```sh
make bin/test
bin/test # should have no output
```

To build and run the command-line search tool:

```sh
make bin/ltrep
sh test.sh # should have no output
bin/ltrep -h # displays usage
bin/ltrep -o '"(~[\\"]|\\.)*"' ltrep.c ltre.c
```

To build and run the regex complementation tool:

```sh
make bin/compl
echo 'abc' | bin/compl # a|ab|(~a|a~b|ab~c|abc.)%|
```

To build and run the regex equivalence tool:

#xxx uev bcond, so ths eul nau. cdjus chec tha `r=s` s `accepting` nd `terminating`

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
# use `stty -icanon -echo -nl` for interactive use
```

## Syntax and Semantics

See [grammar.bnf](grammar.bnf) for the regular expression grammar specification. As an informal quick reference, note that:

#xxx notsf =, %, ! (sepby), : (concat), : (rptsh)

#xxx not: **o** quants rptd. rsn stha cso !, n ols pos to mej. eg `a++!,`. nd cso duol rptsh to, n ols pos to mej

#xxx not: ith doto cdjus prefes RE ith `~\n*&`

- Character ranges support wraparound and may appear outside character classes.
- Metacharacters within character classes must be escaped to be matched literally.
- Character classes are complemented by prefixing the opening bracket with `~`.
- Literal characters and character ranges may be complemented by prefixing them with `~`.
- `-~<>%:&=!` are metacharacters; they can be matched literally by escaping them.
- `.` matches any character, including newlines; to match any character but newlines, use `~\n`.
- `\w` matches alphanumeric characters, excluding underscores; to include underscores, use `[\w_]`.
- The empty-string regular expression matches the empty word; to match no word, use `[]`.
- The lower bound of bounded repetitions may be omitted and defaults to `0`.
- Regular expressions can be intersected with infix `&` and complemented with prefix `!`. #xxx bicond

Supported features are as follows:

#todo add ! sepby

#xxx doc associativity of !

| Name                    | Example    | Meaning                                                   | Type                         |
| ----------------------- | ---------- | --------------------------------------------------------- | ---------------------------- |
| Literal Character       | `a`        | Symbol consisting of the literal character `a`            | `symbol`                     |
| Metacharacter Escape    | `\+`       | Symbol consisting of the metacharacter `+`                | `symbol`                     |
| C-Style Escape          | `\r`       | Symbol corresponding to the C-style escape `\r`           | `symbol`                     |
| Hexadecimal Escape      | `\x41`     | Symbol with character code `0x41`                         | `symbol`                     |
| Symbol Promotion        | any symbol | Singleton set containing the symbol                       | `symbol -> symset`           |
| Character Range         | `a-z`      | Set of all characters from `a` to `z` inclusive           | `(symbol, symbol) -> symset` |
| Symset Wildcard         | `.`        | Set of all characters                                     | `symset -> symset`           |
| Shorthand Class         | `\d`       | Set of characters in the PERL-style class `\d`            | `symset`                     |
| Symset Complement       | `~u`       | Set of all characters not in `u`                          | `symset -> symset`           |
| Symset Union            | `[uv]`     | Set of characters in `u` or in `v`                        | `[symset] -> symset`         |
| Symset Intersection     | `<uv>`     | Set of characters in `u` and in `v`                       | `[symset] -> symset`         |
| Symset Promotion        | any symset | Length-one words over the symset                          | `symset -> regex`            |
| Wildcard                | `%`        | All words                                                 | `regex -> regex`             |
| Exact Repetition        | `r{4}`     | Words some `4`-factoring of which consists of `r`s        | `regex -> regex`             |
| Bounded Repetition      | `r{3,5}`   | Words some `3–5`-factoring of which consists of `r`s      | `regex -> regex`             |
| Minimum Repetition      | `r{3,}`    | Words some `≥3`-factoring of which consists of `r`s       | `regex -> regex`             |
| Maximum Repetition      | `r{,5}`    | Words some `≤5`-factoring of which consists of `r`s       | `regex -> regex`             |
| Kleene Star             | `r*`       | Words some n-factoring of which consists of `r`s          | `regex -> regex`             |
| Kleene Plus             | `r+`       | Words some `≥1`-factoring of which consists of `r`s       | `regex -> regex`             |
| Optional                | `r?`       | Words some `≤1`-factoring of which consists of `r`s       | `regex -> regex`             |
| Dual Exact Repetition   | `r:{4}`    | Words all `4`-factorings of which contain some `r`        | `regex -> regex`             |
| Dual Bounded Repetition | `r:{3,5}`  | Words all `3–5`-factorings of which contain some `r`      | `regex -> regex`             |
| Dual Minimum Repetition | `r:{3,}`   | Words all `≥3`-factorings of which contain some `r`       | `regex -> regex`             |
| Dual Maximum Repetition | `r:{,5}`   | Words all `≤5`-factorings of which contain some `r`       | `regex -> regex`             |
| Dual Kleene Star        | `r:*`      | Words all n-factorings of which contain some `r`          | `regex -> regex`             |
| Dual Kleene Plus        | `r:+`      | Words all `≥1`-factorings of which contain some `r`       | `regex -> regex`             |
| Dual Optional           | `r:?`      | Words all `≤1`-factorings of which contain some `r`       | `regex -> regex`             |
| Intercalation           | `r*!s`     | #todo                                                     | `(regex, regex) -> regex`    |
| Dual Intercalation      | `r:*!s`    | #todo                                                     | `(regex, regex) -> regex`    |
| Concatenation           | `rs`       | Words some 2-factoring of which has head `r` and tail `s` | `(regex, regex) -> regex`    |
| Dual Concatenation      | `r:s`      | Words all 2-factorings of which have head `r` or tail `s` | `(regex, regex) -> regex`    |
| Alternation             | `r\|s`     | Words in `r` or in `s`                                    | `(regex, regex) -> regex`    |
| Intersection            | `r&s`      | Words in `r` and in `s`                                   | `(regex, regex) -> regex`    |
| Biconditional           | `r=s`      | Words in `r` if and only if in `s`                        | `(regex, regex) -> regex`    |
| Complement              | `!r`       | Words not in `r`                                          | `regex -> regex`             |
| Grouping                | `(r)`      | Words in the subexpression `r`                            | `regex -> regex`             |

#xxx doc "dual" means dual under complementation

C-style escapes are supported for `abfnrtv`. Metacharacter escapes are supported for `\-.~[]<>%{}*+?:|&=!()`. Shorthand classes are supported for `dDsSwW`.

The only 0-factoring of the empty word is the empty list; no other word has a 0-factoring.

Alternation, intersection and biconditional have the lowest precedence, followed by complementation, followed by dual concatenation, followed by concatenation, followed by quantification and dual quantification, followed by symset complementation. Alternation, intersection and biconditional are right-associative. At most one complement may be applied to a `regex` per grouping level, and at most one symset complement may be applied to a `symset` per symset union or intersection level.
