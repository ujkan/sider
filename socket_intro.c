#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/_endian.h>
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static void do_something(int);

void die(char *message) {
  fprintf(stderr, "%s\n", message);
  exit(1);
}
int main(void) {
  // args:
  // domain, type (TCP/UDP), protocol
  // SOCK_STREAM is TCP
  // SOCK_DGRAM is UDP
  // 0 is IP
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
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

    do_something(connfd);
    close(connfd);
  }
}

static void do_something(int connfd) {
  char rbuf[64] = {};
  int n = read(connfd, rbuf, sizeof(rbuf) - 1);
  if (n < 0) {
    printf("read() error");
    return;
  }

  printf("Client says: %s\n", rbuf);

  char wbuf[] = "world";
  write(connfd, &wbuf, sizeof(wbuf));

}
