#include <fcntl.h>
#include <glib.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8084

static void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }
static void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] Fatal error, exiting (coredumped). Message: [%s]\n",
          err, msg);
  abort();
}

struct conn {
  int fd;
  bool want_read;
  bool want_write;
  bool want_close;

  GByteArray *incoming;
  GByteArray *outgoing;
};

void conn_init(struct conn *conn, int fd) {
  conn->fd = fd;
  conn->want_close = false;
  conn->want_read = false;
  conn->want_write = false;
  conn->incoming = g_byte_array_sized_new(1024);
  conn->outgoing = g_byte_array_sized_new(1024);
}

void handle_read(struct conn *conn) {
  printf("handle_read  fd %d\n", conn->fd);

  int rv = read(conn->fd, conn->incoming->data, 4);
  if (rv <= 0) {
    die("handle_read(),read(4)");
  }
  uint32_t msg_len;
  memcpy(&msg_len, conn->incoming->data, 4);
  rv = read(conn->fd, conn->incoming->data + 4, msg_len);
  if (rv <= 0) {
    die("handle_read(),read(msg_len)");
  }
  printf("msg_len : %d \n", msg_len);
  char msg[msg_len + 1];
  memcpy(msg, conn->incoming->data + 4, msg_len);
  write(conn->fd, msg, 4);
  conn->want_read = false;
}
void handle_write(int fd) { printf("handle_write fd %d\n", fd); }

static void fd_set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0); // get the flags
  flags |= O_NONBLOCK;               // modify the flags
  fcntl(fd, F_SETFL, flags);         // set the flags
  // TODO: handle errno
}

int main(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket() err");
  }
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = INADDR_ANY;
  saddr.sin_port = htons(8085);

  int rv = bind(fd, (struct sockaddr *)&saddr, sizeof(saddr));
  if (rv < 0) {
    die("bind() err");
  }

  rv = listen(fd, SOMAXCONN);
  if (rv < 0) {
    die("listen() err");
  }
  fd_set_nonblock(fd);

  GPtrArray *conns = g_ptr_array_sized_new((guint)10);
  g_ptr_array_set_size(conns, 10);
  printf("fd: %d\n", fd);
  printf("len: %d\n", conns->len);
  g_ptr_array_insert(conns, fd, NULL);
  GArray *poll_args =
      g_array_sized_new(FALSE, FALSE, sizeof(struct pollfd), 10);
  struct conn *conn = malloc(sizeof(struct conn));
  conn_init(conn, fd);
  conn->want_read = true;
  g_ptr_array_insert(conns, fd, conn);

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = 0;
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
    fd_set_nonblock(connfd);
    struct conn *conn = malloc(sizeof(struct conn));

    conn_init(conn, connfd);
    conn->want_write = true;
    g_ptr_array_insert(conns, connfd, conn);

    for (gint i = 3; i < conns->len; i++) {
      struct pollfd p;
      p.fd = i;
      printf("p.fd: %d\n", p.fd);
      if (((struct conn *)(g_ptr_array_index(conns, i)))) {
        printf("notnull idx: %d\n", i);

        if (((struct conn *)(g_ptr_array_index(conns, i)))->want_read) {
          p.events |= POLLIN;
        }
        if (((struct conn *)(g_ptr_array_index(conns, i)))->want_write) {
          p.events |= POLLOUT;
        }
        if (((struct conn *)(g_ptr_array_index(conns, i)))->want_close) {
          p.events |= POLLERR;
        }
        g_array_insert_vals(poll_args, i - 3, &p, 1);
      }
    }

    printf("exited conns loop\n");

    int r = poll((struct pollfd *)poll_args->data, conns->len, 0);
    printf("poll_args->len: %d\n", poll_args->len);

    for (gint i = 0; i < poll_args->len; i++) {
      struct pollfd p = g_array_index(poll_args, struct pollfd, i);
      short events = p.revents;
      printf("read events for fd=%d = %d\n", p.fd, events);

      if (events & POLLIN) {
        handle_read(((struct conn *)(g_ptr_array_index(conns, p.fd))));
      }
      if (events & POLLOUT) {
        handle_write(p.fd);
      }
      if (events & POLLERR) {
        // close?
        // handle_err
      }
    }
    printf("exited poll loop\n");
  }
}

/*void handle_request(int connfd) {*/
/*    read(connfd,)*/
/*}*/
