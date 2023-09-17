#!/bin/sh

set -xe

mkdir -p ./build

CFLAGS="-Wall -Wextra -Wpedantic -Og $(pkg-config --cflags raylib)"
LFLAGS="$(pkg-config  --libs raylib) -lpthread -lm"

# shellcheck disable=SC2086
cc -o ./build/libplug.so ./plug.c ./fft.c $CFLAGS $LFLAGS -fPIC -shared
