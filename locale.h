#ifndef LOCALE_H
#define LOCALE_H

/*
 *   VPDF
 * 
 *   Copyright © 2008 Christian Rosentreter <tokai@binaryriot.org>
 *   Copyright © 2012 Michal Zukowski, Michal Wozniak
 *   All rights reserved.
 *
 *   $Id: locale.h,v 1.4 2024/12/06 23:08:40 cyfm Exp $
 */

#define NEW_CATCOMP_ARRAY_IDS
#include "VPDF_strings.h"

void   locale_init(void);
void   locale_cleanup(void);
STRPTR locale_getstr(ULONG);

#define LOCSTR(x) locale_getstr(x##_ID)

struct lconv {};

char* setlocale(int category, const char* locale);

#if defined(__GNUC__)  && defined(__cplusplus)

lconv* localeconv();

#endif

#define LC_NUMERIC	4

#endif /* LOCALE_H */
