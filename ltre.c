#include "ltre.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\.-^$*+?{}[]<>()|"
typedef uint8_t symset_t[256 / 8];

// an NFA state. we can assume NFA states have at most two outgoing epsilon
// transitions and at most one outgoing labeled transition without loss of
// generality. so regexes like /./ and /\w/ don't blow out NFA size, we label
// the labeled transition with a set of characters (using `symset_t`) instead
// of with a single character
struct nstate {
  symset_t label;          // set of characters labeling the labeled transition
  struct nstate *target;   // target state for the labeled transition
  struct nstate *epsilon0; // primary epsilon transition, for concatenation
  struct nstate *epsilon1; // secondary epsilon transition, for anything else
  struct nstate *next;     // linked list to keep track of all states of an NFA
  int id;                  // populated and used for various purposes throughout
};

// a DFA state, and maybe an actual DFA too, depending on context. when treated
// as a DFA, the first element of the linked list of states formed by `next` is
// the initial state and subsequent elements enumerate all remaining states
struct dstate {
  struct dstate *transitions[256]; // indexed by input characters
  bool accepting;                  // match result
  bool terminating;                // for early termination
  int id;              // populated and used for various purposes throughout
  struct dstate *next; // linked list to keep track of all states of a DFA
  uint8_t bitset[];    // powerset representation during powerset construction
};

static bool bitset_get(uint8_t bitset[], int id) {
  return bitset[id / 8] & 1 << id % 8;
}

static void bitset_set(uint8_t bitset[], int id) {
  bitset[id / 8] |= 1 << id % 8;
}

static char *symset_fmt(symset_t symset) {
  // pretty-format for debugging

  static char buf[1024], nbuf[1024];
  char *bufp = buf, *nbufp = nbuf;

  *nbufp++ = '^';
  *bufp++ = *nbufp++ = '[';

  for (int chr = 0; chr < 256; chr++) {
  append_chr:;
    char **p = bitset_get(symset, chr) ? &bufp : &nbufp;
    bool is_metachar = chr && strchr(METACHARS, chr);
    if (!isprint(chr) && !is_metachar)
      *p += sprintf(*p, "\\x%02hhx", chr);
    else {
      if (is_metachar)
        *(*p)++ = '\\';
      // avoid breaking Mermaid
      if (strchr("\"#&{}()xo=-", chr))
        *p += sprintf(*p, "#%hhu;", chr);
      else
        *(*p)++ = chr;
    }

    // make character ranges
    int start = chr;
    while (chr < 255 && bitset_get(symset, chr) == bitset_get(symset, chr + 1))
      chr++;
    if (chr - start >= 2)
      *(*p)++ = '-';
    if (chr - start >= 1)
      goto append_chr;
  }

  *bufp++ = *nbufp++ = ']';
  *bufp++ = *nbufp++ = '\0';

  // return a negated character class if it is shorter
  return (bufp - buf < nbufp - nbuf) ? buf : nbuf;
}

static struct nstate *nstate_alloc(void) {
  struct nstate *nstate = malloc(sizeof(struct nstate));
  memset(nstate->label, 0x00, sizeof(symset_t));
  nstate->target = NULL;
  nstate->epsilon0 = NULL;
  nstate->epsilon1 = NULL;
  nstate->next = NULL;
  nstate->id = -1;
  return nstate;
}

void nfa_free(struct nfa nfa) {
  for (struct nstate *next, *nstate = nfa.initial; nstate; nstate = next)
    next = nstate->next, free(nstate);
}

static int nfa_get_size(struct nfa nfa) {
  // also populates `nstate.id` with unique identifiers
  int nfa_size = 0;
  for (struct nstate *nstate = nfa.initial; nstate; nstate = nstate->next)
    nstate->id = nfa_size++;
  return nfa_size;
}

static struct nfa nfa_clone(struct nfa nfa) {
  int nfa_size = nfa_get_size(nfa);

  struct nstate *nstates[nfa_size];
  for (int id = 0; id < nfa_size; id++)
    nstates[id] = nstate_alloc();

#define MAYBE_COPY(FIELD)                                                      \
  if (nstate->FIELD)                                                           \
    nstates[nstate->id]->FIELD = nstates[nstate->FIELD->id];

  for (struct nstate *nstate = nfa.initial; nstate; nstate = nstate->next) {
    memcpy(nstates[nstate->id]->label, nstate->label, sizeof(symset_t));
    MAYBE_COPY(target);
    MAYBE_COPY(epsilon0);
    MAYBE_COPY(epsilon1);
    MAYBE_COPY(next);
  }

#undef MAYBE_COPY

  return (struct nfa){
      .initial = nstates[nfa.initial->id],
      .final = nstates[nfa.final->id],
  };
}

static void nfa_concat(struct nfa *nfap, struct nfa nfa) {
  // memcpys `nfa.initial` into `nfap->final` then frees `nfa.initial`; assumes
  // nothing refers to `nfa.initial` and assumes `nfap->final` refers to nothing
  if (nfap->initial == nfap->final)
    nfa_free(*nfap), *nfap = nfa;
  else if (nfa.initial != nfa.final) {
    memcpy(nfap->final, nfa.initial, sizeof(struct nstate));
    nfap->final = nfa.final;
    free(nfa.initial);
  }
}

static void nfa_pad_initial(struct nfa *nfa) {
  struct nstate *initial = nstate_alloc();
  initial->epsilon0 = nfa->initial;
  initial->next = nfa->initial;
  nfa->initial = initial;
}

static void nfa_pad_final(struct nfa *nfa) {
  struct nstate *final = nstate_alloc();
  nfa->final->epsilon0 = final;
  nfa->final->next = final;
  nfa->final = final;
}

void nfa_dump(struct nfa nfa) {
  (void)nfa_get_size(nfa);

  printf("graph LR\n");
  printf("  I( ) --> %d\n", nfa.initial->id);
  printf("  %d --> F( )\n", nfa.final->id);

  for (struct nstate *nstate = nfa.initial; nstate; nstate = nstate->next) {
    if (nstate->epsilon0)
      printf("  %d --> %d\n", nstate->id, nstate->epsilon0->id);
    if (nstate->epsilon1)
      printf("  %d --> %d\n", nstate->id, nstate->epsilon1->id);

    char *fmt = symset_fmt(nstate->label);
    if (strcmp(fmt, "[]") != 0)
      printf("  %d --%s--> %d\n", nstate->id, fmt, nstate->target->id);
  }
}

static struct dstate *dstate_alloc(int bitset_size) {
  struct dstate *dstate = malloc(sizeof(struct dstate) + bitset_size);
  memset(dstate->transitions, 0, sizeof(dstate->transitions));
  dstate->accepting = false;
  dstate->terminating = false;
  dstate->id = -1;
  dstate->next = NULL;
  memset(dstate->bitset, 0x00, bitset_size);
  return dstate;
}

void dfa_free(struct dstate *dstate) {
  for (struct dstate *next; dstate; dstate = next)
    next = dstate->next, free(dstate);
}

static int dfa_get_size(struct dstate *dfa) {
  // also populates `dstate.id` with unique identifiers
  int dfa_size = 0;
  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next)
    dstate->id = dfa_size++;
  return dfa_size;
}

void dfa_dump(struct dstate *dfa) {
  (void)dfa_get_size(dfa);

  printf("graph LR\n");
  printf("  I( ) --> %d\n", dfa->id);

  for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next) {
    if (ds1->accepting)
      printf("  %d --> F( )\n", ds1->id);

    for (struct dstate *ds2 = dfa; ds2; ds2 = ds2->next) {
      symset_t transitions = {0};
      for (int chr = 0; chr < 256; chr++)
        if (ds1->transitions[chr] == ds2)
          bitset_set(transitions, chr);

      char *fmt = symset_fmt(transitions);
      if (strcmp(fmt, "[]") != 0)
        printf("  %d --%s--> %d\n", ds1->id, fmt, ds2->id);
    }
  }
}

// some invariants for parsers on parse error:
// - `error` shall be set to a non-`NULL` error message
// - `regex` shall point to the error location
// - the returned NFA shall be the null NFA
// - the caller is responsible for backtracking

static unsigned parse_natural(char **regex, char **error) {
  if (!isdigit(**regex)) {
    *error = "expected natural number";
    return 0;
  }

  unsigned natural = 0; // may wrap around
  for (; isdigit(**regex); ++*regex)
    natural *= 10, natural += **regex - '0';
  return natural;
}

static uint8_t parse_hexbyte(char **regex, char **error) {
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

static uint8_t parse_escape(char **regex, char **error) {
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

static uint8_t parse_symbol(char **regex, char **error) {
  if (**regex == '\\') {
    ++*regex;
    uint8_t escape = parse_escape(regex, error);
    if (*error)
      return 0;

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

  if (!isprint(**regex)) {
    *error = "unexpected nonprintable character";
    return 0;
  }

  return *(*regex)++;
}

static void parse_shorthand(symset_t symset, char **regex, char **error) {
  memset(symset, 0x00, sizeof(symset_t));

#define RETURN_SYMSET(COND)                                                    \
  for (int chr = 0; chr < 256; chr++)                                          \
    if (COND)                                                                  \
      bitset_set(symset, chr);                                                 \
  return;

  if (**regex == '\\') {
    ++*regex;

    switch (*(*regex)++) {
    case 'd':
      RETURN_SYMSET(isdigit(chr))
    case 'D':
      RETURN_SYMSET(!isdigit(chr))
    case 's':
      RETURN_SYMSET(isspace(chr))
    case 'S':
      RETURN_SYMSET(!isspace(chr))
    case 'w':
      RETURN_SYMSET(chr == '_' || isalnum(chr))
    case 'W':
      RETURN_SYMSET(chr != '_' && !isalnum(chr))
    }

    --*regex, --*regex;
  }

  if (**regex == '.') {
    ++*regex;
    RETURN_SYMSET(chr != '\n')
  }

#undef RETURN_SYMSET

  *error = "expected shorthand class";
  return;
}

static void parse_symset(symset_t symset, char **regex, char **error) {
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

static struct nfa parse_regex(char **regex, char **error);
static struct nfa parse_atom(char **regex, char **error) {
  if (**regex == '(') {
    ++*regex;
    struct nfa sub = parse_regex(regex, error);
    if (*error)
      return (struct nfa){NULL};

    if (**regex != ')') {
      *error = "expected ')'";
      nfa_free(sub);
      return (struct nfa){NULL};
    }

    ++*regex;
    return sub;
  }

  struct nfa chars = {.initial = nstate_alloc(), .final = nstate_alloc()};
  chars.initial->next = chars.final;
  chars.initial->target = chars.final;

  parse_symset(chars.initial->label, regex, error);
  if (*error) {
    nfa_free(chars);
    return (struct nfa){NULL};
  }

  return chars;
}

static struct nfa parse_term(char **regex, char **error) {
  struct nfa atom = parse_atom(regex, error);
  if (*error)
    return (struct nfa){NULL};

  //         <---
  // -->O-->(atom)-->O-->
  //     ----------->
  if (**regex == '*') {
    ++*regex;
    atom.final->epsilon1 = atom.initial;
    nfa_pad_initial(&atom), nfa_pad_final(&atom);
    atom.initial->epsilon1 = atom.final;
    return atom;
  }

  //         <---
  // -->O-->(atom)-->O-->
  if (**regex == '+') {
    ++*regex;
    atom.final->epsilon1 = atom.initial;
    nfa_pad_initial(&atom), nfa_pad_final(&atom);
    return atom;
  }

  // -->(atom)-->
  //     --->
  if (**regex == '?') {
    ++*regex;
    if (atom.initial->epsilon1)
      nfa_pad_initial(&atom);
    atom.initial->epsilon1 = atom.final;
    return atom;
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
      return (struct nfa){NULL};
    }
    ++*regex;

    if (min > max) {
      *regex = last_regex;
      *error = "misbounded quantifier";
      nfa_free(atom);
      return (struct nfa){NULL};
    }

    struct nfa atoms;
    atoms.initial = atoms.final = nstate_alloc();

    // if `max` is bounded, make `max` copies and add `?` epsilon transitions:
    // -->(atom)...(atom)(atom)-->
    //              --->  --->
    // if it is not, make `min + 1` copies and add one `*` epsilon transition:
    //                       <---
    // -->...(atom)(atom)-->(atom)-->O-->
    //                   ----------->
    unsigned ncopies = max == -1 ? min + 1 : max;
    for (unsigned i = 0; i < ncopies; i++) {
      struct nfa clone = nfa_clone(atom);
      if (i >= min) {
        if (max == -1) {
          clone.final->epsilon1 = clone.initial;
          nfa_pad_initial(&clone), nfa_pad_final(&clone);
        }
        clone.initial->epsilon1 = clone.final;
      }

      nfa_concat(&atoms, clone);
    }

    nfa_free(atom);

    return atoms;
  }

  return atom;
}

static struct nfa parse_regex(char **regex, char **error) {
  struct nfa terms;
  terms.initial = terms.final = nstate_alloc();

  // hacky lookahead for better diagnostics
  while (!strchr(")|", **regex)) {
    struct nfa term = parse_term(regex, error);
    if (*error) {
      nfa_free(terms);
      return (struct nfa){NULL};
    }

    nfa_concat(&terms, term);
  }

  if (**regex == '|') {
    ++*regex;
    struct nfa alt = parse_regex(regex, error);
    if (*error) {
      nfa_free(terms);
      return (struct nfa){NULL};
    }

    // -->O-->(terms)-->
    //     --->(alt)--->O-->
    nfa_pad_initial(&terms), nfa_pad_final(&alt);
    terms.initial->epsilon1 = alt.initial;
    terms.final->epsilon0 = alt.final;
    terms.final->next = alt.initial;
    terms.final = alt.final;
  }

  return terms;
}

struct nfa ltre_parse(char **regex, char **error) {
  // returns a null NFA on error; `*regex` will point to the error location and
  // `*error` will be set to an error message. `error` may be set to `NULL` to
  // disable error reporting

  // don't write to `*regex` or `*error` if error reporting is disabled
  char *e, *r = *regex;
  if (error == NULL)
    error = &e, regex = &r;

  *error = NULL;
  struct nfa nfa = parse_regex(regex, error);
  if (*error)
    return (struct nfa){NULL};

  if (**regex != '\0') {
    *error = "expected end of input";
    nfa_free(nfa);
    return (struct nfa){NULL};
  }

  return nfa;
}

static void epsilon_closure(struct nstate *nstate, uint8_t bitset[]) {
  if (!nstate)
    return;

  if (bitset_get(bitset, nstate->id))
    return; // already visited

  bitset_set(bitset, nstate->id);
  epsilon_closure(nstate->epsilon0, bitset);
  epsilon_closure(nstate->epsilon1, bitset);
}

struct dstate *ltre_compile(struct nfa nfa) {
  // full match. powerset construction and minimization

  int nfa_size = nfa_get_size(nfa);

  // cache locality, probably
  struct nstate *nstates[nfa_size];
  for (struct nstate *nstate = nfa.initial; nstate; nstate = nstate->next)
    nstates[nstate->id] = nstate;

  int bitset_size = (nfa_size + 7) / 8; // ceil

  struct dstate *dfa = dstate_alloc(bitset_size);
  epsilon_closure(nfa.initial, dfa->bitset);

  // construct new DFA states as we're iterating over them and patching
  // transitions, starting from the epsilon closure of the NFA's initial state
  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next) {
    for (int chr = 0; chr < 256; chr++) {
      uint8_t bitset_union[bitset_size];
      memset(bitset_union, 0x00, bitset_size);

      // compute the "superposition" of NFA states reachable by consuming `chr`
      for (int id = 0; id < nfa_size; id++)
        if (bitset_get(dstate->bitset, id) &&
            bitset_get(nstates[id]->label, chr))
          epsilon_closure(nstates[id]->target, bitset_union);

      // create a DFA state whose `bitset` corresponds to this "superposition",
      // if it doesn't already exist. binary tree not necessary, linear search
      // is just as fast
      struct dstate **dstatep = &dfa;
      while (*dstatep && memcmp((*dstatep)->bitset, bitset_union, bitset_size))
        dstatep = &(*dstatep)->next;

      if (!*dstatep) {
        *dstatep = dstate_alloc(bitset_size);
        memcpy((*dstatep)->bitset, bitset_union, bitset_size);
      }

      // patch the `chr` transition to point to this new (or existing) state
      dstate->transitions[chr] = *dstatep;
    }

    // a DFA state is accepting if and only if it contains the NFA's final state
    // in its `bitset`
    dstate->accepting = bitset_get(dstate->bitset, nfa.final->id);
  }

  int dfa_size = dfa_get_size(dfa);

  // flag indistinguishable states. a pair of states is indistinguishable if and
  // only if both states have the same `accepting` value and their transitions
  // are equal up to indistinguishability. to avoid having to deal with cycles,
  // we default to all states being indistinguishable then iteratively rule out
  // the ones that aren't
  bool dis[dfa_size][dfa_size]; // distinguishability matrix. symmetric
  memset(dis, false, sizeof(dis));
  for (bool done = false; (done = !done);)
    for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next)
      for (struct dstate *ds2 = ds1; ds2; ds2 = ds2->next)
        if (!dis[ds1->id][ds2->id])
          for (int chr = 0; chr < 256; chr++)
            if (ds1->accepting != ds2->accepting ||
                dis[ds1->transitions[chr]->id][ds2->transitions[chr]->id])
              dis[ds1->id][ds2->id] = dis[ds2->id][ds1->id] = true,
              ds1->terminating = ds2->terminating = false, done = false;

  // minimize DFA by merging indistinguishable states
  for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next) {
    for (struct dstate *prev = ds1; prev && prev->next; prev = prev->next) {
    redo:;
      struct dstate *ds2 = prev->next;
      if (dis[ds1->id][ds2->id])
        continue;
      // states are indistinguishable. merge them
      for (struct dstate *dstate = dfa; dstate; dstate = dstate->next)
        for (int chr = 0; chr < 256; chr++)
          if (dstate->transitions[chr] == ds2)
            dstate->transitions[chr] = ds1;
      prev->next = ds2->next;
      free(ds2);
      if (prev->next)
        goto redo;
    }

    // flag "terminating" states. a terminating state is a state which either
    // always or never leads to an accepting state. since `ds1` is now
    // distinguishable from all other states, it is terminating if and only if
    // all its transitions point to itself because, by definition, no other
    // state accepts the same set of words as it does (either none or all)
    ds1->terminating = true;
    for (int chr = 0; chr < 256; chr++)
      if (ds1->transitions[chr] != ds1)
        ds1->terminating = false;
  }

  // nfa_dump(nfa);
  // dfa_dump(dfa);

  // printf("%2d -> %2d\n", dfa_size, dfa_get_size(dfa));

  return dfa;
}

void ltre_partial(struct nfa *nfa) {
  // enable partial matching. effectively, surround the NFA by a pair of `<>*`s
  nfa_pad_initial(nfa), nfa_pad_final(nfa);
  nfa->initial->target = nfa->initial;
  nfa->final->target = nfa->final;
  memset(nfa->initial->label, 0xff, sizeof(symset_t));
  memset(nfa->final->label, 0xff, sizeof(symset_t));
}

void ltre_ignorecase(struct nfa *nfa) {
  // enable case-insensitive matching. effectively, for any character a labeled
  // transition contains, make it also contain its swapped-case counterpart
  for (struct nstate *nstate = nfa->initial; nstate; nstate = nstate->next) {
    for (int chr = 0; chr <= 256; chr++) {
      if (bitset_get(nstate->label, chr)) {
        bitset_set(nstate->label, tolower(chr));
        bitset_set(nstate->label, toupper(chr));
      }
    }
  }
}

bool ltre_matches(struct dstate *dfa, uint8_t *input) {
  // time linear in the input length :)
  for (; !dfa->terminating && *input; input++)
    dfa = dfa->transitions[*input];
  return dfa->accepting;
}
