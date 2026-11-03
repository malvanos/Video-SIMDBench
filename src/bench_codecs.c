/*****************************************************************************
 * bench_codecs.c: dispatch to the ported codec kernel bench modules
 *****************************************************************************
 *
 * Copyright (C) 2016 Michail Alvanos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * See PORTING.md: a codec only links when kernels/<codec>/ exists, in which
 * case the root CMakeLists.txt defines VBENCH_HAVE_<CODEC>=1 for vbench.
 *****************************************************************************/

#include "common.h"
#include "osdep.h"
#include "bench.h"

int check_codec_kernels( int cpu_ref, int cpu_new )
{
    int ret = 0;
#if defined(VBENCH_HAVE_X264)
    ret |= vbench_codec_x264( cpu_ref, cpu_new );
#endif
#if defined(VBENCH_HAVE_X265)
    ret |= vbench_codec_x265( cpu_ref, cpu_new );
#endif
#if defined(VBENCH_HAVE_DAV1D)
    ret |= vbench_codec_dav1d( cpu_ref, cpu_new );
#endif
    return ret;
}
