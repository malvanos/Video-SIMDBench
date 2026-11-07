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

This builds the `vbench` executable from `src/` plus every `src/<codec>/`
package found (x264, x265, dav1d). Architecture-specific assembly kernels are
built by default on aarch64 (NEON, via the system compiler) and on x86_64
Linux when `nasm` is installed (`-DVBENCH_ASM=OFF` to disable).

## Run

```sh
./build/vbench --bench              # run all checks + timing table
./build/vbench --bench=x264:        # only x264 kernels
./build/vbench --bench=dav1d:       # only dav1d kernels
./build/vbench --bench 12345        # fixed random seed
```

`--bench=<pattern>` filters kernels by name prefix. Each run prints the
compiler, architecture, CPU model and core count, a `CPU supported:` summary,
then `VideoBench: All tests passed` and the timing table. Every kernel row
shows three measurements: `C-plain` (same sources compiled with
auto-vectorization disabled), `C` (auto-vectorized at `-O3`), and the ISA
columns the CPU supports with hand-written assembly (on arm64: `ARMv8`,
`NEON`; on x86: `MMX` … `AVX2` as available), e.g.:

```
VideoBench: compiler clang 21.0.0 (...), arch aarch64 (64-bit), cpu "Apple M2 Pro", 10 core(s)
CPU supported: ARMv8 NEON
                               :     C  C-plain  ARMv8  NEON
            x264: satd 16x16  :    10      30      -      4
            x265: dct 32x32  :   138     427      -      -
           dav1d: cdef 8x8    :   109     216      -     17
dav1d: mc put_8tap_regular 32x32 :   209    1200      -     73
```

A `-` cell means no kernel is registered for that column (e.g. x265 4.2
implements 32x32 DCT as NEON intrinsics in the C sources, so there is no
separate asm kernel on arm64). All three variants of a kernel are compared
for correctness on every run, not just timed.

## Layout

```
├── CMakeLists.txt        # build; auto-discovers src/<codec>/
├── PLAN.md               # the restructure plan + status
├── PORTING.md            # how to add a new codec kernel package + how to sync it with upstream
├── tools/verify.sh       # regression script: clean build + bench assertions
├── cmake/                # shared build helpers (dual vec/scalar codec build, asm build)
├── src/                  # the benchmark harness + the codec packages
│   ├── *.c *.h           # harness sources (main, bench, cpu, ...)
│   ├── x264/             # upstream x264 C kernels + x86/ + aarch64/ asm + bench_x264.c  (code.videolan.org/videolan/x264)
│   ├── x265/             # upstream x265 C++ kernels + x86/ + aarch64/ asm + bench_x265.cpp (bitbucket.org/multicoreware/x265_git)
│   ├── dav1d/            # upstream dav1d 8bpc C kernels + src/{x86,arm}/ asm + bench_dav1d.c (code.videolan.org/videolan/dav1d)
│   └── tools/            # sync-<codec>.sh: mechanical upstream-sync scripts (see PORTING.md)
└── LICENSE               # GPL (harness, derived from x264); codecs keep their upstream licenses in each src/<codec>/ dir
```

The codec kernel sources are copied into `src/<codec>/` under their native
upstream symbol names (C and asm together, so the asm-offsets compile-time
check keeps them ABI-matched) along with upstream LICENSE/COPYING files, so
the tree is fully self-contained — nothing is cloned or fetched at build time.
Each codec is compiled twice (auto-vectorized `-O3` and scalar
`-fno-tree-vectorize`) from the same sources; the scalar build's symbols are
renamed at build time, so the repo holds a single copy. To update a package to
a newer upstream ref, run its `src/tools/sync-<codec>.sh` script (clones
upstream into `/tmp` as a reference, then copies the C and asm sources together
into `src/<codec>/`) — see `PORTING.md`, "Syncing with upstream".

Adding another codec: create `src/<codec>/` with the copied sources, a
`bench_<codec>.c` implementing `vbench_codec_<codec>()`, and a
`CMakeLists.txt` — see `PORTING.md`. No harness changes needed.
