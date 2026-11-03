# Video-SIMDBench

Video-SIMDBench: A benchmark for measuring the auto-vectorization performance
of compilers in video applications. It runs the C (compiler-vectorized) kernels
of an x264-style video codec pipeline — pixel SAD/SSD/SATD, DCT/IDCT, motion
compensation, intra prediction, deblocking, quantization, CABAC/bitstream —
and prints a timing table (cycles per call, C column by default; SIMD columns
appear when asm kernels are built).

## Build (CMake, primary)

```sh
cmake -S . -B build
cmake --build build -j8
```

Requirements: CMake >= 3.16 and a C99 compiler (Apple clang / GCC). On x86_64
Linux with `nasm` installed, `-DVBENCH_ASM=ON` additionally assembles the x86
asm kernels from `src/asm/x86/*.asm` (off by default, and always off on Apple).

A generated platform header `src/vbench_config.h` (SYS_\*/ARCH_\*/HAVE_\* macros)
is produced by CMake `configure_file` and included first by `src/common.h`.

The legacy `src/Makefile` / `src/compile.sh` still work as an alternative
(C kernels only, portable — no `-march=core-avx2` outside x86):

```sh
cd src && make          # or: ./compile.sh
```

## Run

```sh
./build/vbench --bench              # run all checks + timing table
./build/vbench --bench=sad          # only kernels whose name starts with "sad"
./build/vbench --bench 12345        # fixed random seed (arg after --bench is the seed)
```

`--bench=<pattern>` selects kernels by name prefix. The run prints
`VideoBench: All tests passed` when the correctness checks pass, followed by
the timing table (first column `C` = C kernels).

## Codec kernel packages

Ported codec kernels live in `kernels/<codec>/` (see `PORTING.md`). The root
`CMakeLists.txt` automatically picks up every `kernels/<codec>/` directory that
contains a `CMakeLists.txt`, builds it as a static library, links it into
`vbench`, and defines `VBENCH_HAVE_<CODEC>=1`, which makes
`src/bench_codecs.c` call that codec's `vbench_codec_<codec>()` bench module.
The codec's kernels then show up in the timing table with `<codec>:` name
prefixes — no changes to the harness are needed.

Currently ported: `kernels/x264/` (42 kernels), `kernels/x265/` (40),
`kernels/dav1d/` (27, 8bpc C kernels). The upstream sources they were taken
from are shallow-cloned into `third_party/` (reference only, gitignored);
re-fetch them any time with `./fetch_deps.sh`.
