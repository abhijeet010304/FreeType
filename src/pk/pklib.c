/****************************************************************************
 *
 * pklib.c
 *
 *   FreeType font driver for TeX's PK FONT files.
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

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_STREAM_H
#include FT_INTERNAL_OBJECTS_H
#include FT_SYSTEM_H
#include FT_CONFIG_CONFIG_H
#include FT_ERRORS_H
#include FT_TYPES_H

#include "pk.h"
#include "pkdrivr.h"
#include "pkerror.h"


  /**************************************************************************
   *
   * The macro FT_COMPONENT is used in trace mode.  It is an implicit
   * parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log
   * messages during execution.
   */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_pklib

FT_Byte  bits_table[] = {
  0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };


  /**************************************************************************
   *
   * PK font utility functions.
   *
   */

  FT_Long   pk_read_intn( FT_Stream, FT_Int );
  FT_ULong  pk_read_uintn( FT_Stream, FT_Int );

#define READ_UINT1( stream )    (FT_Byte)pk_read_uintn( stream, 1)
#define READ_UINT2( stream )    (FT_Byte)pk_read_uintn( stream, 2)
#define READ_UINT3( stream )    (FT_Byte)pk_read_uintn( stream, 3)
#define READ_UINT4( stream )    (FT_Byte)pk_read_uintn( stream, 4)
#define READ_UINTN( stream,n)   (FT_ULong)pk_read_uintn( stream, n)
#define READ_INT1( stream )     (FT_String)pk_read_intn( stream, 1)
#define READ_INT2( stream )     (FT_String)pk_read_intn( stream, 2)
#define READ_INT4( stream )     (FT_Long)pk_read_intn( stream, 4)

/*
 * Reading a Number from file
 */
  FT_ULong
  pk_read_uintn(FT_Stream stream, FT_Int size)
  {
    FT_ULong  v,k;
    FT_Error  error;
    FT_Byte tp;
    v = 0L;
    while (size >= 1)
    {
      if ( FT_READ_BYTE(tp) )
        return 0;
      k =(FT_ULong)tp;
      v = v*256L + k;
      --size;
    }
    return v;
  }

  FT_Long
  pk_read_intn(FT_Stream stream, FT_Int size)
  {
    FT_Long  v;
    FT_Byte  tp;
    FT_Error error;
    FT_ULong z ;

    if ( FT_READ_BYTE(tp) )
        return 0;
    z= (FT_ULong)tp;
    v = (FT_Long)z & 0xffL;
    if (v & 0x80L)
      v = v - 256L;
    --size;
    while (size >= 1)
    {
      if ( FT_READ_BYTE(tp) )
        return 0;
      z= (FT_ULong)tp;
      v = v*256L + z;
      --size;
		}
    return v;
  }

  FT_Int  pk_read_nyble_rest_cnt;
  FT_Int  pk_read_nyble_max_bytes;

  void
  pk_read_nyble_init( FT_Int max )
  {
    pk_read_nyble_rest_cnt  = 0;
    pk_read_nyble_max_bytes = max;
  }

  FT_Int
  pk_read_nyble(FT_Stream stream)
  {
    static FT_Byte  d;
    FT_Int          v;

    switch (pk_read_nyble_rest_cnt)
    {
    case 0:
      d = READ_UINT1( stream );
      if (--pk_read_nyble_max_bytes < 0)
        return -1L;
      v = d / 0x10;
      d = d % 0x10;
      pk_read_nyble_rest_cnt = 1;
      break;
    case 1:
    default:
      v = d;
      pk_read_nyble_rest_cnt = 0;
      break;
    }
  return v;
  }

  FT_Long
  pk_read_packed_number(FT_Long* repeat, FT_Stream stream, int dyn_f)
  {
    FT_Int   d, n;
    FT_Long  di;

    entry:
      d = pk_read_nyble( stream );
      if (d == 0)
      {
        n = 0;
        do
        {
          di = pk_read_nyble( stream );
          n++;
        }
        while (di == 0);
        for ( ; n > 0; n--)
          di = di*16 + pk_read_nyble( stream );
        return di - 15 + (13 - dyn_f)*16 + dyn_f;
      }
    if (d <= dyn_f)
      return d;
    if (d <= 13)
      return (d - dyn_f - 1)*16 + pk_read_nyble( stream ) + dyn_f + 1;
    *repeat = 1;
    if (d == 14)
      *repeat = pk_read_packed_number(repeat, stream, dyn_f);
    goto entry;
  }

  FT_Int
  pk_read_14( FT_Stream stream,
              FT_ULong rs,
              PK_Bitmap bm )
  {
    FT_Long   x, y, x8, xm;
    FT_Byte   *bm_ptr;
    FT_ULong  bit16_buff;
    FT_Int    rest_bit16_buff;

    static FT_UInt mask_table[] =
      { 0xdead,   0x80,   0xc0,   0xe0,   0xf0,   0xf8,   0xfc,   0xfe, 0xdead };

    if (rs == 0)
      return 0;

    x8 = bm->bbx_width / 8;
    xm = bm->bbx_width % 8;
    bm_ptr = bm->bitmap;

    bit16_buff = READ_UINT1( stream ) << 8;
    rest_bit16_buff = 8;
    --rs;

    for(y = 0; y < bm->bbx_height; y++)
    {
      for(x = 0; x < x8; x++)
      {
        *(bm_ptr++) = bit16_buff >> 8;
        rest_bit16_buff -= 8;
        bit16_buff = (bit16_buff << 8) & 0xffff;
        if (rs > 0)
        {
	        bit16_buff |= (READ_UINT1( stream ) << (8 - rest_bit16_buff));
	        rest_bit16_buff += 8;
	        --rs;
        }
      }
      if (xm != 0)
      {
        *(bm_ptr++) = (bit16_buff >> 8) & mask_table[xm];
        rest_bit16_buff -= xm;
        bit16_buff = (bit16_buff << xm) & 0xffff;
        if (rest_bit16_buff < 8)
        {
	        if (rs > 0)
	        {
	          bit16_buff |= (READ_UINT1( stream ) << (8 - rest_bit16_buff));
	          rest_bit16_buff += 8;
	          --rs;
	        }
        }
      }
    }
    return 0;
  }

  FT_Int
  pk_read_n14( FT_Stream stream,
               FT_Int dyn_f,
               FT_Int bw,
               FT_ULong rs,
               PK_Bitmap bm )
  {
    FT_Long   x, y, xx, yy, repeat;
    FT_Int    bits, b_p;
    FT_Byte   *p, *p0, *p1;

    pk_read_nyble_init(rs);
    p    = bm->bitmap;
    bw   = 1-bw;
    bits = 0;
    for (y = 0; y < bm->bbx_height; )
    {
      b_p    = 0;
      repeat = 0;
      p0     = p;
      for (x = 0; x < bm->bbx_width; x++)
      {
        if (bits == 0)
        {
	        bw   = 1-bw;
	        if ((bits = pk_read_packed_number(&repeat, stream, dyn_f)) < 0)
	          return -1;
        }
        if (bw == 1)
	        *p = *p | bits_table[b_p];
        --bits;
        if (++b_p >= 8)
        {
	        b_p = 0;
	        p++;
        }
      }
      if (b_p != 0)
        p++;
      y++;
      for (yy = 0; yy < repeat; yy++)
      {
        p1 = p0;
        for (xx = 0; xx < bm->raster; xx++)
	        *(p++) = *(p1++);
        y++;
      }
    }
    return 0;
  }

  /**************************************************************************
   *
   * API.
   *
   */

  FT_LOCAL_DEF( FT_Error )
  pk_load_font(FT_Stream       stream,
               FT_Memory       extmemory,
               PK_Glyph        *goptr )
  {
    PK_Glyph   go;
    FT_Byte    instr, pre, id;
    FT_ULong   ds, check_sum, hppp, vppp, k;
    FT_UInt    flag, dny_f, bw, ess, size;
    FT_ULong   cc, tfm, dx, dy, dm, w, h, rs;
    FT_Long    hoff, voff, mv_x, mv_y, gptr;
    FT_Int     bc, ec, nchars, index, i;
    FT_Error   error  = FT_Err_Ok;
    FT_Memory  memory = extmemory; /* needed for FT_NEW */
    PK_Encoding   encoding = NULL;

    go = NULL;
    nchars = -1;

    if( FT_STREAM_SEEK( 0 ) )
      goto Exit;

    pre = READ_UINT1( stream );
    if (pre != PK_PRE)
    {
      error = FT_THROW( Unknown_File_Format );
      goto Exit;
    }

    id = READ_UINT1( stream );
    if (id != PK_ID)
    {
      error = FT_THROW( Unknown_File_Format );
      goto Exit;
    }

    k = READ_UINT1( stream );
    if ( FT_STREAM_SKIP( k ) )
      goto Exit;

    ds        = READ_INT4( stream );
    check_sum = READ_INT4( stream );
    hppp      = READ_INT4( stream );
    vppp      = READ_INT4( stream );

    FT_TRACE2(( "pk_load_font: checksum is %ld\n",check_sum ));

    gptr = stream->pos;

    #if 0
      /* read min & max char code */
      bc = 256;
      ec = -1;
      for (;;)
      {
        instr = READ_UINT1( stream );
        if (instr == PK_POST)
        break;
        switch ((int) instr)
        {
          case PK_XXX1:  k = (FT_ULong)READ_UINT1( stream ); if ( FT_STREAM_SKIP( k ) ) goto Exit; break;
          case PK_XXX2:  k = (FT_ULong)READ_UINT2( stream ); if ( FT_STREAM_SKIP( k ) ) goto Exit; break;
          case PK_XXX3:  k = (FT_ULong)READ_UINT3( stream ); if ( FT_STREAM_SKIP( k ) ) goto Exit; break;
          case PK_XXX4:  k = (FT_ULong)READ_UINT4( stream ); if ( FT_STREAM_SKIP( k ) ) goto Exit; break;
          case PK_YYY:   if ( FT_STREAM_SKIP( 4 ) ) goto Exit; break;
          case PK_NO_OP: break;
          default:
            size  = instr & 0x3; instr >>= 2;
            ess   = instr & 0x1;
          if (ess == 0)
          {                          /* short */
	          rs = (FT_ULong)(size*256) + (FT_ULong)READ_UINT1( stream );
	          cc   = (FT_ULong)READ_UINT1( stream );
          }
          else if ((ess == 1) && (size != 3))
          {                          /* extended short */
	          rs = (FT_ULong)(size*65536) + (FT_ULong)READ_UINT2( stream );
	          cc   = (FT_ULong)READ_UINT1( stream );
          }
          else
          {                          /* standard */
	          rs   = READ_UINT4( stream );
	          cc   = (FT_ULong)READ_UINT4( stream );
          }
          if ( FT_STREAM_SKIP( rs ) )
            goto Exit;
          if (cc < bc)
	          bc = cc;
          if (cc > ec)
	          ec = cc;
          break;
        }
      }
    #else
      bc = 0;
      ec = 255;
    #endif

    nchars    = ec - bc + 1;
    if( FT_ALLOC(go, sizeof(PK_GlyphRec)) )
      goto Exit;

    if( FT_ALLOC_MULT(go->bm_table, sizeof(PK_BitmapRec), nchars) )
      goto Exit;

    if ( FT_NEW_ARRAY( encoding, nchars ) )
      return error;

    go->ds   = (FT_UInt)ds/(1<<20);
    go->hppp = (FT_UInt)hppp/(1<<16);
    go->vppp = (FT_UInt)vppp/(1<<16);
    go->font_bbx_w = 0;
    go->font_bbx_h = 0;
    go->font_bbx_xoff = 0;
    go->font_bbx_yoff = 0;
    go->code_min = bc;
    go->code_max = ec;

    /* read glyphs */
    if( FT_STREAM_SEEK( gptr ) )
        goto Exit;

    index = 0;
    go->nglyphs = 0;
    for (;;)
    {
      if ((instr = READ_UINT1( stream )) == PK_POST)
        break;
      switch ((FT_Int)instr)
      {
        case PK_XXX1:
          k = (FT_ULong)READ_UINT1( stream );
          if ( FT_STREAM_SKIP( k ) )
            goto Exit;
          break;
        case PK_XXX2:
          k = (FT_ULong)READ_UINT2( stream );
          if ( FT_STREAM_SKIP( k ) )
            goto Exit;
          break;
        case PK_XXX3:
          k = (FT_ULong)READ_UINT3( stream );
          if ( FT_STREAM_SKIP( k ) )
            goto Exit;
          break;
        case PK_XXX4:
          k = (FT_ULong)READ_UINT4( stream );
          if ( FT_STREAM_SKIP( k ) )
            goto Exit;
          break;
        case PK_YYY:
          if ( FT_STREAM_SKIP( 4 ) )
            goto Exit;
          break;
        case PK_NO_OP:
          break;
        default:
          flag  = instr;
          size  = flag % 0x04;  flag = flag >> 2;
          ess   = flag % 0x02;  flag = flag >> 1;
          bw    = flag % 0x02;  flag = flag >> 1;
          dny_f = flag % 0x10;
        if (ess == 0)
        {                          /* short */
          rs   = (FT_ULong)(size*256) + (FT_ULong)READ_UINT1( stream ) - (FT_ULong)8;
          cc   = (FT_ULong)READ_UINT1( stream );
          tfm  = (FT_ULong)READ_UINT3( stream );
          dm   = (FT_ULong)READ_UINT1( stream );
          w    = (FT_ULong)READ_UINT1( stream );
          h    = (FT_ULong)READ_UINT1( stream );
          hoff = (FT_Long)READ_INT1( stream );
          voff = (FT_Long)READ_INT1( stream );
          mv_x = dm;
          mv_y = 0;
        }
        else if ((ess == 1) && (size != 3))
        {                          /* extended short */
          rs   = (FT_ULong)(size*65536) + (FT_ULong)READ_UINT2( stream ) - (FT_ULong)13;
          cc   = (FT_ULong)READ_UINT1( stream );
          tfm  = (FT_ULong)READ_UINT3( stream );
          dm   = (FT_ULong)READ_UINT2( stream );
          w    = (FT_ULong)READ_UINT2( stream );
          h    = (FT_ULong)READ_UINT2( stream );
          hoff = (FT_Long)READ_INT2( stream );
          voff = (FT_Long)READ_INT2( stream );
          mv_x = dm;
          mv_y = 0;
        }
        else
        {                           /* standard */
          rs   = READ_UINT4( stream ) - (FT_ULong)28;
          cc   = READ_UINT4( stream );
          tfm  = READ_UINT4( stream );
          dx   = READ_UINT4( stream );
          dy   = READ_UINT4( stream );
	        w    = READ_UINT4( stream );
          h    = READ_UINT4( stream );
          hoff = READ_INT4( stream );
          voff = READ_INT4( stream );
          mv_x = (FT_UInt)dx/(FT_UInt)(1<<16);
          mv_y = (FT_UInt)dy/(FT_UInt)(1<<16);
        }

        if ((cc < go->code_min) || (go->code_max < cc))
        {
          error = FT_THROW( Invalid_File_Format );
          goto Exit;
        }

        go->bm_table[index].bbx_width  = w;
        go->bm_table[index].bbx_height = h;
        go->bm_table[index].raster = (w+7)/8;
        go->bm_table[index].off_x  = -hoff;
        go->bm_table[index].off_y  = voff;
        go->bm_table[index].mv_x   = mv_x;
        go->bm_table[index].mv_y   = mv_y;
        go->bm_table[index].bitmap = (unsigned char*)malloc(h*((w+7)/8));
        go->bm_table[index].code   = cc ; /* For backward compatibility */
        go->nglyphs               += 1;

        encoding[index].enc   = cc ;
        encoding[index].glyph = index;


        if (go->bm_table[index].bitmap == NULL)
        {
          error = FT_THROW( Invalid_File_Format );
          goto Exit;
        }

        memset(go->bm_table[index].bitmap, 0, h*((w+7)/8));

        if (dny_f == 14)
        {
          if ( pk_read_14(stream, rs, &(go->bm_table[index]) ) < 0)
          {
            FT_ERROR(( "pk_load_font: error in `pk_read_14'\n" ));
            error = FT_THROW( Unknown_File_Format );
            goto Exit;
          }
        }
        else
        {
          if (pk_read_n14(stream, dny_f, bw, rs, &(go->bm_table[index]) ) < 0)
          {
            FT_ERROR(( "pk_load_font: error in `pk_read_n14'\n" ));
            error = FT_THROW( Unknown_File_Format );
            goto Exit;
          }
        }
        if (go->font_bbx_w < w)
          go->font_bbx_w = w;
        if (go->font_bbx_h < h)
          go->font_bbx_h = h;
        if (go->font_bbx_xoff > -hoff)
          go->font_bbx_xoff = -hoff;
        if (go->font_bbx_yoff > (voff - h))
          go->font_bbx_yoff = (voff - h);

        index++;
      }
    }

    go->nencodings = go->nglyphs;
    go->encodings  = encoding;

    *goptr          = go;
    return error;

    Exit:
      if (go != NULL)
      {
        for (i = 0; i < nchars; i++)
        {
          if (go->bm_table[i].bitmap != NULL)
           FT_FREE(go->bm_table[i].bitmap);
        }
        FT_FREE(go->bm_table);
        FT_FREE(go);
      }
      return error;
  }

  FT_LOCAL_DEF( void )
  pk_free_font( PK_Face face )
  {
    FT_Memory  memory = FT_FACE( face )->memory;
    PK_Glyph   go     = face->pk_glyph;
    FT_UInt    nchars = FT_FACE( face )->num_glyphs,i;

    if ( !go )
      return;

    if( go->bm_table )
    {
      for (i = 0; i < nchars; i++)
      {
        if (go->bm_table[i].bitmap != NULL)
          FT_FREE(go->bm_table[i].bitmap);
       }
    }
    FT_FREE(go->bm_table);
    FT_FREE(go);
  }

/* END */
