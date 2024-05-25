#include <stdbool.h>
#include <stdint.h>

struct nstate *ltre_parse(char **regex, char **error);
void ltre_partial(struct nstate *nfa);
void ltre_ignorecase(struct nstate *nfa);
struct dstate *ltre_compile(struct nstate *nfa);
bool ltre_matches(struct dstate *dfa, uint8_t *input);
void nfa_free(struct nstate *nfa);
void dfa_free(struct dstate *dfa);
