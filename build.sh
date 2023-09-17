#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -Wpedantic -Ofast $(pkg-config --cflags raylib)"
LFLAGS="$(pkg-config  --libs raylib) -lpthread -lm"
SRC="main.c fft.c"

# shellcheck disable=SC2086
cc -o musializer $SRC $CFLAGS $LFLAGS

# Tests
CFLAGS_TEST="-Wall -Wextra -Wpedantic -Ofast"
LFLAGS_TEST="-lm"

# shellcheck disable=SC2086
cc fft.c fft_test.c -o fft_test $CFLAGS_TEST $LFLAGS_TEST
