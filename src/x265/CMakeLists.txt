file(GLOB vb_srcs CONFIGURE_DEPENDS *.cpp)
add_library(kernels_x265 STATIC ${vb_srcs})
target_include_directories(kernels_x265 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_compile_features(kernels_x265 PUBLIC cxx_std_17)
target_link_libraries(vbench PRIVATE kernels_x265)

# the harness platform config (vbench_config.h, in ${CMAKE_SOURCE_DIR}/src)
# provides ARCH_*; mirror the arch -D flags for standalone compiles
set(_x265_arch_defs "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(_x265_arch_defs ARCH_AARCH64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(_x265_arch_defs ARCH_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86")
    set(_x265_arch_defs ARCH_X86=1)
endif()
target_compile_definitions(kernels_x265 PUBLIC ${_x265_arch_defs})

# ---------------------------------------------------------------------------
# PLAN.md steps 4-5: upstream hand-written asm (native x265_ / x265:: symbols),
# built self-contained in this directory and linked next to the C++ libs.
#   aarch64      : aarch64/*.S assembled by the system compiler (the .S files
#                  get X265_NS via -D and handle the Mach-O underscore prefix
#                  themselves in asm.S) plus the aarch64 C++ asm-init helpers
#                  (asm-primitives.cpp + the NEON *-prim.cpp files).
#   x86_64 Linux : x86/*.asm with nasm (-f elf64, -DX265_NS=x265, -I x86/) plus
#                  x86/asm-primitives.cpp.
# The SVE/dotprod/i8mm NEON-variant sources are not compiled (the dispatch in
# asm-primitives.cpp only enables them under HAVE_SVE*/HAVE_NEON_*, which
# stay undefined), so only plain NEON/SSE asm is registered.
# When the asm lib is built, ENABLE_ASSEMBLY=1 reaches the C++ build so
# primitives.h includes the arch asm decl headers (aarch64/fun-decls.h).
# ---------------------------------------------------------------------------
set(_x265_asm_built 0)
if(VBENCH_ASM)
    if(VBENCH_ARCH_AARCH64)
        enable_language(ASM)
        file(GLOB _x265_S CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/aarch64/*.S)
        set(_x265_asm_cpp "")
        foreach(_f asm-primitives.cpp arm64-utils.cpp pixel-prim.cpp dct-prim.cpp
                    intrapred-prim.cpp loopfilter-prim.cpp sao-prim.cpp filter-prim.cpp)
            list(APPEND _x265_asm_cpp ${CMAKE_CURRENT_SOURCE_DIR}/aarch64/${_f})
        endforeach()
        add_library(kernels_x265_asm STATIC ${_x265_S} ${_x265_asm_cpp})
        target_include_directories(kernels_x265_asm PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/aarch64
            ${CMAKE_SOURCE_DIR}/src)
        target_compile_features(kernels_x265_asm PRIVATE cxx_std_17)
        target_compile_definitions(kernels_x265_asm PRIVATE
            ENABLE_ASSEMBLY=1 HAVE_NEON=1 X265_NS=x265 HIGH_BIT_DEPTH=0 BIT_DEPTH=8
            ${_x265_arch_defs})
        if(APPLE)
            target_compile_options(kernels_x265_asm PRIVATE -DPIC) # Mach-O: no text relocations
        endif()
        target_link_libraries(vbench PRIVATE kernels_x265_asm)
        set(_x265_asm_built 1)
    elseif(VBENCH_ARCH_X86_64 AND NOT APPLE)
        if(NOT NASM_EXECUTABLE)
            message(WARNING "VBENCH_ASM=ON on x86_64 requires nasm; building x265 C kernels only")
        else()
            set(_asm_dir ${CMAKE_CURRENT_BINARY_DIR}/asm-x86)
            file(MAKE_DIRECTORY ${_asm_dir})
            file(GLOB _x265_asm_srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/x86/*.asm)
            set(_x265_objs "")
            foreach(_s ${_x265_asm_srcs})
                get_filename_component(_n ${_s} NAME_WE)
                set(_o ${_asm_dir}/${_n}.o)
                add_custom_command(OUTPUT ${_o}
                    COMMAND ${NASM_EXECUTABLE} -f elf64
                        -I${CMAKE_CURRENT_SOURCE_DIR}/x86/
                        -DARCH_X86_64=1 -DX265_NS=x265 -DBIT_DEPTH=8 -DHIGH_BIT_DEPTH=0
                        -DSTACK_ALIGNMENT=32 -Worphan-labels
                        -o ${_o} ${_s}
                    DEPENDS ${_s}
                    COMMENT "nasm x265 ${_n}.asm")
                list(APPEND _x265_objs ${_o})
            endforeach()
            add_library(kernels_x265_asm STATIC
                ${_x265_objs}
                ${CMAKE_CURRENT_SOURCE_DIR}/x86/asm-primitives.cpp)
            target_include_directories(kernels_x265_asm PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/x86
                ${CMAKE_SOURCE_DIR}/src)
            target_compile_features(kernels_x265_asm PRIVATE cxx_std_17)
            target_compile_definitions(kernels_x265_asm PRIVATE
                ENABLE_ASSEMBLY=1 HIGH_BIT_DEPTH=0 ${_x265_arch_defs})
            target_link_libraries(vbench PRIVATE kernels_x265_asm)
            set(_x265_asm_built 1)
        endif()
    endif()
endif()
if(_x265_asm_built)
    target_compile_definitions(kernels_x265 PRIVATE ENABLE_ASSEMBLY=1)
endif()

# PLAN.md step 3: build the same sources a second time with auto-vectorization
# disabled and the C++ namespace renamed x265 -> x265s (via X265_NS), feeding
# the "C-plain" column. The bench module itself (bench_x265.cpp) stays in the
# vec lib only, and so do the upstream asm dirs (x86/, aarch64/) — they belong
# to the asm lib above, not to the scalar rebuild.
file(GLOB_RECURSE _x265_asm_mirror CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/x86/* ${CMAKE_CURRENT_SOURCE_DIR}/aarch64/*)
set(_x265_asm_exclude "")
foreach(_f ${_x265_asm_mirror})
    file(RELATIVE_PATH _rel ${CMAKE_CURRENT_SOURCE_DIR} ${_f})
    list(APPEND _x265_asm_exclude ${_rel})
endforeach()
include(${CMAKE_SOURCE_DIR}/cmake/vbench_codec_dual.cmake)
vbench_codec_scalar(x265
    VEC_TARGET kernels_x265
    SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    PATTERNS *.cpp *.h
    EXCLUDE bench_x265.cpp ${_x265_asm_exclude}
    RENAME "x265::" "x265s::"
           "x265_" "x265s_"
           "namespace x265" "namespace x265s"
           "#define X265_NS x265" "#define X265_NS x265s"
    DEFINITIONS ${_x265_arch_defs}
)
