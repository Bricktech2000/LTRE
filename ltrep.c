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
  bool accepting, terminating;
};

// '-S' is dealt with separately in `parse_args`
char *opts = "v xpisF HhnNb o c l ";
struct args {
  struct {
    bool invert;  // -v
    bool exact;   // -x/-p
    bool ignore;  // -i/-s
    bool fixed;   // -F
    bool filehd;  // -H/-h
    bool lineno;  // -n/-N
    bool byteoff; // -b
    bool onlymat; // -o
    bool count;   // -c
    bool list;    // -l
  } opts;
  char *pattern; // <pattern>
  char **files;  // [files...]
};

#define VER "LTREP 0.2\n"
#define DESC "LTREP --- print lines matching a pattern\n"
#define HELP "Try 'ltrep -h' for more information.\n"
#define USAGE                                                                  \
  "Usage:\n"                                                                   \
  "  ltrep [options...] [--] <pattern> [files...]\n"                           \
  "  ltrep [options...] -h,-V\n"
#define OPTS                                                                   \
  "Options:\n"                                                                 \
  "  -v     invert match; print non-matching lines\n"                          \
  "  -x/-p  exact match; match against entire line\n"                          \
  "  -i/-s  ignore case; match case-insensitively\n"                           \
  "  -S     smart case; set '-i' if pattern lowercase\n"                       \
  "  -F     interpret the pattern as a fixed string\n"                         \
  "  -H/-h  prefix matching lines with file names\n"                           \
  "  -n/-N  prefix matching lines with line numbers\n"                         \
  "  -b     prefix matching lines with byte offsets\n"                         \
  "  -o     print the matching part of a line only\n"                          \
  "  -c     only print a count of matching lines\n"                            \
  "  -l     only print a list of files with matches\n"
#define EXTRA                                                                  \
  "Options '-i/-s' and '-S' override eachother.\n"                             \
  "A '--' is needed when <pattern> begins in '-'.\n"                           \
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

      smartcase &= *opt != 'i' && *opt != 's'; // '-i/-s' override '-S'
    }
  }

  if (!*argv)
    fputs(USAGE HELP, stdout), exit(EXIT_FAILURE);
  args.pattern = *argv;
  args.files = ++argv;

  if (smartcase) {
    // not trying to be clever here. /\D/ and /\x6A/, for instance, are treated
    // as uppercase and cause matches to become case-sensitive. probably not
    // much of an issue because one could write /^\d/ and /\x6a/ instead
    args.opts.ignore = true; // '-S' overrides '-i/-s'
    for (char *c = args.pattern; *c; c++)
      args.opts.ignore &= !isupper(*c);
  }

  return args;
}

int main(int argc, char **argv) {
  struct args args = parse_args(argv);

  char *error = NULL, *loc = args.pattern;
  struct regex *regex =
      args.opts.fixed ? ltre_fixed_string(loc) : ltre_parse(&loc, &error);
  if (error)
    fprintf(stderr, "parse error: %s near '%.16s'\n", error, loc),
        exit(EXIT_FAILURE);

  // swapping checks for `args.exact` and `args.ignore` would not affect the
  // accepted language, but swapping checks for `args.exact` and `args.invert`
  // or swapping checks for `args.ignore` and `args.invert` would. we check
  // for `args.invert` last to preserve that:
  // - `ltrep -x -vp` means _does not contain_
  // - `ltrep -x -vi` means _is not a case variation of_
  // - `ltrep -x -vpi` means _does not contain any case variation of_
  //
  // given a regex /abc/, for match boundary extraction (when '-o' is supplied
  // and matching is partial and not inverted), we construct:
  // 1. the regular partial DFA `dfa` that matches /<>*abc<>*/, which we run
  //    in the usual way to filter out lines that don't contain matches;
  // 2. a "reverse DFA" `rev_dfa` that matches /<>*cba/, which we run from the
  //    end of a line to find the spots where a partial match can begin;
  // 3. a "forward DFA" `fwd_dfa` that matches /abc/, which we run from the
  //    beginning of a partial match to find the spots where it can end.
  // by interpreting those "can"s in the right way, we guarantee leftmost-
  // longest semantics for match boundary extraction

  struct dstate *rev_dfa = NULL, *fwd_dfa = NULL;

  if (args.opts.ignore)
    regex = regex_ignorecase(regex, false);
  if (args.opts.onlymat && !args.opts.exact && !args.opts.invert &&
      !args.opts.count && !args.opts.list) {
    fwd_dfa = ltre_compile(regex_incref(regex));
    rev_dfa = ltre_compile(regex_reverse(
        regex_concat(REGEXES(regex_incref(regex), regex_univ()))));
  }
  if (!args.opts.exact)
    regex = regex_concat(REGEXES(regex_univ(), regex, regex_univ()));
  if (args.opts.invert)
    regex = regex_compl(regex);

  struct dstate *dfa = ltre_compile(regex);

#define OUTPUT_LINE                                                            \
  do {                                                                         \
    if (args.opts.count || args.opts.list)                                     \
      break;                                                                   \
    uint8_t *p, *begin = line, *end = nl;                                      \
    if (args.opts.onlymat && !args.opts.exact && !args.opts.invert) {          \
      p = end; /* leftmost */                                                  \
      for (struct dstate *dstate = rev_dfa;                                    \
           dstate->accepting ? begin = p : 0, p > line;)                       \
        dstate = dstate->transitions[*--p];                                    \
      p = begin; /* longest */                                                 \
      for (struct dstate *dstate = fwd_dfa;                                    \
           dstate->accepting ? end = p : 0, p < nl;)                           \
        dstate = dstate->transitions[*p++];                                    \
    }                                                                          \
    if (args.opts.filehd)                                                      \
      printf("%s:", *file);                                                    \
    if (args.opts.lineno)                                                      \
      printf("%zu:", lineno);                                                  \
    if (args.opts.byteoff)                                                     \
      printf("%zu:", begin - line + lineoff);                                  \
    fwrite(begin, sizeof(*begin), end - begin, stdout);                        \
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
      printf("%zu:", lineno);                                                  \
    if (args.opts.count)                                                       \
      printf("%zu\n", count);                                                  \
    else if (args.opts.list)                                                   \
      printf("%s\n", *file);                                                   \
  } while (0)

  if (!*args.files) {
  read_stdin:;
    char **file = &(char *){"<stdin>"}; // fun
    size_t lineno = 0, count = 0, lineoff = 0;
    size_t len = 0, cap = 256;
    uint8_t *nl, *line = malloc(cap);
    while (fgets((char *)line + len, cap - len, stdin) != NULL) {
      if ((nl = memchr(line + len, '\n', cap - len)) == NULL) {
        if (feof(stdin) ? nl = memchr(line + len, '\0', cap - len) : 0)
          break;
        len = cap - 1, line = realloc(line, cap *= 2);
        continue;
      }
      *nl = '\0', len = 0;

      if (lineno++, ltre_matches(dfa, line) && ++count)
        OUTPUT_LINE;
      lineoff += nl - line + 1;
    }

    if (!feof(stdin))
      perror("fgets"), exit(EXIT_FAILURE);
    free(line);

    OUTPUT_FILE;

    // clear EOF indicator in case a file of '-' is supplied more than once
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
    size_t lineno = 0, count = 0, lineoff = 0;
    struct dstate *dstate = dfa;
    uint8_t *nl = data, *line = data;
    for (; nl < data + len; line = ++nl) {
      for (dstate = dfa;
           !dstate->terminating && nl < data + len && *nl != '\n';)
        dstate = dstate->transitions[*nl++];
      if (nl < data + len && *nl != '\n') {
        nl = memchr(nl, '\n', data + len - nl);
        nl = nl ? nl : data + len;
      }

      if (lineno++, dstate->accepting && ++count)
        OUTPUT_LINE;
      lineoff += nl - line + 1;
    }

    if (close(fd) == -1)
      perror("close"), exit(EXIT_FAILURE);

    OUTPUT_FILE;
  }

  dfa_free(dfa);
  dfa_free(rev_dfa), dfa_free(fwd_dfa);
}
