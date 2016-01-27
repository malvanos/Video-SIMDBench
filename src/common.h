
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

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


#define FENC_STRIDE 16
#define FDEC_STRIDE 32



