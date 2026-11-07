# cmake/vbench_codec_dual.cmake — PLAN.md step 3: dual vec/scalar codec build.
#
# vbench_codec_scalar(<codec>
#     VEC_TARGET    <existing vec lib target>            (required)
#     SRC_DIR       <codec source dir>                   (required)
#     PATTERNS      <recursive glob patterns to mirror>  (required) e.g. *.c *.h
#     EXCLUDE       <files, relative to SRC_DIR, not mirrored>  (the bench module)
#     RENAME        <old> <new> [<old> <new> ...]        literal whole-substring
#                   replacements applied to every mirrored file (symbol prefixing)
#     [DEFINITIONS  <compile definitions>...]
#     [INCLUDE_DIRS <extra include dirs>...]
# )
#
# Generates symbol-renamed copies of the codec sources under
# ${CMAKE_BINARY_DIR}/gen/<codec>_scalar/, preserving the relative directory
# layout so quote-includes resolve to the renamed headers. The .c/.cpp copies
# are compiled into a static lib <VEC_TARGET>_scalar with
# "-O3 -fno-tree-vectorize -fno-slp-vectorize" (the "C-plain" column sources),
# which is linked into vbench next to the untouched vec lib. The repo keeps a
# single copy of the codec sources.

function(vbench_codec_scalar codec)
    set(_one VEC_TARGET SRC_DIR)
    set(_multi PATTERNS EXCLUDE RENAME DEFINITIONS INCLUDE_DIRS)
    cmake_parse_arguments(VBDS "" "${_one}" "${_multi}" ${ARGN})

    if(NOT VBDS_VEC_TARGET OR NOT VBDS_SRC_DIR OR NOT VBDS_PATTERNS)
        message(FATAL_ERROR "vbench_codec_scalar: VEC_TARGET, SRC_DIR and PATTERNS are required")
    endif()

    set(gen_dir ${CMAKE_BINARY_DIR}/gen/${codec}_scalar)
    file(MAKE_DIRECTORY ${gen_dir})

    set(all_files "")
    foreach(pat ${VBDS_PATTERNS})
        file(GLOB_RECURSE _f CONFIGURE_DEPENDS ${VBDS_SRC_DIR}/${pat})
        list(APPEND all_files ${_f})
    endforeach()
    list(REMOVE_DUPLICATES all_files)
    foreach(ex ${VBDS_EXCLUDE})
        list(REMOVE_ITEM all_files ${VBDS_SRC_DIR}/${ex})
    endforeach()
    list(SORT all_files)

    set(scalar_srcs "")
    list(LENGTH VBDS_RENAME nren)
    foreach(f ${all_files})
        file(RELATIVE_PATH rel ${VBDS_SRC_DIR} ${f})
        set(out ${gen_dir}/${rel})
        get_filename_component(out_dir ${out} DIRECTORY)
        file(MAKE_DIRECTORY ${out_dir})
        file(READ ${f} content)
        set(i 0)
        while(i LESS nren)
            math(EXPR j "${i} + 1")
            list(GET VBDS_RENAME ${i} old)
            list(GET VBDS_RENAME ${j} new)
            string(REPLACE "${old}" "${new}" content "${content}")
            math(EXPR i "${i} + 2")
        endwhile()
        # only rewrite when the content actually changed (avoids rebuilds)
        set(prev "")
        if(EXISTS ${out})
            file(READ ${out} prev)
        endif()
        if(NOT "${prev}" STREQUAL "${content}")
            file(WRITE ${out} "${content}")
        endif()
        if(rel MATCHES "\\.(c|cpp|cc|cxx)$")
            list(APPEND scalar_srcs ${out})
        endif()
    endforeach()

    set(scalar_target ${VBDS_VEC_TARGET}_scalar)
    add_library(${scalar_target} STATIC ${scalar_srcs})
    # gen dir first: quote-includes must resolve to the renamed headers
    target_include_directories(${scalar_target} PUBLIC
        ${gen_dir} ${CMAKE_SOURCE_DIR}/src ${VBDS_INCLUDE_DIRS})
    set(_novec -O3 -fno-tree-vectorize -fno-slp-vectorize)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # clang spells the loop-vectorizer disable -fno-vectorize; it only
        # warns about the gcc spelling, which is kept for gcc builds
        list(APPEND _novec -fno-vectorize -Wno-ignored-optimization-argument)
    endif()
    target_compile_options(${scalar_target} PRIVATE ${_novec})
    if(VBDS_DEFINITIONS)
        target_compile_definitions(${scalar_target} PRIVATE ${VBDS_DEFINITIONS})
    endif()
    target_link_libraries(vbench PRIVATE ${scalar_target})
endfunction()
