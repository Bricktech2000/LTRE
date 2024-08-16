#include <stdbool.h>
#include <stdint.h>

// `nstate`s support epsilon transitions and therefore we can assume NFAs have a
// unique final state without loss of generality
struct nfa {
  struct nstate *initial; // shall be included in `states`
  struct nstate *final;   // shall be included in `states`
  struct nstate *states;  // linked list of all NFA states
};

struct nfa ltre_parse(char **regex, char **error);
void ltre_partial(struct nfa *nfa);
void ltre_ignorecase(struct nfa *nfa);
struct dstate *ltre_compile(struct nfa nfa);
bool ltre_matches(struct dstate *dfa, uint8_t *input);
void nfa_free(struct nfa nfa);
void dfa_free(struct dstate *dfa);
