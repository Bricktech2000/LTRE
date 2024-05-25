#include "ltre.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\.-^$*+?[]()|"  // for parser
typedef uint8_t charset_t[256 / 8]; // for parser

// as a DFA, `*dstate` is the initial state
struct dstate {
  struct dstate *transitions[256];
  bool accepting;
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
  dstate->accepting = false;
  dstate->next = NULL;
  dstate->qnext = NULL;
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

uint8_t parse_hexbyte(char **regex, char **error) {
  uint8_t byte = 0;
  for (int i = 0; i < 2; i++) {
    byte <<= 4;
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

uint8_t parse_literal(char **regex, char **error) {
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

void parse_shorthand(charset_t charset, char **regex, char **error) {
  // `charset` shall be zeroed

  if (**regex == '\\') {
    ++*regex;
    switch (*(*regex)++) {
    case 'd':
      for (int chr = 0; chr < 256; chr++)
        if (isdigit(chr))
          bitset_set(charset, chr);
      return;
    case 'D':
      for (int chr = 0; chr < 256; chr++)
        if (!isdigit(chr))
          bitset_set(charset, chr);
      return;
    case 's':
      for (int chr = 0; chr < 256; chr++)
        if (isspace(chr))
          bitset_set(charset, chr);
      return;
    case 'S':
      for (int chr = 0; chr < 256; chr++)
        if (!isspace(chr))
          bitset_set(charset, chr);
      return;
    case 'w':
      for (int chr = 0; chr < 256; chr++)
        if (chr == '_' || isalnum(chr))
          bitset_set(charset, chr);
      return;
    case 'W':
      for (int chr = 0; chr < 256; chr++)
        if (chr != '_' && !isalnum(chr))
          bitset_set(charset, chr);
      return;
    }
    --*regex, --*regex;
  }

  if (**regex == '.') {
    ++*regex;
    for (int chr = 0; chr < 256; chr++)
      if (chr != '\n')
        bitset_set(charset, chr);
    return;
  }

  *error = "expected shorthand class";
  return;
}

void parse_charset(charset_t charset, char **regex, char **error) {
  // `charset` shall be zeroed

  bool invert = false;
  if (**regex == '^')
    ++*regex, invert = true;

  char *last_regex = *regex;
  parse_shorthand(charset, regex, error);
  if (!*error)
    goto process_invert;
  *error = NULL;
  *regex = last_regex;

  if (**regex == '[') {
    ++*regex;

    // backwards compatibility
    // if (**regex == '^')
    //   ++*regex, invert ^= true;

    // hacky lookahead for better diagnostics
    while (!strchr("]", **regex)) {
      charset_t class = {0};
      parse_charset(class, regex, error);
      if (*error)
        return;

      for (int i = 0; i < sizeof(charset_t); i++)
        charset[i] |= class[i];
    }

    if (**regex != ']') {
      *error = "expected ']'";
      return;
    }

    ++*regex;
    goto process_invert;
  }
  *regex = last_regex;

  uint8_t begin = parse_literal(regex, error);
  if (!*error) {
    uint8_t end = begin;
    if (**regex == '-') {
      ++*regex;
      end = parse_literal(regex, error);
      if (!*error && begin > end) {
        *regex = last_regex;
        *error = "invalid character range";
      }
      if (*error)
        return;
    }

    for (int chr = begin; chr <= end; chr++)
      bitset_set(charset, chr);
    goto process_invert;
  }
  return;

process_invert:
  if (invert)
    for (int i = 0; i < sizeof(charset_t); i++)
      charset[i] = ~charset[i];
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

  charset_t bitset = {0};
  parse_charset(bitset, regex, error);
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

  // hacky lookahead for better diagnostics
  while (!strchr(")|", **regex)) {
    struct nstate *term = parse_term(regex, error);
    if (*error) {
      nfa_free(terms);
      return NULL;
    }

    // concatenation. move `term->next` (the final state of `term`) to between
    // `*terms` and `*terms->next` (so it becomes the final state of `terms`)
    struct nstate *term_final = term->next;
    term->next = term_final->next;
    term_final->next = terms->next;
    terms->next = term_final;
    nstate_join(&terms, term);
    nstate_lpush(&term_final->next->epsilon, term);

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

  assert(nfa); // sanity check
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

void ltre_partial(struct nstate *nfa) {
  // enable partial matching. effectively, surround the NFA by a pair of `^[]*`s
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
        // different sets of states. because of the way we parse `charset`s,
        // this should never happen. abort if it does
        abort();
    }
  }
}

bool ltre_matches(struct dstate *dfa, uint8_t *input) {
  // time linear in the input length :)
  for (; *input; input++)
    dfa = dfa->transitions[*input];
  return dfa->accepting;
}
