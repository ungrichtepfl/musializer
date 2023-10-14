#!/bin/sh

set -xe

mkdir -p ./build

RAYLIB_VERSION="4.2.0" # Corresponds to raylib 4.5

CFLAGS="-Wall -Wextra -Wpedantic -Ofast $(pkg-config --cflags "raylib = $RAYLIB_VERSION")"
LFLAGS="$(pkg-config  --libs "raylib = $RAYLIB_VERSION") -lpthread -lm"

# shellcheck disable=SC2086
cc -c -o ./build/plug.o ./src/plug.c $CFLAGS -fPIC

# shellcheck disable=SC2086
cc -o ./build/libplug.so ./build/plug.o ./src/fft.c $CFLAGS $LFLAGS -shared
