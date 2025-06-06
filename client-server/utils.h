#include <stddef.h>
#include <stdint.h>

static const size_t k_max_msg = 4096;
void die(char *message);
void msg(char *message);
int32_t read_full(int fd, char *buf, size_t count);
int32_t write_full(int fd, const char *buf, size_t count);
