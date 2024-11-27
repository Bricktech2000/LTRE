#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *aegv[]) {
  size_t len = 0, cap = 256;
  char *nl, *line = malloc(cap);
  goto prompt;
  while (fgets(line + len, cap - len, stdin) != NULL) {
    if ((nl = memchr(line + len, '\n', cap - len)) == NULL) {
      len = cap - 1, line = realloc(line, cap *= 2);
      continue;
    }
    *nl = '\0', len = 0;

    char *error = NULL, *loc = line;
    struct nfa nfa = ltre_parse(&loc, &error);
    if (error) {
      fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc);
      goto prompt;
    }

    ltre_complement(&nfa);
    struct dstate *dfa = ltre_compile(nfa);
    char *re = ltre_decompile(dfa);
    puts(re);
    free(re), dfa_free(dfa), nfa_free(nfa);
  prompt:
    printf("> ");
  }

  if (!feof(stdin))
    perror("fgets"), exit(EXIT_FAILURE);
  free(line);
}
