/*****************************************************************************
 * cpu.h: minimal shim — only the PFX symbol-prefixing macros are needed by
 * primitives.h / contexts.h in this port. No CPU detection is performed.
 * Part of the Video-SIMDBench port of x265 (AGPL, see COPYING).
 *****************************************************************************/

#ifndef X265_CPU_SHIM_H
#define X265_CPU_SHIM_H

#include "x265shim.h"

#define PFX3(prefix, name) prefix ## _ ## name
#define PFX2(prefix, name) PFX3(prefix, name)
#define PFX(name)          PFX2(X265_NS, name)

/* CPU flags: unused in this port (C kernels only), defined for completeness */
#define X265_CPU_MMX             0x0001
#define X265_CPU_SSE2            0x0002
#define X265_CPU_SSE3            0x0004
#define X265_CPU_SSSE3           0x0008
#define X265_CPU_SSE4            0x0010
#define X265_CPU_AVX             0x0020
#define X265_CPU_AVX2            0x0040
#define X265_CPU_NEON            0x0080

#endif // X265_CPU_SHIM_H
