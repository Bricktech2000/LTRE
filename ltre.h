#include <stdbool.h>
#include <stdint.h>

struct dstate *ltre_compile(char *pattern);
bool ltre_matches(struct dstate *dfa, uint8_t *input);
void dfa_dump(struct dstate *dfa);
void dfa_free(struct dstate *dfa);
