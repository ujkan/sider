#include "../vector.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int32_t one_request(int fd) {
  char len_rbuf[4];
  int32_t rv = read_full(fd, len_rbuf, 4);
  if (rv != 0) {
    msg("one_request, read_full len");
    if (errno == EBADF) {
      msg("BAD FD");
    }
//     printf("rv:%d\n", rv);
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
//   printf("Client says '%.*s'\n", len, msg_rbuf);

  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)]; // sizeof gives 5+1 (incl. \0); do we need \0?
                                // assume not since we use strlen below!
  uint32_t reply_len = (uint32_t)strlen(reply);
  memcpy(&wbuf, &reply_len, 4);
  memcpy(&wbuf[4], &reply, reply_len);
  return write_full(fd, wbuf, 4 + reply_len);
}

static void fd_set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1) {
    msg("fd_set_nonblock, F_GETFL");
  }
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
  // TODO: err handling
}

typedef struct pollfd pollfd_t;

#define COMPARE_CONN(a, b) ((a.fd) == (b.fd) ? 0 : ((a.fd) > (b.fd) ? 1 : -1))
#define COMPARE_CHAR(a, b) ((a) == (b) ? 0 : ((a) > (b) ? 1 : -1))

DEFINE_VECTOR(uint8_t, COMPARE_CHAR);

typedef struct conn {
  int fd;
  bool want_read;
  bool want_write;
  bool want_close;

  vector_uint8_t *outgoing;
  vector_uint8_t *incoming;

} conn;
DEFINE_VECTOR(conn, COMPARE_CONN);
DEFINE_VECTOR(pollfd_t, COMPARE_CONN);

void conn_init(conn *c, int fd) {
  c->fd = fd;
  c->want_read = true;
  c->want_write = false;
  c->outgoing = malloc(sizeof(vector_uint8_t));
  c->incoming = malloc(sizeof(vector_uint8_t));
  vector_uint8_t_init(c->outgoing, k_max_msg);
  vector_uint8_t_init(c->incoming, k_max_msg);
}

bool try_one_request(conn *c) {
//   printf("tryonereq");
  int size = c->incoming->size;
  if (size < 4) {
    msg("shorter than 4");
    return false;
  }
  uint32_t len;
//   printf("c->incoming->size: %d", c->incoming->size);
//   printf("c->incoming->data: %d", c->incoming->data[1]);
  memcpy(&len, c->incoming->data, 4);
  if (size < 4 + len) {
    msgn("shorter than msg len; msg_len ==  ");
    fprintf(stderr, "%d; ", len);
    fprintf(stderr, "size == %d; ", size);
    return false;
  }
//   printf("ok!:!:!:!:!\n");
  // // response == echo
  vector_uint8_t_insert(c->outgoing, c->incoming->data, 4);
  vector_uint8_t_insert(c->outgoing, &c->incoming->data[4], len);
  vector_uint8_t_remove_first_n(c->incoming, 4 + len);
  return false;
}

void handle_write(conn *c) {
//   printf("HANDLe WRITE");
  ssize_t rv = write(c->fd, c->outgoing->data, c->outgoing->size);
  if (rv < 0) {
    c->want_close = true;
    return;
  }
  vector_uint8_t_remove_first_n(c->outgoing, rv);
  if (c->outgoing->size == 0) {
    c->want_read = true;
    c->want_write = false;
  }
}

// https://www.youtube.com/watch?v=_3LpJ6I-tzc
void handle_read(conn *c) {
//   printf("handle_read --> --> c fd : %d\n", c->fd);
  uint8_t buf[64 * 1024];
  ssize_t rv = read(c->fd, buf, sizeof(buf));
  if (rv <= 0) {
    // msgn("error while reading, fd:");
//     printf("-- --\n");
//     printf("%d\n", c->fd);
    c->want_close = true;
    return;
  }
//   printf("handle_read read count: %d\n", rv);
  vector_uint8_t_insert(c->incoming, buf, (uint32_t)rv);
  try_one_request(c);

//   printf("c->outgoing->size: %d\n", c->outgoing->size);
  if (c->outgoing->size > 0) {

    c->want_read = false;
    c->want_write = true;
    handle_write(c);
  }
  return;
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

//   printf("FD : %d\n", fd);

  fd_set_nonblock(fd);

  vector_conn conns;
  vector_pollfd_t poll_args;
  vector_conn_init(&conns, 10);
  vector_pollfd_t_init(&poll_args, 10);

  while (true) {

//     printf("loopiter\n");
    poll_args.size = 0;
    pollfd_t pfd = {fd, POLLIN, 0};
    vector_pollfd_t_push(&poll_args, pfd);
    msg("pushed listening fd");

    for (int i = 0; i < conns.size; i++) {
      conn *c = vector_conn_get(&conns, i);
      pollfd_t pfd = {c->fd, POLLERR, 0};
      if (c->want_read) {
        msg("wanna read");
        pfd.events |= POLLIN;
      }
      if (c->want_write) {
        pfd.events |= POLLOUT;
      }
      vector_pollfd_t_push(&poll_args, pfd);
    }
    // msg("pushed all connfds");
//     printf("poll_args size:%d\n", poll_args.size);
    // printf("poll_args data pointer: %p\n", poll_args.data);
    int dx = vector_pollfd_t_get(&poll_args, 0)->fd;
//     printf("poll_args first fd%d:\n", dx);
//     printf("conns first want_read:%d:\n",
           // vector_conn_get(&conns, 0)->want_read);
//     printf("poll_args second events:%x:\n",
           // vector_pollfd_t_get(&poll_args, 1)->events);

    int rv = poll(poll_args.data, (nfds_t)poll_args.size, -1);
//     printf("done polling\n");
    if (rv < 0 && errno == EINTR) {
      continue;
    }
    if (rv < 0) {
      die("poll");
    }

    if (vector_pollfd_t_get(&poll_args, 0)->revents & POLLIN) {
      struct sockaddr_in client_addr = {};
      socklen_t addrlen = sizeof(client_addr);
      int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
      if (connfd != -1) {
        // msg("conn accept");
        fd_set_nonblock(connfd);
        conn c;
        conn_init(&c, connfd);
//         printf("post-init, conn c want_read: %d \n", c.want_read);
        vector_conn_push(&conns, c);
        // printf("post-push, conn c want_read: %d \n", conns.);
      }
    }

    // msg("PRE-loop");
//     printf("poll_args size:%d\n", poll_args.size);
    for (int i = 1; i < poll_args.size; i++) {
//       printf("pollargs loop\n");
      pollfd_t *poll_arg = vector_pollfd_t_get(&poll_args, i);
      uint32_t ready = poll_arg->revents;
      conn *c;
      int err = vector_conn_get_safe(&conns, poll_arg->fd - 4, &c);
      if (err) {
//         printf("conn size: %d ; arg - 2: %d \n", conns.size, poll_arg->fd - 4);
//         printf("error vector get\n");
        continue;
      }
//       printf("c ptr %p\n", c);
      if (c) {
//         printf("c fd : %d\n", c->fd);
      }
      if (ready & POLLIN) {
//         printf("POLLIN ready");
        handle_read(c);
      }
      if (ready & POLLOUT) {
        handle_write(c);
      }
      if ((ready & POLLERR) || c->want_close) {
        close(c->fd);
      }
    }
  }
}
