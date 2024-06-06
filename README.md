# LTRE

_A linear-time regular expression engine_

## Overview

LTRE is a regular expression library written in C99 which has no dependencies but the C standard library. It parses regular expressions into NFAs then compiles them down to DFAs for linear-time matching.

For sample regular expressions, see the test suite [test.c](test.c). For a more realistic use-case, see the minimal command-line search tool [ltrep.c](ltrep.c).

## Usage

To build and run the test suite:

```bash
make test
bin/test # should have no output
```

To build and run the minimal search tool:

```bash
make ltrep
bin/ltrep -h # displays usage
bin/ltrep '"(^[\\"]|\\<>)*"' ltre.c
```

## Syntax and Semantics

See [grammar.bnf](grammar.bnf) for the regular expression grammar specification. As an informal quick reference, note that:

- Character classes may be nested.
- Character ranges support wraparound.
- Character ranges may appear outside character classes.
- Metacharacters within character classes must be escaped to be matched literally.
- Character classes may be negated by prefixing the opening bracket with `^`.
- Literal characters and character ranges may be negated by prefixing them with `^`.
- `-<>` are considered metacharacters; they may be matched literally by escaping them.
- `.` does not match newlines; to match any character including newlines, use `<>`.
- The empty regular expression matches the empty word; to match no word, use `[]`.

Supported features are as follows:

| Name                 | Example | Explanation                           | Type                         |
| -------------------- | ------- | ------------------------------------- | ---------------------------- |
| Character Literal    | `a`     | Matches the character `a`             | `symbol`                     |
| Metacharacter Escape | `\+`    | Matches the metacharacter `+`         | `symbol`                     |
| C-Style Escape       | `\r`    | Matches the carriage return character | `symbol`                     |
| Hexadecimal Escape   | `\x41`  | Matches the ASCII character `A`       | `symbol`                     |
| Grouping             | `(ab)`  | Matches the subexpression `ab`        | `regex -> regex`             |
| Kleene Star          | `a*`    | Matches zero or more `a`s             | `regex -> regex`             |
| Kleene Plus          | `a+`    | Matches one or more `a`s              | `regex -> regex`             |
| Optional             | `a?`    | Matches zero or one `a`               | `regex -> regex`             |
| Concatenation        | `ab`    | Matches `a` followed by `b`           | `(regex, regex) -> regex`    |
| Alternation          | `a\|b`  | Matches either `a` or `b`             | `(regex, regex) -> regex`    |
| Shorthand Class      | `\d`    | Matches a digit character             | `symset`                     |
| Complement           | `^a`    | Matches any character not in `a`      | `symset -> symset`           |
| Character Range      | `a-z`   | Matches any character from `a` to `z` | `(symbol, symbol) -> symset` |
| Character Class      | `[ab]`  | Matches any character in `a` or `b`   | `[symset] -> symset`         |
| Intersection         | `<ab>`  | Matches any character in `a` and `b`  | `[symset] -> symset`         |

C-style escapes are supported for `abfnrtv`. Metacharacter escapes are supported for `\.-^$*+?[]<>()|`. Shorthand classes are supported for `dDsSwW`. Semantically, the dot metacharacter is considered a shorthand class.

`symbol`s promote to `symset`s (forming a singleton set) and `symset`s promote to `regex`es (which match any single character from the set). Alternation has the lowest precedence, followed by concatenation, followed by quantifiers. At most one quantifier may be applied to a `regex` per grouping level, and at most one complement may be applied to a `symset` per character class level.
