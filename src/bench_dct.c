/*****************************************************************************
 * bench_dct.c: run and measure the kernels of dct.
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
uint8_t *buf1, *buf2;
/* buf3, buf4: used to store output */
uint8_t *buf3, *buf4;
/* pbuf1, pbuf2: initialised to random pixel data and shouldn't write into them. */
pixel *pbuf1, *pbuf2;
/* pbuf3, pbuf4: point to buf3, buf4, just for type convenience */
pixel *pbuf3, *pbuf4;

int quiet = 0;

#define report( name ) { \
    if( used_asm && !quiet ) \
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

static void print_bench(void)
{
    uint16_t nops[10000];
    int nfuncs, nop_time=0;

    for( int i = 0; i < 10000; i++ )
    {
        uint32_t t = read_time();
        nops[i] = read_time() - t;
    }
    qsort( nops, 10000, sizeof(uint16_t), cmp_nop );
    for( int i = 500; i < 9500; i++ )
        nop_time += nops[i];
    nop_time /= 900;
    printf( "nop: %d\n", nop_time );

    for( nfuncs = 0; nfuncs < MAX_FUNCS && benchs[nfuncs].name; nfuncs++ );
    qsort( benchs, nfuncs, sizeof(bench_func_t), cmp_bench );
    for( int i = 0; i < nfuncs; i++ )
        for( int j = 0; j < MAX_CPUS && (!j || benchs[i].vers[j].cpu); j++ )
        {
            int k;
            bench_t *b = &benchs[i].vers[j];
            if( !b->den )
                continue;
            for( k = 0; k < j && benchs[i].vers[k].pointer != b->pointer; k++ );
            if( k < j )
                continue;
            printf( "%s_%s%s: %"PRId64"\n", benchs[i].name,
#if HAVE_MMX
                    b->cpu&CPU_AVX2 ? "avx2" :
                    b->cpu&CPU_FMA3 ? "fma3" :
                    b->cpu&CPU_FMA4 ? "fma4" :
                    b->cpu&CPU_XOP ? "xop" :
                    b->cpu&CPU_AVX ? "avx" :
                    b->cpu&CPU_SSE42 ? "sse42" :
                    b->cpu&CPU_SSE4 ? "sse4" :
                    b->cpu&CPU_SSSE3 ? "ssse3" :
                    b->cpu&CPU_SSE3 ? "sse3" :
                    /* print sse2slow only if there's also a sse2fast version of the same func */
                    b->cpu&CPU_SSE2_IS_SLOW && j<MAX_CPUS-1 && b[1].cpu&CPU_SSE2_IS_FAST && !(b[1].cpu&CPU_SSE3) ? "sse2slow" :
                    b->cpu&CPU_SSE2 ? "sse2" :
                    b->cpu&CPU_SSE ? "sse" :
                    b->cpu&CPU_MMX ? "mmx" :
#elif ARCH_PPC
                    b->cpu&CPU_ALTIVEC ? "altivec" :
#elif ARCH_ARM
                    b->cpu&CPU_NEON ? "neon" :
                    b->cpu&CPU_ARMV6 ? "armv6" :
#elif ARCH_AARCH64
                    b->cpu&CPU_NEON ? "neon" :
                    b->cpu&CPU_ARMV8 ? "armv8" :
#elif ARCH_MIPS
                    b->cpu&CPU_MSA ? "msa" :
#endif
                    "c",
#if HAVE_MMX
                    b->cpu&CPU_CACHELINE_32 ? "_c32" :
                    b->cpu&CPU_SLOW_ATOM && b->cpu&CPU_CACHELINE_64 ? "_c64_atom" :
                    b->cpu&CPU_CACHELINE_64 ? "_c64" :
                    b->cpu&CPU_SLOW_SHUFFLE ? "_slowshuffle" :
                    b->cpu&CPU_LZCNT ? "_lzcnt" :
                    b->cpu&CPU_BMI2 ? "_bmi2" :
                    b->cpu&CPU_BMI1 ? "_bmi1" :
                    b->cpu&CPU_SLOW_CTZ ? "_slow_ctz" :
                    b->cpu&CPU_SLOW_ATOM ? "_atom" :
#elif ARCH_ARM
                    b->cpu&CPU_FAST_NEON_MRC ? "_fast_mrc" :
#endif
                    "",
                    (int64_t)(10*b->cycles/b->den - nop_time)/4 );
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

static int check_dct( int cpu_ref, int cpu_new )
{
    vbench_dct_function_t dct_c;
    vbench_dct_function_t dct_ref;
    vbench_dct_function_t dct_asm;
    vbench_quant_function_t qf;
    int ret = 0, ok, used_asm, interlace = 0;
    ALIGNED_ARRAY_N( dctcoef, dct1, [16],[16] );
    ALIGNED_ARRAY_N( dctcoef, dct2, [16],[16] );
    ALIGNED_ARRAY_N( dctcoef, dct4, [16],[16] );
    ALIGNED_ARRAY_N( dctcoef, dct8, [4],[64] );
    ALIGNED_16( dctcoef dctdc[2][8] );
    x264_t h_buf;
    x264_t *h = &h_buf;

    vbench_dct_init( 0, &dct_c );
    vbench_dct_init( cpu_ref, &dct_ref);
    vbench_dct_init( cpu_new, &dct_asm );

    memset( h, 0, sizeof(*h) );
    x264_param_default( &h->param );
    h->sps->i_chroma_format_idc = 1;
    h->chroma_qp_table = i_chroma_qp_table + 12;
    h->param.analyse.i_luma_deadzone[0] = 0;
    h->param.analyse.i_luma_deadzone[1] = 0;
    h->param.analyse.b_transform_8x8 = 1;
    for( int i = 0; i < 6; i++ )
        h->pps->scaling_list[i] = x264_cqm_flat16;
    x264_cqm_init( h );
    vbench_quant_init( h, 0, &qf );

    /* overflow test cases */
    for( int i = 0; i < 5; i++ )
    {
        pixel *enc = &pbuf3[16*i*FENC_STRIDE];
        pixel *dec = &pbuf4[16*i*FDEC_STRIDE];

        for( int j = 0; j < 16; j++ )
        {
            int cond_a = (i < 2) ? 1 : ((j&3) == 0 || (j&3) == (i-1));
            int cond_b = (i == 0) ? 1 : !cond_a;
            enc[0] = enc[1] = enc[4] = enc[5] = enc[8] = enc[9] = enc[12] = enc[13] = cond_a ? PIXEL_MAX : 0;
            enc[2] = enc[3] = enc[6] = enc[7] = enc[10] = enc[11] = enc[14] = enc[15] = cond_b ? PIXEL_MAX : 0;

            for( int k = 0; k < 4; k++ )
                dec[k] = PIXEL_MAX - enc[k];

            enc += FENC_STRIDE;
            dec += FDEC_STRIDE;
        }
    }

#define TEST_DCT( name, t1, t2, size ) \
    if( dct_asm.name != dct_ref.name ) \
    { \
        set_func_name( #name ); \
        used_asm = 1; \
        pixel *enc = pbuf3; \
        pixel *dec = pbuf4; \
        for( int j = 0; j < 5; j++) \
        { \
            call_c( dct_c.name, t1, &pbuf1[j*64], &pbuf2[j*64] ); \
            call_a( dct_asm.name, t2, &pbuf1[j*64], &pbuf2[j*64] ); \
            if( memcmp( t1, t2, size*sizeof(dctcoef) ) ) \
            { \
                ok = 0; \
                fprintf( stderr, #name " [FAILED]\n" ); \
                for( int k = 0; k < size; k++ )\
                    printf( "%d ", ((dctcoef*)t1)[k] );\
                printf("\n");\
                for( int k = 0; k < size; k++ )\
                    printf( "%d ", ((dctcoef*)t2)[k] );\
                printf("\n");\
                break; \
            } \
            call_c( dct_c.name, t1, enc, dec ); \
            call_a( dct_asm.name, t2, enc, dec ); \
            if( memcmp( t1, t2, size*sizeof(dctcoef) ) ) \
            { \
                ok = 0; \
                fprintf( stderr, #name " [FAILED] (overflow)\n" ); \
                break; \
            } \
            enc += 16*FENC_STRIDE; \
            dec += 16*FDEC_STRIDE; \
        } \
    }
    ok = 1; used_asm = 0;
    TEST_DCT( sub4x4_dct, dct1[0], dct2[0], 16 );
    TEST_DCT( sub8x8_dct, dct1, dct2, 16*4 );
    TEST_DCT( sub8x8_dct_dc, dctdc[0], dctdc[1], 4 );
    TEST_DCT( sub8x16_dct_dc, dctdc[0], dctdc[1], 8 );
    TEST_DCT( sub16x16_dct, dct1, dct2, 16*16 );
    report( "sub_dct4 :" );

    ok = 1; used_asm = 0;
    TEST_DCT( sub8x8_dct8, (void*)dct1[0], (void*)dct2[0], 64 );
    TEST_DCT( sub16x16_dct8, (void*)dct1, (void*)dct2, 64*4 );
    report( "sub_dct8 :" );
#undef TEST_DCT

    // fdct and idct are denormalized by different factors, so quant/dequant
    // is needed to force the coefs into the right range.
    dct_c.sub16x16_dct( dct4, pbuf1, pbuf2 );
    dct_c.sub16x16_dct8( dct8, pbuf1, pbuf2 );
    for( int i = 0; i < 16; i++ )
    {
        qf.quant_4x4( dct4[i], h->quant4_mf[CQM_4IY][20], h->quant4_bias[CQM_4IY][20] );
        qf.dequant_4x4( dct4[i], h->dequant4_mf[CQM_4IY], 20 );
    }
    for( int i = 0; i < 4; i++ )
    {
        qf.quant_8x8( dct8[i], h->quant8_mf[CQM_8IY][20], h->quant8_bias[CQM_8IY][20] );
        qf.dequant_8x8( dct8[i], h->dequant8_mf[CQM_8IY], 20 );
    }
    x264_cqm_delete( h );

#define TEST_IDCT( name, src ) \
    if( dct_asm.name != dct_ref.name ) \
    { \
        set_func_name( #name ); \
        used_asm = 1; \
        memcpy( pbuf3, pbuf1, 32*32 * sizeof(pixel) ); \
        memcpy( pbuf4, pbuf1, 32*32 * sizeof(pixel) ); \
        memcpy( dct1, src, 256 * sizeof(dctcoef) ); \
        memcpy( dct2, src, 256 * sizeof(dctcoef) ); \
        call_c1( dct_c.name, pbuf3, (void*)dct1 ); \
        call_a1( dct_asm.name, pbuf4, (void*)dct2 ); \
        if( memcmp( pbuf3, pbuf4, 32*32 * sizeof(pixel) ) ) \
        { \
            ok = 0; \
            fprintf( stderr, #name " [FAILED]\n" ); \
        } \
        call_c2( dct_c.name, pbuf3, (void*)dct1 ); \
        call_a2( dct_asm.name, pbuf4, (void*)dct2 ); \
    }
    ok = 1; used_asm = 0;
    TEST_IDCT( add4x4_idct, dct4 );
    TEST_IDCT( add8x8_idct, dct4 );
    TEST_IDCT( add8x8_idct_dc, dct4 );
    TEST_IDCT( add16x16_idct, dct4 );
    TEST_IDCT( add16x16_idct_dc, dct4 );
    report( "add_idct4 :" );

    ok = 1; used_asm = 0;
    TEST_IDCT( add8x8_idct8, dct8 );
    TEST_IDCT( add16x16_idct8, dct8 );
    report( "add_idct8 :" );
#undef TEST_IDCT

#define TEST_DCTDC( name )\
    ok = 1; used_asm = 0;\
    if( dct_asm.name != dct_ref.name )\
    {\
        set_func_name( #name );\
        used_asm = 1;\
        uint16_t *p = (uint16_t*)buf1;\
        for( int i = 0; i < 16 && ok; i++ )\
        {\
            for( int j = 0; j < 16; j++ )\
                dct1[0][j] = !i ? (j^j>>1^j>>2^j>>3)&1 ? PIXEL_MAX*16 : -PIXEL_MAX*16 /* max dc */\
                           : i<8 ? (*p++)&1 ? PIXEL_MAX*16 : -PIXEL_MAX*16 /* max elements */\
                           : ((*p++)&0x1fff)-0x1000; /* general case */\
            memcpy( dct2, dct1, 16 * sizeof(dctcoef) );\
            call_c1( dct_c.name, dct1[0] );\
            call_a1( dct_asm.name, dct2[0] );\
            if( memcmp( dct1, dct2, 16 * sizeof(dctcoef) ) )\
                ok = 0;\
        }\
        call_c2( dct_c.name, dct1[0] );\
        call_a2( dct_asm.name, dct2[0] );\
    }\
    report( #name " :" );

    TEST_DCTDC(  dct4x4dc );
    TEST_DCTDC( idct4x4dc );
#undef TEST_DCTDC

#define TEST_DCTDC_CHROMA( name )\
    ok = 1; used_asm = 0;\
    if( dct_asm.name != dct_ref.name )\
    {\
        set_func_name( #name );\
        used_asm = 1;\
        uint16_t *p = (uint16_t*)buf1;\
        for( int i = 0; i < 16 && ok; i++ )\
        {\
            for( int j = 0; j < 8; j++ )\
                dct1[j][0] = !i ? (j^j>>1^j>>2)&1 ? PIXEL_MAX*16 : -PIXEL_MAX*16 /* max dc */\
                           : i<8 ? (*p++)&1 ? PIXEL_MAX*16 : -PIXEL_MAX*16 /* max elements */\
                           : ((*p++)&0x1fff)-0x1000; /* general case */\
            memcpy( dct2, dct1, 8*16 * sizeof(dctcoef) );\
            call_c1( dct_c.name, dctdc[0], dct1 );\
            call_a1( dct_asm.name, dctdc[1], dct2 );\
            if( memcmp( dctdc[0], dctdc[1], 8 * sizeof(dctcoef) ) || memcmp( dct1, dct2, 8*16 * sizeof(dctcoef) ) )\
            {\
                ok = 0;\
                fprintf( stderr, #name " [FAILED]\n" ); \
            }\
        }\
        call_c2( dct_c.name, dctdc[0], dct1 );\
        call_a2( dct_asm.name, dctdc[1], dct2 );\
    }\
    report( #name " :" );

    TEST_DCTDC_CHROMA( dct2x4dc );
#undef TEST_DCTDC_CHROMA

    x264_zigzag_function_t zigzag_c[2];
    x264_zigzag_function_t zigzag_ref[2];
    x264_zigzag_function_t zigzag_asm[2];

    ALIGNED_16( dctcoef level1[64] );
    ALIGNED_16( dctcoef level2[64] );

#define TEST_ZIGZAG_SCAN( name, t1, t2, dct, size ) \
    if( zigzag_asm[interlace].name != zigzag_ref[interlace].name ) \
    { \
        set_func_name( "zigzag_"#name"_%s", interlace?"field":"frame" ); \
        used_asm = 1; \
        for( int i = 0; i < size*size; i++ ) \
            dct[i] = i; \
        call_c( zigzag_c[interlace].name, t1, dct ); \
        call_a( zigzag_asm[interlace].name, t2, dct ); \
        if( memcmp( t1, t2, size*size*sizeof(dctcoef) ) ) \
        { \
            ok = 0; \
            for( int i = 0; i < 2; i++ ) \
            { \
                dctcoef *d = (dctcoef*)(i ? t2 : t1); \
                for( int j = 0; j < size; j++ ) \
                { \
                    for( int k = 0; k < size; k++ ) \
                        fprintf( stderr, "%2d ", d[k+j*8] ); \
                    fprintf( stderr, "\n" ); \
                } \
                fprintf( stderr, "\n" ); \
            } \
            fprintf( stderr, #name " [FAILED]\n" ); \
        } \
    }

#define TEST_ZIGZAG_SUB( name, t1, t2, size ) \
    if( zigzag_asm[interlace].name != zigzag_ref[interlace].name ) \
    { \
        int nz_a, nz_c; \
        set_func_name( "zigzag_"#name"_%s", interlace?"field":"frame" ); \
        used_asm = 1; \
        memcpy( pbuf3, pbuf1, 16*FDEC_STRIDE * sizeof(pixel) ); \
        memcpy( pbuf4, pbuf1, 16*FDEC_STRIDE * sizeof(pixel) ); \
        nz_c = call_c1( zigzag_c[interlace].name, t1, pbuf2, pbuf3 ); \
        nz_a = call_a1( zigzag_asm[interlace].name, t2, pbuf2, pbuf4 ); \
        if( memcmp( t1, t2, size*sizeof(dctcoef) ) || memcmp( pbuf3, pbuf4, 16*FDEC_STRIDE*sizeof(pixel) ) || nz_c != nz_a ) \
        { \
            ok = 0; \
            fprintf( stderr, #name " [FAILED]\n" ); \
        } \
        call_c2( zigzag_c[interlace].name, t1, pbuf2, pbuf3 ); \
        call_a2( zigzag_asm[interlace].name, t2, pbuf2, pbuf4 ); \
    }

#define TEST_ZIGZAG_SUBAC( name, t1, t2 ) \
    if( zigzag_asm[interlace].name != zigzag_ref[interlace].name ) \
    { \
        int nz_a, nz_c; \
        dctcoef dc_a, dc_c; \
        set_func_name( "zigzag_"#name"_%s", interlace?"field":"frame" ); \
        used_asm = 1; \
        for( int i = 0; i < 2; i++ ) \
        { \
            memcpy( pbuf3, pbuf2, 16*FDEC_STRIDE * sizeof(pixel) ); \
            memcpy( pbuf4, pbuf2, 16*FDEC_STRIDE * sizeof(pixel) ); \
            for( int j = 0; j < 4; j++ ) \
            { \
                memcpy( pbuf3 + j*FDEC_STRIDE, (i?pbuf1:pbuf2) + j*FENC_STRIDE, 4 * sizeof(pixel) ); \
                memcpy( pbuf4 + j*FDEC_STRIDE, (i?pbuf1:pbuf2) + j*FENC_STRIDE, 4 * sizeof(pixel) ); \
            } \
            nz_c = call_c1( zigzag_c[interlace].name, t1, pbuf2, pbuf3, &dc_c ); \
            nz_a = call_a1( zigzag_asm[interlace].name, t2, pbuf2, pbuf4, &dc_a ); \
            if( memcmp( t1+1, t2+1, 15*sizeof(dctcoef) ) || memcmp( pbuf3, pbuf4, 16*FDEC_STRIDE * sizeof(pixel) ) || nz_c != nz_a || dc_c != dc_a ) \
            { \
                ok = 0; \
                fprintf( stderr, #name " [FAILED]\n" ); \
                break; \
            } \
        } \
        call_c2( zigzag_c[interlace].name, t1, pbuf2, pbuf3, &dc_c ); \
        call_a2( zigzag_asm[interlace].name, t2, pbuf2, pbuf4, &dc_a ); \
    }

#define TEST_INTERLEAVE( name, t1, t2, dct, size ) \
    if( zigzag_asm[interlace].name != zigzag_ref[interlace].name ) \
    { \
        for( int j = 0; j < 100; j++ ) \
        { \
            set_func_name( "zigzag_"#name"_%s", interlace?"field":"frame" ); \
            used_asm = 1; \
            memcpy(dct, buf1, size*sizeof(dctcoef)); \
            for( int i = 0; i < size; i++ ) \
                dct[i] = rand()&0x1F ? 0 : dct[i]; \
            memcpy(buf3, buf4, 10); \
            call_c( zigzag_c[interlace].name, t1, dct, buf3 ); \
            call_a( zigzag_asm[interlace].name, t2, dct, buf4 ); \
            if( memcmp( t1, t2, size*sizeof(dctcoef) ) || memcmp( buf3, buf4, 10 ) ) \
            { \
                ok = 0; printf("%d: %d %d %d %d\n%d %d %d %d\n\n",memcmp( t1, t2, size*sizeof(dctcoef) ),buf3[0], buf3[1], buf3[8], buf3[9], buf4[0], buf4[1], buf4[8], buf4[9]);break;\
            } \
        } \
    }

    x264_zigzag_init( 0, &zigzag_c[0], &zigzag_c[1] );
    x264_zigzag_init( cpu_ref, &zigzag_ref[0], &zigzag_ref[1] );
    x264_zigzag_init( cpu_new, &zigzag_asm[0], &zigzag_asm[1] );

    ok = 1; used_asm = 0;
    TEST_INTERLEAVE( interleave_8x8_cavlc, level1, level2, dct8[0], 64 );
    report( "zigzag_interleave :" );

    for( interlace = 0; interlace <= 1; interlace++ )
    {
        ok = 1; used_asm = 0;
        TEST_ZIGZAG_SCAN( scan_8x8, level1, level2, dct8[0], 8 );
        TEST_ZIGZAG_SCAN( scan_4x4, level1, level2, dct1[0], 4 );
        TEST_ZIGZAG_SUB( sub_4x4, level1, level2, 16 );
        TEST_ZIGZAG_SUB( sub_8x8, level1, level2, 64 );
        TEST_ZIGZAG_SUBAC( sub_4x4ac, level1, level2 );
        report( interlace ? "zigzag_field :" : "zigzag_frame :" );
    }
#undef TEST_ZIGZAG_SCAN
#undef TEST_ZIGZAG_SUB

    return ret;
}

int run_benchmarks(int i){
    /* 32-byte alignment is guaranteed whenever it's useful, 
     * but some functions also vary in speed depending on %64 */
    return x264_stack_pagealign(check_all_flags, i*32 );
}

