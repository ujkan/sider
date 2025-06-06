#include "utils.h"
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int32_t query(int fd, const char *text) {
  uint32_t len = (uint32_t)strlen(text);
  if (len > k_max_msg) {
    return -1;
  }
  // send request
  char wbuf[4 + k_max_msg];
  memcpy(wbuf, &len, 4); // assume little endian
  memcpy(&wbuf[4], text, len);
  int32_t err = write_full(fd, wbuf, 4 + len);
  if (err) {
    return err;
  }
  // 4 bytes header
  char rbuf[4 + k_max_msg + 1];
  errno = 0;
  err = read_full(fd, rbuf, 4);
  if (err) {
    msg(errno == 0 ? "EOF" : "read() error");
    return err;
  }
  memcpy(&len, rbuf, 4); // assume little endian
  if (len > k_max_msg) {
    msg("too long");
    return -1;
  }
  // reply body
  err = read_full(fd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return err;
  }
  // do something
  printf("server says: %.*s\n", len, &rbuf[4]);
  return 0;
}

int main(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  // Q: why ntoh vs. hton?
  // A: it should always be hton, since sockaddr_in expects
  //    its data in network byte order
  //    but, hton and ntoh both have same implementation (just
  //    a byte swap) so code will work fine, just semantics
  //    are not clear
  addr.sin_port = htons(8080);
  // Q: why 127.0.0.1 here vs. 0.0.0.0 in server
  // A: 0.0.0.0 is not an address per se, it's a wildcard
  //    (or meta-address). In the bind in the server code
  //    it's used to tell the system that any address is OK,
  //    we don't care
  //    here, we cannot use that. instead, to tell it we're
  //    looking at stuff in our *local* machine, we specify
  //    the LOOPBACK address, aka 127.0.0.1, which says
  //    the address of this machine
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("connect()");
  }

  int32_t err = query(fd, "hello 1");
  if (err) {
    msg("err?");
    goto L_DONE;
  }

  err = query(fd, "hello 2");
  msg("query 2?");
  if (err) {
    goto L_DONE;
  }

L_DONE:
  close(fd);
  return 0;
}
