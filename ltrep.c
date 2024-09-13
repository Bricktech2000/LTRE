#include "ltre.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// steal implementation details
struct dstate {
  struct dstate *transitions[256];
  bool accepting;
  bool terminating;
};

struct opts {
  bool invert;  // -v
  bool full;    // -x
  bool ignore;  // -i
  bool lineno;  // -n
  bool count;   // -c
  char *regex;  // <regex>
  char **files; // [files...]
};

#define DESC "LTREP - print lines matching a regex\n"
#define HELP "Try 'ltrep -h' for more information.\n"
#define USAGE                                                                  \
  "Usage:\n"                                                                   \
  "  ltrep [options...] <regex> [files...]\n"
#define OPTS                                                                   \
  "Options:\n"                                                                 \
  "  -v  invert match; print non-matching lines\n"                             \
  "  -x  full match; match against whole lines\n"                              \
  "  -i  ignore case; match case-insensitively\n"                              \
  "  -n  prefix matched lines with line number\n"                              \
  "  -c  only print a count of matched lines\n"                                \
  "  -h  display this help message and exit\n"

struct opts parse_opts(int argc, char **argv) {
  argc--, argv++;
  struct opts opts = {0};

  if (argc == 0)
    fputs(DESC HELP, stdout), exit(EXIT_FAILURE);

  while (argc) {
    if (argv[0][0] != '-' || argv[0][2] != '\0')
      break;

    switch (argv[0][1]) {
    case 'v':
      opts.invert = true;
      break;
    case 'x':
      opts.full = true;
      break;
    case 'i':
      opts.ignore = true;
      break;
    case 'n':
      opts.lineno = true;
      break;
    case 'c':
      opts.count = true;
      break;
    case 'h':
      fputs(DESC USAGE OPTS, stdout), exit(EXIT_SUCCESS);
    default:
    usage:
      fputs(USAGE HELP, stdout), exit(EXIT_FAILURE);
    }

    argc--, argv++;
  }

  if (argc < 1)
    goto usage;

  opts.regex = argv[0];
  argc--, argv++;
  opts.files = argv;
  return opts;
}

int main(int argc, char **argv) {
  struct opts opts = parse_opts(argc, argv);

  char *error = NULL, *loc = opts.regex;
  struct nfa nfa = ltre_parse(&loc, &error);
  if (error)
    fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc),
        exit(EXIT_FAILURE);
  if (!opts.full)
    ltre_partial(&nfa);
  if (opts.ignore)
    ltre_ignorecase(&nfa);
  if (opts.invert)
    ltre_complement(&nfa);
  struct dstate *dfa = ltre_compile(nfa);

  int lineno = 0;
  int count = 0;

#define OUTPUT_IF(COND)                                                        \
  lineno++;                                                                    \
  if (COND) {                                                                  \
    count++;                                                                   \
    if (opts.count)                                                            \
      continue;                                                                \
    if (opts.lineno)                                                           \
      printf("%d:", lineno);                                                   \
    fwrite(line, sizeof(uint8_t), nl - line + 1, stdout);                      \
  }

  if (!*opts.files) {
    // read from stdin
    size_t len = 0, cap = 256;
    uint8_t *nl, *line = malloc(cap);
    while (fgets((char *)line + len, cap - len, stdin) != NULL) {
      if ((nl = memchr(line + len, '\n', cap - len)) == NULL) {
        len = cap - 1, line = realloc(line, cap *= 2);
        continue;
      }
      *nl = '\0', len = 0;

      OUTPUT_IF(ltre_matches(dfa, line) && (*nl = '\n'));
    }

    if (!feof(stdin))
      perror("fgets"), exit(EXIT_FAILURE);
    free(line);
  }

  for (char **file = opts.files; *file; file++) {
    // memory-map file
    int fd = open(*file, O_RDONLY);
    if (fd == -1)
      perror("open"), exit(EXIT_FAILURE);
    size_t len = lseek(fd, 0, SEEK_END);
    uint8_t *data = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

    // intertwine `ltre_matches` within walking the file for maximum
    // performance
    struct dstate *dstate = dfa;
    uint8_t *nl = data, *line = data;
    for (; nl < data + len; line = ++nl) {
      for (dstate = dfa; !dstate->terminating && *nl != '\n' && nl < data + len;
           nl++)
        dstate = dstate->transitions[*nl];
      if (*nl != '\n') {
        nl = memchr(nl, '\n', data + len - nl);
        nl = nl ? nl : data + len;
      }

      if (opts.files[1] && dstate->accepting)
        printf("%s:", *file);
      OUTPUT_IF(dstate->accepting);
    }

    if (close(fd) == -1)
      perror("close"), exit(EXIT_FAILURE);
  }

  if (opts.count)
    printf("%d\n", count);

  nfa_free(nfa), dfa_free(dfa);
}
