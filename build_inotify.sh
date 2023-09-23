#!/bin/sh

set -ex

cc -Wall -Wpedantic -Wextra -o build/inotify_test src/inotify_test.c
