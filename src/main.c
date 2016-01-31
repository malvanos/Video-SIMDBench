/*****************************************************************************
 * bench.c: run and measure the kernels
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
 *  Version - Changes
 *
 *  0.1     - Leave only code for handling main.
 *
 *****************************************************************************/


#include <ctype.h>

#include "common.h"
#include "bench.h"
#include "osdep.h"



// GCC doesn't align stack variables on ARM, so use .bss
#if ARCH_ARM
#undef ALIGNED_16
#define ALIGNED_16( var ) DECLARE_ALIGNED( static var, 16 )
#endif

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



int do_bench = 0;
int bench_pattern_len = 0;
const char *bench_pattern = "";
char func_name[100];
static bench_func_t benchs[MAX_FUNCS];


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

static void print_bench(void)
{
    uint16_t nops[10000];
    int nfuncs, nop_time=0;

    for( int i = 0; i < 10000; i++ )
    {
        uint32_t t = read_time();
        nops[i] = read_time() - t;
    }
    qsort( nops, 10000, sizeof(uint16_t), cmp_nop );
    for( int i = 500; i < 9500; i++ )
        nop_time += nops[i];
    nop_time /= 900;
    printf( "nop: %d\n", nop_time );

    for( nfuncs = 0; nfuncs < MAX_FUNCS && benchs[nfuncs].name; nfuncs++ );
    qsort( benchs, nfuncs, sizeof(bench_func_t), cmp_bench );
    for( int i = 0; i < nfuncs; i++ )
        for( int j = 0; j < MAX_CPUS && (!j || benchs[i].vers[j].cpu); j++ )
        {
            int k;
            bench_t *b = &benchs[i].vers[j];
            if( !b->den )
                continue;
            for( k = 0; k < j && benchs[i].vers[k].pointer != b->pointer; k++ );
            if( k < j )
                continue;
            printf( "%s_%s%s: %ld\n", benchs[i].name,
#if HAVE_MMX
                    b->cpu&VSIMD_CPU_AVX2 ? "avx2" :
                    b->cpu&VSIMD_CPU_FMA3 ? "fma3" :
                    b->cpu&VSIMD_CPU_FMA4 ? "fma4" :
                    b->cpu&VSIMD_CPU_XOP ? "xop" :
                    b->cpu&VSIMD_CPU_AVX ? "avx" :
                    b->cpu&VSIMD_CPU_SSE42 ? "sse42" :
                    b->cpu&VSIMD_CPU_SSE4 ? "sse4" :
                    b->cpu&VSIMD_CPU_SSSE3 ? "ssse3" :
                    b->cpu&VSIMD_CPU_SSE3 ? "sse3" :
                    /* print sse2slow only if there's also a sse2fast version of the same func */
                    b->cpu&VSIMD_CPU_SSE2_IS_SLOW && j<MAX_CPUS-1 && b[1].cpu&VSIMD_CPU_SSE2_IS_FAST && !(b[1].cpu&VSIMD_CPU_SSE3) ? "sse2slow" :
                    b->cpu&VSIMD_CPU_SSE2 ? "sse2" :
                    b->cpu&VSIMD_CPU_SSE ? "sse" :
                    b->cpu&VSIMD_CPU_MMX ? "mmx" :
#elif ARCH_PPC
                    b->cpu&VSIMD_CPU_ALTIVEC ? "altivec" :
#elif ARCH_ARM
                    b->cpu&VSIMD_CPU_NEON ? "neon" :
                    b->cpu&VSIMD_CPU_ARMV6 ? "armv6" :
#elif ARCH_AARCH64
                    b->cpu&VSIMD_CPU_NEON ? "neon" :
                    b->cpu&VSIMD_CPU_ARMV8 ? "armv8" :
#elif ARCH_MIPS
                    b->cpu&VSIMD_CPU_MSA ? "msa" :
#endif
                    "c",
#if HAVE_MMX
                    b->cpu&VSIMD_CPU_CACHELINE_32 ? "_c32" :
                    b->cpu&VSIMD_CPU_SLOW_ATOM && b->cpu&VSIMD_CPU_CACHELINE_64 ? "_c64_atom" :
                    b->cpu&VSIMD_CPU_CACHELINE_64 ? "_c64" :
                    b->cpu&VSIMD_CPU_SLOW_SHUFFLE ? "_slowshuffle" :
                    b->cpu&VSIMD_CPU_LZCNT ? "_lzcnt" :
                    b->cpu&VSIMD_CPU_BMI2 ? "_bmi2" :
                    b->cpu&VSIMD_CPU_BMI1 ? "_bmi1" :
                    b->cpu&VSIMD_CPU_SLOW_CTZ ? "_slow_ctz" :
                    b->cpu&VSIMD_CPU_SLOW_ATOM ? "_atom" :
#elif ARCH_ARM
                    b->cpu&VSIMD_CPU_FAST_NEON_MRC ? "_fast_mrc" :
#endif
                    "",
                    (int64_t)(10*b->cycles/b->den - nop_time)/4 );
        }
}


int64_t mdate( void )
{
#if SYS_WINDOWS
    struct timeb tb;
    ftime( &tb );
    return ((int64_t)tb.time * 1000 + (int64_t)tb.millitm) * 1000;
#else
    struct timeval tv_date;
    gettimeofday( &tv_date, NULL );
    return (int64_t)tv_date.tv_sec * 1000000 + (int64_t)tv_date.tv_usec;
#endif
}


int run_benchmarks(int i){ return 1;}

int main(int argc, char *argv[])
{
    int ret = 0;

    if( argc > 1 && !strncmp( argv[1], "--bench", 7 ) )
    {
#if !ARCH_X86 && !ARCH_X86_64 && !ARCH_PPC && !ARCH_ARM && !ARCH_AARCH64 && !ARCH_MIPS
        fprintf( stderr, "no --bench for your cpu until you port rdtsc\n" );
        return 1;
#endif
        do_bench = 1;
        if( argv[1][7] == '=' )
        {
            bench_pattern = argv[1]+8;
            bench_pattern_len = strlen(bench_pattern);
        }
        argc--;
        argv++;
    }

    int seed = ( argc > 1 ) ? atoi(argv[1]) : mdate();
    fprintf( stderr, "x264: using random seed %u\n", seed );
    srand( seed );

    buf1 = memalign(32, 0x1e00 + 0x2000*sizeof(pixel) + 32*BENCH_ALIGNS );
    pbuf1 = memalign(32, 0x1e00*sizeof(pixel) + 32*BENCH_ALIGNS );
    if( !buf1 || !pbuf1 )
    {
        fprintf( stderr, "malloc failed, unable to initiate tests!\n" );
        return -1;
    }
#define INIT_POINTER_OFFSETS\
    buf2 = buf1 + 0xf00;\
    buf3 = buf2 + 0xf00;\
    buf4 = buf3 + 0x1000*sizeof(pixel);\
    pbuf2 = pbuf1 + 0xf00;\
    pbuf3 = (pixel*)buf3;\
    pbuf4 = (pixel*)buf4;
    INIT_POINTER_OFFSETS;
    for( int i = 0; i < 0x1e00; i++ )
    {
        buf1[i] = rand() & 0xFF;
        pbuf1[i] = rand() & PIXEL_MAX;
    }
    memset( buf1+0x1e00, 0, 0x2000*sizeof(pixel) );


    if( do_bench )
        for( int i = 0; i < BENCH_ALIGNS && !ret; i++ )
        {
            INIT_POINTER_OFFSETS;
            ret |=  run_benchmarks(i);
            buf1 += 32;
            pbuf1 += 32;
            quiet = 1;
            fprintf( stderr, "%d/%d\r", i+1, BENCH_ALIGNS );
        }
    else
        ret = run_benchmarks( 0 );

    if( ret )
    {
        fprintf( stderr, "x264: at least one test has failed. Go and fix that Right Now!\n" );
        return -1;
    }
    fprintf( stderr, "x264: All tests passed Yeah :)\n" );
    if( do_bench )
        print_bench();
    return 0;
}

