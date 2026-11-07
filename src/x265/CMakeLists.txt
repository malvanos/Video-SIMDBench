file(GLOB vb_srcs CONFIGURE_DEPENDS *.cpp)
add_library(kernels_x265 STATIC ${vb_srcs})
target_include_directories(kernels_x265 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_compile_features(kernels_x265 PUBLIC cxx_std_17)
target_link_libraries(vbench PRIVATE kernels_x265)

# the harness osdep.h needs an ARCH_* define; provide it on Apple Silicon
set(_x265_arch_defs "")
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(_x265_arch_defs ARCH_AARCH64=1)
    target_compile_definitions(kernels_x265 PUBLIC ${_x265_arch_defs})
endif()

# PLAN.md step 3: build the same sources a second time with auto-vectorization
# disabled and the C++ namespace renamed x265 -> x265s (via X265_NS), feeding
# the "C-plain" column. The bench module itself (bench_x265.cpp) stays in the
# vec lib only.
include(${CMAKE_SOURCE_DIR}/cmake/vbench_codec_dual.cmake)
vbench_codec_scalar(x265
    VEC_TARGET kernels_x265
    SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    PATTERNS *.cpp *.h
    EXCLUDE bench_x265.cpp
    RENAME "x265::" "x265s::"
           "x265_" "x265s_"
           "namespace x265" "namespace x265s"
           "#define X265_NS x265" "#define X265_NS x265s"
    DEFINITIONS ${_x265_arch_defs}
)
