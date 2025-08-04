#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>

// steal implementation details
struct dstate {
  struct dstate *transitions[256];
  bool accepting, terminating;
};

bool run(struct dstate *dfa) {
  // if all outbound transitions are terminating, return. otherwise, if exactly
  // one outbound transition is non-terminating, follow it. otherwise, more than
  // one outbound transition is non-terminating, so let the user disambiguate.
  // interactive use works best with `stty -icanon -echo -nl`
  for (int chr = 0;; dfa = dfa->transitions[chr]) {
    if (putchar(chr) == EOF)
      goto done;

    for (chr = 0; chr < 256; chr++)
      if (!dfa->transitions[chr]->terminating)
        break;
    if (chr == 256)
      goto done;

    for (int split = chr + 1; split < 256; split++)
      if (!dfa->transitions[split]->terminating)
        goto disambiguate;
    continue;

  disambiguate:
    // do
    if ((chr = getchar()) == EOF)
      goto done;
    // while (dfa->transitions[chr]->terminating);
  }

done:
  return dfa->accepting;
}

int main(int argc, char **argv) {
  if (argc != 2)
    fprintf(stderr, "Usage: synth <pattern>\n"), exit(EXIT_FAILURE);

  char *error = NULL, *loc = argv[1];
  struct regex *regex = ltre_parse(&loc, &error);
  if (error)
    fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc),
        exit(EXIT_FAILURE);

  struct dstate *dfa = ltre_determinize(regex);
  dfa_optimize(dfa); // mark terminating states. faster than `dfa_minimize`

  // while (1)
  //   puts(run(dfa) ? "\naccept" : "\nreject");

  bool accept = run(dfa);
  dfa_free(dfa);
  return !accept;
}
