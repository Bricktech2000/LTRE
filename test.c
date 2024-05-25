#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>

void test(char *regex, char *input, bool errors, bool matches) {
  struct nstate *nfa = ltre_parse(&regex, NULL);
  if ((nfa == NULL) != errors)
    fprintf(stderr, "test failed: /%s/ parse\n", regex);

  if (nfa == NULL)
    return;

  struct dstate *dfa = ltre_compile(nfa);
  if (ltre_matches(dfa, (uint8_t *)input) != matches)
    fprintf(stderr, "test failed: /%s/ against '%s'\n", regex, input);

  nfa_free(nfa), dfa_free(dfa);
}

void match(char *input, char *regex) { test(input, regex, false, true); }
void nomatch(char *input, char *regex) { test(input, regex, false, false); }
void syntax(char *input, char *regex) { test(input, regex, true, false); }

int main(void) {
  // catastrophic backtracking
  nomatch("(a*)*c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  nomatch("(x+x+)+y", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

  // potential edge cases
  match("abba", "abba");
  match("[ab]+", "abba");
  nomatch("[ab]+", "abc");
  match(".*", "abba");
  match("\\x61\\+", "a+");
  match("", "");
  nomatch("[]", "");
  nomatch("", "\n");
  match("\\n", "\n");
  nomatch(".", "\n");
  match("^[]", "\n");
  nomatch("[\\-n]", "\n");
  match("(||n)(\\n)", "\n");
  match("\\r?\\n", "\n");
  match("\\r?\\n", "\r\n");
  match("(a+)+", "aa");
  match("(a*)*", "a");
  match("(a?)?", "");
  match("a+", "aa");
  nomatch("a?", "aa");
  match("(a+)?", "aa");
  match("(ba+)?", "baa");
  nomatch("(a+a+)+", "a");
  nomatch("a+", "");
  match("(a+|)+", "aa");
  match("(a+|)+", "");
  match("x+|", "xx");
  match("x+|", "");
  match("x*|", "xx");
  match("x*|", "");
  match("x?|", "x");
  match("x?|", "");
  nomatch("x*y*", "yx");
  nomatch("x+y+", "yx");
  nomatch("x?y?", "yx");
  nomatch("x+y*", "xyx");
  nomatch("x*y+", "yxy");

  // realistic regexes
  char *re = "\"(^[\\\\\"]|\\\\^[])*\"";
  nomatch(re, "foo");
  nomatch(re, "\"foo");
  nomatch(re, "foo \"bar\"");
  nomatch(re, "\"foo\\\"");
  nomatch(re, "\"\\\"");
  nomatch(re, "\"\"\"");
  match(re, "\"\"");
  match(re, "\"foo\"");
  match(re, "\"foo\\\"\"");
  match(re, "\"foo\\\\\"");
  match(re, "\"foo\\nbar\"");

  // parse errors
  syntax("abc)", "");
  syntax("(abc", "");
  syntax("abc]", "");
  syntax("[abc", "");
  syntax("[a?b]", "");
  syntax("[b-a]", "");
  syntax("[a-]", "");
  syntax("[--]", "");
  syntax("[-]", "");
  syntax("+a", "");
  syntax("a+*", "");
  syntax("a|*", "");
  syntax("\\x0", "");
  syntax("\\zzz", "");
  syntax("[a\\x]", "");
  syntax("\a", "");
  syntax("-", "");
  syntax("a-", "");
  syntax("^^a", "");

  // nonstandard syntax
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
  match("\\^", "^");
  nomatch("^\\^", "^");
  match("^[^\\^]", "^");
  match("^[ ^[a b c]]+", "abc");
  nomatch("^[ ^[a b c]]+", "a c");
  match("^[^0-74]+", "0123567");
  nomatch("^[^0-74]+", "89");
  nomatch("^[^0-74]+", "4");
}
