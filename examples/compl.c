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

  free(line);
}
