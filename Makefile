CC = gcc
CXX = g++

# It is recommended to use pkg-config to find the glib-2.0 flags:
# CFLAGS += $(shell pkg-config --cflags glib-2.0)
# LDFLAGS += $(shell pkg-config --libs glib-2.0)
CFLAGS = -Wall -pedantic -g -Iinclude -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
LDFLAGS = -lglib-2.0

RM = rm -f

SRCDIR = src
LIBDIR = $(SRCDIR)/libs
OBJDIR = obj
BINDIR = bin
TESTDIR = tests

# Find all .c files in libs
LIB_SRCS = $(wildcard $(LIBDIR)/*.c)
LIB_OBJS = $(patsubst $(LIBDIR)/%.c,$(OBJDIR)/%.o,$(LIB_SRCS))

SERVER_SRC = $(SRCDIR)/server.c
SERVER_OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SERVER_SRC))
SERVER_EXEC = $(BINDIR)/server

CLIENT_SRC = $(SRCDIR)/client.cpp
CLIENT_EXEC = $(BINDIR)/client

all: $(SERVER_EXEC) $(CLIENT_EXEC)

# Rule to build the server
$(SERVER_EXEC): $(SERVER_OBJ) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Rule to build the client
$(CLIENT_EXEC): $(CLIENT_SRC)
	$(CXX) $(CFLAGS) -o $@ $<

# Rule to compile .c files from src to .o files in obj
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile .c files from libs to .o files in obj
$(OBJDIR)/%.o: $(LIBDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	$(RM) $(OBJDIR)/*.o $(BINDIR)/*

# Test targets
tests: test_hmap test_bytering # test_vector

test_hmap: $(OBJDIR)/test_hmap.o $(OBJDIR)/hmap.o
	$(CC) $(CFLAGS) -o $(BINDIR)/test_hmap $^

test_vector: $(OBJDIR)/test_vector.o $(OBJDIR)/vector.o
	$(CC) $(CFLAGS) -o $(BINDIR)/test_vector $^

test_bytering: $(OBJDIR)/test_bytering.o $(OBJDIR)/bytering.o
	$(CC) $(CFLAGS) -o $(BINDIR)/test_bytering $^

$(OBJDIR)/test_hmap.o: $(TESTDIR)/test_hmap.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/test_vector.o: $(TESTDIR)/test_vector.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/test_bytering.o: $(TESTDIR)/test_bytering.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean tests
