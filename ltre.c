#include "ltre.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\.-^$*+?{}[]<>()|&~"
typedef uint8_t symset_t[256 / 8];

// an NFA state. we can assume NFA states have at most two outgoing epsilon-
// transitions and at most one outgoing labeled transition without loss of
// generality. so regexes like /./ and /\w/ don't blow out NFA size, we label
// the labeled transition with a set of characters (using `symset_t`) instead
// of with a single character. note that when `target` is null, the contents of
// `label` are of no relevance. for efficient NFA reversal, we declare `source,
// delta0, delta1`, dual to `target, epsilon0, epsilon1` (in the sense that the
// former are equivalent to the latter under NFA reversal), satisfying the
// following invariants for all `nstate`:
// - `nstate->target->source == nstate`
// - `nstate->epsilon0->delta0 == nstate`
// - `nstate->epsilon1->delta1 == nstate`
struct nstate {
  symset_t label; // set of characters labeling the labeled transition
  struct nstate *target, *source;   // target state for the labeled transition
  struct nstate *epsilon0, *delta0; // 1st epsilon-transition, for concatenation
  struct nstate *epsilon1, *delta1; // 2nd epsilon-transition, for anything else
  struct nstate *next; // linked list to keep track of all states of an NFA
  int id;              // populated and used for various purposes throughout
};

// a DFA state, and maybe an actual DFA too, depending on context. when treated
// as a DFA, the first element of the linked list of states formed by `next` is
// the initial state and subsequent elements enumerate all remaining states
struct dstate {
  struct dstate *transitions[256]; // indexed by input characters
  bool accepting, terminating;     // for match result and early termination
  int id;              // populated and used for various purposes throughout
  struct dstate *next; // linked list to keep track of all states of a DFA
  uint8_t bitset[];    // powerset representation during powerset construction
};

// do not switch out these `unsigned`s for `int`s without a benchmark. on
// x86_64, `unsigned` saves us a `cdqe` instruction in the fragment `id / 8`
// and speeds up the hot loop in `dfa_step` by 10%
static bool bitset_get(uint8_t bitset[], unsigned id) {
  return bitset[id / 8] & 1 << id % 8;
}

static void bitset_set(uint8_t bitset[], unsigned id) {
  bitset[id / 8] |= 1 << id % 8;
}

static char *symset_fmt(symset_t symset) {
  // returns a static buffer. output shall be parsable by `parse_symset` and
  // satisfy the invariant `parse_symset . symset_fmt == id`. in the general
  // case, will not satisfy `symset_fmt . parse_symset == id`

  static char buf[1024], nbuf[1024];
  char *bufp = buf, *nbufp = nbuf;
  // number of symsets in `buf` and `nbuf` character classes, respectively
  int nsym = 0, nnsym = 0;

  *nbufp++ = '^';
  *bufp++ = *nbufp++ = '[';

  for (int chr = 0; chr < 256; chr++) {
  append_chr:
    bitset_get(symset, chr) ? nsym++ : nnsym++;
    char **p = bitset_get(symset, chr) ? &bufp : &nbufp;
    bool is_metachar = chr && strchr(METACHARS, chr);
    if (!isprint(chr) && !is_metachar)
      *p += sprintf(*p, "\\x%02hhx", chr);
    else {
      if (is_metachar)
        *(*p)++ = '\\';
      *(*p)++ = chr;
    }

    // make character ranges
    int start = chr;
    while (chr < 255 && bitset_get(symset, chr) == bitset_get(symset, chr + 1))
      chr++;
    if (chr - start >= 2)
      *(*p)++ = '-', bitset_get(symset, chr) ? nsym-- : nnsym--;
    if (chr - start >= 1)
      goto append_chr;
  }

  *bufp++ = *nbufp++ = ']';
  *bufp++ = *nbufp++ = '\0';

  // special casees for character classes containing zero or one symsets
  if (nnsym == 0) {
    return "<>";
  } else if (nsym == 1) {
    bufp[-2] = '\0';
    return buf + 1;
  } else if (nnsym == 1) {
    nbufp[-2] = '\0', nbuf[1] = '^';
    return nbuf + 1;
  }

  // return a complemented character class if it is shorter
  return (bufp - buf < nbufp - nbuf) ? buf : nbuf;
}

static struct nstate *nstate_alloc(void) {
  struct nstate *nstate = malloc(sizeof(*nstate));
  *nstate = (struct nstate){.id = -1};
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
    if ((nstate->id = nfa_size++) == INT_MAX)
      abort();
  return nfa_size;
}

static struct nfa nfa_clone(struct nfa nfa) {
  int nfa_size = nfa_get_size(nfa);

  struct nstate *nstates[nfa_size];
  for (int id = 0; id < nfa_size; id++)
    nstates[id] = nstate_alloc();

#define MAYBE_COPY(FIELD)                                                      \
  if (nstate->FIELD)                                                           \
  nstates[nstate->id]->FIELD = nstates[nstate->FIELD->id]

  for (struct nstate *nstate = nfa.initial; nstate; nstate = nstate->next) {
    memcpy(nstates[nstate->id]->label, nstate->label, sizeof(nstate->label));
    MAYBE_COPY(target);
    MAYBE_COPY(source);
    MAYBE_COPY(epsilon0);
    MAYBE_COPY(delta0);
    MAYBE_COPY(epsilon1);
    MAYBE_COPY(delta1);
    MAYBE_COPY(next);
  }

#undef MAYBE_COPY

  return (struct nfa){
      .initial = nstates[nfa.initial->id],
      .final = nstates[nfa.final->id],
      .complemented = nfa.complemented,
      .reversed = nfa.reversed,
  };
}

static void nfa_concat(struct nfa *nfap, struct nfa nfa) {
  // memcpys `nfa.initial` into `nfap->final` then frees `nfa.initial`; assumes
  // nothing refers to `nfa.initial` and assumes `nfap->final` refers to
  // nothing. performs a "visual" concatenation and therefore does not take into
  // account the `nfa.complemented` and `nfa.reversed` flags. if `nfa` or `nfap`
  // have one of them set, the result may not be what you expect
  if (nfa.initial == nfa.final)
    nfa_free(nfa);
  else if (nfap->initial == nfap->final)
    nfa_free(*nfap), *nfap = nfa;
  else {
    // this routine was written before `source, delta0, delta1` were added to
    // `struct nstate`, so let's quickly back-patch them while nobody's looking
#define BACKPATCH(FIELD, DUAL)                                                 \
  nfa.initial->DUAL = nfap->final->DUAL,                                       \
  nfa.initial->FIELD ? nfa.initial->FIELD->DUAL = nfap->final : 0

    BACKPATCH(target, source);
    BACKPATCH(epsilon0, delta0);
    BACKPATCH(epsilon1, delta1);

#undef BACKPATCH

    memcpy(nfap->final, nfa.initial, sizeof(*nfap->final));
    nfap->final = nfa.final;
    free(nfa.initial);
  }
}

static void nfa_pad_initial(struct nfa *nfa) {
  struct nstate *initial = nstate_alloc();
  initial->epsilon0 = nfa->initial, nfa->initial->delta0 = initial;
  initial->next = nfa->initial;
  nfa->initial = initial;
}

static void nfa_pad_final(struct nfa *nfa) {
  struct nstate *final = nstate_alloc();
  nfa->final->epsilon0 = final, final->delta0 = nfa->final;
  nfa->final->next = final;
  nfa->final = final;
}

static void nfa_canonicalize(struct nfa *nfa) {
  // ensures `!nfa->complemented && !nfa->reversed`, a useful property when
  // manipulating NFAs. if `nfa->complemented`, we unfortunately have to go
  // through the whole compile pipeline to un-complement it
  if (nfa->reversed)
    abort(); // has not been needed so far
  if (!nfa->complemented)
    return;
  struct dstate *dfa = ltre_compile(*nfa);
  struct nfa canonicalized = ltre_uncompile(dfa);
  nfa_free(*nfa), dfa_free(dfa);
  memcpy(nfa, &canonicalized, sizeof(*nfa));
}

#define DUMP_SYMSET(SYMSET)                                                    \
  for (char *fmt = symset_fmt(SYMSET); *fmt; fmt++)                            \
  /* avoid breaking Mermaid */                                                 \
  printf(strchr("\\\"#&{}()xo=- ", *fmt) ? "#%hhu;" : "%c", *fmt)

void nfa_dump(struct nfa nfa) {
  (void)nfa_get_size(nfa);

  printf("graph LR\n");
  printf("  I( ) --> %d\n", nfa.initial->id);
  printf("  %d --> F( )\n", nfa.final->id);

  for (struct nstate *nstate = nfa.initial; nstate; nstate = nstate->next) {
    // don't assume duality between `source, delta0, delta1` and `target,
    // epsilon0, epsilon1`, so we can use this routine to debug broken NFAs

#define DUMP_ARROW(TYPE, SOURCE, TARGET)                                       \
  if (nstate SOURCE && nstate TARGET)                                          \
  printf("  %d " TYPE " %d\n", nstate SOURCE->id, nstate TARGET->id)

    DUMP_ARROW("-.-", ->source, );
    DUMP_ARROW("-.-", ->delta0, );
    DUMP_ARROW("-.-", ->delta1, );
    DUMP_ARROW("--o", , ->epsilon0);
    DUMP_ARROW("--x", , ->epsilon1);

#undef DUMP_ARROW

    if (!nstate->target)
      continue;

    printf("  %d --", nstate->id);
    DUMP_SYMSET(nstate->label);
    printf("--> %d\n", nstate->target->id);
  }
}

static struct dstate *dstate_alloc(int bitset_size) {
  struct dstate *dstate = malloc(sizeof(*dstate) + bitset_size);
  *dstate = (struct dstate){.id = -1};
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
    if ((dstate->id = dfa_size++) == INT_MAX)
      abort();
  return dfa_size;
}

static void leb128_put(uint8_t **p, int n) {
  while (n >> 7)
    *(*p)++ = (n & 0x7f) | 0x80, n >>= 7;
  *(*p)++ = n;
}

static int leb128_get(uint8_t **p) {
  int n = 0, c = 0;
  do
    n |= (**p & 0x7f) << c++ * 7;
  while (*(*p)++ & 0x80);

  return n;
}

uint8_t *dfa_serialize(struct dstate *dfa, size_t *size) {
  // serialize a DFA using a mix of RLE and LEB128. `size` is an out parameter

  int dfa_size = dfa_get_size(dfa);

  // len(leb128(dfa_size)) == ceil(log128(dfa_size))
  int ceil_log128 = 0;
  for (int s = dfa_size; s >>= 7;)
    ceil_log128++;
  ceil_log128++;

  uint8_t *buf = malloc(ceil_log128), *p = buf;
  leb128_put(&p, dfa_size);

  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next) {
    // ensure buffer large enough for worst case. worst case is typically around
    // 500 bytes larger than best case, so this is not too wasteful.
    ptrdiff_t len = p - buf;
    // len + <accepting_terminating> + 256 * (<run_length> + <leb128(dfa_size)>)
    uint8_t *new = realloc(buf, len + 1 + 256 * (1 + ceil_log128));
    buf = new, p = new + len;

    *p++ = dstate->accepting << 1 | dstate->terminating;
    for (int chr = 0; chr < 256;) {
      int start = chr;
      while (chr < 255 &&
             dstate->transitions[chr] == dstate->transitions[chr + 1])
        chr++;
      *p++ = chr - start; // run length
      leb128_put(&p, dstate->transitions[chr++]->id);
    }
  }

  *size = p - buf;
  return realloc(buf, p - buf); // don't be wasteful
}

struct dstate *dfa_deserialize(uint8_t *buf, size_t *size) {
  // deserialize a DFA from a **trusted** buffer. `size` is an out parameter

  uint8_t *p = buf;
  int dfa_size = leb128_get(&p);

  struct dstate *dstates[dfa_size];
  for (int id = 0; id < dfa_size; id++)
    dstates[id] = dstate_alloc(0);

  for (int id = 0; id < dfa_size; id++) {
    dstates[id]->accepting = *p >> 1 & 1;
    dstates[id]->terminating = *p++ & 1;
    for (int chr = 0; chr < 256;) {
      int len = *p++;
      struct dstate *target = dstates[leb128_get(&p)];
      do // run length
        dstates[id]->transitions[chr++] = target;
      while (len--);
    }

    if (id != 0)
      dstates[id - 1]->next = dstates[id];
  }

  *size = p - buf;
  return *dstates;
}

void dfa_dump(struct dstate *dfa) {
  (void)dfa_get_size(dfa);

  printf("graph LR\n");
  printf("  I( ) --> %d\n", dfa->id);

  for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next) {
    if (ds1->accepting)
      printf("  %d --> F( )\n", ds1->id);

    for (struct dstate *ds2 = dfa; ds2; ds2 = ds2->next) {
      bool empty = true;
      symset_t transitions = {0};
      for (int chr = 0; chr < 256; chr++)
        if (ds1->transitions[chr] == ds2)
          bitset_set(transitions, chr), empty = false;

      if (empty)
        continue;

      printf("  %d --", ds1->id);
      DUMP_SYMSET(transitions);
      printf("--> %d\n", ds2->id);
    }
  }
}

// some invariants for parsers on parse error:
// - `error` shall be set to a non-`NULL` error message
// - `regex` shall point to the error location
// - the returned NFA should be the null NFA
// - the caller is responsible for backtracking

static unsigned parse_natural(char **regex, char **error) {
  if (!isdigit(**regex)) {
    *error = "expected natural number";
    return 0;
  }

  unsigned natural = 0;
  for (; isdigit(**regex); ++*regex) {
    int digit = **regex - '0';

    if (natural > UINT_MAX / 10 || natural * 10 > UINT_MAX - digit) {
      *error = "natural number overflow";
      return UINT_MAX; // indicate overflow condition
    }

    natural *= 10, natural += digit;
  }
  return natural;
}

static uint8_t parse_hexbyte(char **regex, char **error) {
  uint8_t byte = 0;
  for (int i = 0; i < 2; i++) {
    byte <<= 4;
    if (isdigit(**regex))
      byte |= **regex - '0';
    else if (isxdigit(**regex))
      byte |= tolower(**regex) - 'a' + 10;
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
  if (**regex == '\\' && ++*regex) {
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

static void parse_shorthand(symset_t *symset, char **regex, char **error) {
  memset(symset, 0x00, sizeof(*symset));

#define RETURN_SYMSET(COND)                                                    \
  do {                                                                         \
    for (int chr = 0; chr < 256; chr++)                                        \
      if (COND)                                                                \
        bitset_set(*symset, chr);                                              \
    return;                                                                    \
  } while (0)

  if (**regex == '\\' && ++*regex) {
    switch (*(*regex)++) {
    case 'd':
      RETURN_SYMSET(isdigit(chr));
    case 'D':
      RETURN_SYMSET(!isdigit(chr));
    case 's':
      RETURN_SYMSET(isspace(chr));
    case 'S':
      RETURN_SYMSET(!isspace(chr));
    case 'w':
      RETURN_SYMSET(chr == '_' || isalnum(chr));
    case 'W':
      RETURN_SYMSET(chr != '_' && !isalnum(chr));
    }

    --*regex, --*regex;
  }

  if (**regex == '.' && ++*regex)
    RETURN_SYMSET(chr != '\n');

#undef RETURN_SYMSET

  *error = "expected shorthand class";
  return;
}

static void parse_symset(symset_t *symset, char **regex, char **error) {
  bool complement = **regex == '^' && ++*regex;

  char *last_regex = *regex;
  parse_shorthand(symset, regex, error);
  if (!*error)
    goto process_complement;
  *error = NULL;
  *regex = last_regex;

  if (**regex == '[' && ++*regex) {
    memset(symset, 0x00, sizeof(*symset));
    // hacky lookahead for better diagnostics
    while (!strchr("]", **regex)) {
      symset_t sub;
      parse_symset(&sub, regex, error);
      if (*error)
        return;

      for (int i = 0; i < sizeof(*symset); i++)
        (*symset)[i] |= sub[i];
    }

    if (**regex == ']' && ++*regex)
      goto process_complement;

    *error = "expected ']'";
    return;
  }
  *regex = last_regex;

  if (**regex == '<' && ++*regex) {
    memset(symset, 0xff, sizeof(*symset));
    // hacky lookahead for better diagnostics
    while (!strchr(">", **regex)) {
      symset_t sub;
      parse_symset(&sub, regex, error);
      if (*error)
        return;

      for (int i = 0; i < sizeof(*symset); i++)
        (*symset)[i] &= sub[i];
    }

    if (**regex == '>' && ++*regex)
      goto process_complement;

    *error = "expected '>'";
    return;
  }
  *regex = last_regex;

  uint8_t lower = parse_symbol(regex, error), upper = lower;
  if (!*error) {
    if (**regex == '-' && ++*regex) {
      upper = parse_symbol(regex, error);
      if (*error)
        return;
    }

    upper++; // open upper bound
    memset(symset, 0x00, sizeof(*symset));
    uint8_t chr = lower;
    do // character range wraparound
      bitset_set(*symset, chr);
    while (++chr != upper);
    goto process_complement;
  }
  return;

process_complement:
  if (complement)
    for (int i = 0; i < sizeof(*symset); i++)
      (*symset)[i] = ~(*symset)[i];
  return;
}

static struct nfa parse_regex(char **regex, char **error);
static struct nfa parse_atom(char **regex, char **error) {
  if (**regex == '(' && ++*regex) {
    struct nfa sub = parse_regex(regex, error);
    if (*error)
      return (struct nfa){NULL};

    if (**regex == ')' && ++*regex)
      return sub;

    *error = "expected ')'";
    return nfa_free(sub), (struct nfa){NULL};
  }

  struct nfa chars = {.initial = nstate_alloc(),
                      .final = nstate_alloc(),
                      .complemented = false,
                      .reversed = false};
  chars.initial->next = chars.final;
  chars.initial->target = chars.final, chars.final->source = chars.initial;

  parse_symset(&chars.initial->label, regex, error);
  if (*error)
    return nfa_free(chars), (struct nfa){NULL};

  return chars;
}

static struct nfa parse_factor(char **regex, char **error) {
  struct nfa atom = parse_atom(regex, error);
  if (*error)
    return (struct nfa){NULL};

  //         <---
  // -->O-->(atom)-->O-->
  //     ----------->
  if (**regex == '*' && ++*regex) {
    nfa_canonicalize(&atom);
    atom.final->epsilon1 = atom.initial, atom.initial->delta1 = atom.final;
    nfa_pad_initial(&atom), nfa_pad_final(&atom);
    atom.initial->epsilon1 = atom.final, atom.final->delta1 = atom.initial;
    return atom;
  }

  //         <---
  // -->O-->(atom)-->O-->
  if (**regex == '+' && ++*regex) {
    nfa_canonicalize(&atom);
    atom.final->epsilon1 = atom.initial, atom.initial->delta1 = atom.final;
    nfa_pad_initial(&atom), nfa_pad_final(&atom);
    return atom;
  }

  // -->(atom)-->
  //     --->
  if (**regex == '?' && ++*regex) {
    nfa_canonicalize(&atom);
    if (atom.initial->epsilon1)
      nfa_pad_initial(&atom);
    if (atom.final->delta1)
      nfa_pad_final(&atom);
    atom.initial->epsilon1 = atom.final, atom.final->delta1 = atom.initial;
    return atom;
  }

  char *last_regex = *regex;
  if (**regex == '{' && ++*regex) {
    nfa_canonicalize(&atom);
    unsigned min = parse_natural(regex, error);
    if (*error && min == UINT_MAX) // overflow condition
      return nfa_free(atom), (struct nfa){NULL};
    else if (*error)
      min = 0, *error = NULL;

    unsigned max = min;
    bool max_unbounded = false;
    if (**regex == ',' && ++*regex) {
      max = parse_natural(regex, error);
      if (*error && max == UINT_MAX) // overflow condition
        return nfa_free(atom), (struct nfa){NULL};
      else if (*error)
        max_unbounded = true, *error = NULL;
    }

    if (**regex == '}' && ++*regex)
      ;
    else {
      *error = "expected '}'";
      return nfa_free(atom), (struct nfa){NULL};
    }

    if (min > max && !max_unbounded) {
      *regex = last_regex;
      *error = "misbounded quantifier";
      return nfa_free(atom), (struct nfa){NULL};
    }

    struct nfa atoms = {.complemented = false, .reversed = false};
    atoms.initial = atoms.final = nstate_alloc();

    // if `max` is bounded, make `max` copies and add '?' epsilon transitions:
    // -->(atom)...(atom)(atom)-->
    //              --->  --->
    // if it is not, make `min + 1` copies and add one '*' epsilon transition:
    //                       <---
    // -->...(atom)(atom)-->(atom)-->O-->
    //                   ----------->
    for (unsigned i = 0; max_unbounded ? i <= min : i < max; i++) {
      struct nfa clone = nfa_clone(atom);
      if (i >= min) {
        if (max_unbounded) {
          clone.final->epsilon1 = clone.initial,
          clone.initial->delta1 = clone.final;
          nfa_pad_initial(&clone), nfa_pad_final(&clone);
        }
        clone.initial->epsilon1 = clone.final,
        clone.final->delta1 = clone.initial;
      }

      nfa_concat(&atoms, clone);

      // needed for when `min == UINT_MAX && max_unbounded`. this isn't a bodge;
      // the correct number of copies will have been made by the time we break
      if (i == UINT_MAX)
        break;
    }

    nfa_free(atom);

    return atoms;
  }

  return atom;
}

static struct nfa parse_term(char **regex, char **error) {
  bool complement = **regex == '~' && ++*regex;

  struct nfa term = {.complemented = false, .reversed = false};
  term.initial = term.final = nstate_alloc();

  // hacky lookahead for better diagnostics
  while (!strchr(")|&", **regex)) {
    struct nfa factor = parse_factor(regex, error);
    if (*error)
      return nfa_free(term), (struct nfa){NULL};

    nfa_canonicalize(&factor);
    nfa_concat(&term, factor);
  }

  if (complement)
    term.complemented = true;

  return term;
}

static struct nfa parse_regex(char **regex, char **error) {
  struct nfa re = parse_term(regex, error);
  if (*error)
    return (struct nfa){NULL};

  if (**regex == '|' || **regex == '&') {
    bool intersect = *(*regex)++ == '&';
    struct nfa alt = parse_regex(regex, error);
    if (*error)
      return nfa_free(re), (struct nfa){NULL};

    // we perform NFA intersection by rewriting into an alternation using De
    // Morgan's law `a&b == ~(~a|~b)`. this isn't nearly as inefficient as it
    // may appear, because NFA complementation is performed lazily
    re.complemented ^= intersect, alt.complemented ^= intersect;
    nfa_canonicalize(&re), nfa_canonicalize(&alt);

    // -->O-->(re)--->
    //     -->(alt)-->O-->
    nfa_pad_initial(&re), nfa_pad_final(&alt);
    re.initial->epsilon1 = alt.initial, alt.initial->delta1 = re.initial;
    re.final->epsilon1 = alt.final, alt.final->delta1 = re.final;
    re.final->next = alt.initial;
    re.final = alt.final;

    re.complemented ^= intersect;
  }

  return re;
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
    return nfa_free(nfa), (struct nfa){NULL};
  }

  return nfa;
}

struct nfa ltre_fixed_string(char *string) {
  // parses a fixed string into an NFA. never errors

  struct nfa nfa = {.complemented = false, .reversed = false};
  nfa.initial = nfa.final = nstate_alloc();

  for (; *string; string++) {
    struct nstate *initial = nfa.final;
    nfa.final = nstate_alloc();
    initial->next = nfa.final;
    initial->target = nfa.final, nfa.final->source = initial;
    bitset_set(initial->label, (uint8_t)*string);
  }

  return nfa;
}

void ltre_partial(struct nfa *nfa) {
  // enable partial matching. effectively, surround the NFA by a pair of /<>*/s
  nfa_canonicalize(nfa);
  nfa_pad_initial(nfa), nfa_pad_final(nfa);
  nfa->initial->target = nfa->initial->source = nfa->initial;
  nfa->final->target = nfa->final->source = nfa->final;
  memset(nfa->initial->label, 0xff, sizeof(nfa->initial->label));
  memset(nfa->final->label, 0xff, sizeof(nfa->final->label));
}

void ltre_ignorecase(struct nfa *nfa) {
  // enable case-insensitive matching. effectively, for any character a labeled
  // transition contains, make it also contain its swapped-case counterpart
  nfa_canonicalize(nfa);
  for (struct nstate *nstate = nfa->initial; nstate; nstate = nstate->next) {
    for (int chr = 0; chr < 256; chr++) {
      if (bitset_get(nstate->label, chr)) {
        bitset_set(nstate->label, tolower(chr));
        bitset_set(nstate->label, toupper(chr));
      }
    }
  }
}

void ltre_complement(struct nfa *nfa) {
  // complement accepted language. `dfa_step` will read this flag when marking
  // accepting states
  nfa->complemented = !nfa->complemented;
}

void ltre_reverse(struct nfa *nfa) {
  // reverse accepted language. `dfa_step` will read this flag when stepping
  // through the NFA
  nfa->reversed = !nfa->reversed;
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

// dual to `epsilon_closure`; equivalent to an epsilon-closure under reversal
static void delta_closure(struct nstate *nstate, uint8_t bitset[]) {
  if (!nstate)
    return;

  if (bitset_get(bitset, nstate->id))
    return; // already visited

  bitset_set(bitset, nstate->id);
  delta_closure(nstate->delta0, bitset);
  delta_closure(nstate->delta1, bitset);
}

static void dfa_step(struct dstate **dfap, struct dstate *dstate, uint8_t chr,
                     struct nfa nfa, int nfa_size, struct nstate *nstates[]) {
  // step the DFA `*dfap` starting from state `dstate` by consuming input
  // character `chr` in lock step according to the NFA `nfa`. `nstates` shall
  // expose the states of the NFA `nfa` as an array of state pointers of length
  // `nfa_size`. this routine creates new DFA states as needed. call initially
  // with `dstate == NULL` to create a DFA state corresponding to the epsilon-
  // closure of the NFA's initial state

  int bitset_size = (nfa_size + 7) / 8; // ceil
  uint8_t bitset_union[bitset_size];
  memset(bitset_union, 0x00, bitset_size);

  if (dstate == NULL) {
    !nfa.reversed ? epsilon_closure(nfa.initial, bitset_union)
                  : delta_closure(nfa.final, bitset_union);
  } else {
    // compute the "superposition" of NFA states reachable by consuming `chr` in
    // lock step. using the array of state pointers `nstates` speeds up this hot
    // loop by 2.5x over iterating through the states linked list using
    // `nstate->next`, probably because it helps out the prefetcher and breaks
    // the memory load dependency chain
    if (!nfa.reversed) {
      for (int id = 0; id < nfa_size; id++)
        if (bitset_get(dstate->bitset, id) &&
            bitset_get(nstates[id]->label, chr))
          epsilon_closure(nstates[id]->target, bitset_union);
    } else {
      for (int id = 0; id < nfa_size; id++)
        if (bitset_get(dstate->bitset, id) && nstates[id]->source &&
            bitset_get(nstates[id]->source->label, chr))
          delta_closure(nstates[id]->source, bitset_union);
    }
  }

  // create a DFA state whose `bitset` corresponds to this "superposition",
  // if it doesn't already exist. binary tree not necessary, linear search
  // is just as fast
  struct dstate **dstatep = dfap;
  while (*dstatep && memcmp((*dstatep)->bitset, bitset_union, bitset_size))
    dstatep = &(*dstatep)->next;

  if (!*dstatep) {
    *dstatep = dstate_alloc(bitset_size);
    memcpy((*dstatep)->bitset, bitset_union, bitset_size);

    // a DFA state is accepting if and only if it contains the NFA's final state
    // in its `bitset`. also make sure to swap accepting and non-accepting
    // states if the NFA is complemented
    (*dstatep)->accepting = bitset_get(
        bitset_union, !nfa.reversed ? nfa.final->id : nfa.initial->id);
    (*dstatep)->accepting ^= nfa.complemented;
  }

  if (dstate)
    // patch the `chr` transition to point to this new (or existing) state
    dstate->transitions[chr] = *dstatep;
}

struct dstate *ltre_compile(struct nfa nfa) {
  // fully compile DFA. powerset construction followed by DFA minimization

  int nfa_size = nfa_get_size(nfa);
  struct nstate *nstates[nfa_size];
  for (struct nstate *nstate = nfa.initial; nstate; nstate = nstate->next)
    nstates[nstate->id] = nstate;

  // construct new DFA states as we're iterating over them and patching
  // transitions, starting from the epsilon-closure of the NFA's initial state
  struct dstate *dfa = NULL;
  dfa_step(&dfa, NULL, 0, nfa, nfa_size, nstates);
  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next)
    for (int chr = 0; chr < 256; chr++)
      dfa_step(&dfa, dstate, chr, nfa, nfa_size, nstates);

  int dfa_size = dfa_get_size(dfa);
  struct dstate *dstates[dfa_size];
  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next)
    dstates[dstate->id] = dstate;

  // store distinguishability data in a symmetric matrix condensed using bitsets
  uint8_t dis[dfa_size][(dfa_size + 7) / 8]; // ceil
  memset(dis, 0x00, sizeof(dis));
#define ARE_DIS(ID1, ID2) bitset_get(dis[ID1], ID2)
#define MAKE_DIS(ID1, ID2) bitset_set(dis[ID1], ID2), bitset_set(dis[ID2], ID1)

  // flag indistinguishable states. a pair of states is indistinguishable if and
  // only if both states have the same `accepting` value and their transitions
  // are equal up to target state indistinguishability. to avoid dealing with
  // cycles, we default to all states being indistinguishable then iteratively
  // rule out the ones that aren't.
  for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next)
    for (struct dstate *ds2 = ds1->next; ds2; ds2 = ds2->next)
      if (ds1->accepting != ds2->accepting)
        MAKE_DIS(ds1->id, ds2->id);
  for (bool done = false; done = !done;)
    for (int id1 = 0; id1 < dfa_size; id1++)
      for (int id2 = id1 + 1; id2 < dfa_size; id2++)
        if (!ARE_DIS(id1, id2))
          for (int chr = 0; chr < 256; chr++)
            // use irreflexivity of distinguishability as a cheap precheck
            if (dstates[id1]->transitions[chr] !=
                dstates[id2]->transitions[chr])
              if (ARE_DIS(dstates[id1]->transitions[chr]->id,
                          dstates[id2]->transitions[chr]->id)) {
                MAKE_DIS(id1, id2), done = false;
                break;
              }

  // minimize the DFA by merging indistinguishable states. no need to prune
  // unreachable states because the powerset construction yields a DFA with
  // no unreachable states
  for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next) {
    for (struct dstate *prev = ds1; prev; prev = prev->next) {
      for (struct dstate *ds2; ds2 = prev->next;) {
        if (ARE_DIS(ds1->id, ds2->id))
          break;

        // states are indistinguishable. merge them
        for (struct dstate *dstate = dfa; dstate; dstate = dstate->next)
          for (int chr = 0; chr < 256; chr++)
            if (dstate->transitions[chr] == ds2)
              dstate->transitions[chr] = ds1;

        prev->next = ds2->next, free(ds2);
      }
    }

    // flag "terminating" states. a terminating state is a state which either
    // always or never leads to an accepting state. since `ds1` is now
    // distinguishable from all other states, it is terminating if and only if
    // all its transitions point to itself because, by definition, no other
    // state accepts the same set of words as it does (either all or none)
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

bool ltre_matches(struct dstate *dfa, uint8_t *input) {
  // time linear in the input length :)
  for (; !dfa->terminating && *input; input++)
    dfa = dfa->transitions[*input];
  return dfa->accepting;
}

bool ltre_matches_lazy(struct dstate **dfap, struct nfa nfa, uint8_t *input) {
  // Thompson's algorithm. lazily create new DFA states as we need them,
  // marching the NFA in lock step. cached DFA states are stored in `*dfap`.
  // call initially with an empty cache using `*dfap == NULL`, and make sure
  // to `dfa_free(*dfap)` when finished with this NFA

  int nfa_size = nfa_get_size(nfa);
  struct nstate *nstates[nfa_size];
  for (struct nstate *nstate = nfa.initial; nstate; nstate = nstate->next)
    nstates[nstate->id] = nstate;

  dfa_step(dfap, NULL, 0, nfa, nfa_size, nstates);

  // time linear in the input length :)
  struct dstate *dstate = *dfap;
  for (; *input; dstate = dstate->transitions[*input++])
    if (dstate->transitions[*input] == NULL)
      dfa_step(dfap, dstate, *input, nfa, nfa_size, nstates);

  return dstate->accepting;
}

bool ltre_equivalent(struct dstate *dfa1, struct dstate *dfa2) {
  // check whether `dfa1` and `dfa2` accept the same language. all DFAs in
  // LTRE are minimal and minimal DFAs are unique up to renumbering, so we
  // just have to check for a graph isomorphism that preserves the initial
  // state, accepting states, and transitions

  int dfa_size = dfa_get_size(dfa1);
  if (dfa_size != dfa_get_size(dfa2))
    return false;
  struct dstate *map[dfa_size]; // mapping of states from `dfa1` to `dfa2`
  memset(map, 0, sizeof(map)), map[dfa1->id] = dfa2;

  // come up with a tentative mapping by following transitions from the initial
  // states. the mapping will be nonesensical when the DFAs are not equivalent,
  // but that doesn't matter as long as the mapping is an isomorphism when the
  // DFAs actually are equivalent
  for (struct dstate *dstate = dfa1; dstate; dstate = dstate->next)
    for (int chr = 0; chr < 256; chr++)
      map[dstate->transitions[chr]->id] = map[dstate->id]->transitions[chr];

  // now, ensure our tentative mapping is an isomorphism
  if (map[dfa1->id] != dfa2)
    return false; // ininitial state not preserved
  for (struct dstate *dstate = dfa1; dstate; dstate = dstate->next) {
    if (map[dstate->id] == NULL)
      return false; // mapping is not a bijection
    if (dstate->accepting != map[dstate->id]->accepting)
      return false; // accepting states not preserved
    for (int chr = 0; chr < 256; chr++)
      if (map[dstate->transitions[chr]->id] !=
          map[dstate->id]->transitions[chr])
        return false; // transitions not preserved
  }

  return true;
}

struct nfa ltre_uncompile(struct dstate *dfa) {
  int dfa_size = dfa_get_size(dfa);

  struct nfa nfa = {.complemented = false, .reversed = false};
  struct nstate *tail = nfa.initial = nstate_alloc(); // to allocate new states

  struct nstate *nstates[dfa_size], *tgts[dfa_size];
  for (int id = 0; id < dfa_size; id++)
    nstates[id] = tgts[id] = tail->next = nstate_alloc(), tail = tail->next;

  nfa.initial->epsilon0 = nstates[dfa->id],
  nstates[dfa->id]->delta0 = nfa.initial;

  // a `dstate` may have incoming and outgoing labeled transitions from and to
  // multiple different states, but an `nstate` has at most one incoming and one
  // outgoing labeled transition. to bridge the gap, DFA states are mapped not
  // to a single NFA state, but to the head of a doubly-linked list of NFA
  // states formed by `epsilon0` and `epsilon1` transitions. in the diagram
  // below, (T) stands for `*tgts[id]`, the last NFA state of the doubly-linked
  // list that has an incoming labeled transition
  //               |   |    |       incoming labeled transitions
  //               V   V    V
  //            <-- <-- <--   <--    `epsilon1` transitions
  // nstates[id]-->O-->O-->(T)-->O   `epsilon0` transitions
  //               |   |    |    |
  //               V   V    V    V   outgoing labeled transitions

  for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next) {
    struct nstate *src = nstates[ds1->id];

    for (struct dstate *ds2 = dfa; ds2; ds2 = ds2->next) {
      bool empty = true;
      symset_t transitions = {0};
      for (int chr = 0; chr < 256; chr++)
        if (ds1->transitions[chr] == ds2)
          bitset_set(transitions, chr), empty = false;

      if (empty)
        continue;

      // step to the next state of the doubly-linked list to find a suitable
      // source state, or allocate a new NFA state if there is no next state
      if (src->target) {
        if (!src->epsilon0) {
          struct nstate *source;
          source = tail->next = nstate_alloc(), tail = tail->next;
          src->epsilon0 = source, source->delta0 = src;
          source->epsilon1 = src, src->delta1 = source;
        }
        src = src->epsilon0;
      }

      struct nstate *tgt = tgts[ds2->id];

      // step to the next state of the doubly-linked list to find a suitable
      // target state, or allocate a new NFA state if there is no next state
      if (tgt->source) {
        if (!tgt->epsilon0) {
          struct nstate *target;
          target = tail->next = nstate_alloc(), tail = tail->next;
          tgt->epsilon0 = target, target->delta0 = tgt;
          target->epsilon1 = tgt, tgt->delta1 = target;
        }
        tgts[ds2->id] = tgt->epsilon0;
      }

      src->target = tgt, tgt->source = src;
      memcpy(src->label, transitions, sizeof(transitions));
    }
  }

  // to model accepting states, use `epsilon1` transitions pointing to a
  // waterfall of `epsilon0` transitions pointing to `nfa.final`
  //  |   |   |                 `epsilon1` from accepting states
  //  V   V   V
  //  O-->O-->O-->(nfa.final)   `epsilon0` waterfall
  nfa.final = tail->next = nstate_alloc(), tail = tail->next;
  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next) {
    if (dstate->accepting) {
      nstates[dstate->id]->epsilon1 = nfa.final,
      nfa.final->delta1 = nstates[dstate->id];
      nfa_pad_final(&nfa);
    }
  }

  // dfa_dump(dfa);
  // nfa_dump(nfa);
  // printf("%2d -> %2d\n", dfa_size, nfa_get_size(nfa));

  return nfa;
}

// this struct is used solely during DFA-to-RE decompilation, as an intermediate
// representation for regular expression simplification using rewrite rules. we
// store /()/ (epsilon) as the empty concatenation and /[]/ (empty set) as the
// empty alternation. in rewrite rules it's useful to think of /r+/ and /r?/ as
// units, so we encode them using their own `regex_type`s
struct regex {
  enum regex_type {
    TYPE_ALT,    // access children with `REGEX_CHILDREN`
    TYPE_CONCAT, // access children with `REGEX_CHILDREN`
    TYPE_STAR,   // access child with `REGEX_CHILD`
    TYPE_PLUS,   // access child with `REGEX_CHILD`
    TYPE_OPT,    // access child with `REGEX_CHILD`
    TYPE_SYMSET, // access underlying symset with `REGEX_SYMSET`
  } type;

  // `dat` should really be this union, but a flexible array member has to be
  // the direct child of a struct. so instead, we define `unsigned char dat[]`
  // along with a few macros that cast it to the right type.
  //
  // union {
  //   struct regex *children[];
  //   struct regex *child;
  //   symset_t symset;
  // } dat;

#define REGEX_CHILDREN(REGEX) ((struct regex **)&(REGEX)->dat)
#define REGEX_CHILD(REGEX) (*REGEX_CHILDREN(REGEX))
#define REGEX_SYMSET(REGEX) (*(symset_t *)&(REGEX)->dat)
  unsigned char dat[];
};

static struct regex *regex_alloc_symset(enum regex_type type) {
  // `type` must be `TYPE_SYMSET`
  struct regex *regex = malloc(sizeof(*regex) + sizeof(REGEX_SYMSET(regex)));
  memset(REGEX_SYMSET(regex), 0x00, sizeof(REGEX_SYMSET(regex)));
  regex->type = type;
  return regex;
}

static struct regex *regex_alloc_child(enum regex_type type) {
  // `type` must be one of `TYPE_STAR`, `TYPE_PLUS`, `TYPE_OPT`
  struct regex *regex = malloc(sizeof(*regex) + sizeof(REGEX_CHILD(regex)));
  REGEX_CHILD(regex) = NULL;
  regex->type = type;
  return regex;
}

static struct regex *regex_alloc_children(enum regex_type type,
                                          int children_len) {
  // `type` must be either `TYPE_ALT` or `TYPE_CONCAT`. the `NULL` terminator of
  // the `REGEX_CHILDREN` array will be at index `children_len`
  struct regex *regex = malloc(
      sizeof(*regex) + (children_len + 1) * sizeof(*REGEX_CHILDREN(regex)));
  for (int i = 0; i < children_len + 1; i++)
    REGEX_CHILDREN(regex)[i] = NULL;
  regex->type = type;
  return regex;
}

static int regex_children_len(struct regex *regex) {
  // `regex->type` must be either `TYPE_ALT` or `TYPE_CONCAT`
  int children_len = 0;
  for (struct regex **child = REGEX_CHILDREN(regex); *child; child++)
    children_len++;
  return children_len;
}

static void regex_free(struct regex *regex) {
  switch (regex->type) {
  case TYPE_ALT:
  case TYPE_CONCAT:
    for (struct regex **child = REGEX_CHILDREN(regex); *child; child++)
      regex_free(*child);
    break;
  case TYPE_STAR:
  case TYPE_PLUS:
  case TYPE_OPT:
    regex_free(REGEX_CHILD(regex));
    break;
  case TYPE_SYMSET:
    break;
  }

  free(regex);
}

static struct regex *regex_clone(struct regex *regex) {
  struct regex *clone = NULL;

  switch (regex->type) {
  case TYPE_ALT:
  case TYPE_CONCAT:;
    int children_len = regex_children_len(regex);
    clone = regex_alloc_children(regex->type, children_len);
    for (int i = 0; i < children_len; i++)
      REGEX_CHILDREN(clone)[i] = regex_clone(REGEX_CHILDREN(regex)[i]);
    break;
  case TYPE_STAR:
  case TYPE_PLUS:
  case TYPE_OPT:
    clone = regex_alloc_child(regex->type);
    REGEX_CHILD(clone) = regex_clone(REGEX_CHILD(regex));
    break;
  case TYPE_SYMSET:
    clone = regex_alloc_symset(regex->type);
    memcpy(REGEX_SYMSET(clone), REGEX_SYMSET(regex),
           sizeof(REGEX_SYMSET(clone)));
    break;
  }

  return clone;
}

static int regex_cmp(struct regex *regex1, struct regex *regex2) {
  // returns an integer less than, equal to, or greater than zero if `regex1`
  // is, respectively, less than, equal to, or greater than `regex2`. the
  // ordering used is rather arbitrary and purposefully doesn't take into
  // account associativity and commutativity

  int cmp;
  if (cmp = regex1->type - regex2->type)
    return cmp;

  switch (regex1->type) {
  case TYPE_ALT:
  case TYPE_CONCAT:;
    struct regex **child1 = REGEX_CHILDREN(regex1),
                 **child2 = REGEX_CHILDREN(regex2);
    for (; *child1 && *child2; child1++, child2++)
      if (cmp = regex_cmp(*child1, *child2))
        return cmp;
    return !!*child1 - !!*child2;
  case TYPE_STAR:
  case TYPE_PLUS:
  case TYPE_OPT:
    if (cmp = regex_cmp(REGEX_CHILD(regex1), REGEX_CHILD(regex2)))
      return cmp;
    return 0;
  case TYPE_SYMSET:
    for (int chr = 0; chr < 256; chr++)
      if (cmp = bitset_get(REGEX_SYMSET(regex1), chr) -
                bitset_get(REGEX_SYMSET(regex2), chr))
        return cmp;
    return 0;
  }

  abort(); // should have diverged
}

static size_t regex_fmt_len(struct regex *regex, enum regex_type prec) {
  // returns the length of the string that would be produced by `regex_fmt`,
  // excluding the null terminator. this is a one-to-one mirror of `regex_fmt`,
  // except we calculate length instead of writing to a buffer

  if (regex->type == TYPE_OPT &&
      (prec == TYPE_ALT || REGEX_CHILD(regex)->type == TYPE_ALT))
    return regex_fmt_len(REGEX_CHILD(regex), prec) + 1;

  size_t len = regex->type < prec;

  switch (regex->type) {
  case TYPE_ALT:
    len += 2 * !REGEX_CHILD(regex);
    for (struct regex **child = REGEX_CHILDREN(regex); *child; child++)
      len += regex_fmt_len(*child, TYPE_ALT) + !!child[1];
    break;
  case TYPE_CONCAT:
    for (struct regex **child = REGEX_CHILDREN(regex); *child; child++) {
      struct regex **c = child;
      while (c[1] && regex_cmp(*c, c[1]) == 0)
        c++;
      int run = c + 1 - child;
      int l = run > 1 ? regex_fmt_len(*child, TYPE_SYMSET) : 0;
      len += run >= 3 || l >= 3 ? child = c, l + snprintf(NULL, 0, "{%d}", run)
                                : regex_fmt_len(*child, TYPE_CONCAT);
    }
    break;
  case TYPE_STAR:
  case TYPE_PLUS:
  case TYPE_OPT:
    len += regex_fmt_len(REGEX_CHILD(regex), TYPE_SYMSET) + 1;
    break;
  case TYPE_SYMSET:
    len += strlen(symset_fmt(REGEX_SYMSET(regex)));
    break;
  }

  return len + (regex->type < prec);
}

static char *regex_fmt(struct regex *regex, char *buf, enum regex_type prec) {
  // converts `regex` to a string in `buf`. null-terminates `buf` and returns
  // a pointer to the null terminator. call `regex_fmt_len` to know how much
  // memory to allocate for `buf`. `prec` is a lower bound on the precedence of
  // the regex to be produced; call initially with `prec = 0`

  // (r|s)? |- |r|s|
  // r? |- |r , if within an alternation
  bool opt_alt = false;
  if (regex->type == TYPE_OPT &&
      (prec == TYPE_ALT || REGEX_CHILD(regex)->type == TYPE_ALT))
    opt_alt = true, regex = REGEX_CHILD(regex);

  if (regex->type < prec)
    *buf++ = '(';

  if (opt_alt)
    *buf++ = '|';

  switch (regex->type) {
  case TYPE_ALT:
    if (!*REGEX_CHILDREN(regex))
      *buf++ = '[', *buf++ = ']';
    for (struct regex **child = REGEX_CHILDREN(regex); *child; child++) {
      buf = regex_fmt(*child, buf, TYPE_ALT);
      if (child[1])
        *buf++ = '|';
    }
    break;
  case TYPE_CONCAT:
    for (struct regex **child = REGEX_CHILDREN(regex); *child; child++) {
      // rrr... |- r{n}
      // will not rewrite 'ababab -> (ab){3}'
      struct regex **c = child;
      while (c[1] && regex_cmp(*c, c[1]) == 0)
        c++;
      int run = c + 1 - child;
      // using /{1}/ will always make the regex longer, so when we have a run of
      // length 1, avoid calling `regex_fmt` on the child and pretend like it's
      // too short to be worth using /{n}/ on. this improves performance
      int len = run == 1 ? 0 : regex_fmt(*child, buf, TYPE_SYMSET) - buf;
      // only use /{n}/ if it'll make the regex shorter (subject to the above)
      if (run >= 3 || len >= 3)
        child = c, buf += len + sprintf(buf + len, "{%d}", run);
      else
        buf = regex_fmt(*child, buf, TYPE_CONCAT);
    }
    break;
  case TYPE_STAR:
  case TYPE_PLUS:
  case TYPE_OPT:
    // here we use `prec = TYPE_SYMSET` because the direct child of a quantifier
    // can't be a quantifier, as specified by the grammar
    buf = regex_fmt(REGEX_CHILD(regex), buf, TYPE_SYMSET);
    *buf++ = "*+?"[regex->type - TYPE_STAR];
    break;
  case TYPE_SYMSET:;
    char *fmt = symset_fmt(REGEX_SYMSET(regex));
    strcpy(buf, fmt), buf += strlen(fmt);
    break;
  }

  if (regex->type < prec)
    *buf++ = ')';

  return *buf = '\0', buf;
}

static void regex_dump(struct regex *regex, int indent) {
  switch (regex->type) {
  case TYPE_ALT:
  case TYPE_CONCAT:
    printf("%*s%s {\n", indent, "", regex->type == TYPE_ALT ? "ALT" : "CONCAT");
    for (struct regex **child = REGEX_CHILDREN(regex); *child; child++)
      regex_dump(*child, indent + 2);
    printf("%*s}\n", indent, "");
    break;
  case TYPE_STAR:
  case TYPE_PLUS:
  case TYPE_OPT:;
    printf("%*s%s\n", indent, "",
           (char *[3]){"STAR", "PLUS", "OPT"}[regex->type - TYPE_STAR]);
    regex_dump(REGEX_CHILD(regex), indent + 2);
    break;
  case TYPE_SYMSET:
    printf("%*sSYMSET %s\n", indent, "", symset_fmt(REGEX_SYMSET(regex)));
    break;
  }
}

static struct regex *regex_from_str(char **regex) {
  // constructs a `struct regex` intermediate representation from a regular
  // expression string obeying the grammar below. returns a pointer one past
  // the end of the regular expression parsed. meant for internal debugging
  //
  // <regex> ::= <term> ("|" <term>)*
  // <term> ::= <factor>*
  // <factor> ::= <atom> ("*" | "+" | "?")?
  // <atom> ::= "(" <regex> ")" | "\\" <metachar>
  //          | (? any character except <metachar> ?)
  // <metachar> ::= "\\" | "*" | "+" | "?" | "(" | ")" | "|"

  struct regex *regex_ = regex_alloc_children(TYPE_ALT, 0);

  while (1) {
    struct regex *term = regex_alloc_children(TYPE_CONCAT, 0);

    while (1) {
      struct regex *atom = NULL;

      if (**regex == '(') {
        char *prev_regex = (*regex)++;
        atom = regex_from_str(regex);
        if (**regex != ')')
          *regex = prev_regex, regex_free(atom), atom = NULL;
        else
          ++*regex;
      }

      const char *metachars = "\\*+?()|";
      if (**regex && !strchr(metachars, **regex) ||
          **regex == '\\' && *++*regex && strchr(METACHARS, **regex)) {
        symset_t symset = {0};
        bitset_set(symset, **regex), ++*regex;
        atom = regex_alloc_symset(TYPE_SYMSET);
        memcpy(REGEX_SYMSET(atom), symset, sizeof(symset));
      }

      if (atom == NULL)
        break;

      struct regex *factor = atom;
      const char *quants = "*+?", *quant = strchr(quants, **regex);
      if (**regex && quant && ++*regex) {
        factor = regex_alloc_child(TYPE_STAR + (quant - quants));
        REGEX_CHILD(factor) = atom;
      }

      struct regex *concat = regex_alloc_children(TYPE_CONCAT, 2);
      REGEX_CHILDREN(concat)[0] = term;
      REGEX_CHILDREN(concat)[1] = factor;
      term = concat;
    }

    struct regex *alt = regex_alloc_children(TYPE_ALT, 2);
    REGEX_CHILDREN(alt)[0] = regex_;
    REGEX_CHILDREN(alt)[1] = term;
    regex_ = alt;

    if (**regex != '|')
      break;
    ++*regex;
  }

  return regex_;
}

static bool regex_simplify_assoc(struct regex **regex) {
  // flatten nested REGEX_CHILDREN

  bool subchildren = false;
  int subchildren_len = 0;
  for (struct regex **child = REGEX_CHILDREN(*regex); *child; child++)
    if ((*child)->type == (*regex)->type)
      subchildren_len += regex_children_len(*child), subchildren = true;

  if (!subchildren)
    return false;

  struct regex *new = regex_alloc_children(
      (*regex)->type, regex_children_len(*regex) + subchildren_len);
  struct regex **newchild = REGEX_CHILDREN(new);
  for (struct regex **child = REGEX_CHILDREN(*regex); *child; child++) {
    if ((*child)->type == (*regex)->type) {
      for (struct regex **subchild = REGEX_CHILDREN(*child); *subchild;
           subchild++)
        *newchild++ = *subchild;
      free(*child);
    } else
      *newchild++ = *child;
  }

  free(*regex), *regex = new;
  return true;
}

static bool regex_simplify_singleton(struct regex **regex) {
  // turn regex with one-element REGEX_CHILDREN into the child

  if (!*REGEX_CHILDREN(*regex) || REGEX_CHILDREN(*regex)[1])
    return false;

  struct regex *old = *regex;
  *regex = *REGEX_CHILDREN(*regex), free(old);
  return true;
}

static bool regex_simplify_nested(struct regex **regex, enum regex_type inner,
                                  enum regex_type outer, enum regex_type res) {
  // flatten regex of type `inner` whose REGEX_CHILD has type `outer` into a
  // regex of type `res`

  if ((*regex)->type != outer || REGEX_CHILD(*regex)->type != inner)
    return false;

  struct regex *old = *regex;
  *regex = REGEX_CHILD(*regex), free(old);
  (*regex)->type = res;
  return true;
}

static bool regex_simplify_adjacent(struct regex **child, enum regex_type type,
                                    enum regex_type next, enum regex_type res) {
  // turn regex of type `type` followed by regex of type `next` with equal
  // REGEX_CHILDs into one regex of type `res` with that child

  if ((*child)->type != type || child[1]->type != next)
    return false;

  if (regex_cmp(REGEX_CHILD(*child), REGEX_CHILD(child[1])) != 0)
    return false;

  child[1]->type = res;
  for (regex_free(*child); *child; child++)
    *child = child[1];
  return true;
}

static void regex_simplify(struct regex **regex) {
  // simplify `regex` in-place by iteratively applying rewrite rules until none
  // match. rewrite rules either directly decrease cost or help in decreasing
  // cost, a rough measure of regex complexity, as defined by:
  //
  // cost(r|s) = cost(r) + cost(s) + 1
  // cost(rs) = cost(r) + cost(s)
  // cost(r*) = cost(r+) = cost(r?) = cost(r) + 1
  // cost([uv]) = 1
  //
  // this is a pragmatic best-effort algorithm. it simplifies a regular
  // expression until it's not obvious how it could be simplified further.
  // in aggregate, global minima are unlikely to be attained

  // a successful rewrite rule application ends either in `goto resimplify_all`
  // or in `goto resimplify_fast`. `resimplify_all` recursively re-simplifies
  // children; when that is not needed, use `resimplify_fast` instead
resimplify_all:
  switch ((*regex)->type) {
  case TYPE_ALT:
  case TYPE_CONCAT:
    for (struct regex **child = REGEX_CHILDREN(*regex); *child; child++)
      regex_simplify(child);
    break;
  case TYPE_STAR:
  case TYPE_PLUS:
  case TYPE_OPT:
    regex_simplify(&REGEX_CHILD(*regex));
    break;
  case TYPE_SYMSET:
    break;
  }

resimplify_fast:
  switch ((*regex)->type) {
  case TYPE_ALT:
    // (r|s)|t |- r|s|t
    // r|(s|t) |- r|s|t
    // since we encode /[]/ as the empty alternation, we get for free:
    // r|[] -> r
    // []|r -> r
    if (regex_simplify_assoc(regex))
      goto resimplify_fast;

    // r |- r
    if (regex_simplify_singleton(regex))
      goto resimplify_fast;

    // r|() |- r?
    // ()|r |- r?
    for (struct regex **epsilon = REGEX_CHILDREN(*regex); *epsilon; epsilon++)
      if ((*epsilon)->type == TYPE_CONCAT && !*REGEX_CHILDREN(*epsilon)) {
        struct regex *new = regex_alloc_child(TYPE_OPT);
        for (free(*epsilon); *epsilon; epsilon++)
          *epsilon = epsilon[1];
        REGEX_CHILD(new) = *regex, *regex = new;
        goto resimplify_all;
      }

    // rs|rt |- r(s|t)
    // rs|ts |- (r|t)s
    // rs|r |- r(s|)
    // r|rt |- r(|t)
    // sr|r |- (s|)r
    // r|tr |- (|t)r
    // r|r -> r
    // will not rewrite 'r|s|(r|s)t -> (r|s)(t|)'
    for (int suffix = 0; suffix < 2; suffix++) {
      for (struct regex **child1 = REGEX_CHILDREN(*regex); *child1; child1++) {
        // if `child1` is not of TYPE_CONCAT, take its "prefix" (or "suffix") to
        // be `child1` itself
        struct regex **prefix1 =
            (*child1)->type == TYPE_CONCAT
                ? REGEX_CHILDREN(*child1) +
                      (suffix ? regex_children_len(*child1) - 1 : 0)
                : child1;

        for (struct regex **child2 = child1 + 1; *child2; child2++) {
          // if `child2` is not of TYPE_CONCAT, take its "prefix" (or "suffix")
          // to be `child2` itself
          struct regex **prefix2 =
              (*child2)->type == TYPE_CONCAT
                  ? REGEX_CHILDREN(*child2) +
                        (suffix ? regex_children_len(*child2) - 1 : 0)
                  : child2;

          if (regex_cmp(*prefix1, *prefix2) == 0) {
            struct regex *concat = regex_alloc_children(TYPE_CONCAT, 2);
            REGEX_CHILDREN(concat)[suffix ? 1 : 0] = *prefix1;

            // create an alternation of the two children, replacing their common
            // prefix with the empty concatenation /()/
            regex_free(*prefix2);
            *prefix1 = regex_alloc_children(TYPE_CONCAT, 0);
            *prefix2 = regex_alloc_children(TYPE_CONCAT, 0);
            struct regex *alt = regex_alloc_children(TYPE_ALT, 2);
            REGEX_CHILDREN(alt)[0] = *child1;
            REGEX_CHILDREN(alt)[1] = *child2;

            REGEX_CHILDREN(concat)[suffix ? 0 : 1] = alt;

            *child1 = concat;
            for (; *child2; child2++)
              *child2 = child2[1];
            goto resimplify_all;
          }
        }
      }
    }

    // these don't decrease cost but are what enable '[u]?|[v] -> [uv]?'. they
    // must be applied after the distributivity rewrite rule, because otherwise
    // we'd have 'r?|sr? -> (r|sr?)?' instead of 'r?|sr? -> r?s?'.
    // r?|s |- (r|s)?
    // r|s? |- (r|s)?
    for (struct regex **opt = REGEX_CHILDREN(*regex); *opt; opt++)
      if ((*opt)->type == TYPE_OPT) {
        struct regex *new = *opt;
        *opt = REGEX_CHILD(*opt);
        REGEX_CHILD(new) = *regex, *regex = new;
        goto resimplify_all;
      }

    // a|a* |- a*
    // a*|a |- a*
    // a|a+ |- a+
    // a+|a |- a+
    // these are already taken care of in TYPE_ALT:
    // a|a? -> (a|a)? -> a?
    // a?|a -> (a|a)? -> a?
    for (struct regex **quant = REGEX_CHILDREN(*regex); *quant; quant++)
      if ((*quant)->type == TYPE_STAR || (*quant)->type == TYPE_PLUS)
        for (struct regex **child = REGEX_CHILDREN(*regex); *child; child++)
          if (regex_cmp(REGEX_CHILD(*quant), *child) == 0) {
            for (free(*child); *child; child++)
              *child = child[1];
            goto resimplify_all;
          }

    // [u]|[v] |- [uv]
    for (struct regex **symset1 = REGEX_CHILDREN(*regex); *symset1; symset1++)
      if ((*symset1)->type == TYPE_SYMSET)
        for (struct regex **symset2 = symset1 + 1; *symset2; symset2++)
          if ((*symset2)->type == TYPE_SYMSET) {
            symset_t symset_union = {0};
            for (int chr = 0; chr < 256; chr++)
              if (bitset_get(REGEX_SYMSET(*symset1), chr) ||
                  bitset_get(REGEX_SYMSET(*symset2), chr))
                bitset_set(symset_union, chr);
            memcpy(REGEX_SYMSET(*symset1), symset_union, sizeof(symset_union));
            for (free(*symset2); *symset2; symset2++)
              *symset2 = symset2[1];
            goto resimplify_fast;
          }

    break;
  case TYPE_CONCAT:
    // (rs)t |- rst
    // r(st) |- rst
    // since we encode /()/ as the empty concatenation, we get for free:
    // r() -> r
    // ()r -> r
    if (regex_simplify_assoc(regex))
      goto resimplify_fast;

    // r |- r
    if (regex_simplify_singleton(regex))
      goto resimplify_fast;

    for (struct regex **child = REGEX_CHILDREN(*regex); *child && child[1];
         child++) {
      // r*r* |- r*
      // r*r+ |- r+
      // r*r? |- r*
      if (regex_simplify_adjacent(child, TYPE_STAR, TYPE_STAR, TYPE_STAR) ||
          regex_simplify_adjacent(child, TYPE_STAR, TYPE_PLUS, TYPE_PLUS) ||
          regex_simplify_adjacent(child, TYPE_STAR, TYPE_OPT, TYPE_STAR))
        goto resimplify_fast;

      // r+r* |- r+
      // r+r? |- r+
      if (regex_simplify_adjacent(child, TYPE_PLUS, TYPE_STAR, TYPE_PLUS) ||
          regex_simplify_adjacent(child, TYPE_PLUS, TYPE_OPT, TYPE_PLUS))
        goto resimplify_fast;

      // r?r* |- r*
      // r?r+ |- r+
      if (regex_simplify_adjacent(child, TYPE_OPT, TYPE_STAR, TYPE_STAR) ||
          regex_simplify_adjacent(child, TYPE_OPT, TYPE_PLUS, TYPE_PLUS))
        goto resimplify_fast;

      // rewrite to match rewrite rule 'rr* |- r+' below:
      // r*r |- rr*
      // will not rewrite '(ab)*ab -> ab(ab)*'
      // will not rewrite 'a(ab)*b -> ab(ab)*'
      if ((*child)->type == TYPE_STAR &&
          regex_cmp(REGEX_CHILD(*child), child[1]) == 0) {
        struct regex *temp = *child;
        *child = child[1], child[1] = temp;
      }

      // rr* |- r+
      // will not rewrite 'ab(ab)* -> (ab)+'
      if (child[1]->type == TYPE_STAR &&
          regex_cmp(*child, REGEX_CHILD(child[1])) == 0) {
        child[1]->type = TYPE_PLUS;
        for (regex_free(*child); *child; child++)
          *child = child[1];
        goto resimplify_fast;
      }

      // r+r+ |- rr+
      if ((*child)->type == TYPE_PLUS && child[1]->type == TYPE_PLUS &&
          regex_cmp(*child, child[1]) == 0) {
        struct regex *old = *child;
        *child = REGEX_CHILD(*child), free(old);
        goto resimplify_fast;
      }
    }

    // r[] |- []
    // []r |- []
    for (struct regex **empty = REGEX_CHILDREN(*regex); *empty; empty++)
      if ((*empty)->type == TYPE_ALT && !*REGEX_CHILDREN(*empty)) {
        regex_free(*regex), *regex = regex_alloc_children(TYPE_ALT, 0);
        goto resimplify_fast;
      }

    break;
  case TYPE_STAR:
  case TYPE_PLUS:
  case TYPE_OPT:
    // (r*)* |- r*
    // (r+)* |- r*
    // (r?)* |- r*
    if (regex_simplify_nested(regex, TYPE_STAR, TYPE_STAR, TYPE_STAR) ||
        regex_simplify_nested(regex, TYPE_STAR, TYPE_PLUS, TYPE_STAR) ||
        regex_simplify_nested(regex, TYPE_STAR, TYPE_OPT, TYPE_STAR))
      goto resimplify_fast;

    // (r*)+ |- r*
    // (r+)+ |- r+
    // (r?)+ |- r*
    if (regex_simplify_nested(regex, TYPE_PLUS, TYPE_STAR, TYPE_STAR) ||
        regex_simplify_nested(regex, TYPE_PLUS, TYPE_PLUS, TYPE_PLUS) ||
        regex_simplify_nested(regex, TYPE_PLUS, TYPE_OPT, TYPE_STAR))
      goto resimplify_fast;

    // (r*)? |- r*
    // (r+)? |- r*
    // (r?)? |- r?
    if (regex_simplify_nested(regex, TYPE_OPT, TYPE_STAR, TYPE_STAR) ||
        regex_simplify_nested(regex, TYPE_OPT, TYPE_PLUS, TYPE_STAR) ||
        regex_simplify_nested(regex, TYPE_OPT, TYPE_OPT, TYPE_OPT))
      goto resimplify_fast;

    // ()* |- ()
    // []* |- ()
    // ()+ |- ()
    // []+ |- []
    // ()? |- ()
    // []? |- ()
    if ((REGEX_CHILD(*regex)->type == TYPE_ALT ||
         REGEX_CHILD(*regex)->type == TYPE_CONCAT) &&
        !*REGEX_CHILDREN(REGEX_CHILD(*regex))) {
      struct regex *old = *regex;
      *regex = REGEX_CHILD(*regex), free(old);
      if ((*regex)->type == TYPE_STAR || (*regex)->type == TYPE_OPT)
        (*regex)->type = TYPE_CONCAT;
      goto resimplify_fast;
    }

    // (r*|s)* |- (r|s)*
    // (r|s*)* |- (r|s)*
    // (r+|s)* |- (r|s)*
    // (r|s+)* |- (r|s)*
    // (r*|s)+ |- (r|s)*
    // (r|s*)+ |- (r|s)*
    // (r+|s)+ |- (r|s)+
    // (r|s+)+ |- (r|s)+
    // (r*|s)? |- r*|s
    // (r|s*)? |- r|s*
    // (r+|s)? |- r*|s
    // (r|s+)? |- r|s*
    // these are already taken care of in TYPE_ALT:
    // (r?|s)* -> ((r|s)?)* -> (r|s)*
    // (r|s?)* -> ((r|s)?)* -> (r|s)*
    // (r?|s)+ -> ((r|s)?)+ -> (r|s)*
    // (r|s?)+ -> ((r|s)?)+ -> (r|s)*
    // (r?|s)? -> ((r|s)?)? -> (r|s)?
    // (r|s?)? -> ((r|s)?)? -> (r|s)?
    if (REGEX_CHILD(*regex)->type == TYPE_ALT)
      for (struct regex **child = REGEX_CHILDREN(REGEX_CHILD(*regex)); *child;
           child++)
        if ((*child)->type == TYPE_STAR || (*child)->type == TYPE_PLUS) {
          if ((*regex)->type == TYPE_PLUS)
            (*regex)->type = (*child)->type;
          else if ((*regex)->type == TYPE_OPT)
            (*child)->type = TYPE_STAR, child = regex;
          struct regex *old = *child;
          *child = REGEX_CHILD(*child), free(old);
          goto resimplify_all;
        }

    break;
  case TYPE_SYMSET:
    break;
  }
}

static struct regex *regex_from_dfa(struct dstate *dfa) {
  // convert a DFA into a `struct regex` using the classic construction,
  // turning the DFA into a GNFA stored as a matrix of `arrow`s on the stack
  // then iteratively eliminating states by rerouting transitions. within this
  // function and only within this function, we store empty /[]/ transitions
  // as null pointers so they're easier to test for

  int dfa_size = dfa_get_size(dfa);
  // also create an auxiliary state and store it at index `dfa_size`
  struct regex *arrows[dfa_size + 1][dfa_size + 1];
  for (int id = 0; id <= dfa_size; id++)
    arrows[dfa_size][id] = arrows[id][dfa_size] = NULL;

  // create an epsilon /()/ transition from the auxiliary state to the DFA's
  // initial state
  arrows[dfa_size][dfa->id] = regex_alloc_children(TYPE_CONCAT, 0);
  for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next) {
    // create epsilon /()/ transitions from the DFA's accepting states to the
    // auxiliary state
    if (ds1->accepting)
      arrows[ds1->id][dfa_size] = regex_alloc_children(TYPE_CONCAT, 0);

    for (struct dstate *ds2 = dfa; ds2; ds2 = ds2->next) {
      bool empty = true;
      symset_t transitions = {0};
      for (int chr = 0; chr < 256; chr++)
        if (ds1->transitions[chr] == ds2)
          bitset_set(transitions, chr), empty = false;

      arrows[ds1->id][ds2->id] = NULL;

      if (empty)
        continue;

      arrows[ds1->id][ds2->id] = regex_alloc_symset(TYPE_SYMSET);
      memcpy(REGEX_SYMSET(arrows[ds1->id][ds2->id]), transitions,
             sizeof(transitions));
    }
  }

  // iteratively select one state according to some heuristic and reroute all
  // its inbound and outbound transitions so as to bypass that state, while also
  // taking care of self-loops. don't ever select the auxiliary state, so that
  // when we're done the final regular expression ends up as a self-loop on the
  // auxiliary state. choosing the state that minimizes 'in-degree * out-degree'
  // seems to work okay, sa that's the heuristic we're using
  while (1) {
    int best_fit, min_cost = INT_MAX;
    for (int id1 = 0; id1 < dfa_size; id1++) {
      int in_degree = 0, out_degree = 0;
      for (int id2 = 0; id2 < dfa_size; id2++)
        in_degree += !!arrows[id2][id1], out_degree += !!arrows[id1][id2];

      if (in_degree + out_degree == 0)
        continue; // state has already been processed

      int cost = in_degree * out_degree;
      if (cost <= min_cost)
        min_cost = cost, best_fit = id1;
    }

    if (min_cost == INT_MAX)
      break; // all states have been processed

    // iterate through all pairs of inbound and outbound transitions, including
    // those to and from the auxiliary state
    for (int id1 = 0; id1 <= dfa_size; id1++) {
      if (id1 == best_fit || !arrows[id1][best_fit])
        continue; // inbound transition doesn't exist or is a self-loop

      for (int id2 = 0; id2 <= dfa_size; id2++) {
        if (id2 == best_fit || !arrows[best_fit][id2])
          continue; // outbound transition doesn't exist or is a self-loop

        // construct /(self)*/
        struct regex *star;
        if (!arrows[best_fit][best_fit])
          star = regex_alloc_children(TYPE_CONCAT, 0); // /[]*/ == /()/
        else {
          star = regex_alloc_child(TYPE_STAR);
          REGEX_CHILD(star) = regex_clone(arrows[best_fit][best_fit]);
        }

        // construct /(inbound)(self)*(outbound)/
        struct regex *concat = regex_alloc_children(TYPE_CONCAT, 3);
        REGEX_CHILDREN(concat)[0] = regex_clone(arrows[id1][best_fit]);
        REGEX_CHILDREN(concat)[1] = star;
        REGEX_CHILDREN(concat)[2] = regex_clone(arrows[best_fit][id2]);

        // construct /existing|(inbound)(self)*(outbound)/
        struct regex *alt;
        if (!arrows[id1][id2])
          alt = concat; // /(re)|[]/ == /(re)/
        else {
          alt = regex_alloc_children(TYPE_ALT, 2);
          REGEX_CHILDREN(alt)[0] = arrows[id1][id2];
          REGEX_CHILDREN(alt)[1] = concat;
        }

        arrows[id1][id2] = alt;
      }
    }

    // all transitions going through the best-fit state have been rerouted, so
    // replace them all with empty /[]/ transitions, effectively eliminating
    // the state by isolating it
    for (int id = 0; id <= dfa_size; id++) {
      if (arrows[id][best_fit])
        regex_free(arrows[id][best_fit]), arrows[id][best_fit] = NULL;
      if (arrows[best_fit][id])
        regex_free(arrows[best_fit][id]), arrows[best_fit][id] = NULL;
    }
  }

  // the final regular expression ends up as a self-loop on the auxiliary state
  struct regex *regex = arrows[dfa_size][dfa_size];
  regex = regex ? regex : regex_alloc_children(TYPE_ALT, 0);
  return regex;
}

char *ltre_decompile(struct dstate *dfa) {
  struct regex *regex = regex_from_dfa(dfa);
  // regex_dump(regex, 0);

  regex_simplify(&regex);
  // regex_dump(regex, 0);

  char *buf = malloc(regex_fmt_len(regex, 0) + 1);
  (void)regex_fmt(regex, buf, 0), regex_free(regex);

  return buf;
}
