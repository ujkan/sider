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
#define DPRINTLN(format, ...) printf(format "\n", ##__VA_ARGS__)
#else
#define DPRINTLN(format, ...) ((void)0) /* do nothing */
#endif
#define PASS PRINTLN("Test passed");
#define FAIL PRINTLN("Test failed");
