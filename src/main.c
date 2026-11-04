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
#include <unistd.h>

#include "common.h"
#include "osdep.h"
#include "bench.h"



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


#define report( name ) { \
    if( used_asm && vbench_verbose ) \
        fprintf( stderr, " - %-21s [%s]\n", name, ok ? "OK" : "FAILED" ); \
    if( !ok ) ret = -1; \
}



int bench_pattern_len = 0;
const char *bench_pattern = "";
bench_func_t benchs[MAX_FUNCS];



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

    int64_t results[32] = {0};

    /* columns: index 0 is always the C version; 1..12 map to x86 cpu flags,
     * 13..15 to ARM, 16 PPC, 17 MIPS. Only the columns this CPU actually
     * supports are printed (flag bits collide across arch families, so the
     * ranges are selected per arch). */
    enum { COL_C=0, COL_ARMV6=13, COL_ARMV8=14, COL_NEON=15, COL_ALTIVEC=16, COL_MSA=17, NCOLS=18 };
    static const struct { uint32_t flag; const char *name; } coldef[NCOLS] =
    {
        { 0, "C" },
        { VSIMD_CPU_MMX,   "MMX"   }, { VSIMD_CPU_SSE,  "SSE"  },
        { VSIMD_CPU_SSE2,  "SSE2"  }, { VSIMD_CPU_SSE3, "SSE3" },
        { VSIMD_CPU_SSSE3, "SSSE3" }, { VSIMD_CPU_SSE4, "SSE4" },
        { VSIMD_CPU_SSE42, "SSE42" }, { VSIMD_CPU_AVX,  "AVX"  },
        { VSIMD_CPU_XOP,   "XOP"   }, { VSIMD_CPU_FMA4, "FMA4" },
        { VSIMD_CPU_FMA3,  "FMA3"  }, { VSIMD_CPU_AVX2, "AVX2" },
        { VSIMD_CPU_ARMV6, "ARMv6" }, { VSIMD_CPU_ARMV8, "ARMv8" },
        { VSIMD_CPU_NEON,  "NEON"  }, { VSIMD_CPU_ALTIVEC, "Altivec" },
        { VSIMD_CPU_MSA,   "MSA"   }
    };
    uint32_t cpu = cpu_detect();
    int col[NCOLS], ncol = 0;
    col[ncol++] = COL_C;
#if ARCH_X86 || ARCH_X86_64
    for( int c = 1; c <= 12; c++ )
        if( cpu & coldef[c].flag )
            col[ncol++] = c;
#elif ARCH_ARM || ARCH_AARCH64
    for( int c = COL_ARMV6; c <= COL_NEON; c++ )
        if( cpu & coldef[c].flag )
            col[ncol++] = c;
#elif ARCH_PPC
    if( cpu & VSIMD_CPU_ALTIVEC ) col[ncol++] = COL_ALTIVEC;
#elif ARCH_MIPS
    if( cpu & VSIMD_CPU_MSA ) col[ncol++] = COL_MSA;
#endif

    printf( "CPU supported:" );
    for( int j = 1; j < ncol; j++ )
        printf( " %s", coldef[col[j]].name );
    printf( "\n" );

    printf( "%30s :", "" );
    for( int j = 0; j < ncol; j++ )
        printf( "\t%s", coldef[col[j]].name );
    printf( "\n" );
    for( int i = 0; i < nfuncs; i++ ){
        printf( "%30s : \t", benchs[i].name);

     for( int j = 0; j < MAX_CPUS && (!j || benchs[i].vers[j].cpu); j++ )
        {
            int k;
            bench_t *b = &benchs[i].vers[j];
            for( k = 0; k < j && benchs[i].vers[k].pointer != b->pointer; k++ );
            if( k < j )
                continue;

#if ARCH_X86 || ARCH_X86_64
            if (b->cpu&VSIMD_CPU_AVX2) results[12] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_FMA3) results[11] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_FMA4) results[10] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_XOP) results[9] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_AVX) results[8] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_SSE42) results[7] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_SSE4) results[6] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_SSSE3) results[5] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_SSE3) results[4] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_SSE2) results[3] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_SSE) results[2] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_MMX) results[1] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else  results[0] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
#elif ARCH_ARM || ARCH_AARCH64
            if (b->cpu&VSIMD_CPU_NEON) results[COL_NEON] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_ARMV8) results[COL_ARMV8] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else if (b->cpu&VSIMD_CPU_ARMV6) results[COL_ARMV6] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else  results[COL_C] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
#elif ARCH_PPC
            if (b->cpu&VSIMD_CPU_ALTIVEC) results[COL_ALTIVEC] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else  results[COL_C] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
#elif ARCH_MIPS
            if (b->cpu&VSIMD_CPU_MSA) results[COL_MSA] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
            else  results[COL_C] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
#else
            results[COL_C] = (int64_t)(10*b->cycles/b->den - nop_time)/4;
#endif

        }


        for( int j = 0; j < ncol; j++ )
        {
            printf("%ld\t", (long)results[col[j]] );
        }
        memset(results, 0, 32*sizeof(int64_t));
        printf("\n");
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



int main(int argc, char *argv[])
{
    int ret = 0;

    if( argc > 1 && !strncmp( argv[1], "--bench", 7 ) )
    {
#if !ARCH_X86 && !ARCH_X86_64 && !ARCH_PPC && !ARCH_ARM && !ARCH_AARCH64 && !ARCH_MIPS
        fprintf( stderr, "no --bench for your cpu until you port rdtsc\n" );
        return 1;
#endif
        if( argv[1][7] == '=' )
        {
            bench_pattern = argv[1]+8;
            bench_pattern_len = strlen(bench_pattern);
        }
        argc--;
        argv++;
    }

    int seed = ( argc > 1 ) ? atoi(argv[1]) : mdate();
    fprintf( stderr, "VideoBench: using random seed %u\n", seed );
#define VB_STR2(x) #x
#define VB_STR(x) VB_STR2(x)
#if defined(__clang__)
#define VB_COMPILER "clang " __clang_version__
#elif defined(__GNUC__)
#define VB_COMPILER "gcc " VB_STR(__GNUC__) "." VB_STR(__GNUC_MINOR__) "." VB_STR(__GNUC_PATCHLEVEL__)
#else
#define VB_COMPILER "unknown"
#endif
#if ARCH_X86_64
#define VB_ARCH "x86_64"
#elif ARCH_X86
#define VB_ARCH "x86"
#elif ARCH_AARCH64
#define VB_ARCH "aarch64"
#elif ARCH_ARM
#define VB_ARCH "arm"
#elif ARCH_PPC
#define VB_ARCH "ppc"
#elif ARCH_MIPS
#define VB_ARCH "mips"
#else
#define VB_ARCH "unknown"
#endif
    fprintf( stderr, "VideoBench: compiler %s, arch %s (%d-bit), cpu \"%s\", %d core(s)\n",
             VB_COMPILER, VB_ARCH, (int)(8*sizeof(void*)), cpu_brand(), cpu_num_processors() );
    srand( seed );

    buf1 = vbench_memalign(32, 0x1e00 + 0x2000*sizeof(pixel) + 32*BENCH_ALIGNS );
    pbuf1 = vbench_memalign(32, 0x1e00*sizeof(pixel) + 32*BENCH_ALIGNS );
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


        for( int i = 0; i < BENCH_ALIGNS && !ret; i++ )
        {
            INIT_POINTER_OFFSETS;
            vbench_verbose = ( i == 0 ); /* print the [OK] check list only once */
            ret |=  run_benchmarks(i);
            buf1 += 32;
            pbuf1 += 32;
            if( isatty( 2 ) ) /* progress line only on a terminal, not in logs */
                fprintf( stderr, "%d/%d\r", i+1, BENCH_ALIGNS );
        }

    if( ret )
    {
        fprintf( stderr, "VideoBench: at least one test has failed. Go and fix that Right Now!\n" );
        //return -1;
    }else
        fprintf( stderr, "VideoBench: All tests passed Yeah :)\n" );
    print_bench();
    return 0;
}

