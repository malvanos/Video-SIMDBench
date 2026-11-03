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
├── src/                      # existing harness + bench modules (agent A owns)
│   ├── bench_codecs.c        # new: dispatches to codec bench modules (agent A)
│   └── ...
├── third_party/<codec>/      # shallow clone of upstream (each codec agent owns its own)
└── kernels/<codec>/          # ported kernels + bench module (each codec agent owns its own)
    ├── CMakeLists.txt
    ├── <ported upstream sources, symbols prefixed vb<codec>_...>
    └── bench_<codec>.c       # implements int vbench_codec_<codec>(int cpu_ref, int cpu_new)
```

File ownership (do not edit files outside your list — other agents own them):

- Agent A (build): root `CMakeLists.txt`, everything in `src/`, `README.md`, `.gitignore`.
- Agent B (x264): `third_party/x264/`, `kernels/x264/`.
- Agent C (x265): `third_party/x265/`, `kernels/x265/`.
- Agent D (dav1d): `third_party/dav1d/`, `kernels/dav1d/`.

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
file(GLOB _codec_dirs LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/kernels/*)
foreach(d ${_codec_dirs})
    if(EXISTS ${d}/CMakeLists.txt)
        get_filename_component(codec ${d} NAME)
        string(TOUPPER ${codec} CODEC)
        add_subdirectory(${d})
        target_compile_definitions(vbench PRIVATE VBENCH_HAVE_${CODEC}=1)
    endif()
endforeach()
```

Each `kernels/<codec>/CMakeLists.txt` must:

```cmake
file(GLOB vb_srcs CONFIGURE_DEPENDS *.c *.cpp)
add_library(kernels_<codec> STATIC ${vb_srcs})
target_include_directories(kernels_<codec> PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vbench PRIVATE kernels_<codec>)
# add codec-specific defines/includes/features here if needed
```

`src/bench_codecs.c` (agent A) calls the codec modules behind
`#if defined(VBENCH_HAVE_<CODEC>)` guards, so a codec only links when its
`kernels/<codec>/` directory exists.

## Porting rules (keep it simple — do not over-engineer)

1. Shallow-clone the upstream repo into `third_party/<codec>/` (reference).
2. Copy the upstream C/C++ kernel sources you need into `kernels/<codec>/`.
3. Mechanically prefix every *global* symbol so nothing clashes with the
   existing harness or other codecs: e.g. `x264_` -> `vbx264_`,
   `dav1d_` -> `vbdav1d_`. For x265 (C++, `namespace x265`) no rename is
   needed — call `x265::...` from a `.cpp` bench module. Use `sed` scripts
   over the copied files; keep upstream LICENSE/NOTICE files in the dir.
4. Add the smallest possible shim header(s) inside `kernels/<codec>/` to make
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
