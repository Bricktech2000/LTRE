#include <stdbool.h>
#include <stdint.h>

typedef uint8_t dfa_state_t; // maximum of 256 DFA states

// see `ltre_matches`
struct dfa {
  dfa_state_t (*transitions)[256]; // array
  bool *accepting;                 // array
  dfa_state_t size;
};

struct dfa ltre_compile(char *pattern);
bool ltre_matches(struct dfa dfa, uint8_t *input);
void dfa_dump(struct dfa dfa);
void dfa_free(struct dfa dfa);
