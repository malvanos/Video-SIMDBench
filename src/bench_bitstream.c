/*****************************************************************************
 * bench_bitstream.c: run and measure the kernels of bitstream
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
#include "osdep.h"
#include "macroblock.h"

/* buf1, buf2: initialised to random data and shouldn't write into them */
extern uint8_t *buf1, *buf2;
/* buf3, buf4: used to store output */
extern uint8_t *buf3, *buf4;
/* pbuf1, pbuf2: initialised to random pixel data and shouldn't write into them. */
extern pixel *pbuf1, *pbuf2;
/* pbuf3, pbuf4: point to buf3, buf4, just for type convenience */
extern pixel *pbuf3, *pbuf4;


extern int bench_pattern_len;
extern const char *bench_pattern;
char func_name[100];
extern  bench_func_t benchs[MAX_FUNCS];

#define set_func_name(...) snprintf( func_name, sizeof(func_name), __VA_ARGS__ )


#define report( name ) { \
    if( used_asm ) \
        fprintf( stderr, " - %-21s [%s]\n", name, ok ? "OK" : "FAILED" ); \
    if( !ok ) ret = -1; \
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

int check_bitstream( int cpu_ref, int cpu_new )
{
    vbench_bitstream_function_t bs_c;
    vbench_bitstream_function_t bs_ref;
    vbench_bitstream_function_t bs_a;

    int ret = 0, ok = 1, used_asm = 0;

    vbench_bitstream_init( 0, &bs_c );
    vbench_bitstream_init( cpu_ref, &bs_ref );
    vbench_bitstream_init( cpu_new, &bs_a );
    if( bs_a.nal_escape != bs_ref.nal_escape )
    {
        int size = 0x4000;
        uint8_t *input = malloc(size+100);
        uint8_t *output1 = malloc(size*2);
        uint8_t *output2 = malloc(size*2);
        used_asm = 1;
        set_func_name( "nal_escape" );
        for( int i = 0; i < 100; i++ )
        {
            /* Test corner-case sizes */
            int test_size = i < 10 ? i+1 : rand() & 0x3fff;
            /* Test 8 different probability distributions of zeros */
            for( int j = 0; j < test_size+32; j++ )
                input[j] = (rand()&((1 << ((i&7)+1)) - 1)) * rand();
            uint8_t *end_c = (uint8_t*)call_c1( bs_c.nal_escape, output1, input, input+test_size );
            uint8_t *end_a = (uint8_t*)call_a1( bs_a.nal_escape, output2, input, input+test_size );
            int size_c = end_c-output1;
            int size_a = end_a-output2;
            if( size_c != size_a || memcmp( output1, output2, size_c ) )
            {
                fprintf( stderr, "nal_escape :  [FAILED] %d %d\n", size_c, size_a );
                ok = 0;
                break;
            }
        }
        for( int j = 0; j < size+32; j++ )
            input[j] = rand();
        call_c2( bs_c.nal_escape, output1, input, input+size );
        call_a2( bs_a.nal_escape, output2, input, input+size );
        free(input);
        free(output1);
        free(output2);
    }
    report( "nal escape:" );
    return ret;
}

