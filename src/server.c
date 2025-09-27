#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "bytering.h"
#include "hmap.h"
#include "u_array.h"
#include "u_ptr_array.h"

// TODO: Questions Q?
// - does GByteArray store pointers to bytes or the bytes directly?
// - what does it mean to free the GByteArray bytes? Does it not suffice
// to free the entire block of data, since that's the malloc-ed unit? Is it
// not malloc(len * sizeof(byte))?
// - same question as above but for GArray

static uint16_t PORT = 8085;
static uint32_t k_max_len = 4096;
static uint32_t k_min_args = 1;
static uint32_t k_max_args = 3;

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

void conn_free(void *conn) {
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

bool read_u32(const u8 *start, const u8 *end, u32 *value) {
  if (start + 4 > end) {
    return false;
  }
  memcpy(value, start, 4);
  return true;
}

bool read_str(const u8 *start, const u8 *end, u32 msg_len, u8 *buf) {
  if (start + msg_len > end) {
    return false;
  }
  memcpy(buf, start, msg_len);
  return true;
}

// nstr len1 msg1 len2 msg2 len3 msg3 ...
// 4B   4B   ...  4B   ...  4B   ...
// e.g.
// nstr       len1       m1  len2        m2
// 0x00000002 0x00000001 "r" 0x00000005 "hello"
// the request is: get "hello"

struct LString {
  u32 len;
  char *str;
};
void lstring_free(void *data) {
  free(((struct LString *)data)->str);
  free((void *)data);
}

struct Response {
  u32 status;
  u8 *data;
};

int32_t parse_request(u8 *buf, u32 len, PtrArray *out) {
  u32 nstr;
  u8 *curr = buf;
  const u8 *end = buf + len;
  bool rv = read_u32(buf, end, &nstr);
  nstr = ntohl(nstr);
  if (!rv) {
    return -1;
  }
  if (nstr < k_min_args || nstr > k_max_args) {
    return -1;
  }

  curr += 4;
  for (u32 i = 0; i < nstr; i++) {
    struct LString *s = malloc(sizeof(struct LString));

    bool rv = read_u32(curr, end, &s->len);
    s->len = ntohl(s->len);
    if (!rv) {
      return -1;
    }
    curr += 4;

    s->str = malloc(s->len);

    rv = read_str(curr, end, s->len, (u8 *)s->str);
    if (!rv) {
      return -1;
    }
    curr += s->len;
    ptr_array_add(out, s);
  }
  if (curr != end) {
    return -1;
  }
  return 0;
}

static struct hashmap *data;

void do_request(PtrArray *cmd, struct Response *out) {
  out->data[0] = '\0';
  out->status = 1;
  if (cmd->len == 1) {
    char *command = ((struct LString *)ptr_array_index(cmd, 0))->str;
    if (strncmp(command, "print", 5) == 0) {
      hashmap_print_entries_compact(data);
    }
    return;
  }
  if (cmd->len == 2) {
    char *command = ((struct LString *)ptr_array_index(cmd, 0))->str;
    printf("COMMAND ------------------ %s\n", command);
    char *key = ((struct LString *)ptr_array_index(cmd, 1))->str;
    if (strncmp(command, "get", 3) == 0) {
      char *value = hashmap_get(data, key);
      if (value) {
        out->status = 0;
        memcpy(out->data, value,
               strlen(value)); // TODO: fix, should be val_len, but we don't store LStr :(
                               // risky since value is not null-terminated
        /*byte_ring_append_n(out->data, value, key_len);*/
      } else {
        out->status = 1; // not found
      }
      // handle get
    } else if (strncmp(command, "del", 3) == 0) {
      int rv = hashmap_delete(data, key);
      printf("DELETE\n");
      if (rv == 1) {
        out->status = 0; // deleted!
      } else {
        out->status = 1; // not found
      }
    }
  } else if (cmd->len == 3) {
    char *command = ((struct LString *)ptr_array_index(cmd, 0))->str;
    char *key = ((struct LString *)ptr_array_index(cmd, 1))->str;
    char *value = ((struct LString *)ptr_array_index(cmd, 2))->str;

    if (strncmp(command, "set", 3) == 0) {
      printf("setting; key=%s ; value=%s \n", key, value);
      hashmap_upsert(data, key, value);
      out->status = 0;
    }
  } else {
    out->status = 2; // unrecognized command
  }
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

  PtrArray *command = ptr_array_new_full(4, lstring_free);
  u8 buf[conn->incoming->size - 4];
  byte_ring_copy_n(conn->incoming, buf, 4, conn->incoming->size - 4);
  parse_request(buf, sizeof(buf), command);
  struct Response resp = {0};
  resp.data = calloc(128, 1); // calloc to set everything to 0
  do_request(command, &resp);
  u32 resp_len = htonl(sizeof(resp.status) + 128);
  byte_ring_append_n(conn->outgoing, (const u8 *)&resp_len, 4);
  byte_ring_append_n(conn->outgoing, (const u8 *)&resp.status, 4);
  byte_ring_append_n(conn->outgoing, (const u8 *)resp.data, 128);
  free(resp.data);
  /*ptr_array_free(command, TRUE);*/

  // response part
  /*u8 reply[msg_len];*/
  /*char prefix[5] = "echo:";*/
  /*uint32_t len = htonl(sizeof(reply) + 5);*/
  /*byte_ring_append_n(conn->outgoing, (const guint8 *)&len, 4);*/
  /*byte_ring_append_n(conn->outgoing, (const guint8 *)prefix, 5);*/
  /*printf("msg_len=%d\n", msg_len);*/
  // TODO: implement a copy function between two rings
  /*byte_ring_copy_n(conn->incoming, reply, 4, msg_len);*/
  /*byte_ring_append_n(conn->outgoing, reply, msg_len);*/

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
  byte_ring_append_n(conn->incoming, (const u8 *)rbuf, rv);
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
  struct sockaddr_in addr = {0};
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

  PtrArray *conns = ptr_array_new_full(10, conn_free);
  ptr_array_set_length(conns, 10);
  Array *pollfds = array_sized_new(10, sizeof(struct pollfd));
  struct pollfd l_pollfd = {fd, POLLIN, 0};
  array_push(pollfds, &l_pollfd);

  data = malloc(sizeof(hashmap));
  hashmap_init(data, 128);

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
    array_set_length(pollfds, 1);

    // prepare connections for polling
    for (uint i = 0; i < conns->len; i++) {
      struct Conn *conn = (struct Conn *)ptr_array_index(conns, i);
      if (conn) {
        struct pollfd p = {0};
        p.fd = conn->fd;
        p.events |= (conn->want_read ? POLLIN : 0);
        p.events |= (conn->want_write ? POLLOUT : 0);
        p.events |= (conn->want_close ? POLLERR : 0);
        // NOTE: push is a macro for append_vals with &p (ptr to p)
        // yet this doesn't mean that the *pointer* value is stored, rather
        // GLib memcpy's the values of the struct at that address, and it knows
        // how much to read because of element_size during initialization
        array_push(pollfds, &p);
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
    l_pollfd = array_index(pollfds, struct pollfd, 0);
    if (l_pollfd.revents) {
      struct sockaddr_in client_addr = {0};
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

      ptr_array_set(conns, conn->fd, conn);
    }

    // if conn socket revents
    // then handle depending on POLLIN, POLLOUT, POLLERR
    for (uint i = 1; i < pollfds->len; i++) {
      struct pollfd pollfd = array_index(pollfds, struct pollfd, i);
      short ready = pollfd.revents;

      // NOTE: we index the conns array with the fd
      struct Conn *conn = (struct Conn *)ptr_array_index(
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
        // NOTE: set also frees the entry it replaces
        // so no need to free it explicitly
        ptr_array_set(conns, connfd, NULL);
      }
    }
  }

  return 0;
}
