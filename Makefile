CC = gcc
CXX = g++

# Compiler flags
# -fanalyzer
CFLAGS = -I./include -I$(HOME)/.local/include -I./include/stc -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/opt/local/include -Wall -Wextra -O0 -g
CXXFLAGS = -I./include -I$(HOME)/.local/include -I./include/stc -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -Wall -Wextra -O0 -g
# CFLAGS = -I./include -I$(HOME)/.local/include -I./include/stc -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -Wall -Wextra -O2
# CXXFLAGS = -I./include -I$(HOME)/.local/include -I./include/stc -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -Wall -Wextra -O2

# Linker flags
LDFLAGS = -llz4

RM = rm -f

# Directories
SRCDIR = src
LIBDIR = $(SRCDIR)/libs
OBJDIR = obj
BINDIR = bin
TESTDIR = tests

# Explicit source files for server (matching your current build command)
SERVER_LIB_SRCS = \
	$(LIBDIR)/u_array.c \
	$(LIBDIR)/skiplist_str.c \
	$(LIBDIR)/lstr.c \
	$(LIBDIR)/persistence3.c \
	$(LIBDIR)/bytering.c \
	$(LIBDIR)/hmap_si.c \
	$(LIBDIR)/hmap.c \
	$(LIBDIR)/u_ptr_array.c

SERVER_SRC = $(SRCDIR)/server.c
SERVER_EXEC = server_latest_compress

# Client (C++ file)
CLIENT_SRC = $(SRCDIR)/client.cpp
CLIENT_EXEC = $(BINDIR)/client

# Object files (optional, for incremental builds)
SERVER_LIB_OBJS = $(patsubst $(LIBDIR)/%.c,$(OBJDIR)/libs_%.o,$(SERVER_LIB_SRCS))
SERVER_OBJ = $(OBJDIR)/server.o

# Default target
all: $(SERVER_EXEC)

# Build server (direct compilation, matching your command)
$(SERVER_EXEC): $(SERVER_SRC) $(SERVER_LIB_SRCS)
	$(CC) $(CFLAGS) $(SERVER_LIB_SRCS) $(SERVER_SRC) -o $@ $(LDFLAGS)

# Build server with object files (for faster incremental builds)
server-obj: $(SERVER_OBJ) $(SERVER_LIB_OBJS)
	$(CC) $(CFLAGS) $(SERVER_LIB_OBJS) $(SERVER_OBJ) -o $(SERVER_EXEC) $(LDFLAGS)

# Build client
client: $(CLIENT_EXEC)

$(CLIENT_EXEC): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Object file rules (for incremental builds)
$(OBJDIR)/libs_%.o: $(LIBDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/server.o: $(SERVER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create obj directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Create bin directory if it doesn't exist
$(BINDIR):
	mkdir -p $(BINDIR)

# Test targets
tests: test_hmap test_bytering

test_hmap: $(OBJDIR)/test_hmap.o $(OBJDIR)/libs_hmap.o $(OBJDIR)/libs_lstr.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_hmap $^

test_bytering: $(OBJDIR)/test_bytering.o $(OBJDIR)/libs_bytering.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_bytering $^

# Test object files
$(OBJDIR)/test_hmap.o: $(TESTDIR)/test_hmap.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/test_bytering.o: $(TESTDIR)/test_bytering.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean targets
clean:
	$(RM) $(OBJDIR)/*.o $(BINDIR)/* $(SERVER_EXEC)

clean-all: clean
	$(RM) -r $(OBJDIR) $(BINDIR)

# Individual library builds (for testing/development)
lib-u_array: $(OBJDIR)/libs_u_array.o
lib-skiplist_str: $(OBJDIR)/libs_skiplist_str.o
lib-lstr: $(OBJDIR)/libs_lstr.o
lib-persistence3: $(OBJDIR)/libs_persistence3.o
lib-bytering: $(OBJDIR)/libs_bytering.o
lib-hmap_si: $(OBJDIR)/libs_hmap_si.o
lib-hmap: $(OBJDIR)/libs_hmap.o
lib-u_ptr_array: $(OBJDIR)/libs_u_ptr_array.o

.PHONY: all server-obj client tests clean clean-all lib-u_array lib-skiplist_str lib-lstr lib-persistence3 lib-bytering lib-hmap_si lib-hmap lib-u_ptr_array
