/*****************************************************************************
 * cpu.h: minimal shim — only the PFX symbol-prefixing macros and the
 * X265_CPU_* flag values (upstream x265.h) are needed by primitives.h /
 * contexts.h / the arch asm-init files in this port. No CPU detection is
 * performed here; the bench maps the harness cpu flags to these values.
 * Part of the Video-SIMDBench port of x265 (AGPL, see COPYING).
 *****************************************************************************/

#ifndef X265_CPU_SHIM_H
#define X265_CPU_SHIM_H

#include "x265shim.h"

#define PFX3(prefix, name) prefix ## _ ## name
#define PFX2(prefix, name) PFX3(prefix, name)
#define PFX(name)          PFX2(X265_NS, name)

/* CPU flags (values from upstream x265.h) */
#if X265_ARCH_ARM64
#define X265_CPU_ARMV6           (1 << 0)
#define X265_CPU_NEON            (1 << 1)
#define X265_CPU_FAST_NEON_MRC   (1 << 2)
#define X265_CPU_SVE2            (1 << 3)
#define X265_CPU_SVE             (1 << 4)
#define X265_CPU_NEON_DOTPROD    (1 << 5)
#define X265_CPU_NEON_I8MM       (1 << 6)
#define X265_CPU_SVE2_BITPERM    (1 << 7)
#else
#define X265_CPU_MMX             (1 << 0)
#define X265_CPU_MMX2            (1 << 1)
#define X265_CPU_MMXEXT          X265_CPU_MMX2
#define X265_CPU_SSE             (1 << 2)
#define X265_CPU_SSE2            (1 << 3)
#define X265_CPU_LZCNT           (1 << 4)
#define X265_CPU_SSE3            (1 << 5)
#define X265_CPU_SSSE3           (1 << 6)
#define X265_CPU_SSE4            (1 << 7)
#define X265_CPU_SSE42           (1 << 8)
#define X265_CPU_AVX             (1 << 9)
#define X265_CPU_XOP             (1 << 10)
#define X265_CPU_FMA4            (1 << 11)
#define X265_CPU_FMA3            (1 << 12)
#define X265_CPU_BMI1            (1 << 13)
#define X265_CPU_BMI2            (1 << 14)
#define X265_CPU_AVX2            (1 << 15)
#define X265_CPU_AVX512          (1 << 16)
#define X265_CPU_CACHELINE_32    (1 << 17)
#define X265_CPU_CACHELINE_64    (1 << 18)
#define X265_CPU_SSE2_IS_SLOW    (1 << 19)
#define X265_CPU_SSE2_IS_FAST    (1 << 20)
#define X265_CPU_SLOW_SHUFFLE    (1 << 21)
#define X265_CPU_STACK_MOD4      (1 << 22)
#define X265_CPU_SLOW_ATOM       (1 << 23)
#define X265_CPU_SLOW_PSHUFB     (1 << 24)
#define X265_CPU_SLOW_PALIGNR    (1 << 25)
#endif

#endif // X265_CPU_SHIM_H
