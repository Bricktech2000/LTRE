#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  // shouldn't explode
  uint8_t input[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  struct dfa dfa = ltre_compile("(a*)*c");

  // uint8_t input[] = "abba";
  // struct dfa dfa = ltre_compile("\\x61*|b+a\\)");
  // struct dfa dfa = ltre_compile("a|a|a|a|b(asdf[cls]aoeu)aoeu");
  // struct dfa dfa = ltre_compile("a(ba|aaa)*(ab)?a*");
  // struct dfa dfa = ltre_compile("[aoeu]");
  // struct dfa dfa = ltre_compile("");
  // struct dfa dfa = ltre_compile(".*");
  // struct dfa dfa = ltre_compile("[^abc]bba");

  // uint8_t input[] = "\n";
  // struct dfa dfa = ltre_compile("\n"); // should error
  // struct dfa dfa = ltre_compile("\\n"); // should match
  // struct dfa dfa = ltre_compile("."); // shouldn't match
  // struct dfa dfa = ltre_compile("[^]"); // should match
  // struct dfa dfa = ltre_compile("[-n]"); // shouldn't match
  // struct dfa dfa = ltre_compile("(||n)\\n"); // should match

  // uint8_t input[] = "\n"; // should match
  // uint8_t input[] = "\r\n"; // should match
  // struct dfa dfa = ltre_compile("\\r?\\n");

  // should error
  // uint8_t input[] = "";
  // struct dfa dfa = ltre_compile("abc)");
  // struct dfa dfa = ltre_compile("(abc");
  // struct dfa dfa = ltre_compile("abc]");
  // struct dfa dfa = ltre_compile("[abc");
  // struct dfa dfa = ltre_compile("[a?b]");
  // struct dfa dfa = ltre_compile("[b-a]");
  // struct dfa dfa = ltre_compile("+a");
  // struct dfa dfa = ltre_compile("a+*");
  // struct dfa dfa = ltre_compile("a|*");
  // struct dfa dfa = ltre_compile("\\x0");
  // struct dfa dfa = ltre_compile("\\zzz");
  // struct dfa dfa = ltre_compile("[a\\x]");
  // struct dfa dfa = ltre_compile("\a");

  dfa_dump(dfa);
  bool matches = ltre_matches(dfa, input);
  dfa_free(dfa);

  fputs(matches ? "match\n" : "no match\n", stderr);
  return EXIT_SUCCESS;
}
