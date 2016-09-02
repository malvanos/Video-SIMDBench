/*****************************************************************************
 * bench_cabac.c: run and measure the kernels
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
 *  0.2     - Insert cabac tables  
 *
 *****************************************************************************/


#include <ctype.h>
#include "osdep.h"
#include "common.h"
#include "bench.h"
#include "macroblock.h"

#include "c_kernels/pixel.h"
#include "c_kernels/predict.h"

#define set_func_name(...) snprintf( func_name, sizeof(func_name), __VA_ARGS__ )

extern int bench_pattern_len;
extern const char *bench_pattern;
char func_name[100];
extern  bench_func_t benchs[MAX_FUNCS];



/* buf1, buf2: initialised to random data and shouldn't write into them */
extern uint8_t *buf1, *buf2;
/* buf3, buf4: used to store output */
extern uint8_t *buf3, *buf4;
/* pbuf1, pbuf2: initialised to random pixel data and shouldn't write into them. */
extern pixel *pbuf1, *pbuf2;
/* pbuf3, pbuf4: point to buf3, buf4, just for type convenience */
extern pixel *pbuf3, *pbuf4;

//void x264_cabac_encode_ue_bypass( vbench_cabac_t *cb, int exp_bits, int val )
void asm_cabac_encode_ue_bypass( void *cb, int exp_bits, int val )
{
#if 0
    uint32_t v = val + (1<<exp_bits);
    int k = 31 - x264_clz( v );
    uint32_t x = (bypass_lut[k-exp_bits]<<exp_bits) + v;
    k = 2*k+1-exp_bits;
    int i = ((k-1)&7)+1;
    do {
        k -= i;
        cb->i_low <<= i;
        cb->i_low += ((x>>k)&0xff) * cb->i_range;
        cb->i_queue += i;
        x264_cabac_putbyte( cb );
        i = 8;
    } while( k > 0 );
#endif
}




#define DECL_CABAC(cpu) \
static void run_cabac_decision_##cpu( uint8_t *dst )\
{\
    vbench_cabac_t cb;\
    vbench_cabac_context_init( &cb, SLICE_TYPE_P, 26, 0 );\
    vbench_cabac_encode_init( &cb, dst, dst+0xff0 );\
    for( int i = 0; i < 0x1000; i++ )\
        vbench_cabac_encode_decision_##cpu( &cb, buf1[i]>>1, buf1[i]&1 );\
}\
static void run_cabac_bypass_##cpu( uint8_t *dst )\
{\
    vbench_cabac_t cb;\
    vbench_cabac_context_init( &cb, SLICE_TYPE_P, 26, 0 );\
    vbench_cabac_encode_init( &cb, dst, dst+0xff0 );\
    for( int i = 0; i < 0x1000; i++ )\
        vbench_cabac_encode_bypass_##cpu( &cb, buf1[i]&1 );\
}\
static void run_cabac_terminal_##cpu( uint8_t *dst )\
{\
    vbench_cabac_t cb;\
    vbench_cabac_context_init( &cb, SLICE_TYPE_P, 26, 0 );\
    vbench_cabac_encode_init( &cb, dst, dst+0xff0 );\
    for( int i = 0; i < 0x1000; i++ )\
        vbench_cabac_encode_terminal_##cpu( &cb );\
}
DECL_CABAC(c)
#if HAVE_MMX
DECL_CABAC(asm)
#elif defined(ARCH_AARCH64)
DECL_CABAC(asm)
#else
#define run_cabac_decision_asm run_cabac_decision_c
#define run_cabac_bypass_asm run_cabac_bypass_c
#define run_cabac_terminal_asm run_cabac_terminal_c
#endif

#if 0
void x264_cabac_block_residual_c( x264_t *h, vbench_cabac_t *cb, int ctx_block_cat, dctcoef *l );
void x264_cabac_block_residual_8x8_rd_c( x264_t *h, vbench_cabac_t *cb, int ctx_block_cat, dctcoef *l );
void x264_cabac_block_residual_rd_c( x264_t *h, vbench_cabac_t *cb, int ctx_block_cat, dctcoef *l );
#endif

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

#define report( name ) { \
    if( used_asm ) \
        fprintf( stderr, " - %-21s [%s]\n", name, ok ? "OK" : "FAILED" ); \
    if( !ok ) ret = -1; \
}

extern const uint8_t asm_count_cat_m1[14];

int check_cabac( int cpu_ref, int cpu_new )
{
    int ret = 0, ok = 1, used_asm = 0;
    int i_chroma_format_idc = 3;
    vbench_bitstream_function_t bs_ref;
    vbench_bitstream_function_t bs_a;
    vbench_quant_function_t     quantf;
    vbench_bitstream_init( cpu_ref, &bs_ref );
    vbench_bitstream_init( cpu_new, &bs_a );
    vbench_quant_init( 0, cpu_new, &quantf );
    quantf.coeff_last[DCT_CHROMA_DC] = quantf.coeff_last4;

#define CABAC_RESIDUAL(name, start, end, rd)\
{\
    if( bs_a.name##_internal && (bs_a.name##_internal != bs_ref.name##_internal || (cpu_new&CPU_SSE2_IS_SLOW)) )\
    {\
        used_asm = 1;\
        set_func_name( #name );\
        for( int i = 0; i < 2; i++ )\
        {\
            for( intptr_t ctx_block_cat = start; ctx_block_cat <= end; ctx_block_cat++ )\
            {\
                for( int j = 0; j < 256; j++ )\
                {\
                    ALIGNED_ARRAY_N( dctcoef, dct, [2],[64] );\
                    uint8_t bitstream[2][1<<16];\
                    static const uint8_t ctx_ac[14] = {0,1,0,0,1,0,0,1,0,0,0,1,0,0};\
                    int ac = ctx_ac[ctx_block_cat];\
                    int nz = 0;\
                    while( !nz )\
                    {\
                        for( int k = 0; k <= asm_count_cat_m1[ctx_block_cat]; k++ )\
                        {\
                            /* Very rough distribution that covers possible inputs */\
                            int rnd = rand();\
                            int coef = !(rnd&3);\
                            coef += !(rnd&  15) * (rand()&0x0006);\
                            coef += !(rnd&  63) * (rand()&0x0008);\
                            coef += !(rnd& 255) * (rand()&0x00F0);\
                            coef += !(rnd&1023) * (rand()&0x7F00);\
                            nz |= dct[0][ac+k] = dct[1][ac+k] = coef * ((rand()&1) ? 1 : -1);\
                        }\
                    }\
                    int b_interlaced = i;\
                    vbench_cabac_t cb[2];\
                    vbench_cabac_context_init( &cb[0], SLICE_TYPE_P, 26, 0 );\
                    vbench_cabac_context_init( &cb[1], SLICE_TYPE_P, 26, 0 );\
                    vbench_cabac_encode_init( &cb[0], bitstream[0], bitstream[0]+0xfff0 );\
                    vbench_cabac_encode_init( &cb[1], bitstream[1], bitstream[1]+0xfff0 );\
                    cb[0].f8_bits_encoded = 0;\
                    cb[1].f8_bits_encoded = 0;\
                    if( !rd ) memcpy( bitstream[1], bitstream[0], 0x400 );\
                    call_c1( vbench_##name##_c, &quantf, &cb[0], ctx_block_cat, dct[0]+ac,i);\
                    call_a1( bs_a.name##_internal, dct[1]+ac, i, ctx_block_cat, &cb[1] );\
                    ok = cb[0].f8_bits_encoded == cb[1].f8_bits_encoded && !memcmp(cb[0].state, cb[1].state, 1024);\
                    if( !rd ) ok |= !memcmp( bitstream[1], bitstream[0], 0x400 ) && !memcmp( &cb[1], &cb[0], offsetof(vbench_cabac_t, p_start) );\
                    if( !ok )\
                    {\
                        fprintf( stderr, #name " :  [FAILED] ctx_block_cat %d", (int)ctx_block_cat );\
                        if( rd && cb[0].f8_bits_encoded != cb[1].f8_bits_encoded )\
                            fprintf( stderr, " (%d != %d)", cb[0].f8_bits_encoded, cb[1].f8_bits_encoded );\
                        fprintf( stderr, "\n");\
                        goto name##fail;\
                    }\
                    if( (j&15) == 0 )\
                    {\
                        call_c2( vbench_##name##_c, &quantf, &cb[0], ctx_block_cat, dct[0]+ac, i );\
                        call_a2( bs_a.name##_internal, dct[1]+ac, i, ctx_block_cat, &cb[1] );\
                    }\
                }\
            }\
        }\
    }\
}\
name##fail:


//static ALWAYS_INLINE void vbench_cabac_block_residual( vbench_bitstream_function_t *bsf, vbench_quant_function_t *quantf,  vbench_cabac_t *cb, int ctx_block_cat, dctcoef *l, int interlaced)
//    CABAC_RESIDUAL( cabac_block_residual, 0, DCT_LUMA_8x8, 0 )
//    report( "cabac residual:" );

    ok = 1; used_asm = 0;
    CABAC_RESIDUAL( cabac_block_residual_rd, 0, DCT_LUMA_8x8-1, 1 )
    CABAC_RESIDUAL( cabac_block_residual_8x8_rd, DCT_LUMA_8x8, DCT_LUMA_8x8, 1 )
    report( "cabac residual rd:" );

    if( cpu_ref || run_cabac_decision_c == run_cabac_decision_asm )
        return ret;
#if 0
    ok = 1; used_asm = 0;
    x264_cabac_init( &h );

    set_func_name( "cabac_encode_decision" );
    memcpy( buf4, buf3, 0x1000 );
    call_c( run_cabac_decision_c, &h, buf3 );
    call_a( run_cabac_decision_asm, &h, buf4 );
    ok = !memcmp( buf3, buf4, 0x1000 );
    report( "cabac decision:" );

    set_func_name( "cabac_encode_bypass" );
    memcpy( buf4, buf3, 0x1000 );
    call_c( run_cabac_bypass_c, &h, buf3 );
    call_a( run_cabac_bypass_asm, &h, buf4 );
    ok = !memcmp( buf3, buf4, 0x1000 );
    report( "cabac bypass:" );

    set_func_name( "cabac_encode_terminal" );
    memcpy( buf4, buf3, 0x1000 );
    call_c( run_cabac_terminal_c, &h, buf3 );
    call_a( run_cabac_terminal_asm, &h, buf4 );
    ok = !memcmp( buf3, buf4, 0x1000 );
    report( "cabac terminal:" );

#endif
    return ret;
}
