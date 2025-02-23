#!/bin/bash

set -ex

rm -f build/{*.wasm,*.js,*.html}

mkdir -p build/

emcc -o build/musializer.js \
  ./src/main.c ./src/musializer.c ./src/fft.c \
  -Os -Wall \
  -lm -lpthread -ldl \
  -I ./raylib-5.0_wasm/include/ -L./raylib-5.0_wasm/lib -l:libraylib.a \
  -sUSE_GLFW=3 -sASYNCIFY -sMODULARIZE=1 -sEXPORT_NAME=createMusializer \
  -sTOTAL_STACK=512mb -DPLATFORM_WEB \

cp build/musializer.js build/musializer.wasm . 

serve -s
