<!-- keep in sync with ltrep/ltrep.1 -->

# Patterns

## Notes

Matching is exact by default; for partial matching, surround a pattern with `%`s. The metacharacter `%` is a shorthand for `.*`.

Whitespace is largely ignored; to match a whitespace character, use an escape like `\ ` or `\n`. All metacharacters, `\‑.~[]<>%{}*+?:|&=!( )`, must be escaped to be matched literally, even inside character classes.

Character ranges can appear outside character classes. For example, `0‑9+` matches a non-empty sequence of digits. Character classes are complemented by prefixing the opening bracket with `~`. For example, `~[abc]` matches any character except `a` or `b` or `c`. Single characters and character ranges can be complemented as well. For example, `~\n*` matches a sequence of non-newline characters and `[aeiou ~a‑z]` matches any character except lowercase consonants (insignificant whitespace for clarity).

To match “word” characters (alphanumeric characters plus the underscore), you might use `[\m_]`. The metacharacter `.` matches any character, including newlines; to match any character except newlines, you might use `~\n`. To match patters like ‘numbers _separated by_ commas’ you might use intercalation: `(\d+)*!,` (redundant parentheses for clarity). To match patterns like ‘identifiers _which are not_ keywords’ you might use intersection and complementation: `\a\m* & !(if|do|for)` (insignificant whitespace for clarity).

## Supported Syntax

| Name                    | Example    | Meaning                                                   | Type                         |
| ----------------------- | ---------- | --------------------------------------------------------- | ---------------------------- |
| Literal Character       | `a`        | Symbol consisting of the literal character `a`            | `symbol`                     |
| Metacharacter Escape    | `\+`       | Symbol consisting of the metacharacter `+`                | `symbol`                     |
| Simple Escape           | `\r`       | Symbol associated with the simple escape `\r` (see below) | `symbol`                     |
| Hexadecimal Escape      | `\x41`     | Symbol with character code `0x41`                         | `symbol`                     |
| Symbol Promotion        | any symbol | Singleton set containing the symbol                       | `symbol -> symset`           |
| Character Range         | `a‑z`      | Set of all characters from `a` to `z` inclusively         | `(symbol, symbol) -> symset` |
| Symset Wildcard         | `.`        | Set of all characters                                     | `symset`                     |
| Shorthand               | `\d`       | Set of characters in the shorthand `\d` (see below)       | `symset`                     |
| Symset Complement       | `~u`       | Set of all characters not in `u`                          | `symset -> symset`           |
| Symset Union            | `[uv]`     | Set of characters in `u` or in `v`                        | `[symset] -> symset`         |
| Symset Intersection     | `<uv>`     | Set of characters in `u` and in `v`                       | `[symset] -> symset`         |
| Symset Promotion        | any symset | Length-one words over the symset                          | `symset -> regex`            |
| Wildcard                | `%`        | All words                                                 | `regex`                      |
| Empty Repetition        | `r{}`      | Words some 0‑factoring of which consists of `r`s          | `regex -> regex`             |
| Exact Repetition        | `r{4}`     | Words some 4‑factoring of which consists of `r`s          | `regex -> regex`             |
| Bounded Repetition      | `r{3,5}`   | Words some 3–5‑factoring of which consists of `r`s        | `regex -> regex`             |
| Minimum Repetition      | `r{3,}`    | Words some ≥3‑factoring of which consists of `r`s         | `regex -> regex`             |
| Maximum Repetition      | `r{,5}`    | Words some ≤5‑factoring of which consists of `r`s         | `regex -> regex`             |
| Kleene Star             | `r*`       | Words some n‑factoring of which consists of `r`s          | `regex -> regex`             |
| Kleene Plus             | `r+`       | Words some ≥1‑factoring of which consists of `r`s         | `regex -> regex`             |
| Optional                | `r?`       | Words some ≤1‑factoring of which consists of `r`s         | `regex -> regex`             |
| Dual Empty Repetition   | `r:{}`     | Words all 0‑factorings of which contain some `r`          | `regex -> regex`             |
| Dual Exact Repetition   | `r:{4}`    | Words all 4‑factorings of which contain some `r`          | `regex -> regex`             |
| Dual Bounded Repetition | `r:{3,5}`  | Words all 3–5‑factorings of which contain some `r`        | `regex -> regex`             |
| Dual Minimum Repetition | `r:{3,}`   | Words all ≥3‑factorings of which contain some `r`         | `regex -> regex`             |
| Dual Maximum Repetition | `r:{,5}`   | Words all ≤5‑factorings of which contain some `r`         | `regex -> regex`             |
| Dual Kleene Star        | `r:*`      | Words all n‑factorings of which contain some `r`          | `regex -> regex`             |
| Dual Kleene Plus        | `r:+`      | Words all ≥1‑factorings of which contain some `r`         | `regex -> regex`             |
| Dual Optional           | `r:?`      | Words all ≤1‑factorings of which contain some `r`         | `regex -> regex`             |
| Intercalation           | `r*!s`     | Words in `r*` but with `s` inserted between each `r`      | `(regex, regex) -> regex`    |
| Dual Intercalation      | `r:*!s`    | Words in `r:*` but with `s` inserted between each `r`     | `(regex, regex) -> regex`    |
| Concatenation           | `rs`       | Words some 2‑factoring of which has head `r` and tail `s` | `(regex, regex) -> regex`    |
| Dual Concatenation      | `r:s`      | Words all 2‑factorings of which have head `r` or tail `s` | `(regex, regex) -> regex`    |
| Alternation             | `r\|s`     | Words in `r` or in `s`                                    | `(regex, regex) -> regex`    |
| Intersection            | `r&s`      | Words in `r` and in `s`                                   | `(regex, regex) -> regex`    |
| Biconditional           | `r=s`      | Words in `r` if and only if in `s`                        | `(regex, regex) -> regex`    |
| Complement              | `!r`       | Words not in `r`                                          | `regex -> regex`             |
| Grouping                | `(r)`      | Words in the subexpression `r`                            | `regex -> regex`             |

Literal characters work for any printable character that’s not a metacharacter. Metacharacter escapes work for any metacharacter, `\‑.~[]<>%{}*+?:|&=!( )`. Hexadecimal escapes take exactly two digits. Character ranges support wraparound; for example, `z‑a` means `~b‑y`. Intercalation and dual intercalation work for any quantifier, `{}*+?`. At most one complement may be applied per grouping level, and at most one symset complement may be applied per symset union/intersection level.

An _n‑factoring_ of a word is an n‑tuple of strings whose concatenation is that word. The empty word has a unique 0‑factoring, namely the 0‑tuple; no other word has 0‑factorings.

Insignificant whitespace can appear anywhere except within escapes, character ranges, shorthands, quantifiers, and intercalators.

Dual operators are dual with respect to complementation.

## Precedence

In decreasing order, and otherwise right associative:

- Escapes;
- Character ranges, shorthands;
- Symset complementation;
- Quantification, dual quantification;
- Intercalation, dual intercalation;
- Concatenation;
- Dual concatenation;
- Complementation;
- Alternation, intersection, biconditional.

## Simple Escapes

| Simple Escape | Definition | Character Name        |
| ------------- | ---------- | --------------------- |
| `\b`          | `\x08`     | BACKSPACE             |
| `\f`          | `\x0c`     | FORM FEED             |
| `\n`          | `\x0a`     | LINE FEED             |
| `\r`          | `\x0d`     | CARRIAGE RETURN       |
| `\t`          | `\x09`     | HORIZONTAL TABULATION |
| `\v`          | `\x0b`     | VERTICAL TABULATION   |
| `\e`          | `\x1b`     | ESCAPE                |

## Shorthands

| Shorthand   | Definition              | Equivalents         | libc Analogue    |
| ----------- | ----------------------- | ------------------- | ---------------- |
| `\m`, `~\M` | `[0‑9A‑Za‑z]`           | `[\d\a]`, `<\g\Q>`  | `isalnum` (C89)  |
| `\a`, `~\A` | `[A‑Za‑z]`              | `[\u\l]`, `<\m\D>`  | `isalpha` (C89)  |
| `\k`, `~\K` | `[\t\ ]`                | `<\s~\n‑\r>`        | `isblank` (C99)  |
| `\c`, `~\C` | `[\x00‑\x1f\x7f]`       | `<\z\P>`            | `iscntrl` (C89)  |
| `\d`, `~\D` | `0‑9`                   | `<\m\A>`            | `isdigit` (C89)  |
| `\g`, `~\G` | `\!‑\~`                 | `[\q\m]`, `<\p~\ >` | `isgraph` (C89)  |
| `\l`, `~\L` | `a‑z`                   | `<\a\U>`            | `islower` (C89)  |
| `\p`, `~\P` | `\ ‑\~`                 | `[\g\ ]`, `<\z\C>`  | `isprint` (C89)  |
| `\q`, `~\Q` | ``[\!‑/\:‑@\[‑`\{‑\~]`` | `<\g\M>`            | `ispunct` (C89)  |
| `\s`, `~\S` | `[\t‑\r\ ]`             | `[\k\n‑\r]`         | `isspace` (C89)  |
| `\u`, `~\U` | `A‑Z`                   | `<\a\L>`            | `isupper` (C89)  |
| `\h`, `~\H` | `[0‑9A‑Fa‑f]`           |                     | `isxdigit` (C89) |
| `\z`, `~\Z` | `\x00‑\x7f`             | `[\c\p]`            | `isascii` (BSD)  |
