/*****************************************************************************
 * vb_shim.h: shim mapping the copied upstream x264 C+asm kernels onto the
 * VideoBench harness (src/common.h, src/osdep.h, src/bench.h, src/macroblock.h).
 *
 * The copied upstream files keep their native upstream symbol names (x264_*)
 * and their `#include "common.h"` was rewritten to `#include "vb_shim.h"`.
 * This header provides everything the upstream files used to get from
 * x264's own config.h/common.h/base.h/osdep.h/tables.h/frame.h.
 *****************************************************************************/

#ifndef VB_SHIM_H
#define VB_SHIM_H

/* Harness headers. There is no common.h/osdep.h/bench.h/macroblock.h in this
 * directory, so these resolve to the harness ones in src/ (on the include path). */
#include "common.h"
#include "osdep.h"
/* note: harness bench.h is deliberately not included here; it redefines
 * vlc_large_t/bs_t/PIXEL_* which the copied upstream headers also define.
 * The PIXEL_* enum comes from the copied pixel.h below. */

/* Platform macros (SYS_*, ARCH_*, HAVE_MMX, HAVE_NEON, VBENCH_HAVE_ASM, ...)
 * come from the generated vbench_config.h (pulled in by common.h/osdep.h
 * above) and are the real host values — the same ones the harness itself is
 * compiled with. The upstream hand-written asm (x86/, aarch64/, synced
 * together with this C) is enabled through them:
 *   HAVE_MMX  = 1 only on x86_64 when the x86 asm is built (nasm);
 *   HAVE_NEON = 1 on ARM/AArch64 (from vbench_config.h);
 *   HAVE_AARCH64 = 1 on AArch64 when the aarch64 asm is built.
 * The C-plain (scalar) rebuild of these sources passes -DVBENCH_SCALAR_BUILD
 * and must never register asm pointers, so it forces everything off. */
#ifdef VBENCH_SCALAR_BUILD
#undef HAVE_MMX
#undef HAVE_ARMV6
#undef HAVE_NEON
#undef HAVE_AARCH64
#undef HAVE_MSA
#undef HAVE_ALTIVEC
#define HAVE_MMX 0
#define HAVE_ARMV6 0
#define HAVE_NEON 0
#define HAVE_AARCH64 0
#define HAVE_MSA 0
#define HAVE_ALTIVEC 0
#else
#ifndef ARCH_LOONGARCH
#define ARCH_LOONGARCH 0
#endif
/* asm dirs exist only for x86 and aarch64 in this port; the other arch asm
 * paths stay off even where vbench_config.h reports the ISA present */
#undef HAVE_ARMV6
#if ARCH_ARM && VBENCH_HAVE_ASM
#define HAVE_ARMV6 1
#else
#define HAVE_ARMV6 0
#endif
#undef HAVE_AARCH64
#if ARCH_AARCH64 && VBENCH_HAVE_ASM
#define HAVE_AARCH64 1
#else
#define HAVE_AARCH64 0
#endif
#undef HAVE_MSA
#define HAVE_MSA 0
#undef HAVE_ALTIVEC
#define HAVE_ALTIVEC 0
#endif /* VBENCH_SCALAR_BUILD */

#undef HAVE_LSX
#undef HAVE_LASX
#undef HAVE_SVE
#undef HAVE_SVE2
#undef HAVE_DOTPROD
#undef HAVE_I8MM
#undef HAVE_INTERLACED
#undef HAVE_OPENCL
#undef HAVE_PTHREAD
#undef HAVE_WIN32THREAD
#undef HAVE_X86_INLINE_ASM
#define HAVE_LSX 0
#define HAVE_LASX 0
#define HAVE_SVE 0
#define HAVE_SVE2 0
#define HAVE_DOTPROD 0
#define HAVE_I8MM 0
#define HAVE_INTERLACED 0
#define HAVE_OPENCL 0
#define HAVE_PTHREAD HAVE_POSIXTHREAD
#define HAVE_WIN32THREAD SYS_WINDOWS
#define HAVE_X86_INLINE_ASM 0

/* upstream x264.h CPU flag names -> the harness flag bits (common.h), so the
 * cpu word built by the harness (VSIMD_CPU_*) drives the upstream init code.
 * Flags the harness never detects are 0: those kernels are never registered. */
#define X264_CPU_CMOV            VSIMD_CPU_CMOV
#define X264_CPU_MMX             VSIMD_CPU_MMX
#define X264_CPU_MMX2            VSIMD_CPU_MMX2
#define X264_CPU_MMXEXT          X264_CPU_MMX2
#define X264_CPU_SSE             VSIMD_CPU_SSE
#define X264_CPU_SSE2            VSIMD_CPU_SSE2
#define X264_CPU_SSE3            VSIMD_CPU_SSE3
#define X264_CPU_SSSE3           VSIMD_CPU_SSSE3
#define X264_CPU_SSE4            VSIMD_CPU_SSE4
#define X264_CPU_SSE42           VSIMD_CPU_SSE42
#define X264_CPU_LZCNT           VSIMD_CPU_LZCNT
#define X264_CPU_AVX             VSIMD_CPU_AVX
#define X264_CPU_XOP             VSIMD_CPU_XOP
#define X264_CPU_FMA4            VSIMD_CPU_FMA4
#define X264_CPU_FMA3            VSIMD_CPU_FMA3
#define X264_CPU_AVX2            VSIMD_CPU_AVX2
#define X264_CPU_BMI1            VSIMD_CPU_BMI1
#define X264_CPU_BMI2            VSIMD_CPU_BMI2
#define X264_CPU_AVX512          0
#define X264_CPU_CACHELINE_32    VSIMD_CPU_CACHELINE_32
#define X264_CPU_CACHELINE_64    VSIMD_CPU_CACHELINE_64
#define X264_CPU_SSE2_IS_SLOW    VSIMD_CPU_SSE2_IS_SLOW
#define X264_CPU_SSE2_IS_FAST    VSIMD_CPU_SSE2_IS_FAST
#define X264_CPU_SLOW_SHUFFLE    VSIMD_CPU_SLOW_SHUFFLE
#define X264_CPU_STACK_MOD4      VSIMD_CPU_STACK_MOD4
#define X264_CPU_SLOW_CTZ        VSIMD_CPU_SLOW_CTZ
#define X264_CPU_SLOW_ATOM       VSIMD_CPU_SLOW_ATOM
#define X264_CPU_SLOW_PSHUFB     VSIMD_CPU_SLOW_PSHUFB
#define X264_CPU_SLOW_PALIGNR    VSIMD_CPU_SLOW_PALIGNR
#define X264_CPU_ALTIVEC         VSIMD_CPU_ALTIVEC
#define X264_CPU_ARMV6           VSIMD_CPU_ARMV6
#define X264_CPU_NEON            VSIMD_CPU_NEON
#define X264_CPU_FAST_NEON_MRC   VSIMD_CPU_FAST_NEON_MRC
#define X264_CPU_ARMV8           VSIMD_CPU_ARMV8
#define X264_CPU_SVE             0
#define X264_CPU_SVE2            0
#define X264_CPU_DOTPROD         0
#define X264_CPU_I8MM            0
#define X264_CPU_MSA             VSIMD_CPU_MSA

#ifndef BIT_DEPTH
#define BIT_DEPTH 8
#endif
#ifndef HIGH_BIT_DEPTH
#define HIGH_BIT_DEPTH 0
#endif
#ifndef WORDS_BIGENDIAN
#define WORDS_BIGENDIAN 0
#endif

#define SIZEOF_PIXEL ((BIT_DEPTH+7)/8)

/* 4:2:0 */
#define CHROMA_H_SHIFT 1
#define CHROMA_V_SHIFT 1

#ifndef ALIGNED_64
#define ALIGNED_64( var ) DECLARE_ALIGNED( var, 64 )
#endif

/* ---- upstream base.h / osdep.h / common.h bits (post-prefix names) ---- */

#define X264_MIN MIN
#define X264_MAX MAX
#define X264_SCAN8_SIZE SCAN8_SIZE
#define X264_SCAN8_LUMA_SIZE SCAN8_LUMA_SIZE
#define X264_SCAN8_0 SCAN8_0

/* upstream common.h/osdep.h bit-depth templating: the asm symbols are
 * x264_<BIT_DEPTH>_<name> (asm.S EXTERN_ASM) and every templated C symbol
 * goes through the same macro. Written without a bare `x264` prefix token
 * so that the x264_ -> x264s_ scalar-rebuild rename hits every prefix. */
#define x264_template3(bd, w) x264_##bd##_##w
#define x264_template2(bd, w) x264_template3(bd, w)
#define x264_template(w) x264_template2(BIT_DEPTH, w)

/* the harness common.h already provides the x264_scan8 table */

#define x264_emms() vbench_emms()
#define x264_clz(x) __builtin_clz(x)
#define x264_ctz(x) __builtin_ctz(x)
#define x264_constant_p(x) __builtin_constant_p(x)
#define x264_nonconstant_p(x) (!__builtin_constant_p(x))

/* little-endian host */
#define endian_fix(x) (x)
#define endian_fix16(x) (x)
#define endian_fix32(x) (x)
#define endian_fix64(x) (x)

/* zero table: x264_weight_none (mc.h) is ((const x264_weight_t*)x264_zero) and
 * reads weightfn at offset 0x30, so this must cover sizeof(x264_weight_t);
 * newer upstream tables.c defines a global x264_zero[1024] instead */
static ALIGNED_16( const uint8_t x264_zero[64] ) = {0};

static ALWAYS_INLINE int x264_clip3( int v, int i_min, int i_max )
{
    return vbench_clip3( v, i_min, i_max );
}
#define x264_clip_pixel(x) vbench_clip_pixel(x)

/* ---- types that only appear in (unused) encoder-side prototypes ---- */
typedef struct x264_t x264_t;
typedef struct x264_frame_t x264_frame_t;

typedef struct x264_nal_t
{
    int i_ref_idc;
    int i_type;
    int b_long_startcode;
    int i_first_mb;
    int i_last_mb;
    int     i_payload;
    uint8_t *p_payload;
    int i_padding;
} x264_nal_t;

/* ---- deblock function table (from upstream common/frame.h) ---- */
typedef void (*x264_deblock_inter_t)( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 );
typedef void (*x264_deblock_intra_t)( pixel *pix, intptr_t stride, int alpha, int beta );
typedef struct
{
    x264_deblock_inter_t deblock_luma[2];
    x264_deblock_inter_t deblock_chroma[2];
    x264_deblock_inter_t deblock_h_chroma_420;
    x264_deblock_inter_t deblock_h_chroma_422;
    x264_deblock_intra_t deblock_luma_intra[2];
    x264_deblock_intra_t deblock_chroma_intra[2];
    x264_deblock_intra_t deblock_h_chroma_420_intra;
    x264_deblock_intra_t deblock_h_chroma_422_intra;
    x264_deblock_inter_t deblock_luma_mbaff;
    x264_deblock_inter_t deblock_chroma_mbaff;
    x264_deblock_inter_t deblock_chroma_420_mbaff;
    x264_deblock_inter_t deblock_chroma_422_mbaff;
    x264_deblock_intra_t deblock_luma_intra_mbaff;
    x264_deblock_intra_t deblock_chroma_intra_mbaff;
    x264_deblock_intra_t deblock_chroma_420_intra_mbaff;
    x264_deblock_intra_t deblock_chroma_422_intra_mbaff;
    void (*deblock_strength)( uint8_t nnz[X264_SCAN8_SIZE], int8_t ref[2][X264_SCAN8_LUMA_SIZE],
                              int16_t mv[2][X264_SCAN8_LUMA_SIZE][2], uint8_t bs[2][8][4], int mvy_limit,
                              int bframe );
} x264_deblock_function_t;

void x264_deblock_init( uint32_t cpu, x264_deblock_function_t *pf, int b_mbaff );

/* ---- cabac context tables (from upstream common/tables.c, see vb_tables.c) ---- */
extern const int8_t  x264_cabac_context_init_I[1024][2];
extern const int8_t  x264_cabac_context_init_PB[3][1024][2];
extern const uint8_t x264_cabac_range_lps[64][4];
extern const uint8_t x264_cabac_transition[128][2];
extern const uint8_t x264_cabac_renorm_shift[64];
extern const uint8_t x264_hpel_ref0[16];
extern const uint8_t x264_hpel_ref1[16];
extern const uint8_t x264_decimate_table4[16];
extern const uint8_t x264_decimate_table8[64];
extern const uint16_t x264_cabac_entropy[128];

/* ---- copied upstream headers (upstream common.h inclusion list, minus the
 *      encoder-only set.h/frame.h/threadpool.h/rectangle.h) ---- */
#include "cabac.h"
#include "bitstream.h"
#include "predict.h"
#include "pixel.h"
#include "mc.h"
#include "dct.h"
#include "quant.h"

/* harness macroblock.h (needs the PIXEL_* enum from the copied pixel.h above) */
#include "macroblock.h"

/* minimal encoder handle: only the members the copied asm-dir C helpers touch
 * (mc.h PROPAGATE_LIST: h->scratch_buffer2, h->mb.i_mb_*; x86/mc-c.c weight
 * cache: h->mc.weight/offsetadd/offsetsub) */
struct x264_t
{
    void *scratch_buffer2;
    struct
    {
        unsigned i_mb_stride, i_mb_width, i_mb_height;
    } mb;
    struct
    {
        x264_weight_fn_t *weight, *offsetadd, *offsetsub;
    } mc;
};

#endif // VB_SHIM_H
