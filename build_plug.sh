#!/bin/sh

set -xe

mkdir -p ./build

CFLAGS="-Wall -Wextra -Wpedantic -Og $(pkg-config --cflags raylib)"
LFLAGS="$(pkg-config  --libs raylib) -lpthread -lm"

# shellcheck disable=SC2086
cc -c -o ./build/plug.o ./src/plug.c $CFLAGS -fPIC

# shellcheck disable=SC2086
cc -o ./build/libplug.so ./build/plug.o ./src/fft.c $CFLAGS $LFLAGS -shared
