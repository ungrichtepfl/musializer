#!/bin/bash

set -ex

rm -f build/{*.wasm,*.js,*.html}

mkdir -p build/

emcc -o build/musializer.js \
  ./src/main.c ./src/musializer.c ./src/fft.c \
  -Os -Wall \
  -lm -lpthread -ldl \
  -I ./raylib-5.0_wasm/include/ -L./raylib-5.0_wasm/lib -l:libraylib.a \
  -s USE_GLFW=3 -s ASYNCIFY -s TOTAL_STACK=512mb -DPLATFORM_WEB \

cp build/musializer.js build/musializer.wasm . 

serve -s
