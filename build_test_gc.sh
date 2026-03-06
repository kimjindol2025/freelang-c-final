#!/bin/bash
# Build script for test_gc only (excludes problematic files)

set -e

CC="gcc"
CFLAGS="-Wall -Wextra -O2 -std=c11 -I./include -I./lib/src -D_POSIX_C_SOURCE=200809L -g -O0 -DDEBUG"
LDFLAGS="-lm"

echo "Building test_gc (GC unit tests)..."
echo ""

mkdir -p bin

echo "Compiling: src/gc.c"
$CC $CFLAGS -c src/gc.c -o obj/gc_temp.o

echo "Compiling: test/test_gc.c"
$CC $CFLAGS -c test/test_gc.c -o obj/test_gc_temp.o

echo "Linking: bin/test_gc"
$CC $CFLAGS -o bin/test_gc obj/gc_temp.o obj/test_gc_temp.o $LDFLAGS

# Cleanup temp objects
rm -f obj/gc_temp.o obj/test_gc_temp.o

echo ""
echo "✅ Build successful!"
echo ""
echo "Running tests..."
echo ""
./bin/test_gc
