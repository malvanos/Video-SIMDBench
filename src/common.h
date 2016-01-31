
#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>

#include <sys/time.h>


#include <assert.h>

typedef uint16_t pixel_10b;
typedef uint64_t pixel4_10b;
typedef int32_t  dctcoef_10b;
typedef uint32_t udctcoef_10b;

typedef uint8_t  pixel;
typedef uint32_t pixel4;
typedef int16_t  dctcoef;
typedef uint16_t udctcoef;


#   define PIXEL_SPLAT_X4_10b(x) ((x)*0x0001000100010001ULL)
#   define MPIXEL_X4_10b(src) M64(src)
#   define PIXEL_SPLAT_X4(x) ((x)*0x01010101U)
#   define MPIXEL_X4(src) M32(src)


#define PIXEL_MAX ((1 << 10)-1)

#define XCHG(type,a,b) do{ type t = a; a = b; b = t; } while(0)
#define MIN(a,b) ( (a)<(b) ? (a) : (b) )

#define FENC_STRIDE 16
#define FDEC_STRIDE 32


#define BENCH_RUNS 100  // tradeoff between accuracy and speed
#define BENCH_ALIGNS 16 // number of stack+heap data alignments (another accuracy vs speed tradeoff)
#define MAX_FUNCS 1000  // just has to be big enough to hold all the existing functions
#define MAX_CPUS 30     // number of different combinations of cpu flags




#endif // COMMON_H


