/*****************************************************************************
 * bench_mc.c: run and measure the kernels
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
 * Notes:   The _10b subfix is for the high bit depth. 
 *
 * *************************************************************************** 
 *
 *  Version - Changes
 *
 *  0.1     - Initial checkin. 
 *
 *
 *****************************************************************************/


#include <ctype.h>
#include "osdep.h"
#include "common.h"
#include "bench.h"



/* buf1, buf2: initialised to random data and shouldn't write into them */
extern uint8_t *buf1, *buf2;
/* buf3, buf4: used to store output */
extern uint8_t *buf3, *buf4;
/* pbuf1, pbuf2: initialised to random pixel data and shouldn't write into them. */
extern pixel *pbuf1, *pbuf2;
/* pbuf3, pbuf4: point to buf3, buf4, just for type convenience */
extern pixel *pbuf3, *pbuf4;



#define report( name ) { \
    if( used_asm ) \
        fprintf( stderr, " - %-21s [%s]\n", name, ok ? "OK" : "FAILED" ); \
    if( !ok ) ret = -1; \
}


extern int bench_pattern_len;
extern const char *bench_pattern;
char func_name[100];
extern  bench_func_t benchs[MAX_FUNCS];

static const char *pixel_names[12] = { "16x16", "16x8", "8x16", "8x8", "8x4", "4x8", "4x4", "4x16", "4x2", "2x8", "2x4", "2x2" };
static const char *intra_predict_16x16_names[7] = { "v", "h", "dc", "p", "dcl", "dct", "dc8" };
static const char *intra_predict_8x8c_names[7] = { "dc", "h", "v", "p", "dcl", "dct", "dc8" };
static const char *intra_predict_4x4_names[12] = { "v", "h", "dc", "ddl", "ddr", "vr", "hd", "vl", "hu", "dcl", "dct", "dc8" };
static const char **intra_predict_8x8_names = intra_predict_4x4_names;
static const char **intra_predict_8x16c_names = intra_predict_8x8c_names;

#define set_func_name(...) snprintf( func_name, sizeof(func_name), __VA_ARGS__ )



const uint8_t vbench_hpel_ref0[16] = {0,1,1,1,0,1,1,1,2,3,3,3,0,1,1,1};
const uint8_t vbench_hpel_ref1[16] = {0,0,1,0,2,2,3,2,2,2,3,2,2,2,3,2};

static bench_t* get_bench( const char *name, int cpu )
{
    int i, j;
    for( i = 0; benchs[i].name && strcmp(name, benchs[i].name); i++ )
        assert( i < MAX_FUNCS );
    if( !benchs[i].name )
        benchs[i].name = strdup( name );
    if( !cpu )
        return &benchs[i].vers[0];
    for( j = 1; benchs[i].vers[j].cpu && benchs[i].vers[j].cpu != cpu; j++ )
        assert( j < MAX_CPUS );
    benchs[i].vers[j].cpu = cpu;
    return &benchs[i].vers[j];
}
extern vbench_weight_t vbench_weight_none[3];
int check_mc( int cpu_ref, int cpu_new )
{
    vbench_mc_functions_t mc_c;
    vbench_mc_functions_t mc_ref;
    vbench_mc_functions_t mc_a;
    vbench_pixel_function_t pixf;

    pixel *src     = &(pbuf1)[2*64+2];
    pixel *src2[4] = { &(pbuf1)[3*64+2], &(pbuf1)[5*64+2],
                       &(pbuf1)[7*64+2], &(pbuf1)[9*64+2] };
    pixel *dst1    = pbuf3;
    pixel *dst2    = pbuf4;

    int ret = 0, ok, used_asm;

    vbench_mc_init( 0, &mc_c, 0 );
    vbench_mc_init( cpu_ref, &mc_ref, 0 );
    vbench_mc_init( cpu_new, &mc_a, 0 );
    vbench_pixel_init( 0, &pixf );

#define MC_TEST_LUMA( w, h ) \
        if( mc_a.mc_luma != mc_ref.mc_luma && !(w&(w-1)) && h<=16 ) \
        { \
            const vbench_weight_t *weight = vbench_weight_none; \
            set_func_name( "mc_luma_%dx%d", w, h ); \
            used_asm = 1; \
            for( int i = 0; i < 1024; i++ ) \
                pbuf3[i] = pbuf4[i] = 0xCD; \
            call_c( mc_c.mc_luma, dst1, (intptr_t)32, src2, (intptr_t)64, dx, dy, w, h, weight ); \
            call_a( mc_a.mc_luma, dst2, (intptr_t)32, src2, (intptr_t)64, dx, dy, w, h, weight ); \
            if( memcmp( pbuf3, pbuf4, 1024 * sizeof(pixel) ) ) \
            { \
                fprintf( stderr, "mc_luma[mv(%d,%d) %2dx%-2d]     [FAILED]\n", dx, dy, w, h ); \
                ok = 0; \
            } \
        } \
        if( mc_a.get_ref != mc_ref.get_ref ) \
        { \
            pixel *ref = dst2; \
            intptr_t ref_stride = 32; \
            int w_checked = ( ( sizeof(pixel) == 2 && (w == 12 || w == 20)) ? w-2 : w ); \
            const vbench_weight_t *weight = vbench_weight_none; \
            set_func_name( "get_ref_%dx%d", w_checked, h ); \
            used_asm = 1; \
            for( int i = 0; i < 1024; i++ ) \
                pbuf3[i] = pbuf4[i] = 0xCD; \
            call_c( mc_c.mc_luma, dst1, (intptr_t)32, src2, (intptr_t)64, dx, dy, w, h, weight ); \
            ref = (pixel*)call_a( mc_a.get_ref, ref, &ref_stride, src2, (intptr_t)64, dx, dy, w, h, weight ); \
            for( int i = 0; i < h; i++ ) \
                if( memcmp( dst1+i*32, ref+i*ref_stride, w_checked * sizeof(pixel) ) ) \
                { \
                    fprintf( stderr, "get_ref[mv(%d,%d) %2dx%-2d]     [FAILED]\n", dx, dy, w_checked, h ); \
                    ok = 0; \
                    break; \
                } \
        }

#define MC_TEST_CHROMA( w, h ) \
        if( mc_a.mc_chroma != mc_ref.mc_chroma ) \
        { \
            set_func_name( "mc_chroma_%dx%d", w, h ); \
            used_asm = 1; \
            for( int i = 0; i < 1024; i++ ) \
                pbuf3[i] = pbuf4[i] = 0xCD; \
            call_c( mc_c.mc_chroma, dst1, dst1+8, (intptr_t)16, src, (intptr_t)64, dx, dy, w, h ); \
            call_a( mc_a.mc_chroma, dst2, dst2+8, (intptr_t)16, src, (intptr_t)64, dx, dy, w, h ); \
            /* mc_chroma width=2 may write garbage to the right of dst. ignore that. */ \
            for( int j = 0; j < h; j++ ) \
                for( int i = w; i < 8; i++ ) \
                { \
                    dst2[i+j*16+8] = dst1[i+j*16+8]; \
                    dst2[i+j*16  ] = dst1[i+j*16  ]; \
                } \
            if( memcmp( pbuf3, pbuf4, 1024 * sizeof(pixel) ) ) \
            { \
                fprintf( stderr, "mc_chroma[mv(%d,%d) %2dx%-2d]     [FAILED]\n", dx, dy, w, h ); \
                ok = 0; \
            } \
        }
    ok = 1; used_asm = 0;
    for( int dy = -8; dy < 8; dy++ )
        for( int dx = -128; dx < 128; dx++ )
        {
            if( rand()&15 ) continue; // running all of them is too slow
            MC_TEST_LUMA( 20, 18 );
            MC_TEST_LUMA( 16, 16 );
            MC_TEST_LUMA( 16, 8 );
            MC_TEST_LUMA( 12, 10 );
            MC_TEST_LUMA( 8, 16 );
            MC_TEST_LUMA( 8, 8 );
            MC_TEST_LUMA( 8, 4 );
            MC_TEST_LUMA( 4, 8 );
            MC_TEST_LUMA( 4, 4 );
        }
    report( "mc luma :" );

    ok = 1; used_asm = 0;
    for( int dy = -1; dy < 9; dy++ )
        for( int dx = -128; dx < 128; dx++ )
        {
            if( rand()&15 ) continue;
            MC_TEST_CHROMA( 8, 8 );
            MC_TEST_CHROMA( 8, 4 );
            MC_TEST_CHROMA( 4, 8 );
            MC_TEST_CHROMA( 4, 4 );
            MC_TEST_CHROMA( 4, 2 );
            MC_TEST_CHROMA( 2, 4 );
            MC_TEST_CHROMA( 2, 2 );
        }
    report( "mc chroma :" );
#undef MC_TEST_LUMA
#undef MC_TEST_CHROMA

#define MC_TEST_AVG( name, weight ) \
{ \
    for( int i = 0; i < 12; i++ ) \
    { \
        memcpy( pbuf3, pbuf1+320, 320 * sizeof(pixel) ); \
        memcpy( pbuf4, pbuf1+320, 320 * sizeof(pixel) ); \
        if( mc_a.name[i] != mc_ref.name[i] ) \
        { \
            set_func_name( "%s_%s", #name, pixel_names[i] ); \
            used_asm = 1; \
            call_c1( mc_c.name[i], pbuf3, (intptr_t)16, pbuf2+1, (intptr_t)16, pbuf1+18, (intptr_t)16, weight ); \
            call_a1( mc_a.name[i], pbuf4, (intptr_t)16, pbuf2+1, (intptr_t)16, pbuf1+18, (intptr_t)16, weight ); \
            if( memcmp( pbuf3, pbuf4, 320 * sizeof(pixel) ) ) \
            { \
                ok = 0; \
                fprintf( stderr, #name "[%d]: [FAILED]\n", i ); \
            } \
            call_c2( mc_c.name[i], pbuf3, (intptr_t)16, pbuf2+1, (intptr_t)16, pbuf1+18, (intptr_t)16, weight ); \
            call_a2( mc_a.name[i], pbuf4, (intptr_t)16, pbuf2+1, (intptr_t)16, pbuf1+18, (intptr_t)16, weight ); \
        } \
    } \
}

    ok = 1, used_asm = 0;
    for( int w = -63; w <= 127 && ok; w++ )
        MC_TEST_AVG( avg, w );
    report( "mc wpredb :" );

#define MC_TEST_WEIGHT( name, weight, aligned ) \
    int align_off = (aligned ? 0 : rand()%16); \
    for( int i = 1; i <= 5; i++ ) \
    { \
        ALIGNED_16( pixel buffC[640] ); \
        ALIGNED_16( pixel buffA[640] ); \
        int j = MAX( i*4, 2 ); \
        memset( buffC, 0, 640 * sizeof(pixel) ); \
        memset( buffA, 0, 640 * sizeof(pixel) ); \
        /* w12 is the same as w16 in some cases */ \
        if( i == 3 && mc_a.name[i] == mc_a.name[i+1] ) \
            continue; \
        if( mc_a.name[i] != mc_ref.name[i] ) \
        { \
            set_func_name( "%s_w%d", #name, j ); \
            used_asm = 1; \
            call_c1( mc_c.weight[i],     buffC, (intptr_t)32, pbuf2+align_off, (intptr_t)32, &weight, 16 ); \
            mc_a.weight_cache(&mc_a, &weight); \
            call_a1( weight.weightfn[i], buffA, (intptr_t)32, pbuf2+align_off, (intptr_t)32, &weight, 16 ); \
            for( int k = 0; k < 16; k++ ) \
                if( memcmp( &buffC[k*32], &buffA[k*32], j * sizeof(pixel) ) ) \
                { \
                    ok = 0; \
                    fprintf( stderr, #name "[%d]: [FAILED] s:%d o:%d d%d\n", i, s, o, d ); \
                    break; \
                } \
            /* omit unlikely high scales for benchmarking */ \
            if( (s << (8-d)) < 512 ) \
            { \
                call_c2( mc_c.weight[i],     buffC, (intptr_t)32, pbuf2+align_off, (intptr_t)32, &weight, 16 ); \
                call_a2( weight.weightfn[i], buffA, (intptr_t)32, pbuf2+align_off, (intptr_t)32, &weight, 16 ); \
            } \
        } \
    }

    ok = 1; used_asm = 0;

    int align_cnt = 0;
    for( int s = 0; s <= 127 && ok; s++ )
    {
        for( int o = -128; o <= 127 && ok; o++ )
        {
            if( rand() & 2047 ) continue;
            for( int d = 0; d <= 7 && ok; d++ )
            {
                if( s == 1<<d )
                    continue;
                vbench_weight_t weight = { .i_scale = s, .i_denom = d, .i_offset = o };
                MC_TEST_WEIGHT( weight, weight, (align_cnt++ % 4) );
            }
        }

    }
    report( "mc weight :" );

    ok = 1; used_asm = 0;
    for( int o = 0; o <= 127 && ok; o++ )
    {
        int s = 1, d = 0;
        if( rand() & 15 ) continue;
        vbench_weight_t weight = { .i_scale = 1, .i_denom = 0, .i_offset = o };
        MC_TEST_WEIGHT( offsetadd, weight, (align_cnt++ % 4) );
    }
    report( "mc offsetadd :" );
    ok = 1; used_asm = 0;
    for( int o = -128; o < 0 && ok; o++ )
    {
        int s = 1, d = 0;
        if( rand() & 15 ) continue;
        vbench_weight_t weight = { .i_scale = 1, .i_denom = 0, .i_offset = o };
        MC_TEST_WEIGHT( offsetsub, weight, (align_cnt++ % 4) );
    }
    report( "mc offsetsub :" );

    ok = 1; used_asm = 0;
    for( int height = 8; height <= 16; height += 8 )
    {
        if( mc_a.store_interleave_chroma != mc_ref.store_interleave_chroma )
        {
            set_func_name( "store_interleave_chroma" );
            used_asm = 1;
            memset( pbuf3, 0, 64*height );
            memset( pbuf4, 0, 64*height );
            call_c( mc_c.store_interleave_chroma, pbuf3, (intptr_t)64, pbuf1, pbuf1+16, height );
            call_a( mc_a.store_interleave_chroma, pbuf4, (intptr_t)64, pbuf1, pbuf1+16, height );
            if( memcmp( pbuf3, pbuf4, 64*height ) )
            {
                ok = 0;
                fprintf( stderr, "store_interleave_chroma FAILED: h=%d\n", height );
                break;
            }
        }
        if( mc_a.load_deinterleave_chroma_fenc != mc_ref.load_deinterleave_chroma_fenc )
        {
            set_func_name( "load_deinterleave_chroma_fenc" );
            used_asm = 1;
            call_c( mc_c.load_deinterleave_chroma_fenc, pbuf3, pbuf1, (intptr_t)64, height );
            call_a( mc_a.load_deinterleave_chroma_fenc, pbuf4, pbuf1, (intptr_t)64, height );
            if( memcmp( pbuf3, pbuf4, FENC_STRIDE*height ) )
            {
                ok = 0;
                fprintf( stderr, "load_deinterleave_chroma_fenc FAILED: h=%d\n", height );
                break;
            }
        }
        if( mc_a.load_deinterleave_chroma_fdec != mc_ref.load_deinterleave_chroma_fdec )
        {
            set_func_name( "load_deinterleave_chroma_fdec" );
            used_asm = 1;
            call_c( mc_c.load_deinterleave_chroma_fdec, pbuf3, pbuf1, (intptr_t)64, height );
            call_a( mc_a.load_deinterleave_chroma_fdec, pbuf4, pbuf1, (intptr_t)64, height );
            if( memcmp( pbuf3, pbuf4, FDEC_STRIDE*height ) )
            {
                ok = 0;
                fprintf( stderr, "load_deinterleave_chroma_fdec FAILED: h=%d\n", height );
                break;
            }
        }
    }
    report( "store_interleave :" );

    struct plane_spec {
        int w, h, src_stride;
    } plane_specs[] = { {2,2,2}, {8,6,8}, {20,31,24}, {32,8,40}, {256,10,272}, {504,7,505}, {528,6,528}, {256,10,-256}, {263,9,-264}, {1904,1,0} };
    ok = 1; used_asm = 0;
    if( mc_a.plane_copy != mc_ref.plane_copy )
    {
        set_func_name( "plane_copy" );
        used_asm = 1;
        for( int i = 0; i < sizeof(plane_specs)/sizeof(*plane_specs); i++ )
        {
            int w = plane_specs[i].w;
            int h = plane_specs[i].h;
            intptr_t src_stride = plane_specs[i].src_stride;
            intptr_t dst_stride = (w + 127) & ~63;
            assert( dst_stride * h <= 0x1000 );
            pixel *src1 = pbuf1 + MAX(0, -src_stride) * (h-1);
            memset( pbuf3, 0, 0x1000*sizeof(pixel) );
            memset( pbuf4, 0, 0x1000*sizeof(pixel) );
            call_c( mc_c.plane_copy, pbuf3, dst_stride, src1, src_stride, w, h );
            call_a( mc_a.plane_copy, pbuf4, dst_stride, src1, src_stride, w, h );
            for( int y = 0; y < h; y++ )
                if( memcmp( pbuf3+y*dst_stride, pbuf4+y*dst_stride, w*sizeof(pixel) ) )
                {
                    ok = 0;
                    fprintf( stderr, "plane_copy FAILED: w=%d h=%d stride=%d\n", w, h, (int)src_stride );
                    break;
                }
        }
    }

    if( mc_a.plane_copy_swap != mc_ref.plane_copy_swap )
    {
        set_func_name( "plane_copy_swap" );
        used_asm = 1;
        for( int i = 0; i < sizeof(plane_specs)/sizeof(*plane_specs); i++ )
        {
            int w = (plane_specs[i].w + 1) >> 1;
            int h = plane_specs[i].h;
            intptr_t src_stride = plane_specs[i].src_stride;
            intptr_t dst_stride = (2*w + 127) & ~63;
            assert( dst_stride * h <= 0x1000 );
            pixel *src1 = pbuf1 + MAX(0, -src_stride) * (h-1);
            memset( pbuf3, 0, 0x1000*sizeof(pixel) );
            memset( pbuf4, 0, 0x1000*sizeof(pixel) );
            call_c( mc_c.plane_copy_swap, pbuf3, dst_stride, src1, src_stride, w, h );
            call_a( mc_a.plane_copy_swap, pbuf4, dst_stride, src1, src_stride, w, h );
            for( int y = 0; y < h; y++ )
                if( memcmp( pbuf3+y*dst_stride, pbuf4+y*dst_stride, 2*w*sizeof(pixel) ) )
                {
                    ok = 0;
                    fprintf( stderr, "plane_copy_swap FAILED: w=%d h=%d stride=%d\n", w, h, (int)src_stride );
                    break;
                }
        }
    }

    if( mc_a.plane_copy_interleave != mc_ref.plane_copy_interleave )
    {
        set_func_name( "plane_copy_interleave" );
        used_asm = 1;
        for( int i = 0; i < sizeof(plane_specs)/sizeof(*plane_specs); i++ )
        {
            int w = (plane_specs[i].w + 1) >> 1;
            int h = plane_specs[i].h;
            intptr_t src_stride = (plane_specs[i].src_stride + 1) >> 1;
            intptr_t dst_stride = (2*w + 127) & ~63;
            assert( dst_stride * h <= 0x1000 );
            pixel *src1 = pbuf1 + MAX(0, -src_stride) * (h-1);
            memset( pbuf3, 0, 0x1000*sizeof(pixel) );
            memset( pbuf4, 0, 0x1000*sizeof(pixel) );
            call_c( mc_c.plane_copy_interleave, pbuf3, dst_stride, src1, src_stride, src1+1024, src_stride+16, w, h );
            call_a( mc_a.plane_copy_interleave, pbuf4, dst_stride, src1, src_stride, src1+1024, src_stride+16, w, h );
            for( int y = 0; y < h; y++ )
                if( memcmp( pbuf3+y*dst_stride, pbuf4+y*dst_stride, 2*w*sizeof(pixel) ) )
                {
                    ok = 0;
                    fprintf( stderr, "plane_copy_interleave FAILED: w=%d h=%d stride=%d\n", w, h, (int)src_stride );
                    break;
                }
        }
    }

    if( mc_a.plane_copy_deinterleave != mc_ref.plane_copy_deinterleave )
    {
        set_func_name( "plane_copy_deinterleave" );
        used_asm = 1;
        for( int i = 0; i < sizeof(plane_specs)/sizeof(*plane_specs); i++ )
        {
            int w = (plane_specs[i].w + 1) >> 1;
            int h = plane_specs[i].h;
            intptr_t dst_stride = w;
            intptr_t src_stride = (2*w + 127) & ~63;
            intptr_t offv = (dst_stride*h + 31) & ~15;
            memset( pbuf3, 0, 0x1000 );
            memset( pbuf4, 0, 0x1000 );
            call_c( mc_c.plane_copy_deinterleave, pbuf3, dst_stride, pbuf3+offv, dst_stride, pbuf1, src_stride, w, h );
            call_a( mc_a.plane_copy_deinterleave, pbuf4, dst_stride, pbuf4+offv, dst_stride, pbuf1, src_stride, w, h );
            for( int y = 0; y < h; y++ )
                if( memcmp( pbuf3+y*dst_stride,      pbuf4+y*dst_stride, w ) ||
                    memcmp( pbuf3+y*dst_stride+offv, pbuf4+y*dst_stride+offv, w ) )
                {
                    ok = 0;
                    fprintf( stderr, "plane_copy_deinterleave FAILED: w=%d h=%d stride=%d\n", w, h, (int)src_stride );
                    break;
                }
        }
    }

    if( mc_a.plane_copy_deinterleave_rgb != mc_ref.plane_copy_deinterleave_rgb )
    {
        set_func_name( "plane_copy_deinterleave_rgb" );
        used_asm = 1;
        for( int i = 0; i < sizeof(plane_specs)/sizeof(*plane_specs); i++ )
        {
            int w = (plane_specs[i].w + 2) >> 2;
            int h = plane_specs[i].h;
            intptr_t src_stride = plane_specs[i].src_stride;
            intptr_t dst_stride = ALIGN( w, 16 );
            intptr_t offv = dst_stride*h + 16;

            for( int pw = 3; pw <= 4; pw++ )
            {
                memset( pbuf3, 0, 0x1000 );
                memset( pbuf4, 0, 0x1000 );
                call_c( mc_c.plane_copy_deinterleave_rgb, pbuf3, dst_stride, pbuf3+offv, dst_stride, pbuf3+2*offv, dst_stride, pbuf1, src_stride, pw, w, h );
                call_a( mc_a.plane_copy_deinterleave_rgb, pbuf4, dst_stride, pbuf4+offv, dst_stride, pbuf4+2*offv, dst_stride, pbuf1, src_stride, pw, w, h );
                for( int y = 0; y < h; y++ )
                    if( memcmp( pbuf3+y*dst_stride+0*offv, pbuf4+y*dst_stride+0*offv, w ) ||
                        memcmp( pbuf3+y*dst_stride+1*offv, pbuf4+y*dst_stride+1*offv, w ) ||
                        memcmp( pbuf3+y*dst_stride+2*offv, pbuf4+y*dst_stride+2*offv, w ) )
                    {
                        ok = 0;
                        fprintf( stderr, "plane_copy_deinterleave_rgb FAILED: w=%d h=%d stride=%d pw=%d\n", w, h, (int)src_stride, pw );
                        break;
                    }
            }
        }
    }
    report( "plane_copy :" );

    if( mc_a.plane_copy_deinterleave_v210 != mc_ref.plane_copy_deinterleave_v210 )
    {
        set_func_name( "plane_copy_deinterleave_v210" );
        ok = 1; used_asm = 1;
        for( int i = 0; i < sizeof(plane_specs)/sizeof(*plane_specs); i++ )
        {
            int w = (plane_specs[i].w + 1) >> 1;
            int h = plane_specs[i].h;
            intptr_t dst_stride = ALIGN( w, 16 );
            intptr_t src_stride = (w + 47) / 48 * 128 / sizeof(uint32_t);
            intptr_t offv = dst_stride*h + 32;
            memset( pbuf3, 0, 0x1000 );
            memset( pbuf4, 0, 0x1000 );
            call_c( mc_c.plane_copy_deinterleave_v210, pbuf3, dst_stride, pbuf3+offv, dst_stride, (uint32_t *)buf1, src_stride, w, h );
            call_a( mc_a.plane_copy_deinterleave_v210, pbuf4, dst_stride, pbuf4+offv, dst_stride, (uint32_t *)buf1, src_stride, w, h );
            for( int y = 0; y < h; y++ )
                if( memcmp( pbuf3+y*dst_stride,      pbuf4+y*dst_stride,      w*sizeof(uint16_t) ) ||
                    memcmp( pbuf3+y*dst_stride+offv, pbuf4+y*dst_stride+offv, w*sizeof(uint16_t) ) )
                {
                    ok = 0;
                    fprintf( stderr, "plane_copy_deinterleave_v210 FAILED: w=%d h=%d stride=%d\n", w, h, (int)src_stride );
                    break;
                }
        }
        report( "v210 :" );
    }

    if( mc_a.hpel_filter != mc_ref.hpel_filter )
    {
        pixel *srchpel = pbuf1+8+2*64;
        pixel *dstc[3] = { pbuf3+8, pbuf3+8+16*64, pbuf3+8+32*64 };
        pixel *dsta[3] = { pbuf4+8, pbuf4+8+16*64, pbuf4+8+32*64 };
        void *tmp = pbuf3+49*64;
        set_func_name( "hpel_filter" );
        ok = 1; used_asm = 1;
        memset( pbuf3, 0, 4096 * sizeof(pixel) );
        memset( pbuf4, 0, 4096 * sizeof(pixel) );
        call_c( mc_c.hpel_filter, dstc[0], dstc[1], dstc[2], srchpel, (intptr_t)64, 48, 10, tmp );
        call_a( mc_a.hpel_filter, dsta[0], dsta[1], dsta[2], srchpel, (intptr_t)64, 48, 10, tmp );
        for( int i = 0; i < 3; i++ )
            for( int j = 0; j < 10; j++ )
                //FIXME ideally the first pixels would match too, but they aren't actually used
                if( memcmp( dstc[i]+j*64+2, dsta[i]+j*64+2, 43 * sizeof(pixel) ) )
                {
                    ok = 0;
                    fprintf( stderr, "hpel filter differs at plane %c line %d\n", "hvc"[i], j );
                    for( int k = 0; k < 48; k++ )
                        printf( "%02x%s", dstc[i][j*64+k], (k+1)&3 ? "" : " " );
                    printf( "\n" );
                    for( int k = 0; k < 48; k++ )
                        printf( "%02x%s", dsta[i][j*64+k], (k+1)&3 ? "" : " " );
                    printf( "\n" );
                    break;
                }
        report( "hpel filter :" );
    }

    if( mc_a.frame_init_lowres_core != mc_ref.frame_init_lowres_core )
    {
        pixel *dstc[4] = { pbuf3, pbuf3+1024, pbuf3+2048, pbuf3+3072 };
        pixel *dsta[4] = { pbuf4, pbuf4+1024, pbuf4+2048, pbuf4+3072 };
        set_func_name( "lowres_init" );
        ok = 1; used_asm = 1;
        for( int w = 96; w <= 96+24; w += 8 )
        {
            intptr_t stride = (w*2+31)&~31;
            intptr_t stride_lowres = (w+31)&~31;
            call_c( mc_c.frame_init_lowres_core, pbuf1, dstc[0], dstc[1], dstc[2], dstc[3], stride, stride_lowres, w, 8 );
            call_a( mc_a.frame_init_lowres_core, pbuf1, dsta[0], dsta[1], dsta[2], dsta[3], stride, stride_lowres, w, 8 );
            for( int i = 0; i < 8; i++ )
            {
                for( int j = 0; j < 4; j++ )
                    if( memcmp( dstc[j]+i*stride_lowres, dsta[j]+i*stride_lowres, w * sizeof(pixel) ) )
                    {
                        ok = 0;
                        fprintf( stderr, "frame_init_lowres differs at plane %d line %d\n", j, i );
                        for( int k = 0; k < w; k++ )
                            printf( "%d ", dstc[j][k+i*stride_lowres] );
                        printf( "\n" );
                        for( int k = 0; k < w; k++ )
                            printf( "%d ", dsta[j][k+i*stride_lowres] );
                        printf( "\n" );
                        break;
                    }
            }
        }
        report( "lowres init :" );
    }

#define INTEGRAL_INIT( name, size, offset, cmp_len, ... )\
    if( mc_a.name != mc_ref.name )\
    {\
        intptr_t stride = 96;\
        set_func_name( #name );\
        used_asm = 1;\
        memcpy( buf3, buf1, size*2*stride );\
        memcpy( buf4, buf1, size*2*stride );\
        uint16_t *sum = (uint16_t*)buf3;\
        call_c1( mc_c.name, sum+offset, __VA_ARGS__ );\
        sum = (uint16_t*)buf4;\
        call_a1( mc_a.name, sum+offset, __VA_ARGS__ );\
        if( memcmp( buf3+2*offset, buf4+2*offset, cmp_len*2 )\
            || (size>9 && memcmp( buf3+18*stride, buf4+18*stride, (stride-8)*2 )))\
            ok = 0;\
        call_c2( mc_c.name, sum+offset, __VA_ARGS__ );\
        call_a2( mc_a.name, sum+offset, __VA_ARGS__ );\
    }
    ok = 1; used_asm = 0;
    INTEGRAL_INIT( integral_init4h, 2, stride, stride-4, pbuf2, stride );
    INTEGRAL_INIT( integral_init8h, 2, stride, stride-8, pbuf2, stride );
    INTEGRAL_INIT( integral_init4v, 14, 0, stride-8, sum+9*stride, stride );
    INTEGRAL_INIT( integral_init8v, 9, 0, stride-8, stride );
    report( "integral init :" );

    ok = 1; used_asm = 0;
    if( mc_a.mbtree_propagate_cost != mc_ref.mbtree_propagate_cost )
    {
        used_asm = 1;
        vbench_emms();
        for( int i = 0; i < 10; i++ )
        {
            float fps_factor = (rand()&65535) / 65535.0f;
            set_func_name( "mbtree_propagate_cost" );
            int16_t *dsta = (int16_t*)buf3;
            int16_t *dstc = dsta+400;
            uint16_t *prop = (uint16_t*)buf1;
            uint16_t *intra = (uint16_t*)buf4;
            uint16_t *inter = intra+128;
            uint16_t *qscale = inter+128;
            uint16_t *rnd = (uint16_t*)buf2;
            vbench_emms();
            for( int j = 0; j < 100; j++ )
            {
                intra[j]  = *rnd++ & 0x7fff;
                intra[j] += !intra[j];
                inter[j]  = *rnd++ & 0x7fff;
                qscale[j] = *rnd++ & 0x7fff;
            }
            call_c( mc_c.mbtree_propagate_cost, dstc, prop, intra, inter, qscale, &fps_factor, 100 );
            call_a( mc_a.mbtree_propagate_cost, dsta, prop, intra, inter, qscale, &fps_factor, 100 );
            // I don't care about exact rounding, this is just how close the floating-point implementation happens to be
            vbench_emms();
            for( int j = 0; j < 100 && ok; j++ )
            {
                ok &= abs( dstc[j]-dsta[j] ) <= 1 || fabs( (double)dstc[j]/dsta[j]-1 ) < 1e-4;
                if( !ok )
                    fprintf( stderr, "mbtree_propagate_cost FAILED: %f !~= %f\n", (double)dstc[j], (double)dsta[j] );
            }
        }
    }

    if( mc_a.mbtree_propagate_list != mc_ref.mbtree_propagate_list )
    {
        used_asm = 1;
        for( int i = 0; i < 8; i++ )
        {
            set_func_name( "mbtree_propagate_list" );
            int height = 4;
            int width = 128;
            int size = width*height;
            /*h.mb.i_mb_stride = width;
            h.mb.i_mb_width = width;
            h.mb.i_mb_height = height;*/

            uint16_t *ref_costsc = (uint16_t*)buf3;
            uint16_t *ref_costsa = (uint16_t*)buf4;
            int16_t (*mvs)[2] = (int16_t(*)[2])(ref_costsc + size);
            int16_t *propagate_amount = (int16_t*)(mvs + width);
            uint16_t *lowres_costs = (uint16_t*)(propagate_amount + width);


            //h.scratch_buffer2 = (uint8_t*)(ref_costsa + size);
            void *scratch_buffer2 = (uint8_t*)(ref_costsa + size);
            

            int bipred_weight = (rand()%63)+1;
            int list = i&1;
            for( int j = 0; j < size; j++ )
                ref_costsc[j] = ref_costsa[j] = rand()&32767;
            for( int j = 0; j < width; j++ )
            {
                static const uint8_t list_dist[2][8] = {{0,1,1,1,1,1,1,1},{1,1,3,3,3,3,3,2}};
                for( int k = 0; k < 2; k++ )
                    mvs[j][k] = (rand()&127) - 64;
                propagate_amount[j] = rand()&32767;
                lowres_costs[j] = list_dist[list][rand()&7] << LOWRES_COST_SHIFT;
            }

            call_c1( mc_c.mbtree_propagate_list, ref_costsc, mvs, propagate_amount, lowres_costs, bipred_weight, 0, width, list, width, width, height, scratch_buffer2 );
            call_a1( mc_a.mbtree_propagate_list, ref_costsa, mvs, propagate_amount, lowres_costs, bipred_weight, 0, width, list, width, width, height, scratch_buffer2 );

            for( int j = 0; j < size && ok; j++ )
            {
                ok &= abs(ref_costsa[j] - ref_costsc[j]) <= 1;
                if( !ok )
                    fprintf( stderr, "mbtree_propagate_list FAILED at %d: %d !~= %d\n", j, ref_costsc[j], ref_costsa[j] );
            }

            call_c2( mc_c.mbtree_propagate_list, ref_costsc, mvs, propagate_amount, lowres_costs, bipred_weight, 0, width, list, width, width, height, scratch_buffer2 );
            call_a2( mc_a.mbtree_propagate_list, ref_costsa, mvs, propagate_amount, lowres_costs, bipred_weight, 0, width, list, width, width, height, scratch_buffer2 );
        }
    }
    report( "mbtree :" );

    if( mc_a.memcpy_aligned != mc_ref.memcpy_aligned )
    {
        set_func_name( "memcpy_aligned" );
        ok = 1; used_asm = 1;
        for( size_t size = 16; size < 256; size += 16 )
        {
            memset( buf4, 0xAA, size + 1 );
            call_c( mc_c.memcpy_aligned, buf3, buf1, size );
            call_a( mc_a.memcpy_aligned, buf4, buf1, size );
            if( memcmp( buf3, buf4, size ) || buf4[size] != 0xAA )
            {
                ok = 0;
                fprintf( stderr, "memcpy_aligned FAILED: size=%d\n", (int)size );
                break;
            }
        }
        report( "memcpy aligned :" );
    }

    if( mc_a.memzero_aligned != mc_ref.memzero_aligned )
    {
        set_func_name( "memzero_aligned" );
        ok = 1; used_asm = 1;
        for( size_t size = 128; size < 1024; size += 128 )
        {
            memset( buf4, 0xAA, size + 1 );
            call_c( mc_c.memzero_aligned, buf3, size );
            call_a( mc_a.memzero_aligned, buf4, size );
            if( memcmp( buf3, buf4, size ) || buf4[size] != 0xAA )
            {
                ok = 0;
                fprintf( stderr, "memzero_aligned FAILED: size=%d\n", (int)size );
                break;
            }
        }
        report( "memzero aligned :" );
    }

    return ret;
}




