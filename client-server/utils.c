#include <stdio.h>
#include <stdlib.h>

void die(char *message) {
  fprintf(stderr, "%s\n", message);
  exit(1);
}

void msg(char *message) {
  fprintf(stderr, "%s\n", message);
}
