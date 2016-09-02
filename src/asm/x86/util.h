/*****************************************************************************
 * util.h: x86 inline asm
 *****************************************************************************
 * Copyright (C) 2008-2016 x264 project
 *
 * Authors: Fiona Glaser <fiona@x264.com>
 *          Loren Merritt <lorenm@u.washington.edu>
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

#ifndef X264_X86_UTIL_H
#define X264_X86_UTIL_H

#ifdef __SSE__
#include <xmmintrin.h>

#undef M128_ZERO
#define M128_ZERO ((__m128){0,0,0,0})
#define x264_union128_t x264_union128_sse_t
typedef union { __m128 i; uint64_t a[2]; uint32_t b[4]; uint16_t c[8]; uint8_t d[16]; } MAY_ALIAS x264_union128_sse_t;
#if HAVE_VECTOREXT
typedef uint32_t v4si __attribute__((vector_size (16)));
#endif
#endif // __SSE__


#endif
