file(GLOB vb_srcs CONFIGURE_DEPENDS *.cpp)
add_library(kernels_x265 STATIC ${vb_srcs})
target_include_directories(kernels_x265 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_compile_features(kernels_x265 PUBLIC cxx_std_17)
target_link_libraries(vbench PRIVATE kernels_x265)

# the harness osdep.h needs an ARCH_* define; provide it on Apple Silicon
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    target_compile_definitions(kernels_x265 PUBLIC ARCH_AARCH64=1)
endif()
