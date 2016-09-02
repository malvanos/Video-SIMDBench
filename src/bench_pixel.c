/*****************************************************************************
 * bench_pixel.c: run and measure the kernels
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
#include "macroblock.h"

#include "c_kernels/pixel.h"
#include "c_kernels/predict.h"

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
int vbench_stack_pagealign( int (*func)(), int align );

/* detect when callee-saved regs aren't saved
 * needs an explicit asm check because it only sometimes crashes in normal use. */
intptr_t vbench_checkasm_call( intptr_t (*func)(), int *ok, ... );
#else
#define vbench_stack_pagealign( func, align ) func()
#endif

#if ARCH_AARCH64
intptr_t vbench_checkasm_call( intptr_t (*func)(), int *ok, ... );
#endif

#if ARCH_ARM
intptr_t vbench_checkasm_call_neon( intptr_t (*func)(), int *ok, ... );
intptr_t vbench_checkasm_call_noneon( intptr_t (*func)(), int *ok, ... );
intptr_t (*vbench_checkasm_call)( intptr_t (*func)(), int *ok, ... ) = vbench_checkasm_call_noneon;
#endif

#define call_c1(func,...) func(__VA_ARGS__)


/****************************************************************************
 * vbench_pixel_init:
 ****************************************************************************/
void vbench_pixel_init( int cpu, vbench_pixel_function_t *pixf )
{
    memset( pixf, 0, sizeof(*pixf) );

#define INIT2_NAME( name1, name2, cpu, prefix ) \
    pixf->name1[PIXEL_16x16] = prefix##pixel_##name2##_16x16##cpu;\
    pixf->name1[PIXEL_16x8]  = prefix##pixel_##name2##_16x8##cpu;
#define INIT4_NAME( name1, name2, cpu, prefix  ) \
    INIT2_NAME( name1, name2, cpu, prefix  ) \
    pixf->name1[PIXEL_8x16]  = prefix##pixel_##name2##_8x16##cpu;\
    pixf->name1[PIXEL_8x8]   = prefix##pixel_##name2##_8x8##cpu;

#define INIT5_NAME( name1, name2, cpu, prefix  ) \
    INIT4_NAME( name1, name2, cpu, prefix ) \
    pixf->name1[PIXEL_8x4]   = prefix##pixel_##name2##_8x4##cpu;

#define INIT6_NAME( name1, name2, cpu, prefix  ) \
    INIT5_NAME( name1, name2, cpu, prefix  ) \
    pixf->name1[PIXEL_4x8]   = prefix##pixel_##name2##_4x8##cpu;

#define INIT7_NAME( name1, name2, cpu, prefix  ) \
    INIT6_NAME( name1, name2, cpu, prefix  ) \
    pixf->name1[PIXEL_4x4]   = prefix##pixel_##name2##_4x4##cpu;

#define INIT8_NAME( name1, name2, cpu, prefix  ) \
    INIT7_NAME( name1, name2, cpu, prefix ) \
    pixf->name1[PIXEL_4x16]  = prefix##pixel_##name2##_4x16##cpu;

#define INIT2( name, cpu, prefix ) INIT2_NAME( name, name, cpu, prefix )
#define INIT4( name, cpu, prefix ) INIT4_NAME( name, name, cpu, prefix )
#define INIT5( name, cpu, prefix ) INIT5_NAME( name, name, cpu, prefix )
#define INIT6( name, cpu, prefix ) INIT6_NAME( name, name, cpu, prefix )
#define INIT7( name, cpu, prefix ) INIT7_NAME( name, name, cpu, prefix )
#define INIT8( name, cpu, prefix ) INIT8_NAME( name, name, cpu, prefix )

#define INIT_ADS( cpu, prefix ) \
    pixf->ads[PIXEL_16x16] = prefix##pixel_ads4##cpu;\
    pixf->ads[PIXEL_16x8] = prefix##pixel_ads2##cpu;\
    pixf->ads[PIXEL_8x8] = prefix##pixel_ads1##cpu;

    INIT8( sad, , );
    INIT8_NAME( sad_aligned, sad, , );
    INIT7( sad_x3, , );
    INIT7( sad_x4, , );
    INIT8( ssd, , );
    INIT8( satd, , );
    INIT7( satd_x3, , );
    INIT7( satd_x4, , );
    INIT4( hadamard_ac, , );
    INIT_ADS( , );

    pixf->sa8d[PIXEL_16x16] = pixel_sa8d_16x16;
    pixf->sa8d[PIXEL_8x8]   = pixel_sa8d_8x8;
    pixf->var[PIXEL_16x16] = pixel_var_16x16;
    pixf->var[PIXEL_8x16]  = pixel_var_8x16;
    pixf->var[PIXEL_8x8]   = pixel_var_8x8;
    pixf->var2[PIXEL_8x16]  = pixel_var2_8x16;
    pixf->var2[PIXEL_8x8]   = pixel_var2_8x8;

    pixf->ssd_nv12_core = pixel_ssd_nv12_core;
    pixf->ssim_4x4x2_core = ssim_4x4x2_core;
    pixf->ssim_end4 = ssim_end4;
    pixf->vsad = pixel_vsad;
    pixf->asd8 = pixel_asd8;

    pixf->intra_sad_x3_4x4    = intra_sad_x3_4x4;
    pixf->intra_satd_x3_4x4   = intra_satd_x3_4x4;
    pixf->intra_sad_x3_8x8    = intra_sad_x3_8x8;
    pixf->intra_sa8d_x3_8x8   = intra_sa8d_x3_8x8;
    pixf->intra_sad_x3_8x8c   = intra_sad_x3_8x8c;
    pixf->intra_satd_x3_8x8c  = intra_satd_x3_8x8c;
    pixf->intra_sad_x3_8x16c  = intra_sad_x3_8x16c;
    pixf->intra_satd_x3_8x16c = intra_satd_x3_8x16c;
    pixf->intra_sad_x3_16x16  = intra_sad_x3_16x16;
    pixf->intra_satd_x3_16x16 = intra_satd_x3_16x16;

#if HIGH_BIT_DEPTH
#if HAVE_MMX
    if( cpu&CPU_MMX2 )
    {
        INIT7( sad, _mmx2, asm_ );
        INIT7_NAME( sad_aligned, sad, _mmx2, asm_  );
        INIT7( sad_x3, _mmx2, asm_  );
        INIT7( sad_x4, _mmx2, asm_  );
        INIT8( satd, _mmx2, asm_  );
        INIT7( satd_x3, _mmx2, asm_  );
        INIT7( satd_x4, _mmx2, asm_  );
        INIT4( hadamard_ac, _mmx2, asm_  );
        INIT8( ssd, _mmx2, asm_  );
        INIT_ADS( _mmx2, asm_  );

        pixf->ssd_nv12_core = pixel_ssd_nv12_core_mmx2;
        pixf->var[PIXEL_16x16] = pixel_var_16x16_mmx2;
        pixf->var[PIXEL_8x8]   = pixel_var_8x8_mmx2;
#if ARCH_X86
        pixf->var2[PIXEL_8x8]  = pixel_var2_8x8_mmx2;
        pixf->var2[PIXEL_8x16] = pixel_var2_8x16_mmx2;
#endif

        pixf->intra_sad_x3_4x4    = intra_sad_x3_4x4_mmx2;
        pixf->intra_satd_x3_4x4   = intra_satd_x3_4x4_mmx2;
        pixf->intra_sad_x3_8x8    = intra_sad_x3_8x8_mmx2;
        pixf->intra_sad_x3_8x8c   = intra_sad_x3_8x8c_mmx2;
        pixf->intra_satd_x3_8x8c  = intra_satd_x3_8x8c_mmx2;
        pixf->intra_sad_x3_8x16c  = intra_sad_x3_8x16c_mmx2;
        pixf->intra_satd_x3_8x16c = intra_satd_x3_8x16c_mmx2;
        pixf->intra_sad_x3_16x16  = intra_sad_x3_16x16_mmx2;
        pixf->intra_satd_x3_16x16 = intra_satd_x3_16x16_mmx2;
    }
    if( cpu&CPU_SSE2 )
    {
        INIT4_NAME( sad_aligned, sad, _sse2_aligned );
        INIT5( ssd, _sse2 );
        INIT6( satd, _sse2 );
        pixf->satd[PIXEL_4x16] = pixel_satd_4x16_sse2;

        pixf->sa8d[PIXEL_16x16] = pixel_sa8d_16x16_sse2;
        pixf->sa8d[PIXEL_8x8]   = pixel_sa8d_8x8_sse2;
#if ARCH_X86_64
        pixf->intra_sa8d_x3_8x8 = intra_sa8d_x3_8x8_sse2;
        pixf->sa8d_satd[PIXEL_16x16] = pixel_sa8d_satd_16x16_sse2;
#endif
        pixf->intra_sad_x3_4x4  = intra_sad_x3_4x4_sse2;
        pixf->ssd_nv12_core = pixel_ssd_nv12_core_sse2;
        pixf->ssim_4x4x2_core  = pixel_ssim_4x4x2_core_sse2;
        pixf->ssim_end4        = pixel_ssim_end4_sse2;
        pixf->var[PIXEL_16x16] = pixel_var_16x16_sse2;
        pixf->var[PIXEL_8x8]   = pixel_var_8x8_sse2;
        pixf->var2[PIXEL_8x8]  = pixel_var2_8x8_sse2;
        pixf->var2[PIXEL_8x16] = pixel_var2_8x16_sse2;
        pixf->intra_sad_x3_8x8 = intra_sad_x3_8x8_sse2;
    }
    if( (cpu&CPU_SSE2) && !(cpu&CPU_SSE2_IS_SLOW) )
    {
        INIT5( sad, _sse2 );
        INIT2( sad_x3, _sse2 );
        INIT2( sad_x4, _sse2 );
        INIT_ADS( _sse2 );

        if( !(cpu&CPU_STACK_MOD4) )
        {
            INIT4( hadamard_ac, _sse2 );
        }
        pixf->vsad = pixel_vsad_sse2;
        pixf->asd8 = pixel_asd8_sse2;
        pixf->intra_sad_x3_8x8    = intra_sad_x3_8x8_sse2;
        pixf->intra_sad_x3_8x8c   = intra_sad_x3_8x8c_sse2;
        pixf->intra_sad_x3_8x16c  = intra_sad_x3_8x16c_sse2;
        pixf->intra_satd_x3_8x16c = intra_satd_x3_8x16c_sse2;
        pixf->intra_sad_x3_16x16  = intra_sad_x3_16x16_sse2;
    }
    if( cpu&CPU_SSE2_IS_FAST )
    {
        pixf->sad[PIXEL_8x16] = pixel_sad_8x16_sse2;
        pixf->sad_x3[PIXEL_8x16] = pixel_sad_x3_8x16_sse2;
        pixf->sad_x3[PIXEL_8x8]  = pixel_sad_x3_8x8_sse2;
        pixf->sad_x3[PIXEL_8x4]  = pixel_sad_x3_8x4_sse2;
        pixf->sad_x4[PIXEL_8x16] = pixel_sad_x4_8x16_sse2;
        pixf->sad_x4[PIXEL_8x8]  = pixel_sad_x4_8x8_sse2;
        pixf->sad_x4[PIXEL_8x4]  = pixel_sad_x4_8x4_sse2;
    }
    if( cpu&CPU_SSSE3 )
    {
        INIT4_NAME( sad_aligned, sad, _ssse3_aligned );
        pixf->sad_aligned[PIXEL_4x4] = pixel_sad_4x4_ssse3;
        pixf->sad_aligned[PIXEL_4x8] = pixel_sad_4x8_ssse3;
        INIT7( sad, _ssse3 );
        INIT7( sad_x3, _ssse3 );
        INIT7( sad_x4, _ssse3 );
        INIT_ADS( _ssse3 );
        INIT6( satd, _ssse3 );
        pixf->satd[PIXEL_4x16] = pixel_satd_4x16_ssse3;

        if( !(cpu&CPU_STACK_MOD4) )
        {
            INIT4( hadamard_ac, _ssse3 );
        }
        pixf->vsad = pixel_vsad_ssse3;
        pixf->asd8 = pixel_asd8_ssse3;
        pixf->intra_sad_x3_4x4  = intra_sad_x3_4x4_ssse3;
        pixf->sa8d[PIXEL_16x16]= pixel_sa8d_16x16_ssse3;
        pixf->sa8d[PIXEL_8x8]  = pixel_sa8d_8x8_ssse3;
#if ARCH_X86_64
        pixf->sa8d_satd[PIXEL_16x16] = pixel_sa8d_satd_16x16_ssse3;
#endif
        pixf->intra_sad_x3_4x4    = intra_sad_x3_4x4_ssse3;
        pixf->intra_sad_x3_8x8    = intra_sad_x3_8x8_ssse3;
        pixf->intra_sad_x3_8x8c   = intra_sad_x3_8x8c_ssse3;
        pixf->intra_sad_x3_8x16c  = intra_sad_x3_8x16c_ssse3;
        pixf->intra_satd_x3_8x16c = intra_satd_x3_8x16c_ssse3;
        pixf->intra_sad_x3_16x16  = intra_sad_x3_16x16_ssse3;
    }
    if( cpu&CPU_SSE4 )
    {
        INIT6( satd, _sse4 );
        pixf->satd[PIXEL_4x16] = pixel_satd_4x16_sse4;
        if( !(cpu&CPU_STACK_MOD4) )
        {
            INIT4( hadamard_ac, _sse4 );
        }
        pixf->sa8d[PIXEL_16x16]= pixel_sa8d_16x16_sse4;
        pixf->sa8d[PIXEL_8x8]  = pixel_sa8d_8x8_sse4;
#if ARCH_X86_64
        pixf->sa8d_satd[PIXEL_16x16] = pixel_sa8d_satd_16x16_sse4;
#endif
        pixf->intra_satd_x3_8x16c = intra_satd_x3_8x16c_sse4;
    }
    if( cpu&CPU_AVX )
    {
        INIT5_NAME( sad_aligned, sad, _ssse3 ); /* AVX-capable CPUs doesn't benefit from an aligned version */
        INIT_ADS( _avx );
        INIT6( satd, _avx );
        pixf->satd[PIXEL_4x16] = pixel_satd_4x16_avx;
        if( !(cpu&CPU_STACK_MOD4) )
        {
            INIT4( hadamard_ac, _avx );
        }
        pixf->intra_sad_x3_4x4    = intra_sad_x3_4x4_avx;
        pixf->sa8d[PIXEL_16x16]= pixel_sa8d_16x16_avx;
        pixf->sa8d[PIXEL_8x8]  = pixel_sa8d_8x8_avx;
        pixf->var[PIXEL_16x16] = pixel_var_16x16_avx;
        pixf->var[PIXEL_8x8]   = pixel_var_8x8_avx;
        pixf->ssd_nv12_core    = pixel_ssd_nv12_core_avx;
        pixf->ssim_4x4x2_core  = pixel_ssim_4x4x2_core_avx;
        pixf->ssim_end4        = pixel_ssim_end4_avx;
#if ARCH_X86_64
        pixf->sa8d_satd[PIXEL_16x16] = pixel_sa8d_satd_16x16_avx;
#endif
        pixf->intra_satd_x3_8x16c = intra_satd_x3_8x16c_avx;
    }
    if( cpu&CPU_XOP )
    {
        INIT5( sad_x3, _xop );
        INIT5( sad_x4, _xop );
        pixf->ssd_nv12_core    = pixel_ssd_nv12_core_xop;
        pixf->var[PIXEL_16x16] = pixel_var_16x16_xop;
        pixf->var[PIXEL_8x8]   = pixel_var_8x8_xop;
        pixf->vsad = pixel_vsad_xop;
        pixf->asd8 = pixel_asd8_xop;
#if ARCH_X86_64
        pixf->sa8d_satd[PIXEL_16x16] = pixel_sa8d_satd_16x16_xop;
#endif
    }
    if( cpu&CPU_AVX2 )
    {
        INIT2( ssd, _avx2 );
        INIT2( sad, _avx2 );
        INIT2_NAME( sad_aligned, sad, _avx2 );
        INIT2( sad_x3, _avx2 );
        INIT2( sad_x4, _avx2 );
        pixf->var[PIXEL_16x16] = pixel_var_16x16_avx2;
        pixf->vsad = pixel_vsad_avx2;
        pixf->ssd_nv12_core = pixel_ssd_nv12_core_avx2;
        pixf->intra_sad_x3_8x8 = intra_sad_x3_8x8_avx2;
    }
#endif // HAVE_MMX
#else // !HIGH_BIT_DEPTH
#if HAVE_MMX
    if( cpu&CPU_MMX )
    {
        INIT8( ssd, _mmx, asm_ );
    }

    if( cpu&CPU_MMX2 )
    {
        INIT8( sad, _mmx2, asm_ );
        INIT8_NAME( sad_aligned, sad, _mmx2, asm_ );
        INIT7( sad_x3, _mmx2, asm_ );
        INIT7( sad_x4, _mmx2, asm_ );
        INIT8( satd, _mmx2, asm_ );
        INIT7( satd_x3, _mmx2, asm_ );
        INIT7( satd_x4, _mmx2, asm_ );
        INIT4( hadamard_ac, _mmx2, asm_ );
        INIT_ADS( _mmx2, asm_ );
        pixf->var[PIXEL_16x16] = asm_pixel_var_16x16_mmx2;
        pixf->var[PIXEL_8x16]  = asm_pixel_var_8x16_mmx2;
        pixf->var[PIXEL_8x8]   = asm_pixel_var_8x8_mmx2;
        pixf->ssd_nv12_core    = asm_pixel_ssd_nv12_core_mmx2;
#if ARCH_X86
        pixf->sa8d[PIXEL_16x16] = asm_pixel_sa8d_16x16_mmx2;
        pixf->sa8d[PIXEL_8x8]   = asm_pixel_sa8d_8x8_mmx2;
        pixf->intra_sa8d_x3_8x8 = asm_intra_sa8d_x3_8x8_mmx2;
        pixf->ssim_4x4x2_core = asm_pixel_ssim_4x4x2_core_mmx2;
        pixf->var2[PIXEL_8x8] = asm_pixel_var2_8x8_mmx2;
        pixf->var2[PIXEL_8x16] = asm_pixel_var2_8x16_mmx2;
        pixf->vsad = asm_pixel_vsad_mmx2;

        if( cpu&CPU_CACHELINE_32 )
        {
            INIT5( sad, _cache32_mmx2 );
            INIT4( sad_x3, _cache32_mmx2 );
            INIT4( sad_x4, _cache32_mmx2 );
        }
        else if( cpu&CPU_CACHELINE_64 && !(cpu&CPU_SLOW_ATOM) )
        {
            INIT5( sad, _cache64_mmx2 );
            INIT4( sad_x3, _cache64_mmx2 );
            INIT4( sad_x4, _cache64_mmx2 );
        }
#else
        if( cpu&CPU_CACHELINE_64 && !(cpu&CPU_SLOW_ATOM) )
        {
            pixf->sad[PIXEL_8x16] = asm_pixel_sad_8x16_cache64_mmx2;
            pixf->sad[PIXEL_8x8]  = asm_pixel_sad_8x8_cache64_mmx2;
            pixf->sad[PIXEL_8x4]  = asm_pixel_sad_8x4_cache64_mmx2;
            pixf->sad_x3[PIXEL_8x16] = asm_pixel_sad_x3_8x16_cache64_mmx2;
            pixf->sad_x3[PIXEL_8x8]  = asm_pixel_sad_x3_8x8_cache64_mmx2;
            pixf->sad_x4[PIXEL_8x16] = asm_pixel_sad_x4_8x16_cache64_mmx2;
            pixf->sad_x4[PIXEL_8x8]  = asm_pixel_sad_x4_8x8_cache64_mmx2;
        }
#endif
        pixf->intra_satd_x3_16x16 = asm_intra_satd_x3_16x16_mmx2;
        pixf->intra_sad_x3_16x16  = asm_intra_sad_x3_16x16_mmx2;
        pixf->intra_satd_x3_8x16c = asm_intra_satd_x3_8x16c_mmx2;
        pixf->intra_sad_x3_8x16c  = asm_intra_sad_x3_8x16c_mmx2;
        pixf->intra_satd_x3_8x8c  = asm_intra_satd_x3_8x8c_mmx2;
        pixf->intra_sad_x3_8x8c   = asm_intra_sad_x3_8x8c_mmx2;
        pixf->intra_sad_x3_8x8    = asm_intra_sad_x3_8x8_mmx2;
        pixf->intra_satd_x3_4x4   = asm_intra_satd_x3_4x4_mmx2;
        pixf->intra_sad_x3_4x4    = asm_intra_sad_x3_4x4_mmx2;
    }

    if( cpu&CPU_SSE2 )
    {
        INIT5( ssd, _sse2slow, asm_ );
        INIT2_NAME( sad_aligned, sad, _sse2_aligned, asm_ );
        pixf->var[PIXEL_16x16] = asm_pixel_var_16x16_sse2;
        pixf->ssd_nv12_core    = asm_pixel_ssd_nv12_core_sse2;
        pixf->ssim_4x4x2_core  = asm_pixel_ssim_4x4x2_core_sse2;
        pixf->ssim_end4        = asm_pixel_ssim_end4_sse2;
        pixf->sa8d[PIXEL_16x16] = asm_pixel_sa8d_16x16_sse2;
        pixf->sa8d[PIXEL_8x8]   = asm_pixel_sa8d_8x8_sse2;
#if ARCH_X86_64
        pixf->intra_sa8d_x3_8x8 = asm_intra_sa8d_x3_8x8_sse2;
        pixf->sa8d_satd[PIXEL_16x16] = asm_pixel_sa8d_satd_16x16_sse2;
#endif
        pixf->var2[PIXEL_8x8]   = asm_pixel_var2_8x8_sse2;
        pixf->var2[PIXEL_8x16]  = asm_pixel_var2_8x16_sse2;
        pixf->vsad = asm_pixel_vsad_sse2;
        pixf->asd8 = asm_pixel_asd8_sse2;
    }

    if( (cpu&CPU_SSE2) && !(cpu&CPU_SSE2_IS_SLOW) )
    {
        INIT2( sad, _sse2, asm_ );
        INIT2( sad_x3, _sse2, asm_  );
        INIT2( sad_x4, _sse2, asm_  );
        INIT6( satd, _sse2, asm_  );
        pixf->satd[PIXEL_4x16]   = asm_pixel_satd_4x16_sse2;
        INIT6( satd_x3, _sse2, asm_  );
        INIT6( satd_x4, _sse2, asm_  );
        INIT4( hadamard_ac, _sse2, asm_  );
        INIT_ADS( _sse2, asm_  );
        pixf->var[PIXEL_8x8] = asm_pixel_var_8x8_sse2;
        pixf->var[PIXEL_8x16] = asm_pixel_var_8x16_sse2;
        pixf->intra_sad_x3_16x16 = asm_intra_sad_x3_16x16_sse2;
        pixf->intra_satd_x3_8x16c = asm_intra_satd_x3_8x16c_sse2;
        pixf->intra_sad_x3_8x16c  = asm_intra_sad_x3_8x16c_sse2;
        if( cpu&CPU_CACHELINE_64 )
        {
            INIT2( ssd, _sse2, asm_); /* faster for width 16 on p4 */
#if ARCH_X86
            INIT2( sad, _cache64_sse2, asm_ );
            INIT2( sad_x3, _cache64_sse2, asm_ );
            INIT2( sad_x4, _cache64_sse2, asm_ );
#endif
           if( cpu&CPU_SSE2_IS_FAST )
           {
               pixf->sad_x3[PIXEL_8x16] = asm_pixel_sad_x3_8x16_cache64_sse2;
               pixf->sad_x4[PIXEL_8x16] = asm_pixel_sad_x4_8x16_cache64_sse2;
           }
        }
    }

    if( cpu&CPU_SSE2_IS_FAST && !(cpu&CPU_CACHELINE_64) )
    {
        pixf->sad_aligned[PIXEL_8x16] = asm_pixel_sad_8x16_sse2;
        pixf->sad[PIXEL_8x16] = asm_pixel_sad_8x16_sse2;
        pixf->sad_x3[PIXEL_8x16] = asm_pixel_sad_x3_8x16_sse2;
        pixf->sad_x3[PIXEL_8x8] = asm_pixel_sad_x3_8x8_sse2;
        pixf->sad_x3[PIXEL_8x4] = asm_pixel_sad_x3_8x4_sse2;
        pixf->sad_x4[PIXEL_8x16] = asm_pixel_sad_x4_8x16_sse2;
        pixf->sad_x4[PIXEL_8x8] = asm_pixel_sad_x4_8x8_sse2;
        pixf->sad_x4[PIXEL_8x4] = asm_pixel_sad_x4_8x4_sse2;
    }

    if( (cpu&CPU_SSE3) && (cpu&CPU_CACHELINE_64) )
    {
        INIT2( sad, _sse3, asm_ );
        INIT2( sad_x3, _sse3, asm_ );
        INIT2( sad_x4, _sse3, asm_ );
    }

    if( cpu&CPU_SSSE3 )
    {
        INIT4( hadamard_ac, _ssse3, asm_ );
        if( !(cpu&CPU_STACK_MOD4) )
        {
            pixf->intra_sad_x9_4x4  = asm_intra_sad_x9_4x4_ssse3;
            pixf->intra_satd_x9_4x4 = asm_intra_satd_x9_4x4_ssse3;
            pixf->intra_sad_x9_8x8  = asm_intra_sad_x9_8x8_ssse3;
#if ARCH_X86_64
            pixf->intra_sa8d_x9_8x8 = asm_intra_sa8d_x9_8x8_ssse3;
#endif
        }
        INIT_ADS( _ssse3, asm_ );
        if( cpu&CPU_SLOW_ATOM )
        {
            pixf->sa8d[PIXEL_16x16]= asm_pixel_sa8d_16x16_ssse3_atom;
            pixf->sa8d[PIXEL_8x8]  = asm_pixel_sa8d_8x8_ssse3_atom;
            INIT6( satd, _ssse3_atom, asm_ );
            pixf->satd[PIXEL_4x16]  = asm_pixel_satd_4x16_ssse3_atom;
            INIT6( satd_x3, _ssse3_atom, asm_ );
            INIT6( satd_x4, _ssse3_atom, asm_ );
            INIT4( hadamard_ac, _ssse3_atom, asm_ );
#if ARCH_X86_64
            pixf->sa8d_satd[PIXEL_16x16] = asm_pixel_sa8d_satd_16x16_ssse3_atom;
#endif
        }
        else
        {
            INIT8( ssd, _ssse3, asm_ );
            pixf->sa8d[PIXEL_16x16]= asm_pixel_sa8d_16x16_ssse3;
            pixf->sa8d[PIXEL_8x8]  = asm_pixel_sa8d_8x8_ssse3;
            INIT8( satd, _ssse3, asm_ );
            INIT7( satd_x3, _ssse3, asm_ );
            INIT7( satd_x4, _ssse3, asm_ );
#if ARCH_X86_64
            pixf->sa8d_satd[PIXEL_16x16] = asm_pixel_sa8d_satd_16x16_ssse3;
#endif
        }
        pixf->intra_satd_x3_16x16 = asm_intra_satd_x3_16x16_ssse3;
        if( !(cpu&CPU_SLOW_PSHUFB) )
            pixf->intra_sad_x3_16x16  = asm_intra_sad_x3_16x16_ssse3;
        pixf->intra_satd_x3_8x16c = asm_intra_satd_x3_8x16c_ssse3;
        pixf->intra_satd_x3_8x8c  = asm_intra_satd_x3_8x8c_ssse3;
        pixf->intra_sad_x3_8x8c   = asm_intra_sad_x3_8x8c_ssse3;
        pixf->var2[PIXEL_8x8] = asm_pixel_var2_8x8_ssse3;
        pixf->var2[PIXEL_8x16] = asm_pixel_var2_8x16_ssse3;
        pixf->asd8 = asm_pixel_asd8_ssse3;
        if( cpu&CPU_CACHELINE_64 )
        {
            INIT2( sad, _cache64_ssse3, asm_ );
            INIT2( sad_x3, _cache64_ssse3, asm_ );
            INIT2( sad_x4, _cache64_ssse3, asm_ );
        }
        else
        {
            INIT2( sad_x3, _ssse3, asm_ );
            INIT5( sad_x4, _ssse3, asm_ );
        }
        if( (cpu&CPU_SLOW_ATOM) || (cpu&CPU_SLOW_SHUFFLE) )
        {
            INIT5( ssd, _sse2, asm_ ); /* on conroe, sse2 is faster for width8/16 */
        }
    }

    if( cpu&CPU_SSE4 )
    {
        INIT8( satd, _sse4, asm_ );
        INIT7( satd_x3, _sse4, asm_ );
        INIT7( satd_x4, _sse4, asm_ );
        INIT4( hadamard_ac, _sse4, asm_ );
        if( !(cpu&CPU_STACK_MOD4) )
        {
            pixf->intra_sad_x9_4x4  = asm_intra_sad_x9_4x4_sse4;
            pixf->intra_satd_x9_4x4 = asm_intra_satd_x9_4x4_sse4;
            pixf->intra_sad_x9_8x8  = asm_intra_sad_x9_8x8_sse4;
#if ARCH_X86_64
            pixf->intra_sa8d_x9_8x8 = asm_intra_sa8d_x9_8x8_sse4;
#endif
        }
        pixf->sa8d[PIXEL_16x16]= asm_pixel_sa8d_16x16_sse4;
        pixf->sa8d[PIXEL_8x8]  = asm_pixel_sa8d_8x8_sse4;
        pixf->intra_satd_x3_8x16c = asm_intra_satd_x3_8x16c_sse4;
#if ARCH_X86_64
        pixf->sa8d_satd[PIXEL_16x16] = asm_pixel_sa8d_satd_16x16_sse4;
#endif
    }

    if( cpu&CPU_AVX )
    {
        INIT2_NAME( sad_aligned, sad, _sse2, asm_ ); /* AVX-capable CPUs doesn't benefit from an aligned version */
        INIT2( sad_x3, _avx, asm_ );
        INIT2( sad_x4, _avx, asm_ );
        INIT8( satd, _avx, asm_ );
        INIT7( satd_x3, _avx, asm_ );
        INIT7( satd_x4, _avx, asm_ );
        INIT_ADS( _avx, asm_ );
        INIT4( hadamard_ac, _avx, asm_ );
        if( !(cpu&CPU_STACK_MOD4) )
        {
            pixf->intra_sad_x9_4x4  = asm_intra_sad_x9_4x4_avx;
            pixf->intra_satd_x9_4x4 = asm_intra_satd_x9_4x4_avx;
            pixf->intra_sad_x9_8x8  = asm_intra_sad_x9_8x8_avx;
#if ARCH_X86_64
            pixf->intra_sa8d_x9_8x8 = asm_intra_sa8d_x9_8x8_avx;
#endif
        }
        INIT5( ssd, _avx, asm_ );
        pixf->sa8d[PIXEL_16x16]= asm_pixel_sa8d_16x16_avx;
        pixf->sa8d[PIXEL_8x8]  = asm_pixel_sa8d_8x8_avx;
        pixf->intra_satd_x3_8x16c = asm_intra_satd_x3_8x16c_avx;
        pixf->ssd_nv12_core    = asm_pixel_ssd_nv12_core_avx;
        pixf->var[PIXEL_16x16] = asm_pixel_var_16x16_avx;
        pixf->var[PIXEL_8x16]  = asm_pixel_var_8x16_avx;
        pixf->var[PIXEL_8x8]   = asm_pixel_var_8x8_avx;
        pixf->ssim_4x4x2_core  = asm_pixel_ssim_4x4x2_core_avx;
        pixf->ssim_end4        = asm_pixel_ssim_end4_avx;
#if ARCH_X86_64
        pixf->sa8d_satd[PIXEL_16x16] = asm_pixel_sa8d_satd_16x16_avx;
#endif
    }

    if( cpu&CPU_XOP )
    {
        INIT7( satd, _xop, asm_ );
        INIT7( satd_x3, _xop, asm_ );
        INIT7( satd_x4, _xop, asm_ );
        INIT4( hadamard_ac, _xop, asm_ );
        if( !(cpu&CPU_STACK_MOD4) )
        {
            pixf->intra_satd_x9_4x4 = asm_intra_satd_x9_4x4_xop;
        }
        INIT5( ssd, _xop, asm_ );
        pixf->sa8d[PIXEL_16x16]= asm_pixel_sa8d_16x16_xop;
        pixf->sa8d[PIXEL_8x8]  = asm_pixel_sa8d_8x8_xop;
        pixf->intra_satd_x3_8x16c = asm_intra_satd_x3_8x16c_xop;
        pixf->ssd_nv12_core    = asm_pixel_ssd_nv12_core_xop;
        pixf->var[PIXEL_16x16] = asm_pixel_var_16x16_xop;
        pixf->var[PIXEL_8x16]  = asm_pixel_var_8x16_xop;
        pixf->var[PIXEL_8x8]   = asm_pixel_var_8x8_xop;
        pixf->var2[PIXEL_8x8] = asm_pixel_var2_8x8_xop;
        pixf->var2[PIXEL_8x16] = asm_pixel_var2_8x16_xop;
#if ARCH_X86_64
        pixf->sa8d_satd[PIXEL_16x16] = asm_pixel_sa8d_satd_16x16_xop;
#endif
    }

    if( cpu&CPU_AVX2 )
    {
        INIT2( ssd, _avx2, asm_ );
        INIT2( sad_x3, _avx2, asm_ );
        INIT2( sad_x4, _avx2, asm_ );
        INIT4( satd, _avx2, asm_ );
        INIT2( hadamard_ac, _avx2, asm_ );
        INIT_ADS( _avx2, asm_ );
        pixf->sa8d[PIXEL_8x8]  = asm_pixel_sa8d_8x8_avx2;
        pixf->var[PIXEL_16x16] = asm_pixel_var_16x16_avx2;
        pixf->var2[PIXEL_8x16]  = asm_pixel_var2_8x16_avx2;
        pixf->var2[PIXEL_8x8]   = asm_pixel_var2_8x8_avx2;
        pixf->intra_sad_x3_16x16 = asm_intra_sad_x3_16x16_avx2;
        pixf->intra_sad_x9_8x8  = asm_intra_sad_x9_8x8_avx2;
        pixf->intra_sad_x3_8x8c = asm_intra_sad_x3_8x8c_avx2;
        pixf->ssd_nv12_core = asm_pixel_ssd_nv12_core_avx2;
#if ARCH_X86_64
        pixf->sa8d_satd[PIXEL_16x16] = asm_pixel_sa8d_satd_16x16_avx2;
#endif
    }
#endif //HAVE_MMX

#if HAVE_ARMV6
    if( cpu&CPU_ARMV6 )
    {
        pixf->sad[PIXEL_4x8] = asm_pixel_sad_4x8_armv6;
        pixf->sad[PIXEL_4x4] = asm_pixel_sad_4x4_armv6;
        pixf->sad_aligned[PIXEL_4x8] = asm_pixel_sad_4x8_armv6;
        pixf->sad_aligned[PIXEL_4x4] = asm_pixel_sad_4x4_armv6;
    }
    if( cpu&CPU_NEON )
    {
        INIT5( sad, _neon, asm_ );
        INIT5( sad_aligned, _neon, asm_ );
        INIT7( sad_x3, _neon, asm_ );
        INIT7( sad_x4, _neon, asm_ );
        INIT7( ssd, _neon, asm_ );
        INIT7( satd, _neon, asm_ );
        INIT7( satd_x3, _neon, asm_ );
        INIT7( satd_x4, _neon, asm_ );
        INIT4( hadamard_ac, _neon, asm_ );
        pixf->sa8d[PIXEL_8x8]   = asm_pixel_sa8d_8x8_neon;
        pixf->sa8d[PIXEL_16x16] = asm_pixel_sa8d_16x16_neon;
        pixf->sa8d_satd[PIXEL_16x16] = asm_pixel_sa8d_satd_16x16_neon;
        pixf->var[PIXEL_8x8]    = asm_pixel_var_8x8_neon;
        pixf->var[PIXEL_8x16]   = asm_pixel_var_8x16_neon;
        pixf->var[PIXEL_16x16]  = asm_pixel_var_16x16_neon;
        pixf->var2[PIXEL_8x8]   = asm_pixel_var2_8x8_neon;
        pixf->var2[PIXEL_8x16]  = asm_pixel_var2_8x16_neon;
        pixf->vsad = pixel_vsad_neon;
        pixf->asd8 = pixel_asd8_neon;

        pixf->intra_sad_x3_4x4    = asm_intra_sad_x3_4x4_neon;
        pixf->intra_satd_x3_4x4   = asm_intra_satd_x3_4x4_neon;
        pixf->intra_sad_x3_8x8    = asm_intra_sad_x3_8x8_neon;
        pixf->intra_sa8d_x3_8x8   = asm_intra_sa8d_x3_8x8_neon;
        pixf->intra_sad_x3_8x8c   = asm_intra_sad_x3_8x8c_neon;
        pixf->intra_satd_x3_8x8c  = asm_intra_satd_x3_8x8c_neon;
        pixf->intra_sad_x3_8x16c  = asm_intra_sad_x3_8x16c_neon;
        pixf->intra_satd_x3_8x16c = asm_intra_satd_x3_8x16c_neon;
        pixf->intra_sad_x3_16x16  = asm_intra_sad_x3_16x16_neon;
        pixf->intra_satd_x3_16x16 = asm_intra_satd_x3_16x16_neon;

        pixf->ssd_nv12_core     = asm_pixel_ssd_nv12_core_neon;
        pixf->ssim_4x4x2_core   = asm_pixel_ssim_4x4x2_core_neon;
        pixf->ssim_end4         = asm_pixel_ssim_end4_neon;

        if( cpu&CPU_FAST_NEON_MRC )
        {
            pixf->sad[PIXEL_4x8] = asm_pixel_sad_4x8_neon;
            pixf->sad[PIXEL_4x4] = asm_pixel_sad_4x4_neon;
            pixf->sad_aligned[PIXEL_4x8] = asm_pixel_sad_aligned_4x8_neon;
            pixf->sad_aligned[PIXEL_4x4] = asm_pixel_sad_aligned_4x4_neon;
        }
        else    // really just scheduled for dual issue / A8
        {
            INIT5( sad_aligned, _neon_dual );
        }
    }
#endif

#if ARCH_AARCH64
    if( cpu&CPU_NEON )
    {
        INIT8( sad, _neon, asm_ );
        // AArch64 has no distinct instructions for aligned load/store
        INIT8_NAME( sad_aligned, sad, _neon, asm_ );
        INIT7( sad_x3, _neon, asm_ );
        INIT7( sad_x4, _neon, asm_ );
        INIT8( ssd, _neon, asm_ );
        INIT8( satd, _neon, asm_ );
        INIT7( satd_x3, _neon, asm_ );
        INIT7( satd_x4, _neon, asm_ );
        INIT4( hadamard_ac, _neon, asm_ );

        pixf->sa8d[PIXEL_8x8]   = pixel_sa8d_8x8_neon;
        pixf->sa8d[PIXEL_16x16] = pixel_sa8d_16x16_neon;
        pixf->sa8d_satd[PIXEL_16x16] = pixel_sa8d_satd_16x16_neon;

        pixf->var[PIXEL_8x8]    = pixel_var_8x8_neon;
        pixf->var[PIXEL_8x16]   = pixel_var_8x16_neon;
        pixf->var[PIXEL_16x16]  = pixel_var_16x16_neon;
        pixf->var2[PIXEL_8x8]   = pixel_var2_8x8_neon;
        pixf->var2[PIXEL_8x16]  = pixel_var2_8x16_neon;
        pixf->vsad = pixel_vsad_neon;
        pixf->asd8 = pixel_asd8_neon;

        pixf->intra_sad_x3_4x4    = intra_sad_x3_4x4_neon;
        pixf->intra_satd_x3_4x4   = intra_satd_x3_4x4_neon;
        pixf->intra_sad_x3_8x8    = intra_sad_x3_8x8_neon;
        pixf->intra_sa8d_x3_8x8   = intra_sa8d_x3_8x8_neon;
        pixf->intra_sad_x3_8x8c   = intra_sad_x3_8x8c_neon;
        pixf->intra_satd_x3_8x8c  = intra_satd_x3_8x8c_neon;
        pixf->intra_sad_x3_8x16c  = intra_sad_x3_8x16c_neon;
        pixf->intra_satd_x3_8x16c = intra_satd_x3_8x16c_neon;
        pixf->intra_sad_x3_16x16  = intra_sad_x3_16x16_neon;
        pixf->intra_satd_x3_16x16 = intra_satd_x3_16x16_neon;

        pixf->ssd_nv12_core     = pixel_ssd_nv12_core_neon;
        pixf->ssim_4x4x2_core   = pixel_ssim_4x4x2_core_neon;
        pixf->ssim_end4         = pixel_ssim_end4_neon;
    }
#endif // ARCH_AARCH64

#if HAVE_MSA
    if( cpu&CPU_MSA )
    {
        INIT8( sad, _msa, asm_ );
        INIT8_NAME( sad_aligned, sad, _msa, asm_ );
        INIT8( ssd, _msa, asm_ );
        INIT7( sad_x3, _msa, asm_ );
        INIT7( sad_x4, _msa, asm_ );
        INIT8( satd, _msa, asm_ );
        INIT4( hadamard_ac, _msa, asm_ );

        pixf->intra_sad_x3_4x4   = intra_sad_x3_4x4_msa;
        pixf->intra_sad_x3_8x8   = intra_sad_x3_8x8_msa;
        pixf->intra_sad_x3_8x8c  = intra_sad_x3_8x8c_msa;
        pixf->intra_sad_x3_16x16 = intra_sad_x3_16x16_msa;
        pixf->intra_satd_x3_4x4   = intra_satd_x3_4x4_msa;
        pixf->intra_satd_x3_16x16 = intra_satd_x3_16x16_msa;
        pixf->intra_satd_x3_8x8c  = intra_satd_x3_8x8c_msa;
        pixf->intra_sa8d_x3_8x8   = intra_sa8d_x3_8x8_msa;

        pixf->ssim_4x4x2_core = ssim_4x4x2_core_msa;

        pixf->var[PIXEL_16x16] = pixel_var_16x16_msa;
        pixf->var[PIXEL_8x16]  = pixel_var_8x16_msa;
        pixf->var[PIXEL_8x8]   = pixel_var_8x8_msa;
        pixf->var2[PIXEL_8x16]  = pixel_var2_8x16_msa;
        pixf->var2[PIXEL_8x8]   = pixel_var2_8x8_msa;
        pixf->sa8d[PIXEL_16x16] = pixel_sa8d_16x16;
        pixf->sa8d[PIXEL_8x8]   = pixel_sa8d_8x8;
    }
#endif // HAVE_MSA

#endif // HIGH_BIT_DEPTH
#if HAVE_ALTIVEC
    if( cpu&CPU_ALTIVEC )
    {
        pixel_altivec_init( pixf );
    }
#endif

    pixf->ads[PIXEL_8x16] =
    pixf->ads[PIXEL_8x4] =
    pixf->ads[PIXEL_4x8] = pixf->ads[PIXEL_16x8];
    pixf->ads[PIXEL_4x4] = pixf->ads[PIXEL_8x8];
}


int check_pixel( int cpu_ref, int cpu_new )
{

    int ret = 0, ok, used_asm;
    vbench_pixel_function_t pixel_c;
    vbench_pixel_function_t pixel_ref;
    vbench_pixel_function_t pixel_asm;


    vbench_predict_t predict_4x4[12];
    vbench_predict8x8_t predict_8x8[12];
    vbench_predict_8x8_filter_t predict_8x8_filter;
    ALIGNED_16( pixel edge[36] );
    uint16_t cost_mv[32];

    vbench_pixel_init( 0, &pixel_c );
    vbench_pixel_init( cpu_ref, &pixel_ref );
    vbench_pixel_init( cpu_new, &pixel_asm );
    vbench_predict_4x4_init( 0, predict_4x4 );
    vbench_predict_8x8_init( 0, predict_8x8, &predict_8x8_filter );
    predict_8x8_filter( pbuf2+40, edge, ALL_NEIGHBORS, ALL_NEIGHBORS );

    // maximize sum
    for( int i = 0; i < 256; i++ )
    {
        int z = i|(i>>4);
        z ^= z>>2;
        z ^= z>>1;
        pbuf4[i] = -(z&1) & PIXEL_MAX;
        pbuf3[i] = ~pbuf4[i] & PIXEL_MAX;
    }
    // random pattern made of maxed pixel differences, in case an intermediate value overflows
    for( int i = 256; i < 0x1000; i++ )
    {
        pbuf4[i] = -(pbuf1[i&~0x88]&1) & PIXEL_MAX;
        pbuf3[i] = ~(pbuf4[i]) & PIXEL_MAX;
    }

#define TEST_PIXEL( name, align ) \
    ok = 1, used_asm = 0; \
    for( int i = 0; i < ARRAY_ELEMS(pixel_c.name); i++ ) \
    { \
        int res_c, res_asm; \
        if( pixel_asm.name[i] != pixel_ref.name[i] ) \
        { \
            set_func_name( "%s_%s", #name, pixel_names[i] ); \
            used_asm = 1; \
            for( int j = 0; j < 64; j++ ) \
            { \
                res_c   = call_c( pixel_c.name[i],   pbuf1, (intptr_t)16, pbuf2+j*!align, (intptr_t)64 ); \
                res_asm = call_a( pixel_asm.name[i], pbuf1, (intptr_t)16, pbuf2+j*!align, (intptr_t)64 ); \
                if( res_c != res_asm ) \
                { \
                    ok = 0; \
                    fprintf( stderr, #name "[%d]: %d != %d [FAILED]\n", i, res_c, res_asm ); \
                    break; \
                } \
            } \
            for( int j = 0; j < 0x1000 && ok; j += 256 ) \
            { \
                res_c   = pixel_c  .name[i]( pbuf3+j, 16, pbuf4+j, 16 ); \
                res_asm = pixel_asm.name[i]( pbuf3+j, 16, pbuf4+j, 16 ); \
                if( res_c != res_asm ) \
                { \
                    ok = 0; \
                    fprintf( stderr, #name "[%d]: overflow %d != %d\n", i, res_c, res_asm ); \
                } \
            } \
        } \
    } \
    report( "pixel " #name " :" );

    TEST_PIXEL( sad, 0 );
    TEST_PIXEL( sad_aligned, 1 ); 
    TEST_PIXEL( ssd, 1 );
    TEST_PIXEL( satd, 0 );
    TEST_PIXEL( sa8d, 1 );

    // FIXME FIXME TODO: IT does not work!
#if 0
    ok = 1, used_asm = 0;
    if( pixel_asm.sa8d_satd[PIXEL_16x16] != pixel_ref.sa8d_satd[PIXEL_16x16] )
    {
        set_func_name( "sa8d_satd_%s", pixel_names[PIXEL_16x16] );
        used_asm = 1;
        for( int j = 0; j < 64; j++ )
        {
            uint32_t cost8_c = pixel_c.sa8d[PIXEL_16x16]( pbuf1, 16, pbuf2, 64 );
            uint32_t cost4_c = pixel_c.satd[PIXEL_16x16]( pbuf1, 16, pbuf2, 64 );
            uint64_t res_a = call_a64( pixel_asm.sa8d_satd[PIXEL_16x16], pbuf1, (intptr_t)16, pbuf2, (intptr_t)64 );
            uint32_t cost8_a = res_a;
            uint32_t cost4_a = res_a >> 32;
            if( cost8_a != cost8_c || cost4_a != cost4_c )
            {
                ok = 0;
                fprintf( stderr, "Failed here sa8d_satd [%d]: (%d,%d) != (%d,%d) [FAILED]\n", PIXEL_16x16,
                         cost8_c, cost4_c, cost8_a, cost4_a );
                break;
            }
        }
        for( int j = 0; j < 0x1000 && ok; j += 256 ) \
        {
            uint32_t cost8_c = pixel_c.sa8d[PIXEL_16x16]( pbuf3+j, 16, pbuf4+j, 16 );
            uint32_t cost4_c = pixel_c.satd[PIXEL_16x16]( pbuf3+j, 16, pbuf4+j, 16 );
            uint64_t res_a = pixel_asm.sa8d_satd[PIXEL_16x16]( pbuf3+j, 16, pbuf4+j, 16 );
            uint32_t cost8_a = res_a;
            uint32_t cost4_a = res_a >> 32;
            if( cost8_a != cost8_c || cost4_a != cost4_c )
            {
                ok = 0;
                fprintf( stderr, "sa8d_satd [%d]: overflow (%d,%d) != (%d,%d) [FAILED]\n", PIXEL_16x16,
                         cost8_c, cost4_c, cost8_a, cost4_a );
            }
        }
    }
#endif

    report( "pixel sa8d_satd :" );

#define TEST_PIXEL_X( N ) \
    ok = 1; used_asm = 0; \
    for( int i = 0; i < 7; i++ ) \
    { \
        ALIGNED_16( int res_c[4] ) = {0}; \
        ALIGNED_16( int res_asm[4] ) = {0}; \
        if( pixel_asm.sad_x##N[i] && pixel_asm.sad_x##N[i] != pixel_ref.sad_x##N[i] ) \
        { \
            set_func_name( "sad_x%d_%s", N, pixel_names[i] ); \
            used_asm = 1; \
            for( int j = 0; j < 64; j++ ) \
            { \
                pixel *pix2 = pbuf2+j; \
                res_c[0] = pixel_c.sad[i]( pbuf1, 16, pix2,   64 ); \
                res_c[1] = pixel_c.sad[i]( pbuf1, 16, pix2+6, 64 ); \
                res_c[2] = pixel_c.sad[i]( pbuf1, 16, pix2+1, 64 ); \
                if( N == 4 ) \
                { \
                    res_c[3] = pixel_c.sad[i]( pbuf1, 16, pix2+10, 64 ); \
                    call_a( pixel_asm.sad_x4[i], pbuf1, pix2, pix2+6, pix2+1, pix2+10, (intptr_t)64, res_asm ); \
                } \
                else \
                    call_a( pixel_asm.sad_x3[i], pbuf1, pix2, pix2+6, pix2+1, (intptr_t)64, res_asm ); \
                if( memcmp(res_c, res_asm, N*sizeof(int)) ) \
                { \
                    ok = 0; \
                    fprintf( stderr, "sad_x"#N"[%d]: %d,%d,%d,%d != %d,%d,%d,%d [FAILED]\n", \
                             i, res_c[0], res_c[1], res_c[2], res_c[3], \
                             res_asm[0], res_asm[1], res_asm[2], res_asm[3] ); \
                } \
                if( N == 4 ) \
                    call_c2( pixel_c.sad_x4[i], pbuf1, pix2, pix2+6, pix2+1, pix2+10, (intptr_t)64, res_asm ); \
                else \
                    call_c2( pixel_c.sad_x3[i], pbuf1, pix2, pix2+6, pix2+1, (intptr_t)64, res_asm ); \
            } \
        } \
    } \
    report( "pixel sad_x"#N" :" );

    TEST_PIXEL_X(3);
    TEST_PIXEL_X(4);

#define TEST_PIXEL_VAR( i ) \
    if( pixel_asm.var[i] != pixel_ref.var[i] ) \
    { \
        set_func_name( "%s_%s", "var", pixel_names[i] ); \
        used_asm = 1; \
        /* abi-check wrapper can't return uint64_t, so separate it from return value check */ \
        call_c1( pixel_c.var[i],   pbuf1,           16 ); \
        call_a1( pixel_asm.var[i], pbuf1, (intptr_t)16 ); \
        uint64_t res_c   = pixel_c.var[i]( pbuf1, 16 ); \
        uint64_t res_asm = pixel_asm.var[i]( pbuf1, 16 ); \
        if( res_c != res_asm ) \
        { \
            ok = 0; \
            fprintf( stderr, "var[%d]: %d %d != %d %d [FAILED]\n", i, (int)res_c, (int)(res_c>>32), (int)res_asm, (int)(res_asm>>32) ); \
        } \
        call_c2( pixel_c.var[i],   pbuf1, (intptr_t)16 ); \
        call_a2( pixel_asm.var[i], pbuf1, (intptr_t)16 ); \
    }

    ok = 1; used_asm = 0;
    TEST_PIXEL_VAR( PIXEL_16x16 );
    TEST_PIXEL_VAR( PIXEL_8x16 );
    TEST_PIXEL_VAR( PIXEL_8x8 );
    report( "pixel var :" );

#define TEST_PIXEL_VAR2( i ) \
    if( pixel_asm.var2[i] != pixel_ref.var2[i] ) \
    { \
        int res_c, res_asm, ssd_c, ssd_asm; \
        set_func_name( "%s_%s", "var2", pixel_names[i] ); \
        used_asm = 1; \
        res_c   = call_c( pixel_c.var2[i],   pbuf1, (intptr_t)16, pbuf2, (intptr_t)16, &ssd_c   ); \
        res_asm = call_a( pixel_asm.var2[i], pbuf1, (intptr_t)16, pbuf2, (intptr_t)16, &ssd_asm ); \
        if( res_c != res_asm || ssd_c != ssd_asm ) \
        { \
            ok = 0; \
            fprintf( stderr, "var2[%d]: %d != %d or %d != %d [FAILED]\n", i, res_c, res_asm, ssd_c, ssd_asm ); \
        } \
    }

    ok = 1; used_asm = 0;
    TEST_PIXEL_VAR2( PIXEL_8x16 );
    TEST_PIXEL_VAR2( PIXEL_8x8 );
    report( "pixel var2 :" );

    ok = 1; used_asm = 0;
    for( int i = 0; i < 4; i++ )
        if( pixel_asm.hadamard_ac[i] != pixel_ref.hadamard_ac[i] )
        {
            set_func_name( "hadamard_ac_%s", pixel_names[i] );
            used_asm = 1;
            for( int j = 0; j < 32; j++ )
            {
                pixel *pix = (j&16 ? pbuf1 : pbuf3) + (j&15)*256;
                call_c1( pixel_c.hadamard_ac[i],   pbuf1, (intptr_t)16 );
                call_a1( pixel_asm.hadamard_ac[i], pbuf1, (intptr_t)16 );
                uint64_t rc = pixel_c.hadamard_ac[i]( pix, 16 );
                uint64_t ra = pixel_asm.hadamard_ac[i]( pix, 16 );
                if( rc != ra )
                {
                    ok = 0;
                    fprintf( stderr, "hadamard_ac[%d]: %d,%d != %d,%d\n", i, (int)rc, (int)(rc>>32), (int)ra, (int)(ra>>32) );
                    break;
                }
            }
            call_c2( pixel_c.hadamard_ac[i],   pbuf1, (intptr_t)16 );
            call_a2( pixel_asm.hadamard_ac[i], pbuf1, (intptr_t)16 );
        }
    report( "pixel hadamard_ac :" );

    // maximize sum
    for( int i = 0; i < 32; i++ )
        for( int j = 0; j < 16; j++ )
            pbuf4[16*i+j] = -((i+j)&1) & PIXEL_MAX;
    ok = 1; used_asm = 0;
    if( pixel_asm.vsad != pixel_ref.vsad )
    {
        for( int h = 2; h <= 32; h += 2 )
        {
            int res_c, res_asm;
            set_func_name( "vsad" );
            used_asm = 1;
            for( int j = 0; j < 2 && ok; j++ )
            {
                pixel *p = j ? pbuf4 : pbuf1;
                res_c   = call_c( pixel_c.vsad,   p, (intptr_t)16, h );
                res_asm = call_a( pixel_asm.vsad, p, (intptr_t)16, h );
                if( res_c != res_asm )
                {
                    ok = 0;
                    fprintf( stderr, "vsad: height=%d, %d != %d\n", h, res_c, res_asm );
                    break;
                }
            }
        }
    }

    return ret;
    report( "pixel vsad :" );

    ok = 1; used_asm = 0;
    if( pixel_asm.asd8 != pixel_ref.asd8 )
    {
        set_func_name( "asd8" );
        used_asm = 1;
        int res_c = call_c( pixel_c.asd8,   pbuf1, (intptr_t)8, pbuf2, (intptr_t)8, 16 );
        int res_a = call_a( pixel_asm.asd8, pbuf1, (intptr_t)8, pbuf2, (intptr_t)8, 16 );
        if( res_c != res_a )
        {
            ok = 0;
            fprintf( stderr, "asd: %d != %d\n", res_c, res_a );
        }
    }
    report( "pixel asd :" );

#define TEST_INTRA_X3( name, i8x8, ... ) \
    if( pixel_asm.name && pixel_asm.name != pixel_ref.name ) \
    { \
        ALIGNED_16( int res_c[3] ); \
        ALIGNED_16( int res_asm[3] ); \
        set_func_name( #name ); \
        used_asm = 1; \
        call_c( pixel_c.name, pbuf1+48, i8x8 ? edge : pbuf3+48, res_c ); \
        call_a( pixel_asm.name, pbuf1+48, i8x8 ? edge : pbuf3+48, res_asm ); \
        if( memcmp(res_c, res_asm, sizeof(res_c)) ) \
        { \
            ok = 0; \
            fprintf( stderr, #name": %d,%d,%d != %d,%d,%d [FAILED]\n", \
                     res_c[0], res_c[1], res_c[2], \
                     res_asm[0], res_asm[1], res_asm[2] ); \
        } \
    }

#define TEST_INTRA_X9( name, cmp ) \
    if( pixel_asm.name && pixel_asm.name != pixel_ref.name ) \
    { \
        set_func_name( #name ); \
        used_asm = 1; \
        ALIGNED_ARRAY_64( uint16_t, bitcosts,[17] ); \
        for( int i=0; i<17; i++ ) \
            bitcosts[i] = 9*(i!=8); \
        memcpy( pbuf3, pbuf2, 20*FDEC_STRIDE*sizeof(pixel) ); \
        memcpy( pbuf4, pbuf2, 20*FDEC_STRIDE*sizeof(pixel) ); \
        for( int i=0; i<32; i++ ) \
        { \
            pixel *fenc = pbuf1+48+i*12; \
            pixel *fdec1 = pbuf3+48+i*12; \
            pixel *fdec2 = pbuf4+48+i*12; \
            int pred_mode = i%9; \
            int res_c = INT_MAX; \
            for( int j=0; j<9; j++ ) \
            { \
                predict_4x4[j]( fdec1 ); \
                int cost = pixel_c.cmp[PIXEL_4x4]( fenc, FENC_STRIDE, fdec1, FDEC_STRIDE ) + 9*(j!=pred_mode); \
                if( cost < (uint16_t)res_c ) \
                    res_c = cost + (j<<16); \
            } \
            predict_4x4[res_c>>16]( fdec1 ); \
            int res_a = call_a( pixel_asm.name, fenc, fdec2, bitcosts+8-pred_mode ); \
            if( res_c != res_a ) \
            { \
                ok = 0; \
                fprintf( stderr, #name": %d,%d != %d,%d [FAILED]\n", res_c>>16, res_c&0xffff, res_a>>16, res_a&0xffff ); \
                break; \
            } \
            if( memcmp(fdec1, fdec2, 4*FDEC_STRIDE*sizeof(pixel)) ) \
            { \
                ok = 0; \
                fprintf( stderr, #name" [FAILED]\n" ); \
                for( int j=0; j<16; j++ ) \
                    fprintf( stderr, "%02x ", fdec1[(j&3)+(j>>2)*FDEC_STRIDE] ); \
                fprintf( stderr, "\n" ); \
                for( int j=0; j<16; j++ ) \
                    fprintf( stderr, "%02x ", fdec2[(j&3)+(j>>2)*FDEC_STRIDE] ); \
                fprintf( stderr, "\n" ); \
                break; \
            } \
        } \
    }

#define TEST_INTRA8_X9( name, cmp ) \
    if( pixel_asm.name && pixel_asm.name != pixel_ref.name ) \
    { \
        set_func_name( #name ); \
        used_asm = 1; \
        ALIGNED_ARRAY_64( uint16_t, bitcosts,[17] ); \
        ALIGNED_ARRAY_16( uint16_t, satds_c,[16] ); \
        ALIGNED_ARRAY_16( uint16_t, satds_a,[16] ); \
        memset( satds_c, 0, 16 * sizeof(*satds_c) ); \
        memset( satds_a, 0, 16 * sizeof(*satds_a) ); \
        for( int i=0; i<17; i++ ) \
            bitcosts[i] = 9*(i!=8); \
        for( int i=0; i<32; i++ ) \
        { \
            pixel *fenc = pbuf1+48+i*12; \
            pixel *fdec1 = pbuf3+48+i*12; \
            pixel *fdec2 = pbuf4+48+i*12; \
            int pred_mode = i%9; \
            int res_c = INT_MAX; \
            predict_8x8_filter( fdec1, edge, ALL_NEIGHBORS, ALL_NEIGHBORS ); \
            for( int j=0; j<9; j++ ) \
            { \
                predict_8x8[j]( fdec1, edge ); \
                satds_c[j] = pixel_c.cmp[PIXEL_8x8]( fenc, FENC_STRIDE, fdec1, FDEC_STRIDE ) + 9*(j!=pred_mode); \
                if( satds_c[j] < (uint16_t)res_c ) \
                    res_c = satds_c[j] + (j<<16); \
            } \
            predict_8x8[res_c>>16]( fdec1, edge ); \
            int res_a = call_a( pixel_asm.name, fenc, fdec2, edge, bitcosts+8-pred_mode, satds_a ); \
            if( res_c != res_a || memcmp(satds_c, satds_a, 16 * sizeof(*satds_c)) ) \
            { \
                ok = 0; \
                fprintf( stderr, #name": %d,%d != %d,%d [FAILED]\n", res_c>>16, res_c&0xffff, res_a>>16, res_a&0xffff ); \
                for( int j = 0; j < 9; j++ ) \
                    fprintf( stderr, "%5d ", satds_c[j]); \
                fprintf( stderr, "\n" ); \
                for( int j = 0; j < 9; j++ ) \
                    fprintf( stderr, "%5d ", satds_a[j]); \
                fprintf( stderr, "\n" ); \
                break; \
            } \
            for( int j=0; j<8; j++ ) \
                if( memcmp(fdec1+j*FDEC_STRIDE, fdec2+j*FDEC_STRIDE, 8*sizeof(pixel)) ) \
                    ok = 0; \
            if( !ok ) \
            { \
                fprintf( stderr, #name" [FAILED]\n" ); \
                for( int j=0; j<8; j++ ) \
                { \
                    for( int k=0; k<8; k++ ) \
                        fprintf( stderr, "%02x ", fdec1[k+j*FDEC_STRIDE] ); \
                    fprintf( stderr, "\n" ); \
                } \
                fprintf( stderr, "\n" ); \
                for( int j=0; j<8; j++ ) \
                { \
                    for( int k=0; k<8; k++ ) \
                        fprintf( stderr, "%02x ", fdec2[k+j*FDEC_STRIDE] ); \
                    fprintf( stderr, "\n" ); \
                } \
                fprintf( stderr, "\n" ); \
                break; \
            } \
        } \
    }

    memcpy( pbuf3, pbuf2, 20*FDEC_STRIDE*sizeof(pixel) );
    ok = 1; used_asm = 0;
    TEST_INTRA_X3( intra_satd_x3_16x16, 0 );
    TEST_INTRA_X3( intra_satd_x3_8x16c, 0 );
    TEST_INTRA_X3( intra_satd_x3_8x8c, 0 );
    TEST_INTRA_X3( intra_sa8d_x3_8x8, 1, edge );
    TEST_INTRA_X3( intra_satd_x3_4x4, 0 );
    report( "intra satd_x3 :" );
    ok = 1; used_asm = 0;
    TEST_INTRA_X3( intra_sad_x3_16x16, 0 );
    TEST_INTRA_X3( intra_sad_x3_8x16c, 0 );
    TEST_INTRA_X3( intra_sad_x3_8x8c, 0 );
    TEST_INTRA_X3( intra_sad_x3_8x8, 1, edge );
    TEST_INTRA_X3( intra_sad_x3_4x4, 0 );
    report( "intra sad_x3 :" );
    ok = 1; used_asm = 0;
    TEST_INTRA_X9( intra_satd_x9_4x4, satd );
    TEST_INTRA8_X9( intra_sa8d_x9_8x8, sa8d );
    report( "intra satd_x9 :" );
    ok = 1; used_asm = 0;
    TEST_INTRA_X9( intra_sad_x9_4x4, sad );
    TEST_INTRA8_X9( intra_sad_x9_8x8, sad );
    report( "intra sad_x9 :" );

    ok = 1; used_asm = 0;
    if( pixel_asm.ssd_nv12_core != pixel_ref.ssd_nv12_core )
    {
        used_asm = 1;
        set_func_name( "ssd_nv12" );
        uint64_t res_u_c, res_v_c, res_u_a, res_v_a;
        for( int w = 8; w <= 360; w += 8 )
        {
            pixel_c.ssd_nv12_core(   pbuf1, 368, pbuf2, 368, w, 8, &res_u_c, &res_v_c );
            pixel_asm.ssd_nv12_core( pbuf1, 368, pbuf2, 368, w, 8, &res_u_a, &res_v_a );
            if( res_u_c != res_u_a || res_v_c != res_v_a )
            {
                ok = 0;
                fprintf( stderr, "ssd_nv12: %"PRId64",%"PRId64" != %"PRId64",%"PRId64"\n",
                         res_u_c, res_v_c, res_u_a, res_v_a );
            }
        }
        call_c( pixel_c.ssd_nv12_core,   pbuf1, (intptr_t)368, pbuf2, (intptr_t)368, 360, 8, &res_u_c, &res_v_c );
        call_a( pixel_asm.ssd_nv12_core, pbuf1, (intptr_t)368, pbuf2, (intptr_t)368, 360, 8, &res_u_a, &res_v_a );
    }

    report( "ssd_nv12 :" );

    if( pixel_asm.ssim_4x4x2_core != pixel_ref.ssim_4x4x2_core ||
        pixel_asm.ssim_end4 != pixel_ref.ssim_end4 )
    {
        int cnt;
        float res_c, res_a;
        ALIGNED_16( int sums[5][4] ) = {{0}};
        used_asm = ok = 1;
        vbench_emms();
        res_c = vbench_pixel_ssim_wxh( &pixel_c,   pbuf1+2, 32, pbuf2+2, 32, 32, 28, pbuf3, &cnt );
        res_a = vbench_pixel_ssim_wxh( &pixel_asm, pbuf1+2, 32, pbuf2+2, 32, 32, 28, pbuf3, &cnt );
        if( fabs( res_c - res_a ) > 1e-6 )
        {
            ok = 0;
            fprintf( stderr, "ssim: %.7f != %.7f [FAILED]\n", res_c, res_a );
        }
        set_func_name( "ssim_core" );
        call_c( pixel_c.ssim_4x4x2_core,   pbuf1+2, (intptr_t)32, pbuf2+2, (intptr_t)32, sums );
        call_a( pixel_asm.ssim_4x4x2_core, pbuf1+2, (intptr_t)32, pbuf2+2, (intptr_t)32, sums );
        set_func_name( "ssim_end" );
        call_c2( pixel_c.ssim_end4,   sums, sums, 4 );
        call_a2( pixel_asm.ssim_end4, sums, sums, 4 );
        /* check incorrect assumptions that 32-bit ints are zero-extended to 64-bit */
        call_c1( pixel_c.ssim_end4,   sums, sums, 3 );
        call_a1( pixel_asm.ssim_end4, sums, sums, 3 );
        report( "ssim :" );
    }


    ok = 1; used_asm = 0;
    for( int i = 0; i < 32; i++ )
        cost_mv[i] = i*10;
    
    for( int i = 0; i < 100 && ok; i++ )
        if( pixel_asm.ads[i&3] != pixel_ref.ads[i&3] )
        {
            ALIGNED_16( uint16_t sums[72] );
            ALIGNED_16( int dc[4] );
            ALIGNED_16( int16_t mvs_a[48] );
            ALIGNED_16( int16_t mvs_c[48] );
            int mvn_a, mvn_c;
            int thresh = rand() & 0x3fff;
            set_func_name( "esa_ads" );
            for( int j = 0; j < 72; j++ )
                sums[j] = rand() & 0x3fff;
            for( int j = 0; j < 4; j++ )
                dc[j] = rand() & 0x3fff;
            used_asm = 1;
            mvn_c = call_c( pixel_c.ads[i&3], dc, sums, 32, cost_mv, mvs_c, 28, thresh );
            mvn_a = call_a( pixel_asm.ads[i&3], dc, sums, 32, cost_mv, mvs_a, 28, thresh );
            if( mvn_c != mvn_a || memcmp( mvs_c, mvs_a, mvn_c*sizeof(*mvs_c) ) )
            {
                ok = 0;
                printf( "c%d: ", i&3 );
                for( int j = 0; j < mvn_c; j++ )
                    printf( "%d ", mvs_c[j] );
                printf( "\na%d: ", i&3 );
                for( int j = 0; j < mvn_a; j++ )
                    printf( "%d ", mvs_a[j] );
                printf( "\n\n" );
            }
        }
    report( "esa ads:" );
    return ret;
}



