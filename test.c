#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>

void test(char *regex, char *input, bool partial, bool ignorecase, bool errors,
          bool matches) {
  char *error = NULL, *loc = regex;
  struct nfa nfa = ltre_parse(&loc, &error);

  if (!!error != errors)
    fprintf(stderr, "test failed: /%s/ parse\n", regex);
  // if (error)
  //   fprintf(stderr, "note: /%s/ %s near '%.16s'\n", regex, error, loc);

  if (error)
    return;

  if (partial)
    ltre_partial(&nfa);
  if (ignorecase)
    ltre_ignorecase(&nfa);

  struct dstate *dfa = ltre_compile(nfa);
  if (ltre_matches(dfa, (uint8_t *)input) != matches)
    fprintf(stderr, "test failed: /%s/ against '%s'\n", regex, input);

  nfa_free(nfa), dfa_free(dfa);
}

#define error(input, regex) test(input, regex, false, false, true, false);
#define match(input, regex) test(input, regex, false, false, false, true);
#define nomatch(input, regex) test(input, regex, false, false, false, false);
#define pmatch(input, regex) test(input, regex, true, false, false, true);
#define pnomatch(input, regex) test(input, regex, true, false, false, false);
#define imatch(input, regex) test(input, regex, false, true, false, true);
#define inomatch(input, regex) test(input, regex, false, true, false, false);

int main(void) {
  // catastrophic backtracking
  nomatch("(a*)*c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  nomatch("(x+x+)+y", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

  // exponential blowout
  match("[01]*1[01]{8}", "11011100011100");
  nomatch("[01]*1[01]{8}", "01010010010010");

  // potential edge cases
  match("abba", "abba");
  match("[ab]+", "abba");
  nomatch("[ab]+", "abc");
  match(".*", "abba");
  match("(a|b+){3}", "abbba");
  nomatch("(a|b+){3}", "abbab");
  match("\\x61\\+", "a+");
  match("", "");
  nomatch("[]", "");
  match("[]*", "");
  nomatch("[]+", "");
  match("[]?", "");
  match("()", "");
  match("()*", "");
  match("()+", "");
  match("()?", "");
  match(" ", " ");
  nomatch("", "\n");
  match("\\n", "\n");
  nomatch(".", "\n");
  nomatch("\\\\n", "\n");
  match("(|n)(\\n)", "\n");
  match("\\r?\\n", "\n");
  match("\\r?\\n", "\r\n");
  match("(a*)*", "a");
  match("(a+)+", "aa");
  match("(a?)?", "");
  match("a+", "aa");
  nomatch("a?", "aa");
  match("(a+)?", "aa");
  match("(ba+)?", "baa");
  nomatch("(ab+)?", "b");
  nomatch("(a+b)?", "a");
  nomatch("(a+a+)+", "a");
  nomatch("a+", "");
  match("(a+|)+", "aa");
  match("(a+|)+", "");
  match("(a|b)?", "a");
  match("(a|b)?", "b");
  match("x*|", "xx");
  match("x*|", "");
  match("x+|", "xx");
  match("x+|", "");
  match("x?|", "x");
  match("x?|", "");
  nomatch("x*y*", "yx");
  nomatch("x+y+", "yx");
  nomatch("x?y?", "yx");
  nomatch("x+y*", "xyx");
  nomatch("x*y+", "yxy");
  nomatch("x*|y*", "xy");
  nomatch("x+|y+", "xy");
  nomatch("x?|y?", "xy");
  nomatch("x+|y*", "xy");
  nomatch("x*|y+", "xy");
  nomatch("a{1,2}", "");
  match("a{1,2}", "a");
  match("a{1,2}", "aa");
  nomatch("a{1,2}", "aaa");
  match("a{0,}", "");
  match("a{0,}", "a");
  match("a{0,}", "aa");
  match("a{0,}", "aaa");
  nomatch("a{1,}", "");
  match("a{1,}", "a");
  match("a{1,}", "aa");
  match("a{1,}", "aaa");
  nomatch("a{3,}", "aa");
  match("a{3,}", "aaa");
  match("a{3,}", "aaaa");
  match("a{3,}", "aaaaa");
  match("a{0,2}", "");
  match("a{0,2}", "a");
  match("a{0,2}", "aa");
  nomatch("a{0,2}", "aaa");
  nomatch("a{2}", "a");
  match("a{2}", "aa");
  nomatch("a{2}", "aaa");
  match("a{0}", "");
  nomatch("a{0}", "a");

  // partial and ignorecase
  pmatch("", "");
  pmatch("", "abc");
  pmatch("b", "abc");
  pnomatch("ba", "abc");
  pmatch("abc", "abc");
  pnomatch("[]", "");
  imatch("", "");
  imatch("abCdEF", "aBCdEf");
  inomatch("ab", "abc");

  // parse errors
  error("abc]", "");
  error("[abc", "");
  error("abc)", "");
  error("(abc", "");
  error("+a", "");
  error("a|*", "");
  error("\\x0", "");
  error("\\zzz", "");
  error("[a\\x]", "");
  error("\a", "");
  error("\n", "");
  error("^^a", "");
  error("a**", "");
  error("a*+", "");
  error("a*?", "");
  error("a+*", "");
  error("a++", "");
  error("a+?", "");
  error("a?*", "");
  error("a?+", "");
  error("a??", "");

  // nonstandard features
  match("^a", "z");
  nomatch("^a", "a");
  match("^\\n", "\r");
  nomatch("^\\n", "\n");
  match("^.", "\n");
  nomatch("^.", "a");
  match("\\d+", "0123456789");
  match("\\s+", " \f\n\r\t\v");
  match("\\w+", "azAZ09_");
  match("^a-z*", "1A!2$B");
  nomatch("^a-z*", "1aA");
  match("a-z*", "abc");
  match("^[\\d^\\w]+", "abcABC");
  nomatch("^[\\d^\\w]+", "abc123");
  match("^[\\d\\W]+", "abcABC");
  nomatch("^[\\d^\\W]+", "abc123");
  match("[[abc]]+", "abc");
  match("[a[bc]]+", "abc");
  match("[a[b]c]+", "abc");
  match("[a][b][c]", "abc");
  nomatch("^[^a^b]", "a");
  nomatch("^[^a^b]", "b");
  nomatch("^[^a^b]", "");
  nomatch("<ab>", "a");
  nomatch("<ab>", "b");
  nomatch("<ab>", "");
  match("\\^", "^");
  nomatch("^\\^", "^");
  match("^[^\\^]", "^");
  match("^[ ^[a b c]]+", "abc");
  nomatch("^[ ^[a b c]]+", "a c");
  match("<[a b c]^ >+", "abc");
  nomatch("<[a b c]^ >+", "a c");
  match("^[^0-74]+", "0123567");
  nomatch("^[^0-74]+", "89");
  nomatch("^[^0-74]+", "4");
  match("<0-7^4>+", "0123567");
  nomatch("<0-7^4>+", "89");
  nomatch("<0-7^4>+", "4");
  nomatch("[]", " ");
  match("^[]", " ");
  match("<>", " ");
  nomatch("^<>", " ");
  match("9-0*", "abc");
  nomatch("9-0*", "18");
  match("9-0*", "09");
  match("9-0*", "/:");
  match("b-a*", "ab");
  match("a-b*", "ab");
  nomatch("a-a*", "ab");
  match("a-a*", "aa");
  match("a{,2}", "");
  match("a{,2}", "a");
  match("a{,2}", "aa");
  nomatch("a{,2}", "aaa");
  match("a{}", "");
  nomatch("a{}", "a");
  match("a{,}", "");
  match("a{,}", "a");
  error("abc>", "");
  error("<abc", "");
  error("[a?b]", "");
  error("[a-]", "");
  error("[--]", "");
  error("[-]", "");
  error("-", "");
  error("a-", "");
  error("a*{}", "");
  error("a+{}", "");
  error("a?{}", "");
  error("a{}*", "");
  error("a{}+", "");
  error("a{}?", "");
  error("a{}{}", "");
  error("a{2,1}", "");
  error("a{1 2}", "");
  error("a{1, 2}", "");
  error("a{a}", "");

  // realistic regexes
#define STR_LIT "\"(^[\\\\\"]|\\\\<>)*\""
  nomatch(STR_LIT, "foo");
  nomatch(STR_LIT, "\"foo");
  nomatch(STR_LIT, "foo \"bar\"");
  nomatch(STR_LIT, "\"foo\\\"");
  nomatch(STR_LIT, "\"\\\"");
  nomatch(STR_LIT, "\"\"\"");
  match(STR_LIT, "\"\"");
  match(STR_LIT, "\"foo\"");
  match(STR_LIT, "\"foo\\\"\"");
  match(STR_LIT, "\"foo\\\\\"");
  match(STR_LIT, "\"foo\\nbar\"");
  // ISO/IEC 9899:TC3 $7.19.6.1 'The fprintf function'
#define FIELD_WIDTH "(\\*|1-90-9*)?"
#define PRECISION "(\\.|\\.\\*|\\.1-90-9*)?"
#define DIU "[\\-\\+ 0]*" FIELD_WIDTH PRECISION "([hljzt]|hh|ll)?[diu]"
#define OX "[\\-\\+ #0]*" FIELD_WIDTH PRECISION "([hljzt]|hh|ll)?[oxX]"
#define FEGA "[\\-\\+ #0]*" FIELD_WIDTH PRECISION "[lL]?[fFeEgGaA]"
#define C "[\\-\\+ ]*" FIELD_WIDTH "l?c"
#define S "[\\-\\+ ]*" FIELD_WIDTH PRECISION "l?s"
#define P "[\\-\\+ ]*" FIELD_WIDTH "p"
#define N "[\\-\\+ ]*" FIELD_WIDTH "([hljzt]|hh|ll)?n"
#define CONV_SPEC "%(" DIU "|" OX "|" FEGA "|" C "|" S "|" P "|" N "|%)"
#define FORMAT "(^%|" CONV_SPEC ")*"
  nomatch(CONV_SPEC, "%");
  nomatch(CONV_SPEC, "%*");
  match(CONV_SPEC, "%%");
  nomatch(FORMAT, "%");
  nomatch(FORMAT, "%*");
  match(FORMAT, "%%");
  nomatch(CONV_SPEC, "%5%");
  nomatch(FORMAT, "%5%");
  match(CONV_SPEC, "%p");
  match(CONV_SPEC, "%*p");
  match(CONV_SPEC, "% *p");
  match(CONV_SPEC, "%5p");
  nomatch(CONV_SPEC, "d");
  match(CONV_SPEC, "%d");
  match(CONV_SPEC, "%.16s");
  match(CONV_SPEC, "% 5.3f");
  nomatch(CONV_SPEC, "%*32.4g");
  match(CONV_SPEC, "%-#65.4g");
  nomatch(CONV_SPEC, "%03c");
  match(CONV_SPEC, "%06i");
  match(CONV_SPEC, "%lu");
  match(CONV_SPEC, "%hhu");
  nomatch(CONV_SPEC, "%Lu");
  match(CONV_SPEC, "%-*p");
  nomatch(CONV_SPEC, "%-.*p");
  nomatch(CONV_SPEC, "%id");
  nomatch(CONV_SPEC, "%%d");
  nomatch(CONV_SPEC, "i%d");
  nomatch(CONV_SPEC, "%c%s");
  match(FORMAT, "%id");
  match(FORMAT, "%%d");
  match(FORMAT, "i%d");
  match(FORMAT, "%c%s");
  match(CONV_SPEC, "%0-++ #0x");
  match(CONV_SPEC, "%30c");
  nomatch(CONV_SPEC, "%03c");
  match(FORMAT, "%u + %d");
  match(FORMAT, "%d:");
}
