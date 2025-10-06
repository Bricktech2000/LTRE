#include "ltre.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __unix__
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

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

enum { EXIT_MATCH, EXIT_NOMATCH, EXIT_ERROR };

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
    fputs(DESC HELP, stderr), exit(EXIT_ERROR);

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
        fprintf(stderr, INV HELP, *opt == '-' ? -1 : 1, opt), exit(EXIT_ERROR);

      smartcase &= *opt != 'i' && *opt != 's'; // '-i/-s' override '-S'
    }
  }

  if (!*argv)
    fputs(USAGE HELP, stderr), exit(EXIT_ERROR);
  args.pattern = *argv++;
  static char *read_stdin[] = {"-", NULL};
  args.files = *argv ? argv : read_stdin;

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
        exit(EXIT_ERROR);

  // swapping checks for `args.exact` and `args.ignore` would not affect the
  // accepted language, but swapping checks for `args.exact` and `args.invert`
  // or swapping checks for `args.ignore` and `args.invert` would. we check
  // for `args.invert` last to preserve that:
  //   - `ltrep -x -vp` means 'does not contain'
  //   - `ltrep -x -vi` means 'is not a case variation of'
  //   - `ltrep -x -vpi` means 'does not contain any case variation of'
  //
  // given a regex /abc/, for match boundary extraction (when '-o' is supplied
  // and matching is partial and not inverted), we construct:
  //  1. the regular partial DFA `dfa` that matches /<>*abc<>*/, which we run
  //     in the usual way to filter out lines that don't contain matches;
  //  2. a "reverse DFA" `rev_dfa` that matches /<>*cba/, which we run from the
  //     end of a line to find the spots where a partial match can begin;
  //  3. a "forward DFA" `fwd_dfa` that matches /abc/, which we run from the
  //     beginning of a partial match to find the spots where it can end.
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

#define OUTPUT_LINE /* args.opts, file, lineno, lineoff, dfa, line, len */     \
  do {                                                                         \
    if (args.opts.count || args.opts.list)                                     \
      break;                                                                   \
    uint8_t *p, *begin = line, *end = line + len;                              \
    if (args.opts.onlymat && !args.opts.exact && !args.opts.invert) {          \
      p = end; /* leftmost */                                                  \
      for (struct dstate *dstate = rev_dfa;                                    \
           dstate->accepting ? begin = p : 0, p > line;)                       \
        dstate = dstate->transitions[*--p];                                    \
      p = begin; /* longest */                                                 \
      for (struct dstate *dstate = fwd_dfa;                                    \
           dstate->accepting ? end = p : 0, p < line + len;)                   \
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

#define OUTPUT_FILE /* args.opts, file, lineno, count */                       \
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

  int exit_status = EXIT_NOMATCH;

  for (char **file = args.files; *file; file++) {
    FILE *fp = NULL;
    if (strcmp(*file, "-") == 0)
      fp = stdin;

#ifdef __unix__
    // on UNIX, when reading from a regular file and not stdin, memory-map the
    // file to improve performance
    else if (1) {
      int fd = open(*file, O_RDONLY);
      if (fd == -1)
        goto perror_continue;
      off_t size = lseek(fd, 0, SEEK_END);
      if (size == -1)
        goto perror_continue;
      uint8_t *data =
          size ? mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0) : &(uint8_t){0};
      if (data == MAP_FAILED)
        goto perror_continue;

      size_t lineno = 0, count = 0, lineoff = 0;
      uint8_t *line = data, *p = data;

      for (; p < data + size; line = ++p) {
        struct dstate *dstate = dfa;
        while (!dstate->terminating && p < data + size && *p != '\n')
          dstate = dstate->transitions[*p++];
        if (p < data + size && *p != '\n')
          (p = memchr(p, '\n', data + size - p)) || (p = data + size);
        size_t len = p - line;

        if (lineno++, dstate->accepting && ++count) {
          if (exit_status != EXIT_ERROR)
            exit_status = EXIT_MATCH;
          OUTPUT_LINE;
        }
        lineoff += len + 1;
      }

      if (size != 0 && munmap(data, size) == -1)
        goto perror_continue;
      if (close(fd) == -1)
        goto perror_continue;

      OUTPUT_FILE;

      continue;
    }
#endif

    else if ((fp = fopen(*file, "r")) == NULL)
      goto perror_continue;

    size_t lineno = 0, count = 0, lineoff = 0;
    size_t len = 0, cap = 256;
    uint8_t *line = malloc(cap);

    for (; !feof(fp); len = 0) {
      struct dstate *dstate = dfa;
      for (int c; c = fgetc(fp), c != EOF && c != '\n'; line[len++] = c) {
        len == cap ? line = realloc(line, cap *= 2) : 0;
        dstate = dstate->transitions[c];
      }
      if (ferror(fp))
        goto perror_continue;
      if (feof(fp) && len == 0)
        break; // ignore partial line if it's empty

      if (lineno++, dstate->accepting && ++count) {
        if (exit_status != EXIT_ERROR)
          exit_status = EXIT_MATCH;
        OUTPUT_LINE;
      }
      lineoff += len + 1;
    }

    free(line);

    if (fp == stdin)
      clearerr(fp); // clear EOF in case file '-' is supplied more than once
    else if (fclose(fp) != 0)
      goto perror_continue;

    OUTPUT_FILE;

    continue;

  perror_continue:
    exit_status = EXIT_ERROR;
    perror(*file);
  }

  dfa_free(dfa);
  dfa_free(rev_dfa), dfa_free(fwd_dfa);

  return exit_status;
}
