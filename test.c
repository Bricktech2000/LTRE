#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>

void test(char *regex, char *input, bool errors, bool matches) {
  struct nstate *nfa = ltre_parse(regex);
  if ((nfa == NULL) != errors)
    fprintf(stderr, "test failed: /%s/ parse\n", regex);

  if (nfa == NULL)
    return;

  struct dstate *dfa = ltre_compile_full(nfa);
  if (ltre_matches(dfa, (uint8_t *)input) != matches)
    fprintf(stderr, "test failed: /%s/ against '%s'\n", regex, input);

  free(nfa), free(dfa);
}

void match(char *input, char *regex) { test(input, regex, false, true); }
void nomatch(char *input, char *regex) { test(input, regex, false, false); }
void syntax(char *input, char *regex) { test(input, regex, true, false); }

int main(void) {
  nomatch("(a*)*c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  nomatch("(x+x+)+y", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

  match("abba", "abba");
  nomatch("\\x61*|b+a\\)", "abba");
  nomatch("a(ba|aaa)*(ab)?a*", "abba");
  nomatch("[bar]", "abba");
  match("[ab]+", "abba");
  nomatch("", "abba");
  match(".*", "abba");
  nomatch("[^abc]bba", "abba");
  match("((((a(((()b))b)))a))", "abba");

  match("", "");
  nomatch("[]", "");
  nomatch("", "\n");
  match("\\n", "\n");
  nomatch(".", "\n");
  match("[^]", "\n");
  nomatch("[-n]", "\n");
  match("(||n)(\\n)", "\n");
  match("\\r?\\n", "\n");
  match("\\r?\\n", "\r\n");
  match("[-]", "-");
  nomatch("[-]", "-c");
  match("[-a]+", "-a");
  nomatch("[-a]+", "-ac");
  match("[\\^]", "^");
  nomatch("[^\\^]", "^");

  char *re = "\"([^\\\\\"]|\\\\[^])*\"";
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

  syntax("abc)", "");
  syntax("(abc", "");
  syntax("abc]", "");
  syntax("[abc", "");
  syntax("[a?b]", "");
  syntax("[b-a]", "");
  syntax("[a-]", "");
  syntax("[--]", "");
  syntax("+a", "");
  syntax("a+*", "");
  syntax("a|*", "");
  syntax("\\x0", "");
  syntax("\\zzz", "");
  syntax("[a\\x]", "");
  syntax("\a", "");
}
