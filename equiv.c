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

    char *sep = memchr(line, '\t', nl - line);
    if (sep == NULL) {
      fprintf(stderr, "format error: could not find tab separator\n");
      continue;
    }
    *sep = '\0';

    char *error = NULL, *loc = line;
    struct regex *regex1 = ltre_parse(&loc, &error);
    if (error)
      goto parse_error;
    loc = sep + 1;
    struct regex *regex2 = ltre_parse(&loc, &error);
    if (error) {
    parse_error:
      fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc);
      continue;
    }

    struct dstate *dfa1 = ltre_compile(regex1), *dfa2 = ltre_compile(regex2);
    puts(dfa_equivalent(dfa1, dfa2) ? "equivalent" : "not equivalent");

    dfa_free(dfa1), dfa_free(dfa2);
  }

  if (!feof(stdin))
    perror("fgets"), exit(EXIT_FAILURE);
  free(line);
}
