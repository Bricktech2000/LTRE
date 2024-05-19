#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  // should not explode
  uint8_t input[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  struct dstate *dfa = ltre_compile("(a*)*c");

  // uint8_t input[] = "abba";
  // struct dstate *dfa = ltre_compile("abba");
  // struct dstate *dfa = ltre_compile("\\x61*|b+a\\)");
  // struct dstate *dfa = ltre_compile("a|a|a|a|b(asdf[cls]aoeu)aoeu");
  // struct dstate *dfa = ltre_compile("a(ba|aaa)*(ab)?a*");
  // struct dstate *dfa = ltre_compile("[aoeu]");
  // struct dstate *dfa = ltre_compile("");
  // struct dstate *dfa = ltre_compile(".*");
  // struct dstate *dfa = ltre_compile("[^abc]bba");

  // uint8_t input[] = "\n";
  // struct dstate *dfa = ltre_compile("\n"); // should error
  // struct dstate *dfa = ltre_compile("\\n"); // should match
  // struct dstate *dfa = ltre_compile("."); // should not match
  // struct dstate *dfa = ltre_compile("[^]"); // should match
  // struct dstate *dfa = ltre_compile("[-n]"); // should not match
  // struct dstate *dfa = ltre_compile("(||n)\\n"); // should match

  // uint8_t input[] = "\n"; // should match
  // uint8_t input[] = "\r\n"; // should match
  // struct dstate *dfa = ltre_compile("\\r?\\n");

  // should error
  // uint8_t input[] = "";
  // struct dstate *dfa = ltre_compile("abc)");
  // struct dstate *dfa = ltre_compile("(abc");
  // struct dstate *dfa = ltre_compile("abc]");
  // struct dstate *dfa = ltre_compile("[abc");
  // struct dstate *dfa = ltre_compile("[a?b]");
  // struct dstate *dfa = ltre_compile("[b-a]");
  // struct dstate *dfa = ltre_compile("[a-]");
  // struct dstate *dfa = ltre_compile("+a");
  // struct dstate *dfa = ltre_compile("a+*");
  // struct dstate *dfa = ltre_compile("a|*");
  // struct dstate *dfa = ltre_compile("\\x0");
  // struct dstate *dfa = ltre_compile("\\zzz");
  // struct dstate *dfa = ltre_compile("[a\\x]");
  // struct dstate *dfa = ltre_compile("\a");

  dfa_dump(dfa);
  bool matches = ltre_matches(dfa, input);
  dfa_free(dfa);

  fputs(matches ? "match\n" : "no match\n", stderr);
  return EXIT_SUCCESS;
}
