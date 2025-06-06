#include "utils.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int32_t one_request(int fd) {
  char len_rbuf[4];
  int32_t rv = read_full(fd, len_rbuf, 4);
  if (rv != 0) {
    msg("one_request, read_full len");
    printf("rv:%d\n", rv);
    return rv;
  }
  uint32_t len = 0; // why not use atoi?
                    // my guess: slower, since it has to parse
                    // we know that len is an int, so memcpy avoids all that
  memcpy(&len, len_rbuf, 4);

  if (len > k_max_msg) {
    msg("too long");
    return -1;
  }

  char msg_rbuf[k_max_msg];
  rv = read_full(fd, msg_rbuf, len);
  if (rv != 0) {
    msg("one_request, read_full msg");
    return rv;
  }
  printf("Client says '%.*s'\n", len, msg_rbuf);

  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)]; // sizeof gives 5+1 (incl. \0); do we need \0?
                                // assume not since we use strlen below!
  uint32_t reply_len = (uint32_t)strlen(reply);
  memcpy(&wbuf, &reply_len, 4);
  memcpy(&wbuf[4], &reply, reply_len);
  return write_full(fd, wbuf, 4 + reply_len);
}

int main(void) {
  // args:
  // domain, type (TCP/UDP), protocol
  // SOCK_STREAM is TCP
  // SOCK_DGRAM is UDP
  // 0 is IP
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  // Q: why 0.0.0.0 here?
  // A: 0.0.0.0 serves as '*' wildcard, to suggest
  //    any address is OK. addr will be chosen by
  //    TCP/IP subsystem
  addr.sin_addr.s_addr = htonl(0x00000000); // 0.0.0.0
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("bind()");
  }
  rv = listen(fd, SOMAXCONN);
  if (rv) {
    die("listen()");
  }

  while (1) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);

    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd == -1) {
      continue; // error
    }

    while (1) {
      int32_t err = one_request(connfd);
      if (err) {
        break;
      }
    }
    close(connfd);
  }
}
