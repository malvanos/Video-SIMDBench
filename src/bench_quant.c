/*****************************************************************************
 * bench_quant.c: run and measure the kernels
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
#include "osdep.h"
#include "macroblock.h"

/* buf1, buf2: initialised to random data and shouldn't write into them */
extern uint8_t *buf1, *buf2;
/* buf3, buf4: used to store output */
extern uint8_t *buf3, *buf4;
/* pbuf1, pbuf2: initialised to random pixel data and shouldn't write into them. */
extern pixel *pbuf1, *pbuf2;
/* pbuf3, pbuf4: point to buf3, buf4, just for type convenience */
extern pixel *pbuf3, *pbuf4;


extern int bench_pattern_len;
extern const char *bench_pattern;
char func_name[100];
extern  bench_func_t benchs[MAX_FUNCS];

#define set_func_name(...) snprintf( func_name, sizeof(func_name), __VA_ARGS__ )

#define HAVE_X86_INLINE_ASM 1


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

static int cmp_nop( const void *a, const void *b )
{
    return *(uint16_t*)a - *(uint16_t*)b;
}

static int cmp_bench( const void *a, const void *b )
{
    // asciibetical sort except preserving numbers
    const char *sa = ((bench_func_t*)a)->name;
    const char *sb = ((bench_func_t*)b)->name;
    for( ;; sa++, sb++ )
    {
        if( !*sa && !*sb )
            return 0;
        if( isdigit( *sa ) && isdigit( *sb ) && isdigit( sa[1] ) != isdigit( sb[1] ) )
            return isdigit( sa[1] ) - isdigit( sb[1] );
        if( *sa != *sb )
            return *sa - *sb;
    }
}



int check_quant( int cpu_ref, int cpu_new )
{
    vbench_quant_function_t qf_c;
    vbench_quant_function_t qf_ref;
    vbench_quant_function_t qf_a;
    ALIGNED_ARRAY_N( dctcoef, dct1,[64] );
    ALIGNED_ARRAY_N( dctcoef, dct2,[64] );
    ALIGNED_ARRAY_N( dctcoef, dct3,[8],[16] );
    ALIGNED_ARRAY_N( dctcoef, dct4,[8],[16] );
    ALIGNED_ARRAY_N( uint8_t, cqm_buf,[64] );
    int ret = 0, ok, used_asm;
    int oks[3] = {1,1,1}, used_asms[3] = {0,0,0};
    int i_cqm_preset;

    /* These are normally in x264 but we are not going to used them */
    int def_quant4[6][16];
    int def_quant8[6][64];
    int def_dequant4[6][16];
    int def_dequant8[6][64];

    /* quantization matrix for decoding, [cqm][qp%6][coef] */
    ALIGNED_32(int dequant4_mf[4][6][16]);   /* [4][6][16] */
    ALIGNED_32(int dequant8_mf[4][6][64]);   /* [4][6][64] */

    /* quantization matrix for trellis, [cqm][qp][coef] */
    ALIGNED_32(int unquant4_mf[4][QP_MAX_SPEC+1][16]);   /* [4][QP_MAX_SPEC+1][16] */
    ALIGNED_32(int unquant8_mf[4][QP_MAX_SPEC+1][64]);   /* [4][QP_MAX_SPEC+1][64] */

    /* quantization matrix for deadzone */
    ALIGNED_32( udctcoef        quant4_mf[4][QP_MAX_SPEC+1][64]);     /* [4][QP_MAX_SPEC+1][16] */
    ALIGNED_32( udctcoef        quant8_mf[4][QP_MAX_SPEC+1][64]);     /* [4][QP_MAX_SPEC+1][64] */
    ALIGNED_32( udctcoef        quant4_bias[4][QP_MAX_SPEC+1][64]);   /* [4][QP_MAX_SPEC+1][16] */
    ALIGNED_32( udctcoef        quant8_bias[4][QP_MAX_SPEC+1][64]);   /* [4][QP_MAX_SPEC+1][64] */
    ALIGNED_32( udctcoef        quant4_bias0[4][QP_MAX_SPEC+1][64]);  /* [4][QP_MAX_SPEC+1][16] */
    ALIGNED_32( udctcoef        quant8_bias0[4][QP_MAX_SPEC+1][64]);  /* [4][QP_MAX_SPEC+1][64] */

#if 0
    x264_t h_buf;
    x264_t *h = &h_buf;
    memset( h, 0, sizeof(*h) );
    h->sps->i_chroma_format_idc = 1;
    x264_param_default( &h->param );
    h->chroma_qp_table = i_chroma_qp_table + 12;
    h->param.analyse.b_transform_8x8 = 1;
#endif
    for( int i_cqm = 0; i_cqm < 4; i_cqm++ )
    {
        if( i_cqm == 0 )
        {
            //for( int i = 0; i < 6; i++ )
            //    h->pps->scaling_list[i] = x264_cqm_flat16;
            //h->param.i_cqm_preset = h->pps->i_cqm_preset = X264_CQM_FLAT;
            i_cqm_preset = CQM_FLAT;
        }
        else if( i_cqm == 1 )
        {
            //for( int i = 0; i < 6; i++ )
            //    h->pps->scaling_list[i] = x264_cqm_jvt[i];
            //h->param.i_cqm_preset = h->pps->i_cqm_preset = X264_CQM_JVT;
            i_cqm_preset = CQM_JVT;
        }
        else
        {
#if 0
            int max_scale = BIT_DEPTH < 10 ? 255 : 228;
            if( i_cqm == 2 )
                for( int i = 0; i < 64; i++ )
                    cqm_buf[i] = 10 + rand() % (max_scale - 9);
            else
                for( int i = 0; i < 64; i++ )
                    cqm_buf[i] = 1;
            for( int i = 0; i < 6; i++ )
                h->pps->scaling_list[i] = cqm_buf;
            h->param.i_cqm_preset = h->pps->i_cqm_preset = X264_CQM_CUSTOM;
#endif
            i_cqm_preset = CQM_CUSTOM;
        }

        //h->param.rc.i_qp_min = 0;
        //h->param.rc.i_qp_max = QP_MAX_SPEC;
        int i_qp_min = 0;
        int i_qp_max = QP_MAX_SPEC;
        //x264_cqm_init( h ); // FIXME: we need to fix this function
        //
        vbench_quant_init( i_cqm_preset, 0, &qf_c );
        vbench_quant_init( i_cqm_preset, cpu_ref, &qf_ref );
        vbench_quant_init( i_cqm_preset, cpu_new, &qf_a );

#define INIT_QUANT8(j,max) \
        { \
            static const int scale1d[8] = {32,31,24,31,32,31,24,31}; \
            for( int i = 0; i < max; i++ ) \
            { \
                unsigned int scale = (255*scale1d[(i>>3)&7]*scale1d[i&7])/16; \
                dct1[i] = dct2[i] = (j>>(i>>6))&1 ? (rand()%(2*scale+1))-scale : 0; \
            } \
        }

#define INIT_QUANT4(j,max) \
        { \
            static const int scale1d[4] = {4,6,4,6}; \
            for( int i = 0; i < max; i++ ) \
            { \
                unsigned int scale = 255*scale1d[(i>>2)&3]*scale1d[i&3]; \
                dct1[i] = dct2[i] = (j>>(i>>4))&1 ? (rand()%(2*scale+1))-scale : 0; \
            } \
        }

#define TEST_QUANT_DC( name, cqm ) \
        if( qf_a.name != qf_ref.name ) \
        { \
            set_func_name( #name ); \
            used_asms[0] = 1; \
            for( int qp = i_qp_max; qp >= i_qp_min; qp-- ) \
            { \
                for( int j = 0; j < 2; j++ ) \
                { \
                    int result_c, result_a; \
                    for( int i = 0; i < 16; i++ ) \
                        dct1[i] = dct2[i] = j ? (rand() & 0x1fff) - 0xfff : 0; \
                    result_c = call_c1( qf_c.name, dct1, quant4_mf[CQM_4IY][qp][0], quant4_bias[CQM_4IY][qp][0] ); \
                    result_a = call_a1( qf_a.name, dct2, quant4_mf[CQM_4IY][qp][0], quant4_bias[CQM_4IY][qp][0] ); \
                    if( memcmp( dct1, dct2, 16*sizeof(dctcoef) ) || result_c != result_a ) \
                    { \
                        oks[0] = 0; \
                        fprintf( stderr, #name "(cqm=%d): [FAILED]\n", i_cqm ); \
                        break; \
                    } \
                    call_c2( qf_c.name, dct1, quant4_mf[CQM_4IY][qp][0], quant4_bias[CQM_4IY][qp][0] ); \
                    call_a2( qf_a.name, dct2, quant4_mf[CQM_4IY][qp][0], quant4_bias[CQM_4IY][qp][0] ); \
                } \
            } \
        }

#define TEST_QUANT( qname, block, type, w, maxj ) \
        if( qf_a.qname != qf_ref.qname ) \
        { \
            set_func_name( #qname ); \
            used_asms[0] = 1; \
            for( int qp = i_qp_max; qp >= i_qp_min; qp-- ) \
            { \
                for( int j = 0; j < maxj; j++ ) \
                { \
                    INIT_QUANT##type(j, w*w) \
                    int result_c = call_c1( qf_c.qname, (void*)dct1, quant##type##_mf[block][qp], quant##type##_bias[block][qp] ); \
                    int result_a = call_a1( qf_a.qname, (void*)dct2, quant##type##_mf[block][qp], quant##type##_bias[block][qp] ); \
                    if( memcmp( dct1, dct2, w*w*sizeof(dctcoef) ) || result_c != result_a ) \
                    { \
                        oks[0] = 0; \
                        fprintf( stderr, #qname "(qp=%d, cqm=%d, block="#block"): [FAILED]\n", qp, i_cqm ); \
                        break; \
                    } \
                    call_c2( qf_c.qname, (void*)dct1, quant##type##_mf[block][qp], quant##type##_bias[block][qp] ); \
                    call_a2( qf_a.qname, (void*)dct2, quant##type##_mf[block][qp], quant##type##_bias[block][qp] ); \
                } \
            } \
        }

        TEST_QUANT( quant_8x8, CQM_8IY, 8, 8, 2 );
        TEST_QUANT( quant_8x8, CQM_8PY, 8, 8, 2 );
        TEST_QUANT( quant_4x4, CQM_4IY, 4, 4, 2 );
        TEST_QUANT( quant_4x4, CQM_4PY, 4, 4, 2 );
        TEST_QUANT( quant_4x4x4, CQM_4IY, 4, 8, 16 );
        TEST_QUANT( quant_4x4x4, CQM_4PY, 4, 8, 16 );
        TEST_QUANT_DC( quant_4x4_dc, **h->quant4_mf[CQM_4IY] );
        TEST_QUANT_DC( quant_2x2_dc, **h->quant4_mf[CQM_4IC] );

#if 0
#define TEST_DEQUANT( qname, dqname, block, w ) \
        if( qf_a.dqname != qf_ref.dqname ) \
        { \
            set_func_name( "%s_%s", #dqname, i_cqm?"cqm":"flat" ); \
            used_asms[1] = 1; \
            for( int qp = h->param.rc.i_qp_max; qp >= h->param.rc.i_qp_min; qp-- ) \
            { \
                INIT_QUANT##w(1, w*w) \
                qf_c.qname( dct1, h->quant##w##_mf[block][qp], h->quant##w##_bias[block][qp] ); \
                memcpy( dct2, dct1, w*w*sizeof(dctcoef) ); \
                call_c1( qf_c.dqname, dct1, h->dequant##w##_mf[block], qp ); \
                call_a1( qf_a.dqname, dct2, h->dequant##w##_mf[block], qp ); \
                if( memcmp( dct1, dct2, w*w*sizeof(dctcoef) ) ) \
                { \
                    oks[1] = 0; \
                    fprintf( stderr, #dqname "(qp=%d, cqm=%d, block="#block"): [FAILED]\n", qp, i_cqm ); \
                    break; \
                } \
                call_c2( qf_c.dqname, dct1, h->dequant##w##_mf[block], qp ); \
                call_a2( qf_a.dqname, dct2, h->dequant##w##_mf[block], qp ); \
            } \
        }

        TEST_DEQUANT( quant_8x8, dequant_8x8, CQM_8IY, 8 );
        TEST_DEQUANT( quant_8x8, dequant_8x8, CQM_8PY, 8 );
        TEST_DEQUANT( quant_4x4, dequant_4x4, CQM_4IY, 4 );
        TEST_DEQUANT( quant_4x4, dequant_4x4, CQM_4PY, 4 );

#define TEST_DEQUANT_DC( qname, dqname, block, w ) \
        if( qf_a.dqname != qf_ref.dqname ) \
        { \
            set_func_name( "%s_%s", #dqname, i_cqm?"cqm":"flat" ); \
            used_asms[1] = 1; \
            for( int qp = h->param.rc.i_qp_max; qp >= h->param.rc.i_qp_min; qp-- ) \
            { \
                for( int i = 0; i < 16; i++ ) \
                    dct1[i] = rand()%(PIXEL_MAX*16*2+1) - PIXEL_MAX*16; \
                qf_c.qname( dct1, h->quant##w##_mf[block][qp][0]>>1, h->quant##w##_bias[block][qp][0]>>1 ); \
                memcpy( dct2, dct1, w*w*sizeof(dctcoef) ); \
                call_c1( qf_c.dqname, dct1, h->dequant##w##_mf[block], qp ); \
                call_a1( qf_a.dqname, dct2, h->dequant##w##_mf[block], qp ); \
                if( memcmp( dct1, dct2, w*w*sizeof(dctcoef) ) ) \
                { \
                    oks[1] = 0; \
                    fprintf( stderr, #dqname "(qp=%d, cqm=%d, block="#block"): [FAILED]\n", qp, i_cqm ); \
                } \
                call_c2( qf_c.dqname, dct1, h->dequant##w##_mf[block], qp ); \
                call_a2( qf_a.dqname, dct2, h->dequant##w##_mf[block], qp ); \
            } \
        }

        TEST_DEQUANT_DC( quant_4x4_dc, dequant_4x4_dc, CQM_4IY, 4 );

        if( qf_a.idct_dequant_2x4_dc != qf_ref.idct_dequant_2x4_dc )
        {
            set_func_name( "idct_dequant_2x4_dc_%s", i_cqm?"cqm":"flat" );
            used_asms[1] = 1;
            for( int qp = h->param.rc.i_qp_max; qp >= h->param.rc.i_qp_min; qp-- )
            {
                for( int i = 0; i < 8; i++ )
                    dct1[i] = rand()%(PIXEL_MAX*16*2+1) - PIXEL_MAX*16;
                qf_c.quant_2x2_dc( &dct1[0], h->quant4_mf[CQM_4IC][qp+3][0]>>1, h->quant4_bias[CQM_4IC][qp+3][0]>>1 );
                qf_c.quant_2x2_dc( &dct1[4], h->quant4_mf[CQM_4IC][qp+3][0]>>1, h->quant4_bias[CQM_4IC][qp+3][0]>>1 );
                call_c( qf_c.idct_dequant_2x4_dc, dct1, dct3, h->dequant4_mf[CQM_4IC], qp+3 );
                call_a( qf_a.idct_dequant_2x4_dc, dct1, dct4, h->dequant4_mf[CQM_4IC], qp+3 );
                for( int i = 0; i < 8; i++ )
                    if( dct3[i][0] != dct4[i][0] )
                    {
                        oks[1] = 0;
                        fprintf( stderr, "idct_dequant_2x4_dc (qp=%d, cqm=%d): [FAILED]\n", qp, i_cqm );
                        break;
                    }
            }
        }

        if( qf_a.idct_dequant_2x4_dconly != qf_ref.idct_dequant_2x4_dconly )
        {
            set_func_name( "idct_dequant_2x4_dc_%s", i_cqm?"cqm":"flat" );
            used_asms[1] = 1;
            for( int qp = h->param.rc.i_qp_max; qp >= h->param.rc.i_qp_min; qp-- )
            {
                for( int i = 0; i < 8; i++ )
                    dct1[i] = rand()%(PIXEL_MAX*16*2+1) - PIXEL_MAX*16;
                qf_c.quant_2x2_dc( &dct1[0], h->quant4_mf[CQM_4IC][qp+3][0]>>1, h->quant4_bias[CQM_4IC][qp+3][0]>>1 );
                qf_c.quant_2x2_dc( &dct1[4], h->quant4_mf[CQM_4IC][qp+3][0]>>1, h->quant4_bias[CQM_4IC][qp+3][0]>>1 );
                memcpy( dct2, dct1, 8*sizeof(dctcoef) );
                call_c1( qf_c.idct_dequant_2x4_dconly, dct1, h->dequant4_mf[CQM_4IC], qp+3 );
                call_a1( qf_a.idct_dequant_2x4_dconly, dct2, h->dequant4_mf[CQM_4IC], qp+3 );
                if( memcmp( dct1, dct2, 8*sizeof(dctcoef) ) )
                {
                    oks[1] = 0;
                    fprintf( stderr, "idct_dequant_2x4_dconly (qp=%d, cqm=%d): [FAILED]\n", qp, i_cqm );
                    break;
                }
                call_c2( qf_c.idct_dequant_2x4_dconly, dct1, h->dequant4_mf[CQM_4IC], qp+3 );
                call_a2( qf_a.idct_dequant_2x4_dconly, dct2, h->dequant4_mf[CQM_4IC], qp+3 );
            }
        }

#define TEST_OPTIMIZE_CHROMA_DC( optname, size ) \
        if( qf_a.optname != qf_ref.optname ) \
        { \
            set_func_name( #optname ); \
            used_asms[2] = 1; \
            for( int qp = h->param.rc.i_qp_max; qp >= h->param.rc.i_qp_min; qp-- ) \
            { \
                int qpdc = qp + (size == 8 ? 3 : 0); \
                int dmf = h->dequant4_mf[CQM_4IC][qpdc%6][0] << qpdc/6; \
                if( dmf > 32*64 ) \
                    continue; \
                for( int i = 16; ; i <<= 1 ) \
                { \
                    int res_c, res_asm; \
                    int max = X264_MIN( i, PIXEL_MAX*16 ); \
                    for( int j = 0; j < size; j++ ) \
                        dct1[j] = rand()%(max*2+1) - max; \
                    for( int j = 0; i <= size; j += 4 ) \
                        qf_c.quant_2x2_dc( &dct1[j], h->quant4_mf[CQM_4IC][qpdc][0]>>1, h->quant4_bias[CQM_4IC][qpdc][0]>>1 ); \
                    memcpy( dct2, dct1, size*sizeof(dctcoef) ); \
                    res_c   = call_c1( qf_c.optname, dct1, dmf ); \
                    res_asm = call_a1( qf_a.optname, dct2, dmf ); \
                    if( res_c != res_asm || memcmp( dct1, dct2, size*sizeof(dctcoef) ) ) \
                    { \
                        oks[2] = 0; \
                        fprintf( stderr, #optname "(qp=%d, res_c=%d, res_asm=%d): [FAILED]\n", qp, res_c, res_asm ); \
                    } \
                    call_c2( qf_c.optname, dct1, dmf ); \
                    call_a2( qf_a.optname, dct2, dmf ); \
                    if( i >= PIXEL_MAX*16 ) \
                        break; \
                } \
            } \
        }

        TEST_OPTIMIZE_CHROMA_DC( optimize_chroma_2x2_dc, 4 );
        TEST_OPTIMIZE_CHROMA_DC( optimize_chroma_2x4_dc, 8 );
#endif
        //x264_cqm_delete( h );
    }
#if 0
    ok = oks[0]; used_asm = used_asms[0];
    report( "quant :" );

    ok = oks[1]; used_asm = used_asms[1];
    report( "dequant :" );

    ok = oks[2]; used_asm = used_asms[2];
    report( "optimize chroma dc :" );

    ok = 1; used_asm = 0;
    if( qf_a.denoise_dct != qf_ref.denoise_dct )
    {
        used_asm = 1;
        for( int size = 16; size <= 64; size += 48 )
        {
            set_func_name( "denoise_dct" );
            memcpy( dct1, buf1, size*sizeof(dctcoef) );
            memcpy( dct2, buf1, size*sizeof(dctcoef) );
            memcpy( buf3+256, buf3, 256 );
            call_c1( qf_c.denoise_dct, dct1, (uint32_t*)buf3,       (udctcoef*)buf2, size );
            call_a1( qf_a.denoise_dct, dct2, (uint32_t*)(buf3+256), (udctcoef*)buf2, size );
            if( memcmp( dct1, dct2, size*sizeof(dctcoef) ) || memcmp( buf3+4, buf3+256+4, (size-1)*sizeof(uint32_t) ) )
                ok = 0;
            call_c2( qf_c.denoise_dct, dct1, (uint32_t*)buf3,       (udctcoef*)buf2, size );
            call_a2( qf_a.denoise_dct, dct2, (uint32_t*)(buf3+256), (udctcoef*)buf2, size );
        }
    }
    report( "denoise dct :" );

#define TEST_DECIMATE( decname, w, ac, thresh ) \
    if( qf_a.decname != qf_ref.decname ) \
    { \
        set_func_name( #decname ); \
        used_asm = 1; \
        for( int i = 0; i < 100; i++ ) \
        { \
            static const int distrib[16] = {1,1,1,1,1,1,1,1,1,1,1,1,2,3,4};\
            static const int zerorate_lut[4] = {3,7,15,31};\
            int zero_rate = zerorate_lut[i&3];\
            for( int idx = 0; idx < w*w; idx++ ) \
            { \
                int sign = (rand()&1) ? -1 : 1; \
                int abs_level = distrib[rand()&15]; \
                if( abs_level == 4 ) abs_level = rand()&0x3fff; \
                int zero = !(rand()&zero_rate); \
                dct1[idx] = zero * abs_level * sign; \
            } \
            if( ac ) \
                dct1[0] = 0; \
            int result_c = call_c( qf_c.decname, dct1 ); \
            int result_a = call_a( qf_a.decname, dct1 ); \
            if( X264_MIN(result_c,thresh) != X264_MIN(result_a,thresh) ) \
            { \
                ok = 0; \
                fprintf( stderr, #decname ": [FAILED]\n" ); \
                break; \
            } \
        } \
    }

    ok = 1; used_asm = 0;
    TEST_DECIMATE( decimate_score64, 8, 0, 6 );
    TEST_DECIMATE( decimate_score16, 4, 0, 6 );
    TEST_DECIMATE( decimate_score15, 4, 1, 7 );
    report( "decimate_score :" );

#define TEST_LAST( last, lastname, size, ac ) \
    if( qf_a.last != qf_ref.last ) \
    { \
        set_func_name( #lastname ); \
        used_asm = 1; \
        for( int i = 0; i < 100; i++ ) \
        { \
            int nnz = 0; \
            int max = rand() & (size-1); \
            memset( dct1, 0, size*sizeof(dctcoef) ); \
            for( int idx = ac; idx < max; idx++ ) \
                nnz |= dct1[idx] = !(rand()&3) + (!(rand()&15))*rand(); \
            if( !nnz ) \
                dct1[ac] = 1; \
            int result_c = call_c( qf_c.last, dct1+ac ); \
            int result_a = call_a( qf_a.last, dct1+ac ); \
            if( result_c != result_a ) \
            { \
                ok = 0; \
                fprintf( stderr, #lastname ": [FAILED]\n" ); \
                break; \
            } \
        } \
    }

    ok = 1; used_asm = 0;
    TEST_LAST( coeff_last4              , coeff_last4,   4, 0 );
    TEST_LAST( coeff_last8              , coeff_last8,   8, 0 );
    TEST_LAST( coeff_last[  DCT_LUMA_AC], coeff_last15, 16, 1 );
    TEST_LAST( coeff_last[ DCT_LUMA_4x4], coeff_last16, 16, 0 );
    TEST_LAST( coeff_last[ DCT_LUMA_8x8], coeff_last64, 64, 0 );
    report( "coeff_last :" );

#define TEST_LEVELRUN( lastname, name, size, ac ) \
    if( qf_a.lastname != qf_ref.lastname ) \
    { \
        set_func_name( #name ); \
        used_asm = 1; \
        for( int i = 0; i < 100; i++ ) \
        { \
            x264_run_level_t runlevel_c, runlevel_a; \
            int nnz = 0; \
            int max = rand() & (size-1); \
            memset( dct1, 0, size*sizeof(dctcoef) ); \
            memcpy( &runlevel_a, buf1+i, sizeof(x264_run_level_t) ); \
            memcpy( &runlevel_c, buf1+i, sizeof(x264_run_level_t) ); \
            for( int idx = ac; idx < max; idx++ ) \
                nnz |= dct1[idx] = !(rand()&3) + (!(rand()&15))*rand(); \
            if( !nnz ) \
                dct1[ac] = 1; \
            int result_c = call_c( qf_c.lastname, dct1+ac, &runlevel_c ); \
            int result_a = call_a( qf_a.lastname, dct1+ac, &runlevel_a ); \
            if( result_c != result_a || runlevel_c.last != runlevel_a.last || \
                runlevel_c.mask != runlevel_a.mask || \
                memcmp(runlevel_c.level, runlevel_a.level, sizeof(dctcoef)*result_c)) \
            { \
                ok = 0; \
                fprintf( stderr, #name ": [FAILED]\n" ); \
                break; \
            } \
        } \
    }

    ok = 1; used_asm = 0;
    TEST_LEVELRUN( coeff_level_run4              , coeff_level_run4,   4, 0 );
    TEST_LEVELRUN( coeff_level_run8              , coeff_level_run8,   8, 0 );
    TEST_LEVELRUN( coeff_level_run[  DCT_LUMA_AC], coeff_level_run15, 16, 1 );
    TEST_LEVELRUN( coeff_level_run[ DCT_LUMA_4x4], coeff_level_run16, 16, 0 );
    report( "coeff_level_run :" );
 
    return ret;
#endif
    return 0;
}

