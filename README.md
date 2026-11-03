# Video-SIMDBench 2.0

A benchmark for measuring the auto-vectorization performance of compilers in
video applications. It runs the C (compiler-vectorized) kernels of real video
codecs — pixel SAD/SSD/SATD, DCT/IDCT, motion compensation, intra prediction,
loop filtering / deblocking, quantization, entropy coding — and prints a
timing table (cycles per call; the `C` column is always there, SIMD columns
appear when asm kernels are built).

Version 2.0 embeds the current C kernels of **x264**, **x265** and **dav1d**
directly in this repository (copied, not linked) on top of the modernized
harness (CMake build, works on x86_64 and arm64, macOS and Linux). No external
dependencies are needed to build.

## Compile

Requirements: CMake >= 3.16, a C99 compiler and a C++17 compiler
(Apple clang or GCC).

```sh
cmake -S . -B build
cmake --build build -j8
```

This builds the `vbench` executable from `src/` plus every `kernels/<codec>/`
package found (x264, x265, dav1d). On x86_64 Linux with `nasm` installed,
`-DVBENCH_ASM=ON` additionally assembles the x86 asm kernels (off by default).

Legacy alternative (C kernels only): `cd src && make` or `./compile.sh`.

## Run

```sh
./build/vbench --bench              # run all checks + timing table
./build/vbench --bench=x264:        # only x264 kernels
./build/vbench --bench=dav1d:       # only dav1d kernels
./build/vbench --bench 12345        # fixed random seed
```

`--bench=<pattern>` filters kernels by name prefix. A successful run prints
`VideoBench: All tests passed` followed by the timing table, e.g.:

```
            x264: satd 16x16 :   14   0  0 ...
            x265: dct 32x32 :    5358 0  0 ...
           dav1d: cdef 8x8 :     173  0  0 ...
```

## Layout

```
├── CMakeLists.txt        # build; auto-discovers kernels/<codec>/
├── PORTING.md            # how to add a new codec kernel package
├── src/                  # the benchmark harness (x264-derived bench modules)
├── kernels/x264/         # 42 embedded x264 C kernels    + bench_x264.c   (upstream: code.videolan.org/videolan/x264)
├── kernels/x265/         # 40 embedded x265 C++ kernels  + bench_x265.cpp (upstream: bitbucket.org/multicoreware/x265_git)
├── kernels/dav1d/        # 27 embedded dav1d 8bpc C kernels + bench_dav1d.c (upstream: code.videolan.org/videolan/dav1d)
└── LICENSE               # GPL (harness, derived from x264); kernels keep their upstream licenses in each kernels/<codec>/ dir
```

The codec kernel sources are copied into `kernels/<codec>/` (with symbols
prefixed, e.g. `x264_` → `vbx264_`) together with their upstream LICENSE/COPYING
files, so the tree is fully self-contained — nothing is cloned or fetched at
build time.

Adding another codec: create `kernels/<codec>/` with the copied sources, a
`bench_<codec>.c` implementing `vbench_codec_<codec>()`, and a
`CMakeLists.txt` — see `PORTING.md`. No harness changes needed.
