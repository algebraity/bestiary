#!/bin/bash

gcc -c usagi.c -o usagi.o
gcc -c test_usagi.c -o test_usagi.o
gcc test_usagi.o usagi.o -o test_usagi -lm
