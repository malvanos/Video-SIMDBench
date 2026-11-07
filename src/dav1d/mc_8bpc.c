/* 8bpc instantiation of dav1d src/mc_tmpl.c (copied here under native upstream
 * symbol names): set BITDEPTH and include the tmpl source, exactly like
 * dav1d's meson build does with -DBITDEPTH=8. */
#define BITDEPTH 8
#include "mc_tmpl.inc"
