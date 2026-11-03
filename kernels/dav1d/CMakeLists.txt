# dav1d kernel port (agent D). See PORTING.md.
file(GLOB vb_srcs CONFIGURE_DEPENDS *.c)
add_library(kernels_dav1d STATIC ${vb_srcs})
target_include_directories(kernels_dav1d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(vbench PRIVATE kernels_dav1d)

# harness arch defines so this module also compiles standalone
# (config.h in this dir defaults ARCH_* to 0 and defers to these)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    target_compile_definitions(kernels_dav1d PRIVATE ARCH_AARCH64=1 STACK_ALIGNMENT=16)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    target_compile_definitions(kernels_dav1d PRIVATE ARCH_X86_64=1 STACK_ALIGNMENT=32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86")
    target_compile_definitions(kernels_dav1d PRIVATE ARCH_X86=1 STACK_ALIGNMENT=32)
endif()
