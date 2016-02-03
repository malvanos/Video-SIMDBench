/*****************************************************************************
 * bench_intra.c: run and measure the kernels of intra.
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
#include "common.h"
#include "osdep.h"



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

#define BENCH_RUNS 100  // tradeoff between accuracy and speed
#define BENCH_ALIGNS 16 // number of stack+heap data alignments (another accuracy vs speed tradeoff)
#define MAX_FUNCS 1000  // just has to be big enough to hold all the existing functions
#define MAX_CPUS 30     // number of different combinations of cpu flags

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
    bench_t vers[MAX_CPUS];
} bench_func_t;

int do_bench = 0;
int bench_pattern_len = 0;
const char *bench_pattern = "";
char func_name[100];
static bench_func_t benchs[MAX_FUNCS];

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

static int check_intra( int cpu_ref, int cpu_new )
{
    int ret = 0, ok = 1, used_asm = 0;
    ALIGNED_ARRAY_32( pixel, edge,[36] );
    ALIGNED_ARRAY_32( pixel, edge2,[36] );
    ALIGNED_ARRAY_32( pixel, fdec,[FDEC_STRIDE*20] );
    struct
    {
        x264_predict_t      predict_16x16[4+3];
        x264_predict_t      predict_8x8c[4+3];
        x264_predict_t      predict_8x16c[4+3];
        x264_predict8x8_t   predict_8x8[9+3];
        x264_predict_t      predict_4x4[9+3];
        x264_predict_8x8_filter_t predict_8x8_filter;
    } ip_c, ip_ref, ip_a;

    x264_predict_16x16_init( 0, ip_c.predict_16x16 );
    x264_predict_8x8c_init( 0, ip_c.predict_8x8c );
    x264_predict_8x16c_init( 0, ip_c.predict_8x16c );
    x264_predict_8x8_init( 0, ip_c.predict_8x8, &ip_c.predict_8x8_filter );
    x264_predict_4x4_init( 0, ip_c.predict_4x4 );

    x264_predict_16x16_init( cpu_ref, ip_ref.predict_16x16 );
    x264_predict_8x8c_init( cpu_ref, ip_ref.predict_8x8c );
    x264_predict_8x16c_init( cpu_ref, ip_ref.predict_8x16c );
    x264_predict_8x8_init( cpu_ref, ip_ref.predict_8x8, &ip_ref.predict_8x8_filter );
    x264_predict_4x4_init( cpu_ref, ip_ref.predict_4x4 );

    x264_predict_16x16_init( cpu_new, ip_a.predict_16x16 );
    x264_predict_8x8c_init( cpu_new, ip_a.predict_8x8c );
    x264_predict_8x16c_init( cpu_new, ip_a.predict_8x16c );
    x264_predict_8x8_init( cpu_new, ip_a.predict_8x8, &ip_a.predict_8x8_filter );
    x264_predict_4x4_init( cpu_new, ip_a.predict_4x4 );

    memcpy( fdec, pbuf1, 32*20 * sizeof(pixel) );\

    ip_c.predict_8x8_filter( fdec+48, edge, ALL_NEIGHBORS, ALL_NEIGHBORS );

#define INTRA_TEST( name, dir, w, h, align, bench, ... )\
    if( ip_a.name[dir] != ip_ref.name[dir] )\
    {\
        set_func_name( "intra_%s_%s", #name, intra_##name##_names[dir] );\
        used_asm = 1;\
        memcpy( pbuf3, fdec, FDEC_STRIDE*20 * sizeof(pixel) );\
        memcpy( pbuf4, fdec, FDEC_STRIDE*20 * sizeof(pixel) );\
        for( int a = 0; a < (do_bench ? 64/sizeof(pixel) : 1); a += align )\
        {\
            call_c##bench( ip_c.name[dir], pbuf3+48+a, ##__VA_ARGS__ );\
            call_a##bench( ip_a.name[dir], pbuf4+48+a, ##__VA_ARGS__ );\
            if( memcmp( pbuf3, pbuf4, FDEC_STRIDE*20 * sizeof(pixel) ) )\
            {\
                fprintf( stderr, #name "[%d] :  [FAILED]\n", dir );\
                ok = 0;\
                if( ip_c.name == (void *)ip_c.predict_8x8 )\
                {\
                    for( int k = -1; k < 16; k++ )\
                        printf( "%2x ", edge[16+k] );\
                    printf( "\n" );\
                }\
                for( int j = 0; j < h; j++ )\
                {\
                    if( ip_c.name == (void *)ip_c.predict_8x8 )\
                        printf( "%2x ", edge[14-j] );\
                    for( int k = 0; k < w; k++ )\
                        printf( "%2x ", pbuf4[48+k+j*FDEC_STRIDE] );\
                    printf( "\n" );\
                }\
                printf( "\n" );\
                for( int j = 0; j < h; j++ )\
                {\
                    if( ip_c.name == (void *)ip_c.predict_8x8 )\
                        printf( "   " );\
                    for( int k = 0; k < w; k++ )\
                        printf( "%2x ", pbuf3[48+k+j*FDEC_STRIDE] );\
                    printf( "\n" );\
                }\
                break;\
            }\
        }\
    }

    for( int i = 0; i < 12; i++ )
        INTRA_TEST(   predict_4x4, i,  4,  4,  4, );
    for( int i = 0; i < 7; i++ )
        INTRA_TEST(  predict_8x8c, i,  8,  8, 16, );
    for( int i = 0; i < 7; i++ )
        INTRA_TEST( predict_8x16c, i,  8, 16, 16, );
    for( int i = 0; i < 7; i++ )
        INTRA_TEST( predict_16x16, i, 16, 16, 16, );
    for( int i = 0; i < 12; i++ )
        INTRA_TEST(   predict_8x8, i,  8,  8,  8, , edge );

    set_func_name("intra_predict_8x8_filter");
    if( ip_a.predict_8x8_filter != ip_ref.predict_8x8_filter )
    {
        used_asm = 1;
        for( int i = 0; i < 32; i++ )
        {
            if( !(i&7) || ((i&MB_TOPRIGHT) && !(i&MB_TOP)) )
                continue;
            int neighbor = (i&24)>>1;
            memset( edge,  0, 36*sizeof(pixel) );
            memset( edge2, 0, 36*sizeof(pixel) );
            call_c( ip_c.predict_8x8_filter, pbuf1+48, edge,  neighbor, i&7 );
            call_a( ip_a.predict_8x8_filter, pbuf1+48, edge2, neighbor, i&7 );
            if( !(neighbor&MB_TOPLEFT) )
                edge[15] = edge2[15] = 0;
            if( memcmp( edge+7, edge2+7, (i&MB_TOPRIGHT ? 26 : i&MB_TOP ? 17 : 8) * sizeof(pixel) ) )
            {
                fprintf( stderr, "predict_8x8_filter :  [FAILED] %d %d\n", (i&24)>>1, i&7);
                ok = 0;
            }
        }
    }

#define EXTREMAL_PLANE( w, h ) \
    { \
        int max[7]; \
        for( int j = 0; j < 7; j++ ) \
            max[j] = test ? rand()&PIXEL_MAX : PIXEL_MAX; \
        fdec[48-1-FDEC_STRIDE] = (i&1)*max[0]; \
        for( int j = 0; j < w/2; j++ ) \
            fdec[48+j-FDEC_STRIDE] = (!!(i&2))*max[1]; \
        for( int j = w/2; j < w-1; j++ ) \
            fdec[48+j-FDEC_STRIDE] = (!!(i&4))*max[2]; \
        fdec[48+(w-1)-FDEC_STRIDE] = (!!(i&8))*max[3]; \
        for( int j = 0; j < h/2; j++ ) \
            fdec[48+j*FDEC_STRIDE-1] = (!!(i&16))*max[4]; \
        for( int j = h/2; j < h-1; j++ ) \
            fdec[48+j*FDEC_STRIDE-1] = (!!(i&32))*max[5]; \
        fdec[48+(h-1)*FDEC_STRIDE-1] = (!!(i&64))*max[6]; \
    }
    /* Extremal test case for planar prediction. */
    for( int test = 0; test < 100 && ok; test++ )
        for( int i = 0; i < 128 && ok; i++ )
        {
            EXTREMAL_PLANE(  8,  8 );
            INTRA_TEST(  predict_8x8c, I_PRED_CHROMA_P,  8,  8, 64, 1 );
            EXTREMAL_PLANE(  8, 16 );
            INTRA_TEST( predict_8x16c, I_PRED_CHROMA_P,  8, 16, 64, 1 );
            EXTREMAL_PLANE( 16, 16 );
            INTRA_TEST( predict_16x16,  I_PRED_16x16_P, 16, 16, 64, 1 );
        }
    report( "intra pred :" );
    return ret;
}


int run_benchmarks(int i){
    /* 32-byte alignment is guaranteed whenever it's useful, 
     * but some functions also vary in speed depending on %64 */
    return x264_stack_pagealign(check_all_flags, i*32 );
}

