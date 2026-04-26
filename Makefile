.DEFAULT_GOAL := all

CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2
CPPFLAGS += -Iinclude
DEPFLAGS  = -MMD -MP

OBJDIR := build/obj
BINDIR := build/bin

BEAST_SRCS = \
	src/beasts/hebi.c \
	src/beasts/sokko.c \
	src/beasts/usagi.c \
	src/beasts/poni.c \
	src/beasts/ookami.c \
	src/beasts/tora.c \
	src/beasts/neko.c

CORE_SRCS = \
	src/core/lexer.c \
	src/core/ast.c \
	src/core/parser.c \
	src/core/value.c \
	src/core/eval.c

REPL_SRC = src/repl.c

TEST_SRCS = \
	tests/test_sokko.c \
	tests/test_usagi.c \
	tests/test_poni.c \
	tests/test_ookami.c \
	tests/test_tora.c \
	tests/test_neko.c

BEAST_OBJS = $(patsubst src/%.c,$(OBJDIR)/src/%.o,$(BEAST_SRCS))
CORE_OBJS = $(patsubst src/%.c,$(OBJDIR)/src/%.o,$(CORE_SRCS))
REPL_OBJ = $(patsubst src/%.c,$(OBJDIR)/src/%.o,$(REPL_SRC))
TEST_OBJS = $(patsubst tests/%.c,$(OBJDIR)/tests/%.o,$(TEST_SRCS))

REPL_BIN := $(BINDIR)/repl
TEST_BINS = \
	$(BINDIR)/test_sokko \
	$(BINDIR)/test_usagi \
	$(BINDIR)/test_poni \
	$(BINDIR)/test_ookami \
	$(BINDIR)/test_tora \
	$(BINDIR)/test_neko

DEPFILES = $(BEAST_OBJS:.o=.d) $(CORE_OBJS:.o=.d) $(REPL_OBJ:.o=.d) $(TEST_OBJS:.o=.d)

.PHONY: all repl beasts pipeline tests clean

all: repl

repl: $(REPL_BIN)

beasts: $(BEAST_OBJS)

pipeline: $(CORE_OBJS) $(REPL_OBJ)

tests: $(TEST_BINS)

$(REPL_BIN): $(CORE_OBJS) $(REPL_OBJ) $(BEAST_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm -lreadline

$(BINDIR)/test_sokko: $(OBJDIR)/tests/test_sokko.o $(OBJDIR)/src/beasts/sokko.o $(OBJDIR)/src/beasts/hebi.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BINDIR)/test_usagi: $(OBJDIR)/tests/test_usagi.o $(OBJDIR)/src/beasts/usagi.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BINDIR)/test_poni: $(OBJDIR)/tests/test_poni.o $(OBJDIR)/src/beasts/poni.o $(OBJDIR)/src/beasts/sokko.o $(OBJDIR)/src/beasts/hebi.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BINDIR)/test_ookami: $(OBJDIR)/tests/test_ookami.o $(OBJDIR)/src/beasts/ookami.o $(OBJDIR)/src/beasts/hebi.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BINDIR)/test_tora: $(OBJDIR)/tests/test_tora.o $(OBJDIR)/src/beasts/tora.o $(OBJDIR)/src/beasts/sokko.o $(OBJDIR)/src/beasts/usagi.o $(OBJDIR)/src/beasts/hebi.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BINDIR)/test_neko: $(OBJDIR)/tests/test_neko.o $(OBJDIR)/src/beasts/neko.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BINDIR):
	@mkdir -p $@

clean:
	rm -rf build

-include $(DEPFILES)
