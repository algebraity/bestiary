#!/bin/bash

gcc -c ookami.c -o ookami.o
gcc -c test_ookami.c -o test_ookami.o
gcc test_ookami.o ookami.o hebi.o -o test_ookami -lm
