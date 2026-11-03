/*****************************************************************************
 * cabac.h: arithmetic coder
 *****************************************************************************
 * Copyright (C) 2003-2025 x264 project
 *
 * Authors: Loren Merritt <lorenm@u.washington.edu>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

#ifndef X264_CABAC_H
#define X264_CABAC_H

typedef struct
{
    /* state */
    int i_low;
    int i_range;

    /* bit stream */
    int i_queue; //stored with an offset of -8 for faster asm
    int i_bytes_outstanding;

    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    /* aligned for memcpy_aligned starting here */
    ALIGNED_64( int f8_bits_encoded ); // only if using vbx264_cabac_size_decision()

    /* context */
    uint8_t state[1024];

    /* for 16-byte alignment */
    uint8_t padding[12];
} vbx264_cabac_t;

/* init the contexts given i_slice_type, the quantif and the model */
#define vbx264_cabac_context_init vbx264_template(cabac_context_init)
void vbx264_cabac_context_init( vbx264_t *h, vbx264_cabac_t *cb, int i_slice_type, int i_qp, int i_model );

#define vbx264_cabac_encode_init_core vbx264_template(cabac_encode_init_core)
void vbx264_cabac_encode_init_core( vbx264_cabac_t *cb );
#define vbx264_cabac_encode_init vbx264_template(cabac_encode_init)
void vbx264_cabac_encode_init( vbx264_cabac_t *cb, uint8_t *p_data, uint8_t *p_end );
#define vbx264_cabac_encode_decision_c vbx264_template(cabac_encode_decision_c)
void vbx264_cabac_encode_decision_c( vbx264_cabac_t *cb, int i_ctx, int b );
#define vbx264_cabac_encode_decision_asm vbx264_template(cabac_encode_decision_asm)
void vbx264_cabac_encode_decision_asm( vbx264_cabac_t *cb, int i_ctx, int b );
#define vbx264_cabac_encode_bypass_c vbx264_template(cabac_encode_bypass_c)
void vbx264_cabac_encode_bypass_c( vbx264_cabac_t *cb, int b );
#define vbx264_cabac_encode_bypass_asm vbx264_template(cabac_encode_bypass_asm)
void vbx264_cabac_encode_bypass_asm( vbx264_cabac_t *cb, int b );
#define vbx264_cabac_encode_terminal_c vbx264_template(cabac_encode_terminal_c)
void vbx264_cabac_encode_terminal_c( vbx264_cabac_t *cb );
#define vbx264_cabac_encode_terminal_asm vbx264_template(cabac_encode_terminal_asm)
void vbx264_cabac_encode_terminal_asm( vbx264_cabac_t *cb );
#define vbx264_cabac_encode_ue_bypass vbx264_template(cabac_encode_ue_bypass)
void vbx264_cabac_encode_ue_bypass( vbx264_cabac_t *cb, int exp_bits, int val );
#define vbx264_cabac_encode_flush vbx264_template(cabac_encode_flush)
void vbx264_cabac_encode_flush( vbx264_t *h, vbx264_cabac_t *cb );

#if HAVE_MMX
#define vbx264_cabac_encode_decision vbx264_cabac_encode_decision_asm
#define vbx264_cabac_encode_bypass vbx264_cabac_encode_bypass_asm
#define vbx264_cabac_encode_terminal vbx264_cabac_encode_terminal_asm
#elif HAVE_AARCH64
#define vbx264_cabac_encode_decision vbx264_cabac_encode_decision_asm
#define vbx264_cabac_encode_bypass vbx264_cabac_encode_bypass_asm
#define vbx264_cabac_encode_terminal vbx264_cabac_encode_terminal_asm
#else
#define vbx264_cabac_encode_decision vbx264_cabac_encode_decision_c
#define vbx264_cabac_encode_bypass vbx264_cabac_encode_bypass_c
#define vbx264_cabac_encode_terminal vbx264_cabac_encode_terminal_c
#endif
#define vbx264_cabac_encode_decision_noup vbx264_cabac_encode_decision

static ALWAYS_INLINE int vbx264_cabac_pos( vbx264_cabac_t *cb )
{
    return (cb->p - cb->p_start + cb->i_bytes_outstanding) * 8 + cb->i_queue;
}

/* internal only. these don't write the bitstream, just calculate bit cost: */

static ALWAYS_INLINE void vbx264_cabac_size_decision( vbx264_cabac_t *cb, long i_ctx, long b )
{
    int i_state = cb->state[i_ctx];
    cb->state[i_ctx] = vbx264_cabac_transition[i_state][b];
    cb->f8_bits_encoded += vbx264_cabac_entropy[i_state^b];
}

static ALWAYS_INLINE int vbx264_cabac_size_decision2( uint8_t *state, long b )
{
    int i_state = *state;
    *state = vbx264_cabac_transition[i_state][b];
    return vbx264_cabac_entropy[i_state^b];
}

static ALWAYS_INLINE void vbx264_cabac_size_decision_noup( vbx264_cabac_t *cb, long i_ctx, long b )
{
    int i_state = cb->state[i_ctx];
    cb->f8_bits_encoded += vbx264_cabac_entropy[i_state^b];
}

static ALWAYS_INLINE int vbx264_cabac_size_decision_noup2( uint8_t *state, long b )
{
    return vbx264_cabac_entropy[*state^b];
}

#endif
