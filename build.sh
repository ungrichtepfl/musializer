#!/bin/sh

set -xe

mkdir -p ./build

CFLAGS="-Wall -Wextra -Wpedantic -Og $(pkg-config --cflags raylib)"
LFLAGS="$(pkg-config  --libs raylib) -lpthread -lm"

SRC="main.c"

# shellcheck disable=SC2086
cc -o ./build/libplug.so plug.c fft.c $CFLAGS -fPIC -shared $LFLAGS

# shellcheck disable=SC2086
cc -o ./build/musializer $SRC $CFLAGS $LFLAGS -ldl

# Tests
CFLAGS_TEST="-Wall -Wextra -Wpedantic -Ofast"
LFLAGS_TEST="-lm"

# shellcheck disable=SC2086
cc fft.c fft_test.c -o ./build/fft_test $CFLAGS_TEST $LFLAGS_TEST
