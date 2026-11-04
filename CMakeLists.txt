cmake_minimum_required(VERSION 3.16)
project(vbench C CXX)

# ---------------------------------------------------------------------------
# VideoBench — Video SIMD Benchmark harness (modern CMake build)
#
#   cmake -S . -B build
#   cmake --build build -j8
#   ./build/vbench --bench            (optionally: --bench=<pattern>)
#
# The legacy src/Makefile still works as an alternative build.
# Codec kernel packages under kernels/<codec>/ are picked up automatically
# (see PORTING.md).
# ---------------------------------------------------------------------------

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)          # gnu99
set(CMAKE_CXX_STANDARD 17)           # for x265 C++ kernels
set(CMAKE_CXX_EXTENSIONS ON)

# -O3 as required, keep assert() active (do not add -DNDEBUG)
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --------------------------- platform detection ---------------------------
set(VBENCH_SYS_WINDOWS 0)
set(VBENCH_SYS_LINUX 0)
set(VBENCH_SYS_MACOSX 0)
set(VBENCH_SYS_FREEBSD 0)
set(VBENCH_SYS_OPENBSD 0)
set(VBENCH_SYS_BEOS 0)
set(VBENCH_SYS_CYGWIN 0)
set(VBENCH_SYS_SUNOS 0)
if(WIN32)
    set(VBENCH_SYS_WINDOWS 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(VBENCH_SYS_LINUX 1)
elseif(APPLE)
    set(VBENCH_SYS_MACOSX 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(VBENCH_SYS_FREEBSD 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    set(VBENCH_SYS_OPENBSD 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    set(VBENCH_SYS_SUNOS 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "CYGWIN|MSYS")
    set(VBENCH_SYS_CYGWIN 1)
endif()

set(VBENCH_ARCH_X86 0)
set(VBENCH_ARCH_X86_64 0)
set(VBENCH_ARCH_ARM 0)
set(VBENCH_ARCH_AARCH64 0)
set(VBENCH_ARCH_PPC 0)
set(VBENCH_ARCH_MIPS 0)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _proc)
if(_proc MATCHES "x86_64|amd64")
    set(VBENCH_ARCH_X86_64 1)
elseif(_proc MATCHES "i[3-6]86|x86")
    set(VBENCH_ARCH_X86 1)
elseif(_proc MATCHES "aarch64|arm64")
    set(VBENCH_ARCH_AARCH64 1)
elseif(_proc MATCHES "^arm")
    set(VBENCH_ARCH_ARM 1)
elseif(_proc MATCHES "powerpc|ppc")
    set(VBENCH_ARCH_PPC 1)
elseif(_proc MATCHES "mips")
    set(VBENCH_ARCH_MIPS 1)
else()
    message(FATAL_ERROR "vbench: unsupported processor '${CMAKE_SYSTEM_PROCESSOR}'")
endif()

# --------------------------- optional asm kernels -------------------------
# Default OFF on Apple and whenever no suitable assembler is available
# (x86 needs nasm; the sources are .asm). Enable with -DVBENCH_ASM=ON.
find_program(NASM_EXECUTABLE nasm)
set(_asm_default OFF)
if(NOT APPLE AND VBENCH_ARCH_X86_64 AND NASM_EXECUTABLE)
    set(_asm_default ON)
endif()
if(VBENCH_ARCH_AARCH64)
    set(_asm_default ON)   # .S files assemble with the system compiler (clang/gcc)
endif()
option(VBENCH_ASM "Build architecture-specific assembly kernels" ${_asm_default})

set(VBENCH_HAVE_ASM 0)
set(VBENCH_HAVE_X86_ASM 0)
set(VBENCH_HAVE_MMX 0)
if(VBENCH_ASM)
    if(VBENCH_ARCH_X86_64 AND NOT APPLE)
        if(NOT NASM_EXECUTABLE)
            message(FATAL_ERROR "VBENCH_ASM=ON on x86_64 requires nasm")
        endif()
        file(GLOB _asm_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/x86/*.asm)
        set(_asm_objs "")
        foreach(_s ${_asm_srcs})
            get_filename_component(_n ${_s} NAME_WE)
            set(_o ${CMAKE_CURRENT_BINARY_DIR}/asm-x86/${_n}.o)
            add_custom_command(OUTPUT ${_o}
                COMMAND ${NASM_EXECUTABLE}
                    -I${CMAKE_CURRENT_SOURCE_DIR}/src/
                    -DARCH_X86_64=1 -f elf64 -Worphan-labels
                    -DSTACK_ALIGNMENT=32 -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8
                    -o ${_o} ${_s}
                DEPENDS ${_s}
                COMMENT "nasm ${_n}.asm")
            list(APPEND _asm_objs ${_o})
        endforeach()
        add_library(vbench_asm STATIC
            ${_asm_objs}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/x86/predict-c.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/x86/mc-c.c)
        target_include_directories(vbench_asm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        set(VBENCH_HAVE_ASM 1)
        set(VBENCH_HAVE_X86_ASM 1)
        set(VBENCH_HAVE_MMX 1)
    elseif(VBENCH_ARCH_AARCH64)
        enable_language(ASM)
        # the .S files include "config.h"; generate a shim that pulls in the
        # platform config and adds the Mach-O specifics (symbol underscore
        # prefix, no .func directive)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/asm-gen)
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/asm-gen/config.h
            "#include \"vbench_config.h\"\n#if SYS_MACOSX\n#define PREFIX\n#define HAVE_AS_FUNC 0\n#else\n#define HAVE_AS_FUNC 1\n#endif\n")
        file(GLOB _asm_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/aarch64/*.S)
        add_library(vbench_asm STATIC
            ${_asm_srcs}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/aarch64/mc-c.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/aarch64/predict-c.c)
        target_include_directories(vbench_asm PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/asm-gen
            ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/asm)
        if(APPLE)
            target_compile_options(vbench_asm PRIVATE -DPIC)  # Mach-O: no text relocations
        endif()
        set(VBENCH_HAVE_ASM 1)
    else()
        message(WARNING "VBENCH_ASM=ON is not supported for this target; building C kernels only")
    endif()
endif()

# --------------------------- feature macros -------------------------------
set(VBENCH_HAVE_NEON 0)
set(VBENCH_HAVE_ARMV6 0)
if(VBENCH_ARCH_ARM OR VBENCH_ARCH_AARCH64)
    set(VBENCH_HAVE_NEON 1)
endif()
if(VBENCH_ARCH_ARM)
    set(VBENCH_HAVE_ARMV6 1)
endif()
set(VBENCH_HAVE_ALTIVEC 0)
set(VBENCH_HAVE_MSA 0)
set(VBENCH_HAVE_THREAD 0)
set(VBENCH_HAVE_POSIXTHREAD 0)
set(VBENCH_HAVE_CPU_COUNT 0)
if(NOT WIN32)
    set(VBENCH_HAVE_THREAD 1)
endif()
if(CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin|FreeBSD|OpenBSD")
    set(VBENCH_HAVE_POSIXTHREAD 1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(VBENCH_HAVE_CPU_COUNT 1)
endif()
set(VBENCH_BIT_DEPTH 8)
set(VBENCH_HIGH_BIT_DEPTH 0)
if(VBENCH_ARCH_X86_64 AND NOT APPLE AND VBENCH_HAVE_X86_ASM)
    set(VBENCH_STACK_ALIGNMENT 32)
else()
    set(VBENCH_STACK_ALIGNMENT 16)
endif()

# Generated platform config header, included first by common.h / osdep.h.
# It lives in src/ so that kernels/<codec>/ targets (which only add
# ${CMAKE_SOURCE_DIR}/src to their include path, see PORTING.md) find it.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vbench_config.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vbench_config.h
    @ONLY)

# ------------------------------- vbench exe -------------------------------
file(GLOB VBENCH_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/c_kernels/*.c)

add_executable(vbench ${VBENCH_SOURCES})
target_include_directories(vbench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm
    ${CMAKE_CURRENT_BINARY_DIR})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vbench PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
endif()
target_link_libraries(vbench PRIVATE m)
if(TARGET vbench_asm)
    target_link_libraries(vbench PRIVATE vbench_asm)
endif()

# --------------------- codec kernel discovery (PORTING.md) ----------------
file(GLOB _codec_dirs LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/kernels/*)
foreach(d ${_codec_dirs})
    if(EXISTS ${d}/CMakeLists.txt)
        get_filename_component(codec ${d} NAME)
        string(TOUPPER ${codec} CODEC)
        add_subdirectory(${d})
        target_compile_definitions(vbench PRIVATE VBENCH_HAVE_${CODEC}=1)
    endif()
endforeach()
