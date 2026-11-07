# dav1d kernel port (agent D). See PORTING.md.
file(GLOB vb_srcs CONFIGURE_DEPENDS *.c)
add_library(kernels_dav1d STATIC ${vb_srcs})
target_include_directories(kernels_dav1d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vbench PRIVATE kernels_dav1d)

# harness arch defines so this module also compiles standalone
# (config.h in this dir defers to vbench_config.h / these -D flags)
set(_dav1d_arch_defs "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(_dav1d_arch_defs ARCH_AARCH64=1 STACK_ALIGNMENT=16)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(_dav1d_arch_defs ARCH_X86_64=1 STACK_ALIGNMENT=32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86")
    set(_dav1d_arch_defs ARCH_X86=1 STACK_ALIGNMENT=32)
endif()
target_compile_definitions(kernels_dav1d PRIVATE ${_dav1d_arch_defs})

# ---------------------------------------------------------------------------
# PLAN.md steps 4-5: upstream hand-written asm (native dav1d_ symbol names),
# built self-contained in this directory and linked next to the C libs.
#   aarch64      : src/arm/64/*.S assembled by the system compiler; the .S
#                  files include "config.h" (this dir), which pulls in
#                  vbench_config.h and adds the Mach-O PREFIX/HAVE_AS_FUNC.
#   x86_64 Linux : src/x86/*.asm with nasm, using a generated config.asm shim
#                  (private_prefix dav1d) in the binary dir; "ext/x86/
#                  x86inc.asm" resolves via -I<this dir>.
# The 16bpc (*16*) asm files are skipped: this port is 8bpc only.
# When the asm lib is built, HAVE_ASM=1 reaches the C build so the *_tmpl
# sources include the src/{arm,x86} asm decl headers and the *_dsp_init_8bpc
# functions call the arch init (mc_dsp_init_arm / mc_dsp_init_x86).
# ---------------------------------------------------------------------------
set(_dav1d_asm_built 0)
if(VBENCH_ASM)
    if(VBENCH_ARCH_AARCH64)
        enable_language(ASM)
        file(GLOB _dav1d_S CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/arm/64/*.S)
        list(FILTER _dav1d_S EXCLUDE REGEX "/([^/]*16[^/]*)\\.S$")
        add_library(kernels_dav1d_asm STATIC ${_dav1d_S})
        target_include_directories(kernels_dav1d_asm PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
        target_compile_definitions(kernels_dav1d_asm PRIVATE
            ${_dav1d_arch_defs} BITDEPTH=8 HIGH_BIT_DEPTH=0)
        if(APPLE)
            target_compile_options(kernels_dav1d_asm PRIVATE -DPIC) # Mach-O: no text relocations
        endif()
        target_link_libraries(vbench PRIVATE kernels_dav1d_asm)
        set(_dav1d_asm_built 1)
    elseif(VBENCH_ARCH_X86_64 AND NOT APPLE)
        if(NOT NASM_EXECUTABLE)
            message(WARNING "VBENCH_ASM=ON on x86_64 requires nasm; building dav1d C kernels only")
        else()
            set(_asm_dir ${CMAKE_CURRENT_BINARY_DIR}/asm-x86)
            file(MAKE_DIRECTORY ${_asm_dir})
            # meson-generated config.asm equivalent (nasm syntax)
            file(WRITE ${_asm_dir}/config.asm
                "%define private_prefix dav1d\n%define ARCH_X86_64 1\n%define ARCH_X86_32 0\n%define STACK_ALIGNMENT 32\n%define FORCE_VEX_ENCODING 0\n")
            file(GLOB _dav1d_asm CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/x86/*.asm)
            list(FILTER _dav1d_asm EXCLUDE REGEX "/([^/]*16[^/]*)\\.asm$")
            set(_dav1d_objs "")
            foreach(_s ${_dav1d_asm})
                get_filename_component(_n ${_s} NAME_WE)
                set(_o ${_asm_dir}/${_n}.o)
                add_custom_command(OUTPUT ${_o}
                    COMMAND ${NASM_EXECUTABLE} -f elf64
                        -I${_asm_dir}/ -I${CMAKE_CURRENT_SOURCE_DIR}/
                        -DARCH_X86_64=1 -DBITDEPTH=8 -DHIGH_BIT_DEPTH=0
                        -o ${_o} ${_s}
                    DEPENDS ${_s}
                    COMMENT "nasm dav1d ${_n}.asm")
                list(APPEND _dav1d_objs ${_o})
            endforeach()
            add_library(kernels_dav1d_asm STATIC ${_dav1d_objs})
            target_link_libraries(vbench PRIVATE kernels_dav1d_asm)
            set(_dav1d_asm_built 1)
        endif()
    endif()
endif()
if(_dav1d_asm_built)
    target_compile_definitions(kernels_dav1d PRIVATE HAVE_ASM=1)
endif()

# PLAN.md step 3: build the same sources a second time (whole tree, headers
# included) with auto-vectorization disabled and the symbols renamed
# dav1d_ -> dav1ds_, feeding the "C-plain" column. The bench module itself
# (bench_dav1d.c) stays in the vec lib only, and so do the upstream asm dirs
# (src/arm, src/x86, ext) — they belong to the asm lib above, not to the
# scalar rebuild.
file(GLOB_RECURSE _dav1d_asm_mirror CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arm/* ${CMAKE_CURRENT_SOURCE_DIR}/src/x86/*
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/*)
set(_dav1d_asm_exclude "")
foreach(_f ${_dav1d_asm_mirror})
    file(RELATIVE_PATH _rel ${CMAKE_CURRENT_SOURCE_DIR} ${_f})
    list(APPEND _dav1d_asm_exclude ${_rel})
endforeach()
include(${CMAKE_SOURCE_DIR}/cmake/vbench_codec_dual.cmake)
vbench_codec_scalar(dav1d
    VEC_TARGET kernels_dav1d
    SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    PATTERNS *.c *.h *.inc
    EXCLUDE bench_dav1d.c ${_dav1d_asm_exclude}
    RENAME dav1d_ dav1ds_
    DEFINITIONS ${_dav1d_arch_defs}
)
