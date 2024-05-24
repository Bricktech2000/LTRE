#define _GNU_SOURCE // getline
#include "ltre.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

// steal implementation details
struct dstate {
  struct dstate *transitions[256];
  bool accepting;
};

struct opts {
  bool invert; // -v
  bool full;   // -x
  bool lineno; // -n
  bool count;  // -c
  char *regex; // <regex>
  char *file;  // [file]
};

#define DESC "LTREP - print lines matching a regex\n"
#define HELP "Try 'ltrep -h' for more information.\n"
#define USAGE                                                                  \
  "Usage:\n"                                                                   \
  "  ltrep [options...] <regex> [file]\n"
#define OPTS                                                                   \
  "Options:\n"                                                                 \
  "  -v  invert match; print non-matching lines\n"                             \
  "  -x  full match; match against whole lines\n"                              \
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

  if (argc < 1 || argc > 2)
    goto usage;

  opts.regex = argv[0];
  opts.file = argc == 2 ? argv[1] : NULL;
  return opts;
}

int main(int argc, char **argv) {
  struct opts opts = parse_opts(argc, argv);

  char *error, *regex = opts.regex;
  struct nstate *nfa = ltre_parse(&regex, &error);
  if (nfa == NULL)
    fprintf(stderr, "parse error: %s near '%.16s'\n", error, regex),
        exit(EXIT_FAILURE);
  struct dstate *dfa =
      opts.full ? ltre_compile_full(nfa) : ltre_compile_part(nfa);

  int lineno = 0;
  int count = 0;

  if (opts.file) {
    // memory-map file
    int fd = open(opts.file, O_RDONLY);
    if (fd == -1)
      perror("open"), exit(EXIT_FAILURE);
    size_t len = lseek(fd, 0, SEEK_END);
    uint8_t *data = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

    struct dstate *dstate = dfa;
    uint8_t *line = data, *curr = data;
    for (; curr < data + len; curr++) {
      dstate = dstate->transitions[*curr];
      if (*curr != '\n')
        continue;
    write:
      lineno++;
      if (dstate->accepting ^ opts.invert) {
        count++;
        if (opts.lineno)
          printf("%d:", lineno);
        if (!opts.count)
          fwrite(line, sizeof(uint8_t), curr - line + 1, stdout);
      }
      line = curr + 1;
      dstate = dfa;
    }

    if (curr != line)
      goto write;
  } else {
    // read from stdin
    uint8_t *line = NULL;
    size_t len = 0;
    ssize_t nread;
    while ((nread = getline((char **)&line, &len, stdin)) != -1) {
      lineno++;
      if (ltre_matches(dfa, line) ^ opts.invert) {
        count++;
        if (opts.lineno)
          printf("%d:", lineno);
        if (!opts.count)
          fwrite(line, sizeof(uint8_t), nread, stdout);
      }
    }
    if (errno)
      perror("getline"), exit(EXIT_FAILURE);
    free(line);
  }

  if (opts.count)
    printf("%d\n", count);

  nfa_free(nfa), dfa_free(dfa);
}
