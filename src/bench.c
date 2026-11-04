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
 * Notes:   The _10b subfix is for the high bit depth. 
 *
 * *************************************************************************** 
 *
 *  Version - Changes
 *
 *  0.1     - Leave only code for calling the kernels. 
 *
 *
 *****************************************************************************/


#include <ctype.h>
#include <string.h>
#include "common.h"
#include "osdep.h"
#include "bench.h"


#define report( name ) { \
    if( used_asm && vbench_verbose ) \
        fprintf( stderr, " - %-21s [%s]\n", name, ok ? "OK" : "FAILED" ); \
    if( !ok ) ret = -1; \
}

/* Shared harness globals (declared extern in bench.h; buf1..buf4,
 * pbuf1..pbuf4, bench_pattern_len, bench_pattern, benchs are defined in main.c) */
char func_name[100];
/* print per-check [OK] lines only on the first alignment pass */
int vbench_verbose = 1;

bench_t *get_bench( const char *name, int cpu )
{
    int i, j;
    for( i = 0; benchs[i].name && strcmp(name, benchs[i].name); i++ )
        assert( i < MAX_FUNCS );
    if( !benchs[i].name )
        benchs[i].name = strdup( name );
    if( !cpu )
        return &benchs[i].vers[0];
    for( j = 1; benchs[i].vers[j].cpu && benchs[i].vers[j].cpu != (uint32_t)cpu; j++ )
        assert( j < MAX_CPUS );
    benchs[i].vers[j].cpu = cpu;
    return &benchs[i].vers[j];
}

#define HAVE_X86_INLINE_ASM 1


#if ARCH_X86 || ARCH_X86_64
int x264_stack_pagealign( int (*func)(), int align );

/* detect when callee-saved regs aren't saved
 * needs an explicit asm check because it only sometimes crashes in normal use. */
intptr_t x264_checkasm_call( intptr_t (*func)(), int *ok, ... );
#else
#define x264_stack_pagealign( func, align ) func()
#endif

#if ARCH_AARCH64
intptr_t x264_checkasm_call( intptr_t (*func)(), int *ok, ... );
#endif

#if ARCH_ARM
intptr_t x264_checkasm_call_neon( intptr_t (*func)(), int *ok, ... );
intptr_t x264_checkasm_call_noneon( intptr_t (*func)(), int *ok, ... );
intptr_t (*x264_checkasm_call)( intptr_t (*func)(), int *ok, ... ) = x264_checkasm_call_noneon;
#endif



static int check_all_funcs( int cpu_ref, int cpu_new )
{
    return check_pixel( cpu_ref, cpu_new )
         + check_dct( cpu_ref, cpu_new )
         + check_mc( cpu_ref, cpu_new )
         + check_intra( cpu_ref, cpu_new )
         + check_deblock( cpu_ref, cpu_new )
         + check_quant( cpu_ref, cpu_new )
         + check_cabac( cpu_ref, cpu_new )
         + check_codec_kernels( cpu_ref, cpu_new );
#if 0
         + check_bitstream( cpu_ref, cpu_new );
#endif
}

static int add_flags( int *cpu_ref, int *cpu_new, int flags, const char *name )
{
    *cpu_ref = *cpu_new;
    *cpu_new |= flags;
#if STACK_ALIGNMENT < 16
    *cpu_new |= VSIMD_CPU_STACK_MOD4;
#endif
    if( *cpu_new & VSIMD_CPU_SSE2_IS_FAST )
        *cpu_new &= ~VSIMD_CPU_SSE2_IS_SLOW;
    if( vbench_verbose ) fprintf( stderr, "VideoBench: %s\n", name );
    return check_all_funcs( *cpu_ref, *cpu_new );
}

static int check_all_flags( void )
{
    int ret = 0;
    int cpu0 = 0, cpu1 = 0;
    uint32_t cpu_detect_rs = cpu_detect();
#if HAVE_MMX
    if( cpu_detect_rs & VSIMD_CPU_MMX2 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_MMX | VSIMD_CPU_MMX2, "MMX" );
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_CACHELINE_64, "MMX Cache64" );
        cpu1 &= ~VSIMD_CPU_CACHELINE_64;
#if ARCH_X86
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_CACHELINE_32, "MMX Cache32" );
        cpu1 &= ~VSIMD_CPU_CACHELINE_32;
#endif
        if( cpu_detect_rs & VSIMD_CPU_LZCNT )
        {
            ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_LZCNT, "MMX LZCNT" );
            cpu1 &= ~VSIMD_CPU_LZCNT;
        }
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SLOW_CTZ, "MMX SlowCTZ" );
        cpu1 &= ~VSIMD_CPU_SLOW_CTZ;
    }
    if( cpu_detect_rs & VSIMD_CPU_SSE )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SSE, "SSE" );
    if( cpu_detect_rs & VSIMD_CPU_SSE2 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SSE2 | VSIMD_CPU_SSE2_IS_SLOW, "SSE2Slow" );
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SSE2_IS_FAST, "SSE2Fast" );
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_CACHELINE_64, "SSE2Fast Cache64" );
        cpu1 &= ~VSIMD_CPU_CACHELINE_64;
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SLOW_SHUFFLE, "SSE2 SlowShuffle" );
        cpu1 &= ~VSIMD_CPU_SLOW_SHUFFLE;
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SLOW_CTZ, "SSE2 SlowCTZ" );
        cpu1 &= ~VSIMD_CPU_SLOW_CTZ;
        if( cpu_detect_rs & VSIMD_CPU_LZCNT )
        {
            ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_LZCNT, "SSE2 LZCNT" );
            cpu1 &= ~VSIMD_CPU_LZCNT;
        }
    }
    if( cpu_detect_rs & VSIMD_CPU_SSE3 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SSE3 | VSIMD_CPU_CACHELINE_64, "SSE3" );
        cpu1 &= ~VSIMD_CPU_CACHELINE_64;
    }
    if( cpu_detect_rs & VSIMD_CPU_SSSE3 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SSSE3, "SSSE3" );
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_CACHELINE_64, "SSSE3 Cache64" );
        cpu1 &= ~VSIMD_CPU_CACHELINE_64;
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SLOW_SHUFFLE, "SSSE3 SlowShuffle" );
        cpu1 &= ~VSIMD_CPU_SLOW_SHUFFLE;
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SLOW_CTZ, "SSSE3 SlowCTZ" );
        cpu1 &= ~VSIMD_CPU_SLOW_CTZ;
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SLOW_ATOM, "SSSE3 SlowAtom" );
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_CACHELINE_64, "SSSE3 Cache64 SlowAtom" );
        cpu1 &= ~VSIMD_CPU_CACHELINE_64;
        cpu1 &= ~VSIMD_CPU_SLOW_ATOM;
        if( cpu_detect_rs & VSIMD_CPU_LZCNT )
        {
            ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_LZCNT, "SSSE3 LZCNT" );
            cpu1 &= ~VSIMD_CPU_LZCNT;
        }
    }
    if( cpu_detect_rs & VSIMD_CPU_SSE4 )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SSE4, "SSE4" );
    if( cpu_detect_rs & VSIMD_CPU_SSE42 )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_SSE42, "SSE4.2" );
    if( cpu_detect_rs & VSIMD_CPU_AVX )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_AVX, "AVX" );
    if( cpu_detect_rs & VSIMD_CPU_XOP )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_XOP, "XOP" );
    if( cpu_detect_rs & VSIMD_CPU_FMA4 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_FMA4, "FMA4" );
        cpu1 &= ~VSIMD_CPU_FMA4;
    }
    if( cpu_detect_rs & VSIMD_CPU_FMA3 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_FMA3, "FMA3" );
        cpu1 &= ~VSIMD_CPU_FMA3;
    }
    if( cpu_detect_rs & VSIMD_CPU_AVX2 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_FMA3 | VSIMD_CPU_AVX2, "AVX2" );
        if( cpu_detect_rs & VSIMD_CPU_LZCNT )
        {
            ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_LZCNT, "AVX2 LZCNT" );
            cpu1 &= ~VSIMD_CPU_LZCNT;
        }
    }
    if( cpu_detect_rs & VSIMD_CPU_BMI1 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_BMI1, "BMI1" );
        cpu1 &= ~VSIMD_CPU_BMI1;
    }
    if( cpu_detect_rs & VSIMD_CPU_BMI2 )
    {
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_BMI1|VSIMD_CPU_BMI2, "BMI2" );
        cpu1 &= ~(VSIMD_CPU_BMI1|VSIMD_CPU_BMI2);
    }
#elif ARCH_PPC
    if( cpu_detect_rs & VSIMD_CPU_ALTIVEC )
    {
        fprintf( stderr, "x264: ALTIVEC against C\n" );
        ret = check_all_funcs( 0, VSIMD_CPU_ALTIVEC );
    }
#elif ARCH_ARM
    if( cpu_detect_rs & VSIMD_CPU_NEON )
        x264_checkasm_call = x264_checkasm_call_neon;
    if( cpu_detect_rs & VSIMD_CPU_ARMV6 )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_ARMV6, "ARMv6" );
    if( cpu_detect_rs & VSIMD_CPU_NEON )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_NEON, "NEON" );
    if( cpu_detect_rs & VSIMD_CPU_FAST_NEON_MRC )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_FAST_NEON_MRC, "Fast NEON MRC" );
#elif ARCH_AARCH64
    if( cpu_detect_rs & VSIMD_CPU_ARMV8 )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_ARMV8, "ARMv8" );
    if( cpu_detect_rs & VSIMD_CPU_NEON )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_NEON, "NEON" );
#elif ARCH_MIPS
    if( cpu_detect_rs & VSIMD_CPU_MSA )
        ret |= add_flags( &cpu0, &cpu1, VSIMD_CPU_MSA, "MSA" );
#endif
    return ret;
}

int run_benchmarks(int i){
    /* 32-byte alignment is guaranteed whenever it's useful,
     * but some functions also vary in speed depending on %64 */
    //return x264_stack_pagealign(check_all_flags, i*32 );
    (void)i;
    check_all_flags();
    return 0;
}

