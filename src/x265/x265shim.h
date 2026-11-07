/*****************************************************************************
 * x265shim.h: minimal shim replacing x265's common.h / x265.h for the
 * self-contained C kernels copied into this directory.
 *
 * Provides only what pixel.cpp / dct.cpp / intrapred.cpp / loopfilter.cpp /
 * constants.cpp / contexts.h / primitives.h use: pixel/coeff typedefs, scan
 * and block-size constants, X265_MIN/MAX macros, clip helpers, no-op
 * X265_CHECK, and stubbed-out build flags (no threading, no params, no asm).
 *
 * This file is part of the Video-SIMDBench port of x265 (AGPL, see COPYING).
 *****************************************************************************/

#ifndef X265_SHIM_H
#define X265_SHIM_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <algorithm>
#include <climits>

/* ---- build configuration stubs (normally from CMake / x265.h) ---- */
#include "vbench_config.h"   /* harness platform config: ARCH_*, HAVE_*, ... */
#define X265_NS x265
#ifndef X265_DEPTH
#define X265_DEPTH 8
#endif
#ifndef HIGH_BIT_DEPTH
#define HIGH_BIT_DEPTH 0
#endif
#define CHECKED_BUILD 0
#define ENABLE_PPA 0
#define ENABLE_VTUNE 0
/* asm availability: forced to 1 by the codec CMakeLists when the upstream
 * asm lib (x86/*.asm, aarch64/*.S) is built for this arch; 0 otherwise */
#ifndef ENABLE_ASSEMBLY
#define ENABLE_ASSEMBLY 0
#endif
#define EXPORT_C_API 0
#define HAVE_ALTIVEC 0
#define HAVE_INT_TYPES_H 1
/* arch selection from the harness platform config (vbench_config.h) */
#if ARCH_AARCH64
#define X265_ARCH_ARM64 1
#define X265_ARCH_X86 0
#elif ARCH_X86_64 || ARCH_X86
#define X265_ARCH_ARM64 0
#define X265_ARCH_X86 1
#else
#define X265_ARCH_ARM64 0
#define X265_ARCH_X86 0
#endif
#define X265_LOG_ERROR 3

/* color space defines (from x265.h) */
#define X265_CSP_I400           0
#define X265_CSP_I420           1
#define X265_CSP_I422           2
#define X265_CSP_I444           3
#define X265_CSP_COUNT          4

/* symbol prefixing used by cpu.h / contexts.h */
#define PFX3(prefix, name) prefix ## _ ## name
#define PFX2(prefix, name) PFX3(prefix, name)
#define PFX(name)          PFX2(X265_NS, name)

/* ---- alignment macros (from common.h) ---- */
#if defined(__GNUC__)
#define ALIGN_VAR_4(T, var)  T var __attribute__((aligned(4)))
#define ALIGN_VAR_8(T, var)  T var __attribute__((aligned(8)))
#define ALIGN_VAR_16(T, var) T var __attribute__((aligned(16)))
#define ALIGN_VAR_32(T, var) T var __attribute__((aligned(32)))
#define ALIGN_VAR_64(T, var) T var __attribute__((aligned(64)))
#endif

/* ---- compile-time checks: no-op in the bench build ---- */
#define X265_CHECK(expr, ...)
#define ProfileScopeEvent(x)
#define THREAD_NAME(n,i)

/* bit-scan helpers (upstream threading.h); used by some arch prim files */
#define BSR(id, x)            (id) = ((unsigned long)__builtin_clz(x) ^ 31)
#define BSF(id, x)            (id) = ((unsigned long)__builtin_ctz(x))
#define BSR64(id, x)          (id) = ((unsigned long)__builtin_clzll(x) ^ 63)
#define BSF64(id, x)          (id) = ((unsigned long)__builtin_ctzll(x))

/* ---- pixel / coefficient types (from common.h) ---- */
#if HIGH_BIT_DEPTH
typedef uint16_t pixel;
typedef uint32_t sum_t;
typedef uint64_t sum2_t;
typedef uint64_t pixel4;
typedef int64_t  ssum2_t;
#define SHIFT_TO_BITPLANE 9
#else
typedef uint8_t  pixel;
typedef uint16_t sum_t;
typedef uint32_t sum2_t;
typedef uint32_t pixel4;
typedef int32_t  ssum2_t;
#define SHIFT_TO_BITPLANE 7
#endif

#if X265_DEPTH < 10
typedef uint32_t sse_t;
#else
typedef uint64_t sse_t;
#endif

typedef int16_t coeff_t;

/* ---- constants (from common.h) ---- */
#define MAX_UINT        0xFFFFFFFFU
#define MAX_UINT64      0xFFFFFFFFFFFFFFFFULL
#define MAX_INT         2147483647
#define MAX_INT64       0x7FFFFFFFFFFFFFFFLL

#define QP_MIN          0
#undef QP_MAX_SPEC
#define QP_MAX_SPEC     51
#undef QP_MAX_MAX
#define QP_MAX_MAX      69

#define MIN_QPSCALE     0.21249999999999999
#define MAX_MAX_QPSCALE 615.46574234477100

#define NUM_CU_DEPTH            4
#define NUM_FULL_DEPTH          5
#define MIN_LOG2_CU_SIZE        3
#define MAX_LOG2_CU_SIZE        6
#define MIN_CU_SIZE             (1 << MIN_LOG2_CU_SIZE)
#define MAX_CU_SIZE             (1 << MAX_LOG2_CU_SIZE)
#define LOG2_UNIT_SIZE          2
#define UNIT_SIZE               (1 << LOG2_UNIT_SIZE)
#define LOG2_RASTER_SIZE        (MAX_LOG2_CU_SIZE - LOG2_UNIT_SIZE)
#define RASTER_SIZE             (1 << LOG2_RASTER_SIZE)
#define MAX_NUM_PARTITIONS      (RASTER_SIZE * RASTER_SIZE)

#define MIN_PU_SIZE             4
#define MIN_TU_SIZE             4
#define MAX_NUM_SPU_W           (MAX_CU_SIZE / MIN_PU_SIZE)

#define MAX_LOG2_TR_SIZE 5
#define MAX_LOG2_TS_SIZE 2
#define MAX_TR_SIZE (1 << MAX_LOG2_TR_SIZE)
#define MAX_TS_SIZE (1 << MAX_LOG2_TS_SIZE)

#define COEF_REMAIN_BIN_REDUCTION   3
#define SBH_THRESHOLD               4
#define C1FLAG_NUMBER               8
#define C2FLAG_NUMBER               1

#define QUANT_IQUANT_SHIFT          20
#define QUANT_SHIFT                 14
#define SCALE_BITS                  15
#define MAX_TR_DYNAMIC_RANGE        15

#define SHIFT_INV_1ST               7
#define SHIFT_INV_2ND               12

#define MLS_GRP_NUM                 64
#define MLS_CG_SIZE                 4
#define MLS_CG_BLK_SIZE             (MLS_CG_SIZE * MLS_CG_SIZE)
#define MLS_CG_LOG2_SIZE            2

#define SCAN_SET_SIZE               16
#define LOG2_SCAN_SET_SIZE          4

#define ALL_IDX                     -1
#define PLANAR_IDX                  0
#define VER_IDX                     26
#define HOR_IDX                     10
#define DC_IDX                      1
#define NUM_CHROMA_MODE             5
#define DM_CHROMA_IDX               36
#define MDCS_ANGLE_LIMIT            4
#define MDCS_LOG2_MAX_SIZE          3

#define MAX_NUM_TR_COEFFS           MAX_TR_SIZE * MAX_TR_SIZE
#define MAX_NUM_TR_CATEGORIES       16

#define INTEGRAL_PLANE_NUM          12

#define X265_LOWRES_CU_SIZE   8
#define X265_LOWRES_CU_BITS   3

#define X265_BYTE 8

#define MAX_CHROMA_LAMBDA_OFFSET 36

#define CHROMA_H_SHIFT(x) (x == X265_CSP_I420 || x == X265_CSP_I422)
#define CHROMA_V_SHIFT(x) (x == X265_CSP_I420)

/* FENC_STRIDE / PIXEL_MAX / QP_BD_OFFSET may already be defined by the
 * VideoBench harness src/common.h when both are included in one TU;
 * redefine to the x265 values. */
#undef FENC_STRIDE
#define FENC_STRIDE 64
#define NUM_INTRA_MODE 35

#undef PIXEL_MAX
#define PIXEL_MAX ((1 << X265_DEPTH) - 1)

#undef QP_BD_OFFSET
#define QP_BD_OFFSET (6 * (X265_DEPTH - 8))

#undef LOWRES_COST_MASK
#define LOWRES_COST_MASK ((1<<14)-1)
#define LOWRES_COST_SHIFT 14

/* ---- helpers (from common.h) ---- */
template<typename T>
inline T x265_min(T a, T b) { return a < b ? a : b; }

template<typename T>
inline T x265_max(T a, T b) { return a > b ? a : b; }

template<typename T>
inline T x265_clip3(T minVal, T maxVal, T a) { return x265_min(x265_max(minVal, a), maxVal); }

template<typename T>
inline pixel x265_clip(T x) { return (pixel)x265_min<T>(T((1 << X265_DEPTH) - 1), x265_max<T>(T(0), x)); }

static inline int8_t x265_signOf(int32_t x)
{
    return (x >> 31) | ((int32_t)((((uint32_t) - x)) >> 31));
}

#define X265_MIN(a, b) ((a) < (b) ? (a) : (b))
#define X265_MAX(a, b) ((a) > (b) ? (a) : (b))
#define COPY1_IF_LT(x, y) {if ((y) < (x)) (x) = (y);}
#define COPY2_IF_LT(x, y, a, b) \
    if ((y) < (x)) \
    { \
        (x) = (y); \
        (a) = (b); \
    }
#define COPY3_IF_LT(x, y, a, b, c, d) \
    if ((y) < (x)) \
    { \
        (x) = (y); \
        (a) = (b); \
        (c) = (d); \
    }
#define COPY4_IF_LT(x, y, a, b, c, d, e, f) \
    if ((y) < (x)) \
    { \
        (x) = (y); \
        (a) = (b); \
        (c) = (d); \
        (e) = (f); \
    }
#define X265_MIN3(a, b, c) X265_MIN((a), X265_MIN((b), (c)))
#define X265_MAX3(a, b, c) X265_MAX((a), X265_MAX((b), (c)))
#define X265_MIN4(a, b, c, d) X265_MIN((a), X265_MIN3((b), (c), (d)))
#define X265_MAX4(a, b, c, d) X265_MAX((a), X265_MAX3((b), (c), (d)))

#if defined(_MSC_VER)
#define X265_LOG2F(x) (logf((float)(x)) * 1.44269504088896405f)
#define X265_LOG2(x) (log((double)(x)) * 1.4426950408889640513713538072172)
#else
#define X265_LOG2F(x) log2f(x)
#define X265_LOG2(x)  log2(x)
#endif

namespace X265_NS {

enum TextType
{
    TEXT_LUMA     = 0,
    TEXT_CHROMA_U = 1,
    TEXT_CHROMA_V = 2,
    MAX_NUM_COMPONENT = 3
};

enum ScanType
{
    SCAN_DIAG = 0,
    SCAN_HOR = 1,
    SCAN_VER = 2,
    NUM_SCAN_TYPE = 3
};

enum SignificanceMapContextType
{
    CONTEXT_TYPE_4x4 = 0,
    CONTEXT_TYPE_8x8 = 1,
    CONTEXT_TYPE_NxN = 2,
    CONTEXT_NUMBER_OF_TYPES = 3
};

} // namespace X265_NS

/* upstream common.h pulls in the scan/transform tables at the end; do the same */
#include "constants.h"

#endif // X265_SHIM_H
