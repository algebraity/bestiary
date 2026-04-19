#!/bin/bash

gcc -c poni.c -o poni.o
gcc -c test_poni.c -o test_poni.o
gcc test_poni.o poni.o sokko.o hebi.o -o test_poni -lm
