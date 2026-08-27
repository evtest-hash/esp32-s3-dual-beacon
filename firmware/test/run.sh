#!/bin/sh
# Host unit tests. No ESP-IDF dependency -- beacon_data.c must stay free of
# any IDF dependency.
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR/.."
BUILD="$DIR/build"
mkdir -p "$BUILD"
cc -std=c11 -Wall -Wextra -Werror -g \
   -I "$ROOT/main" \
   "$DIR/test_beacon_data.c" "$ROOT/main/beacon_data.c" \
   -o "$BUILD/test_beacon_data"
exec "$BUILD/test_beacon_data"
