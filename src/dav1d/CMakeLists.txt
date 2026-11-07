# dav1d kernel port (agent D). See PORTING.md.
file(GLOB vb_srcs CONFIGURE_DEPENDS *.c)
add_library(kernels_dav1d STATIC ${vb_srcs})
target_include_directories(kernels_dav1d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vbench PRIVATE kernels_dav1d)

# harness arch defines so this module also compiles standalone
# (config.h in this dir defaults ARCH_* to 0 and defers to these)
set(_dav1d_arch_defs "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(_dav1d_arch_defs ARCH_AARCH64=1 STACK_ALIGNMENT=16)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(_dav1d_arch_defs ARCH_X86_64=1 STACK_ALIGNMENT=32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86")
    set(_dav1d_arch_defs ARCH_X86=1 STACK_ALIGNMENT=32)
endif()
target_compile_definitions(kernels_dav1d PRIVATE ${_dav1d_arch_defs})

# PLAN.md step 3: build the same sources a second time (whole tree, headers
# included) with auto-vectorization disabled and the symbols renamed
# vbdav1d_ -> vbdav1ds_, feeding the "C-plain" column. The bench module itself
# (bench_dav1d.c) stays in the vec lib only.
include(${CMAKE_SOURCE_DIR}/cmake/vbench_codec_dual.cmake)
vbench_codec_scalar(dav1d
    VEC_TARGET kernels_dav1d
    SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    PATTERNS *.c *.h *.inc
    EXCLUDE bench_dav1d.c
    RENAME vbdav1d_ vbdav1ds_
    DEFINITIONS ${_dav1d_arch_defs}
)
