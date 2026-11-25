/***************************************************************************/
/*                                                                         */
/*  ftamigafont.h                                                          */
/*                                                                         */
/*    FreeType API for accessing Amiga font-specific data.                 */
/*                                                                         */
/*  Copyright 2019 by                                                      */
/*  Nicholai Benalal                                                       */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#ifndef FTAMIGAFONT_H_
#define FTAMIGAFONT_H_

#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef FREETYPE_H
#error "freetype.h of FreeType 1 has been loaded!"
#error "Please fix the directory search order for header files"
#error "so that freetype.h of FreeType 2 is found first."
#endif


FT_BEGIN_HEADER


  /*************************************************************************/
  /*                                                                       */
  /* <Section>                                                             */
  /*    amigafont_fonts                                                    */
  /*                                                                       */
  /* <Title>                                                               */
  /*    Amiga Font Files                                                   */
  /*                                                                       */
  /* <Abstract>                                                            */
  /*    Amiga Font specific API.                                           */
  /*                                                                       */
  /* <Description>                                                         */
  /*    This section contains the declaration of Amiga Font specific       */
  /*    functions.                                                         */
  /*                                                                       */
  /*************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* <Struct>                                                              */
  /*    FT_AmigaFont_HeaderRec                                             */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Amiga Font Header info.                                            */
  /*                                                                       */
  typedef struct  FT_AmigaFont_HeaderRec_
  {
	  FT_UShort  version;

  } FT_AmigaFont_HeaderRec;


  /*************************************************************************/
  /*                                                                       */
  /* <Struct>                                                              */
  /*    FT_AmigaFont_Header                                                */
  /*                                                                       */
  /* <Description>                                                         */
  /*    A handle to an @FT_AmigaFont_HeaderRec structure.                     */
  /*                                                                       */
  typedef struct FT_AmigaFont_HeaderRec_*  FT_AmigaFont_Header;


  /**********************************************************************
   *
   * @function:
   *    FT_Get_AmigaFont_Header
   *
   * @description:
   *    Retrieve an Amiga font info header.
   *
   * @input:
   *    face    :: A handle to the input face.
   *
   * @output:
   *    aheader :: The AmigaFont header.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   This function only works with Amiga font, returning an error
   *   otherwise.
   */
  FT_EXPORT( FT_Error )
  FT_Get_AmigaFont_Header( FT_Face               face,
                        FT_AmigaFont_HeaderRec  *aheader );

  /* */


FT_END_HEADER

#endif /* FTAMIGAFONT_H_ */


/* END */


/* Local Variables: */
/* coding: utf-8    */
/* End:             */
