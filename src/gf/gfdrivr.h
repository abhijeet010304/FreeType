/****************************************************************************
 *
 * gfdrivr.h
 *
 *   FreeType font driver for METAFONT GF FONT files
 *
 * Copyright 1996-2018 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


#ifndef GFDRIVR_H_
#define GFDRIVR_H_

#include <ft2build.h>
#include FT_INTERNAL_DRIVER_H

#include "gf.h"


FT_BEGIN_HEADER


  typedef struct  GF_EncodingRec_
  {
    FT_Long   enc;
    FT_UShort glyph;

  } GF_EncodingRec, *GF_Encoding;

  /* BitmapRec for GF format specific glyphs  */
  typedef struct GF_BitmapRec_
  {
    FT_Long         bbx_width, bbx_height;
    FT_Long         off_x, off_y;
    FT_Long         mv_x,  mv_y;
    FT_Byte         *bitmap;
    FT_UInt         raster;
    FT_UShort       code;

  } GF_BitmapRec, *GF_Bitmap;


  typedef struct GF_GlyphRec_
  {
    FT_UInt         code_min, code_max;
    GF_Bitmap       bm_table;
    FT_Int          ds, hppp, vppp;
    FT_Int          font_bbx_w, font_bbx_h;
    FT_Int          font_bbx_xoff, font_bbx_yoff;

    FT_ULong        nencodings;
    GF_Encoding     encodings;

    FT_ULong        nglyphs;

  } GF_GlyphRec, *GF_Glyph;


  typedef struct  GF_FaceRec_
  {
    FT_FaceRec      root;
    GF_Glyph        gf_glyph;

    const void*     tfm;
    const void*     tfm_data;

  } GF_FaceRec, *GF_Face;


  FT_EXPORT_VAR( const FT_Driver_ClassRec )  gf_driver_class;


FT_END_HEADER


#endif /* GFDRIVR_H_ */


/* END */
