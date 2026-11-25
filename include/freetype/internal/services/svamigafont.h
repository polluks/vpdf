/***************************************************************************/
/*                                                                         */
/*  svamigafont.h                                                          */
/*                                                                         */
/*    The FreeType Amiga Font service (specification).                     */
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


#ifndef SVAMIGAFONT_H_
#define SVAMIGAFONT_H_

#include <freetype/internal/ftserv.h>
#include FT_AMIGAFONT_H


FT_BEGIN_HEADER


#define FT_SERVICE_ID_AMIGAFONT  "amigafont"

  typedef FT_Error
  (*FT_AmigaFont_GetHeaderFunc)( FT_Face               face,
                              FT_AmigaFont_HeaderRec  *aheader );


  FT_DEFINE_SERVICE( AmigaFont )
  {
    FT_AmigaFont_GetHeaderFunc  get_header;
  };

  /* */


FT_END_HEADER


#endif /* SVAMIGAFONT_H_ */


/* END */
