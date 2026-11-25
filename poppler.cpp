/*
 * Wrap poppler document class functions into c functions.
 */

//#define USE_SPLASH

#define AROS_ALMOST_COMPATIBLE

//#define _GLIBCXX_CLOCALE 1	 /* mute some annoying locale errors triggered by c++ includes */
//#define _GLIBCXX_CXX_LOCALE_H 1	 /* inside <string> that is loaded via object.h */

#include "Object.h"

#include <proto/charsets.h>
#define _NO_PPCINLINE
#include <proto/keymap.h>
#include <proto/exec.h>
#define DOS_DOSEXTENS_H	// avoid including dosextens.h as libpoppler >= 25.07.0 has its own ErrorString
#include <proto/dos.h>
#include <proto/graphics.h>
#include <clib/debug_protos.h>
#include <exec/semaphores.h>
#include <exec/lists.h>
#include <constructor.h>



#define USE_FLOAT 1
#include <poppler-config.h>
#include <config.h>

#include <stddef.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <time.h>

#include <cpp/poppler-version.h>

#include "PDFDoc.h"
#include "Outline.h"
#include "Link.h"
#include "GlobalParams.h"
#include "PDFDocEncoding.h"

//#define USE_SPLASH
#ifdef USE_SPLASH
#include "splash/SplashBitmap.h"
#include "splash/Splash.h"
#include "SplashOutputDev.h"
#include "TextOutputDev.h"
#else
#include "CairoOutputDev.h"
#endif
#include "PSOutputDev.h"

#include "poppler.h"
#include "poppler_io.h"
#include "poppler_device.h"

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


#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
#include <algorithm>
#include <vector>
#else
#include "goo/GooList.h"
#endif

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,72,0)
#define POPPLER_GT_007200
#endif

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,71,0)
typedef bool GBool;
#define gFalse false
#define gTrue true
#endif

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,64,0)
#define CONSTUNICODE const
#define CONSTVOID const
#else
#define CONSTUNICODE
#define CONSTVOID
#endif

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,61,0)
#define CONSTLINKDEST const
#define CONSTGOOLIST const
#define CONSTGOOSTRING const
#define POPPLER_GT_006100
#else
#define CONSTLINKDEST
#define CONSTGOOLIST
#define CONSTGOOSTRING
#endif

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,58,0)
#define NEW_OBJECTS
#endif

#include <cstddef>

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,86,0)
//typedef decltype(nullptr) NULLPTR;
#define NULLPTR nullptr
#define STRNULLPTR (const std::string&)nullptr;
#define CONSTNULLPTR (const)nullptr
#define CONSTSTDSTRNULLPTR	(const std::string)nullptr
#else
#define NULLPTR NULL
#define STRNULLPTR NULL
#define CONSTNULLPTR NULL
#define CONSTSTDSTRNULLPTR NULL
#endif

extern struct Library *LocaleBase;
#define LOCALE_BASE_NAME LocaleBase

#warning TODO: make the semaphore per-document

struct SignalSemaphore semaphore;

extern struct Library *CairoBase;

/* all of this is just to not having to expose GBool in ppoppler.h... */

struct abortcallbackcontext
{
	int	(*userfunction)(void *);
	void *userdata;
	struct devicecontext *ctx;
};

/* this is bit hacky but we can't/shouldn't use boopsi in poppler.c */

#define D(x) 

extern "C" {
	void vpdfErrorFunction(int pos, char *message);
	int __morphos_global_disable_cms = 0;
};

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,70,0)
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,85,0)
static void	pdfErrorFunction(ErrorCategory category, Goffset pos, const char *msg)
{
	vpdfErrorFunction(pos, (char *)msg);
}
#else
static void	pdfErrorFunction(void *data, ErrorCategory category, Goffset pos, const char *msg)
{
	vpdfErrorFunction(pos, (char *)msg);
}
#endif
#else
static void	pdfErrorFunction(void *data, ErrorCategory category, Goffset pos, char *msg)
{
	vpdfErrorFunction(pos, msg);
}
#endif

static CONSTRUCTOR_P(init_poppler, 0)
{
	InitSemaphore(&semaphore);
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,85,0)
	setErrorCallback(pdfErrorFunction);
#else
	setErrorCallback(pdfErrorFunction, NULL);
#endif
	return 0;
}

static DESTRUCTOR_P(cleanup_poppler, 0)
{
	D(kprintf("destroy poppler:%p\n", &semaphore));
}


#if 1
#define ENTER_SECTION \
{ ObtainSemaphore(&semaphore);}
#else // time delay needed to enter critical section
#define ENTER_SECTION \
{ clock_t t0 = clock(); ObtainSemaphore(&semaphore); t0 = clock() - t0; kprintf("section enter:%f:%s\n", (float)t0/CLOCKS_PER_SEC, __FUNCTION__);}
#endif

//#define ENTER_SECTION
#define LEAVE_SECTION ReleaseSemaphore(&semaphore);
//#define LEAVE_SECTION

//struct Library *CairoBase;

/// utility functions

static char *convertUTF16ToANSI(unsigned char *string, int length)
{
	char *buff = (char*)calloc(1, length + 1);
	if(buff != NULL)
		ConvertTagList((unsigned char*)string + 2, length * 2 - 2, buff, length + 1, MIBENUM_UTF_16BE, MIBENUM_SYSTEM, NULL);
	return buff;
}

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(24,2,0)
static char *convertUTF32ToANSI(const std::vector<Unicode> &ucstring)
{
	int length = ucstring.size();
	char *buff = (char*)calloc(1, length + 1);
	const unsigned int *string=ucstring.data();
	if(buff != NULL)
		ConvertTagList((unsigned char*)string, length * 4, buff, length + 1, MIBENUM_UTF_32BE, MIBENUM_SYSTEM, NULL);

	return buff;
}
#endif

static char *convertUTF32ToANSI(CONSTUNICODE Unicode *string, int length)
{
	char *buff = (char*)calloc(1, length + 1);
	if(buff != NULL)
		ConvertTagList((unsigned char*)string, length * 4, buff, length + 1, MIBENUM_UTF_32BE, MIBENUM_SYSTEM, NULL);

	return buff;
}


static WCHAR *convertToUCS4(const char *string)
{
	size_t length = strlen(string);
	size_t i;

	WCHAR *buff = (WCHAR*)calloc(sizeof(WCHAR), length + 1);
	if(buff != NULL)
	{
		for(i = 0; i < length; i++)
			buff[i] = ToUCS4(string[i], NULL);
	}

	return buff;
}


#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(24,5,0)
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
static bool hasUnicodeMarker(CONSTGOOSTRING GooString *s) { return s->size() >= 2 && s->getChar(0) == '\xfe' && s->getChar(1) == '\xff'; }
#else
static bool hasUnicodeMarker(CONSTGOOSTRING GooString *s) { return s->getLength() >= 2 && s->getChar(0) == '\xfe' && s->getChar(1) == '\xff'; }
#endif
#endif

static char *convertToANSI(CONSTGOOSTRING GooString *s)
{
	if(!s)
		return NULL;

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(24,5,0)
	if(hasUnicodeMarker(s))
#else
	if(s->hasUnicodeMarker())
#endif
	{
		//printf("has unicode marker\n");
#ifdef POPPLER_GT_007200
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
		return convertUTF16ToANSI((unsigned char*)s->c_str(), s->size() - 2);
#else
		return convertUTF16ToANSI((unsigned char*)s->c_str(), s->getLength() - 2);
#endif
#else
		return convertUTF16ToANSI((unsigned char*)s->getCString(), s->getLength() - 2);
#endif
	}
	else
	{
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
		char *buffer = (char*)calloc(1, s->size() + 1);
#else
		char *buffer = (char*)calloc(1, s->getLength() + 1);
#endif
		int i;

		//printf("no unicode marker\n");
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
		for(i = 0; i < s->size(); ++i)
#else
		for(i = 0; i < s->getLength(); ++i)
#endif
			buffer[i] = s->getChar(i);

		return buffer;
	}
}

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,86,0)
static char *convertToANSI(const std::string& s)
{
	try
	{

//	if(s == (const std::string)nullptr)
//		return NULL;

#if 0
	if(s->hasUnicodeMarker())
	{
		//printf("has unicode marker\n");
#ifdef POPPLER_GT_007200
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
		return convertUTF16ToANSI((unsigned char*)s->c_str(), s->size() - 2);
#else
		return convertUTF16ToANSI((unsigned char*)s->c_str(), s->getLength() - 2);
#endif
#else
		return convertUTF16ToANSI((unsigned char*)s->getCString(), s->getLength() - 2);
#endif
	}
	else
#endif
	{
		char *buffer = (char*)calloc(1, s.length() + 1);

		//printf("no unicode marker\n");
		for(unsigned int i = 0; i < s.length(); ++i)
			buffer[i] = s[i];

		return buffer;
	}

	}
	catch(...)
	{
		//kprintf("%s: exception !\n",__FUNCTION__);
	}

	return NULL;
}
#endif

////

struct foo
{
	int bar;
};

struct foo foo;

int pdfNewWithOptions(void **_pdfctx,const char *fname,const char *ownerpw,const char *userpw)
{
	int ret = errNone;
	//base1 = CairoBase;
	//CairoBase = OpenLibrary("cairo.library", 0);
//StackFrame[-1].LR-> Address 0x26870dd4 -> vpdf Hunk 1 Offset 0x00001284
//StackFrame[-2].LR-> Address 0x2686ff34 -> vpdf Hunk 1 Offset 0x000003e4

#ifndef USE_SPLASH
	D(kprintf("Cairo Base:%p. sema:%p, %p, %p\n", CairoBase, &semaphore, foo, &foo);)
#endif

	struct devicecontext *ctx = (struct devicecontext*)calloc(1, sizeof(*ctx));
	if(ctx == NULL)
		return errOpenFile;

	if(cairo_version() > CAIRO_VERSION)
	{
		kprintf("%s: WARNING: build with version %s but used with %s in cairo.library\n",__FUNCTION__,
			CAIRO_VERSION_STRING, cairo_version_string());
	}

	ENTER_SECTION


	try
	{
		int i;
		Object obj;
#ifndef NEW_OBJECTS
		obj.initNull();
#endif

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,83,0)
                // std:unique_ptr ...
		if(globalParams == nullptr)
		{
                        std::unique_ptr<GlobalParams> gp(new GlobalParams);
                        globalParams = std::move(gp);
		}
#else
		if(globalParams == NULL)
		{
			globalParams = std::unique_ptr<GlobalParams>new GlobalParams();
#ifndef POPPLER_GT_006100
			globalParams->setScreenType(screenClustered);
			globalParams->setScreenSize(3);
#endif
			//globalParams->setPSBinary(true);
		}
#endif
		//CachedFile *cachedFile = new CachedFile(new StdCacheLoader(), new GooString(fname));
		D(kprintf("pdf file opened...\n"));
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
#else
		//ctx->stream = new CachedFileStream(cachedFile, 0, gTrue, cachedFile->getLength(), &obj);
#endif
		{
			
			void *mem = NULL;
			int len = 0;

			{
				struct FileInfoBlock fib;
				BPTR lock = Lock(fname, ACCESS_READ);
				BPTR diskFile = (BPTR)NULL;
			
				if (lock)
				{
					if (Examine(lock, &fib))
					{
						len = fib.fib_Size;
					}
					UnLock(lock);
				}
	
				mem = malloc(len);
				if (mem)
				{
					diskFile = Open(fname, MODE_OLDFILE);
					if (diskFile != (BPTR)NULL)
					{
						Read(diskFile, mem, len);
						Close(diskFile);
					}
					else
					{
						free(mem);
						mem = NULL;
					}
				}
			}
		
			if (mem != NULL)
			{
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,5,0)
				Object memobj;
				memobj.setToNull();
#endif
				// use memory stream. previous cached file was doing the same by preloading whole file
				MemStream *memstream = 
#ifndef NEW_OBJECTS
				new MemStream((char*)mem, 0, len, &obj);
#else
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,5,0)
					new MemStream((char*)mem, 0, len, std::move(memobj));			
#else
				new MemStream((char*)mem, 0, len, Object(objNull));
#endif
#endif
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
#else
				//new CachedFileStream(cachedFile, 0, gTrue, cachedFile->getLength(), &obj);
#endif
				ctx->stream = memstream;
				ctx->file_content = mem;
			}
		}
		D(kprintf("pdf stream created..\n"));
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
#else
		//if(cachedFile->getLength() > 0)
#endif
		if (ctx->stream != NULL)
		{
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,3,0)
			if(userpw || ownerpw)
			{
		        const std::optional<GooString> op{ownerpw};	// owner password
	    	    const std::optional<GooString> up{userpw}; // user password

				ctx->doc = new PDFDoc(ctx->stream,op,up);
			}
			else
			{
				ctx->doc = new PDFDoc(ctx->stream);
			}
#else
			ctx->doc = new PDFDoc(ctx->stream, NULL, NULL);
#endif
			D(kprintf("document opened...\n"));
		}

		if(ctx->doc == NULL || !ctx->doc->isOk() || ctx->doc->getNumPages() == 0)
		{
			D(kprintf("failed to open a document:%p...\n", ctx->doc));

			if(ctx->doc)
			{
				if(!ctx->doc->isOk())
				{
					const int errCode = ctx->doc->getErrorCode();

					if(errCode == errEncrypted)
					{
						ret = errCode;
					}
					
				}
			}
			else
			{
				ret = errOpenFile;
			}
			pdfDelete(ctx);
			LEAVE_SECTION
			return ret;
		}

#ifdef USE_SPLASH
		SplashColor paperColor;

		paperColor[0] = 255;
		paperColor[1] = 255;
		paperColor[2] = 255;

		ctx->dev = new SplashOutputDev(splashModeRGB8, 4, gFalse, paperColor);
		ctx->dev->startDoc(ctx->doc);
#else
		ctx->dev = new CairoOutputDev();
		ctx->dev->setPrinting(false);
		ctx->surface = NULL;
		ctx->dev->startDoc(ctx->doc);
#endif
		D(kprintf("media_box:%f,%f\n", ctx->doc->getPageMediaWidth(1), ctx->doc->getPageMediaHeight(1)));
		D(kprintf("media_box:%f, %f\n", pdfGetPageMediaWidth(ctx, 1), pdfGetPageMediaHeight(ctx, 1)));

		NEWLIST(&ctx->search.searchresultlist);

		/* cache some common values to remove need for locking */
		ctx->pagesnum = ctx->doc->getNumPages();
		ctx->documentwidth = ctx->documentheight = 0.0f;

        for(i = 0; i < ctx->pagesnum; i++)
		{
			Page *pdfpage = ctx->doc->getCatalog()->getPage(i + 1);
			int rotate = pdfpage->getRotate();
			float width, height;
			if(rotate == 90 || rotate == 270)
			{
				width = pdfpage->getMediaHeight();
				height = pdfpage->getMediaWidth();
			}
			else
			{
				width = pdfpage->getMediaWidth();
				height = pdfpage->getMediaHeight();
			}

			ctx->documentwidth = std::max(ctx->documentwidth, width);
			ctx->documentheight	= std::max(ctx->documentheight, height);
		}
	}
	catch(...)
	{
		D(kprintf("exception while opening pdf document\n"));
		pdfDelete(ctx);
		LEAVE_SECTION
		ret = errOpenFile;
		return ret;
	}

	LEAVE_SECTION

	if(_pdfctx)
	{
		*_pdfctx = ctx;
	}

	return ret;
}

void *pdfNew(const char *fname)
{
	void *ctx=NULL;
	int ret;

	ret=pdfNewWithOptions(&ctx,fname,NULL,NULL);

	if((ret == errNone) && (ctx != NULL))
	{
		return ctx;
	}

	return NULL;
}

void pdfDelete(void *_ctx)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;

	ENTER_SECTION

	D(kprintf("%s: delete output device:%p. base:%p\n", __FUNCTION__, ctx->dev, CairoBase);)
	if(ctx->dev != NULL)
		delete ctx->dev;

	if (ctx->selection_dev != NULL)
		delete ctx->selection_dev;

#ifndef USE_SPLASH
	D(kprintf("%s: delete surface:%p. cairo:%p base:%p\n", __FUNCTION__, ctx->surface, ctx->cairo, CairoBase);)

	if(ctx->surface != NULL)
	{
		//cairo_create(ctx->surface); <- fm 2021/8/2 make sure the surface reference isn't increased again, otherwise the imagedata is stuck in pixman
		cairo_surface_destroy(ctx->surface);
	}
#endif

	D(kprintf("delete document\n"));
	if(ctx->doc != NULL)
		delete ctx->doc;

	// Stream is disposed with document
	//if (ctx->stream != NULL)
	//	  delete ctx->stream;

	free(ctx->file_content);
	free(ctx);
	//CloseLibrary(CairoBase);
	LEAVE_SECTION
}

float pdfGetPageMediaWidth(void *_ctx, int page)
{
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	//float width = ctx->doc->getPageMediaWidth(page);
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	int rotate = pdfpage->getRotate();
	float width;
	if(rotate == 90 || rotate == 270)
		width = pdfpage->getMediaHeight();
	else
		width = pdfpage->getMediaWidth();
	LEAVE_SECTION
	return width;
}

float pdfGetPageMediaHeight(void *_ctx, int page)
{
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	//float height = ctx->doc->getPageMediaHeight(page);
	int rotate = pdfpage->getRotate();
	float height;
	if(rotate == 90 || rotate == 270)
		height = pdfpage->getMediaWidth();
	else
		height = pdfpage->getMediaHeight();
	LEAVE_SECTION
	return height;
}

int pdfGetPageRotate(void *_ctx, int page)
{
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	int rotate = pdfpage->getRotate();
	LEAVE_SECTION
	return rotate;
}

int pdfGetDocumentDimensions(void *_ctx, float *width, float *height)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;

	*width = ctx->documentwidth;
	*height = ctx->documentheight;
	return 1;
}

int	pdfGetPagesNum(void *_ctx)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	return ctx->pagesnum;
}

static GBool abortcheckcbk_wrapper(void *data)
{
	struct abortcallbackcontext *abortctx = (struct abortcallbackcontext*)data;
	if (abortctx->userfunction != NULL)
		return (GBool)abortctx->userfunction(abortctx->userdata);

	//D(printf("abort check...\n"));
	return 0;
}


static void applyrotation(int *width, int *height, int rotation)
{
	if(rotation == 90 || rotation == 270)
	{
		int t = *height;
		*height = *width;
		*width = t;
	}
}

int pdfDisplayPageSlice(void *_ctx, int page, double scale, int rotate,
						int	useMediaBox, int crop, int printing, enum PdfDisplayMode displayMode, int sliceX, int sliceY, int sliceW, int sliceH, int (*abortcheckcbk)(void *), void *abortcheckcbkdata)
{
	ENTER_SECTION

	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	int cropwidth = (int)pdfGetPageMediaWidth(ctx, page);
	int cropheight = (int)pdfGetPageMediaHeight(ctx, page);
	int width = (int)ceil(cropwidth * scale);
	int height = (int)ceil(cropheight * scale);
	int rc = FALSE;

	applyrotation(&width, &height, rotate);
	applyrotation(&cropwidth, &cropheight, rotate);

#ifdef USE_SPLASH
#else

	if(ctx->surface != NULL)
	{
		if(cairo_image_surface_get_width(ctx->surface) != width || cairo_image_surface_get_height(ctx->surface) != height)
		{
			//cairo_destroy(ctx->cairo);
			cairo_surface_destroy(ctx->surface);
			ctx->surface = NULL;
		}
	}

	if(ctx->surface == NULL)
	{
		ctx->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	}

	if(ctx->surface == NULL)
	{
		LEAVE_SECTION
		return FALSE;
	}

	ctx->cairo = cairo_create(ctx->surface);

	{
		//cairo_font_options_t *options = cairo_font_options_create ();
		//cairo_get_font_options (ctx->cairo, options);
		//cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_SUBPIXEL);
		//cairo_font_options_set_subpixel_order (options, CAIRO_SUBPIXEL_ORDER_BGR);
		//cairo_set_font_options (ctx->cairo, options);
		//cairo_font_options_destroy (options);
	}

	cairo_save(ctx->cairo);
	cairo_set_source_rgba(ctx->cairo, 1., 1., 1., 1);
	cairo_paint(ctx->cairo);
	cairo_restore(ctx->cairo);
	ctx->dev->setCairo(ctx->cairo);
#endif

	//Delay(50);
	//cairo_save(ctx->cairo);
	//printf("render:%d, %d, crop:%d, %d\n", width, height, cropwidth, cropheight);
	try
	{
		struct abortcallbackcontext abortctx;
		abortctx.userfunction = abortcheckcbk;
		abortctx.userdata = abortcheckcbkdata;
		abortctx.ctx = ctx;

		__morphos_global_disable_cms = 0;
		if (displayMode == PdfDisplayMode_Fast)
			__morphos_global_disable_cms = 1;
	
		pdfpage->displaySlice(ctx->dev, 72 * scale, 72 * scale, rotate, gTrue, gFalse, sliceX, sliceY, sliceW, sliceH,
			printing, abortcheckcbk_wrapper, &abortctx);
		rc = TRUE;
	}
	catch(...)
	{
		D(kprintf("Exception during page rendering\n"));
	}

	//cairo_restore(ctx->cairo);
	//Delay(20);

#ifndef USE_SPLASH
	ctx->dev->setCairo(NULL);
	cairo_destroy(ctx->cairo);
	ctx->cairo = NULL;
#endif
	//doc->displayPage(dev, page, hDPI, vDPI, rotate, useMediaBox, crop, printing);
	
	__morphos_global_disable_cms = 0;

	LEAVE_SECTION
	return rc;
}


int pdfDisplayPreviewPageSlice(void *_ctx, int page, double scale, int rotate,
						int	useMediaBox, int crop, int printing, enum PdfDisplayMode displayMode, int sliceX, int sliceY, int sliceW, int sliceH, int (*abortcheckcbk)(void *), void *abortcheckcbkdata)
{
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	int rc = FALSE;
	double ctm[6];
	double *defCTM;
	int	i;
	
	defCTM = (double *)ctx->dev->getDefCTM();

    for (i = 0; i < 6; ++i) {
        ctm[i] = defCTM[i];
    }

	rc = pdfDisplayPageSlice(_ctx, page, scale, rotate, useMediaBox, crop, printing, displayMode, sliceX, sliceY, sliceW, sliceH, abortcheckcbk, abortcheckcbkdata);

	ctx->dev->setDefaultCTM(ctm);

	LEAVE_SECTION
	return rc;
}

unsigned char *pdfGetBitmapRowData(void *_ctx, int row)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
#ifdef USE_SPLASH
	return ctx->dev->getBitmap()->getDataPtr() + ctx->dev->getBitmap()->getRowSize() * row;
#else
	//printf("getrow:%d\n", row);
	unsigned char *data = cairo_image_surface_get_data(ctx->surface);
	data = data + cairo_image_surface_get_stride(ctx->surface) * row;
	//printf("done:%d, %p\n", row, data);
	return data;
#endif
}

int pdfGetBitmapWidth(void *_ctx)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
#ifdef USE_SPLASH
	return ctx->dev->getBitmap()->getWidth();
#else
	return cairo_image_surface_get_width(ctx->surface);
#endif
}

int pdfGetBitmapHeight(void *_ctx)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
#ifdef USE_SPLASH
	return ctx->dev->getBitmap()->getHeight();
#else
	return cairo_image_surface_get_height(ctx->surface);
#endif
}

void pdfGetBitmap(void *_ctx, struct pdfBitmap *bm)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	bm->width = pdfGetBitmapWidth(_ctx);
	bm->height = pdfGetBitmapHeight(_ctx);
	bm->stride = cairo_image_surface_get_stride(ctx->surface);
	bm->data = cairo_image_surface_get_data(ctx->surface);
}

void pdfDisposeRegionForSelection(void *_ctx, struct pdfSelectionRegion *region)
{
	free(region);
}

void pdfDisposeTextForSelection(void *_ctx, struct pdfSelectionText *text)
{
	free(text);
}

struct pdfSelectionRegion *pdfBuildRegionForSelection(void *_ctx, int page, double x1, double y1, double x2, double y2, struct pdfSelectionRegion *previous)
{
	ENTER_SECTION

	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	double cropwidth = pdfGetPageMediaWidth(ctx, page);
	double cropheight = pdfGetPageMediaHeight(ctx, page);

	TextOutputDev *text_dev;
	
	if (ctx->selection_dev == NULL)
		text_dev = ctx->selection_dev = new TextOutputDev(NULL, gTrue, 0, gFalse, gFalse);
	else
		text_dev = ctx->selection_dev;
	
	if (page != ctx->selection_pagenum)
		pdfpage->display(text_dev, 72, 72, 0, gTrue, gTrue, gFalse);
	
	ctx->selection_pagenum = page;
	
	PDFRectangle rect(x1 * cropwidth, y1 * cropheight, x2 * cropwidth, y2 * cropheight);


#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
	std::vector<PDFRectangle*>*list = text_dev->getSelectionRegion(&rect, selectionStyleGlyph, 1.0f);
#else
	GooList *list = text_dev->getSelectionRegion(&rect, selectionStyleGlyph, 1.0f);
#endif	
	/* use gfx.lib region functions to merge all pdf rectangles. this sucks a bit but hopefuly using scaled coords will be fine */
	
	struct Region *region = NewRegion();
	int i;
	const float scale = (float)(1<<12);
	
	D(kprintf("source region:%f, %f, %f, %f\n", x1,y1,x2,y2));
	
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
	for (i = 0; i < (int)list->size(); i++)
#else
	for (i = 0; i < list->getLength(); i++)
#endif
	{
		Rectangle rect;
		
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
		PDFRectangle *selection_rect = (PDFRectangle *) (*list)[i];
#else
		PDFRectangle *selection_rect = (PDFRectangle *) list->get(i);
#endif
		rect.MinX      = (int)(selection_rect->x1 * scale / cropwidth);
		rect.MinY      = (int)(selection_rect->y1 * scale / cropheight);
		rect.MaxX      = rect.MinX + (int)((selection_rect->x2 - selection_rect->x1) * scale / cropwidth);
		rect.MaxY      = rect.MinY + (int)((selection_rect->y2 - selection_rect->y1) * scale / cropheight);
		D(kprintf("reg:%d, %d, %d, %d, %f, %f, %f, %f\n", rect.MinX, rect.MinY, rect.MaxX, rect.MaxY, selection_rect->x1/cropwidth, selection_rect->y1/cropheight, selection_rect->x2/cropwidth, selection_rect->y2/cropheight));
				
		OrRectRegion(region, &rect);
		delete selection_rect;
	}
	
	/* convert gfx.lib region to pdfregion and return to the caller */
	
	delete list;
	
	int numrects = 0;
	RegionRectangle *rrect;
	
	D(kprintf("count regions...\n"));
	for(numrects = 0, rrect = region->RegionRectangle; rrect != NULL; rrect = rrect->Next, numrects++) {}; 
	
	struct pdfSelectionRegion *selection = (struct pdfSelectionRegion*)malloc(sizeof(struct pdfSelectionRegion) + numrects * sizeof(struct pdfSelectionRectangle));
	if (selection != NULL)
	{
		rrect = region->RegionRectangle;
		int rectind = 0;
		D(kprintf("build selection rectangles...\n"));
		while(rrect != NULL)
		{
			//int px, py;
			//int pw, ph;
			
			//int x, y;
			Rectangle rect = rrect->bounds;
			struct pdfSelectionRectangle *pdfrect = &selection->rectangles[rectind++];
			
			rect.MinX += region->bounds.MinX;
			rect.MinY += region->bounds.MinY;
			rect.MaxX += region->bounds.MinX;
			rect.MaxY += region->bounds.MinY;
				
			pdfrect->x1 = rect.MinX / scale;
			pdfrect->y1 = rect.MinY / scale;
			pdfrect->x2 = rect.MaxX / scale;
			pdfrect->y2 = rect.MaxY / scale;
			rrect = rrect->Next;
		}
		
		selection->numrects = numrects;
	}
		
	DisposeRegion(region);
	
	LEAVE_SECTION
	
	return selection;
}

struct pdfSelectionText *pdfBuildTextForSelection(void *_ctx, int page, double x1, double y1, double x2, double y2)
{
	ENTER_SECTION

	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	double cropwidth = pdfGetPageMediaWidth(ctx, page);
	double cropheight = pdfGetPageMediaHeight(ctx, page);

	TextOutputDev *text_dev = new TextOutputDev(NULL, gTrue, 0, gFalse, gFalse);
	pdfpage->display(text_dev, 72, 72, 0, gTrue, gTrue, gFalse);
	PDFRectangle rect(x1 * cropwidth, y1 * cropheight, x2 * cropwidth, y2 * cropheight);

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,1,0)
	GooString text = text_dev->getSelectionText(&rect, selectionStyleGlyph);
		
	delete text_dev;
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
	struct pdfSelectionText *selection = (struct pdfSelectionText*)malloc(sizeof(struct pdfSelectionText) + text.size() + 1);

	if (selection != NULL)
	{
		memcpy(selection->utf8, text.c_str(), text.size());
		selection->utf8[text.size()] = 0;
	}
#else		
	struct pdfSelectionText *selection = (struct pdfSelectionText*)malloc(sizeof(struct pdfSelectionText) + text.getLength() + 1);

	if (selection != NULL)
	{
		memcpy(selection->utf8, text.c_str(), text.getLength());
		selection->utf8[text.getLength()] = 0;
	}
#endif
#else
	GooString *text = text_dev->getSelectionText(&rect, selectionStyleGlyph);
		
	delete text_dev;
		
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
	struct pdfSelectionText *selection = (struct pdfSelectionText*)malloc(sizeof(struct pdfSelectionText) + text->size() + 1);
	if (selection != NULL)
	{
		memcpy(selection->utf8, text->c_str(), text->size());
		selection->utf8[text->size()] = 0;
	}
#else
	struct pdfSelectionText *selection = (struct pdfSelectionText*)malloc(sizeof(struct pdfSelectionText) + text->getLength() + 1);
	if (selection != NULL)
	{
	#ifdef POPPLER_GT_007200
		memcpy(selection->utf8, text->c_str(), text->getLength());
	#else
		memcpy(selection->utf8, text->getCString(), text->getLength());
	#endif
		selection->utf8[text->getLength()] = 0;
	}
#endif
	delete text;
#endif	
	LEAVE_SECTION
	
	return selection;
}

static int actionGetPage(void *_doc, CONSTVOID void *_action)
{
	PDFDoc *doc = (PDFDoc*)_doc;
	LinkAction *link_action = (LinkAction *)_action;

	if(link_action == NULL)
		return 0;

	ENTER_SECTION

//	  printf("linkk:%p\n", link_action);
//	  printf("linkkind:%d\n", link_action->getKind());

	switch(link_action->getKind())
	{
	case actionGoTo:
	{
		LinkGoTo *link_goto = dynamic_cast<LinkGoTo*>(link_action);
		if(link_goto->getNamedDest() != NULL || link_goto->getDest() != NULL)
		{
			//printf("lookup dest for link_goto:%p\n", link_goto)
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,86,0)
			std::unique_ptr<LinkDest> dd;
			CONSTLINKDEST LinkDest *d; // = link_goto->getDest();


			if(link_goto->getDest() == nullptr)
			{
				dd = doc->findDest(link_goto->getNamedDest());

				d = dd.get();
			}
			else
			{
				d = link_goto->getDest();
			}
#else
			CONSTLINKDEST LinkDest *d = link_goto->getDest() != CONSTNULLPTR ? link_goto->getDest() : doc->findDest(link_goto->getNamedDest());
#endif
			if(d != NULL)
			{
				int ret;
				if(d->isPageRef() == FALSE)
				{
					ret = d->getPageNum();
				}
				else
				{
					Ref ref = d->getPageRef();
					//printf("page from ref:%p,%p\n", ref.num, ref.gen);

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
					int page = doc->findPage(ref);
#else
					int page = doc->findPage(ref.num, ref.gen);
#endif
					//printf("found pge:%d\n", page);
					ret = page;
				}
				LEAVE_SECTION
				return ret;
			}

			#if 0
			// this is a case where only getNamedDest() is not NULL but no idea how to handle that yet - kiero

			//char *buff = convertToANSI(link_goto->getNamedDest());
			//printf("%s\n", buff);
			//printf("goto link. isok:%d\n", link_goto->getDest()->isOk());

			if(link_goto->getDest()->isPageRef() == FALSE)
			{
				LEAVE_SECTION
				return link_goto->getDest()->getPageNum();
			}
			else
			{
				Ref ref = link_goto->getDest()->getPageRef();
				//printf("page from ref:%p,%p\n", ref.num, ref.gen);

				int page = doc->findPage(ref.num, ref.gen);
				//printf("found pge:%d\n", page);
				LEAVE_SECTION
				return page;
			}
			#endif
		}
		else
		{
		}
	}
	//action->type = POPPLER_ACTION_GOTO_DEST;
	//build_goto_dest (document, action, dynamic_cast <LinkGoTo *> (link));
	break;
	case actionGoToR:
		//action->type = POPPLER_ACTION_GOTO_REMOTE;
		//build_goto_remote (action, dynamic_cast <LinkGoToR *> (link));
		break;
	case actionLaunch:
		//action->type = POPPLER_ACTION_LAUNCH;
		//build_launch (action, dynamic_cast <LinkLaunch *> (link));
		break;
	case actionURI:
	{
		LinkURI *link_uri = dynamic_cast<LinkURI*>(link_action);
#if 1
		if(link_uri->isOk())
#else
		if(link_uri->getURI() != CONSTSTDSTRNULLPTR)
#endif
		{
			//char *buff = convertToANSI(link_uri->getURI());
			//printf("%s\n", buff);
		}
	}
	//action->type = POPPLER_ACTION_URI;
	//build_uri (action, dynamic_cast <LinkURI *> (link));
	break;
	case actionNamed:
		//action->type = POPPLER_ACTION_NAMED;
		//build_named (action, dynamic_cast <LinkNamed *> (link));
		break;
	case actionMovie:
		//action->type = POPPLER_ACTION_MOVIE;
		//build_movie (action, link);
		break;
	case actionUnknown:
	default:
		//action->type = POPPLER_ACTION_UNKNOWN;
		break;
	}

	LEAVE_SECTION
	return 0;
}

struct OutlineItemNode
{
	struct MinNode n;
	OutlineItem *item;
	struct MinList *children;
	char *title;
	int page;
};

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
static struct MinList *buildoutlineitemlist(PDFDoc *doc, const std::vector<OutlineItem*>*items)
#else
static struct MinList *buildoutlineitemlist(PDFDoc *doc, CONSTGOOLIST GooList *items)
#endif
{
	struct MinList *l = (struct MinList*)calloc(1, sizeof(struct MinList));
	if(l != NULL)
	{
		NEWLIST(l);

		if(items != NULL)
		{

			int i;

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
			for(i = 0; i < (int)items->size(); i++)
#else
			for(i = 0; i < items->getLength(); i++)
#endif
			{
				struct OutlineItemNode *n = (struct OutlineItemNode*)calloc(1, sizeof(*n));

				if(n != NULL)
				{
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
					n->item = (OutlineItem*)(*items)[i];
#else
					n->item = (OutlineItem*)items->get(i);
#endif
					n->item->open();
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(24,2,0)
					n->title = convertUTF32ToANSI(n->item->getTitle());
#else
					n->title = convertUTF32ToANSI(n->item->getTitle(), n->item->getTitleLength());
#endif
					n->page = actionGetPage(doc, n->item->getAction());


					//printf("outline item:%s, page:%d\n", n->title, n->page);

					if(n->item->hasKids())
					{
						//printf("has children, process...\n");
						n->children = buildoutlineitemlist(doc, n->item->getKids());
					}

					ADDTAIL(l, n);
				}
			}
		}
	}

	return l;
}

struct MinList *pdfGetOutlines(void *_ctx)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	struct MinList *l=NULL;
	bool gotitems=false;

	ENTER_SECTION

	try
	{
		Outline	*outline = ctx->doc->getOutline();
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(0,76,0)
		const std::vector<OutlineItem*>*items;
		if(outline->getItems() != nullptr)
		{
			gotitems=TRUE;
			items = outline->getItems();
		}
#else
		CONSTGOOLIST GooList *items = outline->getItems();
#endif
		if(gotitems)
		{
			l = buildoutlineitemlist(ctx->doc, items);
		}

	}
	catch(...)
	{
		kprintf("%s: Exception during pdfGetOutlines\n",__FUNCTION__);
	}

	if(l == NULL)
	{
		l = (struct MinList*)calloc(1, sizeof(struct MinList));
		NEWLIST(l);
	}

	LEAVE_SECTION
	return l;
}

int outlineHasChildren(void *_outline)
{
	struct OutlineItemNode *outline = (struct OutlineItemNode*)_outline;
	return outline->children != NULL ? TRUE : FALSE;
}

struct MinList *outlineGetChildren(void *_outline)
{
	struct OutlineItemNode *outline = (struct OutlineItemNode*)_outline;
	return outline->children;
}


char *outlineGetTitle(void *_outline)
{
	struct OutlineItemNode *outline = (struct OutlineItemNode*)_outline;
	return outline->title;
}

int outlineGetPage(void *_outline)
{
	struct OutlineItemNode *outline = (struct OutlineItemNode*)_outline;
	return outline->page;
}



void
poppler_page_get_size(Page *page,
                      double      *width,
                      double      *height)
{
	double page_width, page_height;
	int rotate;

	ENTER_SECTION

	rotate = page->getRotate();
	if(rotate == 90 || rotate == 270)
	{
		page_height = page->getMediaWidth();
		page_width = page->getMediaHeight();
	}
	else
	{
		page_width = page->getMediaWidth();
		page_height = page->getMediaHeight();
	}

	if(width != NULL)
		*width = page_width;
	if(height != NULL)
		*height = page_height;

	LEAVE_SECTION
}

void pdfConvertUserToDevice(void *_ctx, int page, double* x, double* y)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;

	ENTER_SECTION
	OutputDev *dev = ctx->dev;
	int dx, dy;
	/* copied from pdfConvertDeviceToUser()
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	double ctm[6];
	pdfpage->getDefaultCTM(ctm, 72, 72, 0, gFalse, gFalse);
	dev->setDefaultCTM(ctm);
	*/
	dev->cvtUserToDev(*x, *y, &dx, &dy);
	LEAVE_SECTION
	*x = dx;
	*y = dy;
}

void pdfConvertDeviceToUser(void *_ctx, int page, double x, double y, int *ix, int *iy)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;

	ENTER_SECTION
	OutputDev *dev = ctx->dev;
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	double dx, dy;
	double ctm[6];
	pdfpage->getDefaultCTM(ctm, 72, 72, 0, gFalse, gFalse);
	dev->setDefaultCTM(ctm);
	dev->cvtDevToUser(x, y, &dx, &dy);
	LEAVE_SECTION
	*ix = dx;
	*iy = dy;
}

void pdfLock(void *_ctx)
{
	ENTER_SECTION;
}
void pdfRelease(void *_ctx)
{
	LEAVE_SECTION;
}


void *pdfFindLink(void *_ctx, int pagenum, int x, int y)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	Links *links;
	//Object obj;
	Page *page;

	double width, height;

	ENTER_SECTION

	Catalog *catalog = ctx->doc->getCatalog();
	//printf("catalog:%p\n", catalog);
	if(catalog == NULL)
	{
		LEAVE_SECTION
		return NULL;
	}


	page = catalog->getPage(pagenum);

	if(page == NULL)
	{
		LEAVE_SECTION
		return NULL;
	}



	links = new Links(page->getAnnots());
	//obj.free();

	if(links == NULL)
	{
		LEAVE_SECTION
		return NULL;
	}

	poppler_page_get_size(page, &width, &height);

	if(links != NULL)
	{
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,5,0)
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,6,0)
		std::vector<std::shared_ptr<AnnotLink>> alinks=links->getLinks();
#else
		std::vector<AnnotLink *> alinks=links->getLinks();
#endif
		int num = alinks.size();
#else
		//printf("links:%p\n", links);
		int num = links->getNumLinks();
#endif
		int i;

		//printf("num annots:%d\n", num);

		try
		{

		for(i = 0; i < num; i++)
		{
			LinkAction *link_action;
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,6,0)
			std::shared_ptr<AnnotLink> link;
#else
			AnnotLink *link;
#endif

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,5,0)
			link = alinks[i];
#else
			link = links->getLink(i);
#endif
			link_action = link->getAction();

			if (link_action == NULL)
				continue;

			switch(link_action->getKind())
			{
			case actionGoTo:
			{
				/*
				LinkGoTo *link_goto = dynamic_cast<LinkGoTo*>(link_action);
				if (link_goto->getDest() == NULL)
					  continue;
				if (link_goto->getNamedDest() != NULL) {
					char *buff = convertToANSI(link_goto->getNamedDest());
					printf("%s\n", buff);
				}
				*/
			}
			//action->type = POPPLER_ACTION_GOTO_DEST;
			//build_goto_dest (document, action, dynamic_cast <LinkGoTo *> (link));
			break;
			case actionGoToR:
				//action->type = POPPLER_ACTION_GOTO_REMOTE;
				//build_goto_remote (action, dynamic_cast <LinkGoToR *> (link));
				break;
			case actionLaunch:
				//action->type = POPPLER_ACTION_LAUNCH;
				//build_launch (action, dynamic_cast <LinkLaunch *> (link));
				break;
			case actionURI:
			{
				LinkURI *link_uri = dynamic_cast<LinkURI*>(link_action);
#if 1
				if(link_uri->isOk())
#else
				if(link_uri->getURI() != CONSTSTDSTRNULLPTR)
#endif
				{
					//char *buff = convertToANSI(link_uri->getURI());
					//printf("buff = %s\n", buff);
					//free(buff);
				}
			}
			//action->type = POPPLER_ACTION_URI;
			//build_uri (action, dynamic_cast <LinkURI *> (link));
			break;
			case actionNamed:
				//action->type = POPPLER_ACTION_NAMED;
				//build_named (action, dynamic_cast <LinkNamed *> (link));
				break;
			case actionMovie:
				//action->type = POPPLER_ACTION_MOVIE;
				//build_movie (action, link);
				break;
			case actionUnknown:
			default:
				//action->type = POPPLER_ACTION_UNKNOWN;
				break;
			}

			double x1, y1, x2, y2;
			link->getRect(&x1, &y1, &x2, &y2);
			//printf("area nach getRect(): %f, %f, %f, %f\n", (float)x1, (float)y1, (float)x2, (float)y2);

			pdfConvertUserToDevice(_ctx, pagenum, &x1, &y1);
			pdfConvertUserToDevice(_ctx, pagenum, &x2, &y2);
			//printf("area nach ConvU2D(): %f, %f, %f, %f\n", (float)x1, (float)y1, (float)x2, (float)y2);

			if(y1 > y2)
			{
				double t = y1;
				y1 = y2;
				y2 = t;
			}

			//printf("area: x = %f, y = %f\n", (float)x, (float)y);
			//printf("area:                %f, %f, %f, %f\n", (float)x1, (float)y1, (float)x2, (float)y2);

			if(x1 <= x && x2 >= x && y1 <= y && y2 >= y)
			{
				//kprintf("found link: %p\n", link);
				LEAVE_SECTION
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,6,0)
				return link.get();
#else
				return link;
#endif
			}

			/*
			if (annot->getName() != NULL)
			{
				printf("has name!\n");
				char *buff = convertToANSI(annot->getName());
			}
			if (annot->getContents() != NULL)
			{
				printf("has content\n");
				char *buff = convertToANSI(annot->getContents());
			}
			*/
			//printf("annot%s\n", buff);
			//free(buff);
			//buff = convertToANSI(annot->get());
			//printf("annot%s\n", buff);
			//free(buff);

		}

		}
		catch(...)
		{
			//kprintf("%s: Exception during find link\n",__FUNCTION__);
		}
	}

	delete links;

	LEAVE_SECTION
	return NULL;
}

char *linkGetActionURL(void *_link)
{
	char *url = NULL;
	AnnotLink *link = (AnnotLink*)_link;
	LinkAction *link_action;

	if (link && (link_action = link->getAction()))
	{
		switch(link_action->getKind())
		{
		case actionURI:
		{
			LinkURI *link_uri = dynamic_cast<LinkURI*>(link_action);
#if 1
			if(link_uri->isOk())
#else
			if(link_uri->getURI() != CONSTSTDSTRNULLPTR)
#endif
			{
				url = convertToANSI(link_uri->getURI());
			}
		}
		break;
		default:
			break;
		}
	}
	return url;
}

void pdfListLinks(void *_ctx, int pagenum)
{
	struct devicecontext *ctx = (struct devicecontext*)_ctx;

	Links *links;
	//Object obj;

	double width, height;
	ENTER_SECTION
	Catalog *catalog = ctx->doc->getCatalog();
	//printf("catalog:%p\n", catalog);
	if(catalog == NULL)
	{
		LEAVE_SECTION
		return;
	}

	Page *page = catalog->getPage(pagenum);
	//printf("page:%p\n", page);

	if(page == NULL)
	{
		LEAVE_SECTION
		return;
	}

	links = new Links(page->getAnnots());
	//obj.free();

	if(links == NULL)
	{
		LEAVE_SECTION;
		return;
	}

	poppler_page_get_size(page, &width, &height);

	if(links != NULL)
	{
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,5,0)
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,6,0)
		std::vector<std::shared_ptr<AnnotLink>> alinks=links->getLinks();
#else
		std::vector<AnnotLink *> alinks=links->getLinks();
#endif
		int num = alinks.size();
#else
		//printf("links:%p\n", links);
		int num = links->getNumLinks();
#endif
		int i;

		//printf("num annots:%d\n", num);

		for(i = 0; i < num; i++)
		{
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,6,0)
			std::shared_ptr<AnnotLink> link;
#else
			AnnotLink *link;
#endif

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,5,0)
			link = alinks[i];
#else
			link = links->getLink(i);
#endif

			double x1, y1, x2, y2;
			link->getRect(&x1, &y1, &x2, &y2);

			D(kprintf("area:%f,%f,%f,%f\n", (float)x1, (float)y1, (float)x2, (float)y2));
		}
	}

	//printf("media:%f,%f\n", page->getMediaWidth(), page->getMediaHeight());
	LEAVE_SECTION
	delete links;
}

char *linkGetDescription(void *_link)
{
	AnnotLink *link = (AnnotLink*)_link;
	LinkAction *link_action = link->getAction();

	ENTER_SECTION

	//Printf("linkGetDescription(): getKind() = %ld\n", link_action->getKind());
	switch(link_action->getKind())
	{
	case actionGoTo:
	{
		LinkGoTo *link_goto = dynamic_cast<LinkGoTo*>(link_action);
		if(link_goto->getNamedDest() != NULL)
		{
			char *buff = convertToANSI(link_goto->getNamedDest());
			// getPageNum() returns everytime -8531??
			//Printf("%s. page:%d\n", buff, link_goto->getDest()->getPageNum());
			return buff; // remember that you have to free(buff)!
		}
		else if(link_goto->getDest() != NULL)
		{
			//Printf("reference:%p\n", link_goto->getDest());
		}
	}
	//action->type = POPPLER_ACTION_GOTO_DEST;
	//build_goto_dest (document, action, dynamic_cast <LinkGoTo *> (link));
	break;
	case actionGoToR:
		//action->type = POPPLER_ACTION_GOTO_REMOTE;
		//build_goto_remote (action, dynamic_cast <LinkGoToR *> (link));
		break;
	case actionLaunch:
		//action->type = POPPLER_ACTION_LAUNCH;
		//build_launch (action, dynamic_cast <LinkLaunch *> (link));
		break;
	case actionURI:
	{
		LinkURI *link_uri = dynamic_cast<LinkURI*>(link_action);
#if 1
		if(link_uri->isOk())
#else
		if(link_uri->getURI() != CONSTSTDSTRNULLPTR)
#endif
		{
			char *buff = convertToANSI(link_uri->getURI());
			//printf("%s\n", buff);
			return buff; // remember that you have to free(buff)!
		}
	}
	//action->type = POPPLER_ACTION_URI;
	//build_uri (action, dynamic_cast <LinkURI *> (link));
	break;
	case actionNamed:
		//action->type = POPPLER_ACTION_NAMED;
		//build_named (action, dynamic_cast <LinkNamed *> (link));
		break;
	case actionMovie:
		//action->type = POPPLER_ACTION_MOVIE;
		//build_movie (action, link);
		break;
	case actionUnknown:
	default:
		//action->type = POPPLER_ACTION_UNKNOWN;
		break;
	}

	LEAVE_SECTION
	return NULL;
}

int pdfGetActionPageFromLink(void *_ctx, void *_link)
{
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	AnnotLink *link = (AnnotLink*)_link;
	int page = actionGetPage(ctx->doc, link->getAction());
	LEAVE_SECTION
	return page;
}

static struct searchresult *nextsearchresult(struct devicecontext *ctx, int direction)
{
	if(direction > 0)
	{
		return (struct searchresult*)GetSucc(ctx->search.currentsearchresult);
	}
	else
	{
		return (struct searchresult*)GetPred(ctx->search.currentsearchresult);
	}
}

/* result list is always built in same order. it is traversal order that differs */

int pdfSearch(void *_ctx, int *page, char *phrase, int direction, double *x1, double *y1, double *x2, double *y2)
{
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	int found = FALSE;

//Printf("pdfSearch(): ctx->search.phrase = '%s', ->page = %ld\n", ctx->search.phrase ? ctx->search.phrase : "NULL", ctx->search.page);
	if((ctx->search.phrase == NULL || 0 == strcmp(ctx->search.phrase, phrase)) && ctx->search.page == *page && (ctx->search.currentsearchresult != NULL && nextsearchresult(ctx, direction) == NULL))
	{
		/* if same search criteria, same page but no more results */
//Printf("pdfSearch(): 1\n");

		free(ctx->search.phrase);

		while(!IsListEmpty((struct List*)&ctx->search.searchresultlist))
		{
			struct searchresult *srn = (struct searchresult*)GetHead(&ctx->search.searchresultlist);
			REMOVE(srn);
			free(srn);
		}

		ctx->search.phrase = NULL; //strdup(phrase); // bitRocky: if not NULL, searching on docs with only 1 page, doesn't cycle
		ctx->search.currentsearchresult = NULL;
		LEAVE_SECTION
		return PDFSEARCH_NEXTPAGE; 
	
	}
	else if(ctx->search.phrase == NULL || 0 != strcmp(ctx->search.phrase, phrase) || ctx->search.page != *page)
	{
		/* if new phrase or new page fill the list with new results */
//Printf("pdfSearch(): 2\n");
		free(ctx->search.phrase);

		while(!IsListEmpty((struct List*)&ctx->search.searchresultlist))
		{
			struct searchresult *srn = (struct searchresult*)GetHead(&ctx->search.searchresultlist);
			REMOVE(srn);
			free(srn);
		}

		ctx->search.phrase = strdup(phrase);
		ctx->search.page = *page;
		ctx->search.currentsearchresult = NULL;

		/* new search */

		WCHAR *phraseUCS4 = convertToUCS4(phrase);
		TextOutputDev *text_dev = new TextOutputDev(NULL, gTrue, 0, gFalse, gFalse);
		
		{
			double xMin, yMin, xMax, yMax;
			double height;
			Page *pdfpage = ctx->doc->getCatalog()->getPage(*page);

			pdfpage->display(text_dev, 72, 72, 0,
							 gTrue, gTrue, gFalse);

			height = pdfpage->getMediaHeight();

			xMin = 0;
			yMin = 0;

			while(text_dev->findText((Unicode*)phraseUCS4, strlen(phrase),
			                         gFalse, gTrue, // startAtTop, stopAtBottom
			                         gTrue, gFalse, // startAtLast, stopAtLast
			                         gFalse, gFalse, // caseSensitive, backwards
									 gFalse,        // wholeWord
			                         &xMin, &yMin, &xMax, &yMax))
			{
				//double ctm[6];
				struct searchresult *searchresult = (struct searchresult*)calloc(1, sizeof(*searchresult));
				searchresult->x1 = xMin;
				searchresult->y1 = height - yMax;
				searchresult->x2 = xMax;
				searchresult->y2 = height - yMin;

				text_dev->cvtDevToUser(searchresult->x1, searchresult->y1, &searchresult->x1, &searchresult->y1);
				text_dev->cvtDevToUser(searchresult->x2, searchresult->y2, &searchresult->x2, &searchresult->y2);
				searchresult->x1 /= pdfpage->getMediaWidth();
				searchresult->y1 /= pdfpage->getMediaHeight();
				searchresult->x2 /= pdfpage->getMediaWidth();
				searchresult->y2 /= pdfpage->getMediaHeight();

				ADDTAIL(&ctx->search.searchresultlist, searchresult);
				found = TRUE;
			}
		}
		
		delete text_dev;
		free(phraseUCS4);

		if (found == FALSE)
		{
			LEAVE_SECTION
			return PDFSEARCH_NEXTPAGE;
		}
		
		if (found)
		{
			if(direction > 0)
				ctx->search.currentsearchresult = (struct searchresult*)GetHead(&ctx->search.searchresultlist);
			else
				ctx->search.currentsearchresult = (struct searchresult*)GetTail(&ctx->search.searchresultlist);
				
			ctx->search.page = *page;
		}

	}
	else if(ctx->search.phrase != NULL && 0 == strcmp(ctx->search.phrase, phrase) && ctx->search.page == *page && ctx->search.currentsearchresult != NULL && nextsearchresult(ctx, direction) != NULL)
	{
//Printf("pdfSearch(): 3\n");
		/* same phrase, same page and not empty pool of results */

		ctx->search.currentsearchresult = nextsearchresult(ctx, direction);
		found = TRUE;
	}
//Printf("pdfSearch(): found = %ld\n", found);
	if (ctx->search.currentsearchresult != NULL)
	{
		*x1 = ctx->search.currentsearchresult->x1;
		*y1 = ctx->search.currentsearchresult->y1;
		*x2 = ctx->search.currentsearchresult->x2;
		*y2 = ctx->search.currentsearchresult->y2;
	}

	LEAVE_SECTION
	return found ? PDFSEARCH_FOUND : PDFSEARCH_NOTFOUND;
}

static char *goo_string_to_utf8(CONSTGOOSTRING GooString *s)
{
	char *result;

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(24,5,0)
	if(hasUnicodeMarker(s))
#else
	if(s->hasUnicodeMarker())
#endif
	{
		result = convertToANSI(s);
	}
	else
	{
		int len;
		Unicode *ucs4_temp;
		int i;

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
		len = s->size();
#else
		len = s->getLength();
#endif
		ucs4_temp = (Unicode*)calloc(sizeof(*ucs4_temp), len + 1);
		for(i = 0; i < len; ++i)
		{
			ucs4_temp[i] = pdfDocEncoding[(unsigned char)s->getChar(i)];
		}
		ucs4_temp[i] = 0;

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,10,0)
		result = convertUTF32ToANSI(ucs4_temp, s->size());
#else
		result = convertUTF32ToANSI(ucs4_temp, s->getLength());
#endif
		free(ucs4_temp);
	}

	return result;
}

static char *info_dict_get_string(Dict *info_dict, const char *key)
{
	CONSTGOOSTRING GooString *goo_value;
	char *result;
#ifdef NEW_OBJECTS
	Object obj = info_dict->lookup((char *)key);

	if(!obj.isString())
#else
	Object obj;

	if(!info_dict->lookup((char *)key, &obj)->isString())
#endif
	{
#ifndef NEW_OBJECTS
		obj.free();
#endif
		return NULL;
	}

	goo_value = obj.getString();
	result = goo_string_to_utf8(goo_value);

#ifndef NEW_OBJECTS
	obj.free();
#endif
	return result;
}


int pdfSaveFile(void *_ctx, char *filename, int mode)
{
	int ret = 0;
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx;

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,3,0)
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,9,0)
	std::string gs(filename);
	std::string &gstring = gs;
#else
	GooString gs(filename);
	CONSTGOOSTRING GooString &gstring=gs;
#endif
#else
	GooString *gstring = new GooString(filename);
	if (gstring)
#endif
	{
		ret = ctx->doc->saveAs(gstring, writeStandard);
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,3,0)
#else
		delete gstring;
#endif
	}
	LEAVE_SECTION;
	return ret;

}


static struct pdfAttribute *buildStringAttribute(char *value)
{
	struct pdfAttribute *attr = (struct pdfAttribute*)malloc(sizeof(*attr));
	if(attr != NULL)
	{
		attr->type = PDFATTRTYPE_STRING;
		attr->value.s = value;
	}

	return attr;
}

struct pdfAttribute *pdfGetAttr(void *_ctx, int property)
{
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx;
	struct pdfAttribute *ret = NULL;

#ifdef NEW_OBJECTS
	Object obj = ctx->doc->getDocInfo();
#else
	Object obj;
	ctx->doc->getDocInfo(&obj);
#endif

	if(!obj.isDict())
	{
		LEAVE_SECTION;
		return NULL;
	}

	switch(property)
	{
	case PDFATTR_TITLE:
		ret = buildStringAttribute(info_dict_get_string(obj.getDict(), "Title"));
		break;
	};

	LEAVE_SECTION;
	return ret;

}

void pdfFreeAttr(void *_ctx, struct pdfAttribute *attr)
{
	if(attr != NULL)
	{
		if(attr->value.s != NULL && attr->type == PDFATTRTYPE_STRING)
			free(attr->value.s);

		free(attr);
	}
}

/* annotations */

struct MinList *pdfGetAnnotations(void *_ctx, int page)
{
	ENTER_SECTION
	struct devicecontext *ctx = (struct devicecontext*)_ctx; 
	Page *pdfpage = ctx->doc->getCatalog()->getPage(page);
	Annots *annots = pdfpage->getAnnots();

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,5,0)
	int num;
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,6,0)
	std::vector<std::shared_ptr<Annot>> _annots;
#else
	std::vector<Annot *> _annots;
#endif
	if (annots == NULL)
	{
		LEAVE_SECTION
		return NULL;
	}

	_annots = annots->getAnnots();

	if(_annots.size() == 0)
	{
		LEAVE_SECTION
		return NULL;
	}
#else
	if (annots == NULL || annots->getNumAnnots() == 0)
	{
		LEAVE_SECTION
		return NULL;
	}
#endif

	struct MinList *l = (struct MinList*)calloc(1, sizeof(*l));
	if (l == NULL)
	{	
		LEAVE_SECTION
		return NULL;
	}	
	
	NEWLIST(l);
	
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,5,0)
	num = _annots.size();
	for(int i=0; i<num; i++)
#else
	for(int i=0; i<annots->getNumAnnots(); i++)
#endif
	{
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,5,0)
#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(25,6,0)
		std::shared_ptr<Annot> pdfAnnot = _annots[i];
#else
		Annot *pdfAnnot = _annots[i];
#endif
#else
		Annot *pdfAnnot = annots->getAnnot(i);
#endif
		struct pdfAnnotation *annot = NULL;
		
		switch (pdfAnnot->getType ())
		{
			case Annot::typeText:
			case Annot::typeLink:
			case Annot::typeFreeText:
			case Annot::typeLine:
			case Annot::typeSquare:
			case Annot::typeCircle:
			case Annot::typePolygon:
			case Annot::typePolyLine:
			case Annot::typeHighlight:
			case Annot::typeUnderline:
			case Annot::typeSquiggly:
			case Annot::typeStrikeOut:
			case Annot::typeStamp:
			case Annot::typeCaret:
			case Annot::typeInk:
			case Annot::typePopup:
			case Annot::typeFileAttachment:
			case Annot::typeSound:
			case Annot::typeMovie:
			case Annot::typeWidget:
			case Annot::typeScreen:
			case Annot::typePrinterMark:
			case Annot::typeTrapNet:
			case Annot::typeWatermark:
			case Annot::type3D:
				if (pdfAnnot->getContents() != NULL)
				{
					annot = (struct pdfAnnotation*)calloc(1, sizeof(*annot));
					if (annot != NULL)
					{
						annot->type = 0; // TODO
						//annot->author = convertToANSI(pdfAnnot->getAuthor());
						annot->contents = convertToANSI(pdfAnnot->getContents());
					}
				}
				break;
			default:
			{
			}
		}
		
		if (annot != NULL)
		{
			int rotation = 0;
			double width = pdfGetPageMediaWidth(_ctx, page);
			double height = pdfGetPageMediaHeight(_ctx, page);
			PDFRectangle rect;
			
			if (!(pdfAnnot->getFlags () & Annot::flagNoRotate))
				rotation = pdfpage->getRotate();

#if POPPLER_VERSION >= POPPLER_VERSION_ENCODE(22,3,0)
			rect = pdfAnnot->getRect();
#else
			rect = *(pdfAnnot->getRect());
#endif
			switch (rotation)
			{
				case 90:
					annot->x1 = rect.y1;
					annot->y1 = height - rect.x2;
					annot->x2 = annot->x1 + (rect.y2 - rect.y1);
					annot->y2 = annot->y1 + (rect.x2 - rect.x1);
					break;
				case 180:
					annot->x1 = width - rect.x2;
					annot->y1 = height - rect.y2;
					annot->x2 = annot->x1 + (rect.x2 - rect.x1);
					annot->y2 = annot->y1 + (rect.y2 - rect.y1);
					break;
				case 270:
					annot->x1 = width - rect.y2;
					annot->y1 = rect.x1;
					annot->x2 = annot->x1 + (rect.y2 - rect.y1);
					annot->y2 = annot->y1 + (rect.x2 - rect.x1);
					break;
				default:
					annot->x1 = rect.x1;
					annot->y1 = rect.y1;
					annot->x2 = rect.x2;
					annot->y2 = rect.y2;
			}
			annot->y1 = height - annot->y1;
			annot->y2 = height - annot->y2;		

			annot->x1 /= pdfpage->getMediaWidth();
			annot->x2 /= pdfpage->getMediaWidth();
			annot->y1 /= pdfpage->getMediaHeight();
			annot->y2 /= pdfpage->getMediaHeight();

			ADDTAIL(l, annot);
		}
	}
		

	LEAVE_SECTION
	return l;
}

