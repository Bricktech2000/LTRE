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
  nomatch("(a+a+)+", "a");
  nomatch("a+", "");
  match("(a+|)+", "aa");
  match("(a+|)+", "");
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
  nomatch("a{1,2}", "");
  match("a{1,2}", "a");
  match("a{1,2}", "aa");
  nomatch("a{1,2}", "aaa");
  nomatch("a{1,}", "");
  match("a{1,}", "a");
  match("a{1,}", "aa");
  match("a{1,}", "aaa");
  match("a{0,2}", "");
  match("a{0,2}", "a");
  match("a{0,2}", "aa");
  nomatch("a{0,2}", "aaa");
  nomatch("a{2}", "a");
  match("a{2}", "aa");
  nomatch("a{2}", "aaa");
  match("a{0}", "");
  nomatch("a{0}", "a");

  // realistic regexes
  char *re = "\"(^[\\\\\"]|\\\\<>)*\"";
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
  syntax("abc]", "");
  syntax("[abc", "");
  syntax("abc>", "");
  syntax("<abc", "");
  syntax("abc)", "");
  syntax("(abc", "");
  syntax("[a?b]", "");
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
  syntax("a**", "");
  syntax("a*+", "");
  syntax("a*?", "");
  syntax("a*{}", "");
  syntax("a+*", "");
  syntax("a++", "");
  syntax("a+?", "");
  syntax("a+{}", "");
  syntax("a?*", "");
  syntax("a?+", "");
  syntax("a??", "");
  syntax("a?{}", "");
  syntax("a{}*", "");
  syntax("a{}+", "");
  syntax("a{}?", "");
  syntax("a{}{}", "");
  syntax("a{2,1}", "");
  syntax("a{1 2}", "");
  syntax("a{1, 2}", "");

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
}
