/*****************************************************************************
 * bench_x264.c: VideoBench bench module for the ported x264 C kernels.
 *
 * Implements int vbench_codec_x264( int cpu_ref, int cpu_new ) per PORTING.md.
 * Registers a representative set of the ported vbx264_* C kernels with
 * set_func_name( "x264: ..." ) + call_c(...).
 *****************************************************************************/

#include "common.h"
#include "osdep.h"
#include "bench.h"

/* Let the copied upstream headers coexist with bench.h (same PIXEL_* enum /
 * bs_t / vlc_large_t definitions): bench.h already provided them. */
#define VB_SHIM_PIXEL_ENUM 1
#define VB_SHIM_BS_TYPES 1
#include "vb_shim.h"

/* harness globals (defined once in src/bench.c / src/main.c, per PORTING.md) */
extern char func_name[100];
extern bench_func_t benchs[MAX_FUNCS];
extern int bench_pattern_len;
extern const char *bench_pattern;
extern uint8_t *buf1, *buf2, *buf3, *buf4;
extern pixel *pbuf1, *pbuf2, *pbuf3, *pbuf4;
bench_t *get_bench( const char *name, int cpu );
#ifndef set_func_name
#define set_func_name(...) snprintf( func_name, sizeof(func_name), __VA_ARGS__ )
#endif

/* quant kernels are static upstream and their init was dropped with the
 * encoder-coupled x264_quant_init, so they are exported directly */
int  vbx264_quant_4x4_c( dctcoef dct[16], udctcoef mf[16], udctcoef bias[16] );
int  vbx264_quant_8x8_c( dctcoef dct[64], udctcoef mf[64], udctcoef bias[64] );
void vbx264_dequant_4x4_c( dctcoef dct[16], int dequant_mf[6][16], int i_qp );
void vbx264_dequant_8x8_c( dctcoef dct[64], int dequant_mf[6][64], int i_qp );

/* encode a small block of cabac decisions+bypasses and flush (idempotent) */
void vbx264_cabac_init( vbx264_t *h );
static void vb_cabac_encode_block( vbx264_cabac_t *cb, uint8_t *dst, const uint8_t *src, int n )
{
    vbx264_cabac_encode_init( cb, dst, dst+0xff0 );
    for( int i = 0; i < n; i++ )
    {
        vbx264_cabac_encode_decision_c( cb, src[i]&63, (src[i]>>6)&1 );
        vbx264_cabac_encode_bypass_c( cb, src[i]&1 );
    }
    vbx264_cabac_encode_terminal_c( cb );
    vbx264_cabac_encode_flush( NULL, cb );
}

int vbench_codec_x264( int cpu_ref, int cpu_new )
{
    (void)cpu_ref; (void)cpu_new;

    vbx264_pixel_function_t pixf;
    vbx264_mc_functions_t mcf;
    vbx264_dct_function_t dctf;
    vbx264_deblock_function_t dbf;
    vbx264_bitstream_function_t bsf;
    vbx264_predict_t pred16[7], pred4x4[12], pred8x8c[7];

    vbx264_pixel_init( 0, &pixf );
    vbx264_mc_init( 0, &mcf, 1 );
    vbx264_dct_init( 0, &dctf );
    vbx264_deblock_init( 0, &dbf, 0 );
    vbx264_bitstream_init( 0, &bsf );
    vbx264_predict_16x16_init( 0, pred16 );
    vbx264_predict_4x4_init( 0, pred4x4 );
    vbx264_predict_8x8c_init( 0, pred8x8c );

    ALIGNED_16( int scores[4] );

    /* pixel metrics */
    set_func_name( "x264: sad 16x16" );
    call_c( pixf.sad[PIXEL_16x16], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: sad 8x8" );
    call_c( pixf.sad[PIXEL_8x8], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: sad 4x4" );
    call_c( pixf.sad[PIXEL_4x4], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: ssd 16x16" );
    call_c( pixf.ssd[PIXEL_16x16], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: ssd 8x8" );
    call_c( pixf.ssd[PIXEL_8x8], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: ssd 4x4" );
    call_c( pixf.ssd[PIXEL_4x4], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: satd 16x16" );
    call_c( pixf.satd[PIXEL_16x16], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: satd 8x8" );
    call_c( pixf.satd[PIXEL_8x8], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: satd 4x4" );
    call_c( pixf.satd[PIXEL_4x4], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: sad_x3 16x16" );
    call_c( pixf.sad_x3[PIXEL_16x16], pbuf1, pbuf2, pbuf2+6, pbuf2+1, (intptr_t)FDEC_STRIDE, scores );
    set_func_name( "x264: sad_x4 16x16" );
    call_c( pixf.sad_x4[PIXEL_16x16], pbuf1, pbuf2, pbuf2+6, pbuf2+1, pbuf2+10, (intptr_t)FDEC_STRIDE, scores );

    /* mc copy 16x16 */
    set_func_name( "x264: copy 16x16" );
    call_c( mcf.copy[PIXEL_16x16], pbuf3, (intptr_t)FDEC_STRIDE, pbuf1, (intptr_t)FENC_STRIDE, 16 );

    /* mc luma hpel/qpel 16x16 */
    pixel *src2[4] = { pbuf1+3*64+2, pbuf1+5*64+2, pbuf1+7*64+2, pbuf1+9*64+2 };
    const vbx264_weight_t *weight = vbx264_weight_none;
    set_func_name( "x264: mc_luma hpel 16x16" );
    call_c( mcf.mc_luma, pbuf3, (intptr_t)FDEC_STRIDE, src2, (intptr_t)64, 2, 2, 16, 16, weight );
    set_func_name( "x264: mc_luma qpel 16x16" );
    call_c( mcf.mc_luma, pbuf3, (intptr_t)FDEC_STRIDE, src2, (intptr_t)64, 5, 3, 16, 16, weight );

    /* mc chroma 8x8 */
    set_func_name( "x264: mc_chroma 8x8" );
    call_c( mcf.mc_chroma, pbuf3, pbuf3+8, (intptr_t)16, pbuf1+2*64+2, (intptr_t)64, 2, 4, 8, 8 );

    /* hpel filter */
    ALIGNED_16( int16_t hbuf[16*16] );
    set_func_name( "x264: hpel_filter 16x16" );
    call_c( mcf.hpel_filter, pbuf3, pbuf3+16*FDEC_STRIDE, pbuf3+2*16*FDEC_STRIDE, pbuf1+2*64+2,
            (intptr_t)64, 16, 16, hbuf );

    /* dct / idct */
    ALIGNED_16( dctcoef dct4[16] );
    ALIGNED_16( dctcoef dct8[64] );
    ALIGNED_16( dctcoef dct16[16][16] );
    set_func_name( "x264: sub4x4_dct" );
    call_c( dctf.sub4x4_dct, dct4, pbuf1, pbuf2 );
    set_func_name( "x264: add4x4_idct" );
    call_c( dctf.add4x4_idct, pbuf3, dct4 );
    set_func_name( "x264: sub8x8_dct8" );
    call_c( dctf.sub8x8_dct8, dct8, pbuf1, pbuf2 );
    set_func_name( "x264: add8x8_idct8" );
    call_c( dctf.add8x8_idct8, pbuf3, dct8 );
    set_func_name( "x264: sub16x16_dct" );
    call_c( dctf.sub16x16_dct, dct16, pbuf1, pbuf2 );
    set_func_name( "x264: add16x16_idct" );
    call_c( dctf.add16x16_idct, pbuf3, dct16 );

    /* quant / dequant */
    ALIGNED_16( udctcoef qmf4[16] );
    ALIGNED_16( udctcoef qbias4[16] );
    ALIGNED_16( udctcoef qmf8[64] );
    ALIGNED_16( udctcoef qbias8[64] );
    ALIGNED_16( int dqmf4[6][16] );
    ALIGNED_16( int dqmf8[6][64] );
    for( int i = 0; i < 16; i++ ) { qmf4[i] = 13; qbias4[i] = 6; dqmf4[0][i] = 40; }
    for( int i = 0; i < 64; i++ ) { qmf8[i] = 13; qbias8[i] = 6; dqmf8[0][i] = 40; }
    set_func_name( "x264: quant 4x4" );
    call_c( vbx264_quant_4x4_c, dct4, qmf4, qbias4 );
    set_func_name( "x264: quant 8x8" );
    call_c( vbx264_quant_8x8_c, dct8, qmf8, qbias8 );
    set_func_name( "x264: dequant 4x4" );
    call_c( vbx264_dequant_4x4_c, dct4, dqmf4, 26 );
    set_func_name( "x264: dequant 8x8" );
    call_c( vbx264_dequant_8x8_c, dct8, dqmf8, 26 );

    /* intra predict */
    set_func_name( "x264: predict 16x16 dc" );
    call_c( pred16[I_PRED_16x16_DC], pbuf3+48 );
    set_func_name( "x264: predict 16x16 v" );
    call_c( pred16[I_PRED_16x16_V], pbuf3+48 );
    set_func_name( "x264: predict 16x16 h" );
    call_c( pred16[I_PRED_16x16_H], pbuf3+48 );
    set_func_name( "x264: predict 16x16 p" );
    call_c( pred16[I_PRED_16x16_P], pbuf3+48 );
    set_func_name( "x264: predict 4x4 v" );
    call_c( pred4x4[I_PRED_4x4_V], pbuf3+48 );
    set_func_name( "x264: predict 4x4 h" );
    call_c( pred4x4[I_PRED_4x4_H], pbuf3+48 );
    set_func_name( "x264: predict 4x4 dc" );
    call_c( pred4x4[I_PRED_4x4_DC], pbuf3+48 );
    set_func_name( "x264: predict 8x8c dc" );
    call_c( pred8x8c[I_PRED_8x8_DC], pbuf3+48 );

    /* deblock */
    ALIGNED_16( int8_t tc0[4] ) = { 0, 1, 2, 3 };
    set_func_name( "x264: deblock luma h" );
    call_c( dbf.deblock_luma[0], pbuf3, (intptr_t)FDEC_STRIDE, 30, 20, tc0 );
    set_func_name( "x264: deblock luma v" );
    call_c( dbf.deblock_luma[1], pbuf3+1, (intptr_t)FDEC_STRIDE, 30, 20, tc0 );
    set_func_name( "x264: deblock chroma v" );
    call_c( dbf.deblock_chroma[1], pbuf3+1, (intptr_t)FDEC_STRIDE, 30, 20, tc0 );
    set_func_name( "x264: deblock luma intra h" );
    call_c( dbf.deblock_luma_intra[0], pbuf3, (intptr_t)FDEC_STRIDE, 30, 20 );
    set_func_name( "x264: deblock chroma intra v" );
    call_c( dbf.deblock_chroma_intra[1], pbuf3+1, (intptr_t)FDEC_STRIDE, 30, 20 );
    ALIGNED_16( uint8_t nnz[SCAN8_SIZE] );
    ALIGNED_16( int8_t dref[2][SCAN8_LUMA_SIZE] );
    ALIGNED_16( int16_t dmv[2][SCAN8_LUMA_SIZE][2] );
    ALIGNED_16( uint8_t bs[2][8][4] );
    memset( nnz, 3, sizeof(nnz) );
    memset( dref, 1, sizeof(dref) );
    memset( dmv, 0, sizeof(dmv) );
    memset( bs, 0, sizeof(bs) );
    set_func_name( "x264: deblock_strength" );
    call_c( dbf.deblock_strength, nnz, dref, dmv, bs, 32, 0 );

    /* cabac encode of a small block */
    static vbx264_cabac_t cb;
    static uint8_t cabac_dst[0x1000];
    vbx264_cabac_init( NULL );
    vbx264_cabac_context_init( NULL, &cb, SLICE_TYPE_P, 26, 0 );
    set_func_name( "x264: cabac encode block" );
    call_c( vb_cabac_encode_block, &cb, cabac_dst, buf1, 64 );

    /* bitstream nal escape */
    set_func_name( "x264: nal_escape" );
    call_c( bsf.nal_escape, buf3, buf1, buf1+0x400 );

    return 0;
}
