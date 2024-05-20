# LTRE

_A linear-time regular expression engine_

## Overview

LTRE is a regular expression library written in C99 which has no dependencies but the C standard library. It parses regular expressions into NFAs then compiles them down to DFAs for linear-time matching.

## Syntax

See [grammar.bnf](grammar.bnf) for the syntax specification. Note that:

- Metacharacters within character classes must be escaped (with a backslash).
- To include `-` in a character class, ensure it is not preceeded by a literal.
- To include `^` in a character class, ensure it is not the first class character.
- `.` does not match newlines; to match any character including newlines, use `[^]`.
- The empty regular expression matches the empty word; to match no word, use `[]`.

## Features

| Name                 | Example | Description                           |
| -------------------- | ------- | ------------------------------------- |
| Dot                  | `.`     | Matches any character but a newline   |
| Literal Character    | `a`     | Matches the character `a`             |
| Metacharacter Escape | `\+`    | Matches the metacharacter `+`         |
| C-style Escape       | `\r`    | Matches the carriage return character |
| Concatenation        | `ab`    | Matches `a` followed by `b`           |
| Alternation          | `a\|b`  | Matches either `a` or `b`             |
| Kleene Star          | `a*`    | Matches zero or more `a`s             |
| Kleene Plus          | `a+`    | Matches one or more `a`s              |
| Optional             | `a?`    | Matches zero or one `a`               |
| Character Class      | `[ab]`  | Matches either `a` or `b`             |
| Negated Class        | `[^ab]` | Matches any character but `a` or `b`  |
| Character Range      | `[a-z]` | Matches any character from `a` to `z` |
| Grouping             | `(ab)`  | Matches the subexpression `ab`        |
