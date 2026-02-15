#include <errno.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>

#include "bytering.h"
#include "hmap.h"
#include "lstr.h"
#include "persistence.h"
#include "skiplist_str.h"
#include "types.h"
#include "u_array.h"
#include "u_ptr_array.h"
#include <dirent.h>

#define T SLQueue, SkipList *
#include <stc/deque.h>

// TODO: Questions Q?
// - does GByteArray store pointers to bytes or the bytes directly?
// - what does it mean to free the GByteArray bytes? Does it not suffice
// to free the entire block of data, since that's the malloc-ed unit? Is it
// not malloc(len * sizeof(byte))?
// - same question as above but for GArray

typedef struct {
  SLQueue queue;
  pthread_mutex_t lock;
  pthread_cond_t condition;
  bool running;
} InactiveMemtables;

typedef struct {
  Array *sstables;
} Level;

typedef struct {
  // serves requests, tier 1
  SkipList *active_memtable;
  // these also serve requests, tier 2
  // TODO: make it a queue
  // TODO: async thread dumps these to SSTable
  // once dump is done, pop from here
  InactiveMemtables inactive_memtables;
  // these also serve requests, tier 3
  Array *sstables;
  Array *lvl_zero_sstables;
  Array *levels;
  atomic_int sstable_count;
} Store;

static u16 PORT = 8085;
static u32 k_max_len = 4096;
static u32 k_min_args = 1;
static u32 k_max_args = 3;
static u32 k_max_key_len = 2 << 8;
static u32 k_max_value_len = 2 << 14;
static u32 kMemtableLimit = 1 * 64;
static struct hashmap *data;
static Store *store;

int count_sst_files() {
  DIR *dir;
  struct dirent *entry;
  int count = 0;
  const char *suffix = ".sst";
  size_t suffix_len = strlen(suffix);

  // Open current directory (".")
  dir = opendir(".");
  if (dir == NULL) {
    perror("Unable to open directory");
    return -1;
  }

  // Iterate over every file/folder in the directory
  while ((entry = readdir(dir)) != NULL) {
    size_t name_len = strlen(entry->d_name);

    // Check if filename is long enough to contain the suffix
    if (name_len >= suffix_len) {
      // Compare the end of the filename with our suffix
      if (strcmp(entry->d_name + (name_len - suffix_len), suffix) == 0) {
        count++;
      }
    }
  }

  closedir(dir);
  return count;
}

Store *store_init() {
  Store *s = malloc(sizeof(Store));
  s->active_memtable = sl_s_init();
  s->inactive_memtables.queue = SLQueue_init();
  pthread_mutex_init(&s->inactive_memtables.lock, NULL);
  pthread_cond_init(&s->inactive_memtables.condition, NULL);
  s->inactive_memtables.running = true;
  s->sstables = array_sized_new(16, sizeof(SSTable));
  atomic_store(&s->sstable_count, count_sst_files());
  for (int i = 0; i < atomic_load(&s->sstable_count); i++) {
    SSTable sst = {0};
    sst.filepath = lstring_create(5 + i / 10);
    sprintf(sst.filepath->data, "%d.sst", i);
    sst.size = 0;
    array_push(s->sstables, &sst);
  }
  return s;
}

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

struct Response {
  u32 status;
  u8 *data;
  u32 data_len;
};

typedef enum {
  COMMAND_TYPE_GET = 'g',
  COMMAND_TYPE_SET = 's',
  COMMAND_TYPE_DELETE = 'd',
  COMMAND_TYPE_MERGE = 'm'
} CommandType;
struct Command {
  CommandType type;
  union {
    struct {
      LString *key;
      LString *value;
    } pair;
    LString *key;
  } data;
};

// nstr len1 msg1 len2 msg2 len3 msg3 ...
// 4B   4B   ...  4B   ...  4B   ...
// e.g.
// the request is: "get hello"
// nstr       len1       m1  len2       m2
// 0x00000002 0x00000003 get 0x00000005 hello
i32 parse_request(u8 *buf, u32 len, PtrArray *out) {
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

    u32 len;
    bool rv = read_u32(curr, end, &len);
    if (!rv) {
      return -1;
    }
    len = ntohl(len);
    // TODO: lstring uses 16-bit now !!
    LString *s = lstring_create(len);
    curr += 4;

    rv = read_str(curr, end, s->len, (u8 *)s->data);
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

int parse_command(PtrArray *cmd_arr, struct Command *command) {
  if (cmd_arr->len < 1) {
    return 1;
  }
  u8 *ctype = ((LString *)ptr_array_index(cmd_arr, 0))->data;
  if (cmd_arr->len == 1 && strncmp((char *)ctype, "print", 5) == 0) {
    command->type = 'p';
    return 0;
  }
  if (cmd_arr->len == 1 && strncmp((char *)ctype, "merge", 5) == 0) {
    command->type = 'm';
    return 0;
  }
  if (cmd_arr->len == 2) {
    if (strncmp((char *)ctype, "get", 3) == 0) {
      command->type = COMMAND_TYPE_GET;
    } else if (strncmp((char *)ctype, "del", 3) == 0) {
      command->type = COMMAND_TYPE_DELETE;
    } else {
      return 1;
    }
    command->data.key = ((LString *)ptr_array_index(cmd_arr, 1));
    return 0;
  }
  if (cmd_arr->len == 3) {
    if (strncmp((char *)ctype, "set", 3) == 0) {
      command->type = COMMAND_TYPE_SET;
      command->data.pair.key = ((LString *)ptr_array_index(cmd_arr, 1));
      command->data.pair.value = ((LString *)ptr_array_index(cmd_arr, 2));
      return 0;
    }
  }
  return 1;
}

void handle_command(struct Command *cmd, struct Response *out) {
  out->data_len = 0;
  out->status = 2;
  if (cmd->type == 'p') {
    printf("------------------------------\n");
    printf("Active skiplist\n");
    pretty_print_skiplist(store->active_memtable);
    printf("SSTables\n");
    int sstable_count = atomic_load(&store->sstable_count);
    for (int i = 0; i < sstable_count; i++) {
      SSTable sst = array_index(store->sstables, SSTable, i);
      printf("SSTable %d : filepath=%.*s : size=%d\n", i, sst.filepath->len,
             sst.filepath->data, sst.size);
    }
    printf("------------------------------\n");

    // hashmap_print_entries_compact(data);
    return;
  }
  switch (cmd->type) {
    LString *key;
    LString *value;
  case COMMAND_TYPE_GET:
    key = cmd->data.key;
    if (key->len > k_max_key_len) {
      msg("Key length exceeds maximum length.");
      out->status = 2; // invalid request!
      break;
    }
    // value = hashmap_get(data, key);
    value = sl_s_find(store->active_memtable, key);
    if (value) {
      out->status = 0;
      memcpy(out->data, value->data, value->len);
      out->data_len = value->len;
    } else {
      pthread_mutex_lock(&store->inactive_memtables.lock);
      for (c_each(table, SLQueue, store->inactive_memtables.queue)) {
        value = sl_s_find(*table.ref, key);
        if (value) {
          out->status = 0;
          memcpy(out->data, value->data, value->len);
          out->data_len = value->len;
          return;
        }
      }
      pthread_mutex_unlock(&store->inactive_memtables.lock);
      int sstable_count = atomic_load(&store->sstable_count);
      for (int i = sstable_count - 1; i >= 0; i--) {
        SSTable sst = array_index(store->sstables, SSTable, i);
        value = search_in_sst(sst, key);
        if (value) {
          out->status = 0;
          memcpy(out->data, value->data, value->len);
          out->data_len = value->len;
          lstring_free(value);
          return;
        }
      }
      out->status = 1; // not found
    }
    break;
  case COMMAND_TYPE_DELETE:

    key = cmd->data.key;
    int rv = sl_s_remove(store->active_memtable, key);
    if (rv == 0) {
      out->status = 0; // deleted!
    } else {
      out->status = 1; // not found
    }
    break;
    ;
  case COMMAND_TYPE_SET:
    key = cmd->data.pair.key;
    value = cmd->data.pair.value;
    if (key->len > k_max_key_len) {
      msg("Key length exceeds maximum length.");
      out->status = 2; // invalid request!
      break;
    }
    if (value->len > k_max_value_len) {
      msg("Value length exceeds maximum length.");
      out->status = 2; // invalid request!
      break;
    }
    // hashmap_upsert(data, key, value);
    sl_s_insert(store->active_memtable, key, value);
    printf("store->active_memtable %p\n", store->active_memtable);
    printf("SIZE_IN_BYTES: %zu ; limit : %d\n",
           store->active_memtable->size_in_bytes, kMemtableLimit);
    if (store->active_memtable->size_in_bytes > kMemtableLimit) {
      pthread_mutex_lock(&store->inactive_memtables.lock);
      SLQueue_push_back(&store->inactive_memtables.queue,
                        store->active_memtable);

      pthread_cond_signal(&store->inactive_memtables.condition);
      pthread_mutex_unlock(&store->inactive_memtables.lock);
      store->active_memtable = sl_s_init();
    }
    out->status = 0;
    break;
  case COMMAND_TYPE_MERGE:
    merge_and_compact_level_zero(store->sstables);
  }
  if (out->status == 2) {
    const char *r = "Invalid request!";
    memcpy(out->data, r, 16);
    out->data_len = 16;
  }
  return;
}

bool try_one_request(struct Conn *conn) {
  if (conn->incoming->size < 4) {
    msg("not enough data in incoming buffer");
    return false;
  }
  u32 msg_len;
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

  PtrArray *cmd_arr = ptr_array_new_full(3, lstring_free);
  u8 buf[conn->incoming->size - 4];
  byte_ring_copy_n(conn->incoming, buf, 4, conn->incoming->size - 4);
  parse_request(buf, sizeof(buf), cmd_arr);
  struct Command command = {0};
  parse_command(cmd_arr, &command);
  struct Response resp = {0};
  // TODO: ehhhh weird case; well btw this needs to be coordinated with the size
  // of outgoing or maybe it should work independently. i had the case where
  // before these changes i had issues with v large values soooo it needs to be
  // stress tested also, appending entire resp.data to outgoing made no sense;
  // so i added a resp.len attr
  resp.data = calloc(k_max_len, 1); // calloc to set everything to 0
  handle_command(&command, &resp);
  u32 resp_len = htonl(sizeof(resp.status) + resp.data_len);
  byte_ring_append_n(conn->outgoing, (const u8 *)&resp_len, 4);
  byte_ring_append_n(conn->outgoing, (const u8 *)&resp.status, 4);
  byte_ring_append_n(conn->outgoing, (const u8 *)resp.data, resp.data_len);
  free(resp.data);
  if (command.type == COMMAND_TYPE_GET || command.type == COMMAND_TYPE_DELETE ||
      resp.status == 2) {
    ptr_array_free(cmd_arr, true);
  }
  if (command.type == COMMAND_TYPE_SET && resp.status != 2) {
    lstring_free(ptr_array_index(cmd_arr, 0));
    free(cmd_arr->data);
    free(cmd_arr);
  }

  /*byte_ring_debug_print(conn->incoming);*/
  byte_ring_pop_first_n(conn->incoming, 4 + msg_len);
  /*byte_ring_debug_print(conn->incoming);*/

  return true;
}

void handle_read(struct Conn *conn) {
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
  //   printf("replied to fd=%d\n", conn->fd);
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
void *dump_memtable_to_sstable(void *arg) {
  InactiveMemtables *inactive_memtables = (InactiveMemtables *)arg;

  while (1) {
    pthread_mutex_lock(&inactive_memtables->lock);
    while (SLQueue_is_empty(&inactive_memtables->queue) &&
           inactive_memtables->running) {
      pthread_cond_wait(&inactive_memtables->condition,
                        &inactive_memtables->lock);
    }
    if (SLQueue_is_empty(&inactive_memtables->queue) &&
        !inactive_memtables->running) {
      pthread_mutex_unlock(&inactive_memtables->lock);
      break;
    }

    SkipList *mt = *SLQueue_front(&inactive_memtables->queue);
    SLQueue_pop_front(&inactive_memtables->queue);

    pthread_mutex_unlock(&inactive_memtables->lock);

    // do work
    char sst_filepath[128];
    unsigned int file_id = atomic_load(&store->sstable_count);
    int len = snprintf(sst_filepath, 128, "%u.sst", file_id);
    SSTable *sst =
        dump_memtable_to_sst(mt, lstring_create_from_buf(len, sst_filepath));
    if (sst) {
      atomic_fetch_add(&store->sstable_count, 1);
      array_push(store->sstables, sst);
    }

    sl_s_destroy(mt);
    free(mt);
  }

  return NULL;
}

int main(void) {
  store = store_init();

  pthread_t worker_thread;
  // pthread_create arguments:
  // 1. Pointer to the thread variable
  // 2. Attributes (NULL for default)
  // 3. The function to run
  // 4. The argument to pass to that function
  if (pthread_create(&worker_thread, NULL, dump_memtable_to_sstable,
                     (void *)&store->inactive_memtables) != 0) {
    perror("Failed to create thread");
    return 1;
  }
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
