cmake_minimum_required(VERSION 3.16)
project(vbench C CXX)

# ---------------------------------------------------------------------------
# VideoBench — Video SIMD Benchmark harness (modern CMake build)
#
#   cmake -S . -B build
#   cmake --build build -j8
#   ./build/vbench --bench            (optionally: --bench=<pattern>)
#
# Codec kernel packages under src/<codec>/ are picked up automatically
# (see PORTING.md); each package also builds its synced upstream asm via
# cmake/vbench_asm.cmake (vbench_add_asm).
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
# The asm itself is no longer built here: each codec package syncs its own
# upstream asm (src/<codec>/{x86,aarch64}/) and builds it via
# cmake/vbench_asm.cmake (vbench_add_asm). This block only records which
# asm kinds this build will have, for vbench_config.h (HAVE_MMX is 1 only
# when the x86 asm is actually built; HAVE_NEON comes from the arch below).
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
        set(VBENCH_HAVE_ASM 1)
        set(VBENCH_HAVE_X86_ASM 1)
        set(VBENCH_HAVE_MMX 1)
    elseif(VBENCH_ARCH_AARCH64)
        set(VBENCH_HAVE_ASM 1)
    else()
        message(WARNING "VBENCH_ASM=ON is not supported for this target; building C kernels only")
    endif()
endif()
if(VBENCH_HAVE_ASM AND VBENCH_ARCH_AARCH64)
    # codec asm .S files (src/<codec>/aarch64/) assemble with the system
    # compiler (clang/gcc) — enabled here at directory scope so every
    # src/<codec>/ subdir can add .S sources (see cmake/vbench_asm.cmake)
    enable_language(ASM)
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
# harness sources only: src/*.c (non-recursive) — the codec kernel packages
# under src/<codec>/ are added via their own CMakeLists.txt below
file(GLOB VBENCH_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_executable(vbench ${VBENCH_SOURCES})
target_include_directories(vbench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vbench PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
endif()
target_link_libraries(vbench PRIVATE m)

# --------------------- codec kernel discovery (PORTING.md) ----------------
# Each src/<codec>/ directory with a CMakeLists.txt is a codec package.
file(GLOB _codec_dirs LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
foreach(d ${_codec_dirs})
    if(EXISTS ${d}/CMakeLists.txt)
        get_filename_component(codec ${d} NAME)
        string(TOUPPER ${codec} CODEC)
        add_subdirectory(${d})
        target_compile_definitions(vbench PRIVATE VBENCH_HAVE_${CODEC}=1)
    endif()
endforeach()
