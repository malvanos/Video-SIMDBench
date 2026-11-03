#!/bin/sh
# Legacy one-shot compile script for VideoBench.
# CMake is the primary build (see ../CMakeLists.txt):
#     cmake -S .. -B ../build && cmake --build ../build -j8
#
# This script compiles the C kernels only (no asm) into ./bench:
#     ./compile.sh            # uses cc, -O3, portable (no -march=native flags)
#     CC=clang ./compile.sh
set -e
cd "$(dirname "$0")"

make vbench_config.h

CC=${CC:-cc}
# shellcheck disable=SC2086
$CC -O3 -Wall -Wno-sign-compare --std=gnu99 -I./ *.c c_kernels/*.c -o bench -lm
echo "built ./bench"
