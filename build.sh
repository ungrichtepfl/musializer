#!/bin/sh

set -xe

mkdir -p ./build

# Check if the flag static is set
if [ "$1" = "--static" ] || [ "$1" = "-s" ]; then
    CFLAGS="-Wall -Wextra -Wpedantic -Ofast $(pkg-config --cflags raylib)"
    LFLAGS="$(pkg-config  --libs raylib) -lpthread -lm"
    CPPFLAGS=""
    SRC="./src/main.c ./build/plug.o ./src/fft.c"
else
    CFLAGS="-Wall -Wextra -Wpedantic -Ofast"
    LFLAGS="-ldl"
    CPPFLAGS="-DDYLIB"
    SRC="./src/main.c"
fi

# shellcheck disable=SC2086
cc -o ./build/musializer $SRC $CPPFLAGS $CFLAGS $LFLAGS

# Tests
CFLAGS_TEST="-Wall -Wextra -Wpedantic -Ofast"
LFLAGS_TEST="-lm"

# shellcheck disable=SC2086
cc ./src/fft.c ./src/fft_test.c -o ./build/fft_test $CFLAGS_TEST $LFLAGS_TEST
