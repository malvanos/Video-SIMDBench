#ifndef PIXEL_H
#define PIXEL_H

#define PIXEL_SAD_CD( name, lx, ly ) \
int name( pixel *pix1, intptr_t i_stride_pix1,  \
          pixel *pix2, intptr_t i_stride_pix2 ) ;

    PIXEL_SAD_CD( pixel_sad_16x16, 16, 16 )
PIXEL_SAD_CD( pixel_sad_16x8,  16,  8 )
PIXEL_SAD_CD( pixel_sad_8x16,   8, 16 )
PIXEL_SAD_CD( pixel_sad_8x8,    8,  8 )
PIXEL_SAD_CD( pixel_sad_8x4,    8,  4 )
PIXEL_SAD_CD( pixel_sad_4x16,   4, 16 )
PIXEL_SAD_CD( pixel_sad_4x8,    4,  8 )
PIXEL_SAD_CD( pixel_sad_4x4,    4,  4 )

#define PIXEL_SAD_CD_10B( name, lx, ly ) \
int name( pixel_10b *pix1, intptr_t i_stride_pix1,  \
          pixel_10b *pix2, intptr_t i_stride_pix2 ); 

PIXEL_SAD_CD_10B( pixel_sad_16x16_10b, 16, 16 )
PIXEL_SAD_CD_10B( pixel_sad_16x8_10b,  16,  8 )
PIXEL_SAD_CD_10B( pixel_sad_8x16_10b,   8, 16 )
PIXEL_SAD_CD_10B( pixel_sad_8x8_10b,    8,  8 )
PIXEL_SAD_CD_10B( pixel_sad_8x4_10b,    8,  4 )
PIXEL_SAD_CD_10B( pixel_sad_4x16_10b,   4, 16 )
PIXEL_SAD_CD_10B( pixel_sad_4x8_10b,    4,  8 )
PIXEL_SAD_CD_10B( pixel_sad_4x4_10b,    4,  4 )


#define SAD_XD( size ) \
void pixel_sad_x3_##size( pixel *fenc, pixel *pix0, pixel *pix1, pixel *pix2,\
                                      intptr_t i_stride, int scores[3] );\
void pixel_sad_x4_##size( pixel *fenc, pixel *pix0, pixel *pix1,pixel *pix2, pixel *pix3,\
                                      intptr_t i_stride, int scores[4] );

SAD_XD( 16x16 )
SAD_XD( 16x8 )
SAD_XD( 8x16 )
SAD_XD( 8x8 )
SAD_XD( 8x4 )
SAD_XD( 4x8 )
SAD_XD( 4x4 )




#define SAD_XD_10B( size ) \
void pixel_sad_x3_##size( pixel_10b *fenc, pixel_10b *pix0, pixel_10b *pix1, pixel_10b *pix2,\
                                      intptr_t i_stride, int scores[3] );\
void pixel_sad_x4_##size( pixel_10b *fenc, pixel_10b *pix0, pixel_10b *pix1, pixel_10b *pix2, pixel_10b *pix3,\
                                      intptr_t i_stride, int scores[4] );

SAD_XD_10B( 16x16_10b )
SAD_XD_10B( 16x8_10b )
SAD_XD_10B( 8x16_10b )
SAD_XD_10B( 8x8_10b )
SAD_XD_10B( 8x4_10b )
SAD_XD_10B( 4x8_10b )
SAD_XD_10B( 4x4_10b )




#define PIXEL_SSD_CD( name, lx, ly ) \
int name( pixel *pix1, intptr_t i_stride_pix1,  \
          pixel *pix2, intptr_t i_stride_pix2 ); 

#define PIXEL_SSD_CD_10B( name, lx, ly ) \
int name( pixel_10b *pix1, intptr_t i_stride_pix1,  \
          pixel_10b *pix2, intptr_t i_stride_pix2 );
    
    
PIXEL_SSD_CD( pixel_ssd_16x16, 16, 16 )
PIXEL_SSD_CD( pixel_ssd_16x8,  16,  8 )
PIXEL_SSD_CD( pixel_ssd_8x16,   8, 16 )
PIXEL_SSD_CD( pixel_ssd_8x8,    8,  8 )
PIXEL_SSD_CD( pixel_ssd_8x4,    8,  4 )
PIXEL_SSD_CD( pixel_ssd_4x16,   4, 16 )
PIXEL_SSD_CD( pixel_ssd_4x8,    4,  8 )
PIXEL_SSD_CD( pixel_ssd_4x4,    4,  4 )


PIXEL_SSD_CD_10B( pixel_ssd_16x16_10b, 16, 16 )
PIXEL_SSD_CD_10B( pixel_ssd_16x8_10b,  16,  8 )
PIXEL_SSD_CD_10B( pixel_ssd_8x16_10b,   8, 16 )
PIXEL_SSD_CD_10B( pixel_ssd_8x8_10b,    8,  8 )
PIXEL_SSD_CD_10B( pixel_ssd_8x4_10b,    8,  4 )
PIXEL_SSD_CD_10B( pixel_ssd_4x16_10b,   4, 16 )
PIXEL_SSD_CD_10B( pixel_ssd_4x8_10b,    4,  8 )
PIXEL_SSD_CD_10B( pixel_ssd_4x4_10b,    4,  4 )



#define PIXEL_SATD_CD( w, h, sub )\
int pixel_satd_##w##x##h( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 );

PIXEL_SATD_CD( 16, 16, pixel_satd_8x4 )
PIXEL_SATD_CD( 16, 8,  pixel_satd_8x4 )
PIXEL_SATD_CD( 8,  16, pixel_satd_8x4 )
PIXEL_SATD_CD( 8,  8,  pixel_satd_8x4 )
PIXEL_SATD_CD( 8,  4,  pixel_satd_8x4 )
PIXEL_SATD_CD( 4,  16, pixel_satd_4x4 )
PIXEL_SATD_CD( 4,  8,  pixel_satd_4x4 )
PIXEL_SATD_CD( 4,  4,  pixel_satd_4x4 )




#define PIXEL_SATD_CD_10B( w, h, sub )\
int pixel_satd_10b_##w##x##h( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 );

PIXEL_SATD_CD_10B( 16, 16, pixel_satd_8x4_10b )
PIXEL_SATD_CD_10B( 16, 8,  pixel_satd_8x4_10b )
PIXEL_SATD_CD_10B( 8,  16, pixel_satd_8x4_10b )
PIXEL_SATD_CD_10B( 8,  8,  pixel_satd_8x4_10b )
PIXEL_SATD_CD_10B( 8,  4,  pixel_satd_8x4_10b )
PIXEL_SATD_CD_10B( 4,  16, pixel_satd_4x4_10b )
PIXEL_SATD_CD_10B( 4,  8,  pixel_satd_4x4_10b )
PIXEL_SATD_CD_10B( 4,  4,  pixel_satd_4x4_10b )


#define SATD_XD( size, cpu, prefix ) \
void prefix##pixel_satd_x3_##size##cpu( pixel *fenc, pixel *pix0, pixel *pix1, pixel *pix2,\
                                            intptr_t i_stride, int scores[3] );\
void prefix##pixel_satd_x4_##size##cpu( pixel *fenc, pixel *pix0, pixel *pix1, pixel *pix2, pixel *pix3,\
                                            intptr_t i_stride, int scores[4] );

#define SATD_XD_DECL6( cpu, prefix )\
SATD_XD( 16x16, cpu, prefix  )\
SATD_XD( 16x8, cpu, prefix  )\
SATD_XD( 8x16, cpu, prefix  )\
SATD_XD( 8x8, cpu, prefix  )\
SATD_XD( 8x4, cpu, prefix  )\
SATD_XD( 4x8, cpu, prefix  )

#define SATD_XD_DECL7( cpu, prefix  )\
SATD_XD_DECL6( cpu, prefix  )\
SATD_XD( 4x4, cpu, prefix  )

SATD_XD_DECL7(,)

#if HAVE_MMX
SATD_XD_DECL7( _mmx2, asm_ )
SATD_XD_DECL6( _sse2, asm_ )
SATD_XD_DECL7( _ssse3, asm_ )
SATD_XD_DECL6( _ssse3_atom, asm_ )
SATD_XD_DECL7( _sse4, asm_ )
SATD_XD_DECL7( _avx, asm_ )
SATD_XD_DECL7( _xop, asm_ )
#endif


#define HADAMARD_ACD(w,h) \
uint64_t pixel_hadamard_ac_##w##x##h( pixel *pix, intptr_t stride );
HADAMARD_ACD( 16, 16 )
HADAMARD_ACD( 16, 8 )
HADAMARD_ACD( 8, 16 )
HADAMARD_ACD( 8, 8 )


#define HADAMARD_ACD_10B(w,h) \
uint64_t pixel_hadamard_ac_10b_##w##x##h( pixel_10b *pix, intptr_t stride );
HADAMARD_ACD_10B( 16, 16 )
HADAMARD_ACD_10B( 16, 8 )
HADAMARD_ACD_10B( 8, 16 )
HADAMARD_ACD_10B( 8, 8 )


int pixel_ads4( int enc_dc[4], uint16_t *sums, int delta,
                            uint16_t *cost_mvx, int16_t *mvs, int width, int thresh );

int pixel_ads2( int enc_dc[2], uint16_t *sums, int delta,
                           uint16_t *cost_mvx, int16_t *mvs, int width, int thresh );


int pixel_ads1( int enc_dc[1], uint16_t *sums, int delta,
                            uint16_t *cost_mvx, int16_t *mvs, int width, int thresh );



int sa8d_8x8( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 );
int sa8d_8x8_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 );
int pixel_sa8d_16x16( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 );
int pixel_sa8d_16x16_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 );

int pixel_sa8d_8x8( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2 );
int pixel_sa8d_8x8_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2 );


#define PIXEL_VAR_CD( name, w, h ) \
uint64_t name( pixel *pix, intptr_t i_stride );

PIXEL_VAR_CD( pixel_var_16x16, 16, 16 )
PIXEL_VAR_CD( pixel_var_8x16,   8, 16 )
PIXEL_VAR_CD( pixel_var_8x8,    8,  8 )




#define PIXEL_VAR_CD_10B( name, w, h ) \
uint64_t name( pixel_10b *pix, intptr_t i_stride ); 

PIXEL_VAR_CD_10B( pixel_var_16x16_10b, 16, 16 )
PIXEL_VAR_CD_10B( pixel_var_8x16_10b,   8, 16 )
PIXEL_VAR_CD_10B( pixel_var_8x8_10b,    8,  8 )


#define PIXEL_VAR2_CD( name, w, h, shift ) \
int name( pixel *pix1, intptr_t i_stride1, pixel *pix2, intptr_t i_stride2, int *ssd ) ;

PIXEL_VAR2_CD( pixel_var2_8x16, 8, 16, 7 )
PIXEL_VAR2_CD( pixel_var2_8x8,  8,  8, 6 )

#define PIXEL_VAR2_CD_10B( name, w, h, shift ) \
int name( pixel_10b *pix1, intptr_t i_stride1, pixel_10b *pix2, intptr_t i_stride2, int *ssd );

PIXEL_VAR2_CD_10B( pixel_var2_8x16_10b, 8, 16, 7 )
PIXEL_VAR2_CD_10B( pixel_var2_8x8_10b,  8,  8, 6 )





#define SATD_X_10BD( size, cpu ) \
void pixel_satd_x3_10B_##size##cpu( pixel_10b *fenc, pixel_10b *pix0, pixel_10b *pix1, pixel_10b *pix2,\
                                            intptr_t i_stride, int scores[3] );\
void pixel_satd_x4_10B_##size##cpu( pixel_10b *fenc, pixel_10b *pix0, pixel_10b *pix1, pixel_10b *pix2, pixel_10b *pix3,\
                                            intptr_t i_stride, int scores[4] );

#define SATD_X_DECL6_10BD( cpu )\
SATD_X_10BD( 16x16, cpu )\
SATD_X_10BD( 16x8, cpu )\
SATD_X_10BD( 8x16, cpu )\
SATD_X_10BD( 8x8, cpu )\
SATD_X_10BD( 8x4, cpu )\
SATD_X_10BD( 4x8, cpu )

#define SATD_X_DECL7_10BD( cpu )\
SATD_X_DECL6_10BD( cpu )\
SATD_X_10BD( 4x4, cpu )

SATD_X_DECL7_10BD()







#define DECL_PIXELS( ret, name, suffix, args, prefix ) \
    ret prefix##pixel_##name##_16x16_##suffix args;\
    ret prefix##pixel_##name##_16x8_##suffix args;\
    ret prefix##pixel_##name##_8x16_##suffix args;\
    ret prefix##pixel_##name##_8x8_##suffix args;\
    ret prefix##pixel_##name##_8x4_##suffix args;\
    ret prefix##pixel_##name##_4x16_##suffix args;\
    ret prefix##pixel_##name##_4x8_##suffix args;\
    ret prefix##pixel_##name##_4x4_##suffix args;\

#define DECL_X1( name, suffix, prefix ) \
    DECL_PIXELS( int, name, suffix, ( pixel *, intptr_t, pixel *, intptr_t ), prefix )

#define DECL_X4( name, suffix, prefix ) \
    DECL_PIXELS( void, name##_x3, suffix, ( pixel *, pixel *, pixel *, pixel *, intptr_t, int * ), prefix )\
    DECL_PIXELS( void, name##_x4, suffix, ( pixel *, pixel *, pixel *, pixel *, pixel *, intptr_t, int * ), prefix )

    DECL_X1( sad, mmx2, asm_ )
    DECL_X1( sad, sse2, asm_ )
    DECL_X1( sad, sse3, asm_ )
    DECL_X1( sad, sse2_aligned, asm_ )
    DECL_X1( sad, ssse3, asm_ )
    DECL_X1( sad, ssse3_aligned, asm_ )
    DECL_X1( sad, avx2, asm_ )
    DECL_X4( sad, mmx2, asm_ )
    DECL_X4( sad, sse2, asm_ )
    DECL_X4( sad, sse3, asm_ )
    DECL_X4( sad, ssse3, asm_ )
    DECL_X4( sad, xop, asm_ )
    DECL_X4( sad, avx, asm_ )
    DECL_X4( sad, avx2, asm_ )
    DECL_X1( ssd, mmx, asm_ )
    DECL_X1( ssd, mmx2, asm_ )
    DECL_X1( ssd, sse2slow, asm_ )
    DECL_X1( ssd, sse2, asm_ )
    DECL_X1( ssd, ssse3, asm_ )
    DECL_X1( ssd, avx, asm_ )
    DECL_X1( ssd, xop, asm_ )
    DECL_X1( ssd, avx2, asm_ )
    DECL_X1( satd, mmx2, asm_ )
    DECL_X1( satd, sse2, asm_ )
    DECL_X1( satd, ssse3, asm_ )
    DECL_X1( satd, ssse3_atom, asm_ )
    DECL_X1( satd, sse4, asm_ )
    DECL_X1( satd, avx, asm_ )
    DECL_X1( satd, xop, asm_ )
    DECL_X1( satd, avx2, asm_ )
    DECL_X1( sa8d, mmx2, asm_ )
    DECL_X1( sa8d, sse2, asm_ )
    DECL_X1( sa8d, ssse3, asm_ )
    DECL_X1( sa8d, ssse3_atom, asm_ )
    DECL_X1( sa8d, sse4, asm_ )
    DECL_X1( sa8d, avx, asm_ )
    DECL_X1( sa8d, xop, asm_ )
    DECL_X1( sa8d, avx2, asm_ )
    DECL_X1( sad, cache32_mmx2, asm_ );
    DECL_X1( sad, cache64_mmx2, asm_ );
    DECL_X1( sad, cache64_sse2, asm_ );
    DECL_X1( sad, cache64_ssse3, asm_ );
    DECL_X4( sad, cache32_mmx2, asm_ );
    DECL_X4( sad, cache64_mmx2, asm_ );
    DECL_X4( sad, cache64_sse2, asm_ );
    DECL_X4( sad, cache64_ssse3, asm_ );

    DECL_PIXELS( uint64_t, var, mmx2, ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, var, sse2, ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, var, avx,  ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, var, xop,  ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, var, avx2, ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, hadamard_ac, mmx2,  ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, hadamard_ac, sse2,  ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, hadamard_ac, ssse3, ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, hadamard_ac, ssse3_atom, ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, hadamard_ac, sse4,  ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, hadamard_ac, avx,   ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, hadamard_ac, xop,   ( pixel *pix, intptr_t i_stride ), asm_)
    DECL_PIXELS( uint64_t, hadamard_ac, avx2,  ( pixel *pix, intptr_t i_stride ), asm_)

    void intra_sad_x3_4x4( uint8_t *fenc, uint8_t *fdec, int res[3] );
    void intra_sad_x3_8x8( uint8_t *fenc, uint8_t edge[36], int res[3]);
    void intra_sad_x3_8x8c( uint8_t *fenc, uint8_t *fdec, int res[3] );
    void intra_sad_x3_8x16c( uint8_t *fenc, uint8_t *fdec, int res[3] );
    void intra_sad_x3_16x16( uint8_t *fenc, uint8_t *fdec, int res[3] );

    void intra_satd_x3_4x4        ( pixel   *, pixel   *, int * );
    void intra_satd_x3_8x8c       ( pixel   *, pixel   *, int * );

    void asm_intra_satd_x3_4x4_mmx2   ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_4x4_mmx2    ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_4x4_sse2    ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_4x4_ssse3   ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_4x4_avx     ( pixel   *, pixel   *, int * );
    void asm_intra_satd_x3_8x8c_mmx2  ( pixel   *, pixel   *, int * );
    void asm_intra_satd_x3_8x8c_ssse3 ( uint8_t *, uint8_t *, int * );
    void asm_intra_sad_x3_8x8c_mmx2   ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x8c_sse2   ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x8c_ssse3  ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x16c_avx2   ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x16c_mmx2   ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x16c_sse2   ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x16c_ssse3  ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x16c_avx2   ( pixel   *, pixel   *, int * );


    void intra_satd_x3_8x16c      ( pixel   *, pixel   *, int * );
    void asm_intra_satd_x3_8x16c_mmx2 ( pixel   *, pixel   *, int * );
    void asm_intra_satd_x3_8x16c_ssse3( uint8_t *, uint8_t *, int * );
    void asm_intra_satd_x3_8x16c_sse2 ( uint8_t *, uint8_t *, int * );
    void asm_intra_satd_x3_8x16c_sse4 ( uint8_t *, uint8_t *, int * );
    void asm_intra_satd_x3_8x16c_avx  ( uint8_t *, uint8_t *, int * );
    void asm_intra_satd_x3_8x16c_avx2 ( uint8_t *, uint8_t *, int * );
    void asm_intra_satd_x3_8x8c_avx2 ( uint8_t *, uint8_t *, int * );
    void asm_intra_satd_x3_8x16c_xop  ( uint8_t *, uint8_t *, int * );
    void asm_intra_sad_x3_8x8c_avx2 ( uint8_t *, uint8_t *, int * );



    void intra_satd_x3_16x16      ( pixel   *, pixel   *, int * );
    void asm_intra_satd_x3_16x16_mmx2 ( pixel   *, pixel   *, int * );
    void asm_intra_satd_x3_16x16_ssse3( uint8_t *, uint8_t *, int * );
    void asm_intra_sad_x3_16x16_mmx2  ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_16x16_sse2  ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_16x16_ssse3 ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_16x16_avx2  ( pixel   *, pixel   *, int * );
    void asm_intra_sa8d_x3_8x8_mmx2   ( uint8_t *, uint8_t *, int * );
    void asm_intra_sa8d_x3_8x8_sse2   ( pixel   *, pixel   *, int * );
    void intra_sa8d_x3_8x8( uint8_t *fenc, uint8_t edge[36], int *res );
    void asm_intra_sad_x3_8x8_mmx2    ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x8_sse2    ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x8_ssse3   ( pixel   *, pixel   *, int * );
    void asm_intra_sad_x3_8x8_avx2    ( uint16_t*, uint16_t*, int * );
    int asm_intra_satd_x9_4x4_ssse3( uint8_t *, uint8_t *, uint16_t * );
    int asm_intra_satd_x9_4x4_sse4 ( uint8_t *, uint8_t *, uint16_t * );
    int asm_intra_satd_x9_4x4_avx  ( uint8_t *, uint8_t *, uint16_t * );
    int asm_intra_satd_x9_4x4_xop  ( uint8_t *, uint8_t *, uint16_t * );
    int asm_intra_sad_x9_4x4_ssse3 ( uint8_t *, uint8_t *, uint16_t * );
    int asm_intra_sad_x9_4x4_sse4  ( uint8_t *, uint8_t *, uint16_t * );
    int asm_intra_sad_x9_4x4_avx   ( uint8_t *, uint8_t *, uint16_t * );
    int asm_intra_sa8d_x9_8x8_ssse3( uint8_t *, uint8_t *, uint8_t *, uint16_t *, uint16_t * );
    int asm_intra_sa8d_x9_8x8_sse4 ( uint8_t *, uint8_t *, uint8_t *, uint16_t *, uint16_t * );
    int asm_intra_sa8d_x9_8x8_avx  ( uint8_t *, uint8_t *, uint8_t *, uint16_t *, uint16_t * );
    int asm_intra_sad_x9_8x8_ssse3 ( uint8_t *, uint8_t *, uint8_t *, uint16_t *, uint16_t * );
    int asm_intra_sad_x9_8x8_sse4  ( uint8_t *, uint8_t *, uint8_t *, uint16_t *, uint16_t * );
    int asm_intra_sad_x9_8x8_avx   ( uint8_t *, uint8_t *, uint8_t *, uint16_t *, uint16_t * );
    int asm_intra_sad_x9_8x8_avx2  ( uint8_t *, uint8_t *, uint8_t *, uint16_t *, uint16_t * );



int pixel_asd8( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2, int height );

void pixel_ssd_nv12_core( pixel *pixuv1, intptr_t stride1, pixel *pixuv2, intptr_t stride2,
                                 int width, int height, uint64_t *ssd_u, uint64_t *ssd_v );


void pixel_ssd_nv12_core_10b( pixel_10b *pixuv1, intptr_t stride1, pixel_10b *pixuv2, intptr_t stride2,
                                 int width, int height, uint64_t *ssd_u, uint64_t *ssd_v );

void pixel_ssd_nv12( pixel *pix1, intptr_t i_pix1, pixel *pix2, intptr_t i_pix2,
                          int i_width, int i_height, uint64_t *ssd_u, uint64_t *ssd_v );


void pixel_ssd_nv12_10b( pixel_10b *pix1, intptr_t i_pix1, pixel_10b *pix2, intptr_t i_pix2,
                              int i_width, int i_height, uint64_t *ssd_u, uint64_t *ssd_v );


void asm_pixel_ssd_nv12_core_mmx2( pixel *pixuv1, intptr_t stride1,
            pixel *pixuv2, intptr_t stride2, int width,
            int height, uint64_t *ssd_u, uint64_t *ssd_v );
void asm_pixel_ssd_nv12_core_sse2( pixel *pixuv1, intptr_t stride1,
        pixel *pixuv2, intptr_t stride2, int width,
        int height, uint64_t *ssd_u, uint64_t *ssd_v );
void asm_pixel_ssd_nv12_core_avx ( pixel *pixuv1, intptr_t stride1,
        pixel *pixuv2, intptr_t stride2, int width,
        int height, uint64_t *ssd_u, uint64_t *ssd_v );
void asm_pixel_ssd_nv12_core_xop ( pixel *pixuv1, intptr_t stride1,
        pixel *pixuv2, intptr_t stride2, int width,
        int height, uint64_t *ssd_u, uint64_t *ssd_v );
void asm_pixel_ssd_nv12_core_avx2( pixel *pixuv1, intptr_t stride1,
        pixel *pixuv2, intptr_t stride2, int width,
        int height, uint64_t *ssd_u, uint64_t *ssd_v );

void ssim_4x4x2_core( const pixel *pix1, intptr_t stride1,
                             const pixel *pix2, intptr_t stride2,
                             int sums[2][4] );


void ssim_4x4x2_core_10b( const pixel_10b *pix1, intptr_t stride1,
                          const pixel_10b *pix2, intptr_t stride2,
                             int sums[2][4] );

void asm_pixel_ssim_4x4x2_core_mmx2( const uint8_t *pix1, intptr_t stride1,
        const uint8_t *pix2, intptr_t stride2, int sums[2][4] );
void asm_pixel_ssim_4x4x2_core_sse2( const pixel *pix1, intptr_t stride1,
        const pixel *pix2, intptr_t stride2, int sums[2][4] );
void asm_pixel_ssim_4x4x2_core_avx ( const pixel *pix1, intptr_t stride1,
        const pixel *pix2, intptr_t stride2, int sums[2][4] );

float ssim_end4( int sum0[5][4], int sum1[5][4], int width );
float ssim_end4_10b( int sum0[5][4], int sum1[5][4], int width );
float asm_pixel_ssim_end4_sse2( int sum0[5][4], int sum1[5][4], int width );
float asm_pixel_ssim_end4_avx ( int sum0[5][4], int sum1[5][4], int width );
int  asm_pixel_var2_8x8_mmx2  ( pixel *,   intptr_t, pixel *,   intptr_t, int * );
int  asm_pixel_var2_8x8_sse2  ( pixel *,   intptr_t, pixel *,   intptr_t, int * );
int  asm_pixel_var2_8x8_ssse3 ( uint8_t *, intptr_t, uint8_t *, intptr_t, int * );
int  asm_pixel_var2_8x8_xop   ( uint8_t *, intptr_t, uint8_t *, intptr_t, int * );
int  asm_pixel_var2_8x8_avx2  ( uint8_t *, intptr_t, uint8_t *, intptr_t, int * );
int  asm_pixel_var2_8x16_mmx2 ( pixel *,   intptr_t, pixel *,   intptr_t, int * );
int  asm_pixel_var2_8x16_sse2 ( pixel *,   intptr_t, pixel *,   intptr_t, int * );
int  asm_pixel_var2_8x16_ssse3( uint8_t *, intptr_t, uint8_t *, intptr_t, int * );
int  asm_pixel_var2_8x16_xop  ( uint8_t *, intptr_t, uint8_t *, intptr_t, int * );
int  asm_pixel_var2_8x16_avx2 ( uint8_t *, intptr_t, uint8_t *, intptr_t, int * );
int  pixel_vsad_mmx2 ( pixel *src, intptr_t stride, int height );
int  pixel_vsad_sse2 ( pixel *src, intptr_t stride, int height );
int  pixel_vsad_ssse3( pixel *src, intptr_t stride, int height );
int  pixel_vsad_xop  ( pixel *src, intptr_t stride, int height );
int  pixel_vsad_avx2 ( uint16_t *src, intptr_t stride, int height );
int pixel_asd8_sse2 ( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2, int height );
int pixel_asd8_ssse3( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2, int height );
int pixel_asd8_xop  ( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2, int height );
uint64_t pixel_sa8d_satd_16x16_sse2      ( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2 );
uint64_t pixel_sa8d_satd_16x16_ssse3     ( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2 );
uint64_t pixel_sa8d_satd_16x16_ssse3_atom( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2 );
uint64_t pixel_sa8d_satd_16x16_sse4      ( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2 );
uint64_t pixel_sa8d_satd_16x16_avx       ( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2 );
uint64_t pixel_sa8d_satd_16x16_xop       ( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2 );
uint64_t pixel_sa8d_satd_16x16_avx2      ( pixel *pix1, intptr_t stride1, pixel *pix2, intptr_t stride2 );




int pixel_vsad( pixel *src, intptr_t stride, int height );
int pixel_vsad_10b( pixel *src, intptr_t stride, int height );

#define DECL_ADS( size, suffix, prefix ) \
    int prefix##pixel_ads##size##_##suffix( int enc_dc[size], uint16_t *sums, int delta,\
            uint16_t *cost_mvx, int16_t *mvs, int width, int thresh );
    DECL_ADS( 4, mmx2, asm_ )
    DECL_ADS( 2, mmx2, asm_ )
    DECL_ADS( 1, mmx2, asm_ )
    DECL_ADS( 4, sse2, asm_ )
    DECL_ADS( 2, sse2, asm_ )
    DECL_ADS( 1, sse2, asm_ )
    DECL_ADS( 4, ssse3, asm_ )
    DECL_ADS( 2, ssse3, asm_ )
    DECL_ADS( 1, ssse3, asm_ )
    DECL_ADS( 4, avx, asm_ )
    DECL_ADS( 2, avx, asm_ )
    DECL_ADS( 1, avx, asm_ )
    DECL_ADS( 4, avx2, asm_ )
    DECL_ADS( 2, avx2, asm_ )
    DECL_ADS( 1, avx2, asm_ )

#undef DECL_PIXELS
#undef DECL_X1
#undef DECL_X4
#undef DECL_ADS


#endif /* PIXEL_H */
