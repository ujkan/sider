#include "utils.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
  addr.sin_port = htons(1234);
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

  char wbuf[] = "hello";
  // Q: diff between strlen(wbuf) and sizeof(wbuf)?
  // A: strlen rets 5 (len of 'hello') but sizeof(wbuf) rets 6 (includes '\0')
  write(fd, &wbuf, strlen(wbuf));

  char rbuf[64] = {};
  read(fd, &rbuf, sizeof(rbuf) - 1); // same as strlen; last byte is '\0'
  printf("Server says: %s\n", rbuf);
}
