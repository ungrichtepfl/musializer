#!/bin/sh

set -xe

mkdir -p ./build

CFLAGS="-Wall -Wextra -Wpedantic -Og $(pkg-config --cflags raylib)"
LFLAGS="$(pkg-config  --libs raylib) -lpthread -lm"

# shellcheck disable=SC2086
cc -o ./build/musializer ./src/main.c $CFLAGS $LFLAGS -ldl 

# Tests
CFLAGS_TEST="-Wall -Wextra -Wpedantic -Ofast"
LFLAGS_TEST="-lm"

# shellcheck disable=SC2086
cc ./src/fft.c ./src/fft_test.c -o ./build/fft_test $CFLAGS_TEST $LFLAGS_TEST
