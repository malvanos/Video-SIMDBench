# PLAN — unify codec kernels under `src/`, three-way benchmark columns

## Goal

Move the embedded codec kernel packages (`kernels/x264/`, `kernels/x265/`,
`kernels/dav1d/`) into `src/<codec>/`, sync them from upstream with per-codec
agent scripts, and make every benchmarked kernel show three measurements:

| column      | meaning                                                        | source                          |
|-------------|---------------------------------------------------------------|---------------------------------|
| `C-plain`   | plain C, auto-vectorization disabled                          | same sources, `-fno-tree-vectorize -fno-slp-vectorize` |
| `C`         | auto-vectorized C (compiler SIMD)                             | same sources, `-O3`              |
| `NEON`/`SSE2`/… | hand-crafted assembly                                   | upstream asm synced together with the C |

## Why

- `src/c_kernels/` + `src/asm/` (2016 x264 pair) and `kernels/x264/`
  (2025 upstream x264) are two vintages of the same code — duplication.
- The 2016 asm cannot be wired to the 2025 C: it is ABI-coupled to 2016 struct
  layouts (`src/asm/aarch64/asm-offsets.c` compile-time-checks offsets against
  `vbench_cabac_t`; upstream's `vbx264_cabac_t` already differs, e.g.
  `ALIGNED_16` vs `ALIGNED_64` on `f8_bits_encoded`). Upstream, however, keeps its
  own C and asm ABI-matched by construction — so sync C **and** asm together and
  the coupling problem disappears.
- Once the 2016 pair is replaced by `src/x264/`, no symbol clash remains
  (`x264_` / `dav1d_` / `x265::`), so the `vbx264_` renaming can be dropped and
  upstream sync becomes a plain file copy (no sed), keeping diffs against
  upstream clean.

## Target layout

```
src/
├── main.c bench.c bench.h bench_codecs.c common.h cpu.c osdep.h vbench_config.h.in   # harness only
├── x264/    # upstream x264: common/*.c + common/{x86,aarch64,arm}/ + bench_x264.c + LICENSE + local shim
├── x265/    # upstream x265: source/common/*.cpp|h + asm + bench_x265.cpp + COPYING + shim
├── dav1d/   # upstream dav1d: src/ (8bpc) + src/{x86,arm}/ + bench_dav1d.c + COPYING + shim
└── tools/sync-<codec>.sh   # mechanical upstream-sync script run by agents
```

Deleted: `src/c_kernels/`, `src/asm/`, `kernels/`, and the per-category harness
bench modules (`bench_pixel.c`, `bench_dct.c`, `bench_mc.c`, `bench_intra.c`,
`bench_deblock.c`, `bench_quant.c`, `bench_cabac.c`, `bench_bitstream.c`) —
replaced by per-codec `src/<codec>/bench_<codec>.c` driving `x264_*` /
`x265::` / `dav1d_*` tables.

## Three-column mechanism

The harness already keys benchmark versions by a CPU-flag word
(`src/bench.h:268-269`: `call_c2` → `cpu=0` = C column, `call_a2` → `cpu_new`
= current ISA). Add plain C as a pseudo-flag:

1. `common.h`: `#define VSIMD_CPU_PLAIN_C 0x8000000` (free: x86 modifier bits
   top out at `0x4000000`).
2. `bench.h`: `call_cp`/`call_cp2` macros identical to `call_c`/`call_c2` but
   registering under `VSIMD_CPU_PLAIN_C`.
3. `main.c` `print_bench()`: add `{ VSIMD_CPU_PLAIN_C, "C-plain" }` to
   `coldef` (index 18, `NCOLS=19`), always emit the column right after `C`, and
   map `b->cpu & VSIMD_CPU_PLAIN_C` to it in the per-version column selection.
4. Each codec builds **twice from the same sources**: vec (`-O3` → `C` column)
   and scalar (`-O3 -fno-tree-vectorize -fno-slp-vectorize` → `C-plain`), the
   scalar build symbol-renamed at build time (sed-copy into the build dir or
   `objcopy --prefix-symbols`) so the repo holds a single copy.
5. Each `bench_<codec>.c` registers per kernel: `call_cp(...)` (plain),
   `call_c(...)` (auto-vec), `call_a(...)` (hand-crafted, when the table has
   asm entries for the current ISA pass). All three results are compared for
   correctness on every ISA pass, not just timed.

## Build changes

- Root `CMakeLists.txt`: discover codecs via `src/*/CMakeLists.txt` instead of
  `kernels/*/`; glob only `src/*.c` (non-recursive) into `vbench`.
- Per-codec `src/<codec>/CMakeLists.txt`: build `<codec>_vec` + `<codec>_scalar`
  static libs + `<codec>_asm` (nasm for `x86/*.asm` on x86_64 Linux, `.S` via the
  system compiler on aarch64, reusing the existing `asm-gen/config.h` shim
  pattern from the current root CMake) and link all into `vbench`.
- Keep `VBENCH_HAVE_<CODEC>` guards in `src/bench_codecs.c` unchanged.

## Upstream sync workflow (for the spawned agents)

Documented in `PORTING.md` ("Syncing" section), implemented as
`src/tools/sync-<codec>.sh`:

1. Clone upstream at the chosen tag into `/tmp` (reference only, never in-repo).
2. Script copies the DSP-relevant dirs **C and asm together**: x264 `common/`
   (+ `common/x86`, `common/aarch64`, `common/arm`), x265 `source/common/`,
   dav1d `src/` 8bpc (+ `src/x86`, `src/arm`), plus upstream LICENSE/COPYING.
3. Script never overwrites local files (shim headers, `bench_<codec>.c`,
   `CMakeLists.txt`, `config.h` shims).
4. Acceptance: `cmake --build build -j8 && ./build/vbench --bench=<codec>:` must
   print the codec's rows with `C-plain`, `C` and ISA columns populated, and
   all checks must pass. The asm-offsets compile-time check fails the build if C
   and asm ever drift apart — that is the safety net.

## Status — implemented (master @ b92749e)

All steps done, verified on macOS arm64 (Apple M2 Pro, clang 21) with a clean
build + `tools/verify.sh` (exit 0, `VideoBench: All tests passed`, 0 FAILED):

1. Done — `kernels/*` → `src/<codec>/`; native upstream symbol names; the 2016
   pair (`src/c_kernels/`, `src/asm/`, per-category `bench_*.c`, legacy
   Makefile/compile.sh) deleted; harness now runs codec modules only.
2. Done — upstream x264 asm (`common/{x86,aarch64}`) synced into `src/x264/`,
   built by `cmake/vbench_asm.cmake`; x264 rows show NEON cells.
3. Done — `C-plain` column (always printed after `C`) + dual vec/scalar build
   per codec via `cmake/vbench_codec_dual.cmake`; scalar results compared to vec.
4. Done — x265 asm (`source/common/{x86,aarch64}`) wired; on aarch64 the asm
   covers the pixel/quant family (x265 4.2 moved mc/dct/intra to NEON
   intrinsics in the C sources, so those rows have no separate asm kernel).
5. Done — dav1d 8bpc asm (`src/{x86,arm/64}`) wired under `src/dav1d/src/`.
6. Done — `src/tools/sync-{x264,x265,dav1d}.sh` + PORTING.md "Syncing with
   upstream" recipe; `tools/verify.sh` regression script at repo root.

Sample (arm64): `dav1d: mc put_8tap_regular 32x32 : C=209 C-plain=1200 NEON=73`.

Remaining caveats: the x86_64/nasm asm paths are wired but unexercised here
(no nasm on macOS) — check on a Linux x86_64 box; 32-bit arm and 16bpc asm are
synced but not compiled; x265 SVE/dotprod/i8mm primitives are not compiled.

## Risks

- **dav1d asm** is the heaviest: bitdepth-templated, needs meson-generated
  `config.h` shims (pattern already exists in `kernels/dav1d/config.h`).
- **x265 aarch64 asm / x86 nasm** add per-arch toolchain requirements.
- Build time roughly doubles (vec + scalar per codec).
- If C and asm are ever synced separately, the asm-offsets check must fail the
  build — keep it enabled, never bypass it.

## Open decisions — resolved

- Column headers are `C-plain` / `C` / ISA names.
- Scalar builds use `-O3 -fno-tree-vectorize -fno-slp-vectorize` (plus
  `-fno-vectorize` on Clang), so vectorization is the only difference vs `C`.
