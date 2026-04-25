#!/bin/bash
# Build the bestiary pipeline (lex -> parse -> eval) and the REPL.
# Link against each BEAST you want exposed to commands.

set -e
CC="${CC:-gcc}"
CFLAGS="${CFLAGS:--Wall -Wextra -O2}"

INC="-I hebi -I sokko -I usagi -I poni -I ookami-ng -I tora"

# Make sure hebi.o exists (we call pi/e/constructFraction from eval.c)
(cd hebi && $CC $CFLAGS -c hebi.c -o hebi.o)

$CC $CFLAGS $INC -c lexer.c  -o lexer.o
$CC $CFLAGS $INC -c parser.c -o parser.o
$CC $CFLAGS $INC -c ast.c    -o ast.o
$CC $CFLAGS $INC -c value.c  -o value.o
$CC $CFLAGS $INC -c eval.c   -o eval.o
$CC $CFLAGS $INC -c repl.c   -o repl.o

$CC $CFLAGS -o repl \
    repl.o eval.o value.o parser.o ast.o lexer.o hebi/hebi.o \
    -lm -lreadline

echo "built ./repl"
