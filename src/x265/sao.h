/*****************************************************************************
 * sao.h: minimal local shim replacing upstream source/encoder/sao.h, which
 * the copied aarch64/sao-prim.cpp includes. Only the SAO class constants
 * used by the kernel port are provided.
 * Part of the Video-SIMDBench port of x265 (AGPL, see COPYING).
 *****************************************************************************/

#ifndef X265_SAO_SHIM_H
#define X265_SAO_SHIM_H

#include "x265shim.h"

namespace X265_NS {

class SAO
{
public:
    enum { SAO_BO_BITS = 5 };
};

} // namespace X265_NS

#endif // X265_SAO_SHIM_H
