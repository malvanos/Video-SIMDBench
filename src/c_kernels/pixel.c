/*****************************************************************************
 * pixel.c: pixel metrics
 *****************************************************************************
 *
 * Copyright (C) 2016 Michail Alvanos
 * Copyright (C) 2003-2016 x264 project
 *
 * Authors of x264 : Loren Merritt <lorenm@u.washington.edu>
 *                   Laurent Aimar <fenrir@via.ecp.fr>
 *                   Fiona Glaser <fiona@x264.com>
 *          
 * Author of Video SIMD-Benchmark: Michail Alvanos <malvanos@gmail.com>
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
 * This x264 is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *
 * *************************************************************************** 
 *
 * Notes:   The _10b subfix is for the high bit depth. Each funcion that has 
 *          pixel as argument must have an alternative version with higher
 *          depth. This usually slows down by two the speed.
 *
 * *************************************************************************** 
 *
 *  Version - Changes
 *
 *  0.1     - Leave only code responsible for pixel processing.
 *
 *
 *****************************************************************************/

#include "common.h"
#include "osdep.h"
#include "pixel.h"


/****************************************************************************
 * pixel_sad_WxH
 ****************************************************************************/
#define PIXEL_SAD_C( name, lx, ly ) \
int name( pixel *pix1, intptr_t i_stride_pix1,  \
          pixel *pix2, intptr_t i_stride_pix2 ) \
{                                                   \
    int i_sum = 0;                                  \
    for( int y = 0; y < ly; y++ )                   \
    {                                               \
        for( int x = 0; x < lx; x++ )               \
        {                                           \
            i_sum += abs( pix1[x] - pix2[x] );      \
        }                                           \
        pix1 += i_stride_pix1;                      \
        pix2 += i_stride_pix2;                      \
    }                                               \
    return i_sum;                                   \
}

#define PIXEL_SAD_C_10B( name, lx, ly ) \
int name( pixel_10b *pix1, intptr_t i_stride_pix1,  \
          pixel_10b *pix2, intptr_t i_stride_pix2 ) \
{                                                   \
    int i_sum = 0;                                  \
    for( int y = 0; y < ly; y++ )                   \
    {                                               \
        for( int x = 0; x < lx; x++ )               \
        {                                           \
            i_sum += abs( pix1[x] - pix2[x] );      \
        }                                           \
        pix1 += i_stride_pix1;                      \
        pix2 += i_stride_pix2;                      \
    }                                               \
    return i_sum;                                   \
}




PIXEL_SAD_C( pixel_sad_16x16, 16, 16 )
PIXEL_SAD_C( pixel_sad_16x8,  16,  8 )
PIXEL_SAD_C( pixel_sad_8x16,   8, 16 )
PIXEL_SAD_C( pixel_sad_8x8,    8,  8 )
PIXEL_SAD_C( pixel_sad_8x4,    8,  4 )
PIXEL_SAD_C( pixel_sad_4x16,   4, 16 )
PIXEL_SAD_C( pixel_sad_4x8,    4,  8 )
PIXEL_SAD_C( pixel_sad_4x4,    4,  4 )



PIXEL_SAD_C_10B( pixel_sad_16x16_10b, 16, 16 )
PIXEL_SAD_C_10B( pixel_sad_16x8_10b,  16,  8 )
PIXEL_SAD_C_10B( pixel_sad_8x16_10b,   8, 16 )
PIXEL_SAD_C_10B( pixel_sad_8x8_10b,    8,  8 )
PIXEL_SAD_C_10B( pixel_sad_8x4_10b,    8,  4 )
PIXEL_SAD_C_10B( pixel_sad_4x16_10b,   4, 16 )
PIXEL_SAD_C_10B( pixel_sad_4x8_10b,    4,  8 )
PIXEL_SAD_C_10B( pixel_sad_4x4_10b,    4,  4 )


/****************************************************************************
 * pixel_sad_x4
 ****************************************************************************/
#define SAD_X( size ) \
void pixel_sad_x3_##size( pixel *fenc, pixel *pix0, pixel *pix1, pixel *pix2,\
                                      intptr_t i_stride, int scores[3] )\
{\
    scores[0] = pixel_sad_##size( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = pixel_sad_##size( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = pixel_sad_##size( fenc, FENC_STRIDE, pix2, i_stride );\
}\
void pixel_sad_x4_##size( pixel *fenc, pixel *pix0, pixel *pix1,pixel *pix2, pixel *pix3,\
                                      intptr_t i_stride, int scores[4] )\
{\
    scores[0] = pixel_sad_##size( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = pixel_sad_##size( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = pixel_sad_##size( fenc, FENC_STRIDE, pix2, i_stride );\
    scores[3] = pixel_sad_##size( fenc, FENC_STRIDE, pix3, i_stride );\
}

SAD_X( 16x16 )
SAD_X( 16x8 )
SAD_X( 8x16 )
SAD_X( 8x8 )
SAD_X( 8x4 )
SAD_X( 4x8 )
SAD_X( 4x4 )


#define SAD_X_10B( size ) \
void pixel_sad_x3_##size( pixel_10b *fenc, pixel_10b *pix0, pixel_10b *pix1, pixel_10b *pix2,\
                                      intptr_t i_stride, int scores[3] )\
{\
    scores[0] = pixel_sad_##size( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = pixel_sad_##size( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = pixel_sad_##size( fenc, FENC_STRIDE, pix2, i_stride );\
}\
void pixel_sad_x4_##size( pixel_10b *fenc, pixel_10b *pix0, pixel_10b *pix1, pixel_10b *pix2, pixel_10b *pix3,\
                                      intptr_t i_stride, int scores[4] )\
{\
    scores[0] = pixel_sad_##size( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = pixel_sad_##size( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = pixel_sad_##size( fenc, FENC_STRIDE, pix2, i_stride );\
    scores[3] = pixel_sad_##size( fenc, FENC_STRIDE, pix3, i_stride );\
}

SAD_X_10B( 16x16_10b )
SAD_X_10B( 16x8_10b )
SAD_X_10B( 8x16_10b )
SAD_X_10B( 8x8_10b )
SAD_X_10B( 8x4_10b )
SAD_X_10B( 4x8_10b )
SAD_X_10B( 4x4_10b )



/****************************************************************************
 * pixel_ssd_WxH
 ****************************************************************************/
#define PIXEL_SSD_C( name, lx, ly ) \
int name( pixel *pix1, intptr_t i_stride_pix1,  \
          pixel *pix2, intptr_t i_stride_pix2 ) \
{                                                   \
    int i_sum = 0;                                  \
    for( int y = 0; y < ly; y++ )                   \
    {                                               \
        for( int x = 0; x < lx; x++ )               \
        {                                           \
            int d = pix1[x] - pix2[x];              \
            i_sum += d*d;                           \
        }                                           \
        pix1 += i_stride_pix1;                      \
        pix2 += i_stride_pix2;                      \
    }                                               \
    return i_sum;                                   \
}

#define PIXEL_SSD_C_10B( name, lx, ly ) \
int name( pixel_10b *pix1, intptr_t i_stride_pix1,  \
          pixel_10b *pix2, intptr_t i_stride_pix2 ) \
{                                                   \
    int i_sum = 0;                                  \
    for( int y = 0; y < ly; y++ )                   \
    {                                               \
        for( int x = 0; x < lx; x++ )               \
        {                                           \
            int d = pix1[x] - pix2[x];              \
            i_sum += d*d;                           \
        }                                           \
        pix1 += i_stride_pix1;                      \
        pix2 += i_stride_pix2;                      \
    }                                               \
    return i_sum;                                   \
}
PIXEL_SSD_C( pixel_ssd_16x16, 16, 16 )
PIXEL_SSD_C( pixel_ssd_16x8,  16,  8 )
PIXEL_SSD_C( pixel_ssd_8x16,   8, 16 )
PIXEL_SSD_C( pixel_ssd_8x8,    8,  8 )
PIXEL_SSD_C( pixel_ssd_8x4,    8,  4 )
PIXEL_SSD_C( pixel_ssd_4x16,   4, 16 )
PIXEL_SSD_C( pixel_ssd_4x8,    4,  8 )
PIXEL_SSD_C( pixel_ssd_4x4,    4,  4 )


PIXEL_SSD_C_10B( pixel_ssd_16x16_10b, 16, 16 )
PIXEL_SSD_C_10B( pixel_ssd_16x8_10b,  16,  8 )
PIXEL_SSD_C_10B( pixel_ssd_8x16_10b,   8, 16 )
PIXEL_SSD_C_10B( pixel_ssd_8x8_10b,    8,  8 )
PIXEL_SSD_C_10B( pixel_ssd_8x4_10b,    8,  4 )
PIXEL_SSD_C_10B( pixel_ssd_4x16_10b,   4, 16 )
PIXEL_SSD_C_10B( pixel_ssd_4x8_10b,    4,  8 )
PIXEL_SSD_C_10B( pixel_ssd_4x4_10b,    4,  4 )



uint64_t pixel_ssd_wxh( pixel *pix1, intptr_t i_pix1,
                             pixel *pix2, intptr_t i_pix2, int i_width, int i_height )
{
    uint64_t i_ssd = 0;
    int y;
    int align = !(((intptr_t)pix1 | (intptr_t)pix2 | i_pix1 | i_pix2) & 15);

    for( y = 0; y < i_height-15; y += 16 )
    {
        int x = 0;
        if( align ){
            for( ; x < i_width-15; x += 16 ){
                i_ssd += pixel_ssd_16x16( pix1 + y*i_pix1 + x, i_pix1, \
                                               pix2 + y*i_pix2 + x, i_pix2 );
            }
        }
        for( ; x < i_width-7; x += 8 ){
            i_ssd += pixel_ssd_8x16( pix1 + y*i_pix1 + x, i_pix1, \
                                          pix2 + y*i_pix2 + x, i_pix2 );
        }
    }
    if( y < i_height-7 ){
        for( int x = 0; x < i_width-7; x += 8 ){
            i_ssd += pixel_ssd_8x8( pix1 + y*i_pix1 + x, i_pix1, \
                                         pix2 + y*i_pix2 + x, i_pix2 );
        }
    }

#define SSD1 { int d = pix1[y*i_pix1+x] - pix2[y*i_pix2+x]; i_ssd += d*d; }
    if( i_width & 7 )
    {
        for( y = 0; y < (i_height & ~7); y++ )
            for( int x = i_width & ~7; x < i_width; x++ )
                SSD1;
    }
    if( i_height & 7 )
    {
        for( y = i_height & ~7; y < i_height; y++ )
            for( int x = 0; x < i_width; x++ )
                SSD1;
    }
#undef SSD1

    return i_ssd;
}


uint64_t pixel_ssd_wxh_10b( pixel_10b *pix1, intptr_t i_pix1,
                                 pixel_10b *pix2, intptr_t i_pix2, int i_width, int i_height )
{
    uint64_t i_ssd = 0;
    int y;
    int align = !(((intptr_t)pix1 | (intptr_t)pix2 | i_pix1 | i_pix2) & 15);

    for( y = 0; y < i_height-15; y += 16 )
    {
        int x = 0;
        if( align ){
            for( ; x < i_width-15; x += 16 ){
                i_ssd += pixel_ssd_16x16_10b( pix1 + y*i_pix1 + x, i_pix1, \
                                               pix2 + y*i_pix2 + x, i_pix2 );
            }
        }
        for( ; x < i_width-7; x += 8 ){
            i_ssd += pixel_ssd_8x16_10b( pix1 + y*i_pix1 + x, i_pix1, \
                                          pix2 + y*i_pix2 + x, i_pix2 );
        }
    }
    if( y < i_height-7 ){
        for( int x = 0; x < i_width-7; x += 8 ){
            i_ssd += pixel_ssd_8x8_10b( pix1 + y*i_pix1 + x, i_pix1, \
                                         pix2 + y*i_pix2 + x, i_pix2 );
        }
    }

#define SSD1 { int d = pix1[y*i_pix1+x] - pix2[y*i_pix2+x]; i_ssd += d*d; }
    if( i_width & 7 )
    {
        for( y = 0; y < (i_height & ~7); y++ )
            for( int x = i_width & ~7; x < i_width; x++ )
                SSD1;
    }
    if( i_height & 7 )
    {
        for( y = i_height & ~7; y < i_height; y++ )
            for( int x = 0; x < i_width; x++ )
                SSD1;
    }
#undef SSD1

    return i_ssd;
}



void pixel_ssd_nv12_core( pixel *pixuv1, intptr_t stride1, pixel *pixuv2, intptr_t stride2,
                                 int width, int height, uint64_t *ssd_u, uint64_t *ssd_v )
{
    *ssd_u = 0, *ssd_v = 0;
    for( int y = 0; y < height; y++, pixuv1+=stride1, pixuv2+=stride2 )
        for( int x = 0; x < width; x++ )
        {
            int du = pixuv1[2*x]   - pixuv2[2*x];
            int dv = pixuv1[2*x+1] - pixuv2[2*x+1];
            *ssd_u += du*du;
            *ssd_v += dv*dv;
        }
}


void pixel_ssd_nv12_core_10b( pixel_10b *pixuv1, intptr_t stride1, pixel_10b *pixuv2, intptr_t stride2,
                                 int width, int height, uint64_t *ssd_u, uint64_t *ssd_v )
{
    *ssd_u = 0, *ssd_v = 0;
    for( int y = 0; y < height; y++, pixuv1+=stride1, pixuv2+=stride2 )
        for( int x = 0; x < width; x++ )
        {
            int du = pixuv1[2*x]   - pixuv2[2*x];
            int dv = pixuv1[2*x+1] - pixuv2[2*x+1];
            *ssd_u += du*du;
            *ssd_v += dv*dv;
        }
}

void pixel_ssd_nv12( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2,
                          int i_width, int i_height, uint64_t *ssd_u, uint64_t *ssd_v )
{
    pixel_ssd_nv12_core( pix1, i_pix1, pix2, i_pix2, i_width&~7, i_height, ssd_u, ssd_v );
    if( i_width&7 )
    {
        uint64_t tmp[2];
        pixel_ssd_nv12_core( pix1+(i_width&~7), i_pix1, pix2+(i_width&~7), i_pix2, i_width&7, i_height, &tmp[0], &tmp[1] );
        *ssd_u += tmp[0];
        *ssd_v += tmp[1];
    }
}



void pixel_ssd_nv12_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2,
                              int i_width, int i_height, uint64_t *ssd_u, uint64_t *ssd_v )
{
     pixel_ssd_nv12_core_10b( pix1, i_pix1, pix2, i_pix2, i_width&~7, i_height, ssd_u, ssd_v );
    if( i_width&7 )
    {
        uint64_t tmp[2];
        pixel_ssd_nv12_core_10b( pix1+(i_width&~7), i_pix1, pix2+(i_width&~7), i_pix2, i_width&7, i_height, &tmp[0], &tmp[1] );
        *ssd_u += tmp[0];
        *ssd_v += tmp[1];
    }
}



/****************************************************************************
 * pixel_var_wxh
 ****************************************************************************/
#define PIXEL_VAR_C( name, w, h ) \
uint64_t name( pixel *pix, intptr_t i_stride ) \
{                                             \
    uint32_t sum = 0, sqr = 0;                \
    for( int y = 0; y < h; y++ )              \
    {                                         \
        for( int x = 0; x < w; x++ )          \
        {                                     \
            sum += pix[x];                    \
            sqr += pix[x] * pix[x];           \
        }                                     \
        pix += i_stride;                      \
    }                                         \
    return sum + ((uint64_t)sqr << 32);       \
}

PIXEL_VAR_C( pixel_var_16x16, 16, 16 )
PIXEL_VAR_C( pixel_var_8x16,   8, 16 )
PIXEL_VAR_C( pixel_var_8x8,    8,  8 )




#define PIXEL_VAR_C_10B( name, w, h ) \
uint64_t name( pixel_10b *pix, intptr_t i_stride ) \
{                                             \
    uint32_t sum = 0, sqr = 0;                \
    for( int y = 0; y < h; y++ )              \
    {                                         \
        for( int x = 0; x < w; x++ )          \
        {                                     \
            sum += pix[x];                    \
            sqr += pix[x] * pix[x];           \
        }                                     \
        pix += i_stride;                      \
    }                                         \
    return sum + ((uint64_t)sqr << 32);       \
}

PIXEL_VAR_C_10B( pixel_var_16x16_10b, 16, 16 )
PIXEL_VAR_C_10B( pixel_var_8x16_10b,   8, 16 )
PIXEL_VAR_C_10B( pixel_var_8x8_10b,    8,  8 )


/****************************************************************************
 * pixel_var2_wxh
 ****************************************************************************/
#define PIXEL_VAR2_C( name, w, h, shift ) \
int name( pixel *pix1, intptr_t i_stride1, pixel *pix2, intptr_t i_stride2, int *ssd ) \
{ \
    int var = 0, sum = 0, sqr = 0; \
    for( int y = 0; y < h; y++ ) \
    { \
        for( int x = 0; x < w; x++ ) \
        { \
            int diff = pix1[x] - pix2[x]; \
            sum += diff; \
            sqr += diff * diff; \
        } \
        pix1 += i_stride1; \
        pix2 += i_stride2; \
    } \
    var = sqr - ((int64_t)sum * sum >> shift); \
    *ssd = sqr; \
    return var; \
}

PIXEL_VAR2_C( pixel_var2_8x16, 8, 16, 7 )
PIXEL_VAR2_C( pixel_var2_8x8,  8,  8, 6 )

#define PIXEL_VAR2_C_10B( name, w, h, shift ) \
int name( pixel_10b *pix1, intptr_t i_stride1, pixel_10b *pix2, intptr_t i_stride2, int *ssd ) \
{ \
    int var = 0, sum = 0, sqr = 0; \
    for( int y = 0; y < h; y++ ) \
    { \
        for( int x = 0; x < w; x++ ) \
        { \
            int diff = pix1[x] - pix2[x]; \
            sum += diff; \
            sqr += diff * diff; \
        } \
        pix1 += i_stride1; \
        pix2 += i_stride2; \
    } \
    var = sqr - ((int64_t)sum * sum >> shift); \
    *ssd = sqr; \
    return var; \
}

PIXEL_VAR2_C_10B( pixel_var2_8x16_10b, 8, 16, 7 )
PIXEL_VAR2_C_10B( pixel_var2_8x8_10b,  8,  8, 6 )



typedef uint32_t sum_t_10b;
typedef uint64_t sum2_t_10b;
typedef uint16_t sum_t;
typedef uint32_t sum2_t;

#define BITS_PER_SUM (8 * sizeof(sum_t))
#define BITS_PER_SUM_10B (8 * sizeof(sum_t_10b))

#define HADAMARD4(d0, d1, d2, d3, s0, s1, s2, s3) {\
    sum2_t t0 = s0 + s1;\
    sum2_t t1 = s0 - s1;\
    sum2_t t2 = s2 + s3;\
    sum2_t t3 = s2 - s3;\
    d0 = t0 + t2;\
    d2 = t0 - t2;\
    d1 = t1 + t3;\
    d3 = t1 - t3;\
}

// in: a pseudo-simd number of the form x+(y<<16)
// return: abs(x)+(abs(y)<<16)
static  sum2_t abs2( sum2_t a )
{
    sum2_t s = ((a>>(BITS_PER_SUM-1))&(((sum2_t)1<<BITS_PER_SUM)+1))*((sum_t)-1);
    return (a+s)^s;
}

static  sum2_t_10b abs2_10b( sum2_t_10b a )
{
    sum2_t_10b s = ((a>>(BITS_PER_SUM-1))&(((sum2_t_10b)1<<BITS_PER_SUM)+1))*((sum_t_10b)-1);
    return (a+s)^s;
}

/****************************************************************************
 * pixel_satd_WxH: sum of 4x4 Hadamard transformed differences
 ****************************************************************************/

int pixel_satd_4x4( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 )
{
    sum2_t tmp[4][2];
    sum2_t a0, a1, a2, a3, b0, b1;
    sum2_t sum = 0;
    for( int i = 0; i < 4; i++, pix1 += i_pix1, pix2 += i_pix2 )
    {
        a0 = pix1[0] - pix2[0];
        a1 = pix1[1] - pix2[1];
        b0 = (a0+a1) + ((a0-a1)<<BITS_PER_SUM);
        a2 = pix1[2] - pix2[2];
        a3 = pix1[3] - pix2[3];
        b1 = (a2+a3) + ((a2-a3)<<BITS_PER_SUM);
        tmp[i][0] = b0 + b1;
        tmp[i][1] = b0 - b1;
    }
    for( int i = 0; i < 2; i++ )
    {
        HADAMARD4( a0, a1, a2, a3, tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i] );
        a0 = abs2(a0) + abs2(a1) + abs2(a2) + abs2(a3);
        sum += ((sum_t)a0) + (a0>>BITS_PER_SUM);
    }
    return sum >> 1;
}

int pixel_satd_4x4_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 )
{
    sum2_t_10b tmp[4][2];
    sum2_t_10b a0, a1, a2, a3, b0, b1;
    sum2_t_10b sum = 0;
    for( int i = 0; i < 4; i++, pix1 += i_pix1, pix2 += i_pix2 )
    {
        a0 = pix1[0] - pix2[0];
        a1 = pix1[1] - pix2[1];
        b0 = (a0+a1) + ((a0-a1)<<BITS_PER_SUM);
        a2 = pix1[2] - pix2[2];
        a3 = pix1[3] - pix2[3];
        b1 = (a2+a3) + ((a2-a3)<<BITS_PER_SUM);
        tmp[i][0] = b0 + b1;
        tmp[i][1] = b0 - b1;
    }
    for( int i = 0; i < 2; i++ )
    {
        HADAMARD4( a0, a1, a2, a3, tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i] );
        a0 = abs2_10b(a0) + abs2_10b(a1) + abs2_10b(a2) + abs2_10b(a3);
        sum += ((sum_t)a0) + (a0>>BITS_PER_SUM);
    }
    return sum >> 1;
}

int pixel_satd_8x4( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 )
{
    sum2_t tmp[4][4];
    sum2_t a0, a1, a2, a3;
    sum2_t sum = 0;
    for( int i = 0; i < 4; i++, pix1 += i_pix1, pix2 += i_pix2 )
    {
        a0 = (pix1[0] - pix2[0]) + ((sum2_t)(pix1[4] - pix2[4]) << BITS_PER_SUM);
        a1 = (pix1[1] - pix2[1]) + ((sum2_t)(pix1[5] - pix2[5]) << BITS_PER_SUM);
        a2 = (pix1[2] - pix2[2]) + ((sum2_t)(pix1[6] - pix2[6]) << BITS_PER_SUM);
        a3 = (pix1[3] - pix2[3]) + ((sum2_t)(pix1[7] - pix2[7]) << BITS_PER_SUM);
        HADAMARD4( tmp[i][0], tmp[i][1], tmp[i][2], tmp[i][3], a0,a1,a2,a3 );
    }
    for( int i = 0; i < 4; i++ )
    {
        HADAMARD4( a0, a1, a2, a3, tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i] );
        sum += abs2(a0) + abs2(a1) + abs2(a2) + abs2(a3);
    }
    return (((sum_t)sum) + (sum>>BITS_PER_SUM)) >> 1;
}

int pixel_satd_8x4_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 )
{
    sum2_t_10b tmp[4][4];
    sum2_t_10b a0, a1, a2, a3;
    sum2_t_10b sum = 0;
    for( int i = 0; i < 4; i++, pix1 += i_pix1, pix2 += i_pix2 )
    {
        a0 = (pix1[0] - pix2[0]) + ((sum2_t)(pix1[4] - pix2[4]) << BITS_PER_SUM);
        a1 = (pix1[1] - pix2[1]) + ((sum2_t)(pix1[5] - pix2[5]) << BITS_PER_SUM);
        a2 = (pix1[2] - pix2[2]) + ((sum2_t)(pix1[6] - pix2[6]) << BITS_PER_SUM);
        a3 = (pix1[3] - pix2[3]) + ((sum2_t)(pix1[7] - pix2[7]) << BITS_PER_SUM);
        HADAMARD4( tmp[i][0], tmp[i][1], tmp[i][2], tmp[i][3], a0,a1,a2,a3 );
    }
    for( int i = 0; i < 4; i++ )
    {
        HADAMARD4( a0, a1, a2, a3, tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i] );
        sum += abs2_10b(a0) + abs2_10b(a1) + abs2_10b(a2) + abs2_10b(a3);
    }
    return (((sum_t)sum) + (sum>>BITS_PER_SUM)) >> 1;
}




#define PIXEL_SATD_C( w, h, sub )\
int pixel_satd_##w##x##h( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 )\
{\
    int sum = sub( pix1, i_pix1, pix2, i_pix2 )\
            + sub( pix1+4*i_pix1, i_pix1, pix2+4*i_pix2, i_pix2 );\
    if( w==16 )\
        sum+= sub( pix1+8, i_pix1, pix2+8, i_pix2 )\
            + sub( pix1+8+4*i_pix1, i_pix1, pix2+8+4*i_pix2, i_pix2 );\
    if( h==16 )\
        sum+= sub( pix1+8*i_pix1, i_pix1, pix2+8*i_pix2, i_pix2 )\
            + sub( pix1+12*i_pix1, i_pix1, pix2+12*i_pix2, i_pix2 );\
    if( w==16 && h==16 )\
        sum+= sub( pix1+8+8*i_pix1, i_pix1, pix2+8+8*i_pix2, i_pix2 )\
            + sub( pix1+8+12*i_pix1, i_pix1, pix2+8+12*i_pix2, i_pix2 );\
    return sum;\
}
PIXEL_SATD_C( 16, 16, pixel_satd_8x4 )
PIXEL_SATD_C( 16, 8,  pixel_satd_8x4 )
PIXEL_SATD_C( 8,  16, pixel_satd_8x4 )
PIXEL_SATD_C( 8,  8,  pixel_satd_8x4 )
PIXEL_SATD_C( 4,  16, pixel_satd_4x4 )
PIXEL_SATD_C( 4,  8,  pixel_satd_4x4 )


#define PIXEL_SATD_C_10B( w, h, sub )\
int pixel_satd_##w##x##h##_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 )\
{\
    int sum = sub( pix1, i_pix1, pix2, i_pix2 )\
            + sub( pix1+4*i_pix1, i_pix1, pix2+4*i_pix2, i_pix2 );\
    if( w==16 )\
        sum+= sub( pix1+8, i_pix1, pix2+8, i_pix2 )\
            + sub( pix1+8+4*i_pix1, i_pix1, pix2+8+4*i_pix2, i_pix2 );\
    if( h==16 )\
        sum+= sub( pix1+8*i_pix1, i_pix1, pix2+8*i_pix2, i_pix2 )\
            + sub( pix1+12*i_pix1, i_pix1, pix2+12*i_pix2, i_pix2 );\
    if( w==16 && h==16 )\
        sum+= sub( pix1+8+8*i_pix1, i_pix1, pix2+8+8*i_pix2, i_pix2 )\
            + sub( pix1+8+12*i_pix1, i_pix1, pix2+8+12*i_pix2, i_pix2 );\
    return sum;\
}
PIXEL_SATD_C_10B( 16, 16, pixel_satd_8x4_10b )
PIXEL_SATD_C_10B( 16, 8,  pixel_satd_8x4_10b )
PIXEL_SATD_C_10B( 8,  16, pixel_satd_8x4_10b )
PIXEL_SATD_C_10B( 8,  8,  pixel_satd_8x4_10b )
PIXEL_SATD_C_10B( 4,  16, pixel_satd_4x4_10b )
PIXEL_SATD_C_10B( 4,  8,  pixel_satd_4x4_10b )

/****************************************************************************
 * pixel_satd_x4
 * no faster than single satd, but needed for satd to be a drop-in replacement for sad
 ****************************************************************************/

#define SATD_X( size, cpu, prefix ) \
void prefix##pixel_satd_x3_##size##cpu( pixel *fenc, pixel *pix0, pixel *pix1, pixel *pix2,\
                                            intptr_t i_stride, int scores[3] )\
{\
    scores[0] = prefix##pixel_satd_##size##cpu( fenc, FENC_STRIDE, pix0, i_stride  );\
    scores[1] = prefix##pixel_satd_##size##cpu( fenc, FENC_STRIDE, pix1, i_stride  );\
    scores[2] = prefix##pixel_satd_##size##cpu( fenc, FENC_STRIDE, pix2, i_stride  );\
}\
void prefix##pixel_satd_x4_##size##cpu( pixel *fenc, pixel *pix0, pixel *pix1, pixel *pix2, pixel *pix3,\
                                            intptr_t i_stride, int scores[4] )\
{\
    scores[0] = prefix##pixel_satd_##size##cpu( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = prefix##pixel_satd_##size##cpu( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = prefix##pixel_satd_##size##cpu( fenc, FENC_STRIDE, pix2, i_stride );\
    scores[3] = prefix##pixel_satd_##size##cpu( fenc, FENC_STRIDE, pix3, i_stride );\
}

#define SATD_X_DECL6( cpu, prefix )\
SATD_X( 16x16, cpu, prefix )\
SATD_X( 16x8, cpu, prefix )\
SATD_X( 8x16, cpu, prefix )\
SATD_X( 8x8, cpu, prefix )\
SATD_X( 8x4, cpu, prefix )\
SATD_X( 4x8, cpu, prefix )

#define SATD_X_DECL7( cpu, prefix )\
SATD_X_DECL6( cpu, prefix )\
SATD_X( 4x4, cpu, prefix )

SATD_X_DECL7(,)

#if HAVE_MMX
SATD_X_DECL7( _mmx2, asm_ )
#if !HIGH_BIT_DEPTH
SATD_X_DECL6( _sse2, asm_ )
SATD_X_DECL7( _ssse3, asm_ )
SATD_X_DECL6( _ssse3_atom, asm_ )
SATD_X_DECL7( _sse4, asm_ )
SATD_X_DECL7( _avx, asm_ )
SATD_X_DECL7( _xop, asm_ )
#endif // !HIGH_BIT_DEPTH
#endif




#define SATD_X_10B( size, cpu ) \
void pixel_satd_x3_10B_##size##cpu( pixel_10b *fenc, pixel_10b *pix0, pixel_10b *pix1, pixel_10b *pix2,\
                                            intptr_t i_stride, int scores[3] )\
{\
    scores[0] = pixel_satd_##size##cpu##_10b( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = pixel_satd_##size##cpu##_10b( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = pixel_satd_##size##cpu##_10b( fenc, FENC_STRIDE, pix2, i_stride );\
}\
void pixel_satd_x4_10B_##size##cpu( pixel_10b *fenc, pixel_10b *pix0, pixel_10b *pix1, pixel_10b *pix2, pixel_10b *pix3,\
                                            intptr_t i_stride, int scores[4] )\
{\
    scores[0] = pixel_satd_##size##cpu##_10b( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = pixel_satd_##size##cpu##_10b( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = pixel_satd_##size##cpu##_10b( fenc, FENC_STRIDE, pix2, i_stride );\
    scores[3] = pixel_satd_##size##cpu##_10b( fenc, FENC_STRIDE, pix3, i_stride );\
}
#define SATD_X_DECL6_10B( cpu )\
SATD_X_10B( 16x16, cpu )\
SATD_X_10B( 16x8, cpu )\
SATD_X_10B( 8x16, cpu )\
SATD_X_10B( 8x8, cpu )\
SATD_X_10B( 8x4, cpu )\
SATD_X_10B( 4x8, cpu )
#define SATD_X_DECL7_10B( cpu )\
SATD_X_DECL6_10B( cpu )\
SATD_X_10B( 4x4, cpu )

SATD_X_DECL7_10B()





int sa8d_8x8( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 )
{
    sum2_t tmp[8][4];
    sum2_t a0, a1, a2, a3, a4, a5, a6, a7, b0, b1, b2, b3;
    sum2_t sum = 0;
    for( int i = 0; i < 8; i++, pix1 += i_pix1, pix2 += i_pix2 )
    {
        a0 = pix1[0] - pix2[0];
        a1 = pix1[1] - pix2[1];
        b0 = (a0+a1) + ((a0-a1)<<BITS_PER_SUM);
        a2 = pix1[2] - pix2[2];
        a3 = pix1[3] - pix2[3];
        b1 = (a2+a3) + ((a2-a3)<<BITS_PER_SUM);
        a4 = pix1[4] - pix2[4];
        a5 = pix1[5] - pix2[5];
        b2 = (a4+a5) + ((a4-a5)<<BITS_PER_SUM);
        a6 = pix1[6] - pix2[6];
        a7 = pix1[7] - pix2[7];
        b3 = (a6+a7) + ((a6-a7)<<BITS_PER_SUM);
        HADAMARD4( tmp[i][0], tmp[i][1], tmp[i][2], tmp[i][3], b0,b1,b2,b3 );
    }
    for( int i = 0; i < 4; i++ )
    {
        HADAMARD4( a0, a1, a2, a3, tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i] );
        HADAMARD4( a4, a5, a6, a7, tmp[4][i], tmp[5][i], tmp[6][i], tmp[7][i] );
        b0  = abs2(a0+a4) + abs2(a0-a4);
        b0 += abs2(a1+a5) + abs2(a1-a5);
        b0 += abs2(a2+a6) + abs2(a2-a6);
        b0 += abs2(a3+a7) + abs2(a3-a7);
        sum += (sum_t)b0 + (b0>>BITS_PER_SUM);
    }
    return sum;
}


int sa8d_8x8_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 )
{
    sum2_t tmp[8][4];
    sum2_t a0, a1, a2, a3, a4, a5, a6, a7, b0, b1, b2, b3;
    sum2_t sum = 0;
    for( int i = 0; i < 8; i++, pix1 += i_pix1, pix2 += i_pix2 )
    {
        a0 = pix1[0] - pix2[0];
        a1 = pix1[1] - pix2[1];
        b0 = (a0+a1) + ((a0-a1)<<BITS_PER_SUM);
        a2 = pix1[2] - pix2[2];
        a3 = pix1[3] - pix2[3];
        b1 = (a2+a3) + ((a2-a3)<<BITS_PER_SUM);
        a4 = pix1[4] - pix2[4];
        a5 = pix1[5] - pix2[5];
        b2 = (a4+a5) + ((a4-a5)<<BITS_PER_SUM);
        a6 = pix1[6] - pix2[6];
        a7 = pix1[7] - pix2[7];
        b3 = (a6+a7) + ((a6-a7)<<BITS_PER_SUM);
        HADAMARD4( tmp[i][0], tmp[i][1], tmp[i][2], tmp[i][3], b0,b1,b2,b3 );
    }
    for( int i = 0; i < 4; i++ )
    {
        HADAMARD4( a0, a1, a2, a3, tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i] );
        HADAMARD4( a4, a5, a6, a7, tmp[4][i], tmp[5][i], tmp[6][i], tmp[7][i] );
        b0  = abs2(a0+a4) + abs2(a0-a4);
        b0 += abs2(a1+a5) + abs2(a1-a5);
        b0 += abs2(a2+a6) + abs2(a2-a6);
        b0 += abs2(a3+a7) + abs2(a3-a7);
        sum += (sum_t)b0 + (b0>>BITS_PER_SUM);
    }
    return sum;
}

int pixel_sa8d_8x8( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 )
{
    int sum = sa8d_8x8( pix1, i_pix1, pix2, i_pix2 );
    return (sum+2)>>2;
}

int pixel_sa8d_8x8_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 )
{
    int sum = sa8d_8x8_10b( pix1, i_pix1, pix2, i_pix2 );
    return (sum+2)>>2;
}




int pixel_sa8d_16x16( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 )
{
    int sum = sa8d_8x8( pix1, i_pix1, pix2, i_pix2 )
            + sa8d_8x8( pix1+8, i_pix1, pix2+8, i_pix2 )
            + sa8d_8x8( pix1+8*i_pix1, i_pix1, pix2+8*i_pix2, i_pix2 )
            + sa8d_8x8( pix1+8+8*i_pix1, i_pix1, pix2+8+8*i_pix2, i_pix2 );
    return (sum+2)>>2;
}



int pixel_sa8d_16x16_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 )
{
    int sum = sa8d_8x8_10b( pix1, i_pix1, pix2, i_pix2 )
            + sa8d_8x8_10b( pix1+8, i_pix1, pix2+8, i_pix2 )
            + sa8d_8x8_10b( pix1+8*i_pix1, i_pix1, pix2+8*i_pix2, i_pix2 )
            + sa8d_8x8_10b( pix1+8+8*i_pix1, i_pix1, pix2+8+8*i_pix2, i_pix2 );
    return (sum+2)>>2;
}






////////////////////////////////////////////////////////////////////////////

uint64_t pixel_hadamard_ac( pixel *pix, intptr_t stride )
{
    sum2_t tmp[32];
    sum2_t a0, a1, a2, a3, dc;
    sum2_t sum4 = 0, sum8 = 0;
    for( int i = 0; i < 8; i++, pix+=stride )
    {
        sum2_t *t = tmp + (i&3) + (i&4)*4;
        a0 = (pix[0]+pix[1]) + ((sum2_t)(pix[0]-pix[1])<<BITS_PER_SUM);
        a1 = (pix[2]+pix[3]) + ((sum2_t)(pix[2]-pix[3])<<BITS_PER_SUM);
        t[0] = a0 + a1;
        t[4] = a0 - a1;
        a2 = (pix[4]+pix[5]) + ((sum2_t)(pix[4]-pix[5])<<BITS_PER_SUM);
        a3 = (pix[6]+pix[7]) + ((sum2_t)(pix[6]-pix[7])<<BITS_PER_SUM);
        t[8] = a2 + a3;
        t[12] = a2 - a3;
    }
    for( int i = 0; i < 8; i++ )
    {
        HADAMARD4( a0, a1, a2, a3, tmp[i*4+0], tmp[i*4+1], tmp[i*4+2], tmp[i*4+3] );
        tmp[i*4+0] = a0;
        tmp[i*4+1] = a1;
        tmp[i*4+2] = a2;
        tmp[i*4+3] = a3;
        sum4 += abs2(a0) + abs2(a1) + abs2(a2) + abs2(a3);
    }
    for( int i = 0; i < 8; i++ )
    {
        HADAMARD4( a0,a1,a2,a3, tmp[i], tmp[8+i], tmp[16+i], tmp[24+i] );
        sum8 += abs2(a0) + abs2(a1) + abs2(a2) + abs2(a3);
    }
    dc = (sum_t)(tmp[0] + tmp[8] + tmp[16] + tmp[24]);
    sum4 = (sum_t)sum4 + (sum4>>BITS_PER_SUM) - dc;
    sum8 = (sum_t)sum8 + (sum8>>BITS_PER_SUM) - dc;
    return ((uint64_t)sum8<<32) + sum4;
}

#define HADAMARD_AC(w,h) \
uint64_t pixel_hadamard_ac_##w##x##h( pixel *pix, intptr_t stride )\
{\
    uint64_t sum = pixel_hadamard_ac( pix, stride );\
    if( w==16 )\
        sum += pixel_hadamard_ac( pix+8, stride );\
    if( h==16 )\
        sum += pixel_hadamard_ac( pix+8*stride, stride );\
    if( w==16 && h==16 )\
        sum += pixel_hadamard_ac( pix+8*stride+8, stride );\
    return ((sum>>34)<<32) + ((uint32_t)sum>>1);\
}
HADAMARD_AC( 16, 16 )
HADAMARD_AC( 16, 8 )
HADAMARD_AC( 8, 16 )
HADAMARD_AC( 8, 8 )


uint64_t pixel_hadamard_ac_10b( pixel_10b *pix, intptr_t stride )
{
    sum2_t tmp[32];
    sum2_t a0, a1, a2, a3, dc;
    sum2_t sum4 = 0, sum8 = 0;
    for( int i = 0; i < 8; i++, pix+=stride )
    {
        sum2_t *t = tmp + (i&3) + (i&4)*4;
        a0 = (pix[0]+pix[1]) + ((sum2_t_10b)(pix[0]-pix[1])<<BITS_PER_SUM);
        a1 = (pix[2]+pix[3]) + ((sum2_t_10b)(pix[2]-pix[3])<<BITS_PER_SUM);
        t[0] = a0 + a1;
        t[4] = a0 - a1;
        a2 = (pix[4]+pix[5]) + ((sum2_t_10b)(pix[4]-pix[5])<<BITS_PER_SUM);
        a3 = (pix[6]+pix[7]) + ((sum2_t_10b)(pix[6]-pix[7])<<BITS_PER_SUM);
        t[8] = a2 + a3;
        t[12] = a2 - a3;
    }
    for( int i = 0; i < 8; i++ )
    {
        HADAMARD4( a0, a1, a2, a3, tmp[i*4+0], tmp[i*4+1], tmp[i*4+2], tmp[i*4+3] );
        tmp[i*4+0] = a0;
        tmp[i*4+1] = a1;
        tmp[i*4+2] = a2;
        tmp[i*4+3] = a3;
        sum4 += abs2(a0) + abs2(a1) + abs2(a2) + abs2(a3);
    }
    for( int i = 0; i < 8; i++ )
    {
        HADAMARD4( a0,a1,a2,a3, tmp[i], tmp[8+i], tmp[16+i], tmp[24+i] );
        sum8 += abs2(a0) + abs2(a1) + abs2(a2) + abs2(a3);
    }
    dc = (sum_t_10b)(tmp[0] + tmp[8] + tmp[16] + tmp[24]);
    sum4 = (sum_t_10b)sum4 + (sum4>>BITS_PER_SUM) - dc;
    sum8 = (sum_t_10b)sum8 + (sum8>>BITS_PER_SUM) - dc;
    return ((uint64_t)sum8<<32) + sum4;
}

#define HADAMARD_AC_10B(w,h) \
uint64_t pixel_hadamard_ac_10b_##w##x##h( pixel_10b *pix, intptr_t stride )\
{\
    uint64_t sum = pixel_hadamard_ac_10b( pix, stride );\
    if( w==16 )\
        sum += pixel_hadamard_ac_10b( pix+8, stride );\
    if( h==16 )\
        sum += pixel_hadamard_ac_10b( pix+8*stride, stride );\
    if( w==16 && h==16 )\
        sum += pixel_hadamard_ac_10b( pix+8*stride+8, stride );\
    return ((sum>>34)<<32) + ((uint32_t)sum>>1);\
}
HADAMARD_AC_10B( 16, 16 )
HADAMARD_AC_10B( 16, 8 )
HADAMARD_AC_10B( 8, 16 )
HADAMARD_AC_10B( 8, 8 )

/////////////////////////////////////////////////////////////////////////////////
#define SAD_X( size ) \
void vbench_pixel_sad_x3_##size( pixel *fenc, pixel *pix0, pixel *pix1, pixel *pix2,\
                                      intptr_t i_stride, int scores[3] )\
{\
    scores[0] = pixel_sad_##size( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = pixel_sad_##size( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = pixel_sad_##size( fenc, FENC_STRIDE, pix2, i_stride );\
}\
void vbench_pixel_sad_x4_##size( pixel *fenc, pixel *pix0, pixel *pix1,pixel *pix2, pixel *pix3,\
                                      intptr_t i_stride, int scores[4] )\
{\
    scores[0] = pixel_sad_##size( fenc, FENC_STRIDE, pix0, i_stride );\
    scores[1] = pixel_sad_##size( fenc, FENC_STRIDE, pix1, i_stride );\
    scores[2] = pixel_sad_##size( fenc, FENC_STRIDE, pix2, i_stride );\
    scores[3] = pixel_sad_##size( fenc, FENC_STRIDE, pix3, i_stride );\
}

SAD_X( 16x16 )
SAD_X( 16x8 )
SAD_X( 8x16 )
SAD_X( 8x8 )
SAD_X( 8x4 )
SAD_X( 4x8 )
SAD_X( 4x4 )


// FIXME: merge these twos: must rename all pixel funcs
#define INTRA_MBCMP_8x8_C( mbcmp, cpu, cpu2, prefix )\
void intra_##mbcmp##_x3_8x8##cpu( pixel *fenc, pixel edge[36], int res[3] )\
{\
    ALIGNED_ARRAY_16( pixel, pix, [8*FDEC_STRIDE] );\
    prefix##predict_8x8_v##cpu2( pix, edge );\
    res[0] = pixel_##mbcmp##_8x8##cpu( pix, FDEC_STRIDE, fenc, FENC_STRIDE );\
    prefix##predict_8x8_h##cpu2( pix, edge );\
    res[1] = pixel_##mbcmp##_8x8##cpu( pix, FDEC_STRIDE, fenc, FENC_STRIDE );\
    prefix##predict_8x8_dc##cpu2( pix, edge );\
    res[2] = pixel_##mbcmp##_8x8##cpu( pix, FDEC_STRIDE, fenc, FENC_STRIDE );\
}



#define INTRA_MBCMP_8x8( mbcmp, cpu, cpu2, prefix )\
void intra_##mbcmp##_x3_8x8##cpu( pixel *fenc, pixel edge[36], int res[3] )\
{\
    ALIGNED_ARRAY_16( pixel, pix, [8*FDEC_STRIDE] );\
    prefix##predict_8x8_v##cpu2( pix, edge );\
    res[0] = prefix##pixel_##mbcmp##_8x8##cpu( pix, FDEC_STRIDE, fenc, FENC_STRIDE );\
    prefix##predict_8x8_h##cpu2( pix, edge );\
    res[1] = prefix##pixel_##mbcmp##_8x8##cpu( pix, FDEC_STRIDE, fenc, FENC_STRIDE );\
    prefix##predict_8x8_dc##cpu2( pix, edge );\
    res[2] = prefix##pixel_##mbcmp##_8x8##cpu( pix, FDEC_STRIDE, fenc, FENC_STRIDE );\
}



INTRA_MBCMP_8x8_C( sad,, _c, vbench_)
INTRA_MBCMP_8x8_C(sa8d,, _c, vbench_)
#if HIGH_BIT_DEPTH && HAVE_MMX
#define predict_8x8_v_sse2 predict_8x8_v_sse
INTRA_MBCMP_8x8( sad, _mmx2,  _c, asm_)
INTRA_MBCMP_8x8(sa8d, _sse2,  _sse2, asm_)
#endif
#if !HIGH_BIT_DEPTH && (HAVE_ARMV6 || ARCH_AARCH64)
INTRA_MBCMP_8x8( sad, _neon, _neon )
INTRA_MBCMP_8x8(sa8d, _neon, _neon )
#endif

#define INTRA_MBCMP_C( mbcmp, size, pred1, pred2, pred3, chroma, cpu, cpu2, prefix )\
void intra_##mbcmp##_x3_##size##chroma##cpu( pixel *fenc, pixel *fdec, int res[3] )\
{\
    prefix##predict_##size##chroma##_##pred1##cpu2( fdec );\
    res[0] = pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
    prefix##predict_##size##chroma##_##pred2##cpu2( fdec );\
    res[1] = pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
    prefix##predict_##size##chroma##_##pred3##cpu2( fdec );\
    res[2] = pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
}

INTRA_MBCMP_C( sad,  4x4,   v, h, dc,  ,, _c, vbench_ )
INTRA_MBCMP_C(satd,  4x4,   v, h, dc,  ,, _c, vbench_ )
INTRA_MBCMP_C( sad,  8x8,  dc, h,  v, c,, _c, vbench_ )
INTRA_MBCMP_C(satd,  8x8,  dc, h,  v, c,, _c, vbench_ )
INTRA_MBCMP_C( sad,  8x16, dc, h,  v, c,, _c, vbench_ )
INTRA_MBCMP_C(satd,  8x16, dc, h,  v, c,, _c, vbench_ )
INTRA_MBCMP_C( sad, 16x16,  v, h, dc,  ,, _c, vbench_ )
INTRA_MBCMP_C(satd, 16x16,  v, h, dc,  ,, _c, vbench_ )


// FIXME: merge these two
#define INTRA_MBCMP_C( mbcmp, size, pred1, pred2, pred3, chroma, cpu, cpu2 )\
void intra_##mbcmp##_x3_##size##chroma##cpu( pixel *fenc, pixel *fdec, int res[3] )\
{\
    vbench_predict_##size##chroma##_##pred1##cpu2( fdec );\
    res[0] = pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
    vbench_predict_##size##chroma##_##pred2##cpu2( fdec );\
    res[1] = pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
    vbench_predict_##size##chroma##_##pred3##cpu2( fdec );\
    res[2] = pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
}

#define INTRA_MBCMP( mbcmp, size, pred1, pred2, pred3, chroma, cpu, cpu2, prefix)\
void asm_intra_##mbcmp##_x3_##size##chroma##cpu( pixel *fenc, pixel *fdec, int res[3] )\
{\
    prefix##predict_##size##chroma##_##pred1##cpu2( fdec );\
    res[0] = prefix##pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
    prefix##predict_##size##chroma##_##pred2##cpu2( fdec );\
    res[1] = prefix##pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
    prefix##predict_##size##chroma##_##pred3##cpu2( fdec );\
    res[2] = prefix##pixel_##mbcmp##_##size##cpu( fdec, FDEC_STRIDE, fenc, FENC_STRIDE );\
}




#if HAVE_MMX
#if HIGH_BIT_DEPTH
#define predict_8x8c_v_mmx2 x264_predict_8x8c_v_mmx
#define predict_8x16c_v_mmx2 x264_predict_8x16c_v_c
#define predict_8x8c_v_sse2 x264_predict_8x8c_v_sse
#define predict_8x16c_v_sse2 x264_predict_8x16c_v_sse
#define predict_16x16_v_sse2 x264_predict_16x16_v_sse
INTRA_MBCMP( sad,  4x4,   v, h, dc,  , _mmx2, _c, asm_ )
INTRA_MBCMP( sad,  8x8,  dc, h,  v, c, _mmx2, _mmx2, asm_ )
INTRA_MBCMP( sad,  8x16, dc, h,  v, c, _mmx2, _mmx2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _mmx2, _mmx2, asm_ )
INTRA_MBCMP( sad, 16x16,  v, h, dc,  , _mmx2, _mmx2, asm_ )
INTRA_MBCMP( sad,  8x8,  dc, h,  v, c, _sse2, _sse2, asm_ )
INTRA_MBCMP( sad,  8x16, dc, h,  v, c, _sse2, _sse2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _sse2, _sse2, asm_ )
INTRA_MBCMP( sad, 16x16,  v, h, dc,  , _sse2, _sse2, asm_ )
INTRA_MBCMP( sad,  8x8,  dc, h,  v, c, _ssse3, _sse2, asm_ )
INTRA_MBCMP( sad,  8x16, dc, h,  v, c, _ssse3, _sse2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _ssse3, _sse2, asm_ )
INTRA_MBCMP( sad, 16x16,  v, h, dc,  , _ssse3, _sse2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _sse4, _sse2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _avx, _sse2, asm_ )
#else
#define asm_predict_8x16c_v_mmx2 asm_predict_8x16c_v_mmx
INTRA_MBCMP( sad,  8x16, dc, h,  v, c, _mmx2, _mmx2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _mmx2, _mmx2, asm_ )
INTRA_MBCMP( sad,  8x16, dc, h,  v, c, _sse2, _mmx2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _sse2, _mmx2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _ssse3, _mmx2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _sse4, _mmx2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _avx, _mmx2, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _xop, _mmx2, asm_ )
#endif
#endif
#if !HIGH_BIT_DEPTH && HAVE_ARMV6
INTRA_MBCMP( sad,  4x4,   v, h, dc,  , _neon, _armv6, asm_ )
INTRA_MBCMP(satd,  4x4,   v, h, dc,  , _neon, _armv6, asm_ )
INTRA_MBCMP( sad,  8x8,  dc, h,  v, c, _neon, _neon, asm_ )
INTRA_MBCMP(satd,  8x8,  dc, h,  v, c, _neon, _neon, asm_ )
INTRA_MBCMP( sad,  8x16, dc, h,  v, c, _neon, _c, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _neon, _c, asm_ )
INTRA_MBCMP( sad, 16x16,  v, h, dc,  , _neon, _neon, asm_ )
INTRA_MBCMP(satd, 16x16,  v, h, dc,  , _neon, _neon, asm_ )
#endif
#if !HIGH_BIT_DEPTH && ARCH_AARCH64
INTRA_MBCMP( sad,  4x4,   v, h, dc,  , _neon, _neon, asm_ )
INTRA_MBCMP(satd,  4x4,   v, h, dc,  , _neon, _neon, asm_ )
INTRA_MBCMP( sad,  8x8,  dc, h,  v, c, _neon, _neon, asm_ )
INTRA_MBCMP(satd,  8x8,  dc, h,  v, c, _neon, _neon, asm_ )
INTRA_MBCMP( sad,  8x16, dc, h,  v, c, _neon, _neon, asm_ )
INTRA_MBCMP(satd,  8x16, dc, h,  v, c, _neon, _neon, asm_ )
INTRA_MBCMP( sad, 16x16,  v, h, dc,  , _neon, _neon, asm_ )
INTRA_MBCMP(satd, 16x16,  v, h, dc,  , _neon, _neon, asm_ )
#endif


/****************************************************************************
 * structural similarity metric
 ****************************************************************************/
void ssim_4x4x2_core( const pixel *pix1, intptr_t stride1,
                             const pixel *pix2, intptr_t stride2,
                             int sums[2][4] )
{
    for( int z = 0; z < 2; z++ )
    {
        uint32_t s1 = 0, s2 = 0, ss = 0, s12 = 0;
        for( int y = 0; y < 4; y++ )
            for( int x = 0; x < 4; x++ )
            {
                int a = pix1[x+y*stride1];
                int b = pix2[x+y*stride2];
                s1  += a;
                s2  += b;
                ss  += a*a;
                ss  += b*b;
                s12 += a*b;
            }
        sums[z][0] = s1;
        sums[z][1] = s2;
        sums[z][2] = ss;
        sums[z][3] = s12;
        pix1 += 4;
        pix2 += 4;
    }
}

void ssim_4x4x2_core_10b( const pixel_10b *pix1, intptr_t stride1,
                          const pixel_10b *pix2, intptr_t stride2,
                             int sums[2][4] )
{
    for( int z = 0; z < 2; z++ )
    {
        uint32_t s1 = 0, s2 = 0, ss = 0, s12 = 0;
        for( int y = 0; y < 4; y++ )
            for( int x = 0; x < 4; x++ )
            {
                int a = pix1[x+y*stride1];
                int b = pix2[x+y*stride2];
                s1  += a;
                s2  += b;
                ss  += a*a;
                ss  += b*b;
                s12 += a*b;
            }
        sums[z][0] = s1;
        sums[z][1] = s2;
        sums[z][2] = ss;
        sums[z][3] = s12;
        pix1 += 4;
        pix2 += 4;
    }
}

float ssim_end1( int s1, int s2, int ss, int s12 )
{
/* Maximum value for 10-bit is: ss*64 = (2^10-1)^2*16*4*64 = 4286582784, which will overflow in some cases.
 * s1*s1, s2*s2, and s1*s2 also obtain this value for edge cases: ((2^10-1)*16*4)^2 = 4286582784.
 * Maximum value for 9-bit is: ss*64 = (2^9-1)^2*16*4*64 = 1069551616, which will not overflow. */
#define type int
#undef PIXEL_MAX
#define PIXEL_MAX  ((1<<8)-1)
    static const int ssim_c1 = (int)(.01*.01*PIXEL_MAX*PIXEL_MAX*64 + .5);
    static const int ssim_c2 = (int)(.03*.03*PIXEL_MAX*PIXEL_MAX*64*63 + .5);

    type fs1 = s1;
    type fs2 = s2;
    type fss = ss;
    type fs12 = s12;
    type vars = fss*64 - fs1*fs1 - fs2*fs2;
    type covar = fs12*64 - fs1*fs2;
    return (float)(2*fs1*fs2 + ssim_c1) * (float)(2*covar + ssim_c2)
         / ((float)(fs1*fs1 + fs2*fs2 + ssim_c1) * (float)(vars + ssim_c2));
#undef PIXEL_MAX
#undef type
}



static float ssim_end1_10b( int s1, int s2, int ss, int s12 )
{
/* Maximum value for 10-bit is: ss*64 = (2^10-1)^2*16*4*64 = 4286582784, which will overflow in some cases.
 * s1*s1, s2*s2, and s1*s2 also obtain this value for edge cases: ((2^10-1)*16*4)^2 = 4286582784.
 * Maximum value for 9-bit is: ss*64 = (2^9-1)^2*16*4*64 = 1069551616, which will not overflow. */
#define type float
#undef PIXEL_MAX
#define PIXEL_MAX  ((1<<10)-1)
    static const float ssim_c1 = .01*.01*PIXEL_MAX*PIXEL_MAX*64;
    static const float ssim_c2 = .03*.03*PIXEL_MAX*PIXEL_MAX*64*63;
    type fs1 = s1;
    type fs2 = s2;
    type fss = ss;
    type fs12 = s12;
    type vars = fss*64 - fs1*fs1 - fs2*fs2;
    type covar = fs12*64 - fs1*fs2;
    return (float)(2*fs1*fs2 + ssim_c1) * (float)(2*covar + ssim_c2)
         / ((float)(fs1*fs1 + fs2*fs2 + ssim_c1) * (float)(vars + ssim_c2));
#undef PIXEL_MAX
#undef type
}




float ssim_end4( int sum0[5][4], int sum1[5][4], int width )
{
    float ssim = 0.0;
    for( int i = 0; i < width; i++ )
        ssim += ssim_end1( sum0[i][0] + sum0[i+1][0] + sum1[i][0] + sum1[i+1][0],
                           sum0[i][1] + sum0[i+1][1] + sum1[i][1] + sum1[i+1][1],
                           sum0[i][2] + sum0[i+1][2] + sum1[i][2] + sum1[i+1][2],
                           sum0[i][3] + sum0[i+1][3] + sum1[i][3] + sum1[i+1][3] );
    return ssim;
}


float ssim_end4_10b( int sum0[5][4], int sum1[5][4], int width )
{
    float ssim = 0.0;
    for( int i = 0; i < width; i++ )
        ssim += ssim_end1_10b( sum0[i][0] + sum0[i+1][0] + sum1[i][0] + sum1[i+1][0],
                           sum0[i][1] + sum0[i+1][1] + sum1[i][1] + sum1[i+1][1],
                           sum0[i][2] + sum0[i+1][2] + sum1[i][2] + sum1[i+1][2],
                           sum0[i][3] + sum0[i+1][3] + sum1[i][3] + sum1[i+1][3] );
    return ssim;
}

float vbench_pixel_ssim_wxh( 
                           pixel *pix1, intptr_t stride1,
                           pixel *pix2, intptr_t stride2,
                           int width, int height, void *buf, int *cnt )
{
    int z = 0;
    float ssim = 0.0;
    int (*sum0)[4] = buf;
    int (*sum1)[4] = sum0 + (width >> 2) + 3;
    width >>= 2;
    height >>= 2;
    for( int y = 1; y < height; y++ )
    {
        for( ; z <= y; z++ )
        {
            XCHG( void*, sum0, sum1 );
            for( int x = 0; x < width; x+=2 )
                ssim_4x4x2_core( &pix1[4*(x+z*stride1)], stride1, &pix2[4*(x+z*stride2)], stride2, &sum0[x] );
        }
        for( int x = 0; x < width-1; x += 4 )
            ssim += ssim_end4( sum0+x, sum1+x, MIN(4,width-x-1) );
    }
    *cnt = (height-1) * (width-1);
    return ssim;
}

float vbench_pixel_ssim_wxh_10b( 
                               pixel_10b *pix1, intptr_t stride1,
                               pixel_10b *pix2, intptr_t stride2,
                               int width, int height, void *buf, int *cnt )
{
    int z = 0;
    float ssim = 0.0;
    int (*sum0)[4] = buf;
    int (*sum1)[4] = sum0 + (width >> 2) + 3;
    width >>= 2;
    height >>= 2;
    for( int y = 1; y < height; y++ )
    {
        for( ; z <= y; z++ )
        {
            XCHG( void*, sum0, sum1 );
            for( int x = 0; x < width; x+=2 )
                ssim_4x4x2_core_10b( &pix1[4*(x+z*stride1)], stride1, &pix2[4*(x+z*stride2)], stride2, &sum0[x] );
        }
        for( int x = 0; x < width-1; x += 4 )
            ssim += ssim_end4_10b( sum0+x, sum1+x, MIN(4,width-x-1) );
    }
    *cnt = (height-1) * (width-1);
    return ssim;
}

/////////////////////////////////////////////////////////////////////
int pixel_vsad( pixel *src, intptr_t stride, int height )
{
    int score = 0;
    for( int i = 1; i < height; i++, src += stride )
        for( int j = 0; j < 16; j++ )
            score += abs(src[j] - src[j+stride]);
    return score;
}

int pixel_vsad_10b( pixel *src, intptr_t stride, int height )
{
    int score = 0;
    for( int i = 1; i < height; i++, src += stride )
        for( int j = 0; j < 16; j++ )
            score += abs(src[j] - src[j+stride]);
    return score;
}

int pixel_asd8( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2, int height )
{
    int sum = 0;
    for( int y = 0; y < height; y++, pix1 += stride1, pix2 += stride2 )
        for( int x = 0; x < 8; x++ )
            sum += pix1[x] - pix2[x];
    return abs( sum );
}

int pixel_asd8_10b( pixel_10b *pix1, intptr_t stride1, pixel_10b *pix2, intptr_t stride2, int height )
{
    int sum = 0;
    for( int y = 0; y < height; y++, pix1 += stride1, pix2 += stride2 )
        for( int x = 0; x < 8; x++ )
            sum += pix1[x] - pix2[x];
    return abs( sum );
}

/****************************************************************************
 * successive elimination
 ****************************************************************************/
int pixel_ads4( int enc_dc[4], uint16_t *sums, int delta,
                            uint16_t *cost_mvx, int16_t *mvs, int width, int thresh )
{
    int nmv = 0;
    for( int i = 0; i < width; i++, sums++ )
    {
        int ads = abs( enc_dc[0] - sums[0] )
                + abs( enc_dc[1] - sums[8] )
                + abs( enc_dc[2] - sums[delta] )
                + abs( enc_dc[3] - sums[delta+8] )
                + cost_mvx[i];
        if( ads < thresh )
            mvs[nmv++] = i;
    }
    return nmv;
}

int pixel_ads2( int enc_dc[2], uint16_t *sums, int delta,
                            uint16_t *cost_mvx, int16_t *mvs, int width, int thresh )
{
    int nmv = 0;
    for( int i = 0; i < width; i++, sums++ )
    {
        int ads = abs( enc_dc[0] - sums[0] )
                + abs( enc_dc[1] - sums[delta] )
                + cost_mvx[i];
        if( ads < thresh )
            mvs[nmv++] = i;
    }
    return nmv;
}

int pixel_ads1( int enc_dc[1], uint16_t *sums, int delta,
                            uint16_t *cost_mvx, int16_t *mvs, int width, int thresh )
{
    int nmv = 0;
    for( int i = 0; i<width; i++, sums++ )
    {
        int ads = abs( enc_dc[0] - sums[0] )
                + cost_mvx[i];
        if( ads < thresh )
            mvs[nmv++] = i;
    }
    return nmv;
}





