/*****************************************************************************
 * osdep.h: platform-specific code
 *****************************************************************************
 * Copyright (C) 2007-2016 x264 project
 *
 * Authors: Loren Merritt <lorenm@u.washington.edu>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Henrik Gramner <henrik@gramner.com>
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
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/


#ifndef OSDEP_H
#define OSDEP_H

#ifdef __INTEL_COMPILER
#include <mathimf.h>
#else
#include <math.h>
#endif

#include <stdint.h>
#include <limits.h>



#if !defined(va_copy) && defined(__INTEL_COMPILER)
#define va_copy(dst, src) ((dst) = (src))
#endif

#if !defined(isfinite) && (SYS_OPENBSD || SYS_SunOS)
#define isfinite finite
#endif


#define ALIGN(x,a) (((x)+((a)-1))&~((a)-1))

#define DECLARE_ALIGNED( var, n ) var __attribute__((aligned(n)))
#define ALIGNED_32( var ) DECLARE_ALIGNED( var, 32 )
#define ALIGNED_16( var ) DECLARE_ALIGNED( var, 16 )
#define ALIGNED_8( var )  DECLARE_ALIGNED( var, 8 )
#define ALIGNED_4( var )  DECLARE_ALIGNED( var, 4 )

// ARM compiliers don't reliably align stack variables
// - EABI requires only 8 byte stack alignment to be maintained
// - gcc can't align stack variables to more even if the stack were to be correctly aligned outside the function
// - armcc can't either, but is nice enough to actually tell you so
// - Apple gcc only maintains 4 byte alignment
// - llvm can align the stack, but only in svn and (unrelated) it exposes bugs in all released GNU binutils...

#define ALIGNED_ARRAY_EMU( mask, type, name, sub1, ... )\
    uint8_t name##_u [sizeof(type sub1 __VA_ARGS__) + mask]; \
    type (*name) __VA_ARGS__ = (void*)((intptr_t)(name##_u+mask) & ~mask)

#if ARCH_ARM && SYS_MACOSX
#define ALIGNED_ARRAY_8( ... ) ALIGNED_ARRAY_EMU( 7, __VA_ARGS__ )
#else
#define ALIGNED_ARRAY_8( type, name, sub1, ... )\
    ALIGNED_8( type name sub1 __VA_ARGS__ )
#endif

#if ARCH_ARM
#define ALIGNED_ARRAY_16( ... ) ALIGNED_ARRAY_EMU( 15, __VA_ARGS__ )
#else
#define ALIGNED_ARRAY_16( type, name, sub1, ... )\
    ALIGNED_16( type name sub1 __VA_ARGS__ )
#endif

#define EXPAND(x) x

#if STACK_ALIGNMENT >= 32
#define ALIGNED_ARRAY_32( type, name, sub1, ... )\
    ALIGNED_32( type name sub1 __VA_ARGS__ )
#else
#define ALIGNED_ARRAY_32( ... ) EXPAND( ALIGNED_ARRAY_EMU( 31, __VA_ARGS__ ) )
#endif

#define ALIGNED_ARRAY_64( ... ) EXPAND( ALIGNED_ARRAY_EMU( 63, __VA_ARGS__ ) )

/* For AVX2 */
#if ARCH_X86 || ARCH_X86_64
#define NATIVE_ALIGN 32
#define ALIGNED_N ALIGNED_32
#define ALIGNED_ARRAY_N ALIGNED_ARRAY_32
#else
#define NATIVE_ALIGN 16
#define ALIGNED_N ALIGNED_16
#define ALIGNED_ARRAY_N ALIGNED_ARRAY_16
#endif

#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
#define UNUSED __attribute__((unused))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define NOINLINE __attribute__((noinline))
#define MAY_ALIAS __attribute__((may_alias))
#define x264_constant_p(x) __builtin_constant_p(x)
#define x264_nonconstant_p(x) (!__builtin_constant_p(x))
#else
#define UNUSED
#define MAY_ALIAS
#define x264_constant_p(x) 0
#define x264_nonconstant_p(x) 0
#endif



// GCC doesn't align stack variables on ARM, so use .bss
#if ARCH_ARM
    #undef ALIGNED_16
    #define ALIGNED_16( var ) DECLARE_ALIGNED( static var, 16 )
#endif



#define WORD_SIZE sizeof(void*)


/* For values with 4 bits or less. */
static int ALWAYS_INLINE vbench_ctz_4bit( uint32_t x )
{
    static uint8_t lut[16] = {4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0};
    return lut[x];
}

#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 3)
#define x264_clz(x) __builtin_clz(x)
#define x264_ctz(x) __builtin_ctz(x)
#else
static int ALWAYS_INLINE vbench_clz( uint32_t x )
{
    static uint8_t lut[16] = {4,3,2,2,1,1,1,1,0,0,0,0,0,0,0,0};
    int y, z = (((x >> 16) - 1) >> 27) & 16;
    x >>= z^16;
    z += y = ((x - 0x100) >> 28) & 8;
    x >>= y^8;
    z += y = ((x - 0x10) >> 29) & 4;
    x >>= y^4;
    return z + lut[x];
}

static int ALWAYS_INLINE vbench_ctz( uint32_t x )
{
    static uint8_t lut[16] = {4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0};
    int y, z = (((x & 0xffff) - 1) >> 27) & 16;
    x >>= z;
    z += y = (((x & 0xff) - 1) >> 28) & 8;
    x >>= y;
    z += y = (((x & 0xf) - 1) >> 29) & 4;
    x >>= y;
    return z + lut[x&0xf];
}
#endif



#define asm __asm__


#if HAVE_MMX
/* There is no way to forbid the compiler from using float instructions
 *  * before the emms so miscompilation could theoretically occur in the
 *   * unlikely event that the compiler reorders emms and float instructions. */
#if HAVE_X86_INLINE_ASM
/* Clobbering memory makes the compiler less likely to reorder code. */
#define vbench_emms() asm volatile( "emms":::"memory","st","st(1)","st(2)", \
                                          "st(3)","st(4)","st(5)","st(6)","st(7)" )
#else
#define vbench_emms() asm_cpu_emms()
#endif
#else
#define vbench_emms()
#endif



#endif // OSDEP


