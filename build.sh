#!/bin/sh

set -xe

mkdir -p ./build

if [ "$1" = "--dynamic" ] || [ "$1" = "-d" ]; then
    ./build_plug.sh
    CFLAGS="-Wall -Wextra -Wpedantic -Ofast"
    LFLAGS="-ldl"
    CPPFLAGS="-DDYLIB"
    RAYLIB="" # No raylib needed, will be handled by build_plug.sh
    SRC="./src/main.c"
else
    CFLAGS="-Wall -Wextra -Wpedantic -Ofast"
    LFLAGS="-lm -lpthread"
    CPPFLAGS=""
    if [ "$(uname -m)" = "x86_64" ]; then
        RAYLIB="-I ./raylib-5.0_linux_amd64/include/ -L./raylib-5.0_linux_amd64/lib -l:libraylib.a -ldl"
    else
        echo "You are not on a x86_64 machine, please install raylib 5.0.0 and make sure that pkg-config can find it."
        RAYLIB="$(pkg-config --libs --cflags "raylib")"
    fi
    SRC="./src/main.c ./src/plug.c ./src/fft.c"
fi


# shellcheck disable=SC2086
cc -o ./build/musializer $SRC $CPPFLAGS $CFLAGS $RAYLIB $LFLAGS

# Tests
CFLAGS_TEST="-Wall -Wextra -Wpedantic -Ofast"
LFLAGS_TEST="-lm"

# shellcheck disable=SC2086
cc ./src/fft.c ./src/fft_test.c -o ./build/fft_test $CFLAGS_TEST $LFLAGS_TEST
