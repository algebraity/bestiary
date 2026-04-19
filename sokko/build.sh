#!/bin/bash

gcc -c sokko.c -o sokko.o
gcc -c test_sokko.c -o test_sokko.o
gcc test_sokko.o sokko.o hebi.o -o test_sokko -lm
