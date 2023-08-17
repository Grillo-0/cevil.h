#!/bin/sh

set -xe
clang -Wall -Wextra -Wswitch-enum -pedantic -ggdb -o test_cevil test_cevil.c
./test_cevil
