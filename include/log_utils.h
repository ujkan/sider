#include <stdarg.h>
#include <stdio.h>
static inline void print_message(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  printf("\n");
  va_end(args);
}
#define PRINTLN(...) print_message(__VA_ARGS__)
#ifdef DEBUG
#define DPRINT(format, ...) printf(format "\n", ##__VA_ARGS__)
#else
#define DPRINT(format, ...) ((void)0) /* do nothing */
#endif
#define PASS PRINT("Test passed");
#define FAIL PRINT("Test failed");
