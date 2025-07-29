#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct regex *ltre_parse(char **pattern, char **error);
struct regex *ltre_fixed_string(char *string);
char *ltre_stringify(struct regex *regex);

struct dstate *ltre_compile(struct regex *regex);
struct dstate *ltre_determinize(struct regex *regex);
bool ltre_matches(struct dstate *dfa, uint8_t *input);
struct regex *ltre_decompile(struct dstate *dfa);

struct dstate *dstate_alloc(struct regex *regex);
bool ltre_matches_lazy(struct dstate **dfap, uint8_t *input);

typedef uint8_t symset_t[256 / 8];

void dfa_free(struct dstate *dfa);
int dfa_get_size(struct dstate *dfa);
uint8_t *dfa_serialize(struct dstate *dfa, size_t *size);
struct dstate *dfa_deserialize(uint8_t *image, size_t *size);

void dfa_optimize(struct dstate *dfa);
void dfa_minimize(struct dstate *dfa);
bool dfa_equivalent(struct dstate *dfa1, struct dstate *dfa2);

struct regex *regex_incref(struct regex *regex);
struct regex *regex_decref(struct regex *regex);
int regex_cmp(struct regex *regex1, struct regex *regex2);

struct regex *regex_alt(struct regex *children[]);
struct regex *regex_int(struct regex *children[]);
struct regex *regex_compl(struct regex *child);
struct regex *regex_concat(struct regex *children[]);
struct regex *regex_repeat(struct regex *child, unsigned lower, unsigned upper);
struct regex *regex_symset(symset_t *symset);
struct regex *regex_empty(void);
struct regex *regex_univ(void);
struct regex *regex_eps(void);
struct regex *regex_negeps(void);

struct regex *regex_ignorecase(struct regex *regex, bool dual);
struct regex *regex_reverse(struct regex *regex);
struct regex *regex_differentiate(struct regex *regex, uint8_t chr);

#define REGEXES(...) ((struct regex *[]){__VA_ARGS__, NULL})
