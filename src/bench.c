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

/* buf1, buf2: initialised to random data and shouldn't write into them */
uint8_t *buf1, *buf2;
/* buf3, buf4: used to store output */
uint8_t *buf3, *buf4;
/* pbuf1, pbuf2: initialised to random pixel data and shouldn't write into them. */
pixel *pbuf1, *pbuf2;
/* pbuf3, pbuf4: point to buf3, buf4, just for type convenience */
pixel *pbuf3, *pbuf4;


#define report( name ) { \
    if( used_asm ) \
        fprintf( stderr, " - %-21s [%s]\n", name, ok ? "OK" : "FAILED" ); \
    if( !ok ) ret = -1; \
}

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

extern int do_bench;
extern int bench_pattern_len;
extern const char *bench_pattern;
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

#define call_c1(func,...) func(__VA_ARGS__)

#if ARCH_X86_64
/* Evil hack: detect incorrect assumptions that 32-bit ints are zero-extended to 64-bit.
 * This is done by clobbering the stack with junk around the stack pointer and calling the
 * assembly function through x264_checkasm_call with added dummy arguments which forces all
 * real arguments to be passed on the stack and not in registers. For 32-bit argument the
 * upper half of the 64-bit register location on the stack will now contain junk. Note that
 * this is dependant on compiler behaviour and that interrupts etc. at the wrong time may
 * overwrite the junk written to the stack so there's no guarantee that it will always
 * detect all functions that assumes zero-extension.
 */
void x264_checkasm_stack_clobber( uint64_t clobber, ... );
#define call_a1(func,...) ({ \
    uint64_t r = (rand() & 0xffff) * 0x0001000100010001ULL; \
    x264_checkasm_stack_clobber( r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r ); /* max_args+6 */ \
    x264_checkasm_call(( intptr_t(*)())func, &ok, 0, 0, 0, 0, __VA_ARGS__ ); })
#elif ARCH_X86 || (ARCH_AARCH64 && !defined(__APPLE__)) || ARCH_ARM
#define call_a1(func,...) x264_checkasm_call( (intptr_t(*)())func, &ok, __VA_ARGS__ )
#else
#define call_a1 call_c1
#endif

#if ARCH_ARM
#define call_a1_64(func,...) ((uint64_t (*)(intptr_t(*)(), int*, ...))x264_checkasm_call)( (intptr_t(*)())func, &ok, __VA_ARGS__ )
#else
#define call_a1_64 call_a1
#endif

#define call_bench(func,cpu,...)\
    if( do_bench && !strncmp(func_name, bench_pattern, bench_pattern_len) )\
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
            func(__VA_ARGS__);\
            t = read_time() - t;\
            if( (uint64_t)t*tcount <= tsum*4 && ti > 0 )\
            {\
                tsum += t;\
                tcount++;\
            }\
        }\
        bench_t *b = get_bench( func_name, cpu );\
        b->cycles += tsum;\
        b->den += tcount;\
        b->pointer = func;\
    }

/* for most functions, run benchmark and correctness test at the same time.
 * for those that modify their inputs, run the above macros separately */
#define call_a(func,...) ({ call_a2(func,__VA_ARGS__); call_a1(func,__VA_ARGS__); })
#define call_c(func,...) ({ call_c2(func,__VA_ARGS__); call_c1(func,__VA_ARGS__); })
#define call_a2(func,...) ({ call_bench(func,cpu_new,__VA_ARGS__); })
#define call_c2(func,...) ({ call_bench(func,0,__VA_ARGS__); })
#define call_a64(func,...) ({ call_a2(func,__VA_ARGS__); call_a1_64(func,__VA_ARGS__); })


static int check_all_funcs( int cpu_ref, int cpu_new )
{
    return check_pixel( cpu_ref, cpu_new );
#if 0
         + check_dct( cpu_ref, cpu_new )
         + check_mc( cpu_ref, cpu_new )
         + check_intra( cpu_ref, cpu_new )
         + check_deblock( cpu_ref, cpu_new )
         + check_quant( cpu_ref, cpu_new )
         + check_cabac( cpu_ref, cpu_new )
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
    fprintf( stderr, "VideoBench: %s\n", name );
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
    check_all_flags();
}

