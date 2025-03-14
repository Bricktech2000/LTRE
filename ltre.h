#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct dstate;

// `nstate`s support epsilon-transitions and therefore we can assume NFAs have
// a unique final state without loss of generality. `initial` and `final` also
// serve as the head and tail, respectively, of the `nstate.next` linked list,
// which serves as an iterator over all states of the NFA
struct nfa {
  struct nstate *initial, *final;
  // NFA complementation and reversal are performed lazily by flipping these
  // flags, often saving us the trip through the compile pipeline. when this NFA
  // is eventually compiled into a DFA by `ltre_compile`, they will be read by
  // `dfa_step` when stepping through the NFA and when marking accepting states
  bool complemented, reversed;
};

void nfa_free(struct nfa nfa);
void dfa_free(struct dstate *dfa);
uint8_t *dfa_serialize(struct dstate *dfa, size_t *size);
struct dstate *dfa_deserialize(uint8_t *buf, size_t *size);
struct nfa ltre_parse(char **regex, char **error);
struct nfa ltre_fixed_string(char *string);
void ltre_partial(struct nfa *nfa);
void ltre_ignorecase(struct nfa *nfa);
void ltre_complement(struct nfa *nfa);
void ltre_reverse(struct nfa *nfa);
struct dstate *ltre_compile(struct nfa nfa);
struct nfa ltre_uncompile(struct dstate *dfa);
char *ltre_decompile(struct dstate *dfa);
bool ltre_matches(struct dstate *dfa, uint8_t *input);
bool ltre_matches_lazy(struct dstate **dfap, struct nfa nfa, uint8_t *input);
bool ltre_equivalent(struct dstate *dfa1, struct dstate *dfa2);
