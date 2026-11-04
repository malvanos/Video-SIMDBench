/*****************************************************************************
 * mc-c.c: aarch64 motion compensation
 *****************************************************************************
 * Copyright (C) 2009-2016 x264 project
 *
 * Authors: David Conrad <lessen42@gmail.com>
 *          Janne Grunau <janne-x264@jannau.net>
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

#include "common.h"
#include "osdep.h"
#include "bench.h"
#include "mc.h"
extern const uint8_t vbench_hpel_ref0[16];
extern const uint8_t vbench_hpel_ref1[16];

void asm_prefetch_ref_aarch64( uint8_t *, intptr_t, int );
void asm_prefetch_fenc_420_aarch64( uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_prefetch_fenc_422_aarch64( uint8_t *, intptr_t, uint8_t *, intptr_t, int );

void *asm_memcpy_aligned_neon( void *dst, const void *src, size_t n );
void asm_memzero_aligned_neon( void *dst, size_t n );

void asm_pixel_avg_16x16_neon( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_pixel_avg_16x8_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_pixel_avg_8x16_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_pixel_avg_8x8_neon  ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_pixel_avg_8x4_neon  ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_pixel_avg_4x16_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_pixel_avg_4x8_neon  ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_pixel_avg_4x4_neon  ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_pixel_avg_4x2_neon  ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, intptr_t, int );

void asm_pixel_avg2_w4_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, int );
void asm_pixel_avg2_w8_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, int );
void asm_pixel_avg2_w16_neon( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, int );
void asm_pixel_avg2_w20_neon( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, int );

void asm_plane_copy_neon( pixel *dst, intptr_t i_dst,
                           pixel *src, intptr_t i_src, int w, int h );
void asm_plane_copy_deinterleave_neon(  pixel *dstu, intptr_t i_dstu,
                                         pixel *dstv, intptr_t i_dstv,
                                         pixel *src,  intptr_t i_src, int w, int h );
void asm_plane_copy_deinterleave_rgb_neon( pixel *dsta, intptr_t i_dsta,
                                            pixel *dstb, intptr_t i_dstb,
                                            pixel *dstc, intptr_t i_dstc,
                                            pixel *src,  intptr_t i_src, int pw, int w, int h );
void asm_plane_copy_interleave_neon( pixel *dst,  intptr_t i_dst,
                                      pixel *srcu, intptr_t i_srcu,
                                      pixel *srcv, intptr_t i_srcv, int w, int h );

void asm_store_interleave_chroma_neon( pixel *dst, intptr_t i_dst, pixel *srcu, pixel *srcv, int height );
void asm_load_deinterleave_chroma_fdec_neon( pixel *dst, pixel *src, intptr_t i_src, int height );
void asm_load_deinterleave_chroma_fenc_neon( pixel *dst, pixel *src, intptr_t i_src, int height );

#define MC_WEIGHT(func)\
void asm_mc_weight_w20##func##_neon( uint8_t *, intptr_t, uint8_t *, intptr_t, const vbench_weight_t *, int );\
void asm_mc_weight_w16##func##_neon( uint8_t *, intptr_t, uint8_t *, intptr_t, const vbench_weight_t *, int );\
void asm_mc_weight_w8##func##_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, const vbench_weight_t *, int );\
void asm_mc_weight_w4##func##_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, const vbench_weight_t *, int );\
\
static void (* asm_mc##func##_wtab_neon[6])( uint8_t *, intptr_t, uint8_t *, intptr_t, const vbench_weight_t *, int ) =\
{\
    asm_mc_weight_w4##func##_neon,\
    asm_mc_weight_w4##func##_neon,\
    asm_mc_weight_w8##func##_neon,\
    asm_mc_weight_w16##func##_neon,\
    asm_mc_weight_w16##func##_neon,\
    asm_mc_weight_w20##func##_neon,\
};

MC_WEIGHT()
MC_WEIGHT(_nodenom)
MC_WEIGHT(_offsetadd)
MC_WEIGHT(_offsetsub)

void asm_mc_copy_w4_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_mc_copy_w8_neon ( uint8_t *, intptr_t, uint8_t *, intptr_t, int );
void asm_mc_copy_w16_neon( uint8_t *, intptr_t, uint8_t *, intptr_t, int );

void asm_mc_chroma_neon( uint8_t *, uint8_t *, intptr_t, uint8_t *, intptr_t, int, int, int, int );
void integral_init4h_neon( uint16_t *, uint8_t *, intptr_t );
void integral_init4v_neon( uint16_t *, uint16_t *, intptr_t );
void integral_init8h_neon( uint16_t *, uint8_t *, intptr_t );
void integral_init8v_neon( uint16_t *, intptr_t );
void asm_frame_init_lowres_core_neon( uint8_t *, uint8_t *, uint8_t *, uint8_t *, uint8_t *, intptr_t, intptr_t, int, int );

void asm_mbtree_propagate_cost_neon( int16_t *, uint16_t *, uint16_t *, uint16_t *, uint16_t *, float *, int );

#if !HIGH_BIT_DEPTH
static void asm_weight_cache_neon( vbench_mc_functions_t *h, vbench_weight_t *w )
{
    if( w->i_scale == 1<<w->i_denom )
    {
        if( w->i_offset < 0 )
        {
            w->weightfn = asm_mc_offsetsub_wtab_neon;
            w->cachea[0] = -w->i_offset;
        }
        else
        {
            w->weightfn = asm_mc_offsetadd_wtab_neon;
            w->cachea[0] = w->i_offset;
        }
    }
    else if( !w->i_denom )
        w->weightfn = asm_mc_nodenom_wtab_neon;
    else
        w->weightfn = asm_mc_wtab_neon;
}

static void (* const asm_pixel_avg_wtab_neon[6])( uint8_t *, intptr_t, uint8_t *, intptr_t, uint8_t *, int ) =
{
    NULL,
    asm_pixel_avg2_w4_neon,
    asm_pixel_avg2_w8_neon,
    asm_pixel_avg2_w16_neon,   // no slower than w12, so no point in a separate function
    asm_pixel_avg2_w16_neon,
    asm_pixel_avg2_w20_neon,
};

static void (* const asm_mc_copy_wtab_neon[5])( uint8_t *, intptr_t, uint8_t *, intptr_t, int ) =
{
    NULL,
    asm_mc_copy_w4_neon,
    asm_mc_copy_w8_neon,
    NULL,
    asm_mc_copy_w16_neon,
};

static void mc_luma_neon( uint8_t *dst,    intptr_t i_dst_stride,
                          uint8_t *src[4], intptr_t i_src_stride,
                          int mvx, int mvy,
                          int i_width, int i_height, const vbench_weight_t *weight )
{
    int qpel_idx = ((mvy&3)<<2) + (mvx&3);
    intptr_t offset = (mvy>>2)*i_src_stride + (mvx>>2);
    uint8_t *src1 = src[vbench_hpel_ref0[qpel_idx]] + offset;
    if ( (mvy&3) == 3 )             // explict if() to force conditional add
        src1 += i_src_stride;

    if( qpel_idx & 5 ) /* qpel interpolation needed */
    {
        uint8_t *src2 = src[vbench_hpel_ref1[qpel_idx]] + offset + ((mvx&3) == 3);
        asm_pixel_avg_wtab_neon[i_width>>2](
                dst, i_dst_stride, src1, i_src_stride,
                src2, i_height );
        if( weight->weightfn )
            weight->weightfn[i_width>>2]( dst, i_dst_stride, dst, i_dst_stride, weight, i_height );
    }
    else if( weight->weightfn )
        weight->weightfn[i_width>>2]( dst, i_dst_stride, src1, i_src_stride, weight, i_height );
    else
        asm_mc_copy_wtab_neon[i_width>>2]( dst, i_dst_stride, src1, i_src_stride, i_height );
}

static uint8_t *get_ref_neon( uint8_t *dst,   intptr_t *i_dst_stride,
                              uint8_t *src[4], intptr_t i_src_stride,
                              int mvx, int mvy,
                              int i_width, int i_height, const vbench_weight_t *weight )
{
    int qpel_idx = ((mvy&3)<<2) + (mvx&3);
    intptr_t offset = (mvy>>2)*i_src_stride + (mvx>>2);
    uint8_t *src1 = src[vbench_hpel_ref0[qpel_idx]] + offset;
    if ( (mvy&3) == 3 )             // explict if() to force conditional add
        src1 += i_src_stride;

    if( qpel_idx & 5 ) /* qpel interpolation needed */
    {
        uint8_t *src2 = src[vbench_hpel_ref1[qpel_idx]] + offset + ((mvx&3) == 3);
        asm_pixel_avg_wtab_neon[i_width>>2](
                dst, *i_dst_stride, src1, i_src_stride,
                src2, i_height );
        if( weight->weightfn )
            weight->weightfn[i_width>>2]( dst, *i_dst_stride, dst, *i_dst_stride, weight, i_height );
        return dst;
    }
    else if( weight->weightfn )
    {
        weight->weightfn[i_width>>2]( dst, *i_dst_stride, src1, i_src_stride, weight, i_height );
        return dst;
    }
    else
    {
        *i_dst_stride = i_src_stride;
        return src1;
    }
}

void asm_hpel_filter_neon( uint8_t *dsth, uint8_t *dstv, uint8_t *dstc,
                            uint8_t *src, intptr_t stride, int width,
                            int height, int16_t *buf );
#endif // !HIGH_BIT_DEPTH


void vbench_mc_init_aarch64( int cpu, vbench_mc_functions_t *pf )
{
#if !HIGH_BIT_DEPTH
    if( cpu&VSIMD_CPU_ARMV8 )
    {
        pf->prefetch_fenc_420 = asm_prefetch_fenc_420_aarch64;
        pf->prefetch_fenc_422 = asm_prefetch_fenc_422_aarch64;
        pf->prefetch_ref      = asm_prefetch_ref_aarch64;
    }

    if( !(cpu&VSIMD_CPU_NEON) )
        return;

    pf->copy_16x16_unaligned = asm_mc_copy_w16_neon;
    pf->copy[PIXEL_16x16]    = asm_mc_copy_w16_neon;
    pf->copy[PIXEL_8x8]      = asm_mc_copy_w8_neon;
    pf->copy[PIXEL_4x4]      = asm_mc_copy_w4_neon;

    pf->plane_copy                  = asm_plane_copy_neon;
    pf->plane_copy_deinterleave     = asm_plane_copy_deinterleave_neon;
    pf->plane_copy_deinterleave_rgb = asm_plane_copy_deinterleave_rgb_neon;
    pf->plane_copy_interleave       = asm_plane_copy_interleave_neon;

    pf->load_deinterleave_chroma_fdec = asm_load_deinterleave_chroma_fdec_neon;
    pf->load_deinterleave_chroma_fenc = asm_load_deinterleave_chroma_fenc_neon;
    pf->store_interleave_chroma       = asm_store_interleave_chroma_neon;

    pf->avg[PIXEL_16x16] = asm_pixel_avg_16x16_neon;
    pf->avg[PIXEL_16x8]  = asm_pixel_avg_16x8_neon;
    pf->avg[PIXEL_8x16]  = asm_pixel_avg_8x16_neon;
    pf->avg[PIXEL_8x8]   = asm_pixel_avg_8x8_neon;
    pf->avg[PIXEL_8x4]   = asm_pixel_avg_8x4_neon;
    pf->avg[PIXEL_4x16]  = asm_pixel_avg_4x16_neon;
    pf->avg[PIXEL_4x8]   = asm_pixel_avg_4x8_neon;
    pf->avg[PIXEL_4x4]   = asm_pixel_avg_4x4_neon;
    pf->avg[PIXEL_4x2]   = asm_pixel_avg_4x2_neon;

    pf->weight       = asm_mc_wtab_neon;
    pf->offsetadd    = asm_mc_offsetadd_wtab_neon;
    pf->offsetsub    = asm_mc_offsetsub_wtab_neon;
    pf->weight_cache = asm_weight_cache_neon;

    pf->mc_chroma = asm_mc_chroma_neon;
    pf->mc_luma = mc_luma_neon;
    pf->get_ref = get_ref_neon;
    pf->hpel_filter = asm_hpel_filter_neon;
    pf->frame_init_lowres_core = asm_frame_init_lowres_core_neon;

    pf->integral_init4h = integral_init4h_neon;
    pf->integral_init8h = integral_init8h_neon;
    pf->integral_init4v = integral_init4v_neon;
    pf->integral_init8v = integral_init8v_neon;

    pf->mbtree_propagate_cost = asm_mbtree_propagate_cost_neon;

    pf->memcpy_aligned  = asm_memcpy_aligned_neon;
    pf->memzero_aligned = asm_memzero_aligned_neon;
#endif // !HIGH_BIT_DEPTH
}
