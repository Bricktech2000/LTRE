#include <stdbool.h>
#include <stdint.h>

// `nstate`s support epsilon transitions and therefore we can assume NFAs have
// a unique final state without loss of generality. `initial` and `final` also
// serve as the head and tail, respectively, of the `nstate.next` linked list,
// which serves as an iterator over all states of the NFA
struct nfa {
  struct nstate *initial;
  struct nstate *final;
};

struct nfa ltre_parse(char **regex, char **error);
void ltre_partial(struct nfa *nfa);
void ltre_ignorecase(struct nfa *nfa);
struct dstate *ltre_compile(struct nfa nfa);
bool ltre_matches(struct dstate *dfa, uint8_t *input);
void nfa_free(struct nfa nfa);
void dfa_free(struct dstate *dfa);
