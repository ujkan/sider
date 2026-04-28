#include "hex_dump.h"
#include "ctype.h"
void hex_dump(const void *data, size_t size) {
  const unsigned char *p = (const unsigned char *)data;
  size_t i, j;

  for (i = 0; i < size; i += 16) {
    // 1. Print the offset (current memory position)
    printf("%04zx: ", i);

    // 2. Print Hexadecimal bytes
    for (j = 0; j < 16; j++) {
      if (i + j < size) {
        printf("%02x ", p[i + j]);
      } else {
        printf("   "); // Padding for alignment on the last line
      }
    }

    printf(" | ");

    // 3. Print ASCII characters
    for (j = 0; j < 16; j++) {
      if (i + j < size) {
        unsigned char c = p[i + j];
        // Check if char is printable; otherwise print a dot
        if (isprint(c)) {
          printf("%c", c);
        } else {
          printf(".");
        }
      }
    }
    printf("\n");
  }
}
