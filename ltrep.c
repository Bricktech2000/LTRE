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

// steal implementation details. this probably has undefined behavior
struct dstate {
  struct dstate *transitions[256];
  bool accepting, terminating;
};

char *opts = "v pxo isS FEHhnNkKb Ttc l L 0 zZq ";
struct args {
  struct {
    bool invert;  // -v
    bool partial; // -p/-x
    bool onlymat; // -o
    bool ignore;  // -i/-s
    bool smart;   // -S
    bool fixed;   // -F
    bool filehd;  // -H/-h
    bool lineno;  // -n/-N
    bool colno;   // -k/-K
    bool byteoff; // -b
    bool inittab; // -T/-t
    bool count;   // -c
    bool list;    // -l
    bool nlist;   // -L
    bool nulname; // -0
    bool nuldata; // -z/-Z
    bool quiet;   // -q
  } opts;
  char *pattern; // <pattern>
  char **files;  // [files...]
};

enum { EXIT_MATCH, EXIT_NOMATCH, EXIT_ERROR };

// the fundamental problem with a --multiline mode is quoting: in the output
// stream, how do you communicate that one match ends and the next one begins?
// let's take a closer look at line terminators (as in, \n when -Z and \0 when
// -z). with -x, line terminators terminate lines, each line is a word (a
// formal-language-theory word), and we output matching words. -p is the same
// but the pattern is first surrounded by a pair of wildcards. but for -o, there
// are two equivalent perspectives:
//  1. the input stream is a sequence of lines, line terminators terminate each
//     such line, each line is a string, and we output the substrings that are
//     matching words.
//  2. the input stream is a homogeneous blob of data, line terminators are
//     sentinel bytes, and the user asks that any match containig such sentinels
//     be ignored so we can use them to delineate matches in the output stream.
// perspective 2 with line terminator \0 is our multiline mode. matches that
// contain \0 bytes are ignored, but it must be so because \0 bytes are what's
// used in the output to delineate matches. for reporting matches that contain
// both \n and \0 bytes, you might use tr(1).

#define VER "LTREP 0.3\n"
#define DESC "LTREP --- print lines matching a pattern\n"
#define HELP "Try 'ltrep -h' for more information.\n"
#define USAGE                                                                  \
  "Usage:\n"                                                                   \
  "  ltrep [options...] [--] <pattern> [files...]\n"                           \
  "  ltrep [options...] -h,-V\n"
#define OPTS                                                                   \
  "Options:\n"                                                                 \
  "  -v     invert match; print non-matching lines\n"                          \
  "  -p/-x  partial match; print lines that contain a match\n"                 \
  "  -o     match only; print the matches, not the lines\n"                    \
  "  -i/-s  ignore case; match case-insensitively\n"                           \
  "  -S     smart case; set '-i' if pattern is all-lowercase\n"                \
  "  -F/-E  parse the pattern as a fixed string, not a regex\n"                \
  "  -H/-h  prefix matches with their file name\n"                             \
  "  -n/-N  prefix matches with their 1-based line number\n"                   \
  "  -k/-K  prefix matches with their 1-based column number\n"                 \
  "  -b     prefix matches with their 0-based byte offset\n"                   \
  "  -T/-t  prefix matches with a \\t character\n"                             \
  "  -c     only print a count of matching lines per file\n"                   \
  "  -l     only print a list of files containing matches\n"                   \
  "  -L     only print a list of files containing no matches\n"                \
  "  -0     terminate all file names with \\0, not : or \\n\n"                 \
  "  -z/-Z  use line terminator \\0 for input and output data\n"               \
  "  -q     produce no output and prioritize exit status 0\n"
#define EXTRA                                                                  \
  "Options '-i/-s' and '-S' override eachother.\n"                             \
  "Options '-p/-x' and '-o' override eachother.\n"                             \
  "A '--' is needed when <pattern> begins in '-'.\n"                           \
  "A file of '-' denotes standard input. If no\n"                              \
  "files are provided, read from standard input.\n"                            \
  "Exit status is 2 if errors occurred, else 0 if\n"                           \
  "a line matched, else 1; with '-q' it is 0 if a\n"                           \
  "line matched, else 2 if errors occurred, else 1.\n"                         \
  "Show help and version info with '-h' and '-V'.\n"
#define INV "Unrecognized option '-%.*s'.\n"

struct args parse_args(char **argv) {
  struct args args = {0};

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
      if ((p = strchr(opts, *opt)) && *opt != ' ')
        ((bool *)&args.opts)[p - opts >> 1] = !(p - opts & 1);
      else
        fprintf(stderr, INV HELP, *opt == '-' ? -1 : 1, opt), exit(EXIT_ERROR);

      args.opts.smart &= *opt != 'i' && *opt != 's';   // '-i/-s' override '-S'
      args.opts.onlymat &= *opt != 'p' && *opt != 'x'; // '-p/-x' override '-o'
    }
  }

  if (!*argv)
    fputs(USAGE HELP, stderr), exit(EXIT_ERROR);
  args.pattern = *argv++;
  static char *read_stdin[] = {"-", NULL};
  args.files = *argv ? argv : read_stdin;

  args.opts.partial |= args.opts.onlymat; // '-o' overrides '-p/-x'
  args.opts.ignore |= args.opts.smart;    // '-S' overrides '-i/-s'

  // not trying to be clever here. /\D/ and /\x6A/, for example, are treated as
  // uppercase and cause matches to become case sensitive. probably not much of
  // an issue because one could write /~\d/ and /\x6a/ instead
  if (args.opts.smart)
    for (char *c = args.pattern; *c; c++)
      args.opts.ignore &= !isupper(*c);

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

  // swapping checks for `args.partial` and `args.ignore` would not affect the
  // accepted language, but swapping checks for `args.partial` and `args.invert`
  // or swapping checks for `args.ignore` and `args.invert` would. we check
  // for `args.invert` last to preserve that:
  //   - `ltrep -vp` means 'does not contain'
  //   - `ltrep -vi` means 'is not a case variation of'
  //   - `ltrep -vpi` means 'does not contain any case variation of'
  //
  // given a regex /abc/, for match boundary extraction (when '-o' is supplied
  // and matching is not inverted), we construct:
  //  1. the regular partial DFA `dfa` that matches /%abc%/, which we run in
  //     the usual way to filter out lines that don't contain matches;
  //  2. a "reverse DFA" `rev_dfa` that matches /%cba/, which we run from the
  //     end of a line to find the spots where a partial match can begin;
  //  3. a "forward DFA" `fwd_dfa` that matches /abc/, which we run from the
  //     beginning of a partial match to find the spots where it can end.
  // so we perform one forward scan, then one backward scan within which we
  // perform several forward scans. the alternative would be to construct:
  //  1. a DFA that matches /%abc/, which we run forward from the start of a
  //     line to find the spots where a partial match can end; and
  //  2. a DFA that matches /cba/, which we run backward from the end of a
  //     partial match to find the spots where it can begin.
  // but that means performing one forward scan within which we perform several
  // backward scans, and that's not as nice to the prefetcher

  struct dstate *rev_dfa = NULL, *fwd_dfa = NULL;

  if (args.opts.ignore)
    regex = regex_ignorecase(regex, false);
  if (args.opts.onlymat && !args.opts.invert && !args.opts.count &&
      !args.opts.list && !args.opts.nlist) {
    fwd_dfa = ltre_compile(regex_incref(regex));
    rev_dfa = ltre_compile(regex_reverse(
        regex_concat(REGEXES(regex_incref(regex), regex_univ()))));
  }
  if (args.opts.partial)
    regex = regex_concat(REGEXES(regex_univ(), regex, regex_univ()));
  if (args.opts.invert)
    regex = regex_compl(regex);

  struct dstate *dfa = ltre_compile(regex);

  // be extremely careful with -o: in general, the space and time complexity
  // becomes quadratic in the input length. to preserve linear-time, linear-
  // space guarantees it is sufficient (and necessary, when other options don't
  // suppress any output) to show, for a given pattern, that there is a K such
  // that every input character is contained in at most K distinct matches. for
  // patterns with bounded match length this is always the case.

#define OUTPUT_MATCH /* args.opts, file, lineno, lineoff, line, begin, end */  \
  do {                                                                         \
    if (args.opts.count || args.opts.list || args.opts.nlist)                  \
      break;                                                                   \
    if (args.opts.quiet)                                                       \
      break;                                                                   \
                                                                               \
    if (args.opts.filehd)                                                      \
      printf("%s%c", *file, args.opts.nulname ? '\0' : ':');                   \
    if (args.opts.lineno)                                                      \
      printf("%zu:", lineno + 1);                                              \
    if (args.opts.colno)                                                       \
      printf("%zu:", begin - line + 1);                                        \
    if (args.opts.byteoff)                                                     \
      printf("%zu:", begin - line + lineoff);                                  \
    if (args.opts.inittab)                                                     \
      putchar('\t');                                                           \
    fwrite(begin, sizeof(*begin), end - begin, stdout);                        \
    putchar(args.opts.nuldata ? '\0' : '\n');                                  \
  } while (0)

#define OUTPUT_LINE /* args.opts, file, lineno, lineoff, line, len, dfa */     \
  do {                                                                         \
    if (args.opts.count || args.opts.list || args.opts.nlist)                  \
      break;                                                                   \
    if (args.opts.quiet)                                                       \
      break;                                                                   \
                                                                               \
    if (args.opts.onlymat && !args.opts.invert) {                              \
      uint8_t *begin = line + len; /* rightmost to leftmost */                 \
      for (struct dstate *dstate = rev_dfa;;                                   \
           dstate = dstate->transitions[*--begin]) {                           \
        if (dstate->accepting) {                                               \
          uint8_t *end = begin; /* shortest to longest */                      \
          for (struct dstate *dstate = fwd_dfa;;                               \
               dstate = dstate->transitions[*end++]) {                         \
            if (dstate->accepting)                                             \
              OUTPUT_MATCH;                                                    \
            else if (dstate->terminating)                                      \
              break;                                                           \
            if (end == line + len)                                             \
              break;                                                           \
          }                                                                    \
        } else if (dstate->terminating)                                        \
          break;                                                               \
        if (begin == line)                                                     \
          break;                                                               \
      }                                                                        \
    } else {                                                                   \
      uint8_t *begin = line, *end = line + len;                                \
      OUTPUT_MATCH;                                                            \
    }                                                                          \
  } while (0)

#define OUTPUT_FILE /* args.opts, file, lineno, lineoff, count */              \
  do {                                                                         \
    if (!args.opts.count && (count ? !args.opts.list : !args.opts.nlist))      \
      break;                                                                   \
    if (args.opts.quiet)                                                       \
      break;                                                                   \
                                                                               \
    if (args.opts.filehd)                                                      \
      printf("%s%c", *file, args.opts.nulname ? '\0' : ':');                   \
    if (args.opts.lineno)                                                      \
      printf("%zu:", lineno + 1);                                              \
    if (args.opts.colno)                                                       \
      printf("%zu:", line - line + 1);                                         \
    if (args.opts.byteoff)                                                     \
      printf("%zu:", line - line + lineoff);                                   \
    if (args.opts.count)                                                       \
      printf("%zu\n", count);                                                  \
    else if (args.opts.list || args.opts.nlist)                                \
      printf("%s%c", *file, args.opts.nulname ? '\0' : '\n');                  \
  } while (0)

  int exit_status = EXIT_NOMATCH;
  uint8_t ieol = args.opts.nuldata ? '\0' : '\n';

  for (char **file = args.files; *file; file++) {
    if (args.opts.quiet && exit_status == EXIT_MATCH)
      break; // no need to process more files

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
        while (!dstate->terminating && p < data + size && *p != ieol)
          dstate = dstate->transitions[*p++];
        if (p < data + size && *p != ieol)
          (p = memchr(p, ieol, data + size - p)) || (p = data + size);
        size_t len = p - line;

        if (dstate->accepting && ++count) {
          OUTPUT_LINE;
          if (args.opts.quiet || exit_status != EXIT_ERROR)
            exit_status = EXIT_MATCH; // without '-q', EXIT_ERROR takes priority
          if (args.opts.list && !args.opts.count || args.opts.quiet)
            break; // no need to process more lines
        }
        lineno++, lineoff += len + 1;
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
      for (int c; c = fgetc(fp), c != EOF && c != ieol; line[len++] = c) {
        len == cap ? line = realloc(line, cap *= 2) : 0;
        dstate = dstate->transitions[c];
      }
      if (ferror(fp) && (free(line), 1))
        goto perror_continue;
      if (feof(fp) && len == 0)
        break; // ignore partial line if it's empty

      if (dstate->accepting && ++count) {
        OUTPUT_LINE;
        if (args.opts.quiet || exit_status != EXIT_ERROR)
          exit_status = EXIT_MATCH; // without '-q', EXIT_ERROR takes priority
        if (args.opts.list && !args.opts.count || args.opts.quiet)
          break; // no need to process more lines
      }
      lineno++, lineoff += len + 1;
    }

    free(line);

    if (fp == stdin)
      clearerr(fp); // clear EOF in case file '-' is supplied more than once
    else if (fclose(fp) != 0)
      goto perror_continue;

    OUTPUT_FILE;

    continue;

  perror_continue:
    perror(*file);
    if (!args.opts.quiet || exit_status != EXIT_MATCH)
      exit_status = EXIT_ERROR; // with '-q', EXIT_MATCH takes priority
  }

  dfa_free(dfa);
  dfa_free(rev_dfa), dfa_free(fwd_dfa);

  return exit_status;
}
