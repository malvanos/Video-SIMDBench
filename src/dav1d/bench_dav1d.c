/*
 * bench_dav1d.c: VideoBench bench module for the ported dav1d 8bpc C kernels.
 *
 * The dav1d kernels themselves are static inside the *_tmpl.inc files; the
 * only exported symbols are the vbdav1d_*_dsp_init_8bpc() functions, which
 * fill a Dav1d*DSPContext with pointers to the C kernels (same approach as
 * dav1d's own tests/checkasm). So we init each context once and benchmark
 * through the context, following dav1d's calling conventions.
 *
 * Buffers (from the harness, see PORTING.md): pbuf1/pbuf2 are random
 * read-only inputs, pbuf3/pbuf4 are scratch outputs. All scratch use is kept
 * within 0x1000 bytes of each scratch buffer.
 */

#include "common.h"
#include "osdep.h"
#include "bench.h"

/* dav1d DSP headers, mirrored in this directory with dav1d_ -> vbdav1d_
 * symbol prefixing. BITDEPTH=8 selects the _8bpc instantiations and the
 * uint8_t-pixel ABI (HIGHBD_DECL_SUFFIX is empty for 8bpc). */
#define BITDEPTH 8
#include "src/levels.h"
#include "src/mc.h"
#include "src/loopfilter.h"
#include "src/lf_mask.h"
#include "src/cdef.h"
#include "src/ipred.h"
#include "src/itx.h"

static Dav1dMCDSPContext mc_ctx;
static Dav1dLoopFilterDSPContext lf_ctx;
static Dav1dCdefDSPContext cdef_ctx;
static Dav1dIntraPredDSPContext ipred_ctx;
static Dav1dInvTxfmDSPContext itx_ctx;

/* Scalar ("C-plain") rebuild of the same kernels: the kernels_dav1d_scalar
 * lib is compiled from renamed copies of the same sources (vbdav1d_ ->
 * vbdav1ds_), so the context struct layouts are identical and only the init
 * symbol prefix differs. */
static Dav1dMCDSPContext mc_ctx_s;
static Dav1dLoopFilterDSPContext lf_ctx_s;
static Dav1dCdefDSPContext cdef_ctx_s;
static Dav1dIntraPredDSPContext ipred_ctx_s;
static Dav1dInvTxfmDSPContext itx_ctx_s;
void vbdav1ds_mc_dsp_init_8bpc( Dav1dMCDSPContext *c );
void vbdav1ds_loop_filter_dsp_init_8bpc( Dav1dLoopFilterDSPContext *c );
void vbdav1ds_cdef_dsp_init_8bpc( Dav1dCdefDSPContext *c );
void vbdav1ds_intra_pred_dsp_init_8bpc( Dav1dIntraPredDSPContext *c );
void vbdav1ds_itx_dsp_init_8bpc( Dav1dInvTxfmDSPContext *c, int bpc );

/* mc: 8tap reads 3 rows/cols around the block, so give src a 3px margin.
 * Worst case (32x32 hv) touches pbuf1[0 .. 37*32+38] < 0xf00. */
#define MC_SRC        (pbuf1 + 3*32 + 3)
#define MC_SRC_STRIDE 32
#define MC_DST_STRIDE 32
/* warp8x8 samples a 15x15 area starting 3 rows/cols above-left of src */
#define WARP_SRC        (pbuf1 + 3*16 + 3)
#define WARP_SRC_STRIDE 16
static const int16_t warp_abcd[4] = { 0x0640, -0x01e0, 0x02d0, -0x0510 };

/* loop filter: n_blks 4px blocks per sb row; dst margins like checkasm */
#define LF_NBLKS      16
#define LF_DST_H      (pbuf3 + 128)              /* dir=0: w=16,  h=64, stride 16 */
#define LF_DST_V      (pbuf3 + 8*LF_NBLKS*4)     /* dir=1: w=64,  h=16, stride 64 */
static Av1FilterLUT lf_lut;
static uint32_t lf_vmask[4];
static uint8_t lf_lvl[128][2];

/* cdef 8x8: dst 16x10 area with dst at +8, top/bottom 2 rows, left 8x2 */
#define CDEF_DST  (pbuf3 + 8)
#define CDEF_TOP  (pbuf3 + 264)
#define CDEF_BOT  (pbuf3 + 328)
#define CDEF_LEFT ((const pixel (*)[2])(pbuf3 + 384))

/* intra pred: topleft has a 128px margin each side, dst stride 64 */
#define IPRED_TOPLEFT (pbuf2 + 128)
#define IPRED_STRIDE  64

/* itx: dst in pbuf3 (stride 32), coefficients in pbuf4 */
#define ITX_COEFF ((coef *)pbuf4)
#define ITX_STRIDE 32

static void dav1d_bench_init( void )
{
    static int inited = 0;
    if( !inited )
    {
        inited = 1;
        vbdav1d_mc_dsp_init_8bpc( &mc_ctx );
        vbdav1d_loop_filter_dsp_init_8bpc( &lf_ctx );
        vbdav1d_cdef_dsp_init_8bpc( &cdef_ctx );
        vbdav1d_intra_pred_dsp_init_8bpc( &ipred_ctx );
        vbdav1d_itx_dsp_init_8bpc( &itx_ctx, 8 );
        vbdav1ds_mc_dsp_init_8bpc( &mc_ctx_s );
        vbdav1ds_loop_filter_dsp_init_8bpc( &lf_ctx_s );
        vbdav1ds_cdef_dsp_init_8bpc( &cdef_ctx_s );
        vbdav1ds_intra_pred_dsp_init_8bpc( &ipred_ctx_s );
        vbdav1ds_itx_dsp_init_8bpc( &itx_ctx_s, 8 );
    }

    /* refill per-run inputs (buffers shift with each alignment iteration) */
    for( int level = 0; level < 64; level++ )
    {
        const int limit = MAX( level, 1 ); /* sharpness 0 */
        lf_lut.i[level] = limit;
        lf_lut.e[level] = 2 * (level + 2) + limit;
    }
    lf_lut.sharp[0] = 0;
    lf_lut.sharp[1] = 0xff;
    for( int i = 0; i < 4; i++ )
        lf_vmask[i] = rand();
    for( int i = 0; i < 128; i++ )
    {
        lf_lvl[i][0] = rand() & 63;
        lf_lvl[i][1] = rand() & 63;
    }
    for( int i = 0; i < 16*16; i++ )
        ITX_COEFF[i] = (coef)rand();
}

/* time the vec (-O3) and scalar (C-plain, no auto-vec) builds of the same
 * kernel under the C and C-plain columns (all dav1d kernels return void) */
#define call_c_cp_void(f_v, f_s, ...) \
    do { call_c( f_v, __VA_ARGS__ ); call_cp( f_s, __VA_ARGS__ ); } while(0)

int vbench_codec_dav1d( int cpu_ref, int cpu_new )
{
    (void)cpu_ref; (void)cpu_new;
    dav1d_bench_init();

    /* --- motion compensation --- */
    set_func_name( "dav1d: mc put_bilin 16x16" );
    call_c_cp_void( mc_ctx.mc[FILTER_2D_BILINEAR], mc_ctx_s.mc[FILTER_2D_BILINEAR], pbuf3, MC_DST_STRIDE, MC_SRC, MC_SRC_STRIDE, 16, 16, 6, 10 );

    set_func_name( "dav1d: mc put_8tap_regular 8x8" );
    call_c_cp_void( mc_ctx.mc[FILTER_2D_8TAP_REGULAR], mc_ctx_s.mc[FILTER_2D_8TAP_REGULAR], pbuf3, MC_DST_STRIDE, MC_SRC, MC_SRC_STRIDE, 8, 8, 4, 9 );
    set_func_name( "dav1d: mc put_8tap_regular 16x16" );
    call_c_cp_void( mc_ctx.mc[FILTER_2D_8TAP_REGULAR], mc_ctx_s.mc[FILTER_2D_8TAP_REGULAR], pbuf3, MC_DST_STRIDE, MC_SRC, MC_SRC_STRIDE, 16, 16, 4, 9 );
    set_func_name( "dav1d: mc put_8tap_regular 32x32" );
    call_c_cp_void( mc_ctx.mc[FILTER_2D_8TAP_REGULAR], mc_ctx_s.mc[FILTER_2D_8TAP_REGULAR], pbuf3, MC_DST_STRIDE, MC_SRC, MC_SRC_STRIDE, 32, 32, 4, 9 );
    set_func_name( "dav1d: mc put_8tap_sharp 16x16" );
    call_c_cp_void( mc_ctx.mc[FILTER_2D_8TAP_SHARP], mc_ctx_s.mc[FILTER_2D_8TAP_SHARP], pbuf3, MC_DST_STRIDE, MC_SRC, MC_SRC_STRIDE, 16, 16, 7, 3 );

    set_func_name( "dav1d: mc prep_8tap_regular 8x8" );
    call_c_cp_void( mc_ctx.mct[FILTER_2D_8TAP_REGULAR], mc_ctx_s.mct[FILTER_2D_8TAP_REGULAR], (int16_t *)pbuf4, MC_SRC, MC_SRC_STRIDE, 8, 8, 4, 9 );
    set_func_name( "dav1d: mc prep_8tap_regular 16x16" );
    call_c_cp_void( mc_ctx.mct[FILTER_2D_8TAP_REGULAR], mc_ctx_s.mct[FILTER_2D_8TAP_REGULAR], (int16_t *)pbuf4, MC_SRC, MC_SRC_STRIDE, 16, 16, 4, 9 );
    set_func_name( "dav1d: mc prep_8tap_sharp 16x16" );
    call_c_cp_void( mc_ctx.mct[FILTER_2D_8TAP_SHARP], mc_ctx_s.mct[FILTER_2D_8TAP_SHARP], (int16_t *)pbuf4, MC_SRC, MC_SRC_STRIDE, 16, 16, 7, 3 );

    set_func_name( "dav1d: mc warp_8x8" );
    call_c_cp_void( mc_ctx.warp8x8, mc_ctx_s.warp8x8, pbuf3, 16, WARP_SRC, WARP_SRC_STRIDE, warp_abcd, 0x3c0, -0x2a0 );

    /* --- loop filter (luma/chroma, horizontal/vertical edge) --- */
    set_func_name( "dav1d: lf luma h edge" );
    call_c_cp_void( lf_ctx.loop_filter_sb[0][0], lf_ctx_s.loop_filter_sb[0][0], LF_DST_H, 16, lf_vmask,
            (const uint8_t (*)[2]) &lf_lvl[1][0], 2, &lf_lut, LF_NBLKS );
    set_func_name( "dav1d: lf luma v edge" );
    call_c_cp_void( lf_ctx.loop_filter_sb[0][1], lf_ctx_s.loop_filter_sb[0][1], LF_DST_V, LF_NBLKS*4, lf_vmask,
            (const uint8_t (*)[2]) &lf_lvl[32][0], 32, &lf_lut, LF_NBLKS );
    set_func_name( "dav1d: lf chroma h edge" );
    call_c_cp_void( lf_ctx.loop_filter_sb[1][0], lf_ctx_s.loop_filter_sb[1][0], LF_DST_H, 16, lf_vmask,
            (const uint8_t (*)[2]) &lf_lvl[1][0], 2, &lf_lut, LF_NBLKS );
    set_func_name( "dav1d: lf chroma v edge" );
    call_c_cp_void( lf_ctx.loop_filter_sb[1][1], lf_ctx_s.loop_filter_sb[1][1], LF_DST_V, LF_NBLKS*4, lf_vmask,
            (const uint8_t (*)[2]) &lf_lvl[32][0], 32, &lf_lut, LF_NBLKS );

    /* --- cdef --- */
    set_func_name( "dav1d: cdef 8x8" );
    call_c_cp_void( cdef_ctx.fb[0], cdef_ctx_s.fb[0], CDEF_DST, 16, CDEF_LEFT, CDEF_TOP, CDEF_BOT,
            3, 2, 2, 4, (enum CdefEdgeFlags)(CDEF_HAVE_LEFT|CDEF_HAVE_RIGHT|CDEF_HAVE_TOP|CDEF_HAVE_BOTTOM) );
    set_func_name( "dav1d: cdef 4x4 420" );
    call_c_cp_void( cdef_ctx.fb[2], cdef_ctx_s.fb[2], CDEF_DST, 16, CDEF_LEFT, CDEF_TOP, CDEF_BOT,
            3, 2, 5, 4, (enum CdefEdgeFlags)(CDEF_HAVE_LEFT|CDEF_HAVE_RIGHT|CDEF_HAVE_TOP|CDEF_HAVE_BOTTOM) );

    /* --- intra prediction --- */
    set_func_name( "dav1d: ipred dc 4x4" );
    call_c_cp_void( ipred_ctx.intra_pred[DC_PRED], ipred_ctx_s.intra_pred[DC_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 4, 4, 0, 64, 64 );
    set_func_name( "dav1d: ipred dc 16x16" );
    call_c_cp_void( ipred_ctx.intra_pred[DC_PRED], ipred_ctx_s.intra_pred[DC_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 16, 16, 0, 64, 64 );
    set_func_name( "dav1d: ipred paeth 8x8" );
    call_c_cp_void( ipred_ctx.intra_pred[PAETH_PRED], ipred_ctx_s.intra_pred[PAETH_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 8, 8, 0, 64, 64 );
    set_func_name( "dav1d: ipred paeth 16x16" );
    call_c_cp_void( ipred_ctx.intra_pred[PAETH_PRED], ipred_ctx_s.intra_pred[PAETH_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 16, 16, 0, 64, 64 );
    set_func_name( "dav1d: ipred smooth 8x8" );
    call_c_cp_void( ipred_ctx.intra_pred[SMOOTH_PRED], ipred_ctx_s.intra_pred[SMOOTH_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 8, 8, 0, 64, 64 );
    set_func_name( "dav1d: ipred smooth 16x16" );
    call_c_cp_void( ipred_ctx.intra_pred[SMOOTH_PRED], ipred_ctx_s.intra_pred[SMOOTH_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 16, 16, 0, 64, 64 );
    set_func_name( "dav1d: ipred z2 4x4" );
    call_c_cp_void( ipred_ctx.intra_pred[Z2_PRED], ipred_ctx_s.intra_pred[Z2_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 4, 4, 122, 64, 64 );
    set_func_name( "dav1d: ipred z2 8x8" );
    call_c_cp_void( ipred_ctx.intra_pred[Z2_PRED], ipred_ctx_s.intra_pred[Z2_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 8, 8, 122, 64, 64 );
    set_func_name( "dav1d: ipred z2 16x16" );
    call_c_cp_void( ipred_ctx.intra_pred[Z2_PRED], ipred_ctx_s.intra_pred[Z2_PRED], pbuf3, IPRED_STRIDE, IPRED_TOPLEFT, 16, 16, 122, 64, 64 );

    /* --- inverse transform add --- */
    set_func_name( "dav1d: itx add dct_dct 4x4" );
    call_c_cp_void( itx_ctx.itxfm_add[TX_4X4][DCT_DCT], itx_ctx_s.itxfm_add[TX_4X4][DCT_DCT], pbuf3, ITX_STRIDE, ITX_COEFF, 16 );
    set_func_name( "dav1d: itx add dct_dct 8x8" );
    call_c_cp_void( itx_ctx.itxfm_add[TX_8X8][DCT_DCT], itx_ctx_s.itxfm_add[TX_8X8][DCT_DCT], pbuf3, ITX_STRIDE, ITX_COEFF, 64 );
    set_func_name( "dav1d: itx add dct_dct 16x16" );
    call_c_cp_void( itx_ctx.itxfm_add[TX_16X16][DCT_DCT], itx_ctx_s.itxfm_add[TX_16X16][DCT_DCT], pbuf3, ITX_STRIDE, ITX_COEFF, 256 );

    return 0;
}
