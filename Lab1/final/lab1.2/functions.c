#include "functions.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

void doWrite(int fd, const char *buff, int len) {
  ssize_t wcnt;
  ssize_t idx = 0;

  // using idx to verify that all the data is written to the outfile
  do {
    wcnt = write(fd, buff + idx, len - idx);
    if (wcnt == -1) {
      perror("Writing outfile");
      exit(EXIT_FAILURE);
    }
    idx += wcnt;
  } while (idx < len);
}

void write_file(int fd, const char *infile) {
  int open_flags = O_RDONLY;
  int open_mode = S_IRUSR;
  int inf = open(infile, open_flags, open_mode);
  if (inf == -1) {
    perror(infile);
    exit(EXIT_FAILURE);
  }

  char buff[BUFFER_SIZE];
  ssize_t rcnt;

  // write into buffer and call doWrite to write into the outfile.
  // If buffer is not enough (for reading infile), repeat
  for (;;) {
    rcnt = read(inf, buff, sizeof(buff) - 1);
    if (rcnt == 0) break;
    if (rcnt == -1) {
      perror(infile);
      exit(EXIT_FAILURE);
    }
    buff[rcnt] = '\0';

    doWrite(fd, buff, rcnt);
  }

  if (close(inf) == -1) {
    perror("Error closing file");
    exit(EXIT_FAILURE);
  }
  return;
}

int check_file(const char *infile) {
  int open_flags = O_RDONLY;
  int open_mode = S_IRUSR;
  int inf = open(infile, open_flags, open_mode);

  if (inf == -1) return 1;
  if (close(inf) == -1) return 1;
  return 0;
}
