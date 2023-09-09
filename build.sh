#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -Wpedantic $(pkg-config --cflags raylib)"
LFLAGS=$(pkg-config  --libs raylib)

# shellcheck disable=SC2086
cc -o musializer main.c $CFLAGS $LFLAGS
