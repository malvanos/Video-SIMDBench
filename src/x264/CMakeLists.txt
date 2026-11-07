file(GLOB vb_srcs CONFIGURE_DEPENDS *.c *.cpp)
add_library(kernels_x264 STATIC ${vb_srcs})
target_include_directories(kernels_x264 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vbench PRIVATE kernels_x264)
# x264 port: native upstream symbol names (x264_*), with the upstream
# hand-written asm synced next to the C (x86/, aarch64/). The asm is built
# into x264_asm and linked into vbench; the C build registers it through the
# real platform macros in vbench_config.h (see vb_shim.h).

# upstream asm (NEON on aarch64, SSE on x86_64 Linux) — cmake/vbench_asm.cmake
include(${CMAKE_SOURCE_DIR}/cmake/vbench_asm.cmake)
if(VBENCH_ASM)
    vbench_add_asm(x264)
endif()

# PLAN.md step 3: build the same sources a second time with auto-vectorization
# disabled and the symbols renamed x264_ -> x264s_, feeding the "C-plain"
# column. The bench module itself (bench_x264.c) stays in the vec lib only, and
# so do the asm-dir C helpers (they are compiled into x264_asm, and the scalar
# build must not register asm: VBENCH_SCALAR_BUILD forces the asm paths off in
# vb_shim.h).
include(${CMAKE_SOURCE_DIR}/cmake/vbench_codec_dual.cmake)
vbench_codec_scalar(x264
    VEC_TARGET kernels_x264
    SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    PATTERNS *.c *.h
    EXCLUDE bench_x264.c
            x86/mc-c.c x86/predict-c.c
            aarch64/mc-c.c aarch64/predict-c.c aarch64/asm-offsets.c
    RENAME x264_ x264s_
    DEFINITIONS VBENCH_SCALAR_BUILD
)
