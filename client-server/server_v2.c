#include "glib.h"
#include "glibconfig.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

static uint16_t PORT = 8085;
static uint32_t k_max_len = 4096;

static void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] Fatal error, exiting. Message: %s\n", err, msg);
  abort();
}

struct Conn {
  int fd;
  bool want_read;
  bool want_write;
  bool want_close;

  GByteArray *incoming;
  GByteArray *outgoing;
};

struct Conn *conn_new(int fd) {
  struct Conn *c = malloc(sizeof(struct Conn));
  c->fd = fd;
  c->want_read = false;
  c->want_write = false;
  c->want_close = false;
  c->incoming = g_byte_array_sized_new(1024);
  c->outgoing = g_byte_array_sized_new(1024);
  return c;
}

void conn_destroy(struct Conn *c) {
  g_byte_array_free(c->incoming, FALSE);
  g_byte_array_free(c->outgoing, FALSE);
  free(c);
}

void _g_ptr_array_insert_expand(GPtrArray *array, gint index_, gpointer data) {
  if (index_ >= array->len) {
    g_ptr_array_set_size(array, index_ + 10);
  }
  g_ptr_array_insert(array, index_, data);
}

bool try_one_request(struct Conn *c) {
  if (c->incoming->len < 4) {
    return false;
  }
  int len;
  memcpy(&len, c->incoming->data, 4);
  if (len > k_max_len) {
    printf("handle_read / len=%d exceeding k_max_len=%d\n", len, k_max_len);
    return false;
  }
  if (len + 4 > c->incoming->len) {
    printf("handle_read / len=%d exceeding available data in buffer =%d\n", len,
           c->incoming->len - 4);
    return false;
  }
  guint8 msg[len + 1];
  memcpy(msg, c->incoming->data + 4, len);

  printf("msg: %s\n", msg);

  g_byte_array_append(c->outgoing, (const guint8 *)&len, 4);
  g_byte_array_append(c->outgoing, msg, len);
  printf("msg2: %s\n", c->outgoing->data);

  g_byte_array_remove_range(c->incoming, 0, len + 4);

  return true;
  /*if (rv == len) { // or do we check c->incoming->len == len+4?*/
  /*  // ready to write*/
  /*  c->want_read = false;*/
  /*  c->want_write = true;*/
  /*}*/
}

void handle_read(struct Conn *c) {
  uint8_t buf[64 * 1024];
  ssize_t rv = read(c->fd, buf, sizeof(buf));
  if (rv <= 0) {
    c->want_close = true;
    return;
  }

  g_byte_array_append(c->incoming, buf, (guint)rv);

  bool did_respond = try_one_request(c);
  if (did_respond) {
    c->want_read = false;
    c->want_write = true;
  } else {
    printf("handle_read / did_response=false / c->want_close=true for fd=%d\n",
           c->fd);
    c->want_close = true;
  }
}

void handle_write(struct Conn *c) {
  ssize_t rv = write(c->fd, c->outgoing->data, c->outgoing->len);
  if (rv < 0) {
    c->want_close = true;
    return;
  }

  printf("written to fd %d\n", c->fd);
  g_byte_array_remove_range(c->outgoing, 0, rv);
  if (c->outgoing->len == 0) {
    c->want_read = true;
    c->want_write = false;
  }
}

int main(void) {

  // create IPv4 socket (general)
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  // bind to address
  struct sockaddr_in addr = {};
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(PORT);
  addr.sin_family = AF_INET;
  int rv = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (rv < 0) {
    die("bind()");
  }
  // create listening socket
  rv = listen(fd, SOMAXCONN);
  if (rv < 0) {
    die("listen()");
  }

  // conns contains pointers to struct Conn
  GPtrArray *conns = g_ptr_array_sized_new(10);

  // poll_args is an array of 'struct pollfd'
  GArray *poll_args =
      g_array_sized_new(FALSE, FALSE, sizeof(struct pollfd), 10);

  // start event loop
  while (1) {
    /*printf("Starting loop\n");*/

    poll_args->len = 0;
    // add the listening socket to the poll list
    // for the listening socket, a POLLIN event is an "accept" request
    struct pollfd pollfd = {fd, POLLIN, 0};
    g_array_append_val(poll_args, pollfd);

    // create poll_args of pollfd with the correct `events`
    for (guint i = 0; i < conns->len; i++) {
      /*printf("In conn loop fd=%d\n", i);*/
      struct Conn *c = g_ptr_array_index(conns, i);
      if (c) {
        /*printf("In conn loop fd=%d -- IF TRUE\n", i);*/
        struct pollfd pollfd = {};
        pollfd.fd = c->fd;
        pollfd.events |= (c->want_read ? POLLIN : 0);
        pollfd.events |= (c->want_write ? POLLOUT : 0);
        pollfd.events |= (c->want_close ? POLLERR : 0);
        printf("pollfd fd=%d events=%d\n", pollfd.fd, pollfd.events);
        /*printf("events for fd=%d : %d\n", pollfd.fd, pollfd.events);*/
        g_array_append_val(poll_args, pollfd);
      }
    }

    // poll the poll_args
    /*printf("poll_args len=%d\n", poll_args->len);*/
    int rv = poll((struct pollfd *)poll_args->data, poll_args->len, -1);

    // register sockets/connections ("accept")
    // only when ready to read
    struct pollfd lsocket_pollfd = g_array_index(poll_args, struct pollfd, 0);
    /*printf("lsocket events: %x\n", lsocket_pollfd.events);*/
    /*printf("lsocket fd: %d\n", lsocket_pollfd.fd);*/
    /*printf("lsocket revents: %x\n", lsocket_pollfd.revents);*/
    if (lsocket_pollfd.revents) {
      /*printf("lsocket revent POLLIN\n");*/
      struct sockaddr peer_addr = {};
      socklen_t peer_addr_size = sizeof(peer_addr);
      int connfd = accept(fd, &peer_addr, &peer_addr_size);

      if (connfd < 0) {
        // handle error
      } else {
        struct Conn *c = conn_new(connfd);
        c->want_read = true;
        _g_ptr_array_insert_expand(conns, connfd, c);
      }
    }

    // act on the different `revents` of client socket fds after the poll
    for (guint i = 1; i < poll_args->len; i++) {
      struct pollfd curr = g_array_index(poll_args, struct pollfd, i);
      struct Conn *conn = g_ptr_array_index(conns, curr.fd);
      printf("revents fd=%d r=%d\n", curr.fd, curr.revents);
      if (curr.revents & POLLIN) {
        handle_read(conn);
      }
      if (curr.revents & POLLOUT) {
        handle_write(conn);
      }
      if ((curr.revents & POLLERR) || conn->want_close) {
        close(conn->fd);
        free(conn);
        g_ptr_array_index(conns, curr.fd) = NULL;
        printf("closed\n");
        // close
      }
    }
    //
  }
}
