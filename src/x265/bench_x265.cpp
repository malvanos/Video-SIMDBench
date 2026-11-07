/*****************************************************************************
 * bench_x265.cpp: benchmark module for the ported x265 C kernels.
 *
 * Implements extern "C" int vbench_codec_x265(int cpu_ref, int cpu_new) per
 * PORTING.md. The x265 kernels are C++ in namespace x265, so no symbol
 * prefixing is needed; pixel.cpp kernels are file-static and are reached
 * through the x265::primitives table filled by the setup*Primitives_c
 * functions, dct/idct kernels are called directly as x265::dct*_c.
 *
 * Part of the Video-SIMDBench port of x265 (AGPL, see COPYING).
 *****************************************************************************/

/* harness headers; wrapped in extern "C" because the harness globals and
 * get_bench() are defined in the C sources (src/bench.c, src/main.c) */
extern "C" {
#include "common.h"
#include "osdep.h"
#include "bench.h"
}

#ifndef set_func_name
#define set_func_name(...) snprintf(func_name, sizeof(func_name), __VA_ARGS__)
#endif

/* bench.h's call_c assigns the function pointer to a void* bench_t member,
 * which is ill-formed in C++ (function pointer -> void*). Redefine call_c
 * here with an explicit reinterpret_cast; timing semantics are identical
 * to call_c/call_c2/call_c1 in bench.h (C column, cpu=0). */
#undef call_c
#define call_c(func,...) ({ \
    if( !strncmp(func_name, bench_pattern, bench_pattern_len) ) \
    { \
        uint64_t tsum = 0; \
        int tcount = 0; \
        func(__VA_ARGS__); \
        for( int ti = 0; ti < (BENCH_RUNS/4); ti++ ) \
        { \
            uint32_t t = read_time(); \
            func(__VA_ARGS__); \
            t = read_time() - t; \
            if( (uint64_t)t*tcount <= tsum*4 && ti > 0 ) \
            { \
                tsum += t; \
                tcount++; \
            } \
        } \
        bench_t *b = get_bench( func_name, 0 ); \
        b->cycles += tsum; \
        b->den += tcount; \
        b->pointer = reinterpret_cast<void*>(func); \
    } \
    func(__VA_ARGS__); \
})

/* call_cp: same as call_c but registers the timing under the C-plain column
 * (VSIMD_CPU_PLAIN_C pseudo-flag) — the scalar rebuild of the same kernels
 * (namespace x265s, compiled with -fno-tree-vectorize -fno-slp-vectorize).
 * Mirrors call_bench's nonzero-cpu path (BENCH_RUNS iterations). */
#undef call_cp
#define call_cp(func,...) ({ \
    if( !strncmp(func_name, bench_pattern, bench_pattern_len) ) \
    { \
        uint64_t tsum = 0; \
        int tcount = 0; \
        func(__VA_ARGS__); \
        for( int ti = 0; ti < BENCH_RUNS; ti++ ) \
        { \
            uint32_t t = read_time(); \
            func(__VA_ARGS__); \
            t = read_time() - t; \
            if( (uint64_t)t*tcount <= tsum*4 && ti > 0 ) \
            { \
                tsum += t; \
                tcount++; \
            } \
        } \
        bench_t *b = get_bench( func_name, VSIMD_CPU_PLAIN_C ); \
        b->cycles += tsum; \
        b->den += tcount; \
        b->pointer = reinterpret_cast<void*>(func); \
    } \
    func(__VA_ARGS__); \
})

/* call_a: same shape as call_c/call_cp but registers the timing under the
 * ISA column (cpu_new) — the hand-written asm kernels. As above, the function
 * pointer needs an explicit reinterpret_cast in C++. */
#undef call_a2
#define call_a2(func,...) ({ \
    if( !strncmp(func_name, bench_pattern, bench_pattern_len) ) \
    { \
        uint64_t tsum = 0; \
        int tcount = 0; \
        func(__VA_ARGS__); \
        for( int ti = 0; ti < BENCH_RUNS; ti++ ) \
        { \
            uint32_t t = read_time(); \
            func(__VA_ARGS__); \
            t = read_time() - t; \
            if( (uint64_t)t*tcount <= tsum*4 && ti > 0 ) \
            { \
                tsum += t; \
                tcount++; \
            } \
        } \
        bench_t *b = get_bench( func_name, cpu_new ); \
        b->cycles += tsum; \
        b->den += tcount; \
        b->pointer = reinterpret_cast<void*>(func); \
    } \
    func(__VA_ARGS__); \
})
#undef call_a
#define call_a(func,...) call_a2(func,__VA_ARGS__)

/* bench the vec (-O3), scalar (C-plain) and asm (hand-written) builds of the
 * same kernel under the C, C-plain and ISA columns. Kernels returning a value
 * must agree across all three (ret=-1 on mismatch). The asm entry is only
 * registered when the arch setup actually installed an asm kernel (pointer
 * differs from the C one), so rows without asm keep an empty ISA cell. */
#define call_c_cp_a(f_v, f_s, f_a, ...) \
    do { int rv = call_c( f_v, __VA_ARGS__ ); \
         int rs = call_cp( f_s, __VA_ARGS__ ); \
         int ra = ( (const void*)(f_a) != (const void*)(f_v) ) ? call_a( f_a, __VA_ARGS__ ) : rv; \
         if( rv != rs || rv != ra ) ret = -1; } while(0)
#define call_c_cp_a_void(f_v, f_s, f_a, ...) \
    do { call_c( f_v, __VA_ARGS__ ); call_cp( f_s, __VA_ARGS__ ); \
         if( (const void*)(f_a) != (const void*)(f_v) ) call_a( f_a, __VA_ARGS__ ); } while(0)

/* x265 shim + ported kernel headers */
#include "x265shim.h"
#include "primitives.h"
#include "constants.h"

namespace x265 {
/* setup functions (external linkage in the copied kernel files) */
void setupPixelPrimitives_c(EncoderPrimitives& p);
void setupFilterPrimitives_c(EncoderPrimitives& p);
void setupDCTPrimitives_c(EncoderPrimitives& p);
void setupIntraPrimitives_c(EncoderPrimitives& p);
void setupLoopFilterPrimitives_c(EncoderPrimitives& p);

/* dct/idct kernels (external linkage in dct.cpp) */
void dst4_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void dct4_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void dct8_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void dct16_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void dct32_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void idst4_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
void idct4_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
void idct8_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
void idct16_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
void idct32_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
}

namespace x265s {
/* Scalar ("C-plain") rebuild of the same kernels: renamed copies of the same
 * sources with X265_NS=x265s, so everything lives in namespace x265s with
 * byte-identical layouts. The forward declaration makes these declarations
 * mangle exactly like the definitions in the scalar lib (a typedef alias
 * would mangle as the underlying x265:: type and not link). */
struct EncoderPrimitives;
extern EncoderPrimitives primitives;
void setupPixelPrimitives_c(EncoderPrimitives& p);
void setupFilterPrimitives_c(EncoderPrimitives& p);
void setupDCTPrimitives_c(EncoderPrimitives& p);
void setupIntraPrimitives_c(EncoderPrimitives& p);
void setupLoopFilterPrimitives_c(EncoderPrimitives& p);
void dst4_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void dct4_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void dct8_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void dct16_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void dct32_c(const int16_t* src, int16_t* dst, intptr_t srcStride);
void idst4_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
void idct4_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
void idct8_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
void idct16_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
void idct32_c(const int16_t* src, int16_t* dst, intptr_t dstStride);
}

/* view the scalar lib's primitive table through the identical vec type */
static x265::EncoderPrimitives& prim_s = *reinterpret_cast<x265::EncoderPrimitives*>(&x265s::primitives);
#define X265S_SETUP(fn) fn(reinterpret_cast<x265s::EncoderPrimitives&>(prim_s))

/* asm ("ISA column") primitive table: filled with the C kernels, then
 * x265::setupAssemblyPrimitives() overrides the entries for which upstream
 * hand-written asm exists and the cpu mask enables it */
static x265::EncoderPrimitives prim_a;

/* map the harness cpu flag word (cpu_new) to x265's X265_CPU_* flags */
static int x265_cpu_mask( int cpu_new )
{
#if ARCH_AARCH64 || ARCH_ARM
    int m = 0;
    if( cpu_new & VSIMD_CPU_NEON )
        m |= X265_CPU_NEON;
    return m;
#else
    int m = 0;
    if( cpu_new & VSIMD_CPU_MMX2 )  m |= X265_CPU_MMX2;
    if( cpu_new & VSIMD_CPU_SSE )   m |= X265_CPU_SSE;
    if( cpu_new & VSIMD_CPU_SSE2 )  m |= X265_CPU_SSE2;
    if( cpu_new & VSIMD_CPU_SSE2_IS_SLOW ) m |= X265_CPU_SSE2_IS_SLOW;
    if( cpu_new & VSIMD_CPU_SSE3 )  m |= X265_CPU_SSE3;
    if( cpu_new & VSIMD_CPU_SSSE3 ) m |= X265_CPU_SSSE3;
    if( cpu_new & VSIMD_CPU_SSE4 )  m |= X265_CPU_SSE4;
    if( cpu_new & VSIMD_CPU_SSE42 ) m |= X265_CPU_SSE42;
    if( cpu_new & VSIMD_CPU_AVX )   m |= X265_CPU_AVX;
    if( cpu_new & VSIMD_CPU_XOP )   m |= X265_CPU_XOP;
    if( cpu_new & VSIMD_CPU_FMA3 )  m |= X265_CPU_FMA3;
    if( cpu_new & VSIMD_CPU_BMI2 )  m |= X265_CPU_BMI2;
    if( cpu_new & VSIMD_CPU_AVX2 )  m |= X265_CPU_AVX2;
    return m;
#endif
}

extern "C" int vbench_codec_x265(int cpu_ref, int cpu_new)
{
        (void)cpu_ref;
    int ret = 0;

    /* fill the C primitive tables with the ported kernels (vec + scalar) */
    x265::setupPixelPrimitives_c(x265::primitives);
    x265::setupFilterPrimitives_c(x265::primitives);
    x265::setupDCTPrimitives_c(x265::primitives);
    x265::setupIntraPrimitives_c(x265::primitives);
    x265::setupLoopFilterPrimitives_c(x265::primitives);
    x265s::X265S_SETUP(setupPixelPrimitives_c);
    x265s::X265S_SETUP(setupFilterPrimitives_c);
    x265s::X265S_SETUP(setupDCTPrimitives_c);
    x265s::X265S_SETUP(setupIntraPrimitives_c);
    x265s::X265S_SETUP(setupLoopFilterPrimitives_c);

    /* asm table: C base + upstream asm for the enabled cpu flags */
    x265::setupPixelPrimitives_c(prim_a);
    x265::setupFilterPrimitives_c(prim_a);
    x265::setupDCTPrimitives_c(prim_a);
    x265::setupIntraPrimitives_c(prim_a);
    x265::setupLoopFilterPrimitives_c(prim_a);
    x265::setupAssemblyPrimitives(prim_a, x265_cpu_mask(cpu_new));

    /* strides: pbuf* are 0x1e00 bytes each */
    const intptr_t SS = 64;   /* src stride */
    const intptr_t DS = 32;   /* dst stride */
    pixel *src = pbuf1 + 8 * 64 + 8;  /* margin for the mc filters */
    pixel *dst = pbuf3;

    /* ---- pixel: sad / ssd / satd ---- */
    x265::pixelcmp_t sad16 = x265::primitives.pu[x265::LUMA_16x16].sad;
    x265::pixelcmp_t sad16a = prim_a.pu[x265::LUMA_16x16].sad;
    x265::pixelcmp_t sad8  = x265::primitives.pu[x265::LUMA_8x8].sad;
    x265::pixelcmp_t sad8a = prim_a.pu[x265::LUMA_8x8].sad;
    x265::pixelcmp_t sad4  = x265::primitives.pu[x265::LUMA_4x4].sad;
    x265::pixelcmp_t sad4a = prim_a.pu[x265::LUMA_4x4].sad;
    x265::pixel_sse_t ssd16 = x265::primitives.cu[x265::BLOCK_16x16].sse_pp;
    x265::pixel_sse_t ssd16a = prim_a.cu[x265::BLOCK_16x16].sse_pp;
    x265::pixel_sse_t ssd8  = x265::primitives.cu[x265::BLOCK_8x8].sse_pp;
    x265::pixel_sse_t ssd8a = prim_a.cu[x265::BLOCK_8x8].sse_pp;
    x265::pixelcmp_t satd16 = x265::primitives.pu[x265::LUMA_16x16].satd;
    x265::pixelcmp_t satd16a = prim_a.pu[x265::LUMA_16x16].satd;
    x265::pixelcmp_t satd8  = x265::primitives.pu[x265::LUMA_8x8].satd;
    x265::pixelcmp_t satd8a = prim_a.pu[x265::LUMA_8x8].satd;
    x265::pixelcmp_t satd4  = x265::primitives.pu[x265::LUMA_4x4].satd;
    x265::pixelcmp_t satd4a = prim_a.pu[x265::LUMA_4x4].satd;
    x265::pixelcmp_t sad16s = prim_s.pu[x265::LUMA_16x16].sad;
    x265::pixelcmp_t sad8s  = prim_s.pu[x265::LUMA_8x8].sad;
    x265::pixelcmp_t sad4s  = prim_s.pu[x265::LUMA_4x4].sad;
    x265::pixel_sse_t ssd16s = prim_s.cu[x265::BLOCK_16x16].sse_pp;
    x265::pixel_sse_t ssd8s  = prim_s.cu[x265::BLOCK_8x8].sse_pp;
    x265::pixelcmp_t satd16s = prim_s.pu[x265::LUMA_16x16].satd;
    x265::pixelcmp_t satd8s  = prim_s.pu[x265::LUMA_8x8].satd;
    x265::pixelcmp_t satd4s  = prim_s.pu[x265::LUMA_4x4].satd;

    set_func_name("x265: sad 16x16");
    call_c_cp_a(sad16, sad16s, sad16a, pbuf1, SS, pbuf2, DS);
    set_func_name("x265: sad 8x8");
    call_c_cp_a(sad8, sad8s, sad8a, pbuf1, SS, pbuf2, DS);
    set_func_name("x265: sad 4x4");
    call_c_cp_a(sad4, sad4s, sad4a, pbuf1, SS, pbuf2, DS);
    set_func_name("x265: ssd 16x16");
    call_c_cp_a(ssd16, ssd16s, ssd16a, pbuf1, SS, pbuf2, DS);
    set_func_name("x265: ssd 8x8");
    call_c_cp_a(ssd8, ssd8s, ssd8a, pbuf1, SS, pbuf2, DS);
    set_func_name("x265: satd 16x16");
    call_c_cp_a(satd16, satd16s, satd16a, pbuf1, SS, pbuf2, DS);
    set_func_name("x265: satd 8x8");
    call_c_cp_a(satd8, satd8s, satd8a, pbuf1, SS, pbuf2, DS);
    set_func_name("x265: satd 4x4");
    call_c_cp_a(satd4, satd4s, satd4a, pbuf1, SS, pbuf2, DS);

    /* ---- copy ---- */
    x265::copy_pp_t copy16 = x265::primitives.pu[x265::LUMA_16x16].copy_pp;
    x265::copy_pp_t copy16a = prim_a.pu[x265::LUMA_16x16].copy_pp;
    x265::copy_pp_t copy8  = x265::primitives.pu[x265::LUMA_8x8].copy_pp;
    x265::copy_pp_t copy8a = prim_a.pu[x265::LUMA_8x8].copy_pp;
    x265::copy_pp_t copy16s = prim_s.pu[x265::LUMA_16x16].copy_pp;
    x265::copy_pp_t copy8s  = prim_s.pu[x265::LUMA_8x8].copy_pp;

    set_func_name("x265: copy 16x16");
    call_c_cp_a_void(copy16, copy16s, copy16a, dst, DS, pbuf1, SS);
    set_func_name("x265: copy 8x8");
    call_c_cp_a_void(copy8, copy8s, copy8a, dst, DS, pbuf1, SS);

    /* ---- mc: luma hpel/vpel/qpel, chroma hpel/qpel ---- */
    x265::filter_pp_t luma_hpp16  = x265::primitives.pu[x265::LUMA_16x16].luma_hpp;
    x265::filter_pp_t luma_hpp16a = prim_a.pu[x265::LUMA_16x16].luma_hpp;
    x265::filter_pp_t luma_vpp16  = x265::primitives.pu[x265::LUMA_16x16].luma_vpp;
    x265::filter_pp_t luma_vpp16a = prim_a.pu[x265::LUMA_16x16].luma_vpp;
    x265::filter_hv_pp_t luma_hvpp16 = x265::primitives.pu[x265::LUMA_16x16].luma_hvpp;
    x265::filter_hv_pp_t luma_hvpp16a = prim_a.pu[x265::LUMA_16x16].luma_hvpp;
    x265::filter_pp_t luma_hpp8   = x265::primitives.pu[x265::LUMA_8x8].luma_hpp;
    x265::filter_pp_t luma_hpp8a = prim_a.pu[x265::LUMA_8x8].luma_hpp;
    x265::filter_pp_t ch_hpp8 = x265::primitives.chroma[X265_CSP_I420].pu[x265::CHROMA_420_8x8].filter_hpp;
    x265::filter_pp_t ch_hpp8a = prim_a.chroma[X265_CSP_I420].pu[x265::CHROMA_420_8x8].filter_hpp;
    x265::filter_pp_t ch_vpp8 = x265::primitives.chroma[X265_CSP_I420].pu[x265::CHROMA_420_8x8].filter_vpp;
    x265::filter_pp_t ch_vpp8a = prim_a.chroma[X265_CSP_I420].pu[x265::CHROMA_420_8x8].filter_vpp;
    x265::filter_pp_t luma_hpp16s  = prim_s.pu[x265::LUMA_16x16].luma_hpp;
    x265::filter_pp_t luma_vpp16s  = prim_s.pu[x265::LUMA_16x16].luma_vpp;
    x265::filter_hv_pp_t luma_hvpp16s = prim_s.pu[x265::LUMA_16x16].luma_hvpp;
    x265::filter_pp_t luma_hpp8s   = prim_s.pu[x265::LUMA_8x8].luma_hpp;
    x265::filter_pp_t ch_hpp8s = prim_s.chroma[X265_CSP_I420].pu[x265::CHROMA_420_8x8].filter_hpp;
    x265::filter_pp_t ch_vpp8s = prim_s.chroma[X265_CSP_I420].pu[x265::CHROMA_420_8x8].filter_vpp;

    set_func_name("x265: mc luma hpel 16x16");
    call_c_cp_a_void(luma_hpp16, luma_hpp16s, luma_hpp16a, src, SS, dst, DS, 6);
    set_func_name("x265: mc luma vpel 16x16");
    call_c_cp_a_void(luma_vpp16, luma_vpp16s, luma_vpp16a, src, SS, dst, DS, 6);
    set_func_name("x265: mc luma qpel 16x16");
    call_c_cp_a_void(luma_hvpp16, luma_hvpp16s, luma_hvpp16a, src, SS, dst, DS, 2, 6);
    set_func_name("x265: mc luma hpel 8x8");
    call_c_cp_a_void(luma_hpp8, luma_hpp8s, luma_hpp8a, src, SS, dst, DS, 4);
    set_func_name("x265: mc chroma hpel 8x8");
    call_c_cp_a_void(ch_hpp8, ch_hpp8s, ch_hpp8a, src, SS, dst, DS, 4);
    set_func_name("x265: mc chroma qpel 8x8");
    call_c_cp_a_void(ch_vpp8, ch_vpp8s, ch_vpp8a, src, SS, dst, DS, 6);

    /* ---- dct / idct (residual and coeff buffers are int16_t) ---- */
    int16_t *rsrc = (int16_t*)buf1;
    int16_t *rdst = (int16_t*)buf3;

    set_func_name("x265: dct 4x4");
    call_c_cp_a_void(x265::primitives.cu[x265::BLOCK_4x4].dct, prim_s.cu[x265::BLOCK_4x4].dct, prim_a.cu[x265::BLOCK_4x4].dct, rsrc, rdst, (intptr_t)4);
    set_func_name("x265: dct 8x8");
    call_c_cp_a_void(x265::primitives.cu[x265::BLOCK_8x8].dct, prim_s.cu[x265::BLOCK_8x8].dct, prim_a.cu[x265::BLOCK_8x8].dct, rsrc, rdst, (intptr_t)8);
    set_func_name("x265: dct 16x16");
    call_c_cp_a_void(x265::primitives.cu[x265::BLOCK_16x16].dct, prim_s.cu[x265::BLOCK_16x16].dct, prim_a.cu[x265::BLOCK_16x16].dct, rsrc, rdst, (intptr_t)16);
    set_func_name("x265: dct 32x32");
    call_c_cp_a_void(x265::primitives.cu[x265::BLOCK_32x32].dct, prim_s.cu[x265::BLOCK_32x32].dct, prim_a.cu[x265::BLOCK_32x32].dct, rsrc, rdst, (intptr_t)32);
    set_func_name("x265: idct 4x4");
    call_c_cp_a_void(x265::primitives.cu[x265::BLOCK_4x4].idct, prim_s.cu[x265::BLOCK_4x4].idct, prim_a.cu[x265::BLOCK_4x4].idct, rsrc, rdst, (intptr_t)4);
    set_func_name("x265: idct 8x8");
    call_c_cp_a_void(x265::primitives.cu[x265::BLOCK_8x8].idct, prim_s.cu[x265::BLOCK_8x8].idct, prim_a.cu[x265::BLOCK_8x8].idct, rsrc, rdst, (intptr_t)8);
    set_func_name("x265: idct 16x16");
    call_c_cp_a_void(x265::primitives.cu[x265::BLOCK_16x16].idct, prim_s.cu[x265::BLOCK_16x16].idct, prim_a.cu[x265::BLOCK_16x16].idct, rsrc, rdst, (intptr_t)16);
    set_func_name("x265: idct 32x32");
    call_c_cp_a_void(x265::primitives.cu[x265::BLOCK_32x32].idct, prim_s.cu[x265::BLOCK_32x32].idct, prim_a.cu[x265::BLOCK_32x32].idct, rsrc, rdst, (intptr_t)32);
    set_func_name("x265: dst 4x4");
    call_c_cp_a_void(x265::primitives.dst4x4, prim_s.dst4x4, prim_a.dst4x4, rsrc, rdst, (intptr_t)4);
    set_func_name("x265: idst 4x4");
    call_c_cp_a_void(x265::primitives.idst4x4, prim_s.idst4x4, prim_a.idst4x4, rsrc, rdst, (intptr_t)4);

    /* ---- quant / dequant ---- */
    int16_t *coef   = (int16_t*)buf1;
    int32_t *qmf    = (int32_t*)buf2;   /* quantCoeff / dequantCoef multipliers */
    int32_t *deltaU = (int32_t*)buf3;
    int16_t *qcoef  = (int16_t*)buf4;
    for (int i = 0; i < 256; i++)
        qmf[i] = 0x4000 + i;            /* small positive multipliers */

    set_func_name("x265: quant 4x4");
    call_c_cp_a(x265::primitives.quant, prim_s.quant, prim_a.quant, coef, qmf, deltaU, qcoef, 17, 1 << 16, 16);
    set_func_name("x265: quant 8x8");
    call_c_cp_a(x265::primitives.quant, prim_s.quant, prim_a.quant, coef, qmf, deltaU, qcoef, 17, 1 << 16, 64);
    set_func_name("x265: dequant normal 4x4");
    call_c_cp_a_void(x265::primitives.dequant_normal, prim_s.dequant_normal, prim_a.dequant_normal, qcoef, coef, 16, 1 << 10, 6);
    set_func_name("x265: dequant scaling 8x8");
    call_c_cp_a_void(x265::primitives.dequant_scaling, prim_s.dequant_scaling, prim_a.dequant_scaling, qcoef, qmf, coef, 64, 4, 6);

    /* ---- intra prediction: dc / planar / angular ---- */
    x265::intra_pred_t dc16    = x265::primitives.cu[x265::BLOCK_16x16].intra_pred[DC_IDX];
    x265::intra_pred_t dc16a = prim_a.cu[x265::BLOCK_16x16].intra_pred[DC_IDX];
    x265::intra_pred_t dc8     = x265::primitives.cu[x265::BLOCK_8x8].intra_pred[DC_IDX];
    x265::intra_pred_t dc8a = prim_a.cu[x265::BLOCK_8x8].intra_pred[DC_IDX];
    x265::intra_pred_t planar16 = x265::primitives.cu[x265::BLOCK_16x16].intra_pred[PLANAR_IDX];
    x265::intra_pred_t planar16a = prim_a.cu[x265::BLOCK_16x16].intra_pred[PLANAR_IDX];
    x265::intra_pred_t ang16   = x265::primitives.cu[x265::BLOCK_16x16].intra_pred[30];
    x265::intra_pred_t ang16a = prim_a.cu[x265::BLOCK_16x16].intra_pred[30];
    x265::intra_pred_t ang4    = x265::primitives.cu[x265::BLOCK_4x4].intra_pred[18];
    x265::intra_pred_t ang4a = prim_a.cu[x265::BLOCK_4x4].intra_pred[18];
    x265::intra_pred_t dc16s    = prim_s.cu[x265::BLOCK_16x16].intra_pred[DC_IDX];
    x265::intra_pred_t dc8s     = prim_s.cu[x265::BLOCK_8x8].intra_pred[DC_IDX];
    x265::intra_pred_t planar16s = prim_s.cu[x265::BLOCK_16x16].intra_pred[PLANAR_IDX];
    x265::intra_pred_t ang16s   = prim_s.cu[x265::BLOCK_16x16].intra_pred[30];
    x265::intra_pred_t ang4s    = prim_s.cu[x265::BLOCK_4x4].intra_pred[18];

    set_func_name("x265: intra pred dc 16x16");
    call_c_cp_a_void(dc16, dc16s, dc16a, dst, DS, pbuf1, DC_IDX, 1);
    set_func_name("x265: intra pred dc 8x8");
    call_c_cp_a_void(dc8, dc8s, dc8a, dst, DS, pbuf1, DC_IDX, 1);
    set_func_name("x265: intra pred planar 16x16");
    call_c_cp_a_void(planar16, planar16s, planar16a, dst, DS, pbuf1, PLANAR_IDX, 0);
    set_func_name("x265: intra pred angular 16x16");
    call_c_cp_a_void(ang16, ang16s, ang16a, dst, DS, pbuf1, 30, 1);
    set_func_name("x265: intra pred angular 4x4");
    call_c_cp_a_void(ang4, ang4s, ang4a, dst, (intptr_t)8, pbuf1, 18, 1);

    /* ---- loopfilter: SAO (luma/chroma) + deblocking pel filters ---- */
    int8_t offsetEo[4] = { 1, -2, 3, -4 };
    int8_t offsetBo[32];
    int8_t signLeft[64];
    int8_t upBuff[64];
    for (int i = 0; i < 32; i++)
        offsetBo[i] = (int8_t)(i - 16);
    for (int i = 0; i < 64; i++)
    {
        signLeft[i] = (int8_t)((i & 1) ? 1 : -1);
        upBuff[i]   = (int8_t)(i & 3);
    }

    set_func_name("x265: sao luma edge E0");
    call_c_cp_a_void(x265::primitives.saoCuOrgE0, prim_s.saoCuOrgE0, prim_a.saoCuOrgE0, dst, offsetEo, 16, signLeft, DS);
    set_func_name("x265: sao luma band B0");
    call_c_cp_a_void(x265::primitives.saoCuOrgB0, prim_s.saoCuOrgB0, prim_a.saoCuOrgB0, dst, offsetBo, 16, 16, DS);
    set_func_name("x265: sao chroma edge E1");
    call_c_cp_a_void(x265::primitives.saoCuOrgE1, prim_s.saoCuOrgE1, prim_a.saoCuOrgE1, dst, upBuff, offsetEo, DS, 8);
    set_func_name("x265: deblock luma strong");
    call_c_cp_a_void(x265::primitives.pelFilterLumaStrong[0], prim_s.pelFilterLumaStrong[0], prim_a.pelFilterLumaStrong[0], dst + 64, (intptr_t)1, (intptr_t)8, 32, 32);
    set_func_name("x265: deblock chroma V");
    call_c_cp_a_void(x265::primitives.pelFilterChroma[0], prim_s.pelFilterChroma[0], prim_a.pelFilterChroma[0], dst + 64, (intptr_t)1, (intptr_t)2, 16, -1, -1);

    return ret;
}
