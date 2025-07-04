#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  size_t len = 0, cap = 256;
  char *nl, *line = malloc(cap);
  while (fgets(line + len, cap - len, stdin) != NULL) {
    if ((nl = memchr(line + len, '\n', cap - len)) == NULL) {
      len = cap - 1, line = realloc(line, cap *= 2);
      continue;
    }
    *nl = '\0', len = 0;

    char *error = NULL, *loc = line;
    struct regex *regex = ltre_parse(&loc, &error);
    if (error) {
      fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc);
      continue;
    }

    struct dstate *dfa = ltre_compile(regex_compl(regex));
    char *pattern = ltre_stringify(ltre_decompile(dfa));
    puts(pattern);
    dfa_free(dfa), free(pattern);
  }

  if (!feof(stdin))
    perror("fgets"), exit(EXIT_FAILURE);
  free(line);
}
