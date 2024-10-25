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

const char *opts = "vxiFnHch";
struct args {
  struct {
    bool invert; // -v
    bool full;   // -x
    bool ignore; // -i
    bool fixed;  // -F
    bool lineno; // -n
    bool file;   // -H
    bool count;  // -c
    bool help;   // -h
  } opts;
  char *regex;  // <regex>
  char **files; // [files...]
};

#define DESC "LTREP - print lines matching a regex\n"
#define HELP "Try 'ltrep -h' for more information.\n"
#define USAGE                                                                  \
  "Usage:\n"                                                                   \
  "  ltrep [options...] [--] <regex> [files...]\n"
#define OPTS                                                                   \
  "Options:\n"                                                                 \
  "  -v  invert match; print non-matching lines\n"                             \
  "  -x  full match; match against whole lines\n"                              \
  "  -i  ignore case; match case-insensitively\n"                              \
  "  -F  interpret the regex as a fixed string\n"                              \
  "  -n  prefix matching lines with line numbers\n"                            \
  "  -H  prefix matching lines with file names\n"                              \
  "  -c  only print a count of matching lines\n"                               \
  "  -h  display this help message and exit\n"                                 \
  "A '--' is needed when <regex> begins in '-'.\n"                             \
  "A file of '-' denotes standard input. If no\n"                              \
  "files are provided, read from standard input.\n"
#define INV "Unrecognized option '-%.*s'\n"

struct args parse_args(char **argv) {
  struct args args = {0};

  if (!*++argv)
    fputs(DESC HELP, stdout), exit(EXIT_FAILURE);

  for (; *argv && **argv == '-'; argv++) {
    if (strcmp(*argv, "--") == 0 && argv++)
      break;
    for (char *p, *opt = *argv + 1; *opt; opt++) {
      if (p = strchr(opts, *opt))
        (&args.opts.invert)[p - opts] = true;
      else
        printf(INV HELP, *opt == '-' ? -1 : 1, opt), exit(EXIT_FAILURE);
    }
  }

  if (args.opts.help)
    fputs(DESC USAGE OPTS, stdout), exit(EXIT_SUCCESS);

  if (!*argv)
    fputs(USAGE HELP, stdout), exit(EXIT_FAILURE);
  args.regex = *argv;
  args.files = ++argv;

  return args;
}

int main(int argc, char **argv) {
  struct args args = parse_args(argv);

  char *error = NULL, *loc = args.regex;
  struct nfa nfa =
      args.opts.fixed ? ltre_fixed_string(loc) : ltre_parse(&loc, &error);
  if (error)
    fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc),
        exit(EXIT_FAILURE);

  // swapping `ltre_partial` and `ltre_ignorecase` would not affect the accepted
  // language, but swapping `ltre_partial` and `ltre_complement` or swapping
  // `ltre_ignorecase` and `ltre_complement` would. we perform `ltre_complement`
  // last to preserve that:
  // - `ltrep -v` means _does not contain_
  // - `ltrep -vi` means _does not contain any case variation of_
  // - `ltrep -vxi` means _is not a case variation of_
  if (!args.opts.full)
    ltre_partial(&nfa);
  if (args.opts.ignore)
    ltre_ignorecase(&nfa);
  if (args.opts.invert)
    ltre_complement(&nfa);

  struct dstate *dfa = ltre_compile(nfa);

  int count = 0;

#define OUTPUT_IF(COND)                                                        \
  lineno++;                                                                    \
  if (COND) {                                                                  \
    count++;                                                                   \
    if (args.opts.count)                                                       \
      continue;                                                                \
    if (args.opts.file)                                                        \
      printf("%s:", *file);                                                    \
    if (args.opts.lineno)                                                      \
      printf("%d:", lineno);                                                   \
    fwrite(line, sizeof(uint8_t), nl - line, stdout);                          \
    fputc('\n', stdout);                                                       \
  }

  if (!*args.files) {
  read_stdin:;
    int lineno = 0;
    size_t len = 0, cap = 256;
    uint8_t *nl, *line = malloc(cap);
    while (fgets((char *)line + len, cap - len, stdin) != NULL) {
      if ((nl = memchr(line + len, '\n', cap - len)) == NULL) {
        if (!feof(stdin)) {
          len = cap - 1, line = realloc(line, cap *= 2);
          continue;
        }
        nl = memchr(line + len, '\0', cap - len);
      }
      *nl = '\0', len = 0;

      char **file = &(char *){"<stdin>"}; // fun
      OUTPUT_IF(ltre_matches(dfa, line));
    }

    if (!feof(stdin))
      perror("fgets"), exit(EXIT_FAILURE);
    free(line);

    // clear EOF indicator in case a file of `-` is supplied more than once
    clearerr(stdin);
  }

  for (char **file; *(file = args.files++);) {
    if (strcmp(*file, "-") == 0)
      goto read_stdin;

    int fd = open(*file, O_RDONLY);
    if (fd == -1)
      perror("open"), exit(EXIT_FAILURE);
    size_t len = lseek(fd, 0, SEEK_END);
    uint8_t *data = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

    // intertwine `ltre_matches` within walking the file for maximum performance
    int lineno = 0;
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

      OUTPUT_IF(dstate->accepting);
    }

    if (close(fd) == -1)
      perror("close"), exit(EXIT_FAILURE);
  }

  if (args.opts.count)
    printf("%d\n", count);

  nfa_free(nfa), dfa_free(dfa);
}
