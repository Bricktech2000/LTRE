#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t symset_t[256 / 8];
// on x86 and x86-64, `unsigned` saves us a `cmovs` over `int` in `chr / 8`
inline bool symset_read(symset_t symset, unsigned chr) {
  return symset[chr / 8] & 1 << chr % 8;
}
inline void symset_write(symset_t symset, unsigned chr, bool val) {
  symset[chr / 8] = symset[chr / 8] & ~(1 << chr % 8) | val << chr % 8;
}
char *symset_fmt(symset_t symset);

struct regex *regex_incref(struct regex *regex);
struct regex *regex_decref(struct regex *regex);
unsigned regex_size(struct regex *regex);
int regex_cmp(struct regex *regex1, struct regex *regex2);

#define REGEXES(...) ((struct regex *[]){__VA_ARGS__, NULL})
struct regex *regex_alt(struct regex *children[]);
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

struct dstate *dstate_alloc(struct regex *regex);
void dfa_free(struct dstate *dfa);
int dfa_get_size(struct dstate *dfa);
uint8_t *dfa_serialize(struct dstate *dfa, size_t *size);
struct dstate *dfa_deserialize(uint8_t *image, size_t *size);

void dfa_mark(struct dstate *dfa);
void dfa_minimize(struct dstate *dfa);
bool dfa_equivalent(struct dstate *dfa1, struct dstate *dfa2);

struct regex *ltre_parse(char **pattern, char **error);
struct regex *ltre_fixed_string(char *string);
char *ltre_stringify(struct regex *regex);

bool ltre_matches_lazy(struct dstate **dfap, uint8_t *input);
struct dstate *ltre_compile(struct regex *regex);
struct dstate *ltre_determinize(struct regex *regex);
bool ltre_matches(struct dstate *dfa, uint8_t *input);
struct regex *ltre_decompile(struct dstate *dfa);
