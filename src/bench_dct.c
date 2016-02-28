/*****************************************************************************
 * bench_dct.c: run and measure the kernels of dct.
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
 *  0.1     - Initial checkin. 
 *
 *
 *****************************************************************************/


#include <ctype.h>
#include "osdep.h"
#include "common.h"
#include "bench.h"
#include "macroblock.h"


/* buf1, buf2: initialised to random data and shouldn't write into them */
uint8_t *buf1, *buf2;
/* buf3, buf4: used to store output */
uint8_t *buf3, *buf4;
/* pbuf1, pbuf2: initialised to random pixel data and shouldn't write into them. */
pixel *pbuf1, *pbuf2;
/* pbuf3, pbuf4: point to buf3, buf4, just for type convenience */
pixel *pbuf3, *pbuf4;

int quiet = 0;

#define report( name ) { \
    if( used_asm && !quiet ) \
        fprintf( stderr, " - %-21s [%s]\n", name, ok ? "OK" : "FAILED" ); \
    if( !ok ) ret = -1; \
}


extern int bench_pattern_len;
extern const char *bench_pattern;
char func_name[100];
extern  bench_func_t benchs[MAX_FUNCS];

static const char *pixel_names[12] = { "16x16", "16x8", "8x16", "8x8", "8x4", "4x8", "4x4", "4x16", "4x2", "2x8", "2x4", "2x2" };
static const char *intra_predict_16x16_names[7] = { "v", "h", "dc", "p", "dcl", "dct", "dc8" };
static const char *intra_predict_8x8c_names[7] = { "dc", "h", "v", "p", "dcl", "dct", "dc8" };
static const char *intra_predict_4x4_names[12] = { "v", "h", "dc", "ddl", "ddr", "vr", "hd", "vl", "hu", "dcl", "dct", "dc8" };
static const char **intra_predict_8x8_names = intra_predict_4x4_names;
static const char **intra_predict_8x16c_names = intra_predict_8x8c_names;

#define set_func_name(...) snprintf( func_name, sizeof(func_name), __VA_ARGS__ )

static inline uint32_t read_time(void)
{
    uint32_t a = 0;
#if HAVE_X86_INLINE_ASM
    asm volatile( "lfence \n"
                  "rdtsc  \n"
                  : "=a"(a) :: "edx", "memory" );
#elif ARCH_PPC
    asm volatile( "mftb %0" : "=r"(a) :: "memory" );
#elif ARCH_ARM     // ARMv7 only
    asm volatile( "mrc p15, 0, %0, c9, c13, 0" : "=r"(a) :: "memory" );
#elif ARCH_AARCH64
    uint64_t b = 0;
    asm volatile( "mrs %0, pmccntr_el0" : "=r"(b) :: "memory" );
    a = b;
#elif ARCH_MIPS
    asm volatile( "rdhwr %0, $2" : "=r"(a) :: "memory" );
#endif
    return a;
}

static bench_t* get_bench( const char *name, int cpu )
{
    int i, j;
    for( i = 0; benchs[i].name && strcmp(name, benchs[i].name); i++ )
        assert( i < MAX_FUNCS );
    if( !benchs[i].name )
        benchs[i].name = strdup( name );
    if( !cpu )
        return &benchs[i].vers[0];
    for( j = 1; benchs[i].vers[j].cpu && benchs[i].vers[j].cpu != cpu; j++ )
        assert( j < MAX_CPUS );
    benchs[i].vers[j].cpu = cpu;
    return &benchs[i].vers[j];
}


#define call_c1(func,...) func(__VA_ARGS__)


int check_dct( int cpu_ref, int cpu_new )
{
    vbench_dct_function_t dct_c;
    vbench_dct_function_t dct_ref;
    vbench_dct_function_t dct_asm;
    vbench_quant_function_t qf;
    int ret = 0, ok, used_asm, interlace = 0;
    ALIGNED_ARRAY_N( dctcoef, dct1, [16],[16] );
    ALIGNED_ARRAY_N( dctcoef, dct2, [16],[16] );
    ALIGNED_ARRAY_N( dctcoef, dct4, [16],[16] );
    ALIGNED_ARRAY_N( dctcoef, dct8, [4],[64] );
    ALIGNED_16( dctcoef dctdc[2][8] );

    vbench_dct_init( 0, &dct_c );
    vbench_dct_init( cpu_ref, &dct_ref);
    vbench_dct_init( cpu_new, &dct_asm );

    uint8_t *scaling_list[8]; /* could be 12, but we don't allow separate Cb/Cr lists */
    uint8_t   *chroma_qp_table; /* includes both the nonlinear luma->chroma mapping and chroma_qp_offset */

    /* quantization matrix for decoding, [cqm][qp%6][coef] */
    int             (*dequant4_mf[4])[16];   /* [4][6][16] */
    int             (*dequant8_mf[4])[64];   /* [4][6][64] */
    /* quantization matrix for trellis, [cqm][qp][coef] */
    int             (*unquant4_mf[4])[16];   /* [4][QP_MAX_SPEC+1][16] */
    int             (*unquant8_mf[4])[64];   /* [4][QP_MAX_SPEC+1][64] */
    /* quantization matrix for deadzone */
    udctcoef        (*quant4_mf[4])[16];     /* [4][QP_MAX_SPEC+1][16] */
    udctcoef        (*quant8_mf[4])[64];     /* [4][QP_MAX_SPEC+1][64] */
    udctcoef        (*quant4_bias[4])[16];   /* [4][QP_MAX_SPEC+1][16] */
    udctcoef        (*quant8_bias[4])[64];   /* [4][QP_MAX_SPEC+1][64] */
    udctcoef        (*quant4_bias0[4])[16];  /* [4][QP_MAX_SPEC+1][16] */
    udctcoef        (*quant8_bias0[4])[64];  /* [4][QP_MAX_SPEC+1][64] */
    udctcoef        (*nr_offset_emergency)[4][64];


    //memset( h, 0, sizeof(*h) );
    //x264_param_default( &h->param );

    chroma_qp_table = i_chroma_qp_table + 12;
    for( int i = 0; i < 6; i++ )
           scaling_list[i] = vbench_cqm_flat16;

    vbench_cqm_init( scaling_list, chroma_qp_table );
    vbench_quant_init( 0, &qf );

    /* overflow test cases */
    for( int i = 0; i < 5; i++ )
    {
        pixel *enc = &pbuf3[16*i*FENC_STRIDE];
        pixel *dec = &pbuf4[16*i*FDEC_STRIDE];

        for( int j = 0; j < 16; j++ )
        {
            int cond_a = (i < 2) ? 1 : ((j&3) == 0 || (j&3) == (i-1));
            int cond_b = (i == 0) ? 1 : !cond_a;
            enc[0] = enc[1] = enc[4] = enc[5] = enc[8] = enc[9] = enc[12] = enc[13] = cond_a ? PIXEL_MAX : 0;
            enc[2] = enc[3] = enc[6] = enc[7] = enc[10] = enc[11] = enc[14] = enc[15] = cond_b ? PIXEL_MAX : 0;

            for( int k = 0; k < 4; k++ )
                dec[k] = PIXEL_MAX - enc[k];

            enc += FENC_STRIDE;
            dec += FDEC_STRIDE;
        }
    }

#define TEST_DCT( name, t1, t2, size ) \
    if( dct_asm.name != dct_ref.name ) \
    { \
        set_func_name( #name ); \
        used_asm = 1; \
        pixel *enc = pbuf3; \
        pixel *dec = pbuf4; \
        for( int j = 0; j < 5; j++) \
        { \
            call_c( dct_c.name, t1, &pbuf1[j*64], &pbuf2[j*64] ); \
            call_a( dct_asm.name, t2, &pbuf1[j*64], &pbuf2[j*64] ); \
            if( memcmp( t1, t2, size*sizeof(dctcoef) ) ) \
            { \
                ok = 0; \
                fprintf( stderr, #name " [FAILED]\n" ); \
                for( int k = 0; k < size; k++ )\
                    printf( "%d ", ((dctcoef*)t1)[k] );\
                printf("\n");\
                for( int k = 0; k < size; k++ )\
                    printf( "%d ", ((dctcoef*)t2)[k] );\
                printf("\n");\
                break; \
            } \
            call_c( dct_c.name, t1, enc, dec ); \
            call_a( dct_asm.name, t2, enc, dec ); \
            if( memcmp( t1, t2, size*sizeof(dctcoef) ) ) \
            { \
                ok = 0; \
                fprintf( stderr, #name " [FAILED] (overflow)\n" ); \
                break; \
            } \
            enc += 16*FENC_STRIDE; \
            dec += 16*FDEC_STRIDE; \
        } \
    }
    ok = 1; used_asm = 0;
    TEST_DCT( sub4x4_dct, dct1[0], dct2[0], 16 );
    TEST_DCT( sub8x8_dct, dct1, dct2, 16*4 );
    TEST_DCT( sub8x8_dct_dc, dctdc[0], dctdc[1], 4 );
    TEST_DCT( sub8x16_dct_dc, dctdc[0], dctdc[1], 8 );
    TEST_DCT( sub16x16_dct, dct1, dct2, 16*16 );
    report( "sub_dct4 :" );

    ok = 1; used_asm = 0;
    TEST_DCT( sub8x8_dct8, (void*)dct1[0], (void*)dct2[0], 64 );
    TEST_DCT( sub16x16_dct8, (void*)dct1, (void*)dct2, 64*4 );
    report( "sub_dct8 :" );
#undef TEST_DCT

    // fdct and idct are denormalized by different factors, so quant/dequant
    // is needed to force the coefs into the right range.
    dct_c.sub16x16_dct( dct4, pbuf1, pbuf2 );
    dct_c.sub16x16_dct8( dct8, pbuf1, pbuf2 );
    for( int i = 0; i < 16; i++ )
    {
        qf.quant_4x4( dct4[i], quant4_mf[CQM_4IY][20], quant4_bias[CQM_4IY][20] );
        qf.dequant_4x4( dct4[i], dequant4_mf[CQM_4IY], 20 );
    }
    for( int i = 0; i < 4; i++ )
    {
        qf.quant_8x8( dct8[i], quant8_mf[CQM_8IY][20], quant8_bias[CQM_8IY][20] );
        qf.dequant_8x8( dct8[i], dequant8_mf[CQM_8IY], 20 );
    }
    vbench_cqm_delete( dequant4_mf, dequant8_mf, unquant4_mf, unquant8_mf, quant4_mf,   quant8_mf,    quant4_bias,    quant8_bias,  quant4_bias0,  quant8_bias0,   nr_offset_emergency  );

#define TEST_IDCT( name, src ) \
    if( dct_asm.name != dct_ref.name ) \
    { \
        set_func_name( #name ); \
        used_asm = 1; \
        memcpy( pbuf3, pbuf1, 32*32 * sizeof(pixel) ); \
        memcpy( pbuf4, pbuf1, 32*32 * sizeof(pixel) ); \
        memcpy( dct1, src, 256 * sizeof(dctcoef) ); \
        memcpy( dct2, src, 256 * sizeof(dctcoef) ); \
        call_c1( dct_c.name, pbuf3, (void*)dct1 ); \
        call_a1( dct_asm.name, pbuf4, (void*)dct2 ); \
        if( memcmp( pbuf3, pbuf4, 32*32 * sizeof(pixel) ) ) \
        { \
            ok = 0; \
            fprintf( stderr, #name " [FAILED]\n" ); \
        } \
        call_c2( dct_c.name, pbuf3, (void*)dct1 ); \
        call_a2( dct_asm.name, pbuf4, (void*)dct2 ); \
    }
    ok = 1; used_asm = 0;
    TEST_IDCT( add4x4_idct, dct4 );
    TEST_IDCT( add8x8_idct, dct4 );
    TEST_IDCT( add8x8_idct_dc, dct4 );
    TEST_IDCT( add16x16_idct, dct4 );
    TEST_IDCT( add16x16_idct_dc, dct4 );
    report( "add_idct4 :" );

    ok = 1; used_asm = 0;
    TEST_IDCT( add8x8_idct8, dct8 );
    TEST_IDCT( add16x16_idct8, dct8 );
    report( "add_idct8 :" );
#undef TEST_IDCT

#define TEST_DCTDC( name )\
    ok = 1; used_asm = 0;\
    if( dct_asm.name != dct_ref.name )\
    {\
        set_func_name( #name );\
        used_asm = 1;\
        uint16_t *p = (uint16_t*)buf1;\
        for( int i = 0; i < 16 && ok; i++ )\
        {\
            for( int j = 0; j < 16; j++ )\
                dct1[0][j] = !i ? (j^j>>1^j>>2^j>>3)&1 ? PIXEL_MAX*16 : -PIXEL_MAX*16 /* max dc */\
                           : i<8 ? (*p++)&1 ? PIXEL_MAX*16 : -PIXEL_MAX*16 /* max elements */\
                           : ((*p++)&0x1fff)-0x1000; /* general case */\
            memcpy( dct2, dct1, 16 * sizeof(dctcoef) );\
            call_c1( dct_c.name, dct1[0] );\
            call_a1( dct_asm.name, dct2[0] );\
            if( memcmp( dct1, dct2, 16 * sizeof(dctcoef) ) )\
                ok = 0;\
        }\
        call_c2( dct_c.name, dct1[0] );\
        call_a2( dct_asm.name, dct2[0] );\
    }\
    report( #name " :" );

    TEST_DCTDC(  dct4x4dc );
    TEST_DCTDC( idct4x4dc );
#undef TEST_DCTDC

#define TEST_DCTDC_CHROMA( name )\
    ok = 1; used_asm = 0;\
    if( dct_asm.name != dct_ref.name )\
    {\
        set_func_name( #name );\
        used_asm = 1;\
        uint16_t *p = (uint16_t*)buf1;\
        for( int i = 0; i < 16 && ok; i++ )\
        {\
            for( int j = 0; j < 8; j++ )\
                dct1[j][0] = !i ? (j^j>>1^j>>2)&1 ? PIXEL_MAX*16 : -PIXEL_MAX*16 /* max dc */\
                           : i<8 ? (*p++)&1 ? PIXEL_MAX*16 : -PIXEL_MAX*16 /* max elements */\
                           : ((*p++)&0x1fff)-0x1000; /* general case */\
            memcpy( dct2, dct1, 8*16 * sizeof(dctcoef) );\
            call_c1( dct_c.name, dctdc[0], dct1 );\
            call_a1( dct_asm.name, dctdc[1], dct2 );\
            if( memcmp( dctdc[0], dctdc[1], 8 * sizeof(dctcoef) ) || memcmp( dct1, dct2, 8*16 * sizeof(dctcoef) ) )\
            {\
                ok = 0;\
                fprintf( stderr, #name " [FAILED]\n" ); \
            }\
        }\
        call_c2( dct_c.name, dctdc[0], dct1 );\
        call_a2( dct_asm.name, dctdc[1], dct2 );\
    }\
    report( #name " :" );

    TEST_DCTDC_CHROMA( dct2x4dc );
#undef TEST_DCTDC_CHROMA

    vbench_zigzag_function_t zigzag_c[2];
    vbench_zigzag_function_t zigzag_ref[2];
    vbench_zigzag_function_t zigzag_asm[2];

    ALIGNED_16( dctcoef level1[64] );
    ALIGNED_16( dctcoef level2[64] );

#define TEST_ZIGZAG_SCAN( name, t1, t2, dct, size ) \
    if( zigzag_asm[interlace].name != zigzag_ref[interlace].name ) \
    { \
        set_func_name( "zigzag_"#name"_%s", interlace?"field":"frame" ); \
        used_asm = 1; \
        for( int i = 0; i < size*size; i++ ) \
            dct[i] = i; \
        call_c( zigzag_c[interlace].name, t1, dct ); \
        call_a( zigzag_asm[interlace].name, t2, dct ); \
        if( memcmp( t1, t2, size*size*sizeof(dctcoef) ) ) \
        { \
            ok = 0; \
            for( int i = 0; i < 2; i++ ) \
            { \
                dctcoef *d = (dctcoef*)(i ? t2 : t1); \
                for( int j = 0; j < size; j++ ) \
                { \
                    for( int k = 0; k < size; k++ ) \
                        fprintf( stderr, "%2d ", d[k+j*8] ); \
                    fprintf( stderr, "\n" ); \
                } \
                fprintf( stderr, "\n" ); \
            } \
            fprintf( stderr, #name " [FAILED]\n" ); \
        } \
    }

#define TEST_ZIGZAG_SUB( name, t1, t2, size ) \
    if( zigzag_asm[interlace].name != zigzag_ref[interlace].name ) \
    { \
        int nz_a, nz_c; \
        set_func_name( "zigzag_"#name"_%s", interlace?"field":"frame" ); \
        used_asm = 1; \
        memcpy( pbuf3, pbuf1, 16*FDEC_STRIDE * sizeof(pixel) ); \
        memcpy( pbuf4, pbuf1, 16*FDEC_STRIDE * sizeof(pixel) ); \
        nz_c = call_c1( zigzag_c[interlace].name, t1, pbuf2, pbuf3 ); \
        nz_a = call_a1( zigzag_asm[interlace].name, t2, pbuf2, pbuf4 ); \
        if( memcmp( t1, t2, size*sizeof(dctcoef) ) || memcmp( pbuf3, pbuf4, 16*FDEC_STRIDE*sizeof(pixel) ) || nz_c != nz_a ) \
        { \
            ok = 0; \
            fprintf( stderr, #name " [FAILED]\n" ); \
        } \
        call_c2( zigzag_c[interlace].name, t1, pbuf2, pbuf3 ); \
        call_a2( zigzag_asm[interlace].name, t2, pbuf2, pbuf4 ); \
    }

#define TEST_ZIGZAG_SUBAC( name, t1, t2 ) \
    if( zigzag_asm[interlace].name != zigzag_ref[interlace].name ) \
    { \
        int nz_a, nz_c; \
        dctcoef dc_a, dc_c; \
        set_func_name( "zigzag_"#name"_%s", interlace?"field":"frame" ); \
        used_asm = 1; \
        for( int i = 0; i < 2; i++ ) \
        { \
            memcpy( pbuf3, pbuf2, 16*FDEC_STRIDE * sizeof(pixel) ); \
            memcpy( pbuf4, pbuf2, 16*FDEC_STRIDE * sizeof(pixel) ); \
            for( int j = 0; j < 4; j++ ) \
            { \
                memcpy( pbuf3 + j*FDEC_STRIDE, (i?pbuf1:pbuf2) + j*FENC_STRIDE, 4 * sizeof(pixel) ); \
                memcpy( pbuf4 + j*FDEC_STRIDE, (i?pbuf1:pbuf2) + j*FENC_STRIDE, 4 * sizeof(pixel) ); \
            } \
            nz_c = call_c1( zigzag_c[interlace].name, t1, pbuf2, pbuf3, &dc_c ); \
            nz_a = call_a1( zigzag_asm[interlace].name, t2, pbuf2, pbuf4, &dc_a ); \
            if( memcmp( t1+1, t2+1, 15*sizeof(dctcoef) ) || memcmp( pbuf3, pbuf4, 16*FDEC_STRIDE * sizeof(pixel) ) || nz_c != nz_a || dc_c != dc_a ) \
            { \
                ok = 0; \
                fprintf( stderr, #name " [FAILED]\n" ); \
                break; \
            } \
        } \
        call_c2( zigzag_c[interlace].name, t1, pbuf2, pbuf3, &dc_c ); \
        call_a2( zigzag_asm[interlace].name, t2, pbuf2, pbuf4, &dc_a ); \
    }

#define TEST_INTERLEAVE( name, t1, t2, dct, size ) \
    if( zigzag_asm[interlace].name != zigzag_ref[interlace].name ) \
    { \
        for( int j = 0; j < 100; j++ ) \
        { \
            set_func_name( "zigzag_"#name"_%s", interlace?"field":"frame" ); \
            used_asm = 1; \
            memcpy(dct, buf1, size*sizeof(dctcoef)); \
            for( int i = 0; i < size; i++ ) \
                dct[i] = rand()&0x1F ? 0 : dct[i]; \
            memcpy(buf3, buf4, 10); \
            call_c( zigzag_c[interlace].name, t1, dct, buf3 ); \
            call_a( zigzag_asm[interlace].name, t2, dct, buf4 ); \
            if( memcmp( t1, t2, size*sizeof(dctcoef) ) || memcmp( buf3, buf4, 10 ) ) \
            { \
                ok = 0; printf("%d: %d %d %d %d\n%d %d %d %d\n\n",memcmp( t1, t2, size*sizeof(dctcoef) ),buf3[0], buf3[1], buf3[8], buf3[9], buf4[0], buf4[1], buf4[8], buf4[9]);break;\
            } \
        } \
    }

    vbench_zigzag_init( 0, &zigzag_c[0], &zigzag_c[1] );
    vbench_zigzag_init( cpu_ref, &zigzag_ref[0], &zigzag_ref[1] );
    vbench_zigzag_init( cpu_new, &zigzag_asm[0], &zigzag_asm[1] );

    ok = 1; used_asm = 0;
    TEST_INTERLEAVE( interleave_8x8_cavlc, level1, level2, dct8[0], 64 );
    report( "zigzag_interleave :" );

    for( interlace = 0; interlace <= 1; interlace++ )
    {
        ok = 1; used_asm = 0;
        TEST_ZIGZAG_SCAN( scan_8x8, level1, level2, dct8[0], 8 );
        TEST_ZIGZAG_SCAN( scan_4x4, level1, level2, dct1[0], 4 );
        TEST_ZIGZAG_SUB( sub_4x4, level1, level2, 16 );
        TEST_ZIGZAG_SUB( sub_8x8, level1, level2, 64 );
        TEST_ZIGZAG_SUBAC( sub_4x4ac, level1, level2 );
        report( interlace ? "zigzag_field :" : "zigzag_frame :" );
    }
#undef TEST_ZIGZAG_SCAN
#undef TEST_ZIGZAG_SUB

    return ret;
}


#define SHIFT(x,s) ((s)<=0 ? (x)<<-(s) : ((x)+(1<<((s)-1)))>>(s))
#define DIV(n,d) (((n) + ((d)>>1)) / (d))

static const uint8_t dequant4_scale[6][3] =
{
    { 10, 13, 16 },
    { 11, 14, 18 },
    { 13, 16, 20 },
    { 14, 18, 23 },
    { 16, 20, 25 },
    { 18, 23, 29 }
};
static const uint16_t quant4_scale[6][3] =
{
    { 13107, 8066, 5243 },
    { 11916, 7490, 4660 },
    { 10082, 6554, 4194 },
    {  9362, 5825, 3647 },
    {  8192, 5243, 3355 },
    {  7282, 4559, 2893 },
};

static const uint8_t quant8_scan[16] =
{
    0,3,4,3, 3,1,5,1, 4,5,2,5, 3,1,5,1
};
static const uint8_t dequant8_scale[6][6] =
{
    { 20, 18, 32, 19, 25, 24 },
    { 22, 19, 35, 21, 28, 26 },
    { 26, 23, 42, 24, 33, 31 },
    { 28, 25, 45, 26, 35, 33 },
    { 32, 28, 51, 30, 40, 38 },
    { 36, 32, 58, 34, 46, 43 },
};
static const uint16_t quant8_scale[6][6] =
{
    { 13107, 11428, 20972, 12222, 16777, 15481 },
    { 11916, 10826, 19174, 11058, 14980, 14290 },
    { 10082,  8943, 15978,  9675, 12710, 11985 },
    {  9362,  8228, 14913,  8931, 11984, 11259 },
    {  8192,  7346, 13159,  7740, 10486,  9777 },
    {  7282,  6428, 11570,  6830,  9118,  8640 }
};

int vbench_cqm_init( uint8_t *scaling_list[8],  uint8_t   *chroma_qp_table )
{
    int def_quant4[6][16];
    int def_quant8[6][64];
    int def_dequant4[6][16];
    int def_dequant8[6][64];

    /* quantization matrix for decoding, [cqm][qp%6][coef] */
    int (*dequant4_mf[4])[16];   /* [4][6][16] */
    int (*dequant8_mf[4])[64];   /* [4][6][64] */

    /* quantization matrix for trellis, [cqm][qp][coef] */
    int (*unquant4_mf[4])[16];   /* [4][QP_MAX_SPEC+1][16] */
    int (*unquant8_mf[4])[64];   /* [4][QP_MAX_SPEC+1][64] */

    /* quantization matrix for deadzone */
    udctcoef        (*quant4_mf[4])[16];     /* [4][QP_MAX_SPEC+1][16] */
    udctcoef        (*quant8_mf[4])[64];     /* [4][QP_MAX_SPEC+1][64] */
    udctcoef        (*quant4_bias[4])[16];   /* [4][QP_MAX_SPEC+1][16] */
    udctcoef        (*quant8_bias[4])[64];   /* [4][QP_MAX_SPEC+1][64] */
    udctcoef        (*quant4_bias0[4])[16];  /* [4][QP_MAX_SPEC+1][16] */
    udctcoef        (*quant8_bias0[4])[64];  /* [4][QP_MAX_SPEC+1][64] */
    udctcoef        (*nr_offset_emergency)[4][64];



    int deadzone[4] = { 32 - 0,
                        32 - 1,
                        32 - 11, 32 - 21 };
    int max_qp_err = -1;
    int max_chroma_qp_err = -1;
    int min_qp_err = QP_MAX+1;
    int num_8x8_lists = 1 == CHROMA_444 ? 4
                      : 1 ? 2 : 0; /* Checkasm may segfault if optimized out by --chroma-format */

#define CQM_ALLOC( w, count, type )\
    for( int i = 0; i < count; i++ )\
    {\
        int size = w*w;\
        int start = w == 8 ? 4 : 0;\
        int j;\
        for( j = 0; j < i; j++ )\
            if( !memcmp( scaling_list[i+start], scaling_list[j+start], size*sizeof(uint8_t) ) )\
                break;\
        if( j < i )\
        {\
              quant##w##_mf[i] =   quant##w##_mf[j];\
            dequant##w##_mf[i] = dequant##w##_mf[j];\
            unquant##w##_mf[i] = unquant##w##_mf[j];\
        }\
        else\
        {\
              quant##w##_mf[i] = memalign( NATIVE_ALIGN, (QP_MAX_SPEC+1)*size*sizeof(udctcoef) );\
            dequant##w##_mf[i] = memalign( NATIVE_ALIGN, 6*size*sizeof(int) );\
            unquant##w##_mf[i] = memalign( NATIVE_ALIGN, (QP_MAX_SPEC+1)*size*sizeof(int) );\
        }\
        for( j = 0; j < i; j++ )\
            if( deadzone[j] == deadzone[i] &&\
                !memcmp( scaling_list[i+start], scaling_list[j+start], size*sizeof(uint8_t) ) )\
                break;\
        if( j < i )\
        {\
             quant##w##_bias[i] = quant##w##_bias[j];\
            quant##w##_bias0[i] = quant##w##_bias0[j];\
        }\
        else\
        {\
             quant##w##_bias[i]  = memalign( NATIVE_ALIGN,(QP_MAX_SPEC+1)*size*sizeof(udctcoef) );\
             quant##w##_bias0[i] = memalign( NATIVE_ALIGN, (QP_MAX_SPEC+1)*size*sizeof(udctcoef) );\
        }\
    }

    CQM_ALLOC( 4, 4, (int[6][16]) )
    CQM_ALLOC( 8, num_8x8_lists, (int[6][16]) )

    for( int q = 0; q < 6; q++ )
    {
        for( int i = 0; i < 16; i++ )
        {
            int j = (i&1) + ((i>>2)&1);
            def_dequant4[q][i] = dequant4_scale[q][j];
            def_quant4[q][i]   =   quant4_scale[q][j];
        }
        for( int i = 0; i < 64; i++ )
        {
            int j = quant8_scan[((i>>1)&12) | (i&3)];
            def_dequant8[q][i] = dequant8_scale[q][j];
            def_quant8[q][i]   =   quant8_scale[q][j];
        }
    }

    for( int q = 0; q < 6; q++ )
    {
        for( int i_list = 0; i_list < 4; i_list++ )
            for( int i = 0; i < 16; i++ )
            {
                   dequant4_mf[i_list][q][i] = def_dequant4[q][i] *scaling_list[i_list][i];
                     quant4_mf[i_list][q][i] = DIV(def_quant4[q][i] * 16, scaling_list[i_list][i]);
            }
        for( int i_list = 0; i_list < num_8x8_lists; i_list++ )
            for( int i = 0; i < 64; i++ )
            {
                   dequant8_mf[i_list][q][i] = def_dequant8[q][i] * scaling_list[4+i_list][i];
                     quant8_mf[i_list][q][i] = DIV(def_quant8[q][i] * 16, scaling_list[4+i_list][i]);
            }
    }
    for( int q = 0; q <= QP_MAX_SPEC; q++ )
    {
        int j;
        for( int i_list = 0; i_list < 4; i_list++ )
            for( int i = 0; i < 16; i++ )
            {
                unquant4_mf[i_list][q][i] = (1ULL << (q/6 + 15 + 8)) / quant4_mf[i_list][q%6][i];
                quant4_mf[i_list][q][i] = j = SHIFT(quant4_mf[i_list][q%6][i], q/6 - 1);
                if( !j )
                {
                    min_qp_err = MIN( min_qp_err, q );
                    continue;
                }
                // round to nearest, unless that would cause the deadzone to be negative
                quant4_bias[i_list][q][i] = MIN( DIV(deadzone[i_list]<<10, j), (1<<15)/j );
                quant4_bias0[i_list][q][i] = (1<<15)/j;
                if( j > 0xffff && q > max_qp_err && (i_list == CQM_4IY || i_list == CQM_4PY) )
                    max_qp_err = q;
                if( j > 0xffff && q > max_chroma_qp_err && (i_list == CQM_4IC || i_list == CQM_4PC) )
                    max_chroma_qp_err = q;
            }

            for( int i_list = 0; i_list < num_8x8_lists; i_list++ )
                for( int i = 0; i < 64; i++ )
                {
                    unquant8_mf[i_list][q][i] = (1ULL << (q/6 + 16 + 8)) / quant8_mf[i_list][q%6][i];
                    j = SHIFT(quant8_mf[i_list][q%6][i], q/6);
                    quant8_mf[i_list][q][i] = (uint16_t)j;

                    if( !j )
                    {
                        min_qp_err = MIN( min_qp_err, q );
                        continue;
                    }
                    quant8_bias[i_list][q][i] = MIN( DIV(deadzone[i_list]<<10, j), (1<<15)/j );
                    quant8_bias0[i_list][q][i] = (1<<15)/j;
                    if( j > 0xffff && q > max_qp_err && (i_list == CQM_8IY || i_list == CQM_8PY) )
                        max_qp_err = q;
                    if( j > 0xffff && q > max_chroma_qp_err && (i_list == CQM_8IC || i_list == CQM_8PC) )
                        max_chroma_qp_err = q;
                }
    }

    /* Emergency mode denoising. */
    vbench_emms();
    nr_offset_emergency = memalign( NATIVE_ALIGN,  sizeof(*nr_offset_emergency)*(QP_MAX-QP_MAX_SPEC) );
    for( int q = 0; q < QP_MAX - QP_MAX_SPEC; q++ )
        for( int cat = 0; cat < 3 + CHROMA444; cat++ )
        {
            int dct8x8 = cat&1;

            int size = dct8x8 ? 64 : 16;
            udctcoef *nr_offset = nr_offset_emergency[q][cat];
            /* Denoise chroma first (due to h264's chroma QP offset), then luma, then DC. */
            int dc_threshold =    (QP_MAX-QP_MAX_SPEC)*2/3;
            int luma_threshold =  (QP_MAX-QP_MAX_SPEC)*2/3;
            int chroma_threshold = 0;

            for( int i = 0; i < size; i++ )
            {
#define BIT_DEPTH 8
                int max = (1 << (7 + BIT_DEPTH)) - 1;
                /* True "emergency mode": remove all DCT coefficients */
                if( q == QP_MAX - QP_MAX_SPEC - 1 )
                {
                    nr_offset[i] = max;
                    continue;
                }

                int thresh = i == 0 ? dc_threshold : cat >= 2 ? chroma_threshold : luma_threshold;
                if( q < thresh )
                {
                    nr_offset[i] = 0;
                    continue;
                }
                double pos = (double)(q-thresh+1) / (QP_MAX - QP_MAX_SPEC - thresh);

                /* XXX: this math is largely tuned for /dev/random input. */
                double start = dct8x8 ? unquant8_mf[CQM_8PY][QP_MAX_SPEC][i]
                                      : unquant4_mf[CQM_4PY][QP_MAX_SPEC][i];
                /* Formula chosen as an exponential scale to vaguely mimic the effects
                 * of a higher quantizer. */
                double bias = (pow( 2, pos*(QP_MAX - QP_MAX_SPEC)/10. )*0.003-0.003) * start;
                nr_offset[i] = MIN( bias + 0.5, max );
            }
        }
#if 0
    if( !h->mb.b_lossless )
    {
        while( chroma_qp_table[SPEC_QP(param.rc.i_qp_min)] <= max_chroma_qp_err )
            param.rc.i_qp_min++;
        if( min_qp_err <= h->param.rc.i_qp_max )
            param.rc.i_qp_max = min_qp_err-1;
        if( max_qp_err >= h->param.rc.i_qp_min )
            param.rc.i_qp_min = max_qp_err+1;
    }
#endif
    return 0;
fail:
    vbench_cqm_delete( 
                    dequant4_mf[4][16],   /* [4][6][16] */
                    dequant8_mf[4][64],   /* [4][6][64] */
                    unquant4_mf[4][16],   /* [4][qp_max_spec+1][16] */
                    unquant8_mf[4][64],   /* [4][qp_max_spec+1][64] */
                    quant4_mf[4][16],     /* [4][qp_max_spec+1][16] */
                    quant8_mf[4][64],     /* [4][qp_max_spec+1][64] */
                    quant4_bias[4][16],   /* [4][qp_max_spec+1][16] */
                    quant8_bias[4][64],   /* [4][qp_max_spec+1][64] */
                    quant4_bias0[4][16],  /* [4][qp_max_spec+1][16] */
                    quant8_bias0[4][64],  /* [4][qp_max_spec+1][64] */
                    nr_offset_emergency[4][64]


            
            );
    return -1;
}

#define CQM_DELETE( n, max )\
    for( int i = 0; i < (max); i++ )\
    {\
        int j;\
        for( j = 0; j < i; j++ )\
            if( quant##n##_mf[i] == quant##n##_mf[j] )\
                break;\
        if( j == i )\
        {\
            free(   quant##n##_mf[i] );\
            free( dequant##n##_mf[i] );\
            free( unquant##n##_mf[i] );\
        }\
        for( j = 0; j < i; j++ )\
            if( quant##n##_bias[i] == quant##n##_bias[j] )\
                break;\
        if( j == i )\
        {\
            free( quant##n##_bias[i] );\
            free( quant##n##_bias0[i] );\
        }\
    }

void vbench_cqm_delete(
                    /* quantization matrix for decoding, [cqm][qp%6][coef] */
                    int             (*dequant4_mf[4])[16],   /* [4][6][16] */
                    int             (*dequant8_mf[4])[64],   /* [4][6][64] */
                    /* quantization matrix for trellis, [cqm][qp][coef] */
                    int             (*unquant4_mf[4])[16],   /* [4][qp_max_spec+1][16] */
                    int             (*unquant8_mf[4])[64],   /* [4][qp_max_spec+1][64] */
                    /* quantization matrix for deadzone */
                    udctcoef        (*quant4_mf[4])[16],     /* [4][qp_max_spec+1][16] */
                    udctcoef        (*quant8_mf[4])[64],     /* [4][qp_max_spec+1][64] */
                    udctcoef        (*quant4_bias[4])[16],   /* [4][qp_max_spec+1][16] */
                    udctcoef        (*quant8_bias[4])[64],   /* [4][qp_max_spec+1][64] */
                    udctcoef        (*quant4_bias0[4])[16],  /* [4][qp_max_spec+1][16] */
                    udctcoef        (*quant8_bias0[4])[64],  /* [4][qp_max_spec+1][64] */
                    udctcoef        (*nr_offset_emergency)[4][64]

        
        )
{
    CQM_DELETE( 4, 4 );
    CQM_DELETE( 8, CHROMA444 ? 4 : 2 );
    free( nr_offset_emergency );
}

