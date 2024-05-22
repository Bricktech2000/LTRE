#include "ltre.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct dstate {
  struct dstate *transitions[256];
  struct dstate *next;
  struct dstate *qnext;
  bool accepting;
  uint8_t bitset[];
};

int main(int argc, char **argv) {
  if (argc != 3)
    fprintf(stderr, "usage: %s <regex> <file>\n", argv[0]), exit(EXIT_FAILURE);

  int fd = open(argv[2], O_RDONLY);
  if (fd == -1)
    perror("open"), exit(EXIT_FAILURE);
  size_t len = lseek(fd, 0, SEEK_END);
  uint8_t *data = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

  struct nstate *nfa = ltre_parse(argv[1]);
  if (nfa == NULL)
    fprintf(stderr, "ltre: parse error\n"), exit(EXIT_FAILURE);
  struct dstate *dfa = ltre_compile_part(nfa); // partial match

  struct dstate *dstate = dfa;
  uint8_t *line = data, *curr = data;
  for (; curr < data + len; curr++) {
    dstate = dstate->transitions[*curr];
    if (*curr != '\n')
      continue;
  write:
    if (dstate->accepting)
      fwrite(line, curr - line + 1, 1, stdout);
    line = curr + 1;
    dstate = dfa;
  }

  if (curr != line)
    goto write;

  nfa_free(nfa), dfa_free(dfa);
}
