/*****************************************************************************
 * vb_shim.h: shim mapping the copied upstream x264 C kernels onto the
 * VideoBench harness (src/common.h, src/osdep.h, src/bench.h, src/macroblock.h).
 *
 * The copied upstream files were mechanically renamed x264_ -> vbx264_ and
 * their `#include "common.h"` was rewritten to `#include "vb_shim.h"`.
 * This header provides everything the upstream files used to get from
 * x264's own common.h/base.h/osdep.h/tables.h/frame.h.
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

/* Force a pure-C build of the copied upstream kernels, whatever -D flags the
 * harness itself was compiled with (arch/asm paths stay disabled here). */
#undef ARCH_X86
#undef ARCH_X86_64
#undef ARCH_ARM
#undef ARCH_AARCH64
#undef ARCH_PPC
#undef ARCH_MIPS
#undef ARCH_LOONGARCH
#undef SYS_WINDOWS
#undef SYS_LINUX
#undef SYS_MACOSX
#undef SYS_BEOS
#undef SYS_OPENBSD
#undef SYS_SunOS
#define ARCH_X86 0
#define ARCH_X86_64 0
#define ARCH_ARM 0
#define ARCH_AARCH64 0
#define ARCH_PPC 0
#define ARCH_MIPS 0
#define ARCH_LOONGARCH 0
#define SYS_WINDOWS 0
#define SYS_LINUX 0
#define SYS_MACOSX 0
#define SYS_BEOS 0
#define SYS_OPENBSD 0
#define SYS_SunOS 0

#undef HAVE_MMX
#undef HAVE_ARMV6
#undef HAVE_NEON
#undef HAVE_AARCH64
#undef HAVE_MSA
#undef HAVE_LSX
#undef HAVE_LASX
#undef HAVE_ALTIVEC
#undef HAVE_SVE
#undef HAVE_SVE2
#undef HAVE_INTERLACED
#undef HAVE_OPENCL
#undef HAVE_THREAD
#undef HAVE_PTHREAD
#undef HAVE_WIN32THREAD
#undef HAVE_X86_INLINE_ASM
#define HAVE_MMX 0
#define HAVE_ARMV6 0
#define HAVE_NEON 0
#define HAVE_AARCH64 0
#define HAVE_MSA 0
#define HAVE_LSX 0
#define HAVE_LASX 0
#define HAVE_ALTIVEC 0
#define HAVE_SVE 0
#define HAVE_SVE2 0
#define HAVE_INTERLACED 0
#define HAVE_OPENCL 0
#define HAVE_THREAD 0
#define HAVE_PTHREAD 0
#define HAVE_WIN32THREAD 0
#define HAVE_X86_INLINE_ASM 0

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

/* identity templating: keeps every symbol a plain vbx264_<name> */
#define vbx264_template(w) vbx264_##w

/* harness common.h array */
#define vbx264_scan8 x264_scan8

#define vbx264_emms() vbench_emms()
#define vbx264_clz(x) __builtin_clz(x)
#define vbx264_ctz(x) __builtin_ctz(x)
#define vbx264_constant_p(x) __builtin_constant_p(x)
#define vbx264_nonconstant_p(x) (!__builtin_constant_p(x))

/* little-endian host */
#define endian_fix(x) (x)
#define endian_fix16(x) (x)
#define endian_fix32(x) (x)
#define endian_fix64(x) (x)

static const uint8_t vbx264_zero[16] = {0};

static ALWAYS_INLINE int vbx264_clip3( int v, int i_min, int i_max )
{
    return vbench_clip3( v, i_min, i_max );
}
#define vbx264_clip_pixel(x) vbench_clip_pixel(x)

/* ---- types that only appear in (unused) encoder-side prototypes ---- */
typedef struct vbx264_t vbx264_t;
typedef struct vbx264_frame_t vbx264_frame_t;

typedef struct vbx264_nal_t
{
    int i_ref_idc;
    int i_type;
    int b_long_startcode;
    int i_first_mb;
    int i_last_mb;
    int     i_payload;
    uint8_t *p_payload;
    int i_padding;
} vbx264_nal_t;

/* ---- deblock function table (from upstream common/frame.h) ---- */
typedef void (*vbx264_deblock_inter_t)( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 );
typedef void (*vbx264_deblock_intra_t)( pixel *pix, intptr_t stride, int alpha, int beta );
typedef struct
{
    vbx264_deblock_inter_t deblock_luma[2];
    vbx264_deblock_inter_t deblock_chroma[2];
    vbx264_deblock_inter_t deblock_h_chroma_420;
    vbx264_deblock_inter_t deblock_h_chroma_422;
    vbx264_deblock_intra_t deblock_luma_intra[2];
    vbx264_deblock_intra_t deblock_chroma_intra[2];
    vbx264_deblock_intra_t deblock_h_chroma_420_intra;
    vbx264_deblock_intra_t deblock_h_chroma_422_intra;
    vbx264_deblock_inter_t deblock_luma_mbaff;
    vbx264_deblock_inter_t deblock_chroma_mbaff;
    vbx264_deblock_inter_t deblock_chroma_420_mbaff;
    vbx264_deblock_inter_t deblock_chroma_422_mbaff;
    vbx264_deblock_intra_t deblock_luma_intra_mbaff;
    vbx264_deblock_intra_t deblock_chroma_intra_mbaff;
    vbx264_deblock_intra_t deblock_chroma_420_intra_mbaff;
    vbx264_deblock_intra_t deblock_chroma_422_intra_mbaff;
    void (*deblock_strength)( uint8_t nnz[X264_SCAN8_SIZE], int8_t ref[2][X264_SCAN8_LUMA_SIZE],
                              int16_t mv[2][X264_SCAN8_LUMA_SIZE][2], uint8_t bs[2][8][4], int mvy_limit,
                              int bframe );
} vbx264_deblock_function_t;

void vbx264_deblock_init( uint32_t cpu, vbx264_deblock_function_t *pf, int b_mbaff );

/* ---- cabac context tables (from upstream common/tables.c, see vb_tables.c) ---- */
extern const int8_t  vbx264_cabac_context_init_I[1024][2];
extern const int8_t  vbx264_cabac_context_init_PB[3][1024][2];
extern const uint8_t vbx264_cabac_range_lps[64][4];
extern const uint8_t vbx264_cabac_transition[128][2];
extern const uint8_t vbx264_cabac_renorm_shift[64];
extern const uint8_t vbx264_hpel_ref0[16];
extern const uint8_t vbx264_hpel_ref1[16];
extern const uint8_t vbx264_decimate_table4[16];
extern const uint8_t vbx264_decimate_table8[64];
extern const uint16_t vbx264_cabac_entropy[128];

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

#endif // VB_SHIM_H
