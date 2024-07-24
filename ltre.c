#include "ltre.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\.-^$*+?{}[]<>()|" // for parser
typedef uint8_t symset_t[256 / 8];     // for parser

// as a DFA, `*dstate` is the initial state
struct dstate {
  struct dstate *transitions[256];
  bool accepting;
  bool terminating;
  struct dstate *next;  // to keep track of all states
  struct dstate *qnext; // to form linked queue during powerset construction
  uint8_t bitset[];     // powerset representation during powerset construction
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

int nfa_get_size(struct nstate *nfa) {
  // also populates `nstate->id` with unique identifiers
  int nfa_size = 0;
  for (struct nstate *nstate = nfa; nstate; nstate = nstate->next)
    nstate->id = nfa_size++;
  return nfa_size;
}

struct nstate *nfa_clone(struct nstate *nfa) {
  int nfa_size = nfa_get_size(nfa);

  struct nstate *nstates[nfa_size];
  for (int id = 0; id < nfa_size; id++)
    nstates[id] = nstate_alloc();

  for (struct nstate *nstate = nfa; nstate; nstate = nstate->next) {
    for (int chr = 0; chr < 256; chr++)
      if (nstate->transitions[chr])
        nstates[nstate->id]->transitions[chr] =
            nstates[nstate->transitions[chr]->id];
    if (nstate->epsilon)
      nstates[nstate->id]->epsilon = nstates[nstate->epsilon->id];
    if (nstate->next)
      nstates[nstate->id]->next = nstates[nstate->next->id];
    if (nstate->lnext)
      nstates[nstate->id]->lnext = nstates[nstate->lnext->id];
  }

  return nstates[nfa->id];
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

void nfa_concat(struct nstate *nfa, struct nstate *other) {
  // concatenate NFAs. move `other->next` (the final state of `other`) to
  // between `*nfa` and `*nfa->next` (so it becomes the final state of `nfa`)
  struct nstate *nfa_final = other->next;
  other->next = nfa_final->next;
  nfa_final->next = nfa->next;
  nfa->next = nfa_final;
  nstate_join(&nfa, other);
  nstate_lpush(&nfa_final->next->epsilon, other);
}

struct dstate *dstate_alloc(int bitset_size) {
  struct dstate *dstate = malloc(sizeof(struct dstate) + bitset_size);
  memset(dstate->transitions, 0, sizeof(dstate->transitions));
  dstate->accepting = false;
  dstate->terminating = false;
  dstate->next = NULL;
  dstate->qnext = NULL;
  memset(dstate->bitset, 0x00, bitset_size);
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
  printf("  I( ) --> %d\n", 0);

  int id1 = 0;
  for (struct dstate *dfa1 = dfa; dfa1; dfa1 = dfa1->next, id1++) {
    if (dfa1->accepting)
      printf("  %d --> F( )\n", id1);

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
      printf("  %d -- %s --> %d\n", id1, buf, id2);
    }
  }
}

// some invariants for parsers on `error`:
// - the NFA returned shall be `NULL`
// - `regex` shall point to the error location
// - the caller may backtrack if necessary

unsigned parse_natural(char **regex, char **error) {
  if (!isdigit(**regex)) {
    *error = "expected natural number";
    return 0;
  }

  unsigned natural = 0; // may wrap around
  for (; isdigit(**regex); ++*regex)
    natural *= 10, natural += **regex - '0';
  return natural;
}

uint8_t parse_hexbyte(char **regex, char **error) {
  uint8_t byte = 0;
  for (int i = 0; i < 2; i++) {
    byte <<= 4;
    char chr = **regex;
    if (isdigit(chr))
      byte |= chr - '0';
    else if (isxdigit(chr))
      byte |= tolower(chr) - 'a' + 10;
    else {
      *error = "expected hex digit";
      return 0;
    }
    ++*regex;
  }
  return byte;
}

uint8_t parse_escape(char **regex, char **error) {
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
  *error = "unknown escape";
  return 0;
}

uint8_t parse_symbol(char **regex, char **error) {
  if (**regex == '\\') {
    ++*regex;
    uint8_t escape = parse_escape(regex, error);
    if (*error) {
      --*regex;
      *error = "unknown escape sequence";
      return 0;
    }

    return escape;
  }

  if (**regex == '\0') {
    *error = "expected symbol";
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

void parse_shorthand(symset_t symset, char **regex, char **error) {
  memset(symset, 0x00, sizeof(symset_t));

  if (**regex == '\\') {
    ++*regex;
    switch (*(*regex)++) {
    case 'd':
      for (int chr = 0; chr < 256; chr++)
        if (isdigit(chr))
          bitset_set(symset, chr);
      return;
    case 'D':
      for (int chr = 0; chr < 256; chr++)
        if (!isdigit(chr))
          bitset_set(symset, chr);
      return;
    case 's':
      for (int chr = 0; chr < 256; chr++)
        if (isspace(chr))
          bitset_set(symset, chr);
      return;
    case 'S':
      for (int chr = 0; chr < 256; chr++)
        if (!isspace(chr))
          bitset_set(symset, chr);
      return;
    case 'w':
      for (int chr = 0; chr < 256; chr++)
        if (chr == '_' || isalnum(chr))
          bitset_set(symset, chr);
      return;
    case 'W':
      for (int chr = 0; chr < 256; chr++)
        if (chr != '_' && !isalnum(chr))
          bitset_set(symset, chr);
      return;
    }
    --*regex, --*regex;
  }

  if (**regex == '.') {
    ++*regex;
    for (int chr = 0; chr < 256; chr++)
      if (chr != '\n')
        bitset_set(symset, chr);
    return;
  }

  *error = "expected shorthand class";
  return;
}

void parse_symset(symset_t symset, char **regex, char **error) {
  bool invert = false;
  if (**regex == '^')
    ++*regex, invert = true;

  char *last_regex = *regex;
  parse_shorthand(symset, regex, error);
  if (!*error)
    goto process_invert;
  *error = NULL;
  *regex = last_regex;

  if (**regex == '[') {
    ++*regex;

    // backwards compatibility, if necessary
    // if (**regex == '^')
    //   ++*regex, invert ^= true;

    memset(symset, 0x00, sizeof(symset_t));
    // hacky lookahead for better diagnostics
    while (!strchr("]", **regex)) {
      symset_t sub;
      parse_symset(sub, regex, error);
      if (*error)
        return;

      for (int i = 0; i < sizeof(symset_t); i++)
        symset[i] |= sub[i];
    }

    if (**regex != ']') {
      *error = "expected ']'";
      return;
    }

    ++*regex;
    goto process_invert;
  }
  *regex = last_regex;

  if (**regex == '<') {
    ++*regex;

    memset(symset, 0xff, sizeof(symset_t));
    // hacky lookahead for better diagnostics
    while (!strchr(">", **regex)) {
      symset_t sub;
      parse_symset(sub, regex, error);
      if (*error)
        return;

      for (int i = 0; i < sizeof(symset_t); i++)
        symset[i] &= sub[i];
    }

    if (**regex != '>') {
      *error = "expected '>'";
      return;
    }

    ++*regex;
    goto process_invert;
  }
  *regex = last_regex;

  uint8_t begin = parse_symbol(regex, error);
  if (!*error) {
    uint8_t end = begin;
    if (**regex == '-') {
      ++*regex;
      end = parse_symbol(regex, error);
      if (*error)
        return;
    }

    end++; // open upper bound
    memset(symset, 0x00, sizeof(symset_t));
    uint8_t chr = begin;
    do
      bitset_set(symset, chr);
    while (++chr != end);
    goto process_invert;
  }
  return;

process_invert:
  if (invert)
    for (int i = 0; i < sizeof(symset_t); i++)
      symset[i] = ~symset[i];
  return;
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

  symset_t bitset;
  parse_symset(bitset, regex, error);
  if (*error)
    return NULL;

  struct nstate *chars = nstate_alloc();
  chars->next = nstate_alloc();
  for (int chr = 0; chr < 256; chr++)
    if (bitset_get(bitset, chr))
      nstate_lpush(&chars->transitions[chr], chars->next);

  return chars;
}

struct nstate *parse_term(char **regex, char **error) {
  struct nstate *atom = parse_atom(regex, error);
  if (*error)
    return NULL;

  if (**regex == '*') {
    ++*regex;
    nstate_lpush(&atom->next->epsilon, atom);
    nstate_lpush(&atom->epsilon, atom->next);
    goto pad_atom;
  }

  if (**regex == '+') {
    ++*regex;
    nstate_lpush(&atom->next->epsilon, atom);
    goto pad_atom;
  }

  if (**regex == '?') {
    ++*regex;
    nstate_lpush(&atom->epsilon, atom->next);
    goto pad_atom;
  }

  char *last_regex = *regex;
  if (**regex == '{') {
    ++*regex;
    unsigned min = parse_natural(regex, error);
    if (*error)
      min = 0, *error = NULL;

    unsigned max = min;
    if (**regex == ',') {
      ++*regex;
      max = parse_natural(regex, error);
      if (*error)
        max = -1, *error = NULL;
    }

    if (**regex != '}') {
      *error = "expected '}'";
      nfa_free(atom);
      return NULL;
    }
    ++*regex;

    if (min > max) {
      *regex = last_regex;
      *error = "misbounded quantifier";
      nfa_free(atom);
      return NULL;
    }

    struct nstate *template = atom;
    atom = nstate_alloc();
    atom->next = nstate_alloc();
    nstate_lpush(&atom->epsilon, atom->next);

    unsigned ncopies = max == -1 ? min : max;
    for (unsigned i = 0; i < ncopies; i++) {
      struct nstate *instance = nfa_clone(template);
      // add `?` epsilon transitions to uphold `min` bound
      if (i >= min)
        nstate_lpush(&instance->epsilon, instance->next);
      nfa_concat(atom, instance);
    }
    // add a `+` epsilon transition if `max` is unbounded
    if (max == -1)
      nstate_lpush(&atom->next->epsilon, atom);

    nfa_free(template);
    goto pad_atom;
  }

pad_atom:;
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

  // hacky lookahead for better diagnostics
  while (!strchr(")|", **regex)) {
    struct nstate *term = parse_term(regex, error);
    if (*error) {
      nfa_free(terms);
      return NULL;
    }
    nfa_concat(terms, term);

  epsilon:;
    // introduce a dummy node to handle concatenation. this ensures any `<term>`
    // within a `<regex>` is surrounded by a pair of epsilon transitions to
    // and from states **which have no `lnext`**. the quantifier parsers in
    // `parse_term` assume this invariant. these gymnastics are necessary
    // because `nstate->epsilon` is a linked list of `nstate`s and not of
    // `nstate` pointers
    struct nstate *epsilon = nstate_alloc();
    struct nstate *terms_final = terms->next;
    epsilon->next = terms_final;
    terms->next = epsilon;
    nstate_lpush(&terms_final->epsilon, epsilon);
    assert(!terms->lnext);       // initial state shall have no `lnext`
    assert(!terms->next->lnext); // final state shall have no `lnext`
  }

  // ensure the initial and final states of `terms` are not joined by a single
  // epsilon transition. the quantifier parsers `?` and `*` in `parse_term`
  // assume this invariant
  if (terms->epsilon == terms->next)
    goto epsilon;

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

struct nstate *ltre_parse(char **regex, char **error) {
  // returns `NULL` on error; `regex` will point to the error location and
  // `error` will be set to an error message. `error` may be set to `NULL`
  // to disable error reporting

  // don't write to `*regex` or `*error` if error reporting is disabled
  char *e, *r = *regex;
  if (error == NULL)
    error = &e, regex = &r;

  *error = NULL;
  struct nstate *nfa = parse_regex(regex, error);
  if (*error)
    return NULL;

  if (**regex != '\0') {
    *error = "expected end of input";
    nfa_free(nfa);
    return NULL;
  }

  assert(nfa);
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

struct dstate *ltre_compile(struct nstate *nfa) {
  // full match. powerset construction

  int nfa_size = nfa_get_size(nfa);

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
      memset(bitset_union, 0x00, bitset_size);

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
    elem->terminating = true; // default to true for below
  }

  // flag "terminating" states. a terminating state is a state which either
  // always or never leads to an accepting state. a state is terminating if and
  // only if all its transitions are terminating and have the same `accepting`
  // value as that state. to avoid having to deal with cycles, we default to all
  // states being terminating then iteratively rule out the ones that aren't.
  for (bool done = false; (done = !done);)
    for (struct dstate *elem = head; elem; elem = elem->qnext)
      if (elem->terminating)
        for (int chr = 0; chr < 256; chr++)
          if (elem->accepting != elem->transitions[chr]->accepting ||
              !elem->transitions[chr]->terminating)
            done = elem->terminating = false;

  // dfa_dump(head);

  return head;
}

void ltre_partial(struct nstate *nfa) {
  // enable partial matching. effectively, surround the NFA by a pair of `<>*`s
  for (int chr = 0; chr < 256; chr++) {
    nstate_lpush(&nfa->transitions[chr], nfa);
    nstate_lpush(&nfa->next->transitions[chr], nfa->next);
  }
}

void ltre_ignorecase(struct nstate *nfa) {
  // enable case-insensitive matching. that is, ensure all transitions point to
  // the same set of states as their swapped-case counterpart
  for (struct nstate *nstate = nfa; nstate; nstate = nstate->next) {
    for (int chr = 0; chr <= 256; chr++) {
      int lower = tolower(chr), upper = toupper(chr);
      if (nstate->transitions[upper] == nstate->transitions[lower])
        continue; // already case-insensitive
      else if (nstate->transitions[upper] == NULL)
        nstate->transitions[upper] = nstate->transitions[lower];
      else if (nstate->transitions[lower] == NULL)
        nstate->transitions[lower] = nstate->transitions[upper];
      else
        // both the `upper` and the `lower` transitions exist and point to
        // different sets of states. because of the way we `parse_symset`s,
        // this should never happen. abort if it does
        abort();
    }
  }
}

bool ltre_matches(struct dstate *dfa, uint8_t *input) {
  // time linear in the input length :)
  for (; !dfa->terminating && *input; input++)
    dfa = dfa->transitions[*input];
  return dfa->accepting;
}
