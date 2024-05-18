#include "ltre.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL 0                 // DFA initial state
#define METACHARS "\\.^$*+?[]()|" // for parser

struct nfa_state {
  struct nfa_state_ll *transitions[256];
  struct nfa_state_ll *epsilon;
};

// set of states
struct nfa_state_ll {
  struct nfa_state_ll *next;
  struct nfa_state *state;
  int len;
};

// we support epsilon transitions and therefore can assume NFAs have a unique
// final state without loss of generality. allows us to reuse this struct for
// the regular expression parser
struct nfa {
  struct nfa_state *initial;
  struct nfa_state *final;
  struct nfa_state_ll *states; // owns all states. anything else borrows
};

// set of sets of states (set of metastates)
struct nfa_state_ll_ll {
  struct nfa_state_ll_ll *next;
  struct nfa_state_ll *ll;
  struct nfa_state *state; // bodged in, for powerset construction
  int len;
};

void nfa_state_ll_free(struct nfa_state_ll *ll, bool owns_state);
void nfa_state_free(struct nfa_state *state) {
  for (int chr = 0; chr < 256; chr++)
    nfa_state_ll_free(state->transitions[chr], false);
  nfa_state_ll_free(state->epsilon, false);
  free(state);
}

void nfa_state_ll_free(struct nfa_state_ll *ll, bool owns_state) {
  while (ll) {
    struct nfa_state_ll *next = ll->next;
    if (owns_state)
      nfa_state_free(ll->state);
    free(ll);
    ll = next;
  }
}

void nfa_state_ll_ll_free(struct nfa_state_ll_ll *ll_ll, bool owns_ll,
                          bool owns_ll_state) {
  // will NOT free `ll_ll->state`s

  assert(!owns_ll_state || owns_ll); // owns_ll_state implies owns_ll

  while (ll_ll) {
    struct nfa_state_ll_ll *next = ll_ll->next;
    if (owns_ll)
      nfa_state_ll_free(ll_ll->ll, owns_ll_state);
    (void)ll_ll->state; // do not free
    free(ll_ll);
    ll_ll = next;
  }
}

struct nfa_state *nfa_state_alloc(void) {
  struct nfa_state *state = malloc(sizeof(struct nfa_state));
  for (int chr = 0; chr < 256; chr++)
    state->transitions[chr] = NULL;
  state->epsilon = NULL;
  return state;
}

void nfa_state_ll_prepend(struct nfa_state_ll **ll, struct nfa_state *state) {
  struct nfa_state_ll *new = malloc(sizeof(struct nfa_state_ll));
  new->state = state;
  new->next = *ll;
  new->len = *ll ? (*ll)->len : 0;
  new->len++;
  *ll = new;
}

void nfa_state_ll_join(struct nfa_state_ll **ll, struct nfa_state_ll *other) {
  // reverses order of `other`. moves out of `other`
  while (other) {
    struct nfa_state_ll *next = other->next;
    other->next = *ll;
    other->len = *ll ? (*ll)->len : 0;
    other->len++;
    *ll = other;
    other = next;
  }
}

struct nfa_state_ll *nfa_state_ll_alloc(struct nfa_state *state) {
  struct nfa_state_ll *ll = NULL;
  nfa_state_ll_prepend(&ll, state);
  return ll;
}

void nfa_state_ll_ll_prepend(struct nfa_state_ll_ll **ll_ll,
                             struct nfa_state_ll *ll) {
  struct nfa_state_ll_ll *new = malloc(sizeof(struct nfa_state_ll_ll));
  new->ll = ll;
  new->next = *ll_ll;
  new->state = NULL; // to be assigned during powerset construction
  new->len = *ll_ll ? (*ll_ll)->len : 0;
  new->len++;
  *ll_ll = new;
}

struct nfa_state_ll_ll *nfa_state_ll_ll_alloc(struct nfa_state_ll *ll) {
  struct nfa_state_ll_ll *ll_ll = NULL;
  nfa_state_ll_ll_prepend(&ll_ll, ll);
  return ll_ll;
}

struct nfa_state *nfa_state_ll_get(struct nfa_state_ll *ll,
                                   struct nfa_state *state) {
  // shallow. returns first match
  for (; ll; ll = ll->next)
    if (ll->state == state)
      return ll->state;

  return NULL;
}

bool nfa_state_ll_seteq(struct nfa_state_ll *ll1, struct nfa_state_ll *ll2) {
  // shallow. assumes no duplicates

  int ll1_size = ll1 ? ll1->len : 0;
  int ll2_size = ll2 ? ll2->len : 0;

  if (ll1_size != ll2_size)
    return false;

  for (; ll1; ll1 = ll1->next) {
    bool found = false;
    for (struct nfa_state_ll *ll = ll2; ll; ll = ll->next)
      if (ll1->state == ll->state) {
        found = true;
        break;
      }
    if (!found)
      return false;
  }
  return true;
}

struct nfa_state_ll *nfa_state_ll_ll_get(struct nfa_state_ll_ll *ll_ll,
                                         struct nfa_state_ll *ll) {
  // deep for ll but shallow for state. assumes no duplicates in ll
  for (; ll_ll; ll_ll = ll_ll->next)
    if (nfa_state_ll_seteq(ll_ll->ll, ll))
      return ll_ll->ll;

  // `NULL` is an empty linked list, so we use `-1` to indicate not found
  return (void *)-1;
}

void dfa_dump(struct dfa dfa) {
  printf("graph LR\n");
  printf("  I( ) --> %d\n", INITIAL);

  for (dfa_state_t state = 0; state < dfa.size; state++) {
    if (dfa.accepting[state])
      printf("  %d --> F( )\n", state);

    for (dfa_state_t next = 0; next < dfa.size; next++) {
      char buf[256 + 1];
      char *bufp = buf;
      int first, last = '\0';

      for (int chr = ' '; chr <= '~'; chr++) {
        // avoid breaking Mermaid
        if (chr == ' ' || chr == '"' || chr == '-')
          continue;

        // print ranges
        if (dfa.transitions[state][chr] == next) {
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
      printf("  %d -- %s --> %d\n", state, buf, next);
    }
  }
}

void dfa_free(struct dfa dfa) {
  free(dfa.accepting);
  free(dfa.transitions);
}

// some invariants parsers on `error`:
// - the NFA returned shall be null
// - `input` shall point to the error location
// - the caller is responsible for backtracking if necessary

uint8_t parse_literal(char **input, char **error);
struct nfa parse_class(char **input, char **error) {
  struct nfa lits = {.initial = nfa_state_alloc(), .final = nfa_state_alloc()};
  nfa_state_ll_prepend(&lits.states, lits.initial);
  nfa_state_ll_prepend(&lits.states, lits.final);

  bool invert = false;
  if (**input == '^') {
    ++*input;
    invert = true;
  }

  char *last_input = *input;
  while (1) {
    last_input = *input;
    uint8_t begin = parse_literal(input, error);
    if (*error)
      break;

    uint8_t end = begin;
    if (**input == '-') {
      ++*input;
      end = parse_literal(input, error);
      if (!*error && begin > end)
        *error = "invalid range";
      if (*error) {
        nfa_state_ll_free(lits.states, true);
        return (struct nfa){0};
      }
    }

    for (int chr = begin; chr <= end; chr++)
      nfa_state_ll_prepend(&lits.initial->transitions[chr], lits.final);
  }
  if (strchr("]", *last_input)) { // hacky lookahead for better diagnostics
    // backtrack
    *input = last_input;
    *error = NULL;
  }
  if (*error) {
    nfa_state_ll_free(lits.states, true);
    return (struct nfa){0};
  }

  if (invert) {
    for (int chr = 0; chr < 256; chr++)
      if (lits.initial->transitions[chr])
        // more performant than `nfa_state_ll_free(..., false);` because
        // we know `next` is `NULL`
        free(lits.initial->transitions[chr]),
            lits.initial->transitions[chr] = NULL;
      else
        nfa_state_ll_prepend(&lits.initial->transitions[chr], lits.final);
  }

  return lits;
}

uint8_t parse_hexbyte(char **input, char **error) {
  uint8_t byte = 0;
  for (int i = 0; i < 2; i++, byte <<= 4) {
    uint8_t chr = **input;
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
    ++*input;
  }
  return byte;
}

uint8_t parse_escaped(char **input, char **error) {
  switch (*(*input)++) {
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
    uint8_t chr = parse_hexbyte(input, error);
    if (*error)
      return chr;
    return chr;
  default:
    if (strchr(METACHARS, *(*input - 1)))
      return *(*input - 1);
  }
  *error = "unknown escape sequence";
  return 0;
}

uint8_t parse_literal(char **input, char **error) {
  if (**input == '\\') {
    ++*input;
    uint8_t escaped = parse_escaped(input, error);
    if (*error)
      return escaped;

    return escaped;
  }

  if (**input == '\0') {
    *error = "expected literal";
    return 0;
  }

  if (strchr(METACHARS, **input)) {
    *error = "unexpected metacharacter";
    return 0;
  }

  if (**input >= ' ' && **input <= '~')
    return *(*input)++;

  *error = "invalid character";
  return 0;
}

struct nfa parse_regex(char **input, char **error);
struct nfa parse_atom(char **input, char **error) {
  if (**input == '(') {
    ++*input;
    struct nfa regex = parse_regex(input, error);
    if (*error)
      return regex;

    if (**input != ')') {
      *error = "expected ')'";
      nfa_state_ll_free(regex.states, true);
      return (struct nfa){0};
    }

    ++*input;
    return regex;
  }

  if (**input == '[') {
    ++*input;
    struct nfa class = parse_class(input, error);
    if (*error)
      return class;

    if (**input != ']') {
      *error = "expected ']'";
      nfa_state_ll_free(class.states, true);
      return (struct nfa){0};
    }

    ++*input;
    return class;
  }

  if (**input == '.') {
    ++*input;
    struct nfa nfa = {.initial = nfa_state_alloc(), .final = nfa_state_alloc()};
    nfa_state_ll_prepend(&nfa.states, nfa.initial);
    nfa_state_ll_prepend(&nfa.states, nfa.final);
    // use `[^]` to match any character
    for (int chr = 0; chr < 256; chr++)
      if (chr != '\n')
        nfa_state_ll_prepend(&nfa.initial->transitions[chr], nfa.final);
    return nfa;
  }

  // TODO implement `^` and `$`

  uint8_t literal = parse_literal(input, error);
  if (*error)
    return (struct nfa){0};

  struct nfa nfa = {.initial = nfa_state_alloc(), .final = nfa_state_alloc()};
  nfa_state_ll_prepend(&nfa.states, nfa.initial);
  nfa_state_ll_prepend(&nfa.states, nfa.final);
  nfa_state_ll_prepend(&nfa.initial->transitions[literal], nfa.final);
  return nfa;
}

struct nfa parse_term(char **input, char **error) {
  struct nfa atom = parse_atom(input, error);
  if (*error)
    return atom;

  if (**input == '*') {
    ++*input;
    nfa_state_ll_prepend(&atom.final->epsilon, atom.initial);
    nfa_state_ll_prepend(&atom.initial->epsilon, atom.final);
    return atom;
  }

  if (**input == '?') {
    ++*input;
    nfa_state_ll_prepend(&atom.initial->epsilon, atom.final);
    return atom;
  }

  if (**input == '+') {
    ++*input;
    nfa_state_ll_prepend(&atom.final->epsilon, atom.initial);
    return atom;
  }

  return atom;
}

struct nfa parse_regex(char **input, char **error) {
  struct nfa_state *initial = nfa_state_alloc();
  struct nfa terms = {.initial = initial, .final = initial};
  nfa_state_ll_prepend(&terms.states, initial);

  char *last_input = *input;
  while (1) {
    last_input = *input;
    struct nfa term = parse_term(input, error);
    if (*error)
      break;

    nfa_state_ll_join(&terms.states, term.states);
    nfa_state_ll_prepend(&terms.final->epsilon, term.initial);
    terms.final = term.final;
  }
  if (strchr(")|", *last_input)) { // hacky lookahead for better diagnostics
    // backtrack
    *input = last_input;
    *error = NULL;
  }
  if (*error) {
    nfa_state_ll_free(terms.states, true);
    return (struct nfa){0};
  }

  if (**input == '|') {
    ++*input;
    struct nfa regex = parse_regex(input, error);
    if (*error) {
      nfa_state_ll_free(terms.states, true);
      return regex;
    }

    struct nfa nfa = {.initial = nfa_state_alloc(), .final = nfa_state_alloc()};
    nfa_state_ll_prepend(&nfa.states, nfa.initial);
    nfa_state_ll_prepend(&nfa.states, nfa.final);
    nfa_state_ll_join(&nfa.states, terms.states);
    nfa_state_ll_join(&nfa.states, regex.states);
    nfa_state_ll_prepend(&nfa.initial->epsilon, terms.initial);
    nfa_state_ll_prepend(&nfa.initial->epsilon, regex.initial);
    nfa_state_ll_prepend(&terms.final->epsilon, nfa.final);
    nfa_state_ll_prepend(&regex.final->epsilon, nfa.final);
    return nfa;
  }

  return terms;
}

struct nfa nfa_from_pattern(char *pattern) {
  char *error = NULL;
  char *input = pattern;
  struct nfa regex = parse_regex(&input, &error);
  if (!error && *input != '\0')
    error = "expected end of pattern";

  if (error) {
    fprintf(stderr, "ltre: error: %s near '%s'\n", error, input);
    exit(EXIT_FAILURE);
  }

  assert(*input == '\0'); // sanity check
  return regex;
}

struct dfa dfa_random(dfa_state_t size) {
  struct dfa dfa = {.size = size};

  dfa.transitions = malloc(sizeof(dfa_state_t[256]) * dfa.size);
  for (dfa_state_t state = 0; state < dfa.size; state++)
    for (int chr = 0; chr < 256; chr++)
      dfa.transitions[state][chr] = rand() % dfa.size;

  dfa.accepting = malloc(sizeof(bool) * dfa.size);
  for (dfa_state_t state = 0; state < dfa.size; state++)
    dfa.accepting[state] = rand() % 2;

  return dfa;
}

struct nfa_state_ll *nfa_state_epsilon_closure(struct nfa_state *state) {
  // depth-first epsilon closure

  struct nfa_state_ll *epsilon_closure = NULL;
  struct nfa_state_ll *top = nfa_state_ll_alloc(state);

  while (top) {
    struct nfa_state *state = top->state;
    struct nfa_state_ll *next = top->next;
    free(top);
    top = next;

    if (nfa_state_ll_get(epsilon_closure, state))
      continue;
    nfa_state_ll_prepend(&epsilon_closure, state);

    for (struct nfa_state_ll *ll = state->epsilon; ll; ll = ll->next)
      if (!nfa_state_ll_get(top, ll->state))
        nfa_state_ll_prepend(&top, ll->state);
  }

  return epsilon_closure;
}

struct nfa_state_ll *nfa_state_chr_closure(struct nfa_state *state,
                                           uint8_t chr) {
  // `chr`-then-epsilon closure

  struct nfa_state_ll *chr_closure = NULL;

  for (struct nfa_state_ll *ll = state->transitions[chr]; ll; ll = ll->next) {
    struct nfa_state_ll *epsilon_closure = nfa_state_epsilon_closure(ll->state);
    for (struct nfa_state_ll *ll = epsilon_closure; ll; ll = ll->next)
      if (!nfa_state_ll_get(chr_closure, ll->state))
        nfa_state_ll_prepend(&chr_closure, ll->state);
    nfa_state_ll_free(epsilon_closure, false);
  }

  return chr_closure;
}

struct dfa dfa_from_nfa(struct nfa nfa) {
  // powerset construction. time complexity is probably cubic or quartic in the
  // number of states, but that's fine for now

  struct nfa_state_ll_ll *metastates =
      nfa_state_ll_ll_alloc(nfa_state_epsilon_closure(nfa.initial));
  nfa.initial = NULL; // no longer needed

  // dangerously modify the metastates linked list while iterating over it. for
  // every metastate, for every possible input character, compute the set of all
  // states we could reach by consuming that character. turn that set of states
  // into a metastate and add it to `metastates`. blindly repeat until we run
  // out of new metastates to create
  while (!metastates->state) {
    for (struct nfa_state_ll_ll *ll_ll = metastates; ll_ll;
         ll_ll = ll_ll->next) {
      // we've assigned this metastate a state already and therefore to all
      // subsequent ones too, hopefully
      if (ll_ll->state)
        break;
      ll_ll->state = nfa_state_alloc();

      for (int chr = 0; chr < 256; chr++) {
        struct nfa_state_ll *transition_union = NULL;

        // union of transitions
        for (struct nfa_state_ll *ll = ll_ll->ll; ll; ll = ll->next) {
          struct nfa_state_ll *chr_closure =
              nfa_state_chr_closure(ll->state, chr);
          for (struct nfa_state_ll *ll = chr_closure; ll; ll = ll->next)
            if (!nfa_state_ll_get(transition_union, ll->state))
              nfa_state_ll_prepend(&transition_union, ll->state);
          nfa_state_ll_free(chr_closure, false);
        }

        // if the metastate formed by the union of transitions already exists
        // in `metastates`, point to it. this is necessary because we perform
        // shallow comparisons later
        struct nfa_state_ll *existing_metastate =
            nfa_state_ll_ll_get(metastates, transition_union);
        if (existing_metastate != (void *)-1)
          nfa_state_ll_free(transition_union, false);
        else
          nfa_state_ll_ll_prepend(&metastates,
                                  existing_metastate = transition_union);

        // build transition to the metastate
        assert(ll_ll->state->transitions[chr] == NULL); // sanity check
        ll_ll->state->transitions[chr] = existing_metastate;
      }
    }
  }

  struct dfa dfa = {.size = metastates->len};
  dfa.transitions = malloc(sizeof(dfa_state_t[256]) * dfa.size);
  dfa.accepting = malloc(sizeof(bool) * dfa.size);

  for (struct nfa_state_ll_ll *ll_ll = metastates; ll_ll; ll_ll = ll_ll->next) {
    assert(ll_ll->state->epsilon == NULL);

    // funny index to ensure the initial state in `metastates` ends up at
    // `dfa.transitions[INITIAL]`
    int i = ll_ll->len - 1;

    dfa.accepting[i] = nfa_state_ll_get(ll_ll->ll, nfa.final) != NULL;

    for (int chr = 0; chr < 256; chr++) {
      struct nfa_state_ll *ll = ll_ll->state->transitions[chr];

      // replace pointers to metastates for indices within `dfa.transitions`
      dfa.transitions[i][chr] = -1;
      for (struct nfa_state_ll_ll *ll_ll = metastates; ll_ll;
           ll_ll = ll_ll->next)
        if (ll_ll->ll == ll) // shallow
          dfa.transitions[i][chr] = ll_ll->len - 1;
      assert(dfa.transitions[i][chr] != -1);
    }
  }

  // `metastates` owns its states shallowly; it doesn't own the states'
  // `transitions` linked list or `epsilon` linked list. that's because, at this
  // stage, a `transitions` linked list is actually a pointer to some other
  // existing metastate. therefore, perform a manual shallow `free` here
  for (struct nfa_state_ll_ll *ll_ll = metastates; ll_ll; ll_ll = ll_ll->next)
    free(ll_ll->state);

  nfa_state_ll_ll_free(metastates, true, false);
  nfa_state_ll_free(nfa.states, true);

  return dfa;
}

struct dfa ltre_compile(char *pattern) {
  struct nfa nfa = nfa_from_pattern(pattern);
  struct dfa dfa = dfa_from_nfa(nfa);
  return dfa;
}

bool ltre_matches(struct dfa dfa, uint8_t *input) {
  // time linear in the input length :)
  dfa_state_t state = INITIAL;
  for (; *input; input++)
    state = dfa.transitions[state][*input];
  return dfa.accepting[state];
}
