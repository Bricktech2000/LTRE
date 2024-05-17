#include "ltre.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define INITIAL 0 // DFA initial state

struct nfa_state {
  struct nfa_state_ll *transitions[256];
  struct nfa_state_ll *epsilon;
  bool accepting;
};

// set of states
struct nfa_state_ll {
  struct nfa_state_ll *next;
  struct nfa_state *state;
  int len;
};

struct nfa {
  struct nfa_state *initial;
  struct nfa_state_ll *states; // owns all states. anything else borrows
};

// set of sets of states (set of metastates)
struct nfa_state_ll_ll {
  struct nfa_state_ll_ll *next;
  struct nfa_state_ll *ll;
  struct nfa_state *state; // for powerset construction
  int len;
};

void nfa_state_ll_free(struct nfa_state_ll *ll, bool owns_state);
void nfa_state_free(struct nfa_state *state) {
  for (int chr = 0; chr < 256; chr++)
    nfa_state_ll_free(state->transitions[chr], false);
  nfa_state_ll_free(state->epsilon, false);
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

void nfa_state_ll_ll_free(struct nfa_state_ll_ll *ll_ll, bool owns_state,
                          bool owns_ll, bool owns_ll_state) {
  assert(!owns_ll_state || owns_ll); // owns_ll_state implies owns_ll

  while (ll_ll) {
    struct nfa_state_ll_ll *next = ll_ll->next;
    if (owns_ll)
      nfa_state_ll_free(ll_ll->ll, owns_ll_state);
    if (owns_state)
      nfa_state_free(ll_ll->state);
    free(ll_ll);
    ll_ll = next;
  }
}

void nfa_free(struct nfa nfa) { nfa_state_ll_free(nfa.states, true); }

struct nfa_state *nfa_state_alloc(void) {
  struct nfa_state *state = malloc(sizeof(struct nfa_state));
  for (int chr = 0; chr < 256; chr++)
    state->transitions[chr] = NULL;
  state->epsilon = NULL;
  state->accepting = false;
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

struct nfa nfa_from_pattern(char *pattern) {
  // TODO

  struct nfa nfa = {.initial = NULL, .states = NULL};

  // struct nfa_state *state0 = nfa_state_alloc();
  // nfa_state_ll_prepend(&nfa.states, state0);
  // struct nfa_state *state1 = nfa_state_alloc();
  // nfa_state_ll_prepend(&nfa.states, state1);
  // struct nfa_state *state2 = nfa_state_alloc();
  // nfa_state_ll_prepend(&nfa.states, state2);
  // struct nfa_state *state3 = nfa_state_alloc();
  // nfa_state_ll_prepend(&nfa.states, state3);
  //
  // nfa_state_ll_prepend(&state0->epsilon, state1);
  // nfa_state_ll_prepend(&state1->transitions['a'], state1);
  // nfa_state_ll_prepend(&state1->transitions['a'], state2);
  // nfa_state_ll_prepend(&state1->transitions['b'], state2);
  // nfa_state_ll_prepend(&state2->transitions['a'], state2);
  // nfa_state_ll_prepend(&state2->transitions['a'], state0);
  // nfa_state_ll_prepend(&state2->transitions['b'], state3);
  // nfa_state_ll_prepend(&state3->transitions['b'], state1);
  // state0->accepting = true;
  // nfa.initial = state0;

  // a(ba|aaa)*(ab)?a*
  struct nfa_state *state0 = nfa_state_alloc();
  nfa_state_ll_prepend(&nfa.states, state0);
  struct nfa_state *state1 = nfa_state_alloc();
  nfa_state_ll_prepend(&nfa.states, state1);
  struct nfa_state *state2 = nfa_state_alloc();
  nfa_state_ll_prepend(&nfa.states, state2);
  struct nfa_state *state3 = nfa_state_alloc();
  nfa_state_ll_prepend(&nfa.states, state3);

  nfa_state_ll_prepend(&state0->transitions['a'], state1);
  nfa_state_ll_prepend(&state1->transitions['b'], state0);
  nfa_state_ll_prepend(&state1->transitions['a'], state2);
  nfa_state_ll_prepend(&state1->epsilon, state3);
  nfa_state_ll_prepend(&state2->transitions['a'], state0);
  nfa_state_ll_prepend(&state2->transitions['b'], state3);
  nfa_state_ll_prepend(&state3->transitions['a'], state3);
  state3->accepting = true;
  nfa.initial = state0;

  // struct nfa_state *state0 = nfa_state_alloc();
  // nfa_state_ll_prepend(&nfa.states, state0);
  // struct nfa_state *state1 = nfa_state_alloc();
  // nfa_state_ll_prepend(&nfa.states, state1);
  //
  // nfa_state_ll_prepend(&state0->transitions['b'], state1);
  // nfa_state_ll_prepend(&state0->transitions['c'], state1);
  // nfa_state_ll_prepend(&state1->transitions['d'], state0);
  // state1->accepting = true;
  // nfa.initial = state0;

  return nfa;
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

struct dfa dfa_from_nfa(struct nfa nfa) {
  // powerset construction. time complexity is probably cubic or quartic in the
  // number of states, but that's fine for now

  struct nfa_state_ll_ll *metastates =
      nfa_state_ll_ll_alloc(nfa_state_epsilon_closure(nfa.initial));

  // dangerously modify the metastates linked list while iterating over it. for
  // every metastate, for every possible input character, compute the set of all
  // states we could reach by consuming that character. turn that set of states
  // into a metastate and add it to `metastates`. blindly repeat until we run
  // out of new metastates to create
  for (bool done = false; !done;) {
    done = true;
    for (struct nfa_state_ll_ll *ll_ll = metastates; ll_ll;
         ll_ll = ll_ll->next) {
      // we've assigned this metastate a state already and therefore to all
      // subsequent ones too, hopefully
      if (ll_ll->state)
        break;
      ll_ll->state = nfa_state_alloc();
      done = false;

      for (int chr = 0; chr < 256; chr++) {
        struct nfa_state_ll *transition_union = NULL;
        bool accepting_union = false;

        // union of transitions and accepting states
        for (struct nfa_state_ll *ll = ll_ll->ll; ll; ll = ll->next) {
          accepting_union |= ll->state->accepting;
          struct nfa_state_ll *chr_closure =
              nfa_state_chr_closure(ll->state, chr);
          for (struct nfa_state_ll *ll = chr_closure; ll; ll = ll->next)
            if (!nfa_state_ll_get(transition_union, ll->state))
              nfa_state_ll_prepend(&transition_union, ll->state);
        }

        // if the metastate formed by the union of transitions already exists
        // in `metastates`, point to it. this is necessary because we perform
        // shallow comparisons later
        struct nfa_state_ll *existing_metastate =
            nfa_state_ll_ll_get(metastates, transition_union);
        if (existing_metastate != (void *)-1)
          nfa_state_ll_free(transition_union, false);
        else
          nfa_state_ll_ll_prepend(&metastates, transition_union),
              existing_metastate = transition_union;

        // build transition to the metastate
        assert(ll_ll->state->transitions[chr] == NULL); // sanity check
        ll_ll->state->transitions[chr] = existing_metastate;
        ll_ll->state->accepting = accepting_union;
      }
    }
  }

  struct dfa dfa = {.size = metastates->len};
  dfa.transitions = malloc(sizeof(dfa_state_t[256]) * dfa.size);
  dfa.accepting = malloc(sizeof(bool) * dfa.size);

  for (struct nfa_state_ll_ll *ll_ll = metastates; ll_ll; ll_ll = ll_ll->next) {
    assert(ll_ll->state->epsilon == NULL);

    // funny indexes to ensure `nfa.initial` matches with
    // `dfa.transitions[INITIAL]`
    int i = ll_ll->len - 1;
    if (i == INITIAL)
      assert(ll_ll->len == 1);

    dfa.accepting[i] = ll_ll->state->accepting;

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

  nfa_free(nfa);
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
