# Musializer

Inspiration from [Tsoding the Legend](https://www.youtube.com/watch?v=Xdbk1Pr5WXU&list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT).

## Raylib

Raylib version [4.5.0](https://github.com/raysan5/raylib/releases/tag/4.5.0) is needed.

See [raylib](https://www.raylib.com/) for installation.

On ubuntu check out this [link](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux)

## Build & Run

Hot reloading **disabled** (for users):

```shell
./build.sh && ./build/musializer
```

Hot reloading **enabled** (for developers):

```shell
./build.sh -d && ./build_plug.sh && ./build/musializer
```

When hot reloading is enabled you can modify the `./src/plug.c` file at runtime and
then load the modification by re-running:

```shell
./build_plug.sh
```

You can also reload it manually by pressing `R`.

## Test files

Run `./build.sh` and `./build_inotify.sh` to compile the two test files `fft_test.c`
and `inotify_test.c`. The binaries can then be found in `./build/`
