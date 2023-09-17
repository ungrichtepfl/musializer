#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -Wpedantic -Og $(pkg-config --cflags raylib)"
LFLAGS="$(pkg-config  --libs raylib) -lpthread"

# shellcheck disable=SC2086
cc -o musializer main.c $CFLAGS $LFLAGS

# Tests
CFLAGS_TEST="-Wall -Wextra -Wpedantic -Ofast"
LFLAGS_TEST="-lm"

# shellcheck disable=SC2086
cc fft.c fft_test.c -o fft_test $CFLAGS_TEST $LFLAGS_TEST
