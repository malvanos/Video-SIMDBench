file(GLOB vb_srcs CONFIGURE_DEPENDS *.c *.cpp)
add_library(kernels_x264 STATIC ${vb_srcs})
target_include_directories(kernels_x264 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vbench PRIVATE kernels_x264)
# x264 port: pure-C kernels, no asm; the harness arch defines are forced off
# inside vb_shim.h, so no extra defines are needed here.
