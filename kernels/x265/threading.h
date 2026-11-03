/*****************************************************************************
 * threading.h: minimal shim — the copied kernels only need the bit-scan
 * macros (BSR/BSF). All lock/event/thread types are stubbed out.
 * Part of the Video-SIMDBench port of x265 (AGPL, see COPYING).
 *****************************************************************************/

#ifndef X265_THREADING_SHIM_H
#define X265_THREADING_SHIM_H

#include "x265shim.h"

#define BSR(id, x)            (id) = ((unsigned long)__builtin_clz(x) ^ 31)
#define BSF(id, x)            (id) = ((unsigned long)__builtin_ctz(x))
#define BSR64(id, x)          (id) = ((unsigned long)__builtin_clzll(x) ^ 63)
#define BSF64(id, x)          (id) = ((unsigned long)__builtin_ctzll(x))

#endif // X265_THREADING_SHIM_H
