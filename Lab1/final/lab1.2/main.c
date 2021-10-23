#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "functions.h"

int main(int argc, char const *argv[]) {
  if (argc < 3 || argc > 4) {
    printf("Usage: ./fconc infile1 infile2 [outfile (default:fconc.out)]\n");
    return 0;
  }
  const char *infile1 = argv[1];
  const char *infile2 = argv[2];
  const char *outfile = (argc == 4) ? argv[3] : "fconc.out";

  // checks if both infiles exist, and if at least one doesn't then the outfile
  // is not created
  if (check_file(infile1) != 0) {
    perror(infile1);
    exit(EXIT_FAILURE);
  }
  if (check_file(infile2) != 0) {
    perror(infile2);
    exit(EXIT_FAILURE);
  }

  int open_flags = O_CREAT | O_WRONLY | O_TRUNC;
  int open_mode = S_IRUSR | S_IWUSR;

  int outf = open(outfile, open_flags, open_mode);
  if (outf == -1) {
    perror(outfile);
    exit(EXIT_FAILURE);
  }

  write_file(outf, infile1);
  write_file(outf, infile2);

  if (close(outf) == -1) {
    perror("Close");
    exit(EXIT_FAILURE);
  }
  return 0;
}
