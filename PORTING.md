# Porting contract — how to add codec kernels to VideoBench

This file is the shared contract between the build-modernization agent and the
codec-porting agents (x264 / x265 / dav1d). Everyone codes against exactly this
API. Do not change it; if something is missing, work around it inside your own
directory.

## Layout

```
Video-SIMDBench/
├── CMakeLists.txt            # root build (agent A owns)
├── PORTING.md                # this file
├── src/                      # harness + codec kernel packages
│   ├── *.c *.h               # the benchmark harness itself (agent A owns)
│   ├── x264/                 # upstream x264 kernel package   (synced by tools/sync-x264.sh)
│   ├── x265/                 # upstream x265 kernel package   (synced by tools/sync-x265.sh)
│   ├── dav1d/                # upstream dav1d kernel package  (synced by tools/sync-dav1d.sh)
│   │   ├── CMakeLists.txt
│   │   ├── <ported upstream sources, symbols prefixed vbdav1d_...>
│   │   └── bench_dav1d.c     # implements int vbench_codec_dav1d(int cpu_ref, int cpu_new)
│   └── tools/
│       └── sync-<codec>.sh   # mechanical upstream-sync scripts — see "Syncing with upstream"
└── LICENSE
```

Each codec package `src/<codec>/` holds the copied upstream kernel sources
(symbols prefixed, e.g. `x264_` -> `vbx264_`, `dav1d_` -> `vbdav1d_`; x265 is
C++ in `namespace x265`, so no rename is needed there) together with its bench
module, its upstream LICENSE/COPYING, and a small local shim that maps the
copied files onto the harness (`vb_shim.h` for x264, `x265shim.h` +
`x265_shim.cpp` for x265, `config.h` + the `*_8bpc.c` bitdepth wrappers for
dav1d).

The tree is self-contained: codec sources are copied into `src/<codec>/`,
nothing is cloned or fetched at build time. Upstream repos live only in `/tmp`
(e.g. `/tmp/upstream-x264`) as a reference, and are only ever cloned/updated
by the `src/tools/sync-<codec>.sh` scripts — see "Syncing with upstream" below.

## Harness API available to codec bench modules (implemented in src/bench.h)

```c
// globals (defined once in src/bench.c / src/main.c, declared extern in bench.h)
extern char func_name[100];
extern bench_func_t benchs[MAX_FUNCS];
extern int bench_pattern_len;
extern const char *bench_pattern;
extern uint8_t *buf1, *buf2, *buf3, *buf4;
extern pixel *pbuf1, *pbuf2, *pbuf3, *pbuf4;

#define set_func_name(...) snprintf(func_name, sizeof(func_name), __VA_ARGS__)
bench_t *get_bench( const char *name, int cpu );

// bench.h already provides (uses the above):
#define call_c(func,...)  ({ call_c2(func,__VA_ARGS__); call_c1(func,__VA_ARGS__); })
// call_c2 -> call_bench(func, 0, ...) records timing under the "C" column
// call_c1  -> calls func once (keep args valid!)

int check_codec_kernels( int cpu_ref, int cpu_new );   // in src/bench_codecs.c
// codec modules implement:
int vbench_codec_x264 ( int cpu_ref, int cpu_new );
int vbench_codec_x265 ( int cpu_ref, int cpu_new );
int vbench_codec_dav1d( int cpu_ref, int cpu_new );
```

Typical codec bench module:

```c
#include "common.h"
#include "osdep.h"
#include "bench.h"

int vbench_codec_x264( int cpu_ref, int cpu_new )
{
    (void)cpu_ref; (void)cpu_new;
    set_func_name( "x264: sad 16x16" );
    call_c( vbx264_pixel_sad_16x16_c, pbuf1, 16, pbuf2, 16 );
    ...
    return 0;
}
```

Buffers: `pbuf1`/`pbuf2` are `0x1e00` bytes of random pixel data (read-only
inputs), `pbuf3`/`pbuf4` are scratch outputs. `buf1..buf4` are uint8_t
versions. Strides used by the existing benches: `FENC_STRIDE 16`,
`FDEC_STRIDE 32` (both defined in common.h).

## Build wiring

Root `CMakeLists.txt` (agent A) does, after defining the `vbench` executable:

```cmake
project(vbench C CXX)
file(GLOB _codec_dirs LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
foreach(d ${_codec_dirs})
    if(EXISTS ${d}/CMakeLists.txt)
        get_filename_component(codec ${d} NAME)
        string(TOUPPER ${codec} CODEC)
        add_subdirectory(${d})
        target_compile_definitions(vbench PRIVATE VBENCH_HAVE_${CODEC}=1)
    endif()
endforeach()
```

Each `src/<codec>/CMakeLists.txt` must:

```cmake
file(GLOB vb_srcs CONFIGURE_DEPENDS *.c *.cpp)
add_library(<codec> STATIC ${vb_srcs})
target_include_directories(<codec> PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vbench PRIVATE <codec>)
# add codec-specific defines/includes/features here if needed
```

`src/bench_codecs.c` (agent A) calls the codec modules behind
`#if defined(VBENCH_HAVE_<CODEC>)` guards, so a codec only links when its
`src/<codec>/` directory exists.

## Porting rules (keep it simple — do not over-engineer)

1. Put the upstream C/C++ kernel sources you need into `src/<codec>/`
   (self-contained; no external deps). Prefer running the sync script over
   copying by hand — see the next section.
3. Mechanically prefix every *global* symbol so nothing clashes with the
   existing harness or other codecs: e.g. `x264_` -> `vbx264_`,
   `dav1d_` -> `vbdav1d_`. For x265 (C++, `namespace x265`) no rename is
   needed — call `x265::...` from a `.cpp` bench module. Use `sed` scripts
   over the copied files; keep upstream LICENSE/NOTICE files in the dir.
4. Add the smallest possible shim header(s) inside `src/<codec>/` to make
   the copied files compile against `src/common.h` types (`pixel`, `dctcoef`,
   ...) — do NOT pull in whole upstream build systems.
5. `bench_<codec>` registers a *representative* kernel set (not everything):
   sad/ssd/satd, copy, mc (hpel/qpel), dct/idct, quant/dequant, intra
   predict, loopfilter/deblock, entropy coder — a handful of sizes each.
   Use `set_func_name("<codec>: <kernel> <size>")` + `call_c(...)`.
6. Verify: compile your kernels (`clang/gcc -c -I../../src ...`) and, if the
   root build already exists, run a full `cmake --build` and
   `./build/vbench --bench` and confirm your kernels appear in the table.
   Keep any throwaway test mains OUTSIDE the repo (e.g. /tmp).

## Syncing with upstream

The codec packages in `src/<codec>/` are copies of upstream code (x264, x265,
dav1d). To move a package to a newer upstream ref, run its mechanical sync
script — never copy files by hand, and never edit a synced file by hand:

```sh
src/tools/sync-x264.sh  [ref] [--dry-run]   # code.videolan.org/videolan/x264   (default ref: stable)
src/tools/sync-x265.sh  [ref] [--dry-run]   # bitbucket.org/multicoreware/x265_git (default ref: 4.2)
src/tools/sync-dav1d.sh [ref] [--dry-run]   # code.videolan.org/videolan/dav1d  (default ref: 1.5.4)
```

Each script:

1. clones (or updates) the upstream repo into `/tmp/upstream-<codec>`
   (reference only, never inside this repo) at the chosen ref;
2. copies the DSP-relevant C/C++ sources **and the asm dirs together** into
   `src/<codec>/` — C and asm must always move together; the asm-offsets
   compile-time check is the safety net that fails the build if they ever
   drift apart;
3. copies the upstream LICENSE/COPYING into `src/<codec>/`;
4. applies the same mechanical transform the port was created with (symbol
   prefix + shim-include rewrite), and NEVER overwrites local-only files:
   `bench_<codec>.*`, `CMakeLists.txt`, the shim headers (`vb_shim.h`,
   `x265shim.h`, `x265_shim.cpp`, dav1d's `config.h` and `*_8bpc.c`
   wrappers), x265's local `cpu.h`/`threading.h` shims — anything under
   `src/<codec>/` that is not in the script's copy list is left untouched,
   and nothing is ever deleted.

Agent workflow after running a sync:

1. Review what changed: `git status --short -- src/<codec>`.
2. Never edit synced files by hand — if the fresh upstream code needs changes
   to compile or behave in the harness, make them in the shim instead
   (`vb_shim.h` / `x265shim.h` / dav1d's `config.h` + `*_8bpc.c` wrappers).
3. Acceptance — from the repo root:

   ```sh
   cmake --build build -j8 && ./build/vbench --bench=<codec>:
   ```

   The codec's rows must appear with the `C` column populated (and the ISA
   columns once the asm is wired in), and all checks must pass. If the build
   fails in an asm-offsets check, the C and asm were not synced together —
   re-run the sync script, do not patch the offsets by hand.
