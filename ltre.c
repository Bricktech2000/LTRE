#include "ltre.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\.-^$*+?{}[]<>()|&~"
#define C_ESCAPES "abfnrtv"
#define C_CODEPTS "\a\b\f\n\r\t\v"

// TODO update mentions of "parse to NFA": README.md, ../WRITEUP.md

// on x86_64, `unsigned` saves us a `cdqe` instruction over `int` in the
// fragment `id / 8`
static bool bitset_get(uint8_t bitset[], unsigned idx) {
  return bitset[idx / 8] & 1 << idx % 8;
}

static void bitset_set(uint8_t bitset[], unsigned idx) {
  bitset[idx / 8] |= 1 << idx % 8;
}

static void bitset_clr(uint8_t bitset[], unsigned idx) {
  bitset[idx / 8] &= ~(1 << idx % 8);
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

    char *codept, *codepts = C_CODEPTS;
    if (chr && (codept = strchr(codepts, chr)))
      *(*p)++ = '\\', *(*p)++ = C_ESCAPES[codept - codepts];
    else if (!isprint(chr) && !is_metachar)
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
  if (nsym == 0) {
    return "[]";
  } else if (nnsym == 0) {
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

// `struct regex` forms a persistent data structure with acyclic structural
// sharing. all regexes are immutable and `refcount`ed.

// XXX copy over the doc comment from NU-RE (and modify it)
// XXX doc ensure `-fsmall-enums` for packing
// XXX doc upper = UINT_MAX means unbounded
// XXX "wait so why is it that we're using arrays for ALT and CONCAT? why not
// just use linked lists (aka normalize associativity)?"
// XXX doc regex functions move in the regex unless otherwise specified
struct regex {
  enum regex_type {
    TYPE_ALT,    // r|s
    TYPE_COMPL,  // ~r
    TYPE_CONCAT, // rs
    TYPE_REPEAT, // r* r+ r? r{m,n}
    TYPE_SYMSET, // a-b [uv] <uv> ^u
  } type;
  bool sym_incl;
  // a regular expression is nullable if and only if it accepts the empty word.
  // this field is initialized in the smart constructors
  bool nullable;
  // `struct regex` forms a persistent data structure with acyclic structural
  // sharing.

  // XXX `regex`es are persistent data structure with acyclic structural sharing
  // XXX realization: Brzozowski derivatives with structural sharing is
  // shockingly similar to the lock-step algorithm for NFAs
  unsigned refcount;
  unsigned lower, upper; // for repeats. both bounds inclusive
  // XXX doc `symset` used for caching derivatives too
  uint8_t symset[256]; // for symsets
  struct regex *delta;
  // XXX doc must be acyclic, otherwise memory leak
  struct regex *children[];
};

// XXX go over everything and decide on `static`s

static size_t regexes_len(struct regex *regexes[]);
static struct regex *regex_alloc(struct regex fields,
                                 struct regex *children[]) {
#define regex_alloc(CHILDREN, ...)                                             \
  regex_alloc((struct regex){__VA_ARGS__}, CHILDREN)
  size_t children_size = (regexes_len(children) + 1) * sizeof(*children);
  struct regex *regex = malloc(sizeof(*regex) + children_size);
  *regex = fields, memcpy(regex->children, children, children_size);
  return regex->refcount = 1, regex;
}

static struct regex **regexes_incref(struct regex *regexes[]) {
  for (struct regex **regex = regexes; *regex; regex++)
    regex_incref(*regex);
  return regexes;
}

struct regex *regex_incref(struct regex *regex) {
  return regex->refcount++, regex;
}

static struct regex **regexes_decref(struct regex *regexes[]) {
  for (struct regex **regex = regexes; *regex; regex++)
    regex_decref(*regex);
  return regexes;
}

struct regex *regex_decref(struct regex *regex) {
  if (--regex->refcount)
    return NULL;
  regexes_decref(regex->children);
  if (regex->delta)
    regex->delta = regex_decref(regex->delta);
  return free(regex), NULL;
}

static int regexes_cmp(struct regex *regexes1[], struct regex *regexes2[]) {
  // return an integer less than, equal to, or greater than zero if
  // `regexes1` is, respectively, structurally less than, structurally equal
  // to, or structurally greater than `regexes2`. the ordering is arbitrary

  struct regex **regex1 = regexes1, **regex2 = regexes2;
  for (int cmp; *regex1 && *regex2; regex1++, regex2++)
    if (cmp = regex_cmp(*regex1, *regex2))
      return cmp;
  return !!*regex1 - !!*regex2;
}

int regex_cmp(struct regex *regex1, struct regex *regex2) {
  // return an integer less than, equal to, or greater than zero if
  // `regex1` is, respectively, structurally less than, structurally equal
  // to, or structurally greater than `regex2`. the ordering is arbitrary

  if (regex1 == regex2)
    return 0;

  int cmp;
  if (cmp = regex1->type - regex2->type)
    return cmp;

  switch (regex1->type) {
  case TYPE_ALT:
  case TYPE_CONCAT:
    return regexes_cmp(regex1->children, regex2->children);
  case TYPE_REPEAT:
    if (cmp = regex1->lower - regex2->lower)
      return cmp;
    if (cmp = regex1->upper - regex2->upper)
      return cmp;
  case TYPE_COMPL:
    return regex_cmp(*regex1->children, *regex2->children);
  case TYPE_SYMSET:
    return memcmp(regex1->symset, regex2->symset, sizeof(regex1->symset));
  }

  abort(); // should have diverged
}

static size_t regexes_len(struct regex *regexes[]) {
  // borrows its argument

  struct regex **regex = regexes;
  while (*regex)
    regex++;
  return regex - regexes;
}

static struct regex **regexes_insert(struct regex *regexes[],
                                     struct regex *elem) {
  // insert borrowed element `elem` into the owned sorted array `regexes` if it
  // is not already present

  // find the insertion spot
  for (; *regexes && regex_cmp(elem, *regexes) < 0; regexes++)
    ;
  struct regex **regex = regexes;
  if (*regex && regex_cmp(elem, *regex) == 0)
    return regexes; // `elem` already present
  elem = regex_incref(elem);
  // scooch remaining elements over
  for (struct regex *temp; elem; regex++)
    temp = *regex, *regex = elem, elem = temp;
  *regex = elem;
  return regexes;
}

static struct regex **regexes_extend(struct regex *regexes[],
                                     struct regex *elems[]) {
  // insert borrowed elements `elems` into the owned sorted array `regexes` if
  // they are not already present

  struct regex **regex = regexes;
  for (struct regex **elem = elems; *elem; elem++)
    regex = regexes_insert(regex, *elem);
  return regex;
}

// XXX doc smart constructors respect idempotence, commutativity, associativity,
// De Morgan's... along with identity elements and annihilators

struct regex *regex_alt(struct regex *children[]) {
  bool nullable = false;
  bool eps_child = false;
  bool negeps_child = false;
  size_t flat_len = regexes_len(children);
  for (struct regex **child = children; *child; child++) {
    nullable |= (*child)->nullable && *child != regex_eps();
    eps_child |= *child == regex_eps();
    negeps_child |= *child == regex_negeps();
    if ((*child)->type == TYPE_ALT)
      flat_len += regexes_len((*child)->children) - 1;
    // r|% |- % XXX
    // %|r |- % XXX
    if (*child == regex_univ())
      return regexes_decref(children), regex_univ();
  }

  // r|(~) |- %, if nu(r) XXX
  // (~)|r |- %, if nu(r) XXX
  // r|(~) |- (~), if !nu(r)
  // (~)|r |- (~), if !nu(r)
  if (negeps_child && (nullable || eps_child))
    return regexes_decref(children), regex_univ();
  if (negeps_child && !(nullable || eps_child))
    return regexes_decref(children), regex_negeps();

  struct regex *flat_children[flat_len + 1];
  *flat_children = NULL;
  for (struct regex **child = children; *child; child++) {
    // r|() |- r, if nu(r)
    // ()|r |- r, if nu(r)
    if (nullable && *child == regex_eps())
      ;
    // r|r |- r
    // r|[] |- r
    // []|r |- r
    // r|(s|t) |- r|s|t
    // (r|s)|t |- r|s|t
    // r|s |- s|r, if cmp(r, s) < 0
    else if ((*child)->type == TYPE_ALT)
      regexes_extend(flat_children, (*child)->children);
    else
      regexes_insert(flat_children, *child);
    regex_decref(*child);
  }

  nullable |= eps_child;

  // [] |- []
  if (!*flat_children)
    return regex_empty();

  // r |- r
  if (!flat_children[1])
    return *flat_children;

  struct regex *regex = regex_alloc(flat_children, TYPE_ALT);
  return regex->nullable = nullable, regex;
}

struct regex *regex_compl(struct regex *child) {
  if (child == regex_univ())
    return regex_decref(child), regex_empty(); // ~% |- [] XXX
  if (child == regex_empty())
    return regex_decref(child), regex_univ(); // ~[] |- % XXX
  if (child == regex_eps())
    return regex_decref(child), regex_negeps(); // ~() |- (~)
  if (child == regex_negeps())
    return regex_decref(child), regex_eps(); // ~(~) |- ()
  if (child->type == TYPE_COMPL) {
    struct regex *subchild = regex_incref(*child->children);
    return regex_decref(child), subchild; // ~~r |- r
  }

  bool nullable = !child->nullable;

  struct regex *regex = regex_alloc(REGEXES(child), TYPE_COMPL);
  return regex->nullable = nullable, regex;
}

struct regex *regex_concat(struct regex *children[]) {
  bool nullable = true;
  bool univ_child = false;
  bool negeps_child = false;
  size_t flat_len = regexes_len(children);
  for (struct regex **child = children; *child; child++) {
    nullable &= (*child)->nullable || *child == regex_negeps();
    univ_child |= *child == regex_univ();
    negeps_child |= *child == regex_negeps();

    if ((*child)->type == TYPE_CONCAT)
      flat_len += regexes_len((*child)->children) - 1;
    // r[] |- []
    // []r |- []
    if (*child == regex_empty())
      return regexes_decref(children), regex_empty();
  }

  // r% |- %, if nu(r) XXX
  // %r |- %, if nu(r) XXX
  // r(~) |- (~), if nu(r)
  // (~)r |- (~), if nu(r)
  if (univ_child && (nullable && !negeps_child))
    return regexes_decref(children), regex_univ();
  if (negeps_child && nullable)
    return regexes_decref(children), regex_negeps();

  struct regex *flat_children[flat_len + 1], **flat_child = flat_children;
  for (struct regex **child = children; *child; child++) {
    // r() |- r
    // ()r |- r
    // r(st) |- rst
    // (rs)t |- rst
    if ((*child)->type == TYPE_CONCAT)
      for (struct regex **subchild = (*child)->children; *subchild; subchild++)
        *flat_child++ = regex_incref(*subchild);
    else
      *flat_child++ = regex_incref(*child);
    regex_decref(*child);
  }
  *flat_child = NULL;

  nullable &= !negeps_child;

  // () |- ()
  if (!*flat_children)
    return regex_eps();

  // r |- r
  if (!flat_children[1])
    return *flat_children;

  struct regex *regex = regex_alloc(flat_children, TYPE_CONCAT);
  return regex->nullable = nullable, regex;
}

struct regex *regex_repeat(struct regex *child, unsigned lower,
                           unsigned upper) {
  if (upper == 0)
    return regex_decref(child), regex_eps(); // r{0} |- ()
  if (lower == 1 && upper == 1)
    return child; // r{1} |- r
  if (child == regex_eps())
    return child; // (){m,n} |- ()
  if (child == regex_univ())
    return child; // %{m,n+1} |- % XXX
  if (child == regex_empty())
    if (lower == 0)
      return regex_decref(child), regex_eps(); // []{0,n} |- ()
    else if (lower != 0)
      return child; // []{m+1,n} |- []
  if (child == regex_negeps())
    if (lower == 0)
      return regex_decref(child), regex_univ(); // (~){0,n+1} |- % XXX
    else if (lower == 1)
      return child; // (~){1,n} |- (~) XXX
  if (child->type == TYPE_SYMSET && child->upper && upper == UINT_MAX)
    if (lower == 0)
      return regex_decref(child), regex_univ(); // <>* |- % XXX XXX
    else if (lower == 1)
      return regex_decref(child), regex_negeps(); // <>+ |- (~) XXX

  bool nullable = lower == 0 || child->nullable;

  struct regex *regex =
      regex_alloc(REGEXES(child), TYPE_REPEAT, .lower = lower, .upper = upper);
  return regex->nullable = nullable, regex;
}

struct regex *regex_symset(symset_t *symset) {
  // XXX doc empty set / universal set invariant
  bool empty = 1, upper = 1;
  for (int i = 0; i < sizeof(*symset); i++)
    empty &= (*symset)[i] == 0x00, upper &= (*symset)[i] == 0xff;
  if (empty)
    return regex_empty(); // [] |- []

  bool nullable = false;

  struct regex *regex = regex_alloc(REGEXES(NULL), TYPE_SYMSET, .upper = upper);
  memcpy(regex->symset, *symset, sizeof(*symset));
  return regex->nullable = nullable, regex;
}

// wrappers around the smart constructors, for structural recursion. when
// calling the underlying smart constructor would yield a regular expression
// that is structurally equal to `prev`, return `prev` instead. borrow `prev`
// but move in `child` and `children`

static struct regex *regex_alt_prev(struct regex *prev,
                                    struct regex *children[]) {
  if (regexes_cmp(prev->children, children) == 0)
    return regexes_decref(children), regex_incref(prev);
  return regex_alt(children);
}

static struct regex *regex_compl_prev(struct regex *prev, struct regex *child) {
  if (regex_cmp(*prev->children, child) == 0)
    return regex_decref(child), regex_incref(prev);
  return regex_compl(child);
}

static struct regex *regex_concat_prev(struct regex *prev,
                                       struct regex *children[]) {
  if (regexes_cmp(prev->children, children) == 0)
    return regexes_decref(children), regex_incref(prev);
  return regex_concat(children);
}

static struct regex *regex_repeat_prev(struct regex *prev, struct regex *child,
                                       unsigned lower, unsigned upper) {
  if (regex_cmp(*prev->children, child) == 0 && prev->lower == lower &&
      prev->upper == upper)
    return regex_decref(child), regex_incref(prev);
  return regex_repeat(child, lower, upper);
}

static struct regex *regex_symset_prev(struct regex *prev, symset_t *symset) {
  if (memcmp(prev->symset, symset, sizeof(*symset)) == 0)
    return regex_incref(prev);
  return regex_symset(symset);
}

// shorthands for ALTs and CONCATs that have no children. equivalent to
// calling the underlying smart constructors with `children = REGEXES(NULL)`.
// they allocate memory once and always return the same pointers, which means
// `regex_cmp` can report equality without a memory access

struct regex *regex_empty(void) {
  // empty set regex /[]/
  static struct regex *empty = NULL;
  if (empty == NULL)
    empty = regex_alloc(REGEXES(NULL), TYPE_ALT, .nullable = false);
  return empty->refcount = INT_MAX, empty;
}

struct regex *regex_univ(void) {
  // universal set regex /%/ XXX
  static struct regex *univ = NULL;
  if (univ == NULL)
    univ = regex_alloc(REGEXES(regex_empty()), TYPE_COMPL, .nullable = true);
  return univ->refcount = INT_MAX, univ;
}

struct regex *regex_eps(void) {
  // epsilon regex /()/
  static struct regex *eps = NULL;
  if (eps == NULL)
    eps = regex_alloc(REGEXES(NULL), TYPE_CONCAT, .nullable = true);
  return eps->refcount = INT_MAX, eps;
}

struct regex *regex_negeps(void) {
  // negated epsilon regex /(~)/
  static struct regex *negeps = NULL;
  if (negeps == NULL)
    negeps = regex_alloc(REGEXES(regex_eps()), TYPE_COMPL, .nullable = false);
  return negeps->refcount = INT_MAX, negeps;
}

void regex_dump(struct regex *regex, int indent) {
  // borrows its argument

  char *types[] = {"ALT", "COMPL", "CONCAT", "REPEAT", "SYMSET"};
  printf("%*s%s", indent, "", types[regex->type]);

  if (regex->type == TYPE_REPEAT)
    printf(" %d TO %d", regex->lower, regex->upper);
  if (regex->type == TYPE_SYMSET)
    printf(" %s", symset_fmt(regex->symset));

  putchar('\n');
  for (struct regex **child = regex->children; *child; child++)
    regex_dump(*child, indent + 2);
}

static size_t regex_fmt_len(struct regex *regex, enum regex_type prec) {
  // return the length of the string that would be produced by `regex_fmt`,
  // excluding the null terminator. this is a one-to-one mirror of `regex_fmt`,
  // except we calculate length instead of writing to a buffer. borrows its
  // argument

  if (regex == regex_empty())
    return 2;
  if (regex == regex_univ())
    return 5; // % XXX
  if (regex == regex_eps())
    return 2;
  if (regex == regex_negeps())
    return 3;

  size_t len = regex->type < prec; // (

  switch (regex->type) {
  case TYPE_ALT:
    // XXX doc TYPE_COMPL
    len += regex_fmt_len(*regex->children, TYPE_COMPL);
    for (struct regex **child = regex->children + 1; *child; child++)
      len++ /* | & */, len += regex_fmt_len(*child, TYPE_COMPL);
    break;
  case TYPE_COMPL:
    len++; // ~
    len += regex_fmt_len(*regex->children, TYPE_CONCAT);
    break;
  case TYPE_CONCAT:
    for (struct regex **child = regex->children; *child; child++)
      len += regex_fmt_len(*child, TYPE_CONCAT);
    break;
  case TYPE_REPEAT:;
    // XXX comment
    // here we use `prec = TYPE_SYMSET` because the direct child of a quantifier
    // can't be a quantifier, as specified by the grammar

    bool quant = regex->lower == 0 && regex->upper == UINT_MAX ||
                 regex->lower == 1 && regex->upper == UINT_MAX ||
                 regex->lower == 0 && regex->upper == 1;
    if (quant) {
      // XXX doc this extra parenthesizing
      if (prec >= TYPE_REPEAT)
        len++; // (
      len += regex_fmt_len(*regex->children, /* TYPE_REPEAT */ TYPE_SYMSET);
      len++; // * + ?
      if (prec >= TYPE_REPEAT)
        len++; // )
      break;
    }

    len += regex_fmt_len(*regex->children, /* TYPE_REPEAT */ TYPE_SYMSET);
    len++; // {
    if (regex->lower != 0)
      len += snprintf(NULL, 0, "%u", regex->lower);
    if (regex->lower != regex->upper) {
      len++; // ,
      if (regex->upper != UINT_MAX)
        len += snprintf(NULL, 0, "%u", regex->upper);
    }
    len++; // }
    break;
  case TYPE_SYMSET:
    len += strlen(symset_fmt(regex->symset));
    break;
  }

  return len + (regex->type < prec); // )
}

static char *regex_fmt(struct regex *regex, char *buf, enum regex_type prec) {
  // convert `regex` to a pattern string in `buf`. null-terminate `buf` and
  // return a pointer to the null terminator. output shall be parsable by
  // `parse_regex` and satisfy the invariant `parse_regex . regex_fmt == id`.
  // in the general case, will not satisfy `regex_fmt . parse_regex == id`.
  // call `regex_fmt_len` to know how much memory to allocate for `buf`. `prec`
  // is a lower bound on the precedence of the pattern string to be produced;
  // call initially with `prec = 0`. borrows its argument

  if (regex == regex_empty())
    return strcpy(buf, "[]"), buf += 2;
  if (regex == regex_univ())
    return strcpy(buf, "(<>*)"), buf += 5; // % XXX
  if (regex == regex_eps())
    return strcpy(buf, "()"), buf += 2;
  if (regex == regex_negeps())
    return strcpy(buf, "(~)"), buf += 3;

  if (regex->type < prec)
    *buf++ = '(';

  switch (regex->type) {
  case TYPE_ALT:
    // XXX doc TYPE_COMPL
    buf = regex_fmt(*regex->children, buf, TYPE_COMPL);
    for (struct regex **child = regex->children + 1; *child; child++) {
      *buf++ = regex->type == TYPE_ALT ? '|' : '&';
      buf = regex_fmt(*child, buf, TYPE_COMPL);
    }
    break;
  case TYPE_COMPL:
    *buf++ = '~';
    // XXX doc TYPE_CONCAT
    buf = regex_fmt(*regex->children, buf, TYPE_CONCAT);
    break;
  case TYPE_CONCAT:
    for (struct regex **child = regex->children; *child; child++)
      buf = regex_fmt(*child, buf, TYPE_CONCAT);
    break;
  case TYPE_REPEAT:;
    // XXX comment
    // here we use `prec = TYPE_SYMSET` because the direct child of a quantifier
    // can't be a quantifier, as specified by the grammar
    char quant = '\0';
    if (regex->lower == 0 && regex->upper == UINT_MAX)
      quant = '*';
    if (regex->lower == 1 && regex->upper == UINT_MAX)
      quant = '+';
    if (regex->lower == 0 && regex->upper == 1)
      quant = '?';

    if (quant) {
      // XXX doc this extra parenthesizing
      if (prec >= TYPE_REPEAT)
        *buf++ = '(';
      buf = regex_fmt(*regex->children, buf, /* TYPE_REPEAT */ TYPE_SYMSET);
      *buf++ = quant;
      if (prec >= TYPE_REPEAT)
        *buf++ = ')';
      break;
    }

    buf = regex_fmt(*regex->children, buf, /* TYPE_REPEAT */ TYPE_SYMSET);
    *buf++ = '{';
    if (regex->lower != 0)
      buf += sprintf(buf, "%u", regex->lower);
    if (regex->lower != regex->upper) {
      *buf++ = ',';
      if (regex->upper != UINT_MAX)
        buf += sprintf(buf, "%u", regex->upper);
    }
    *buf++ = '}';
    break;
  case TYPE_SYMSET:;
    char *fmt = symset_fmt(regex->symset);
    strcpy(buf, fmt), buf += strlen(fmt);
    break;
  }

  if (regex->type < prec)
    *buf++ = ')';

  return *buf = '\0', buf;
}

// a DFA state, and maybe an actual DFA too, depending on context. when treated
// as a DFA, the first element of the linked list of states formed by `next` is
// the initial state and subsequent elements enumerate all remaining states
struct dstate {
  struct dstate *transitions[256]; // indexed by input characters
  bool accepting, terminating;     // for match result and early termination
  int id;              // populated and used for various purposes throughout
  struct dstate *next; // linked list to keep track of all states of a DFA
  struct regex *regex; // associated regular expression for determinization
};

static struct dstate *dstate_alloc(struct dstate fields) {
#define dstate_alloc(...) dstate_alloc((struct dstate){__VA_ARGS__})
  struct dstate *dstate = malloc(sizeof(*dstate));
  return *dstate = fields, dstate->id = -1, dstate;
}

void dfa_free(struct dstate *dstate) {
  for (struct dstate *next; dstate; dstate = next) {
    if (dstate->regex)
      dstate->regex = regex_decref(dstate->regex);
    next = dstate->next, free(dstate);
  }
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

  // len(leb128(dfa_size)) == floor(log128(dfa_size) + 1)
  int log128p1 = 0;
  for (int s = dfa_size; s >>= 7;)
    log128p1++;
  log128p1++;

  uint8_t *image = malloc(log128p1), *p = image;
  leb128_put(&p, dfa_size);

  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next) {
    // ensure buffer large enough for worst case. worst case is typically around
    // 500 bytes larger than best case, so this is not too wasteful.
    ptrdiff_t len = p - image;
    // len + <accepting_terminating> + 256 * (<run_length> + <leb128(dfa_size)>)
    uint8_t *new = realloc(image, len + 1 + 256 * (1 + log128p1));
    image = new, p = new + len;

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

  *size = p - image;
  return realloc(image, p - image); // don't be wasteful
}

struct dstate *dfa_deserialize(uint8_t *image, size_t *size) {
  // deserialize a DFA from a **trusted** image. `size` is an out parameter

  uint8_t *p = image;
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

  *size = p - image;
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
      for (char *fmt = symset_fmt(transitions); *fmt; fmt++)
        // avoid breaking Mermaid
        printf(strchr("\\\"#&{}()xo=- ", *fmt) ? "#%hhu;" : "%c", *fmt);
      printf("--> %d\n", ds2->id);
    }
  }
}

// TODO go over parser and do something better than binary
// regex_...(REGEXES(..., ...)) etc.

// some invariants for parsers on parse error:
// - `error` shall be set to a non-`NULL` error message
// - `regex` shall point to the error location
// - the returned regular expression should be `NULL`
// - the caller is responsible for backtracking

static unsigned parse_natural(char **pattern, char **error) {
  if (!isdigit(**pattern)) {
    *error = "expected natural number";
    return 0;
  }

  unsigned natural = 0;
  for (; isdigit(**pattern); ++*pattern) {
    int digit = **pattern - '0';

    if (natural > UINT_MAX / 10 || natural * 10 > UINT_MAX - digit) {
      *error = "natural number overflow";
      return UINT_MAX; // indicate overflow condition
    }

    natural *= 10, natural += digit;
  }
  return natural;
}

static uint8_t parse_hexbyte(char **pattern, char **error) {
  uint8_t byte = 0;
  for (int i = 0; i < 2; i++) {
    byte <<= 4;
    if (isdigit(**pattern))
      byte |= **pattern - '0';
    else if (isxdigit(**pattern))
      byte |= tolower(**pattern) - 'a' + 10;
    else {
      *error = "expected hex digit";
      return 0;
    }
    ++*pattern;
  }
  return byte;
}

static uint8_t parse_escape(char **pattern, char **error) {
  if (strchr(METACHARS, **pattern))
    return *(*pattern)++;

  char *escape, *escapes = C_ESCAPES;
  if (**pattern && (escape = strchr(escapes, **pattern)) && ++*pattern)
    return C_CODEPTS[escape - escapes];

  if (**pattern == 'x' && ++*pattern) {
    uint8_t chr = parse_hexbyte(pattern, error);
    if (*error)
      return 0;
    return chr;
  }

  *error = "unknown escape";
  return 0;
}

static uint8_t parse_symbol(char **pattern, char **error) {
  if (**pattern == '\\' && ++*pattern) {
    uint8_t escape = parse_escape(pattern, error);
    if (*error)
      return 0;

    return escape;
  }

  if (**pattern == '\0') {
    *error = "expected symbol";
    return 0;
  }

  if (strchr(METACHARS, **pattern)) {
    *error = "unexpected metacharacter";
    return 0;
  }

  if (!isprint(**pattern)) {
    *error = "unexpected nonprintable character";
    return 0;
  }

  return *(*pattern)++;
}

static void parse_shorthand(symset_t *symset, char **pattern, char **error) {
  memset(symset, 0x00, sizeof(*symset));

#define RETURN_SYMSET(COND)                                                    \
  do {                                                                         \
    for (int chr = 0; chr < 256; chr++)                                        \
      if (COND)                                                                \
        bitset_set(*symset, chr);                                              \
    return;                                                                    \
  } while (0)

  if (**pattern == '\\' && ++*pattern) {
    switch (*(*pattern)++) {
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

    --*pattern, --*pattern;
  }

  if (**pattern == '.' && ++*pattern)
    RETURN_SYMSET(chr != '\n');

#undef RETURN_SYMSET

  *error = "expected shorthand class";
  return;
}

static void parse_symset(symset_t *symset, char **pattern, char **error) {
  bool complement = **pattern == '^' && ++*pattern;

  char *last_pattern = *pattern;
  parse_shorthand(symset, pattern, error);
  if (!*error)
    goto process_complement;
  *error = NULL;
  *pattern = last_pattern;

  if (**pattern == '[' && ++*pattern) {
    memset(symset, 0x00, sizeof(*symset));
    while (!strchr("]", **pattern)) {
      symset_t sub;
      parse_symset(&sub, pattern, error);
      if (*error)
        return;

      for (int i = 0; i < sizeof(*symset); i++)
        (*symset)[i] |= sub[i];
    }

    if (**pattern == ']' && ++*pattern)
      goto process_complement;

    *error = "expected ']'";
    return;
  }
  *pattern = last_pattern;

  if (**pattern == '<' && ++*pattern) {
    memset(symset, 0xff, sizeof(*symset));
    while (!strchr(">", **pattern)) {
      symset_t sub;
      parse_symset(&sub, pattern, error);
      if (*error)
        return;

      for (int i = 0; i < sizeof(*symset); i++)
        (*symset)[i] &= sub[i];
    }

    if (**pattern == '>' && ++*pattern)
      goto process_complement;

    *error = "expected '>'";
    return;
  }
  *pattern = last_pattern;

  uint8_t lower = parse_symbol(pattern, error), upper = lower;
  if (!*error) {
    if (**pattern == '-' && ++*pattern) {
      upper = parse_symbol(pattern, error);
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

static struct regex *parse_regex(char **pattern, char **error);
static struct regex *parse_atom(char **pattern, char **error) {
  if (**pattern == '(' && ++*pattern) {
    struct regex *sub = parse_regex(pattern, error);
    if (*error)
      return NULL;

    if (**pattern == ')' && ++*pattern)
      return sub;

    *error = "expected ')'";
    return regex_decref(sub), NULL;
  }

  symset_t symset;
  parse_symset(&symset, pattern, error);
  if (*error)
    return NULL;
  return regex_symset(&symset);
}

static struct regex *parse_factor(char **pattern, char **error) {
  struct regex *atom = parse_atom(pattern, error);
  if (*error)
    return NULL;

  const char *quants = "*+?", *quant = strchr(quants, **pattern);
  if (**pattern && quant && ++*pattern) {
    return regex_repeat(atom, (unsigned[]){0, 1, 0}[quant - quants],
                        (unsigned[]){UINT_MAX, UINT_MAX, 1}[quant - quants]);
  }

  char *last_pattern = *pattern;
  if (**pattern == '{' && ++*pattern) {
    unsigned lower = parse_natural(pattern, error);
    if (lower == UINT_MAX) { // UINT_MAX lower bound is reserved
      if (!*error)           // may have been set by `parse_natural`
        *error = "repetition bound overflow";
      return regex_decref(atom), NULL;
    } else if (*error)
      lower = 0, *error = NULL;

    unsigned upper = lower;
    if (**pattern == ',' && ++*pattern) {
      upper = parse_natural(pattern, error);
      if (upper == UINT_MAX) { // UINT_MAX upper bound means unbounded
        if (!*error)           // may have been set by `parse_natural`
          *error = "repetition bound overflow";
        return regex_decref(atom), NULL;
      } else if (*error)
        upper = UINT_MAX, *error = NULL; // mark as unbounded
    }

    if (**pattern == '}' && ++*pattern)
      ;
    else {
      *error = "expected '}'";
      return regex_decref(atom), NULL;
    }

    if (lower > upper) {
      *pattern = last_pattern;
      *error = "misbounded quantifier";
      return regex_decref(atom), NULL;
    }

    return regex_repeat(atom, lower, upper);
  }

  return atom;
}

static struct regex *parse_term(char **pattern, char **error) {
  bool complement = **pattern == '~' && ++*pattern;

  // hacky lookahead for better diagnostics
  if (strchr(")|&", **pattern)) {
    struct regex *term = regex_eps();
    return complement ? regex_compl(term) : term;
  }

  struct regex *factor = parse_factor(pattern, error);
  if (*error)
    return NULL;
  if (**pattern == '~') {
    *error = "unexpected '~'";
    return regex_decref(factor), NULL;
  }

  struct regex *cat = parse_term(pattern, error);
  if (*error)
    return regex_decref(factor), NULL;

  struct regex *term = regex_concat(REGEXES(factor, cat));
  return complement ? regex_compl(term) : term;
}

static struct regex *parse_regex(char **pattern, char **error) {
  struct regex *term = parse_term(pattern, error);
  if (*error)
    return NULL;

  if (**pattern == '|' || **pattern == '&') {
    bool intersect = *(*pattern)++ == '&';
    struct regex *alt = parse_regex(pattern, error);
    if (*error)
      return regex_decref(term), NULL;

    // XXX compls getting cancelled all over the place
    return intersect ? regex_compl(regex_alt(
                           REGEXES(regex_compl(term), regex_compl(alt))))
                     : regex_alt(REGEXES(term, alt));
  }

  return term;
}

struct regex *ltre_parse(char **pattern, char **error) {
  // returns a null regex on error; `*pattern` will point to the error location
  // and `*error` will be set to an error message. `error` may be set to `NULL`
  // to disable error reporting

  // don't write to `*pattern` or `*error` if error reporting is disabled
  char *e, *r = *pattern;
  if (error == NULL)
    error = &e, pattern = &r;

  *error = NULL;
  struct regex *regex = parse_regex(pattern, error);
  if (*error)
    return NULL;

  if (**pattern != '\0') {
    *error = "expected end of input";
    return regex_decref(regex), NULL;
  }

  // regex_dump(regex, 0); // XXX
  // char *s = ltre_stringify(regex); // XXX
  // puts(s), free(s);
  return regex;
}

struct regex *ltre_fixed_string(char *string) {
  // parse a fixed string into a regular expression. never errors

  symset_t symset = {0};
  struct regex *children[strlen(string) + 1], **child = children;
  for (char *p = string; *p; p++) {
    bitset_set(symset, *p);
    *child++ = regex_symset(&symset);
    bitset_clr(symset, *p);
  }
  *child = NULL;

  return regex_concat(children);
}

char *ltre_stringify(struct regex *regex) {
  char *pattern = malloc(regex_fmt_len(regex, 0) + 1);
  (void)regex_fmt(regex, pattern, 0);
  return regex_decref(regex), pattern;
}

static struct regex *regex_ignorecase_ref(struct regex *regex, bool dual) {
  // ignorecase accepted language by canonical structural recursion. borrows its
  // argument. if `!dual`, a word is included in the new language if and only if
  // some case variation of it is present in the existing language. if `dual`, a
  // word is included in the new language if and only if all case variations of
  // it are present in the existing language

  struct regex *children[regexes_len(regex->children) + 1];
  memcpy(children, regex->children, sizeof(children));

  switch (regex->type) {
  case TYPE_ALT:
    for (struct regex **child = children; *child; child++)
      *child = regex_ignorecase_ref(*child, dual);
    return regex_alt_prev(regex, children);
  case TYPE_COMPL:
    return regex_compl_prev(regex, regex_ignorecase_ref(*children, !dual));
  case TYPE_CONCAT:
    for (struct regex **child = children; *child; child++)
      *child = regex_ignorecase_ref(*child, dual);
    return regex_concat_prev(regex, children);
  case TYPE_REPEAT:
    return regex_repeat_prev(regex, regex_ignorecase_ref(*children, dual),
                             regex->lower, regex->upper);
  case TYPE_SYMSET:;
    symset_t symset = {0};
    memcpy(symset, regex->symset, sizeof(symset));
    for (int chr = 0; chr < 256; chr++) {
      if (bitset_get(symset, chr) == !dual) {
        (!dual ? bitset_set : bitset_clr)(symset, tolower(chr));
        (!dual ? bitset_set : bitset_clr)(symset, toupper(chr));
      }
    }
    return regex_symset_prev(regex, &symset);
  }

  abort(); // should have diverged
}

struct regex *regex_ignorecase(struct regex *regex, bool dual) {
  struct regex *temp = regex_ignorecase_ref(regex, dual);
  return regex_decref(regex), temp;
}

static struct regex *regex_reverse_ref(struct regex *regex) {
  // reverse accepted language by canonical structural recursion. borrows
  // its argument

  struct regex *children[regexes_len(regex->children) + 1];
  memcpy(children, regex->children, sizeof(children));

  switch (regex->type) {
  case TYPE_ALT:
    for (struct regex **child = children; *child; child++)
      *child = regex_reverse_ref(*child);
    return regex_alt_prev(regex, children);
  case TYPE_COMPL:
    return regex_compl_prev(regex, regex_reverse_ref(*children));
  case TYPE_CONCAT:;
    size_t children_len = sizeof(children) / sizeof(*children) - 1;
    for (struct regex **child = children; *child; child++)
      *child = regex_reverse_ref(
          regex->children[children + children_len - child - 1]);
    return regex_concat_prev(regex, children);
  case TYPE_REPEAT:
    return regex_repeat_prev(regex, regex_reverse_ref(*children), regex->lower,
                             regex->upper);
  case TYPE_SYMSET:
    return regex_incref(regex);
  }

  abort(); // should have diverged
}

struct regex *regex_reverse(struct regex *regex) {
  struct regex *temp = regex_reverse_ref(regex);
  return regex_decref(regex), temp;
}

// XXX doc modifies in-place for delta invariant
static struct regex *regex_derivate_ref(struct regex *regex, uint8_t chr) {
  // derivate `regex` with respect to `chr`. borrows its argument.
  // a derivative of a regular expression with respect to a symbol is any
  // regular expression that accepts exactly the strings that, if prepended by
  // the symbol, would have been accepted by the original regular expression

  if (regex->delta && bitset_get(regex->symset, chr) == regex->sym_incl)
    return regex_incref(regex->delta);

  if (regex->delta)
    regex->delta = regex_decref(regex->delta);

  struct regex *children[regexes_len(regex->children) + 1];
  memcpy(children, regex->children, sizeof(children));

  // XXX would be nice to use `regex,` instead of `NULL,`. improves 'test.c'
  // perf by 5% but creates circular references so causes memory leaks

  switch (regex->type) {
  case TYPE_ALT:
    for (struct regex **child = children; *child; child++)
      *child = regex_derivate_ref(*child, chr);
    regex->delta = regex_alt(children);
    break;
  case TYPE_COMPL:
    regex->delta = regex_compl(regex_derivate_ref(*children, chr));
    break;
  case TYPE_CONCAT:
    // XXX doc this dark magic
    for (struct regex **child = children; *child; child++) {
      bool nullable = (*child)->nullable;
      *child = regex_derivate_ref(*child, chr);
      regexes_incref(child + 1);
      *child = regex_concat(child);
      if (!nullable)
        child[1] = NULL;
    }
    regex->delta = regex_alt(children);
    break;
  case TYPE_REPEAT:;
    unsigned lower = regex->lower, upper = regex->upper;
    lower -= lower != 0, upper -= upper != 0 && upper != UINT_MAX;
    regex->delta = regex_concat(
        REGEXES(regex_derivate_ref(*children, chr),
                regex_repeat(regex_incref(*children), lower, upper)));
    break;
  case TYPE_SYMSET:
    regex->delta = bitset_get(regex->symset, chr) ? regex_eps() : regex_empty();
  }

  switch (regex->type) {
  case TYPE_ALT:
  case TYPE_CONCAT:
    regex->sym_incl = true;
    memset(regex->symset, 0xff, sizeof(regex->symset));
    for (struct regex **child = regex->children; *child; child++) {
      for (int i = 0; i < sizeof(regex->symset); i++)
        regex->symset[i] &=
            (*child)->symset[i] ^ (unsigned)(*child)->sym_incl - 1;
      if (regex->type == TYPE_CONCAT && !(*child)->nullable)
        break;
    }
    break;
  case TYPE_COMPL:
  case TYPE_REPEAT:
    regex->sym_incl = (*regex->children)->sym_incl;
    memcpy(regex->symset, (*regex->children)->symset, sizeof(regex->symset));
    break;
  case TYPE_SYMSET:
    regex->sym_incl = bitset_get(regex->symset, chr);
  }

  // TODO doc
  if (bitset_get(regex->symset, chr) != regex->sym_incl)
    abort();

  // printf("wrt 0x%02hhx\n", chr);
  // regex_dump(regex, 0);
  // regex_dump(regex->delta, 0);

  return regex_incref(regex->delta);
}

struct regex *regex_derivate(struct regex *regex, uint8_t chr) {
  struct regex *temp = regex_derivate_ref(regex, chr);
  return regex_decref(regex), temp;
}

// TODO update all comments that mention `dfa_step`
static void dfa_step(struct dstate **dfap, struct dstate *dstate, uint8_t chr) {
  // XXX comment
  // create a DFA state whose `bitset` corresponds to this "superposition",
  // if it doesn't already exist. binary tree not necessary, linear search
  // is just as fast

  struct regex *delta = regex_derivate_ref(dstate->regex, chr);

  struct dstate **dstatep = dfap;
  while (*dstatep && regex_cmp((*dstatep)->regex, delta) != 0)
    dstatep = &(*dstatep)->next;

  if (*dstatep)
    delta = regex_decref(delta);
  else
    // a DFA state is accepting if and only if its corresponding regular
    // expression accepts the empty word
    *dstatep = dstate_alloc(.regex = delta, .accepting = delta->nullable);

  // XXX doc character sets
  for (int chr = 0; chr < 256; chr++)
    if (bitset_get(dstate->regex->symset, chr) == dstate->regex->sym_incl)
      // XXX comment
      // patch the `chr` transition to point to this new (or existing) state
      dstate->transitions[chr] = *dstatep;
}

struct dstate *ltre_compile(struct regex *regex) {
  // XXX comment
  // fully compile DFA. powerset construction followed by DFA minimization
  // XXX doc
  struct dstate *dfa =
      dstate_alloc(.regex = regex, .accepting = regex->nullable);
  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next)
    for (int chr = 0; chr < 256; chr++)
      if (!dstate->transitions[chr])
        dfa_step(&dfa, dstate, chr);

  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next)
    dstate->regex = regex_decref(dstate->regex);

  // dfa_dump(dfa);

  int dfa_size = dfa_get_size(dfa);
  struct dstate **dstates =
      malloc(sizeof(*dstates) * dfa_size); // VLA is 35% slower
  for (struct dstate *dstate = dfa; dstate; dstate = dstate->next)
    dstates[dstate->id] = dstate;

  // store distinguishability data in a symmetric matrix condensed using bitsets
  uint8_t(*dis)[(dfa_size + 7) / 8] =
      malloc(sizeof(*dis) * dfa_size); // VLA is 35% slower
  memset(dis, 0x00, sizeof(*dis) * dfa_size);
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

  // dfa_dump(dfa);
  // printf("%d -> %d\n", dfa_size, dfa_get_size(dfa));

  free(dstates), free(dis);
  return dfa;
}

bool ltre_matches(struct dstate *dfa, uint8_t *input) {
  // time linear in the input length :)
  for (; !dfa->terminating && *input; input++)
    dfa = dfa->transitions[*input];
  return dfa->accepting;
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
  for (int id = 0; id < dfa_size; id++)
    map[id] = NULL;

  // come up with a tentative mapping by following transitions from the initial
  // states. the mapping will be nonesensical when the DFAs are not equivalent,
  // but that doesn't matter as long as the mapping is an isomorphism when the
  // DFAs actually are equivalent
  map[dfa1->id] = dfa2;
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

struct regex *ltre_decompile(struct dstate *dfa) {
  // convert a DFA into a regular expression using the classic construction,
  // turning the DFA into a GNFA stored as a matrix of `arrow`s on the stack
  // then iteratively eliminating states by rerouting transitions. within this
  // function and only within this function, we store empty /[]/ transitions
  // as null pointers so they're easier to test for

  int dfa_size = dfa_get_size(dfa);
  // also create an auxiliary state and store it at index `dfa_size`
  struct regex *(*arrows)[dfa_size + 1] =
      malloc(sizeof(*arrows) * (dfa_size + 1)); // VLA is 20% slower
  for (int id = 0; id <= dfa_size; id++)
    arrows[dfa_size][id] = arrows[id][dfa_size] = NULL;

  // create an epsilon /()/ transition from the auxiliary state to the DFA's
  // initial state
  arrows[dfa_size][dfa->id] = regex_eps();
  for (struct dstate *ds1 = dfa; ds1; ds1 = ds1->next) {
    // create epsilon /()/ transitions from the DFA's accepting states to the
    // auxiliary state
    if (ds1->accepting)
      arrows[ds1->id][dfa_size] = regex_eps();

    for (struct dstate *ds2 = dfa; ds2; ds2 = ds2->next) {
      bool empty = true;
      symset_t transitions = {0};
      for (int chr = 0; chr < 256; chr++)
        if (ds1->transitions[chr] == ds2)
          bitset_set(transitions, chr), empty = false;

      arrows[ds1->id][ds2->id] = NULL;

      if (empty)
        continue;

      arrows[ds1->id][ds2->id] = regex_symset(&transitions);
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

        // XXX doc okay not to decref this since global ptr
        struct regex *bypass = regex_eps();

        // construct /(self)*/. note that []* |- ()
        if (arrows[best_fit][best_fit])
          bypass = regex_repeat(regex_incref(arrows[best_fit][best_fit]), 0,
                                UINT_MAX);

        // construct /(inbound)(self)*(outbound)/
        bypass =
            regex_concat(REGEXES(regex_incref(arrows[id1][best_fit]), bypass,
                                 regex_incref(arrows[best_fit][id2])));

        // construct /existing|(inbound)(self)*(outbound)/. note that r|[] |- r
        if (arrows[id1][id2])
          bypass = regex_alt(REGEXES(arrows[id1][id2], bypass));

        arrows[id1][id2] = bypass;
      }
    }

    // all transitions going through the best-fit state have been rerouted, so
    // replace them all with empty /[]/ transitions, effectively eliminating
    // the state by isolating it
    for (int id = 0; id <= dfa_size; id++) {
      if (arrows[id][best_fit])
        arrows[id][best_fit] = regex_decref(arrows[id][best_fit]);
      if (arrows[best_fit][id])
        arrows[best_fit][id] = regex_decref(arrows[best_fit][id]);
    }
  }

  // the final regular expression ends up as a self-loop on the auxiliary state
  struct regex *regex = arrows[dfa_size][dfa_size];
  free(arrows);
  return regex ? regex : regex_empty();
}

struct dstate *ltre_lazy_init(struct regex *regex) {
  return dstate_alloc(.regex = regex, .accepting = regex->nullable);
}

bool ltre_lazy_matches(struct dstate **dfap, uint8_t *input) {
  // XXX comment
  // Thompson's algorithm. lazily create new DFA states as we need them,
  // marching the regex in lock step. cached DFA states are stored in `*dfap`.
  // call initially with an empty cache using `*dfap = ltre_lazy_init(regex)`,
  // and make sure to `dfa_free(*dfap)` when finished with this regex

  struct dstate *dstate = *dfap;
  for (; *input; dstate = dstate->transitions[*input++])
    if (!dstate->transitions[*input])
      dfa_step(dfap, dstate, *input);

  return dstate->accepting;
}

// TODO reimplement regex simplification
