
#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>

#include <sys/time.h>


#include <assert.h>

typedef uint16_t pixel_10b;
typedef uint64_t pixel4_10b;
typedef int32_t  dctcoef_10b;
typedef uint32_t udctcoef_10b;

typedef uint8_t  pixel;
typedef uint32_t pixel4;
typedef int16_t  dctcoef;
typedef uint16_t udctcoef;


// GCC doesn't align stack variables on ARM, so use .bss
#if ARCH_ARM
#undef ALIGNED_16
#define ALIGNED_16( var ) DECLARE_ALIGNED( static var, 16 )
#endif


#   define PIXEL_SPLAT_X4_10b(x) ((x)*0x0001000100010001ULL)
#   define MPIXEL_X4_10b(src) M64(src)
#   define PIXEL_SPLAT_X4(x) ((x)*0x01010101U)
#   define MPIXEL_X4(src) M32(src)


#define PIXEL_MAX ((1 << 10)-1)

#define XCHG(type,a,b) do{ type t = a; a = b; b = t; } while(0)
#define MIN(a,b) ( (a)<(b) ? (a) : (b) )

#define FENC_STRIDE 16
#define FDEC_STRIDE 32




#define BENCH_RUNS 100  // tradeoff between accuracy and speed
#define BENCH_ALIGNS 16 // number of stack+heap data alignments (another accuracy vs speed tradeoff)
#define MAX_FUNCS 2048  // just has to be big enough to hold all the existing functions
#define MAX_CPUS 64     // number of different combinations of cpu flags



/****************************************************************************
 * CPU FLAGS
 ****************************************************************************/

/* x86 */
#define VSIMD_CPU_CMOV            0x0000001
#define VSIMD_CPU_MMX             0x0000002
#define VSIMD_CPU_MMX2            0x0000004  /* MMX2 aka MMXEXT aka ISSE */
#define VSIMD_CPU_MMXEXT          VSIMD_CPU_MMX2
#define VSIMD_CPU_SSE             0x0000008
#define VSIMD_CPU_SSE2            0x0000010
#define VSIMD_CPU_SSE3            0x0000020
#define VSIMD_CPU_SSSE3           0x0000040
#define VSIMD_CPU_SSE4            0x0000080  /* SSE4.1 */
#define VSIMD_CPU_SSE42           0x0000100  /* SSE4.2 */
#define VSIMD_CPU_LZCNT           0x0000200  /* Phenom support for "leading zero count" instruction. */
#define VSIMD_CPU_AVX             0x0000400  /* AVX support: requires OS support even if YMM registers aren't used. */
#define VSIMD_CPU_XOP             0x0000800  /* AMD XOP */
#define VSIMD_CPU_FMA4            0x0001000  /* AMD FMA4 */
#define VSIMD_CPU_FMA3            0x0002000  /* FMA3 */
#define VSIMD_CPU_AVX2            0x0004000  /* AVX2 */
#define VSIMD_CPU_BMI1            0x0008000  /* BMI1 */
#define VSIMD_CPU_BMI2            0x0010000  /* BMI2 */
/* x86 modifiers */
#define VSIMD_CPU_CACHELINE_32    0x0020000  /* avoid memory loads that span the border between two cachelines */
#define VSIMD_CPU_CACHELINE_64    0x0040000  /* 32/64 is the size of a cacheline in bytes */
#define VSIMD_CPU_SSE2_IS_SLOW    0x0080000  /* avoid most SSE2 functions on Athlon64 */
#define VSIMD_CPU_SSE2_IS_FAST    0x0100000  /* a few functions are only faster on Core2 and Phenom */
#define VSIMD_CPU_SLOW_SHUFFLE    0x0200000  /* The Conroe has a slow shuffle unit (relative to overall SSE performance) */
#define VSIMD_CPU_STACK_MOD4      0x0400000  /* if stack is only mod4 and not mod16 */
#define VSIMD_CPU_SLOW_CTZ        0x0800000  /* BSR/BSF x86 instructions are really slow on some CPUs */
#define VSIMD_CPU_SLOW_ATOM       0x1000000  /* The Atom is terrible: slow SSE unaligned loads, slow
                                             * SIMD multiplies, slow SIMD variable shifts, slow pshufb,
                                             * cacheline split penalties -- gather everything here that
                                             * isn't shared by other CPUs to avoid making half a dozen
                                             * new SLOW flags. */
#define VSIMD_CPU_SLOW_PSHUFB     0x2000000  /* such as on the Intel Atom */
#define VSIMD_CPU_SLOW_PALIGNR    0x4000000  /* such as on the AMD Bobcat */

/* PowerPC */
#define VSIMD_CPU_ALTIVEC         0x0000001

/* ARM and AArch64 */
#define VSIMD_CPU_ARMV6           0x0000001
#define VSIMD_CPU_NEON            0x0000002  /* ARM NEON */
#define VSIMD_CPU_FAST_NEON_MRC   0x0000004  /* Transfer from NEON to ARM register is fast (Cortex-A9) */
#define VSIMD_CPU_ARMV8           0x0000008

/* MIPS */
#define VSIMD_CPU_MSA             0x0000001  /* MIPS MSA */




/** CPU DETECT */

typedef struct
{
    const char name[16];
    uint32_t flags;
} cpu_name_t;

extern const cpu_name_t cpu_names[];

/* CPU flags */

/* x86 */
#define CPU_CMOV            0x0000001
#define CPU_MMX             0x0000002
#define CPU_MMX2            0x0000004  /* MMX2 aka MMXEXT aka ISSE */
#define CPU_MMXEXT          CPU_MMX2
#define CPU_SSE             0x0000008
#define CPU_SSE2            0x0000010
#define CPU_SSE3            0x0000020
#define CPU_SSSE3           0x0000040
#define CPU_SSE4            0x0000080  /* SSE4.1 */
#define CPU_SSE42           0x0000100  /* SSE4.2 */
#define CPU_LZCNT           0x0000200  /* Phenom support for "leading zero count" instruction. */
#define CPU_AVX             0x0000400  /* AVX support: requires OS support even if YMM registers aren't used. */
#define CPU_XOP             0x0000800  /* AMD XOP */
#define CPU_FMA4            0x0001000  /* AMD FMA4 */
#define CPU_FMA3            0x0002000  /* FMA3 */
#define CPU_AVX2            0x0004000  /* AVX2 */
#define CPU_BMI1            0x0008000  /* BMI1 */
#define CPU_BMI2            0x0010000  /* BMI2 */
/* x86 modifiers */
#define CPU_CACHELINE_32    0x0020000  /* avoid memory loads that span the border between two cachelines */
#define CPU_CACHELINE_64    0x0040000  /* 32/64 is the size of a cacheline in bytes */
#define CPU_SSE2_IS_SLOW    0x0080000  /* avoid most SSE2 functions on Athlon64 */
#define CPU_SSE2_IS_FAST    0x0100000  /* a few functions are only faster on Core2 and Phenom */
#define CPU_SLOW_SHUFFLE    0x0200000  /* The Conroe has a slow shuffle unit (relative to overall SSE performance) */
#define CPU_STACK_MOD4      0x0400000  /* if stack is only mod4 and not mod16 */
#define CPU_SLOW_CTZ        0x0800000  /* BSR/BSF x86 instructions are really slow on some CPUs */
#define CPU_SLOW_ATOM       0x1000000  /* The Atom is terrible: slow SSE unaligned loads, slow
                                             * SIMD multiplies, slow SIMD variable shifts, slow pshufb,
                                             * cacheline split penalties -- gather everything here that
                                             * isn't shared by other CPUs to avoid making half a dozen
                                             * new SLOW flags. */

#define CPU_SLOW_PSHUFB     0x2000000  /* such as on the Intel Atom */
#define CPU_SLOW_PALIGNR    0x4000000  /* such as on the AMD Bobcat */

/* PowerPC */
#define CPU_ALTIVEC         0x0000001

/* ARM and AArch64 */
#define CPU_ARMV6           0x0000001
#define CPU_NEON            0x0000002  /* ARM NEON */
#define CPU_FAST_NEON_MRC   0x0000004  /* Transfer from NEON to ARM register is fast (Cortex-A9) */
#define CPU_ARMV8           0x0000008

/* MIPS */
#define CPU_MSA             0x0000001  /* MIPS MSA */



uint32_t cpu_detect( void );


#if ARCH_X86 || ARCH_X86_64
int stack_pagealign( int (*func)(), int align );

/* detect when callee-saved regs aren't saved
 * needs an explicit asm check because it only sometimes crashes in normal use. */
intptr_t benchmark_call( intptr_t (*func)(), int *ok, ... );
#else
#define stack_pagealign( func, align ) func()
#endif




/* Calls for the benchmarks */
int check_pixel( int cpu_ref, int cpu_new );



#endif // COMMON_H


