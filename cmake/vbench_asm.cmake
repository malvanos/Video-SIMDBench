# cmake/vbench_asm.cmake — build a codec package's synced upstream hand-written
# asm into a static lib and link it into vbench.
#
#   vbench_add_asm(<codec>)
#
# The codec package src/<codec>/ holds the upstream asm dirs synced together
# with its C sources (C and asm must move together; the asm-offsets check is
# the safety net). On aarch64, src/<codec>/aarch64/*.S is assembled with the
# system compiler using a generated config.h shim in the codec binary dir
# (pulls in vbench_config.h, adds the Mach-O PREFIX / HAVE_AS_FUNC specifics
# and the assembler .arch level / extension directives the upstream asm.S
# needs — probed at configure time like upstream's configure does).
# On x86_64 Linux, src/<codec>/x86/*.asm is assembled with nasm using the same
# flags the old root-CMake asm build used. The x86/aarch64 mc-c.c +
# predict-c.c C helpers (the asm function-table init) are compiled into the
# same lib. The lib target is <codec>_asm and is linked into vbench.
#
# Expects from the root CMakeLists: the VBENCH_ASM option, VBENCH_ARCH_* and
# NASM_EXECUTABLE.

function(_vbench_asm_probe src outvar)
    # assemble a snippet with the ASM compiler; result 0 = supported
    file(WRITE ${CMAKE_BINARY_DIR}/vbench-asm-probe.S "${src}")
    execute_process(
        COMMAND ${CMAKE_ASM_COMPILER} -c ${CMAKE_BINARY_DIR}/vbench-asm-probe.S
                -o ${CMAKE_BINARY_DIR}/vbench-asm-probe.o
        RESULT_VARIABLE _res
        OUTPUT_QUIET ERROR_QUIET)
    set(${outvar} ${_res} PARENT_SCOPE)
endfunction()

function(vbench_add_asm codec)
    set(asm_dir ${CMAKE_SOURCE_DIR}/src/${codec})
    if(NOT EXISTS ${asm_dir})
        message(FATAL_ERROR "vbench_add_asm: no asm dir for codec '${codec}' (${asm_dir})")
    endif()

    if(VBENCH_ARCH_AARCH64)
        enable_language(ASM)
        set(gen ${CMAKE_CURRENT_BINARY_DIR}/${codec}-asm-gen)
        file(MAKE_DIRECTORY ${gen})

        # .arch level + .arch_extension directive support, highest first
        # (same probes as upstream x264 configure)
        set(_level "armv8-a+crc")
        foreach(_l armv8.6-a+crc armv8.4-a+crc armv8.2-a+crc armv8-a+crc)
            _vbench_asm_probe(".arch ${_l}\n" _res)
            if(_res EQUAL 0)
                set(_level ${_l})
                break()
            endif()
        endforeach()
        _vbench_asm_probe(".arch armv8-a\n.arch_extension dotprod\nudot v0.4s, v0.16b, v0.16b\n" _dotprod)
        _vbench_asm_probe(".arch armv8-a\n.arch_extension i8mm\nusdot v0.4s, v0.16b, v0.16b\n" _i8mm)
        _vbench_asm_probe(".arch armv8-a\n.arch_extension sve\nptrue p0.b, vl16\n" _sve)
        _vbench_asm_probe(".arch armv8-a\n.arch_extension sve2\nsmlalb z10.s, z2.h, z1.h\n" _sve2)
        foreach(_f dotprod i8mm sve sve2)
            if(_${_f} EQUAL 0)
                set(_${_f} 1)
            else()
                set(_${_f} 0)
            endif()
        endforeach()

        # config.h shim for the .S files (they do #include "config.h")
        file(WRITE ${gen}/config.h
"#include \"vbench_config.h\"
#if SYS_MACOSX
#define PREFIX
#define HAVE_AS_FUNC 0
#else
#define HAVE_AS_FUNC 1
#endif
#define AS_ARCH_LEVEL ${_level}
#define HAVE_AS_ARCHEXT_DOTPROD_DIRECTIVE ${_dotprod}
#define HAVE_AS_ARCHEXT_I8MM_DIRECTIVE ${_i8mm}
#define HAVE_AS_ARCHEXT_SVE_DIRECTIVE ${_sve}
#define HAVE_AS_ARCHEXT_SVE2_DIRECTIVE ${_sve2}
")

        # *.S minus the include-only fragments (asm.S, *-common.S)
        file(GLOB _asm_srcs ${asm_dir}/aarch64/*.S)
        list(REMOVE_ITEM _asm_srcs ${asm_dir}/aarch64/asm.S)
        file(GLOB _common_srcs ${asm_dir}/aarch64/*-common.S)
        list(REMOVE_ITEM _asm_srcs ${_common_srcs})

        add_library(${codec}_asm STATIC
            ${_asm_srcs}
            ${asm_dir}/aarch64/mc-c.c
            ${asm_dir}/aarch64/predict-c.c)
        target_include_directories(${codec}_asm PRIVATE
            ${gen}
            ${CMAKE_SOURCE_DIR}/src
            ${asm_dir}
            ${asm_dir}/aarch64)
        if(APPLE)
            target_compile_options(${codec}_asm PRIVATE -DPIC)  # Mach-O: no text relocations
        endif()
        target_link_libraries(vbench PRIVATE ${codec}_asm)

    elseif(VBENCH_ARCH_X86_64 AND NOT APPLE)
        if(NOT NASM_EXECUTABLE)
            message(FATAL_ERROR "vbench_add_asm(${codec}): VBENCH_ASM=ON on x86_64 requires nasm")
        endif()
        file(GLOB _asm_srcs ${asm_dir}/x86/*.asm)
        set(_asm_objs "")
        foreach(_s ${_asm_srcs})
            get_filename_component(_n ${_s} NAME_WE)
            set(_o ${CMAKE_CURRENT_BINARY_DIR}/${codec}-asm-x86/${_n}.o)
            add_custom_command(OUTPUT ${_o}
                COMMAND ${NASM_EXECUTABLE}
                    -I${asm_dir}/x86 -I${asm_dir}
                    -DARCH_X86_64=1 -f elf64 -Worphan-labels
                    -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8
                    -o ${_o} ${_s}
                DEPENDS ${_s}
                COMMENT "nasm ${_n}.asm")
            list(APPEND _asm_objs ${_o})
        endforeach()
        add_library(${codec}_asm STATIC
            ${_asm_objs}
            ${asm_dir}/x86/mc-c.c
            ${asm_dir}/x86/predict-c.c)
        target_include_directories(${codec}_asm PRIVATE
            ${asm_dir}
            ${asm_dir}/x86
            ${CMAKE_SOURCE_DIR}/src)

    else()
        message(WARNING "vbench_add_asm(${codec}): no asm for this target; skipping")
    endif()
endfunction()
