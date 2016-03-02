/*****************************************************************************
 * bench.h: header for calling and running benchmarks
 *****************************************************************************
 *
 * Copyright (C) 2016 Michail Alvanos
 * Copyright (C) 2003-2016 x264 project
 *
 * Authors of x264 : Loren Merritt <lorenm@u.washington.edu>
 *                   Laurent Aimar <fenrir@via.ecp.fr>
 *                   Fiona Glaser <fiona@x264.com>
 *          
 * Author of Video SIMD-Benchmark: Michail Alvanos <malvanos@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This x264 is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *
 * *************************************************************************** 
 *
 * Notes:   The _10b subfix is for the high bit depth. 
 *
 * *************************************************************************** 
 *
 *  Version - Changes
 *
 *  0.1     - Import only the basic structures.  
 *  0.2     - Import bench_dct structures.
 *
 *
 *****************************************************************************/

#ifndef BENCH_H
#define BENCH_H

#if !defined(MAX_CPUS)
    #error "You should include the common.h first and then the bench.h"
#endif


typedef struct
{
    void *pointer; // just for detecting duplicates
    uint32_t cpu;
    uint64_t cycles;
    uint32_t den;
} bench_t;

typedef struct
{
    char *name;
    bench_t vers[MAX_CPUS]; // MAX_CPUS defined to common.h
} bench_func_t;


////////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    uint8_t i_bits;
    uint8_t i_size;
} vlc_t;

typedef struct
{
    uint16_t i_bits;
    uint8_t  i_size;
    /* Next level table to use */
    uint8_t  i_next;
} vlc_large_t;

typedef struct bs_s
{
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    uintptr_t cur_bits;
    int     i_left;    /* i_count number of available bits */
    int     i_bits_encoded; /* RD only */
} bs_t;

typedef struct
{
    int32_t last;
    int32_t mask;
    ALIGNED_16( dctcoef level[18] );
} vbench_run_level_t;

struct vbench_weight_t;
typedef void (* weight_fn_t)( pixel *, intptr_t, pixel *,intptr_t, const struct vbench_weight_t *, int );
typedef struct vbench_weight_t
{
    /* aligning the first member is a gcc hack to force the struct to be
     *      * 16 byte aligned, as well as force sizeof(struct) to be a multiple of 16 */
    ALIGNED_16( int16_t cachea[8] );
    int16_t cacheb[8];
    int32_t i_denom;
    int32_t i_scale;
    int32_t i_offset;
    weight_fn_t *weightfn;
} ALIGNED_16( vbench_weight_t );



////////////////////////////////////////////////////////////////////////////////////////////


enum
{
    PIXEL_16x16 = 0,
    PIXEL_16x8  = 1,
    PIXEL_8x16  = 2,
    PIXEL_8x8   = 3,
    PIXEL_8x4   = 4,
    PIXEL_4x8   = 5,
    PIXEL_4x4   = 6,

    /* Subsampled chroma only */
    PIXEL_4x16  = 7,  /* 4:2:2 */
    PIXEL_4x2   = 8,
    PIXEL_2x8   = 9,  /* 4:2:2 */
    PIXEL_2x4   = 10,
    PIXEL_2x2   = 11,
};

static const struct { uint8_t w, h; } vbech_pixel_size[12] =
{
    { 16, 16 }, { 16, 8 }, { 8, 16 }, { 8, 8 }, { 8, 4 }, { 4, 8 }, { 4, 4 },
    {  4, 16 }, {  4, 2 }, { 2,  8 }, { 2, 4 }, { 2, 2 },
};

////////////////////////////////////////////////////////////////////////////////////////////
#if ARCH_X86_64
/* Evil hack: detect incorrect assumptions that 32-bit ints are zero-extended to 64-bit.
 * This is done by clobbering the stack with junk around the stack pointer and calling the
 * assembly function through checkasm_call with added dummy arguments which forces all
 * real arguments to be passed on the stack and not in registers. For 32-bit argument the
 * upper half of the 64-bit register location on the stack will now contain junk. Note that
 * this is dependant on compiler behaviour and that interrupts etc. at the wrong time may
 * overwrite the junk written to the stack so there's no guarantee that it will always
 * detect all functions that assumes zero-extension.
 */
void checkasm_stack_clobber( uint64_t clobber, ... );
#define call_a1(func,...) ({ \
    uint64_t r = (rand() & 0xffff) * 0x0001000100010001ULL; \
    asm_checkasm_stack_clobber( r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r ); /* max_args+6 */ \
    asm_checkasm_call(( intptr_t(*)())func, &ok, 0, 0, 0, 0, __VA_ARGS__ ); })
#elif ARCH_X86 || (ARCH_AARCH64 && !defined(__APPLE__)) || ARCH_ARM
#define call_a1(func,...) checkasm_call( (intptr_t(*)())func, &ok, __VA_ARGS__ )
#else
#define call_a1 call_c1
#endif

#if ARCH_ARM
#define call_a1_64(func,...) ((uint64_t (*)(intptr_t(*)(), int*, ...))checkasm_call)( (intptr_t(*)())func, &ok, __VA_ARGS__ )
#else
#define call_a1_64 call_a1
#endif

#define call_bench(func,cpu,...)\
    if( !strncmp(func_name, bench_pattern, bench_pattern_len) )\
    {\
        uint64_t tsum = 0;\
        int tcount = 0;\
        call_a1(func, __VA_ARGS__);\
        for( int ti = 0; ti < (cpu?BENCH_RUNS:BENCH_RUNS/4); ti++ )\
        {\
            uint32_t t = read_time();\
            func(__VA_ARGS__);\
            func(__VA_ARGS__);\
            func(__VA_ARGS__);\
            func(__VA_ARGS__); \
            t = read_time() - t;\
            if( (uint64_t)t*tcount <= tsum*4 && ti > 0 )\
            {\
                tsum += t;\
                tcount++;\
            }\
        }\
        bench_t *b = get_bench( func_name, cpu );\
        b->cycles += tsum;\
        b->den += tcount;   \
        b->pointer = func;\
    }

/* for most functions, run benchmark and correctness test at the same time.
 * for those that modify their inputs, run the above macros separately */
#define call_a(func,...) ({ call_a2(func,__VA_ARGS__); call_a1(func,__VA_ARGS__); })
#define call_c(func,...) ({ call_c2(func,__VA_ARGS__); call_c1(func,__VA_ARGS__); })
#define call_a2(func,...) ({ call_bench(func,cpu_new,__VA_ARGS__); })
#define call_c2(func,...) ({ call_bench(func,0,__VA_ARGS__); })
#define call_a64(func,...) ({ call_a2(func,__VA_ARGS__); call_a1_64(func,__VA_ARGS__); })


////////////////////////////////////////////////////////////////////////////////////////////

typedef void (*vbench_predict_t)( pixel *src );
typedef void (*vbench_predict8x8_t)( pixel *src, pixel edge[36] );
typedef void (*vbench_predict_8x8_filter_t) ( pixel *src, pixel edge[36], int i_neighbor, int i_filters );


// SSD assumes all args aligned
// other cmp functions assume first arg aligned
typedef int  (*vbench_pixel_cmp_t) ( pixel *, intptr_t, pixel *, intptr_t );
typedef void (*vbench_pixel_cmp_x3_t) ( pixel *, pixel *, pixel *, pixel *, intptr_t, int[3] );
typedef void (*vbench_pixel_cmp_x4_t) ( pixel *, pixel *, pixel *, pixel *, pixel *, intptr_t, int[4] );






typedef struct
{
    vbench_pixel_cmp_t  sad[8];
    vbench_pixel_cmp_t  ssd[8];
    vbench_pixel_cmp_t satd[8];
    vbench_pixel_cmp_t ssim[7];
    vbench_pixel_cmp_t sa8d[4];
    vbench_pixel_cmp_t mbcmp[8]; /* either satd or sad for subpel refine and mode decision */
    vbench_pixel_cmp_t mbcmp_unaligned[8]; /* unaligned mbcmp for subpel */
    vbench_pixel_cmp_t fpelcmp[8]; /* either satd or sad for fullpel motion search */
    vbench_pixel_cmp_x3_t fpelcmp_x3[7];
    vbench_pixel_cmp_x4_t fpelcmp_x4[7];
    vbench_pixel_cmp_t sad_aligned[8]; /* Aligned SAD for mbcmp */
    int (*vsad)( pixel *, intptr_t, int );
    int (*asd8)( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2, int height );
    uint64_t (*sa8d_satd[1])( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2 );

    uint64_t (*var[4])( pixel *pix, intptr_t stride );
    int (*var2[4])( pixel *pix1, intptr_t stride1,
            pixel *pix2, intptr_t stride2, int *ssd );
    uint64_t (*hadamard_ac[4])( pixel *pix, intptr_t stride );

    void (*ssd_nv12_core)( pixel *pixuv1, intptr_t stride1,
            pixel *pixuv2, intptr_t stride2, int width, int height,
            uint64_t *ssd_u, uint64_t *ssd_v );
    void (*ssim_4x4x2_core)( const pixel *pix1, intptr_t stride1,
            const pixel *pix2, intptr_t stride2, int sums[2][4] );
    float (*ssim_end4)( int sum0[5][4], int sum1[5][4], int width );

    /* multiple parallel calls to cmp. */
    vbench_pixel_cmp_x3_t sad_x3[7];
    vbench_pixel_cmp_x4_t sad_x4[7];
    vbench_pixel_cmp_x3_t satd_x3[7];
    vbench_pixel_cmp_x4_t satd_x4[7];


    /* abs-diff-sum for successive elimination.
     *      * may round width up to a multiple of 16. */
    int (*ads[7])( int enc_dc[4], uint16_t *sums, int delta,
            uint16_t *cost_mvx, int16_t *mvs, int width, int thresh );

    /* calculate satd or sad of V, H, and DC modes. */
    void (*intra_mbcmp_x3_16x16)( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_satd_x3_16x16) ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_sad_x3_16x16)  ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_mbcmp_x3_4x4)  ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_satd_x3_4x4)   ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_sad_x3_4x4)    ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_mbcmp_x3_chroma)( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_satd_x3_chroma) ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_sad_x3_chroma)  ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_mbcmp_x3_8x16c) ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_satd_x3_8x16c)  ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_sad_x3_8x16c)   ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_mbcmp_x3_8x8c)  ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_satd_x3_8x8c)   ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_sad_x3_8x8c)    ( pixel *fenc, pixel *fdec, int res[3] );
    void (*intra_mbcmp_x3_8x8)  ( pixel *fenc, pixel edge[36], int res[3] );
    void (*intra_sa8d_x3_8x8)   ( pixel *fenc, pixel edge[36], int res[3] );
    void (*intra_sad_x3_8x8)    ( pixel *fenc, pixel edge[36], int res[3] );
    /* find minimum satd or sad of all modes, and set fdec.
     *      * may be NULL, in which case just use pred+satd instead. */
    int (*intra_mbcmp_x9_4x4)( pixel *fenc, pixel *fdec, uint16_t *bitcosts );
    int (*intra_satd_x9_4x4) ( pixel *fenc, pixel *fdec, uint16_t *bitcosts );
    int (*intra_sad_x9_4x4)  ( pixel *fenc, pixel *fdec, uint16_t *bitcosts );
    int (*intra_mbcmp_x9_8x8)( pixel *fenc, pixel *fdec, pixel edge[36], uint16_t *bitcosts, uint16_t *satds );
    int (*intra_sa8d_x9_8x8) ( pixel *fenc, pixel *fdec, pixel edge[36], uint16_t *bitcosts, uint16_t *satds );
    int (*intra_sad_x9_8x8)  ( pixel *fenc, pixel *fdec, pixel edge[36], uint16_t *bitcosts, uint16_t *satds );
} vbench_pixel_function_t;

typedef struct
{
    // pix1  stride = FENC_STRIDE
    // pix2  stride = FDEC_STRIDE
    // p_dst stride = FDEC_STRIDE
    void (*sub4x4_dct)   ( dctcoef dct[16], pixel *pix1, pixel *pix2 );
    void (*add4x4_idct)  ( pixel *p_dst, dctcoef dct[16] );

    void (*sub8x8_dct)   ( dctcoef dct[4][16], pixel *pix1, pixel *pix2 );
    void (*sub8x8_dct_dc)( dctcoef dct[4], pixel *pix1, pixel *pix2 );
    void (*add8x8_idct)  ( pixel *p_dst, dctcoef dct[4][16] );
    void (*add8x8_idct_dc) ( pixel *p_dst, dctcoef dct[4] );

    void (*sub8x16_dct_dc)( dctcoef dct[8], pixel *pix1, pixel *pix2 );

    void (*sub16x16_dct) ( dctcoef dct[16][16], pixel *pix1, pixel *pix2 );
    void (*add16x16_idct)( pixel *p_dst, dctcoef dct[16][16] );
    void (*add16x16_idct_dc) ( pixel *p_dst, dctcoef dct[16] );

    void (*sub8x8_dct8)  ( dctcoef dct[64], pixel *pix1, pixel *pix2 );
    void (*add8x8_idct8) ( pixel *p_dst, dctcoef dct[64] );

    void (*sub16x16_dct8) ( dctcoef dct[4][64], pixel *pix1, pixel *pix2 );
    void (*add16x16_idct8)( pixel *p_dst, dctcoef dct[4][64] );

    void (*dct4x4dc) ( dctcoef d[16] );
    void (*idct4x4dc)( dctcoef d[16] );

    void (*dct2x4dc)( dctcoef dct[8], dctcoef dct4x4[8][16] );

} vbench_dct_function_t;

typedef struct
{
    void (*scan_8x8)( dctcoef level[64], dctcoef dct[64] );
    void (*scan_4x4)( dctcoef level[16], dctcoef dct[16] );
    int  (*sub_8x8)  ( dctcoef level[64], const pixel *p_src, pixel *p_dst );
    int  (*sub_4x4)  ( dctcoef level[16], const pixel *p_src, pixel *p_dst );
    int  (*sub_4x4ac)( dctcoef level[16], const pixel *p_src, pixel *p_dst, dctcoef *dc );
    void (*interleave_8x8_cavlc)( dctcoef *dst, dctcoef *src, uint8_t *nnz );

} vbench_zigzag_function_t;


typedef struct
{
    int (*quant_8x8)  ( dctcoef dct[64], udctcoef mf[64], udctcoef bias[64] );
    int (*quant_4x4)  ( dctcoef dct[16], udctcoef mf[16], udctcoef bias[16] );
    int (*quant_4x4x4)( dctcoef dct[4][16], udctcoef mf[16], udctcoef bias[16] );
    int (*quant_4x4_dc)( dctcoef dct[16], int mf, int bias );
    int (*quant_2x2_dc)( dctcoef dct[4], int mf, int bias );

    void (*dequant_8x8)( dctcoef dct[64], int dequant_mf[6][64], int i_qp );
    void (*dequant_4x4)( dctcoef dct[16], int dequant_mf[6][16], int i_qp );
    void (*dequant_4x4_dc)( dctcoef dct[16], int dequant_mf[6][16], int i_qp );

    void (*idct_dequant_2x4_dc)( dctcoef dct[8], dctcoef dct4x4[8][16], int dequant_mf[6][16], int i_qp );
    void (*idct_dequant_2x4_dconly)( dctcoef dct[8], int dequant_mf[6][16], int i_qp );

    int (*optimize_chroma_2x2_dc)( dctcoef dct[4], int dequant_mf );
    int (*optimize_chroma_2x4_dc)( dctcoef dct[8], int dequant_mf );

    void (*denoise_dct)( dctcoef *dct, uint32_t *sum, udctcoef *offset, int size );

    int (*decimate_score15)( dctcoef *dct );
    int (*decimate_score16)( dctcoef *dct );
    int (*decimate_score64)( dctcoef *dct );
    int (*coeff_last[14])( dctcoef *dct );
    int (*coeff_last4)( dctcoef *dct );
    int (*coeff_last8)( dctcoef *dct );
    int (*coeff_level_run[13])( dctcoef *dct, vbench_run_level_t *runlevel );
    int (*coeff_level_run4)( dctcoef *dct, vbench_run_level_t *runlevel );
    int (*coeff_level_run8)( dctcoef *dct, vbench_run_level_t *runlevel );

#define TRELLIS_PARAMS const int *unquant_mf, const uint8_t *zigzag, int lambda2,\
    int last_nnz, dctcoef *coefs, dctcoef *quant_coefs, dctcoef *dct,\
    uint8_t *cabac_state_sig, uint8_t *cabac_state_last,\
    uint64_t level_state0, uint16_t level_state1
    int (*trellis_cabac_4x4)( TRELLIS_PARAMS, int b_ac );
    int (*trellis_cabac_8x8)( TRELLIS_PARAMS, int b_interlaced );
    int (*trellis_cabac_4x4_psy)( TRELLIS_PARAMS, int b_ac, dctcoef *fenc_dct, int psy_trellis );
    int (*trellis_cabac_8x8_psy)( TRELLIS_PARAMS, int b_interlaced, dctcoef *fenc_dct, int psy_trellis );
    int (*trellis_cabac_dc)( TRELLIS_PARAMS, int num_coefs );
    int (*trellis_cabac_chroma_422_dc)( TRELLIS_PARAMS );
} vbench_quant_function_t;



/* Do the MC
 *   XXX: Only width = 4, 8 or 16 are valid
 *   width == 4 -> height == 4 or 8
 *   width == 8 -> height == 4 or 8 or 16
 *   width == 16-> height == 8 or 16
 * * */
struct vbench_mc_functions_t;
typedef struct vbench_mc_functions_t
{
    void (*mc_luma)( pixel *dst, intptr_t i_dst, pixel **src, intptr_t i_src,
            int mvx, int mvy, int i_width, int i_height, const vbench_weight_t *weight );

    /* may round up the dimensions if they're not a power of 2 */
    pixel* (*get_ref)( pixel *dst, intptr_t *i_dst, pixel **src, intptr_t i_src,
            int mvx, int mvy, int i_width, int i_height, const vbench_weight_t *weight );

    /* mc_chroma may write up to 2 bytes of garbage to the right of dst,
     *      * so it must be run from left to right. */
    void (*mc_chroma)( pixel *dstu, pixel *dstv, intptr_t i_dst, pixel *src, intptr_t i_src,
            int mvx, int mvy, int i_width, int i_height );

    void (*avg[12])( pixel *dst,  intptr_t dst_stride, pixel *src1, intptr_t src1_stride,
            pixel *src2, intptr_t src2_stride, int i_weight );

    /* only 16x16, 8x8, and 4x4 defined */
    void (*copy[7])( pixel *dst, intptr_t dst_stride, pixel *src, intptr_t src_stride, int i_height );
    void (*copy_16x16_unaligned)( pixel *dst, intptr_t dst_stride, pixel *src, intptr_t src_stride, int i_height );

    void (*store_interleave_chroma)( pixel *dst, intptr_t i_dst, pixel *srcu, pixel *srcv, int height );
    void (*load_deinterleave_chroma_fenc)( pixel *dst, pixel *src, intptr_t i_src, int height );
    void (*load_deinterleave_chroma_fdec)( pixel *dst, pixel *src, intptr_t i_src, int height );

    void (*plane_copy)( pixel *dst, intptr_t i_dst, pixel *src, intptr_t i_src, int w, int h );
    void (*plane_copy_swap)( pixel *dst, intptr_t i_dst, pixel *src, intptr_t i_src, int w, int h );
    void (*plane_copy_interleave)( pixel *dst,  intptr_t i_dst, pixel *srcu, intptr_t i_srcu,
            pixel *srcv, intptr_t i_srcv, int w, int h );
    /* may write up to 15 pixels off the end of each plane */
    void (*plane_copy_deinterleave)( pixel *dstu, intptr_t i_dstu, pixel *dstv, intptr_t i_dstv,
            pixel *src,  intptr_t i_src, int w, int h );
    void (*plane_copy_deinterleave_rgb)( pixel *dsta, intptr_t i_dsta, pixel *dstb, intptr_t i_dstb,
            pixel *dstc, intptr_t i_dstc, pixel *src,  intptr_t i_src, int pw, int w, int h );
    void (*plane_copy_deinterleave_v210)( pixel *dsty, intptr_t i_dsty,
            pixel *dstc, intptr_t i_dstc,
            uint32_t *src, intptr_t i_src, int w, int h );
    void (*hpel_filter)( pixel *dsth, pixel *dstv, pixel *dstc, pixel *src,
            intptr_t i_stride, int i_width, int i_height, int16_t *buf );

    /* prefetch the next few macroblocks of fenc or fdec */
    void (*prefetch_fenc)    ( pixel *pix_y, intptr_t stride_y, pixel *pix_uv, intptr_t stride_uv, int mb_x );
    void (*prefetch_fenc_420)( pixel *pix_y, intptr_t stride_y, pixel *pix_uv, intptr_t stride_uv, int mb_x );
    void (*prefetch_fenc_422)( pixel *pix_y, intptr_t stride_y, pixel *pix_uv, intptr_t stride_uv, int mb_x );
    /* prefetch the next few macroblocks of a hpel reference frame */
    void (*prefetch_ref)( pixel *pix, intptr_t stride, int parity );

    void *(*memcpy_aligned)( void *dst, const void *src, size_t n );
    void (*memzero_aligned)( void *dst, size_t n );

    /* successive elimination prefilter */
    void (*integral_init4h)( uint16_t *sum, pixel *pix, intptr_t stride );
    void (*integral_init8h)( uint16_t *sum, pixel *pix, intptr_t stride );
    void (*integral_init4v)( uint16_t *sum8, uint16_t *sum4, intptr_t stride );
    void (*integral_init8v)( uint16_t *sum8, intptr_t stride );

    void (*frame_init_lowres_core)( pixel *src0, pixel *dst0, pixel *dsth, pixel *dstv, pixel *dstc,
            intptr_t src_stride, intptr_t dst_stride, int width, int height );
    weight_fn_t *weight;
    weight_fn_t *offsetadd;
    weight_fn_t *offsetsub;
    void (*weight_cache)(  struct vbench_mc_functions_t *mc, vbench_weight_t * );

    void (*mbtree_propagate_cost)( int16_t *dst, uint16_t *propagate_in, uint16_t *intra_costs,
            uint16_t *inter_costs, uint16_t *inv_qscales, float *fps_factor, int len );

   void (*mbtree_propagate_list)( uint16_t *ref_costs, int16_t (*mvs)[2],
            int16_t *propagate_amount, uint16_t *lowres_costs,
           int bipred_weight, int mb_y, int len, int list,  unsigned stride, unsigned width, unsigned height, void *buffer2   );
} vbench_mc_functions_t;



////////////////////////////////////////////////////////////////////////////////////////////






int run_benchmarks(int i);

#endif // BENCH_H

