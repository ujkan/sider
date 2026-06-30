CC      = gcc
CXX     = g++

UNITY_DIR ?= ./Unity-2.6.1/src

CFLAGS   = -I./include -I$(UNITY_DIR) -I$(HOME)/.local/include \
           -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
           -I/opt/local/include \
           -Wall -Wextra -O0 -g -gdwarf-4

CXXFLAGS = -I./include -I$(UNITY_DIR) -I$(HOME)/.local/include \
           -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
           -Wall -Wextra -O0 -g -gdwarf-4

LDFLAGS  = -L$(HOME)/.local/lib -L/opt/local/lib -llz4

COVERAGE_CFLAGS   = $(CFLAGS) --coverage
COVERAGE_CXXFLAGS = $(CXXFLAGS) --coverage
COVERAGE_LDFLAGS  = $(LDFLAGS) --coverage

RM = rm -f

# Directories
SRCDIR  = src
LIBDIR  = $(SRCDIR)/libs
OBJDIR  = obj
BINDIR  = bin
TESTDIR = tests

# Unity
UNITY_SRC = $(UNITY_DIR)/unity.c
UNITY_OBJ = $(OBJDIR)/unity.o
UNITY_LIB = $(OBJDIR)/libunity.a

# Server
SERVER_LIB_SRCS = \
	$(LIBDIR)/u_array.c \
	$(LIBDIR)/skiplist_str.c \
	$(LIBDIR)/lstr.c \
	$(LIBDIR)/persistence.c \
	$(LIBDIR)/bytering.c \
	$(LIBDIR)/hmap_si.c \
	$(LIBDIR)/block.c \
	$(LIBDIR)/block_item.c \
	$(LIBDIR)/index_block.c \
	$(LIBDIR)/scribe.c \
	$(LIBDIR)/stb_impl.c \
	$(LIBDIR)/hex_dump.c \
	$(LIBDIR)/u_ptr_array.c

SERVER_SRC      = $(SRCDIR)/server.c
SERVER_EXEC     = $(BINDIR)/server
SERVER_LIB_OBJS = $(patsubst $(LIBDIR)/%.c,$(OBJDIR)/libs_%.o,$(SERVER_LIB_SRCS))
SERVER_OBJ      = $(OBJDIR)/server.o

# Client
CLIENT_SRC  = $(SRCDIR)/client.cpp
CLIENT_EXEC = $(BINDIR)/client

# Test dependency bundles
CORE_OBJS        = $(OBJDIR)/libs_u_array.o $(OBJDIR)/libs_lstr.o $(OBJDIR)/libs_scribe.o \
                   $(OBJDIR)/libs_stb_impl.o $(OBJDIR)/libs_skiplist_str.o
BLOCK_OBJS       = $(CORE_OBJS) $(OBJDIR)/libs_block.o $(OBJDIR)/libs_block_item.o \
                   $(OBJDIR)/libs_index_block.o $(OBJDIR)/libs_hex_dump.o
PERSISTENCE_OBJS = $(BLOCK_OBJS) $(OBJDIR)/libs_persistence.o $(OBJDIR)/libs_bytering.o \
                   $(OBJDIR)/libs_hmap_si.o $(OBJDIR)/libs_u_ptr_array.o
HMAP_OBJS        = $(CORE_OBJS) $(OBJDIR)/libs_hmap.o $(OBJDIR)/libs_hmap_si.o
BYTERING_OBJS    = $(CORE_OBJS) $(OBJDIR)/libs_bytering.o

TEST_UTILS_SRC = $(TESTDIR)/utils.c
TEST_UTILS_OBJ = $(OBJDIR)/test_utils.o

TEST_SRCS = $(filter-out $(TESTDIR)/utils.c,$(wildcard $(TESTDIR)/*.c))
TEST_BINS = $(patsubst $(TESTDIR)/%.c,$(BINDIR)/%,$(TEST_SRCS))

# ── Default target ────────────────────────────────────────────────────────────

.DEFAULT_GOAL := all
all: $(SERVER_EXEC) $(CLIENT_EXEC)

# ── Directories ───────────────────────────────────────────────────────────────

$(OBJDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

# ── Library object files ──────────────────────────────────────────────────────

$(OBJDIR)/libs_%.o: $(LIBDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Server ────────────────────────────────────────────────────────────────────
# Always build via object files so .o files exist on disk when dsymutil runs.

$(SERVER_OBJ): $(SERVER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER_EXEC): $(SERVER_OBJ) $(SERVER_LIB_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# ── Client ────────────────────────────────────────────────────────────────────

CLIENT_OBJ = $(OBJDIR)/client.o

$(OBJDIR)/client.o: $(CLIENT_SRC) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CLIENT_EXEC): $(CLIENT_OBJ) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# ── Unity ─────────────────────────────────────────────────────────────────────

$(UNITY_OBJ): $(UNITY_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(UNITY_LIB): $(UNITY_OBJ)
	ar rcs $@ $^

# ── Test utils ────────────────────────────────────────────────────────────────

$(TEST_UTILS_OBJ): $(TEST_UTILS_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Test binaries ─────────────────────────────────────────────────────────────

$(BINDIR)/test_block: $(TESTDIR)/test_block.c $(TEST_UTILS_OBJ) $(BLOCK_OBJS) $(UNITY_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(TEST_UTILS_OBJ) $(BLOCK_OBJS) $(UNITY_LIB) -o $@ $(LDFLAGS)

$(BINDIR)/test_persistence: $(TESTDIR)/test_persistence.c $(TEST_UTILS_OBJ) $(PERSISTENCE_OBJS) $(UNITY_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(TEST_UTILS_OBJ) $(PERSISTENCE_OBJS) $(UNITY_LIB) -o $@ $(LDFLAGS)

$(BINDIR)/test_index_section: $(TESTDIR)/test_index_section.c $(BLOCK_OBJS) $(UNITY_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(BLOCK_OBJS) $(UNITY_LIB) -o $@ $(LDFLAGS)

$(BINDIR)/test_hmap: $(TESTDIR)/test_hmap.c $(HMAP_OBJS) $(UNITY_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(HMAP_OBJS) $(UNITY_LIB) -o $@ $(LDFLAGS)

$(BINDIR)/test_bytering: $(TESTDIR)/test_bytering.c $(BYTERING_OBJS) $(UNITY_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(BYTERING_OBJS) $(UNITY_LIB) -o $@ $(LDFLAGS)

# Generic test rule for anything not listed above
$(BINDIR)/%: $(TESTDIR)/%.c $(TEST_UTILS_OBJ) $(PERSISTENCE_OBJS) $(UNITY_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $< $(TEST_UTILS_OBJ) $(PERSISTENCE_OBJS) $(UNITY_LIB) -o $@ $(LDFLAGS)

# ── Test runners ──────────────────────────────────────────────────────────────

tests: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "→ $$t"; $$t; done

test: tests

test-block: $(BINDIR)/test_block
	$<

test-persistence: $(BINDIR)/test_persistence
	$<

test-index-section: $(BINDIR)/test_index_section
	$<

test_hmap: $(BINDIR)/test_hmap
	$<

test_bytering: $(BINDIR)/test_bytering
	$<

# ── Coverage ──────────────────────────────────────────────────────────────────

coverage:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(COVERAGE_CFLAGS)" CXXFLAGS="$(COVERAGE_CXXFLAGS)" LDFLAGS="$(COVERAGE_LDFLAGS)" tests

# ── Object file rule for test sources ─────────────────────────────────────────

$(OBJDIR)/%.o: $(TESTDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Individual lib targets ────────────────────────────────────────────────────

lib-%: $(OBJDIR)/libs_%.o ;

# ── Clean ─────────────────────────────────────────────────────────────────────

clean:
	$(RM) $(OBJDIR)/*.o $(OBJDIR)/*.a
	$(RM) -r $(BINDIR)

clean-all: clean
	$(RM) -r $(OBJDIR)

.PHONY: all tests test test-block test-persistence test-index-section \
        test_hmap test_bytering coverage clean clean-all
