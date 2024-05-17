#include "ltre.h"

int main(void) {
  uint8_t input[] = "abaaaabaabaaaaa"; // match
  // uint8_t input[] = "abaaabaabaaaaa"; // no match
  // uint8_t input[] = "abaaabaabaaaaab"; // no match
  struct dfa dfa = ltre_compile("");
  dfa_dump(dfa);
  bool matches = ltre_matches(dfa, input);
  dfa_free(dfa);
  return !matches;
}
