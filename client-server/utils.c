#include "utils.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_ssize_t.h>
#include <unistd.h>

void die(char *message) {
  fprintf(stderr, "%s\n", message);
  exit(1);
}

void msg(char *message) { fprintf(stderr, "%s\n", message); }
void msgn(char *message) { fprintf(stderr, "%s", message); }

int32_t read_full(int fd, char *buf, size_t count) {
  ssize_t n_read;
  size_t n_left = count;
  while (n_left > 0) {
    n_read = read(fd, buf, n_left);
    if (n_read < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        return -1;
      }
    } else if (n_read == 0) {
      break; // EOF
    }
    n_left -= n_read;
    buf += n_read;
  }

  return count - n_read; // 0 if successful full read; > 0 if EOF; -1 if error
}

int32_t write_full(int fd, const char *buf, size_t count) {
  size_t n_left = count;
  ssize_t n_written;
  while (n_left > 0) {
    n_written = write(fd, buf, n_left);
    if (n_written <= 0) {
      if (errno == EINTR) {
        continue;
      } else {
        return -1;
      }
    }
    n_left -= n_written;
    buf += n_written;
  }
  return count - n_written; // should be 0
}

