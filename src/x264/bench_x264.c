/*****************************************************************************
 * bench_x264.c: VideoBench bench module for the embedded upstream x264.
 *
 * Implements int vbench_codec_x264( int cpu_ref, int cpu_new ) per PORTING.md.
 * Registers a representative set of the native x264_* kernels with
 * set_func_name( "x264: ..." ) and three measurements per row:
 *   call_c  (pixf_c,  cpu=0)      -> "C" column       (auto-vectorized C)
 *   call_cp (pixf_s,  scalar lib) -> "C-plain" column (-fno-tree-vectorize)
 *   call_a  (pixf_asm, cpu_new)   -> ISA column       (upstream hand-written
 *                                                      NEON/SSE asm, when
 *                                                      the init registered a
 *                                                      different pointer)
 * Kernels that return a value must agree across all three (ret=-1 on
 * mismatch); void kernels are all called (and timed) the same way.
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
int  x264_quant_4x4_c( dctcoef dct[16], udctcoef mf[16], udctcoef bias[16] );
int  x264_quant_8x8_c( dctcoef dct[64], udctcoef mf[64], udctcoef bias[64] );
void x264_dequant_4x4_c( dctcoef dct[16], int dequant_mf[6][16], int i_qp );
void x264_dequant_8x8_c( dctcoef dct[64], int dequant_mf[6][64], int i_qp );

/* NEON quant kernels (aarch64/quant-a.S), when the aarch64 asm is built.
 * The asm symbols carry the bit-depth infix like every x264_template name. */
#if HAVE_AARCH64
int  x264_template(quant_4x4_neon)( dctcoef dct[16], udctcoef mf[16], udctcoef bias[16] );
int  x264_template(quant_8x8_neon)( dctcoef dct[64], udctcoef mf[64], udctcoef bias[64] );
void x264_template(dequant_4x4_neon)( dctcoef dct[16], int dequant_mf[6][16], int i_qp );
void x264_template(dequant_8x8_neon)( dctcoef dct[64], int dequant_mf[6][64], int i_qp );
#define VB_X264_QUANT_NEON 1
#endif

/* Scalar ("C-plain") rebuild of the same kernels: the kernels_x264_scalar lib
 * is compiled from renamed copies of the same sources (x264_ -> x264s_),
 * so every struct layout is identical and only the global symbol prefix
 * differs — including the bit-depth templating, i.e. the scalar entry points
 * are x264s_<BIT_DEPTH>_<name>. Declare them against the vec types above. */
#define x264s_template3(bd, w) x264s_##bd##_##w
#define x264s_template2(bd, w) x264s_template3(bd, w)
#define x264s_template(w) x264s_template2(BIT_DEPTH, w)
void x264s_template(pixel_init)( uint32_t cpu, x264_pixel_function_t *pixf );
void x264s_template(mc_init)( uint32_t cpu, x264_mc_functions_t *pf, int cpu_independent );
void x264s_template(dct_init)( uint32_t cpu, x264_dct_function_t *dctf );
void x264s_deblock_init( uint32_t cpu, x264_deblock_function_t *pf, int b_mbaff );
void x264s_template(bitstream_init)( uint32_t cpu, x264_bitstream_function_t *pf );
void x264s_template(predict_16x16_init)( uint32_t cpu, x264_predict_t pf[7] );
void x264s_template(predict_4x4_init)( uint32_t cpu, x264_predict_t pf[12] );
void x264s_template(predict_8x8c_init)( uint32_t cpu, x264_predict_t pf[7] );
int  x264s_quant_4x4_c( dctcoef dct[16], udctcoef mf[16], udctcoef bias[16] );
int  x264s_quant_8x8_c( dctcoef dct[64], udctcoef mf[64], udctcoef bias[64] );
void x264s_dequant_4x4_c( dctcoef dct[16], int dequant_mf[6][16], int i_qp );
void x264s_dequant_8x8_c( dctcoef dct[64], int dequant_mf[6][64], int i_qp );
void x264s_cabac_init( x264_t *h );
void x264s_template(cabac_context_init)( x264_t *h, x264_cabac_t *cb, int i_slice_type, int i_qp, int i_model );
void x264s_template(cabac_encode_init)( x264_cabac_t *cb, uint8_t *p_data, uint8_t *p_end );
void x264s_template(cabac_encode_decision_c)( x264_cabac_t *cb, int i_ctx, int b );
void x264s_template(cabac_encode_bypass_c)( x264_cabac_t *cb, int b );
void x264s_template(cabac_encode_terminal_c)( x264_cabac_t *cb );
void x264s_template(cabac_encode_flush)( x264_t *h, x264_cabac_t *cb );

/* encode a small block of cabac decisions+bypasses and flush (idempotent) */
void x264_cabac_init( x264_t *h );
static void vb_cabac_encode_block( x264_cabac_t *cb, uint8_t *dst, const uint8_t *src, int n )
{
    x264_cabac_encode_init( cb, dst, dst+0xff0 );
    for( int i = 0; i < n; i++ )
    {
        x264_cabac_encode_decision_c( cb, src[i]&63, (src[i]>>6)&1 );
        x264_cabac_encode_bypass_c( cb, src[i]&1 );
    }
    x264_cabac_encode_terminal_c( cb );
    x264_cabac_encode_flush( NULL, cb );
}

/* same, through the scalar (C-plain) build of the cabac kernels */
static void vb_cabac_encode_block_s( x264_cabac_t *cb, uint8_t *dst, const uint8_t *src, int n )
{
    x264s_template(cabac_encode_init)( cb, dst, dst+0xff0 );
    for( int i = 0; i < n; i++ )
    {
        x264s_template(cabac_encode_decision_c)( cb, src[i]&63, (src[i]>>6)&1 );
        x264s_template(cabac_encode_bypass_c)( cb, src[i]&1 );
    }
    x264s_template(cabac_encode_terminal_c)( cb );
    x264s_template(cabac_encode_flush)( NULL, cb );
}

/* same, through the upstream hand-written asm cabac kernels (aarch64/cabac-a.S,
 * x86/cabac-a.asm) — available whenever the asm is built */
#if HAVE_AARCH64 || HAVE_MMX
#define VB_X264_CABAC_ASM 1
static void vb_cabac_encode_block_a( x264_cabac_t *cb, uint8_t *dst, const uint8_t *src, int n )
{
    x264_cabac_encode_init( cb, dst, dst+0xff0 );
    for( int i = 0; i < n; i++ )
    {
        x264_cabac_encode_decision_asm( cb, src[i]&63, (src[i]>>6)&1 );
        x264_cabac_encode_bypass_asm( cb, src[i]&1 );
    }
    x264_cabac_encode_terminal_asm( cb );
    x264_cabac_encode_flush( NULL, cb );
}
#endif

/* bench the vec (-O3), scalar (no auto-vec) and asm builds of the same kernel.
 * Kernels that return a value must agree across all three, like the old
 * harness check_* modules compared C vs asm (call_c/call_cp/call_a evaluate
 * to the single call's return value). */
#define call_c_cp(f_v, f_s, ...) \
    do { if( call_c( f_v, __VA_ARGS__ ) != call_cp( f_s, __VA_ARGS__ ) ) ret = -1; } while(0)
#define call_c_cp_void(f_v, f_s, ...) \
    do { call_c( f_v, __VA_ARGS__ ); call_cp( f_s, __VA_ARGS__ ); } while(0)
/* + asm: call_a registers the timing under the cpu_new column, but only when
 * the asm init actually registered a different pointer for this kernel */
#define call_c_cp_a(f_v, f_s, f_a, ...) \
    do { intptr_t r_c = (intptr_t)call_c( f_v, __VA_ARGS__ ); \
         if( r_c != (intptr_t)call_cp( f_s, __VA_ARGS__ ) ) ret = -1; \
         if( (f_a) && (f_a) != (f_v) && (intptr_t)call_a( f_a, __VA_ARGS__ ) != r_c ) ret = -1; } while(0)
#define call_c_cp_a_void(f_v, f_s, f_a, ...) \
    do { call_c( f_v, __VA_ARGS__ ); call_cp( f_s, __VA_ARGS__ ); \
         if( (f_a) && (f_a) != (f_v) ) call_a( f_a, __VA_ARGS__ ); } while(0)

int vbench_codec_x264( int cpu_ref, int cpu_new )
{
    (void)cpu_ref;
    int ret = 0;
    /* direct asm entry points (quant, cabac — not behind an init table) are
     * only invoked on passes whose cpu word asks for the asm ISA, exactly
     * like the table-driven kernels whose init gates on the same flags */
#if HAVE_AARCH64
    int asm_cpu = cpu_new & X264_CPU_NEON;
#elif HAVE_MMX
    int asm_cpu = cpu_new & X264_CPU_MMX2;
#else
    int asm_cpu = 0;
#endif

    /* C (cpu=0), asm (cpu_new: upstream init registers the hand-written asm
     * pointers when the cpu flags ask for them) and scalar (C-plain) tables */
    x264_pixel_function_t pixf_c, pixf_asm, pixf_s;
    x264_mc_functions_t mcf_c, mcf_asm, mcf_s;
    x264_dct_function_t dctf_c, dctf_asm, dctf_s;
    x264_deblock_function_t dbf_c, dbf_asm, dbf_s;
    x264_bitstream_function_t bsf_c, bsf_asm, bsf_s;
    x264_predict_t pred16_c[7], pred16_asm[7], pred16_s[7];
    x264_predict_t pred4x4_c[12], pred4x4_asm[12], pred4x4_s[12];
    x264_predict_t pred8x8c_c[7], pred8x8c_asm[7], pred8x8c_s[7];

    x264_pixel_init( 0, &pixf_c );
    x264_pixel_init( cpu_new, &pixf_asm );
    x264_mc_init( 0, &mcf_c, 1 );
    x264_mc_init( cpu_new, &mcf_asm, 1 );
    x264_dct_init( 0, &dctf_c );
    x264_dct_init( cpu_new, &dctf_asm );
    x264_deblock_init( 0, &dbf_c, 0 );
    x264_deblock_init( cpu_new, &dbf_asm, 0 );
    x264_bitstream_init( 0, &bsf_c );
    x264_bitstream_init( cpu_new, &bsf_asm );
    x264_predict_16x16_init( 0, pred16_c );
    x264_predict_16x16_init( cpu_new, pred16_asm );
    x264_predict_4x4_init( 0, pred4x4_c );
    x264_predict_4x4_init( cpu_new, pred4x4_asm );
    x264_predict_8x8c_init( 0, pred8x8c_c );
    x264_predict_8x8c_init( cpu_new, pred8x8c_asm );

    x264s_template(pixel_init)( 0, &pixf_s );
    x264s_template(mc_init)( 0, &mcf_s, 1 );
    x264s_template(dct_init)( 0, &dctf_s );
    x264s_deblock_init( 0, &dbf_s, 0 );
    x264s_template(bitstream_init)( 0, &bsf_s );
    x264s_template(predict_16x16_init)( 0, pred16_s );
    x264s_template(predict_4x4_init)( 0, pred4x4_s );
    x264s_template(predict_8x8c_init)( 0, pred8x8c_s );

    ALIGNED_16( int scores[4] );

    /* pixel metrics */
    set_func_name( "x264: sad 16x16" );
    call_c_cp_a( pixf_c.sad[PIXEL_16x16], pixf_s.sad[PIXEL_16x16], pixf_asm.sad[PIXEL_16x16], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: sad 8x8" );
    call_c_cp_a( pixf_c.sad[PIXEL_8x8], pixf_s.sad[PIXEL_8x8], pixf_asm.sad[PIXEL_8x8], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: sad 4x4" );
    call_c_cp_a( pixf_c.sad[PIXEL_4x4], pixf_s.sad[PIXEL_4x4], pixf_asm.sad[PIXEL_4x4], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: ssd 16x16" );
    call_c_cp_a( pixf_c.ssd[PIXEL_16x16], pixf_s.ssd[PIXEL_16x16], pixf_asm.ssd[PIXEL_16x16], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: ssd 8x8" );
    call_c_cp_a( pixf_c.ssd[PIXEL_8x8], pixf_s.ssd[PIXEL_8x8], pixf_asm.ssd[PIXEL_8x8], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: ssd 4x4" );
    call_c_cp_a( pixf_c.ssd[PIXEL_4x4], pixf_s.ssd[PIXEL_4x4], pixf_asm.ssd[PIXEL_4x4], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: satd 16x16" );
    call_c_cp_a( pixf_c.satd[PIXEL_16x16], pixf_s.satd[PIXEL_16x16], pixf_asm.satd[PIXEL_16x16], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: satd 8x8" );
    call_c_cp_a( pixf_c.satd[PIXEL_8x8], pixf_s.satd[PIXEL_8x8], pixf_asm.satd[PIXEL_8x8], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: satd 4x4" );
    call_c_cp_a( pixf_c.satd[PIXEL_4x4], pixf_s.satd[PIXEL_4x4], pixf_asm.satd[PIXEL_4x4], pbuf1, (intptr_t)FENC_STRIDE, pbuf2, (intptr_t)FDEC_STRIDE );
    set_func_name( "x264: sad_x3 16x16" );
    call_c_cp_a_void( pixf_c.sad_x3[PIXEL_16x16], pixf_s.sad_x3[PIXEL_16x16], pixf_asm.sad_x3[PIXEL_16x16], pbuf1, pbuf2, pbuf2+6, pbuf2+1, (intptr_t)FDEC_STRIDE, scores );
    set_func_name( "x264: sad_x4 16x16" );
    call_c_cp_a_void( pixf_c.sad_x4[PIXEL_16x16], pixf_s.sad_x4[PIXEL_16x16], pixf_asm.sad_x4[PIXEL_16x16], pbuf1, pbuf2, pbuf2+6, pbuf2+1, pbuf2+10, (intptr_t)FDEC_STRIDE, scores );

    /* mc copy 16x16 */
    set_func_name( "x264: copy 16x16" );
    call_c_cp_a_void( mcf_c.copy[PIXEL_16x16], mcf_s.copy[PIXEL_16x16], mcf_asm.copy[PIXEL_16x16], pbuf3, (intptr_t)FDEC_STRIDE, pbuf1, (intptr_t)FENC_STRIDE, 16 );

    /* mc luma hpel/qpel 16x16 */
    pixel *src2[4] = { pbuf1+3*64+2, pbuf1+5*64+2, pbuf1+7*64+2, pbuf1+9*64+2 };
    const x264_weight_t *weight = x264_weight_none;
    set_func_name( "x264: mc_luma hpel 16x16" );
    call_c_cp_a_void( mcf_c.mc_luma, mcf_s.mc_luma, mcf_asm.mc_luma, pbuf3, (intptr_t)FDEC_STRIDE, src2, (intptr_t)64, 2, 2, 16, 16, weight );
    set_func_name( "x264: mc_luma qpel 16x16" );
    call_c_cp_a_void( mcf_c.mc_luma, mcf_s.mc_luma, mcf_asm.mc_luma, pbuf3, (intptr_t)FDEC_STRIDE, src2, (intptr_t)64, 5, 3, 16, 16, weight );

    /* mc chroma 8x8 */
    set_func_name( "x264: mc_chroma 8x8" );
    call_c_cp_a_void( mcf_c.mc_chroma, mcf_s.mc_chroma, mcf_asm.mc_chroma, pbuf3, pbuf3+8, (intptr_t)16, pbuf1+2*64+2, (intptr_t)64, 2, 4, 8, 8 );

    /* hpel filter */
    ALIGNED_16( int16_t hbuf[16*16] );
    set_func_name( "x264: hpel_filter 16x16" );
    call_c_cp_a_void( mcf_c.hpel_filter, mcf_s.hpel_filter, mcf_asm.hpel_filter, pbuf3, pbuf3+16*FDEC_STRIDE, pbuf3+2*16*FDEC_STRIDE, pbuf1+2*64+2,
            (intptr_t)64, 16, 16, hbuf );

    /* dct / idct */
    ALIGNED_16( dctcoef dct4[16] );
    ALIGNED_16( dctcoef dct8[64] );
    ALIGNED_16( dctcoef dct16[16][16] );
    set_func_name( "x264: sub4x4_dct" );
    call_c_cp_a_void( dctf_c.sub4x4_dct, dctf_s.sub4x4_dct, dctf_asm.sub4x4_dct, dct4, pbuf1, pbuf2 );
    set_func_name( "x264: add4x4_idct" );
    call_c_cp_a_void( dctf_c.add4x4_idct, dctf_s.add4x4_idct, dctf_asm.add4x4_idct, pbuf3, dct4 );
    set_func_name( "x264: sub8x8_dct8" );
    call_c_cp_a_void( dctf_c.sub8x8_dct8, dctf_s.sub8x8_dct8, dctf_asm.sub8x8_dct8, dct8, pbuf1, pbuf2 );
    set_func_name( "x264: add8x8_idct8" );
    call_c_cp_a_void( dctf_c.add8x8_idct8, dctf_s.add8x8_idct8, dctf_asm.add8x8_idct8, pbuf3, dct8 );
    set_func_name( "x264: sub16x16_dct" );
    call_c_cp_a_void( dctf_c.sub16x16_dct, dctf_s.sub16x16_dct, dctf_asm.sub16x16_dct, dct16, pbuf1, pbuf2 );
    set_func_name( "x264: add16x16_idct" );
    call_c_cp_a_void( dctf_c.add16x16_idct, dctf_s.add16x16_idct, dctf_asm.add16x16_idct, pbuf3, dct16 );

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
#ifdef VB_X264_QUANT_NEON
    call_c_cp_a( x264_quant_4x4_c, x264s_quant_4x4_c, asm_cpu ? x264_template(quant_4x4_neon) : x264_quant_4x4_c, dct4, qmf4, qbias4 );
#else
    call_c_cp( x264_quant_4x4_c, x264s_quant_4x4_c, dct4, qmf4, qbias4 );
#endif
    set_func_name( "x264: quant 8x8" );
#ifdef VB_X264_QUANT_NEON
    call_c_cp_a( x264_quant_8x8_c, x264s_quant_8x8_c, asm_cpu ? x264_template(quant_8x8_neon) : x264_quant_8x8_c, dct8, qmf8, qbias8 );
#else
    call_c_cp( x264_quant_8x8_c, x264s_quant_8x8_c, dct8, qmf8, qbias8 );
#endif
    set_func_name( "x264: dequant 4x4" );
#ifdef VB_X264_QUANT_NEON
    call_c_cp_a_void( x264_dequant_4x4_c, x264s_dequant_4x4_c, asm_cpu ? x264_template(dequant_4x4_neon) : x264_dequant_4x4_c, dct4, dqmf4, 26 );
#else
    call_c_cp_void( x264_dequant_4x4_c, x264s_dequant_4x4_c, dct4, dqmf4, 26 );
#endif
    set_func_name( "x264: dequant 8x8" );
#ifdef VB_X264_QUANT_NEON
    call_c_cp_a_void( x264_dequant_8x8_c, x264s_dequant_8x8_c, asm_cpu ? x264_template(dequant_8x8_neon) : x264_dequant_8x8_c, dct8, dqmf8, 26 );
#else
    call_c_cp_void( x264_dequant_8x8_c, x264s_dequant_8x8_c, dct8, dqmf8, 26 );
#endif

    /* intra predict */
    set_func_name( "x264: predict 16x16 dc" );
    call_c_cp_a_void( pred16_c[I_PRED_16x16_DC], pred16_s[I_PRED_16x16_DC], pred16_asm[I_PRED_16x16_DC], pbuf3+48 );
    set_func_name( "x264: predict 16x16 v" );
    call_c_cp_a_void( pred16_c[I_PRED_16x16_V], pred16_s[I_PRED_16x16_V], pred16_asm[I_PRED_16x16_V], pbuf3+48 );
    set_func_name( "x264: predict 16x16 h" );
    call_c_cp_a_void( pred16_c[I_PRED_16x16_H], pred16_s[I_PRED_16x16_H], pred16_asm[I_PRED_16x16_H], pbuf3+48 );
    set_func_name( "x264: predict 16x16 p" );
    call_c_cp_a_void( pred16_c[I_PRED_16x16_P], pred16_s[I_PRED_16x16_P], pred16_asm[I_PRED_16x16_P], pbuf3+48 );
    set_func_name( "x264: predict 4x4 v" );
    call_c_cp_a_void( pred4x4_c[I_PRED_4x4_V], pred4x4_s[I_PRED_4x4_V], pred4x4_asm[I_PRED_4x4_V], pbuf3+48 );
    set_func_name( "x264: predict 4x4 h" );
    call_c_cp_a_void( pred4x4_c[I_PRED_4x4_H], pred4x4_s[I_PRED_4x4_H], pred4x4_asm[I_PRED_4x4_H], pbuf3+48 );
    set_func_name( "x264: predict 4x4 dc" );
    call_c_cp_a_void( pred4x4_c[I_PRED_4x4_DC], pred4x4_s[I_PRED_4x4_DC], pred4x4_asm[I_PRED_4x4_DC], pbuf3+48 );
    set_func_name( "x264: predict 8x8c dc" );
    call_c_cp_a_void( pred8x8c_c[I_PRED_8x8_DC], pred8x8c_s[I_PRED_8x8_DC], pred8x8c_asm[I_PRED_8x8_DC], pbuf3+48 );

    /* deblock */
    ALIGNED_16( int8_t tc0[4] ) = { 0, 1, 2, 3 };
    set_func_name( "x264: deblock luma h" );
    call_c_cp_a_void( dbf_c.deblock_luma[0], dbf_s.deblock_luma[0], dbf_asm.deblock_luma[0], pbuf3, (intptr_t)FDEC_STRIDE, 30, 20, tc0 );
    set_func_name( "x264: deblock luma v" );
    call_c_cp_a_void( dbf_c.deblock_luma[1], dbf_s.deblock_luma[1], dbf_asm.deblock_luma[1], pbuf3+1, (intptr_t)FDEC_STRIDE, 30, 20, tc0 );
    set_func_name( "x264: deblock chroma v" );
    call_c_cp_a_void( dbf_c.deblock_chroma[1], dbf_s.deblock_chroma[1], dbf_asm.deblock_chroma[1], pbuf3+1, (intptr_t)FDEC_STRIDE, 30, 20, tc0 );
    set_func_name( "x264: deblock luma intra h" );
    call_c_cp_a_void( dbf_c.deblock_luma_intra[0], dbf_s.deblock_luma_intra[0], dbf_asm.deblock_luma_intra[0], pbuf3, (intptr_t)FDEC_STRIDE, 30, 20 );
    set_func_name( "x264: deblock chroma intra v" );
    call_c_cp_a_void( dbf_c.deblock_chroma_intra[1], dbf_s.deblock_chroma_intra[1], dbf_asm.deblock_chroma_intra[1], pbuf3+1, (intptr_t)FDEC_STRIDE, 30, 20 );
    ALIGNED_16( uint8_t nnz[SCAN8_SIZE] );
    ALIGNED_16( int8_t dref[2][SCAN8_LUMA_SIZE] );
    ALIGNED_16( int16_t dmv[2][SCAN8_LUMA_SIZE][2] );
    ALIGNED_16( uint8_t bs[2][8][4] );
    memset( nnz, 3, sizeof(nnz) );
    memset( dref, 1, sizeof(dref) );
    memset( dmv, 0, sizeof(dmv) );
    memset( bs, 0, sizeof(bs) );
    set_func_name( "x264: deblock_strength" );
    call_c_cp_a_void( dbf_c.deblock_strength, dbf_s.deblock_strength, dbf_asm.deblock_strength, nnz, dref, dmv, bs, 32, 0 );

    /* cabac encode of a small block (compare the produced bitstreams) */
    static x264_cabac_t cb, cb_s, cb_a;
    static uint8_t cabac_dst[0x1000], cabac_dst_s[0x1000], cabac_dst_a[0x1000];
    x264_cabac_init( NULL );
    x264s_cabac_init( NULL );
    x264_cabac_context_init( NULL, &cb, SLICE_TYPE_P, 26, 0 );
    x264s_template(cabac_context_init)( NULL, &cb_s, SLICE_TYPE_P, 26, 0 );
#ifdef VB_X264_CABAC_ASM
    x264_cabac_context_init( NULL, &cb_a, SLICE_TYPE_P, 26, 0 );
#endif
    set_func_name( "x264: cabac encode block" );
    call_c( vb_cabac_encode_block, &cb, cabac_dst, buf1, 64 );
    call_cp( vb_cabac_encode_block_s, &cb_s, cabac_dst_s, buf1, 64 );
#if defined(VB_X264_CABAC_ASM) && VB_X264_CABAC_ASM
    if( asm_cpu )
        call_a( vb_cabac_encode_block_a, &cb_a, cabac_dst_a, buf1, 64 );
#endif
    if( (cb.p - cabac_dst) != (cb_s.p - cabac_dst_s)
        || memcmp( cabac_dst, cabac_dst_s, cb.p - cabac_dst ) )
        ret = -1;
#if defined(VB_X264_CABAC_ASM) && VB_X264_CABAC_ASM
    if( asm_cpu && ( (cb.p - cabac_dst) != (cb_a.p - cabac_dst_a)
        || memcmp( cabac_dst, cabac_dst_a, cb.p - cabac_dst ) ) )
        ret = -1;
#endif

    /* bitstream nal escape (returns the end pointer: must match) */
    set_func_name( "x264: nal_escape" );
    call_c_cp_a( bsf_c.nal_escape, bsf_s.nal_escape, bsf_asm.nal_escape, buf3, buf1, buf1+0x400 );

    return ret;
}
