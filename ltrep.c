#include "ltre.h"
#include <ctype.h>
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

// `-S` is dealt with separately in `parse_args`
const char *opts = "v xpisF nNHhc l ";
struct args {
  struct {
    bool invert; // -v
    bool exact;  // -x/-p
    bool ignore; // -i/-s
    bool fixed;  // -F
    bool lineno; // -n/-N
    bool filehd; // -H/-h
    bool count;  // -c
    bool list;   // -l
  } opts;
  char *regex;  // <regex>
  char **files; // [files...]
};

// TODO test cases for -l
// TODO test cases for -l -n -H -c interplay

#define VER "LTREP 0.1\n"
#define DESC "LTREP --- print lines matching a regex\n"
#define HELP "Try 'ltrep -h' for more information.\n"
#define USAGE                                                                  \
  "Usage:\n"                                                                   \
  "  ltrep [options...] [--] <regex> [files...]\n"                             \
  "  ltrep [options...] -h,-V\n"
#define OPTS                                                                   \
  "Options:\n"                                                                 \
  "  -v     invert match; print non-matching lines\n"                          \
  "  -x/-p  exact match; match against entire line\n"                          \
  "  -i/-s  ignore case; match case-insensitively\n"                           \
  "  -S     smart case; set '-i' if regex lowercase\n"                         \
  "  -F     interpret the regex as a fixed string\n"                           \
  "  -n/-N  prefix matching lines with line numbers\n"                         \
  "  -H/-h  prefix matching lines with file names\n"                           \
  "  -c     only print a count of matching lines\n"                            \
  "  -l     only print names of files with matches\n"
#define EXTRA                                                                  \
  "Options '-i/-s' and '-S' override eachother.\n"                             \
  "A '--' is needed when <regex> begins in '-'.\n"                             \
  "A file of '-' denotes standard input. If no\n"                              \
  "files are provided, read from standard input.\n"                            \
  "Show help and version info with '-h' and '-V'.\n"
#define INV "Unrecognized option '-%.*s'\n"

struct args parse_args(char **argv) {
  struct args args = {0};
  bool smartcase = false; // -S

  if (!*++argv)
    fputs(DESC HELP, stdout), exit(EXIT_FAILURE);

  for (; *argv && **argv == '-'; argv++) {
    if (strcmp(*argv, "--") == 0 && argv++)
      break;
    if (strcmp(*argv, "-h") == 0 && !argv[1])
      fputs(DESC "\n" USAGE "\n" OPTS "\n" EXTRA, stdout), exit(EXIT_SUCCESS);
    if (strcmp(*argv, "-V") == 0 && !argv[1])
      fputs(VER, stdout), exit(EXIT_SUCCESS);

    for (char *p, *opt = *argv + 1; *opt; opt++) {
      if (*opt == 'S')
        smartcase = true;
      else if ((p = strchr(opts, *opt)) && *opt != ' ')
        ((bool *)&args.opts)[p - opts >> 1] = !(p - opts & 1);
      else
        printf(INV HELP, *opt == '-' ? -1 : 1, opt), exit(EXIT_FAILURE);

      smartcase &= *opt != 'i' && *opt != 's'; // `-i/-s` override `-S`
    }
  }

  if (!*argv)
    fputs(USAGE HELP, stdout), exit(EXIT_FAILURE);
  args.regex = *argv;
  args.files = ++argv;

  if (smartcase) {
    // not trying to be clever here. /\D/ and /\x6A/, for instance, are treated
    // as uppercase and cause matches to become case-sensitive. probably not
    // much of an issue because one could write /^\d/ and /\x6a/ instead
    args.opts.ignore = true; // `-S` overrides `-i/-s`
    for (char *c = args.regex; *c; c++)
      args.opts.ignore &= !isupper(*c);
  }

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
  // - `ltrep -x -vp` means _does not contain_
  // - `ltrep -x -vi` means _is not a case variation of_
  // - `ltrep -x -vpi` means _does not contain any case variation of_
  if (!args.opts.exact)
    ltre_partial(&nfa);
  if (args.opts.ignore)
    ltre_ignorecase(&nfa);
  if (args.opts.invert)
    ltre_complement(&nfa);

  struct dstate *dfa = ltre_compile(nfa);

#define OUTPUT_LINE                                                            \
  do {                                                                         \
    if (args.opts.count || args.opts.list)                                     \
      break;                                                                   \
    if (args.opts.filehd)                                                      \
      printf("%s:", *file);                                                    \
    if (args.opts.lineno)                                                      \
      printf("%d:", lineno);                                                   \
    fwrite(line, sizeof(uint8_t), nl - line, stdout);                          \
    fputc('\n', stdout);                                                       \
  } while (0)

#define OUTPUT_FILE                                                            \
  do {                                                                         \
    if (!args.opts.count && !args.opts.list)                                   \
      break;                                                                   \
    if (count == 0)                                                            \
      break;                                                                   \
    if (args.opts.filehd)                                                      \
      printf("%s:", *file);                                                    \
    if (args.opts.lineno)                                                      \
      printf("%d:", lineno);                                                   \
    if (args.opts.count)                                                       \
      printf("%d\n", count);                                                   \
    else if (args.opts.list)                                                   \
      printf("%s\n", *file);                                                   \
  } while (0)

  if (!*args.files) {
  read_stdin:;
    char **file = &(char *){"<stdin>"}; // fun
    int lineno = 0, count = 0;
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

      if (lineno++, ltre_matches(dfa, line) && ++count)
        OUTPUT_LINE;
    }

    if (!feof(stdin))
      perror("fgets"), exit(EXIT_FAILURE);
    free(line);

    OUTPUT_FILE;

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
    int lineno = 0, count = 0;
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

      if (lineno++, dstate->accepting && ++count)
        OUTPUT_LINE;
    }

    if (close(fd) == -1)
      perror("close"), exit(EXIT_FAILURE);

    OUTPUT_FILE;
  }

  nfa_free(nfa), dfa_free(dfa);
}
