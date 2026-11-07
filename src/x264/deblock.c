/*****************************************************************************
 * deblock.c: deblocking
 *****************************************************************************
 * Copyright (C) 2003-2025 x264 project
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Loren Merritt <lorenm@u.washington.edu>
 *          Fiona Glaser <fiona@x264.com>
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

#include "vb_shim.h"

/* Deblocking filter */
static const uint8_t i_alpha_table[52+12*3] =
{
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  4,  4,  5,  6,
     7,  8,  9, 10, 12, 13, 15, 17, 20, 22,
    25, 28, 32, 36, 40, 45, 50, 56, 63, 71,
    80, 90,101,113,127,144,162,182,203,226,
   255,255,
   255,255,255,255,255,255,255,255,255,255,255,255,
};
static const uint8_t i_beta_table[52+12*3] =
{
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  2,  2,  2,  3,
     3,  3,  3,  4,  4,  4,  6,  6,  7,  7,
     8,  8,  9,  9, 10, 10, 11, 11, 12, 12,
    13, 13, 14, 14, 15, 15, 16, 16, 17, 17,
    18, 18,
    18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
};
static const int8_t i_tc0_table[52+12*3][4] =
{
    {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 },
    {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 },
    {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 },
    {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 },
    {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 },
    {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 },
    {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 0 }, {-1, 0, 0, 1 },
    {-1, 0, 0, 1 }, {-1, 0, 0, 1 }, {-1, 0, 0, 1 }, {-1, 0, 1, 1 }, {-1, 0, 1, 1 }, {-1, 1, 1, 1 },
    {-1, 1, 1, 1 }, {-1, 1, 1, 1 }, {-1, 1, 1, 1 }, {-1, 1, 1, 2 }, {-1, 1, 1, 2 }, {-1, 1, 1, 2 },
    {-1, 1, 1, 2 }, {-1, 1, 2, 3 }, {-1, 1, 2, 3 }, {-1, 2, 2, 3 }, {-1, 2, 2, 4 }, {-1, 2, 3, 4 },
    {-1, 2, 3, 4 }, {-1, 3, 3, 5 }, {-1, 3, 4, 6 }, {-1, 3, 4, 6 }, {-1, 4, 5, 7 }, {-1, 4, 5, 8 },
    {-1, 4, 6, 9 }, {-1, 5, 7,10 }, {-1, 6, 8,11 }, {-1, 6, 8,13 }, {-1, 7,10,14 }, {-1, 8,11,16 },
    {-1, 9,12,18 }, {-1,10,13,20 }, {-1,11,15,23 }, {-1,13,17,25 },
    {-1,13,17,25 }, {-1,13,17,25 }, {-1,13,17,25 }, {-1,13,17,25 }, {-1,13,17,25 }, {-1,13,17,25 },
    {-1,13,17,25 }, {-1,13,17,25 }, {-1,13,17,25 }, {-1,13,17,25 }, {-1,13,17,25 }, {-1,13,17,25 },
};
#define alpha_table(x) i_alpha_table[(x)+24]
#define beta_table(x)  i_beta_table[(x)+24]
#define tc0_table(x)   i_tc0_table[(x)+24]

/* From ffmpeg */
static ALWAYS_INLINE void deblock_edge_luma_c( pixel *pix, intptr_t xstride, int alpha, int beta, int8_t tc0 )
{
    int p2 = pix[-3*xstride];
    int p1 = pix[-2*xstride];
    int p0 = pix[-1*xstride];
    int q0 = pix[ 0*xstride];
    int q1 = pix[ 1*xstride];
    int q2 = pix[ 2*xstride];

    if( abs( p0 - q0 ) < alpha && abs( p1 - p0 ) < beta && abs( q1 - q0 ) < beta )
    {
        int tc = tc0;
        int delta;
        if( abs( p2 - p0 ) < beta )
        {
            if( tc0 )
                pix[-2*xstride] = p1 + x264_clip3( (( p2 + ((p0 + q0 + 1) >> 1)) >> 1) - p1, -tc0, tc0 );
            tc++;
        }
        if( abs( q2 - q0 ) < beta )
        {
            if( tc0 )
                pix[ 1*xstride] = q1 + x264_clip3( (( q2 + ((p0 + q0 + 1) >> 1)) >> 1) - q1, -tc0, tc0 );
            tc++;
        }

        delta = x264_clip3( (((q0 - p0 ) * 4) + (p1 - q1) + 4) >> 3, -tc, tc );
        pix[-1*xstride] = x264_clip_pixel( p0 + delta );    /* p0' */
        pix[ 0*xstride] = x264_clip_pixel( q0 - delta );    /* q0' */
    }
}
static inline void deblock_luma_c( pixel *pix, intptr_t xstride, intptr_t ystride, int alpha, int beta, int8_t *tc0 )
{
    for( int i = 0; i < 4; i++ )
    {
        if( tc0[i] < 0 )
        {
            pix += 4*ystride;
            continue;
        }
        for( int d = 0; d < 4; d++, pix += ystride )
            deblock_edge_luma_c( pix, xstride, alpha, beta, tc0[i] );
    }
}
static void deblock_h_luma_mbaff_c( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 )
{
    for( int d = 0; d < 8; d++, pix += stride )
        deblock_edge_luma_c( pix, 1, alpha, beta, tc0[d>>1] );
}
static void deblock_v_luma_c( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 )
{
    deblock_luma_c( pix, stride, 1, alpha, beta, tc0 );
}
static void deblock_h_luma_c( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 )
{
    deblock_luma_c( pix, 1, stride, alpha, beta, tc0 );
}

static ALWAYS_INLINE void deblock_edge_chroma_c( pixel *pix, intptr_t xstride, int alpha, int beta, int8_t tc )
{
    int p1 = pix[-2*xstride];
    int p0 = pix[-1*xstride];
    int q0 = pix[ 0*xstride];
    int q1 = pix[ 1*xstride];

    if( abs( p0 - q0 ) < alpha && abs( p1 - p0 ) < beta && abs( q1 - q0 ) < beta )
    {
        int delta = x264_clip3( (((q0 - p0 ) * 4) + (p1 - q1) + 4) >> 3, -tc, tc );
        pix[-1*xstride] = x264_clip_pixel( p0 + delta );    /* p0' */
        pix[ 0*xstride] = x264_clip_pixel( q0 - delta );    /* q0' */
    }
}
static ALWAYS_INLINE void deblock_chroma_c( pixel *pix, int height, intptr_t xstride, intptr_t ystride, int alpha, int beta, int8_t *tc0 )
{
    for( int i = 0; i < 4; i++ )
    {
        int tc = tc0[i];
        if( tc <= 0 )
        {
            pix += height*ystride;
            continue;
        }
        for( int d = 0; d < height; d++, pix += ystride-2 )
            for( int e = 0; e < 2; e++, pix++ )
                deblock_edge_chroma_c( pix, xstride, alpha, beta, tc0[i] );
    }
}
static void deblock_h_chroma_mbaff_c( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 )
{
    deblock_chroma_c( pix, 1, 2, stride, alpha, beta, tc0 );
}
static void deblock_v_chroma_c( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 )
{
    deblock_chroma_c( pix, 2, stride, 2, alpha, beta, tc0 );
}
static void deblock_h_chroma_c( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 )
{
    deblock_chroma_c( pix, 2, 2, stride, alpha, beta, tc0 );
}
static void deblock_h_chroma_422_c( pixel *pix, intptr_t stride, int alpha, int beta, int8_t *tc0 )
{
    deblock_chroma_c( pix, 4, 2, stride, alpha, beta, tc0 );
}

static ALWAYS_INLINE void deblock_edge_luma_intra_c( pixel *pix, intptr_t xstride, int alpha, int beta )
{
    int p2 = pix[-3*xstride];
    int p1 = pix[-2*xstride];
    int p0 = pix[-1*xstride];
    int q0 = pix[ 0*xstride];
    int q1 = pix[ 1*xstride];
    int q2 = pix[ 2*xstride];

    if( abs( p0 - q0 ) < alpha && abs( p1 - p0 ) < beta && abs( q1 - q0 ) < beta )
    {
        if( abs( p0 - q0 ) < ((alpha >> 2) + 2) )
        {
            if( abs( p2 - p0 ) < beta ) /* p0', p1', p2' */
            {
                const int p3 = pix[-4*xstride];
                pix[-1*xstride] = ( p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4 ) >> 3;
                pix[-2*xstride] = ( p2 + p1 + p0 + q0 + 2 ) >> 2;
                pix[-3*xstride] = ( 2*p3 + 3*p2 + p1 + p0 + q0 + 4 ) >> 3;
            }
            else /* p0' */
                pix[-1*xstride] = ( 2*p1 + p0 + q1 + 2 ) >> 2;
            if( abs( q2 - q0 ) < beta ) /* q0', q1', q2' */
            {
                const int q3 = pix[3*xstride];
                pix[0*xstride] = ( p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4 ) >> 3;
                pix[1*xstride] = ( p0 + q0 + q1 + q2 + 2 ) >> 2;
                pix[2*xstride] = ( 2*q3 + 3*q2 + q1 + q0 + p0 + 4 ) >> 3;
            }
            else /* q0' */
                pix[0*xstride] = ( 2*q1 + q0 + p1 + 2 ) >> 2;
        }
        else /* p0', q0' */
        {
            pix[-1*xstride] = ( 2*p1 + p0 + q1 + 2 ) >> 2;
            pix[ 0*xstride] = ( 2*q1 + q0 + p1 + 2 ) >> 2;
        }
    }
}
static inline void deblock_luma_intra_c( pixel *pix, intptr_t xstride, intptr_t ystride, int alpha, int beta )
{
    for( int d = 0; d < 16; d++, pix += ystride )
        deblock_edge_luma_intra_c( pix, xstride, alpha, beta );
}
static void deblock_h_luma_intra_mbaff_c( pixel *pix, intptr_t ystride, int alpha, int beta )
{
    for( int d = 0; d < 8; d++, pix += ystride )
        deblock_edge_luma_intra_c( pix, 1, alpha, beta );
}
static void deblock_v_luma_intra_c( pixel *pix, intptr_t stride, int alpha, int beta )
{
    deblock_luma_intra_c( pix, stride, 1, alpha, beta );
}
static void deblock_h_luma_intra_c( pixel *pix, intptr_t stride, int alpha, int beta )
{
    deblock_luma_intra_c( pix, 1, stride, alpha, beta );
}

static ALWAYS_INLINE void deblock_edge_chroma_intra_c( pixel *pix, intptr_t xstride, int alpha, int beta )
{
    int p1 = pix[-2*xstride];
    int p0 = pix[-1*xstride];
    int q0 = pix[ 0*xstride];
    int q1 = pix[ 1*xstride];

    if( abs( p0 - q0 ) < alpha && abs( p1 - p0 ) < beta && abs( q1 - q0 ) < beta )
    {
        pix[-1*xstride] = (2*p1 + p0 + q1 + 2) >> 2;   /* p0' */
        pix[ 0*xstride] = (2*q1 + q0 + p1 + 2) >> 2;   /* q0' */
    }
}
static ALWAYS_INLINE void deblock_chroma_intra_c( pixel *pix, int width, int height, intptr_t xstride, intptr_t ystride, int alpha, int beta )
{
    for( int d = 0; d < height; d++, pix += ystride-2 )
        for( int e = 0; e < width; e++, pix++ )
            deblock_edge_chroma_intra_c( pix, xstride, alpha, beta );
}
static void deblock_h_chroma_intra_mbaff_c( pixel *pix, intptr_t stride, int alpha, int beta )
{
    deblock_chroma_intra_c( pix, 2, 4, 2, stride, alpha, beta );
}
static void deblock_v_chroma_intra_c( pixel *pix, intptr_t stride, int alpha, int beta )
{
    deblock_chroma_intra_c( pix, 1, 16, stride, 2, alpha, beta );
}
static void deblock_h_chroma_intra_c( pixel *pix, intptr_t stride, int alpha, int beta )
{
    deblock_chroma_intra_c( pix, 2, 8, 2, stride, alpha, beta );
}
static void deblock_h_chroma_422_intra_c( pixel *pix, intptr_t stride, int alpha, int beta )
{
    deblock_chroma_intra_c( pix, 2, 16, 2, stride, alpha, beta );
}

static void deblock_strength_c( uint8_t nnz[X264_SCAN8_SIZE], int8_t ref[2][X264_SCAN8_LUMA_SIZE],
                                int16_t mv[2][X264_SCAN8_LUMA_SIZE][2], uint8_t bs[2][8][4], int mvy_limit,
                                int bframe )
{
    for( int dir = 0; dir < 2; dir++ )
    {
        int s1 = dir ? 1 : 8;
        int s2 = dir ? 8 : 1;
        for( int edge = 0; edge < 4; edge++ )
            for( int i = 0, loc = X264_SCAN8_0+edge*s2; i < 4; i++, loc += s1 )
            {
                int locn = loc - s2;
                if( nnz[loc] || nnz[locn] )
                    bs[dir][edge][i] = 2;
                else if( ref[0][loc] != ref[0][locn] ||
                         abs( mv[0][loc][0] - mv[0][locn][0] ) >= 4 ||
                         abs( mv[0][loc][1] - mv[0][locn][1] ) >= mvy_limit ||
                        (bframe && (ref[1][loc] != ref[1][locn] ||
                         abs( mv[1][loc][0] - mv[1][locn][0] ) >= 4 ||
                         abs( mv[1][loc][1] - mv[1][locn][1] ) >= mvy_limit )))
                {
                    bs[dir][edge][i] = 1;
                }
                else
                    bs[dir][edge][i] = 0;
            }
    }
}

#if HAVE_MMX
#include "x86/deblock.h"
#endif
#if HAVE_ALTIVEC
#include "ppc/deblock.h"
#endif
#if HAVE_ARMV6
#include "arm/deblock.h"
#endif
#if HAVE_AARCH64
#include "aarch64/deblock.h"
#endif
#if HAVE_MSA
#include "mips/deblock.h"
#endif
#if HAVE_LSX
#include "loongarch/deblock.h"
#endif

void x264_deblock_init( uint32_t cpu, x264_deblock_function_t *pf, int b_mbaff )
{
    pf->deblock_luma[1] = deblock_v_luma_c;
    pf->deblock_luma[0] = deblock_h_luma_c;
    pf->deblock_chroma[1] = deblock_v_chroma_c;
    pf->deblock_h_chroma_420 = deblock_h_chroma_c;
    pf->deblock_h_chroma_422 = deblock_h_chroma_422_c;
    pf->deblock_luma_intra[1] = deblock_v_luma_intra_c;
    pf->deblock_luma_intra[0] = deblock_h_luma_intra_c;
    pf->deblock_chroma_intra[1] = deblock_v_chroma_intra_c;
    pf->deblock_h_chroma_420_intra = deblock_h_chroma_intra_c;
    pf->deblock_h_chroma_422_intra = deblock_h_chroma_422_intra_c;
    pf->deblock_luma_mbaff = deblock_h_luma_mbaff_c;
    pf->deblock_chroma_420_mbaff = deblock_h_chroma_mbaff_c;
    pf->deblock_luma_intra_mbaff = deblock_h_luma_intra_mbaff_c;
    pf->deblock_chroma_420_intra_mbaff = deblock_h_chroma_intra_mbaff_c;
    pf->deblock_strength = deblock_strength_c;

#if HAVE_MMX
    if( cpu&X264_CPU_MMX2 )
    {
#if ARCH_X86
        pf->deblock_luma[1] = x264_deblock_v_luma_mmx2;
        pf->deblock_luma[0] = x264_deblock_h_luma_mmx2;
        pf->deblock_chroma[1] = x264_deblock_v_chroma_mmx2;
        pf->deblock_h_chroma_420 = x264_deblock_h_chroma_mmx2;
        pf->deblock_chroma_420_mbaff = x264_deblock_h_chroma_mbaff_mmx2;
        pf->deblock_h_chroma_422 = x264_deblock_h_chroma_422_mmx2;
        pf->deblock_h_chroma_422_intra = x264_deblock_h_chroma_422_intra_mmx2;
        pf->deblock_luma_intra[1] = x264_deblock_v_luma_intra_mmx2;
        pf->deblock_luma_intra[0] = x264_deblock_h_luma_intra_mmx2;
        pf->deblock_chroma_intra[1] = x264_deblock_v_chroma_intra_mmx2;
        pf->deblock_h_chroma_420_intra = x264_deblock_h_chroma_intra_mmx2;
        pf->deblock_chroma_420_intra_mbaff = x264_deblock_h_chroma_intra_mbaff_mmx2;
#endif
#if !HIGH_BIT_DEPTH
        pf->deblock_chroma_420_intra_mbaff = x264_deblock_h_chroma_intra_mbaff_mmx2;
#endif
        if( cpu&X264_CPU_SSE2 )
        {
            pf->deblock_strength = x264_deblock_strength_sse2;
            pf->deblock_h_chroma_420 = x264_deblock_h_chroma_sse2;
            pf->deblock_h_chroma_422 = x264_deblock_h_chroma_422_sse2;
            pf->deblock_h_chroma_422_intra = x264_deblock_h_chroma_422_intra_sse2;
            pf->deblock_chroma_420_mbaff = x264_deblock_h_chroma_mbaff_sse2;
            pf->deblock_luma[1] = x264_deblock_v_luma_sse2;
            pf->deblock_luma[0] = x264_deblock_h_luma_sse2;
            pf->deblock_luma_intra[1] = x264_deblock_v_luma_intra_sse2;
            pf->deblock_luma_intra[0] = x264_deblock_h_luma_intra_sse2;
            if( !(cpu&X264_CPU_STACK_MOD4) )
            {
                pf->deblock_chroma[1] = x264_deblock_v_chroma_sse2;
                pf->deblock_chroma_intra[1] = x264_deblock_v_chroma_intra_sse2;
                pf->deblock_h_chroma_420_intra = x264_deblock_h_chroma_intra_sse2;
#if HIGH_BIT_DEPTH
                pf->deblock_chroma_420_intra_mbaff= x264_deblock_h_chroma_intra_mbaff_sse2;
#endif
            }
        }
        if( cpu&X264_CPU_SSSE3 )
            pf->deblock_strength = x264_deblock_strength_ssse3;
        if( cpu&X264_CPU_AVX )
        {
            pf->deblock_strength = x264_deblock_strength_avx;
            pf->deblock_h_chroma_420 = x264_deblock_h_chroma_avx;
            pf->deblock_h_chroma_422 = x264_deblock_h_chroma_422_avx;
            pf->deblock_h_chroma_422_intra = x264_deblock_h_chroma_422_intra_avx;
            pf->deblock_luma[1] = x264_deblock_v_luma_avx;
            pf->deblock_luma[0] = x264_deblock_h_luma_avx;
            pf->deblock_luma_intra[1] = x264_deblock_v_luma_intra_avx;
            pf->deblock_luma_intra[0] = x264_deblock_h_luma_intra_avx;
            if( !(cpu&X264_CPU_STACK_MOD4) )
            {
                pf->deblock_chroma[1] = x264_deblock_v_chroma_avx;
                pf->deblock_chroma_intra[1] = x264_deblock_v_chroma_intra_avx;
                pf->deblock_h_chroma_420_intra = x264_deblock_h_chroma_intra_avx;
#if HIGH_BIT_DEPTH
                pf->deblock_chroma_420_mbaff = x264_deblock_h_chroma_mbaff_avx;
                pf->deblock_chroma_420_intra_mbaff = x264_deblock_h_chroma_intra_mbaff_avx;
#endif
            }
        }
        if( cpu&X264_CPU_AVX2 )
        {
            pf->deblock_strength = x264_deblock_strength_avx2;
        }
        if( cpu&X264_CPU_AVX512 )
        {
            pf->deblock_strength = x264_deblock_strength_avx512;
        }
    }
#endif

#if !HIGH_BIT_DEPTH
#if HAVE_ALTIVEC
    if( cpu&X264_CPU_ALTIVEC )
    {
        pf->deblock_luma[1] = x264_deblock_v_luma_altivec;
        pf->deblock_luma[0] = x264_deblock_h_luma_altivec;
    }
#endif // HAVE_ALTIVEC

#if HAVE_ARMV6 || HAVE_AARCH64
    if( cpu&X264_CPU_NEON )
    {
        pf->deblock_luma[1] = x264_deblock_v_luma_neon;
        pf->deblock_luma[0] = x264_deblock_h_luma_neon;
        pf->deblock_chroma[1] = x264_deblock_v_chroma_neon;
        pf->deblock_h_chroma_420 = x264_deblock_h_chroma_neon;
        pf->deblock_h_chroma_422 = x264_deblock_h_chroma_422_neon;
        pf->deblock_chroma_420_mbaff = x264_deblock_h_chroma_mbaff_neon;
        pf->deblock_chroma_420_intra_mbaff = x264_deblock_h_chroma_intra_mbaff_neon;
        pf->deblock_h_chroma_420_intra = x264_deblock_h_chroma_intra_neon;
        pf->deblock_h_chroma_422_intra = x264_deblock_h_chroma_422_intra_neon;
        pf->deblock_chroma_intra[1] = x264_deblock_v_chroma_intra_neon;
        pf->deblock_luma_intra[0] = x264_deblock_h_luma_intra_neon;
        pf->deblock_luma_intra[1] = x264_deblock_v_luma_intra_neon;
        pf->deblock_strength     = x264_deblock_strength_neon;
    }
#if HAVE_SVE
    if ( cpu&X264_CPU_SVE )
    {
        pf->deblock_chroma[1] = x264_deblock_v_chroma_sve;
    }
#endif
#endif

#if HAVE_MSA
    if( cpu&X264_CPU_MSA )
    {
        pf->deblock_luma[1] = x264_deblock_v_luma_msa;
        pf->deblock_luma[0] = x264_deblock_h_luma_msa;
        pf->deblock_chroma[1] = x264_deblock_v_chroma_msa;
        pf->deblock_h_chroma_420 = x264_deblock_h_chroma_msa;
        pf->deblock_luma_intra[1] = x264_deblock_v_luma_intra_msa;
        pf->deblock_luma_intra[0] = x264_deblock_h_luma_intra_msa;
        pf->deblock_chroma_intra[1] = x264_deblock_v_chroma_intra_msa;
        pf->deblock_h_chroma_420_intra = x264_deblock_h_chroma_intra_msa;
        pf->deblock_strength = x264_deblock_strength_msa;
    }
#endif

#if HAVE_LSX
    if( cpu&X264_CPU_LSX )
    {
        pf->deblock_luma_intra[1] = x264_deblock_v_luma_intra_lsx;
        pf->deblock_luma_intra[0] = x264_deblock_h_luma_intra_lsx;
        pf->deblock_strength = x264_deblock_strength_lsx;
    }
    if( cpu&X264_CPU_LASX )
    {
        pf->deblock_luma[1] = x264_deblock_v_luma_lasx;
        pf->deblock_luma[0] = x264_deblock_h_luma_lasx;
        pf->deblock_luma_intra[1] = x264_deblock_v_luma_intra_lasx;
        pf->deblock_luma_intra[0] = x264_deblock_h_luma_intra_lasx;
        pf->deblock_strength = x264_deblock_strength_lasx;
    }
#endif

#endif // !HIGH_BIT_DEPTH

    /* These functions are equivalent, so don't duplicate them. */
    pf->deblock_chroma_422_mbaff = pf->deblock_h_chroma_420;
    pf->deblock_chroma_422_intra_mbaff = pf->deblock_h_chroma_420_intra;
}
