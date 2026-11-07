file(GLOB vb_srcs CONFIGURE_DEPENDS *.c *.cpp)
add_library(kernels_x264 STATIC ${vb_srcs})
target_include_directories(kernels_x264 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vbench PRIVATE kernels_x264)
# x264 port: pure-C kernels, no asm; the harness arch defines are forced off
# inside vb_shim.h, so no extra defines are needed here.

# PLAN.md step 3: build the same sources a second time with auto-vectorization
# disabled and the symbols renamed vbx264_ -> vbx264s_, feeding the "C-plain"
# column. The bench module itself (bench_x264.c) stays in the vec lib only.
include(${CMAKE_SOURCE_DIR}/cmake/vbench_codec_dual.cmake)
vbench_codec_scalar(x264
    VEC_TARGET kernels_x264
    SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    PATTERNS *.c *.h
    EXCLUDE bench_x264.c
    RENAME vbx264_ vbx264s_
)
