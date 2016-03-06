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

static inline uint32_t read_time(void)
{
    uint32_t a = 0;
#if HAVE_X86_INLINE_ASM
    asm volatile( "lfence \n"
                  "rdtsc  \n"
                  : "=a"(a) :: "edx", "memory" );
#elif ARCH_PPC
    asm volatile( "mftb %0" : "=r"(a) :: "memory" );
#elif ARCH_ARM     // ARMv7 only
    asm volatile( "mrc p15, 0, %0, c9, c13, 0" : "=r"(a) :: "memory" );
#elif ARCH_AARCH64
    uint64_t b = 0;
    asm volatile( "mrs %0, pmccntr_el0" : "=r"(b) :: "memory" );
    a = b;
#elif ARCH_MIPS
    asm volatile( "rdhwr %0, $2" : "=r"(a) :: "memory" );
#endif
    return a;
}


//void x264_cabac_encode_ue_bypass( x264_cabac_t *cb, int exp_bits, int val )
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

#if 0
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

static int cmp_nop( const void *a, const void *b )
{
    return *(uint16_t*)a - *(uint16_t*)b;
}

static int cmp_bench( const void *a, const void *b )
{
    // asciibetical sort except preserving numbers
    const char *sa = ((bench_func_t*)a)->name;
    const char *sb = ((bench_func_t*)b)->name;
    for( ;; sa++, sb++ )
    {
        if( !*sa && !*sb )
            return 0;
        if( isdigit( *sa ) && isdigit( *sb ) && isdigit( sa[1] ) != isdigit( sb[1] ) )
            return isdigit( sa[1] ) - isdigit( sb[1] );
        if( *sa != *sb )
            return *sa - *sb;
    }
}

#if ARCH_X86 || ARCH_X86_64
int x264_stack_pagealign( int (*func)(), int align );

/* detect when callee-saved regs aren't saved
 * needs an explicit asm check because it only sometimes crashes in normal use. */
intptr_t x264_checkasm_call( intptr_t (*func)(), int *ok, ... );
#else
#define x264_stack_pagealign( func, align ) func()
#endif

#if ARCH_AARCH64
intptr_t x264_checkasm_call( intptr_t (*func)(), int *ok, ... );
#endif

#if ARCH_ARM
intptr_t x264_checkasm_call_neon( intptr_t (*func)(), int *ok, ... );
intptr_t x264_checkasm_call_noneon( intptr_t (*func)(), int *ok, ... );
intptr_t (*x264_checkasm_call)( intptr_t (*func)(), int *ok, ... ) = x264_checkasm_call_noneon;
#endif

#define call_c1(func,...) func(__VA_ARGS__)

#if ARCH_X86_64
/* Evil hack: detect incorrect assumptions that 32-bit ints are zero-extended to 64-bit.
 * This is done by clobbering the stack with junk around the stack pointer and calling the
 * assembly function through x264_checkasm_call with added dummy arguments which forces all
 * real arguments to be passed on the stack and not in registers. For 32-bit argument the
 * upper half of the 64-bit register location on the stack will now contain junk. Note that
 * this is dependant on compiler behaviour and that interrupts etc. at the wrong time may
 * overwrite the junk written to the stack so there's no guarantee that it will always
 * detect all functions that assumes zero-extension.
 */
void x264_checkasm_stack_clobber( uint64_t clobber, ... );
#define call_a1(func,...) ({ \
    uint64_t r = (rand() & 0xffff) * 0x0001000100010001ULL; \
    x264_checkasm_stack_clobber( r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r,r ); /* max_args+6 */ \
    x264_checkasm_call(( intptr_t(*)())func, &ok, 0, 0, 0, 0, __VA_ARGS__ ); })
#elif ARCH_X86 || (ARCH_AARCH64 && !defined(__APPLE__)) || ARCH_ARM
#define call_a1(func,...) x264_checkasm_call( (intptr_t(*)())func, &ok, __VA_ARGS__ )
#else
#define call_a1 call_c1
#endif

#if ARCH_ARM
#define call_a1_64(func,...) ((uint64_t (*)(intptr_t(*)(), int*, ...))x264_checkasm_call)( (intptr_t(*)())func, &ok, __VA_ARGS__ )
#else
#define call_a1_64 call_a1
#endif

#define call_bench(func,cpu,...)\
    if( do_bench && !strncmp(func_name, bench_pattern, bench_pattern_len) )\
    {\
        uint64_t tsum = 0;\
        int tcount = 0;\
        call_a1(func, __VA_ARGS__);\
        for( int ti = 0; ti < (cpu?BENCH_RUNS:BENCH_RUNS/4); ti++ )\
        {\
            uint32_t t = read_time();\
            func(__VA_ARGS__);\
            func(__VA_ARGS__);\
            func(__VA_ARGS__);\
            func(__VA_ARGS__);\
            t = read_time() - t;\
            if( (uint64_t)t*tcount <= tsum*4 && ti > 0 )\
            {\
                tsum += t;\
                tcount++;\
            }\
        }\
        bench_t *b = get_bench( func_name, cpu );\
        b->cycles += tsum;\
        b->den += tcount;\
        b->pointer = func;\
    }

/* for most functions, run benchmark and correctness test at the same time.
 * for those that modify their inputs, run the above macros separately */
#define call_a(func,...) ({ call_a2(func,__VA_ARGS__); call_a1(func,__VA_ARGS__); })
#define call_c(func,...) ({ call_c2(func,__VA_ARGS__); call_c1(func,__VA_ARGS__); })
#define call_a2(func,...) ({ call_bench(func,cpu_new,__VA_ARGS__); })
#define call_c2(func,...) ({ call_bench(func,0,__VA_ARGS__); })
#define call_a64(func,...) ({ call_a2(func,__VA_ARGS__); call_a1_64(func,__VA_ARGS__); })

#define DECL_CABAC(cpu) \
static void run_cabac_decision_##cpu( x264_t *h, uint8_t *dst )\
{\
    x264_cabac_t cb;\
    x264_cabac_context_init( h, &cb, SLICE_TYPE_P, 26, 0 );\
    x264_cabac_encode_init( &cb, dst, dst+0xff0 );\
    for( int i = 0; i < 0x1000; i++ )\
        x264_cabac_encode_decision_##cpu( &cb, buf1[i]>>1, buf1[i]&1 );\
}\
static void run_cabac_bypass_##cpu( x264_t *h, uint8_t *dst )\
{\
    x264_cabac_t cb;\
    x264_cabac_context_init( h, &cb, SLICE_TYPE_P, 26, 0 );\
    x264_cabac_encode_init( &cb, dst, dst+0xff0 );\
    for( int i = 0; i < 0x1000; i++ )\
        x264_cabac_encode_bypass_##cpu( &cb, buf1[i]&1 );\
}\
static void run_cabac_terminal_##cpu( x264_t *h, uint8_t *dst )\
{\
    x264_cabac_t cb;\
    x264_cabac_context_init( h, &cb, SLICE_TYPE_P, 26, 0 );\
    x264_cabac_encode_init( &cb, dst, dst+0xff0 );\
    for( int i = 0; i < 0x1000; i++ )\
        x264_cabac_encode_terminal_##cpu( &cb );\
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

void x264_cabac_block_residual_c( x264_t *h, x264_cabac_t *cb, int ctx_block_cat, dctcoef *l );
void x264_cabac_block_residual_8x8_rd_c( x264_t *h, x264_cabac_t *cb, int ctx_block_cat, dctcoef *l );
void x264_cabac_block_residual_rd_c( x264_t *h, x264_cabac_t *cb, int ctx_block_cat, dctcoef *l );
#endif
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
                    call_c1( vbench_##name##_c, &bs_a, &quantf, &cb[0], ctx_block_cat, dct[0]+ac, i );\
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
                        call_c2( vbench_##name##_c,  &bs_a, &quantf, &cb[0], ctx_block_cat, dct[0]+ac, i );\
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

#if 0
    ok = 1; used_asm = 0;
    CABAC_RESIDUAL( cabac_block_residual_rd, 0, DCT_LUMA_8x8-1, 1 )
    CABAC_RESIDUAL( cabac_block_residual_8x8_rd, DCT_LUMA_8x8, DCT_LUMA_8x8, 1 )
    report( "cabac residual rd:" );

    if( cpu_ref || run_cabac_decision_c == run_cabac_decision_asm )
        return ret;
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
