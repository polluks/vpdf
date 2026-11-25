#ifndef SYNCTEX_H
#define SYNCTEX_H

/*
 * Minimal SyncTeX reader used by VPDF.
 *
 * Coordinates exposed here are big points (1/72 inch), with the origin in
 * the top left corner of the page -- the same convention used by the
 * `synctex` command line tool and by VPDF's page markers.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct SynctexHit
{
	int page;          /* 1 based page number */
	double x, y;       /* top left corner of the match, in points */
	double width, height;
	char *file;        /* allocated with malloc(), release with synctexFreeHit() */
	int line;          /* 1 based source line, -1 when unknown */
	int column;        /* 1 based source column, -1 when unknown */
};

/* Locates and parses <pdf>.synctex or <pdf>.synctex.gz next to the PDF.
 * Returns NULL when no usable SyncTeX data was found. */
void *synctexOpen(const char *pdfPath);
void synctexClose(void *scanner);
int synctexIsOpen(void *scanner);

/* Source file + line -> PDF location. Returns TRUE on success. */
int synctexForward(void *scanner, const char *file, int line, int column, int pageHint, struct SynctexHit *hit);

/* PDF page + point (top left, points) -> source location. Returns TRUE on success. */
int synctexInverse(void *scanner, int page, double x, double y, struct SynctexHit *hit);

/* Convert between SyncTeX's unrotated page coordinates and the coordinates
 * of the page as it is displayed.  width/height are the unrotated media box
 * dimensions; rotation is the PDF /Rotate value in degrees. */
void synctexPageToDisplay(int rotation, double width, double height,
	double x, double y, double *displayX, double *displayY);
void synctexDisplayToPage(int rotation, double width, double height,
	double displayX, double displayY, double *x, double *y);

void synctexFreeHit(struct SynctexHit *hit);

#ifdef __cplusplus
}
#endif

#endif /* SYNCTEX_H */
