#include <stdbool.h>
#include <stdint.h>

typedef int dfa_state_t;

struct dfa {
  dfa_state_t (*transitions)[256]; // array
  bool *accepting;                 // array
  dfa_state_t size;
};

struct dfa ltre_compile(char *pattern);
bool ltre_matches(struct dfa dfa, uint8_t *input);
void dfa_free(struct dfa dfa);
