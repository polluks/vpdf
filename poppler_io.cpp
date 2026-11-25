//========================================================================
//
// StdCachedFile.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2010 Michal Wozniak <wozniak_m@interia.pl>
//
//========================================================================

#include <config.h>

//#define _GLIBCXX_CLOCALE 1	 /* mute some annoying locale errors triggered by c++ includes */
//#define _GLIBCXX_CXX_LOCALE_H 1	 /* inside <string> that is loaded via object.h */

#include "poppler_io.h"

#include <stdio.h>

#include <cpp/poppler-version.h>

#ifdef POPPLER_VERSION
#define POPPER_VERSION_STRING POPPLER_VERSION
#undef POPPLER_VERSION
#endif

#define POPPLER_VERSION_ENCODE(major, minor, micro) (	\
	  ((major) * 10000)				\
	+ ((minor) *   100)				\
	+ ((micro) *     1))

#define POPPLER_VERSION POPPLER_VERSION_ENCODE(	\
	POPPLER_VERSION_MAJOR,			\
	POPPLER_VERSION_MINOR,			\
	POPPLER_VERSION_MICRO)

size_t StdCacheLoader::init(GooString *fname, CachedFile *cachedFile)
{
  size_t read, size = 0;
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,72,0)
  FILE *f = fopen(fname->c_str(), "r");
#else
  FILE *f = fopen(fname->getCString(), "r");
#endif
  if (f == NULL)
	return 0;

  char buf[CachedFileChunkSize];

  CachedFileWriter writer = CachedFileWriter (cachedFile, NULL);
  do {
	read = fread(buf, 1, CachedFileChunkSize, f);
    (writer.write) (buf, CachedFileChunkSize);
    size += read;
  }
  while (read == CachedFileChunkSize);

  fclose(f);
  return size;
}

int StdCacheLoader::load(const std::vector<ByteRange> &ranges, CachedFileWriter *writer)
{
  return 0;
}

