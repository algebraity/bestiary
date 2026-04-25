.DEFAULT_GOAL := all

CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2
CPPFLAGS += -I. -Ihebi -Isokko -Iusagi -Iponi -Iookami-ng -Itora -Ineko
LDLIBS   += -lm -lreadline
DEPFLAGS  = -MMD -MP

BEAST_OBJS = \
	hebi/hebi.o \
	sokko/sokko.o \
	usagi/usagi.o \
	poni/poni.o \
	ookami-ng/ookami.o \
	tora/tora.o \
	neko/neko.o

PIPELINE_OBJS = lexer.o ast.o parser.o value.o eval.o repl.o

TEST_OBJS = \
	sokko/test_sokko.o \
	usagi/test_usagi.o \
	poni/test_poni.o \
	ookami-ng/test_ookami.o \
	tora/test_tora.o

TEST_BINS = \
	sokko/test_sokko \
	usagi/test_usagi \
	poni/test_poni \
	ookami-ng/test_ookami \
	tora/test_tora

DEPFILES = $(BEAST_OBJS:.o=.d) $(PIPELINE_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

.PHONY: all beasts pipeline tests clean

all: repl

beasts: $(BEAST_OBJS)

pipeline: $(PIPELINE_OBJS)

tests: $(TEST_BINS)

repl: $(PIPELINE_OBJS) $(BEAST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(PIPELINE_OBJS) $(BEAST_OBJS) $(LDLIBS)

# Encode the intended BEAST build order so `make -j` still respects it.
sokko/sokko.o: | hebi/hebi.o
poni/poni.o: | hebi/hebi.o sokko/sokko.o
ookami-ng/ookami.o: | hebi/hebi.o
tora/tora.o: | hebi/hebi.o sokko/sokko.o usagi/usagi.o
neko/neko.o: | hebi/hebi.o

# Encode the main pipeline order explicitly as well.
parser.o: | lexer.o ast.o
value.o: | hebi/hebi.o
eval.o: | ast.o value.o
repl.o: | lexer.o parser.o ast.o eval.o value.o

lexer.o: lexer.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

ast.o: ast.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

parser.o: parser.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

value.o: value.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

eval.o: eval.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

repl.o: repl.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

hebi/hebi.o: hebi/hebi.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

sokko/sokko.o: sokko/sokko.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

usagi/usagi.o: usagi/usagi.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

poni/poni.o: poni/poni.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

ookami-ng/ookami.o: ookami-ng/ookami.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

tora/tora.o: tora/tora.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

neko/neko.o: neko/neko.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

sokko/test_sokko.o: sokko/test_sokko.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

usagi/test_usagi.o: usagi/test_usagi.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

poni/test_poni.o: poni/test_poni.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

ookami-ng/test_ookami.o: ookami-ng/test_ookami.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

tora/test_tora.o: tora/test_tora.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

sokko/test_sokko: sokko/test_sokko.o sokko/sokko.o hebi/hebi.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

usagi/test_usagi: usagi/test_usagi.o usagi/usagi.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

poni/test_poni: poni/test_poni.o poni/poni.o sokko/sokko.o hebi/hebi.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

ookami-ng/test_ookami: ookami-ng/test_ookami.o ookami-ng/ookami.o hebi/hebi.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

tora/test_tora: tora/test_tora.o tora/tora.o sokko/sokko.o usagi/usagi.o hebi/hebi.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f repl $(PIPELINE_OBJS) $(BEAST_OBJS) $(TEST_OBJS) $(TEST_BINS) $(DEPFILES)

-include $(DEPFILES)
