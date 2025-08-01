#include "bytering.h"
#include "glib.h"
#include "glibconfig.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

// TODO: Questions Q?
// - does GByteArray store pointers to bytes or the bytes directly?
// - what does it mean to free the GByteArray bytes? Does it not suffice
// to free the entire block of data, since that's the malloc-ed unit? Is it
// not malloc(len * sizeof(byte))?
// - same question as above but for GArray

static uint16_t PORT = 8085;
static uint32_t k_max_len = 4096;

static void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] Fatal error, exiting. Message: %s\n", err, msg);
  abort();
}
static void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

struct Conn {
  int fd;
  bool want_read;
  bool want_write;
  bool want_close;

  struct ByteRing *incoming;
  struct ByteRing *outgoing;
};

void _g_ptr_array_set(GPtrArray *array, guint index, gpointer data) {
  if (index >= array->len) {
    g_ptr_array_set_size(array, index + 4);
  }
  g_ptr_array_index(array, index) = data;
}

int _g_array_copy_n(void *dest, GArray *src, size_t n) {
  if (n > src->len) {
    return -1;
  }
  memcpy(dest, src->data, n);
  return 0;
}

void conn_free(gpointer conn) {
  free(((struct Conn *)conn)->incoming->data);
  free(((struct Conn *)conn)->incoming);
  free(((struct Conn *)conn)->outgoing->data);
  free(((struct Conn *)conn)->outgoing);
}

struct Conn *conn_init(int fd) {
  struct Conn *conn = malloc(sizeof(struct Conn));
  conn->fd = fd;
  conn->want_read = false;
  conn->want_write = false;
  conn->want_close = false;
  conn->incoming = byte_ring_init(1024);
  conn->outgoing = byte_ring_init(1024);
  return conn;
}

typedef enum {
  COMMAND_TYPE_READ = 'r',
  COMMAND_TYPE_SET = 's',
  COMMAND_TYPE_DELETE = 'd'
} CommandType;

struct Command {
  CommandType type;
  int key;
  union {
    GByteArray *value;
  };
};

int parse_request(GByteArray *buf, int len, struct Command *cmd) {
  char c = g_array_index(buf, guint8, 0);
  if (c != COMMAND_TYPE_READ || c != COMMAND_TYPE_DELETE ||
      c != COMMAND_TYPE_SET) {
    return -1;
  }
  cmd->type = c;
  return 0;
}

bool try_one_request(struct Conn *conn) {
  if (conn->incoming->size < 4) {
    msg("not enough data in incoming buffer");
    return false;
  }
  uint32_t msg_len;
  memcpy(&msg_len, conn->incoming->data, 4);
  msg_len = ntohl(msg_len);
  if (msg_len > k_max_len) {
    msg("max len exceeded");
    byte_ring_pop_first_n(conn->incoming, 4);
    conn->want_close = true;
    return false;
  }
  if (msg_len + 4 > conn->incoming->size) {
    msg("not enough data in incoming buffer");
    return false;
  }

  /*parse_request(conn->incoming, len, &command);*/

  // response part
  u8 reply[msg_len];
  char prefix[5] = "echo:";
  uint32_t len = htonl(sizeof(reply) + 5);
  byte_ring_append_n(conn->outgoing, (const guint8 *)&len, 4);
  byte_ring_append_n(conn->outgoing, (const guint8 *)prefix, 5);
  printf("msg_len=%d\n", msg_len);
  byte_ring_copy_n(conn->incoming, reply, 4, msg_len);
  byte_ring_append_n(conn->outgoing, reply, msg_len);

  // clean up incoming buffer
  byte_ring_debug_print(conn->incoming);
  byte_ring_pop_first_n(conn->incoming, 4 + msg_len);
  byte_ring_debug_print(conn->incoming);

  return true;
}

void handle_read(struct Conn *conn) {
  /*printf("handle_Read\n");*/
  char rbuf[64 * 1024];
  int rv = read(conn->fd, rbuf, sizeof(rbuf));
  if (rv <= 0) {
    // error
    conn->want_close = true;
    return;
  }
  byte_ring_append_n(conn->incoming, (const guint8 *)rbuf, rv);
  while (try_one_request(conn)) {
  }
  if (conn->outgoing->size > 0) {
    conn->want_read = false;
    conn->want_write = true;
  }
}

void handle_write(struct Conn *conn) {
  int rv = write(conn->fd, conn->outgoing->data, conn->outgoing->size);
  printf("replied to fd=%d\n", conn->fd);
  if (rv <= 0) {
    //
    conn->want_close = true;
    return;
  }

  byte_ring_pop_first_n(conn->outgoing, rv);
  if (conn->outgoing->size == 0) {
    conn->want_read = true;
    conn->want_write = false;
  }
}

int main(void) {
  // socket()
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  // bind()
  struct sockaddr_in addr = {};
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(PORT);
  addr.sin_family = AF_INET;

  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv < 0) {
    die("bind()");
  }

  // listen()
  rv = listen(fd, SOMAXCONN);
  if (rv < 0) {
    die("listen()");
  }

  GPtrArray *conns = g_ptr_array_new_full(10, conn_free);
  g_ptr_array_set_size(conns, 10);
  GArray *pollfds = g_array_sized_new(FALSE, TRUE, sizeof(struct pollfd), 10);
  struct pollfd l_pollfd = {fd, POLLIN, 0};
  g_array_append_val(pollfds, l_pollfd);

  for (;;) {
    // TODO: 2 things need to be fixed
    // 1. we cannot keep appending to pollfds in a loop even if no new
    // connections incoming
    // 2. as connections may change whether they "want_read", "want_write",
    // and "want_close" in each event loop iteration, we need to edit the
    // "events" even for those connections

    // NOTE: conns is the primary state object. it both represents
    // the actual comms (via the in/out buffers) and the read/write/error
    // state of the connection

    // choose an "inefficient" approach
    // clear the pollfds array each time and just rebuild it
    // NOTE: set_size runs in O(n) (sets all to 0) but does not do any memmove
    // TODO: alternatively keep a map but would have to find a map where
    // the "map.getValues()" struct is a simple pointer to struct pollfd and not
    // some other type, since that's what "poll()" admits
    g_array_set_size(pollfds, 1);

    // prepare connections for polling
    for (guint i = 0; i < conns->len; i++) {
      struct Conn *conn = g_ptr_array_index(conns, i);
      if (conn) {
        struct pollfd p = {};
        p.fd = conn->fd;
        p.events |= (conn->want_read ? POLLIN : 0);
        p.events |= (conn->want_write ? POLLOUT : 0);
        p.events |= (conn->want_close ? POLLERR : 0);
        // NOTE: append_val is a macro for append_vals with &p (ptr to p)
        // yet this doesn't mean that the *pointer* value is stored, rather
        // GLib memcpy's the values of the struct at that address, and it knows
        // how much to read because of element_size during initialization
        g_array_append_val(pollfds, p);
      }
    }

    // call poll & respond to its events
    poll((struct pollfd *)pollfds->data, pollfds->len, -1);
    // 2 types of fds being polled:
    // - listening socket
    // - conn sockets

    // if listening socket revents = READ/POLLIN
    // means a socket is trying to connect (aka "can accept")
    // ==> add connection to conns list
    l_pollfd = g_array_index(pollfds, struct pollfd, 0);
    if (l_pollfd.revents) {
      struct sockaddr_in client_addr = {};
      socklen_t client_addrlen = 0;
      int connfd = accept(fd, (struct sockaddr *)&client_addr, &client_addrlen);
      if (connfd < 0) {
        msg("accept failed");
      } else {
        msg("accepted connection");
      }
      printf("at fd:%d / address: %d.%d.%d.%d:%u\n",           //
             connfd,                                           //
             (ntohl(addr.sin_addr.s_addr & 0xff000000)) >> 24, //
             (ntohl(addr.sin_addr.s_addr & 0x00ff0000)) >> 16, //
             (ntohl(addr.sin_addr.s_addr & 0x0000ff00)) >> 8,  //
             (ntohl(addr.sin_addr.s_addr & 0x000000ff)),       //
             ntohs(addr.sin_port));
      struct Conn *conn = conn_init(connfd);
      conn->want_read = true;

      // NOTE: must use a custom "set" method because
      // the insert method shifts everything after index to the right, so what
      // was previously at index 5 becomes index 6 if we insert at index=4. this
      // is buggy
      _g_ptr_array_set(conns, conn->fd, conn);
    }

    // if conn socket revents
    // then handle depending on POLLIN, POLLOUT, POLLERR
    for (guint i = 1; i < pollfds->len; i++) {
      struct pollfd pollfd = g_array_index(pollfds, struct pollfd, i);
      short ready = pollfd.revents;

      // NOTE: we index the conns array with the fd
      struct Conn *conn = g_ptr_array_index(
          conns, pollfd.fd); // value at arr[fd], a ptr to Conn

      if (ready & POLLIN) {
        handle_read(conn);
      }
      if (ready & POLLOUT) {
        handle_write(conn);
      }
      if ((ready & POLLERR) || conn->want_close) {
        int connfd = conn->fd;
        close(connfd);
        conn_free(conn);
        free(conn);
        _g_ptr_array_set(conns, connfd, NULL);
      }
    }
  }

  return 0;
}
