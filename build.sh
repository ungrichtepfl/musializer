#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -Wpedantic -g $(pkg-config --cflags raylib)"
LFLAGS="$(pkg-config  --libs raylib) -lpthread"

# shellcheck disable=SC2086
cc -o musializer main.c $CFLAGS $LFLAGS
