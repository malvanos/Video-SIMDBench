/*****************************************************************************
 * predict.h: intra prediction
 *****************************************************************************
 * Copyright (C) 2003-2016 x264 project
 *
 * Authors: Loren Merritt <lorenm@u.washington.edu>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef PREDICT_H
#define PREDICT_H

typedef void (*vbench_predict_t)( pixel *src );
typedef void (*vbench_predict8x8_t)( pixel *src, pixel edge[36] );
typedef void (*vbench_predict_8x8_filter_t) ( pixel *src, pixel edge[36], int i_neighbor, int i_filters );

enum intra_chroma_pred_e
{
    I_PRED_CHROMA_DC = 0,
    I_PRED_CHROMA_H  = 1,
    I_PRED_CHROMA_V  = 2,
    I_PRED_CHROMA_P  = 3,

    I_PRED_CHROMA_DC_LEFT = 4,
    I_PRED_CHROMA_DC_TOP  = 5,
    I_PRED_CHROMA_DC_128  = 6
};
static const uint8_t vbench_mb_chroma_pred_mode_fix[7] =
{
    I_PRED_CHROMA_DC, I_PRED_CHROMA_H, I_PRED_CHROMA_V, I_PRED_CHROMA_P,
    I_PRED_CHROMA_DC, I_PRED_CHROMA_DC,I_PRED_CHROMA_DC
};

enum intra16x16_pred_e
{
    I_PRED_16x16_V  = 0,
    I_PRED_16x16_H  = 1,
    I_PRED_16x16_DC = 2,
    I_PRED_16x16_P  = 3,

    I_PRED_16x16_DC_LEFT = 4,
    I_PRED_16x16_DC_TOP  = 5,
    I_PRED_16x16_DC_128  = 6,
};
static const uint8_t vbench_mb_pred_mode16x16_fix[7] =
{
    I_PRED_16x16_V, I_PRED_16x16_H, I_PRED_16x16_DC, I_PRED_16x16_P,
    I_PRED_16x16_DC,I_PRED_16x16_DC,I_PRED_16x16_DC
};

enum intra4x4_pred_e
{
    I_PRED_4x4_V  = 0,
    I_PRED_4x4_H  = 1,
    I_PRED_4x4_DC = 2,
    I_PRED_4x4_DDL= 3,
    I_PRED_4x4_DDR= 4,
    I_PRED_4x4_VR = 5,
    I_PRED_4x4_HD = 6,
    I_PRED_4x4_VL = 7,
    I_PRED_4x4_HU = 8,

    I_PRED_4x4_DC_LEFT = 9,
    I_PRED_4x4_DC_TOP  = 10,
    I_PRED_4x4_DC_128  = 11,
};
static const int8_t vbench_mb_pred_mode4x4_fix[13] =
{
    -1,
    I_PRED_4x4_V,   I_PRED_4x4_H,   I_PRED_4x4_DC,
    I_PRED_4x4_DDL, I_PRED_4x4_DDR, I_PRED_4x4_VR,
    I_PRED_4x4_HD,  I_PRED_4x4_VL,  I_PRED_4x4_HU,
    I_PRED_4x4_DC,  I_PRED_4x4_DC,  I_PRED_4x4_DC
};
#define vbench_mb_pred_mode4x4_fix(t) vbench_mb_pred_mode4x4_fix[(t)+1]

/* must use the same numbering as intra4x4_pred_e */
enum intra8x8_pred_e
{
    I_PRED_8x8_V  = 0,
    I_PRED_8x8_H  = 1,
    I_PRED_8x8_DC = 2,
    I_PRED_8x8_DDL= 3,
    I_PRED_8x8_DDR= 4,
    I_PRED_8x8_VR = 5,
    I_PRED_8x8_HD = 6,
    I_PRED_8x8_VL = 7,
    I_PRED_8x8_HU = 8,

    I_PRED_8x8_DC_LEFT = 9,
    I_PRED_8x8_DC_TOP  = 10,
    I_PRED_8x8_DC_128  = 11,
};

void vbench_predict_8x8_dc_c  ( pixel *src, pixel edge[36] );
void vbench_predict_8x8_h_c   ( pixel *src, pixel edge[36] );
void vbench_predict_8x8_v_c   ( pixel *src, pixel edge[36] );
void vbench_predict_4x4_dc_c  ( pixel *src );
void vbench_predict_4x4_h_c   ( pixel *src );
void vbench_predict_4x4_v_c   ( pixel *src );
void vbench_predict_16x16_dc_c( pixel *src );
void vbench_predict_16x16_h_c ( pixel *src );
void vbench_predict_16x16_v_c ( pixel *src );
void vbench_predict_16x16_p_c ( pixel *src );
void vbench_predict_8x8c_dc_c ( pixel *src );
void vbench_predict_8x8c_h_c  ( pixel *src );
void vbench_predict_8x8c_v_c  ( pixel *src );
void vbench_predict_8x8c_p_c  ( pixel *src );
void vbench_predict_8x16c_dc_c( pixel *src );
void vbench_predict_8x16c_h_c ( pixel *src );
void vbench_predict_8x16c_v_c ( pixel *src );
void vbench_predict_8x16c_p_c ( pixel *src );

void vbench_predict_16x16_init ( int cpu, vbench_predict_t pf[7] );
void vbench_predict_8x8c_init  ( int cpu, vbench_predict_t pf[7] );
void vbench_predict_8x16c_init ( int cpu, vbench_predict_t pf[7] );
void vbench_predict_4x4_init   ( int cpu, vbench_predict_t pf[12] );
void vbench_predict_8x8_init   ( int cpu, vbench_predict8x8_t pf[12], vbench_predict_8x8_filter_t *predict_filter );


#if HAVE_MMX
void vbench_predict_16x16_init_mmx ( int cpu, vbench_predict_t pf[7] );
void vbench_predict_8x16c_init_mmx  ( int cpu, vbench_predict_t pf[7] );
void vbench_predict_8x8c_init_mmx  ( int cpu, vbench_predict_t pf[7] );
void vbench_predict_4x4_init_mmx   ( int cpu, vbench_predict_t pf[12] );
void vbench_predict_8x8_init_mmx   ( int cpu, vbench_predict8x8_t pf[12], vbench_predict_8x8_filter_t *predict_8x8_filter );

void asm_predict_16x16_v_mmx2( pixel *src );
void asm_predict_16x16_v_sse ( pixel *src );
void asm_predict_16x16_v_avx ( uint16_t *src );
void asm_predict_16x16_h_mmx2( pixel *src );
void asm_predict_16x16_h_sse2( uint16_t *src );
void asm_predict_16x16_h_ssse3( uint8_t *src );
void asm_predict_16x16_h_avx2( uint16_t *src );
void asm_predict_16x16_dc_mmx2( pixel *src );
void asm_predict_16x16_dc_sse2( pixel *src );
void asm_predict_16x16_dc_core_mmx2( pixel *src, int i_dc_left );
void asm_predict_16x16_dc_core_sse2( pixel *src, int i_dc_left );
void asm_predict_16x16_dc_core_avx2( pixel *src, int i_dc_left );
void asm_predict_16x16_dc_left_core_mmx2( pixel *src, int i_dc_left );
void asm_predict_16x16_dc_left_core_sse2( pixel *src, int i_dc_left );
void asm_predict_16x16_dc_left_core_avx2( pixel *src, int i_dc_left );
void asm_predict_16x16_dc_top_mmx2( pixel *src );
void asm_predict_16x16_dc_top_sse2( pixel *src );
void asm_predict_16x16_dc_top_avx2( pixel *src );
void asm_predict_16x16_p_core_mmx2( uint8_t *src, int i00, int b, int c );
void asm_predict_16x16_p_core_sse2( pixel *src, int i00, int b, int c );
void asm_predict_16x16_p_core_avx( pixel *src, int i00, int b, int c );
void asm_predict_16x16_p_core_avx2( pixel *src, int i00, int b, int c );
void asm_predict_8x16c_dc_mmx2( pixel *src );
void asm_predict_8x16c_dc_sse2( uint16_t *src );
void asm_predict_8x16c_dc_top_mmx2( uint8_t *src );
void asm_predict_8x16c_dc_top_sse2( uint16_t *src );
void asm_predict_8x16c_v_mmx( uint8_t *src );
void asm_predict_8x16c_v_sse( uint16_t *src );
void asm_predict_8x16c_h_mmx2( pixel *src );
void asm_predict_8x16c_h_sse2( uint16_t *src );
void asm_predict_8x16c_h_ssse3( uint8_t *src );
void asm_predict_8x16c_h_avx2( uint16_t *src );
void asm_predict_8x16c_p_core_mmx2( uint8_t *src, int i00, int b, int c );
void asm_predict_8x16c_p_core_sse2( pixel *src, int i00, int b, int c );
void asm_predict_8x16c_p_core_avx ( pixel *src, int i00, int b, int c );
void asm_predict_8x16c_p_core_avx2( pixel *src, int i00, int b, int c );
void asm_predict_8x8c_p_core_mmx2( uint8_t *src, int i00, int b, int c );
void asm_predict_8x8c_p_core_sse2( pixel *src, int i00, int b, int c );
void asm_predict_8x8c_p_core_avx ( pixel *src, int i00, int b, int c );
void asm_predict_8x8c_p_core_avx2( pixel *src, int i00, int b, int c );
void asm_predict_8x8c_dc_mmx2( pixel *src );
void asm_predict_8x8c_dc_sse2( uint16_t *src );
void asm_predict_8x8c_dc_top_mmx2( uint8_t *src );
void asm_predict_8x8c_dc_top_sse2( uint16_t *src );
void asm_predict_8x8c_v_mmx( pixel *src );
void asm_predict_8x8c_v_sse( uint16_t *src );
void asm_predict_8x8c_h_mmx2( pixel *src );
void asm_predict_8x8c_h_sse2( uint16_t *src );
void asm_predict_8x8c_h_ssse3( uint8_t *src );
void asm_predict_8x8c_h_avx2( uint16_t *src );
void asm_predict_8x8_v_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_v_sse ( uint16_t *src, uint16_t edge[36] );
void asm_predict_8x8_h_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_h_sse2( uint16_t *src, uint16_t edge[36] );
void asm_predict_8x8_hd_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_hu_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_dc_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_dc_sse2( uint16_t *src, uint16_t edge[36] );
void asm_predict_8x8_dc_top_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_dc_top_sse2( uint16_t *src, uint16_t edge[36] );
void asm_predict_8x8_dc_left_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_dc_left_sse2( uint16_t *src, uint16_t edge[36] );
void asm_predict_8x8_ddl_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_ddl_sse2( pixel *src, pixel edge[36] );
void asm_predict_8x8_ddl_ssse3( pixel *src, pixel edge[36] );
void asm_predict_8x8_ddl_ssse3_cache64( pixel *src, pixel edge[36] );
void asm_predict_8x8_ddl_avx( pixel *src, pixel edge[36] );
void asm_predict_8x8_ddr_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_ddr_sse2( pixel *src, pixel edge[36] );
void asm_predict_8x8_ddr_ssse3( pixel *src, pixel edge[36] );
void asm_predict_8x8_ddr_ssse3_cache64( pixel *src, pixel edge[36] );
void asm_predict_8x8_ddr_avx( pixel *src, pixel edge[36] );
void asm_predict_8x8_vl_sse2( pixel *src, pixel edge[36] );
void asm_predict_8x8_vl_ssse3( pixel *src, pixel edge[36] );
void asm_predict_8x8_vl_avx( pixel *src, pixel edge[36] );
void asm_predict_8x8_vl_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_vr_mmx2( uint8_t *src, uint8_t edge[36] );
void asm_predict_8x8_vr_sse2( pixel *src, pixel edge[36] );
void asm_predict_8x8_vr_ssse3( pixel *src, pixel edge[36] );
void asm_predict_8x8_vr_avx( pixel *src, pixel edge[36] );
void asm_predict_8x8_hu_sse2( pixel *src, pixel edge[36] );
void asm_predict_8x8_hu_ssse3( pixel *src, pixel edge[36] );
void asm_predict_8x8_hu_avx( pixel *src, pixel edge[36] );
void asm_predict_8x8_hd_sse2( pixel *src, pixel edge[36] );
void asm_predict_8x8_hd_ssse3( pixel *src, pixel edge[36] );
void asm_predict_8x8_hd_avx( pixel *src, pixel edge[36] );
void asm_predict_8x8_filter_mmx2( uint8_t *src, uint8_t edge[36], int i_neighbor, int i_filters );
void asm_predict_8x8_filter_sse2( uint16_t *src, uint16_t edge[36], int i_neighbor, int i_filters );
void asm_predict_8x8_filter_ssse3( pixel *src, pixel edge[36], int i_neighbor, int i_filters );
void asm_predict_8x8_filter_avx( uint16_t *src, uint16_t edge[36], int i_neighbor, int i_filters );
void asm_predict_4x4_h_avx2( uint16_t *src );
void asm_predict_4x4_ddl_mmx2( pixel *src );
void asm_predict_4x4_ddl_sse2( uint16_t *src );
void asm_predict_4x4_ddl_avx( uint16_t *src );
void asm_predict_4x4_ddr_mmx2( pixel *src );
void asm_predict_4x4_vl_mmx2( pixel *src );
void asm_predict_4x4_vl_sse2( uint16_t *src );
void asm_predict_4x4_vl_avx( uint16_t *src );
void asm_predict_4x4_vr_mmx2( uint8_t *src );
void asm_predict_4x4_vr_sse2( uint16_t *src );
void asm_predict_4x4_vr_ssse3( pixel *src );
void asm_predict_4x4_vr_ssse3_cache64( uint8_t *src );
void asm_predict_4x4_vr_avx( uint16_t *src );
void asm_predict_4x4_hd_mmx2( pixel *src );
void asm_predict_4x4_hd_sse2( uint16_t *src );
void asm_predict_4x4_hd_ssse3( pixel *src );
void asm_predict_4x4_hd_avx( uint16_t *src );
void asm_predict_4x4_dc_mmx2( pixel *src );
void asm_predict_4x4_ddr_sse2( uint16_t *src );
void asm_predict_4x4_ddr_ssse3( pixel *src );
void asm_predict_4x4_ddr_avx( uint16_t *src );
void asm_predict_4x4_hu_mmx2( pixel *src );


#endif
#if ARCH_PPC
//#   include "ppc/predict.h"
#endif
#if ARCH_ARM
//#   include "arm/predict.h"
#endif
#if ARCH_AARCH64
//#   include "aarch64/predict.h"
#endif
#if ARCH_MIPS
//#   include "mips/predict.h"
#endif


static ALWAYS_INLINE uint32_t pack16to32( uint32_t a, uint32_t b )
{
#if WORDS_BIGENDIAN
       return b + (a<<16);
#else
          return a + (b<<16);
#endif
}
static ALWAYS_INLINE uint32_t pack8to16( uint32_t a, uint32_t b )
{
#if WORDS_BIGENDIAN
       return b + (a<<8);
#else
          return a + (b<<8);
#endif
}
static ALWAYS_INLINE uint32_t pack8to32( uint32_t a, uint32_t b, uint32_t c, uint32_t d )
{
#if WORDS_BIGENDIAN
       return d + (c<<8) + (b<<16) + (a<<24);
#else
          return a + (b<<8) + (c<<16) + (d<<24);
#endif
}
static ALWAYS_INLINE uint32_t pack16to32_mask( int a, int b )
{
#if WORDS_BIGENDIAN
       return (b&0xFFFF) + (a<<16);
#else
          return (a&0xFFFF) + (b<<16);
#endif
}
static ALWAYS_INLINE uint64_t pack32to64( uint32_t a, uint32_t b )
{
#if WORDS_BIGENDIAN
       return b + ((uint64_t)a<<32);
#else
          return a + ((uint64_t)b<<32);
#endif
}

#   define pack_pixel_1to2_10b pack16to32
#   define pack_pixel_2to4_10b pack32to64
#   define pack_pixel_1to2 pack8to16
#   define pack_pixel_2to4 pack16to32

#endif
