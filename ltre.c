#include "ltre.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\.^$*+?[]()|" // for parser

// as a DFA, `*dstate` is the initial state
struct dstate {
  struct dstate *transitions[256];
  struct dstate *next;  // to keep track of all states
  struct dstate *qnext; // to form linked queue during powerset construction
  bool accepting;
  uint8_t bitset[]; // powerset representation during powerset construction
};

// as an NFA, `*nstate` is the initial state and `nstate->next` is the final
// state. we support epsilon transitions and therefore can assume NFAs have a
// unique final state without loss of generality
struct nstate {
  struct nstate *transitions[256]; // linked list formed by `lnext`
  struct nstate *epsilon;          // linked list formed by `lnext`
  struct nstate *next;             // to keep track of all states
  struct nstate *lnext; // to form linked list for `transitions` and `epsilon`
  int id;               // initialized during powerset construction
};

struct nstate *nstate_alloc() {
  struct nstate *nstate = malloc(sizeof(struct nstate));
  memset(nstate->transitions, 0, sizeof(nstate->transitions));
  nstate->epsilon = NULL;
  nstate->next = NULL;
  nstate->lnext = NULL;
  nstate->id = -1;
  return nstate;
}

void nfa_free(struct nstate *nstate) {
  while (nstate) {
    struct nstate *next = nstate->next;
    free(nstate);
    nstate = next;
  }
}

void nstate_lpush(struct nstate **nstatep, struct nstate *nstate) {
  // pushes to the beginning, with respect to `lnext`
  assert(nstate->lnext == NULL); // assert not already part of a list
  assert(nstate != *nstatep);    // assert not a self loop
  nstate->lnext = *nstatep;
  *nstatep = nstate;
}

void nstate_join(struct nstate **nstatep, struct nstate *nstate) {
  // appends to the end, with respect to `next`
  while (*nstatep)
    nstatep = &(*nstatep)->next;
  *nstatep = nstate;
}

struct dstate *dstate_alloc(int bitset_size) {
  struct dstate *dstate = malloc(sizeof(struct dstate) + bitset_size);
  memset(dstate->transitions, 0, sizeof(dstate->transitions));
  dstate->next = NULL;
  dstate->qnext = NULL;
  dstate->accepting = false;
  memset(dstate->bitset, 0, bitset_size);
  return dstate;
}

void dfa_free(struct dstate *dstate) {
  while (dstate) {
    struct dstate *next = dstate->next;
    free(dstate);
    dstate = next;
  }
}

bool bitset_get(uint8_t bitset[], int id) {
  return bitset[id / 8] & 1 << id % 8;
}

void bitset_set(uint8_t bitset[], int id) { bitset[id / 8] |= 1 << id % 8; }

struct dstate *dfa_random(int dfa_size) {
  struct dstate *dfas[dfa_size];
  for (int id = 0; id < dfa_size; id++)
    dfas[id] = dstate_alloc(0);

  struct dstate *dfa = NULL;
  for (int id = dfa_size; id--;) {
    dfas[id]->next = dfa;
    dfa = dfas[id];
    dfa->accepting = rand() % 2;
    for (int chr = 0; chr < 256; chr++)
      dfa->transitions[chr] = dfas[rand() % dfa_size];
  }

  return dfas[0];
}

void dfa_dump(struct dstate *dfa) {
  printf("graph LR\n");
  printf("  I( ) --> %u\n", 0);

  int id1 = 0;
  for (struct dstate *dfa1 = dfa; dfa1; dfa1 = dfa1->next, id1++) {
    if (dfa1->accepting)
      printf("  %u --> F( )\n", id1);

    int id2 = 0;
    for (struct dstate *dfa2 = dfa; dfa2; dfa2 = dfa2->next, id2++) {
      char buf[256 + 1];
      char *bufp = buf;
      int first, last = '\0';

      for (int chr = ' '; chr <= '~'; chr++) {
        // avoid breaking Mermaid
        if (chr == ' ' || chr == '"' || chr == '-')
          continue;

        // print ranges
        if (dfa1->transitions[chr] == dfa2) {
          if (!last)
            *bufp++ = first = chr;
          last = chr;
        } else if (last) {
        close_off:
          if (last - first >= 2)
            *bufp++ = '-';
          if (last - first >= 1)
            *bufp++ = last;
          last = '\0';
        }
      }

      if (last)
        goto close_off;
      *bufp = '\0';

      // don't print empty transitions
      if (buf == bufp)
        continue;

      // spaces around `%s` for 'x', 'o', '<', '>'
      printf("  %u -- %s --> %u\n", id1, buf, id2);
    }
  }
}

// some invariants for parsers on `error`:
// - the NFA returned shall be `NULL`
// - `regex` shall point to the error location
// - the caller may backtrack if necessary

uint8_t parse_literal(char **regex, char **error);
struct nstate *parse_class(char **regex, char **error) {
  struct nstate *lits = nstate_alloc();
  lits->next = nstate_alloc();

  bool invert = false;
  if (**regex == '^') {
    ++*regex;
    invert = true;
  }

  uint8_t bitset[256 / 8] = {0};
  char *last_regex = *regex;
  while (1) {
    last_regex = *regex;
    uint8_t begin = parse_literal(regex, error);
    if (*error)
      break;

    uint8_t end = begin;
    if (**regex == '-') {
      ++*regex;
      end = parse_literal(regex, error);
      if (!*error && begin > end) {
        *regex = last_regex;
        *error = "invalid character range";
      }
      if (*error) {
        nfa_free(lits);
        return NULL;
      }
    }

    for (int chr = begin; chr <= end; chr++)
      bitset_set(bitset, chr);
  }
  if (strchr("]", *last_regex)) { // hacky lookahead for better diagnostics
    // backtrack
    *regex = last_regex;
    *error = NULL;
  }
  if (*error) {
    nfa_free(lits);
    return NULL;
  }

  for (int chr = 0; chr < 256; chr++)
    if (bitset_get(bitset, chr) ^ invert)
      nstate_lpush(&lits->transitions[chr], lits->next);

  return lits;
}

uint8_t parse_hexbyte(char **regex, char **error) {
  uint8_t byte = 0;
  for (int i = 0; i < 2; i++, byte <<= 4) {
    uint8_t chr = **regex;
    if (chr >= '0' && chr <= '9')
      byte |= chr - '0';
    else if (chr >= 'a' && chr <= 'f')
      byte |= chr - 'a' + 10;
    else if (chr >= 'A' && chr <= 'F')
      byte |= chr - 'A' + 10;
    else {
      *error = "expected hex digit";
      return 0;
    }
    ++*regex;
  }
  return byte;
}

uint8_t parse_escaped(char **regex, char **error) {
  if (strchr(METACHARS, **regex))
    return *(*regex)++;

  switch (*(*regex)++) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  case 'x':;
    uint8_t chr = parse_hexbyte(regex, error);
    if (*error)
      return 0;
    return chr;
  }

  --*regex;
  *error = "unknown escape sequence";
  return 0;
}

uint8_t parse_literal(char **regex, char **error) {
  if (**regex == '\\') {
    ++*regex;
    uint8_t escaped = parse_escaped(regex, error);
    if (*error)
      return 0;

    return escaped;
  }

  if (**regex == '\0') {
    *error = "expected literal";
    return 0;
  }

  if (strchr(METACHARS, **regex)) {
    *error = "unexpected metacharacter";
    return 0;
  }

  if (**regex >= ' ' && **regex <= '~')
    return *(*regex)++;

  *error = "invalid character";
  return 0;
}

struct nstate *parse_regex(char **regex, char **error);
struct nstate *parse_atom(char **regex, char **error) {
  if (**regex == '(') {
    ++*regex;
    struct nstate *sub = parse_regex(regex, error);
    if (*error)
      return NULL;

    if (**regex != ')') {
      *error = "expected ')'";
      nfa_free(sub);
      return NULL;
    }

    ++*regex;
    return sub;
  }

  if (**regex == '[') {
    ++*regex;
    struct nstate *class = parse_class(regex, error);
    if (*error)
      return NULL;

    if (**regex != ']') {
      *error = "expected ']'";
      nfa_free(class);
      return NULL;
    }

    ++*regex;
    return class;
  }

  if (**regex == '.') {
    ++*regex;
    struct nstate *nfa = nstate_alloc();
    nfa->next = nstate_alloc();
    // use `[^]` to match any character
    for (int chr = 0; chr < 256; chr++)
      if (chr != '\n')
        nstate_lpush(&nfa->transitions[chr], nfa->next);
    return nfa;
  }

  // TODO implement `^` and `$`

  uint8_t literal = parse_literal(regex, error);
  if (*error)
    return NULL;

  struct nstate *nfa = nstate_alloc();
  nfa->next = nstate_alloc();
  nstate_lpush(&nfa->transitions[literal], nfa->next);

  return nfa;
}

struct nstate *parse_term(char **regex, char **error) {
  struct nstate *atom = parse_atom(regex, error);
  if (*error)
    return NULL;

  if (**regex == '*') {
    ++*regex;
    nstate_lpush(&atom->next->epsilon, atom);
    nstate_lpush(&atom->epsilon, atom->next);
  }

  if (**regex == '?') {
    ++*regex;
    nstate_lpush(&atom->epsilon, atom->next);
  }

  if (**regex == '+') {
    ++*regex;
    nstate_lpush(&atom->next->epsilon, atom);
  }

  struct nstate *nfa = nstate_alloc();
  nfa->next = nstate_alloc();
  nstate_lpush(&nfa->epsilon, atom);
  nstate_lpush(&atom->next->epsilon, nfa->next);
  nstate_join(&nfa, atom);

  return nfa;
}

struct nstate *parse_regex(char **regex, char **error) {
  struct nstate *terms = nstate_alloc();
  terms->next = nstate_alloc();
  nstate_lpush(&terms->epsilon, terms->next);

  char *last_regex = *regex;
  while (1) {
    last_regex = *regex;
    struct nstate *term = parse_term(regex, error);

    // introduce a dummy node to handle concatenation. this ensures any `<term>`
    // within a `<regex>` is up surrounded by a pair of epsilon transitions to
    // and from states **which have no `lnext`**. the quantifier parsers assume
    // this invariant. these gymnastics are necessary because `nstate->epsilon`
    // is a linked list of `nstate`s, not of `nstate` pointers
    struct nstate *epsilon = nstate_alloc();
    struct nstate *terms_final = terms->next;
    epsilon->next = terms_final;
    terms->next = epsilon;
    nstate_lpush(&terms_final->epsilon, epsilon);
    assert(!terms->lnext);       // initial state shall have no `lnext`
    assert(!terms->next->lnext); // final state shall have no `lnext`

    if (*error)
      break;

    // concatenation. move `term->next` (the final state of `term`) to between
    // `*terms` and `*terms->next` (so it becomes the final state of `terms`)
    struct nstate *term_final = term->next;
    term->next = term_final->next;
    term_final->next = terms->next;
    terms->next = term_final;
    nstate_join(&terms, term);
    nstate_lpush(&term_final->next->epsilon, term);
  }
  if (strchr(")|", *last_regex)) { // hacky lookahead for better diagnostics
    // backtrack
    *regex = last_regex;
    *error = NULL;
  }
  if (*error) {
    nfa_free(terms);
    return NULL;
  }

  if (**regex == '|') {
    ++*regex;
    struct nstate *alt = parse_regex(regex, error);
    if (*error) {
      nfa_free(terms);
      return NULL;
    }

    struct nstate *nfa = nstate_alloc();
    nfa->next = nstate_alloc();
    nstate_join(&nfa, terms);
    nstate_join(&nfa, alt);
    nstate_lpush(&nfa->epsilon, terms);
    nstate_lpush(&nfa->epsilon, alt);
    nstate_lpush(&terms->next->epsilon, nfa->next);
    nstate_lpush(&alt->next->epsilon, nfa->next);
    return nfa;
  }

  return terms;
}

struct nstate *ltre_parse(char *regex) {
  // returns `NULL` on error

  char *error = NULL;
  struct nstate *nfa = parse_regex(&regex, &error);
  if (!error && *regex != '\0') {
    error = "expected end of input";
    nfa_free(nfa);
  }

  if (error)
    // fprintf(stderr, "ltre: error: %s near '%s'\n", error, regex);
    return NULL;

  assert(*regex == '\0'); // sanity check
  return nfa;
}

void epsilon_closure(struct nstate *nstate, uint8_t bitset[]) {
  if (!nstate)
    return; // improves performance?

  for (; nstate; nstate = nstate->lnext) {
    if (bitset_get(bitset, nstate->id))
      continue;
    bitset_set(bitset, nstate->id);
    epsilon_closure(nstate->epsilon, bitset);
  }
}

struct dstate *ltre_compile_full(struct nstate *nfa) {
  // full match. powerset construction

  int nfa_size = 0;
  for (struct nstate *nstate = nfa; nstate; nstate = nstate->next)
    nstate->id = nfa_size++;

  // cache locality, probably
  struct nstate *nstates[nfa_size];
  for (struct nstate *nstate = nfa; nstate; nstate = nstate->next)
    nstates[nstate->id] = nstate;

  int bitset_size = (nfa_size + 7) / 8; // ceil

  struct dstate *head = dstate_alloc(bitset_size);
  epsilon_closure(nfa, head->bitset);
  struct dstate *tail = head;

  for (struct dstate *elem = head; elem; elem = elem->qnext) {
    for (int chr = 0; chr < 256; chr++) {
      uint8_t bitset_union[bitset_size];
      memset(bitset_union, 0, bitset_size);

      for (int id = 0; id < nfa_size; id++)
        if (bitset_get(elem->bitset, id))
          epsilon_closure(nstates[id]->transitions[chr], bitset_union);

      // binary tree not necessary, this is just as fast
      struct dstate **dstatep = &head;
      while (*dstatep && memcmp((*dstatep)->bitset, bitset_union, bitset_size))
        dstatep = &(*dstatep)->next;

      if (!*dstatep) {
        *dstatep = dstate_alloc(bitset_size);
        memcpy((*dstatep)->bitset, bitset_union, bitset_size);
        tail->qnext = *dstatep;
        tail = *dstatep;
      }

      elem->transitions[chr] = *dstatep;
    }

    elem->accepting = bitset_get(elem->bitset, nfa->next->id);
  }

  // dfa_dump(head);

  return head;
}

struct dstate *ltre_compile_part(struct nstate *nfa) {
  // partial match. effectively, surround the NFA between two `[^]*`s
  for (int chr = 0; chr < 256; chr++) {
    nstate_lpush(&nfa->transitions[chr], nfa);
    nstate_lpush(&nfa->next->transitions[chr], nfa->next);
  }

  return ltre_compile_full(nfa);
}

bool ltre_matches(struct dstate *dfa, uint8_t *input) {
  // time linear in the input length :)
  for (; *input; input++)
    dfa = dfa->transitions[*input];
  return dfa->accepting;
}
