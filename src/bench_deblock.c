/*****************************************************************************
 * bench_deblock.c: run and measure the kernels of deblock.
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

/* buf1, buf2: initialised to random data and shouldn't write into them */
extern uint8_t *buf1, *buf2;
/* buf3, buf4: used to store output */
extern uint8_t *buf3, *buf4;
/* pbuf1, pbuf2: initialised to random pixel data and shouldn't write into them. */
extern pixel *pbuf1, *pbuf2;
/* pbuf3, pbuf4: point to buf3, buf4, just for type convenience */
extern pixel *pbuf3, *pbuf4;



#define report( name ) { \
    if( used_asm ) \
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



#define BIT_DEPTH 8

int check_deblock( int cpu_ref, int cpu_new )
{
    vbench_deblock_function_t db_c;
    vbench_deblock_function_t db_ref;
    vbench_deblock_function_t db_a;
    int ret = 0, ok = 1, used_asm = 0;
    int alphas[36], betas[36];
    int8_t tcs[36][4];

    vbench_deblock_init( 0, &db_c, 0 );
    vbench_deblock_init( cpu_ref, &db_ref, 0 );
    vbench_deblock_init( cpu_new, &db_a, 0 );

    /* not exactly the real values of a,b,tc but close enough */
    for( int i = 35, a = 255, c = 250; i >= 0; i-- )
    {
        alphas[i] = a << (BIT_DEPTH-8);
        betas[i] = (i+1)/2 << (BIT_DEPTH-8);
        tcs[i][0] = tcs[i][3] = (c+6)/10 << (BIT_DEPTH-8);
        tcs[i][1] = (c+7)/15 << (BIT_DEPTH-8);
        tcs[i][2] = (c+9)/20 << (BIT_DEPTH-8);
        a = a*9/10;
        c = c*9/10;
    }

#define TEST_DEBLOCK( name, align, ... ) \
    for( int i = 0; i < 36; i++ ) \
    { \
        intptr_t off = 8*32 + (i&15)*4*!align; /* benchmark various alignments of h filter */ \
        for( int j = 0; j < 1024; j++ ) \
            /* two distributions of random to excersize different failure modes */ \
            pbuf3[j] = rand() & (i&1 ? 0xf : PIXEL_MAX ); \
        memcpy( pbuf4, pbuf3, 1024 * sizeof(pixel) ); \
        if( db_a.name != db_ref.name ) \
        { \
            set_func_name( #name ); \
            used_asm = 1; \
            call_c1( db_c.name, pbuf3+off, (intptr_t)32, alphas[i], betas[i], ##__VA_ARGS__ ); \
            call_a1( db_a.name, pbuf4+off, (intptr_t)32, alphas[i], betas[i], ##__VA_ARGS__ ); \
            if( memcmp( pbuf3, pbuf4, 1024 * sizeof(pixel) ) ) \
            { \
                ok = 0; \
                fprintf( stderr, #name "(a=%d, b=%d): [FAILED]\n", alphas[i], betas[i] ); \
                break; \
            } \
            call_c2( db_c.name, pbuf3+off, (intptr_t)32, alphas[i], betas[i], ##__VA_ARGS__ ); \
            call_a2( db_a.name, pbuf4+off, (intptr_t)32, alphas[i], betas[i], ##__VA_ARGS__ ); \
        } \
    }

    TEST_DEBLOCK( deblock_luma[0], 0, tcs[i] );
    TEST_DEBLOCK( deblock_luma[1], 1, tcs[i] );
    TEST_DEBLOCK( deblock_h_chroma_420, 0, tcs[i] );
    TEST_DEBLOCK( deblock_h_chroma_422, 0, tcs[i] );
    TEST_DEBLOCK( deblock_chroma_420_mbaff, 0, tcs[i] );
    TEST_DEBLOCK( deblock_chroma_422_mbaff, 0, tcs[i] );
    TEST_DEBLOCK( deblock_chroma[1], 1, tcs[i] );
    TEST_DEBLOCK( deblock_luma_intra[0], 0 );
    TEST_DEBLOCK( deblock_luma_intra[1], 1 );
    TEST_DEBLOCK( deblock_h_chroma_420_intra, 0 );
    TEST_DEBLOCK( deblock_h_chroma_422_intra, 0 );
    TEST_DEBLOCK( deblock_chroma_420_intra_mbaff, 0 );
    TEST_DEBLOCK( deblock_chroma_422_intra_mbaff, 0 );
    TEST_DEBLOCK( deblock_chroma_intra[1], 1 );

    if( db_a.deblock_strength != db_ref.deblock_strength )
    {
        for( int i = 0; i < 100; i++ )
        {
            ALIGNED_ARRAY_16( uint8_t, nnz, [SCAN8_SIZE] );
            ALIGNED_4( int8_t ref[2][SCAN8_LUMA_SIZE] );
            ALIGNED_ARRAY_16( int16_t, mv, [2],[SCAN8_LUMA_SIZE][2] );
            ALIGNED_ARRAY_N( uint8_t, bs, [2],[2][8][4] );
            memset( bs, 99, sizeof(uint8_t)*2*4*8*2 );
            for( int j = 0; j < SCAN8_SIZE; j++ )
                nnz[j] = ((rand()&7) == 7) * rand() & 0xf;
            for( int j = 0; j < 2; j++ )
                for( int k = 0; k < SCAN8_LUMA_SIZE; k++ )
                {
                    ref[j][k] = ((rand()&3) != 3) ? 0 : (rand() & 31) - 2;
                    for( int l = 0; l < 2; l++ )
                        mv[j][k][l] = ((rand()&7) != 7) ? (rand()&7) - 3 : (rand()&1023) - 512;
                }
            set_func_name( "deblock_strength" );
            call_c( db_c.deblock_strength, nnz, ref, mv, bs[0], 2<<(i&1), ((i>>1)&1) );
            call_a( db_a.deblock_strength, nnz, ref, mv, bs[1], 2<<(i&1), ((i>>1)&1) );
            if( memcmp( bs[0], bs[1], sizeof(uint8_t)*2*4*8 ) )
            {
                ok = 0;
                fprintf( stderr, "deblock_strength: [FAILED]\n" );
                for( int j = 0; j < 2; j++ )
                {
                    for( int k = 0; k < 2; k++ )
                        for( int l = 0; l < 4; l++ )
                        {
                            for( int m = 0; m < 4; m++ )
                                printf("%d ",bs[j][k][l][m]);
                            printf("\n");
                        }
                    printf("\n");
                }
                break;
            }
        }
    }

    report( "deblock :" );

    return ret;
}


