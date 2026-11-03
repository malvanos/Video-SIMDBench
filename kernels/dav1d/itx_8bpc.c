/* 8bpc instantiation of dav1d src/itx_tmpl.c (copied here with dav1d_ -> vbdav1d_
 * symbol prefixing): set BITDEPTH and include the tmpl source, exactly like
 * dav1d's meson build does with -DBITDEPTH=8. */
#define BITDEPTH 8
#include "itx_tmpl.inc"
