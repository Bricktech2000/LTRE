#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  size_t len = 0, cap = 256;
  char *line = malloc(cap);

  for (; !feof(stdin); len = 0) {
    for (int c; c = fgetc(stdin), c != EOF && c != '\n'; line[len++] = c)
      len + 1 == cap ? line = realloc(line, cap *= 2) : 0;
    line[len] = '\0';
    if (ferror(stdin))
      perror("fgetc"), exit(EXIT_FAILURE);
    if (feof(stdin))
      break;

    char *sep = memchr(line, '\t', len);
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

  free(line);
}
