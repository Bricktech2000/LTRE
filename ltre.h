#include <stdbool.h>
#include <stdint.h>

struct nstate *ltre_parse(char **regex, char **error);
struct dstate *ltre_compile_part(struct nstate *nfa);
struct dstate *ltre_compile_full(struct nstate *nfa);
bool ltre_matches(struct dstate *dfa, uint8_t *input);
void nfa_free(struct nstate *nfa);
void dfa_free(struct dstate *dfa);
