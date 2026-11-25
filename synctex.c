/*
 * Compact SyncTeX reader for VPDF.
 *
 * Supports plain and gzip compressed .synctex files.  The gzip decompressor
 * is included here so that VPDF does not need an additional library.
 *
 * Only the information needed for forward and inverse search is kept: input
 * file names and the records with their source line and page coordinates.
 * Coordinates returned by this module are big points (1/72 inch) with the
 * origin in the top left corner of the page.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "synctex.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define ST_MAX_SYNCTEX_SIZE (128L * 1024L * 1024L)
#define ST_MISSING LONG_MIN

#include <limits.h>

/* ------------------------------------------------------------------ */
/* gzip / deflate                                                       */
/* ------------------------------------------------------------------ */

struct st_bits
{
	const unsigned char *data;
	size_t size;
	size_t pos;
	unsigned int buffer;
	int count;
};

struct st_output
{
	unsigned char *data;
	size_t len;
	size_t cap;
};

struct st_huffman
{
	short count[16];
	short first[16];
	short index[16];
	short symbol[288];
};

static int st_getbit(struct st_bits *b)
{
	int bit;

	if (b->count == 0)
	{
		if (b->pos >= b->size)
			return -1;
		b->buffer = b->data[b->pos++];
		b->count = 8;
	}

	bit = (int)(b->buffer & 1);
	b->buffer >>= 1;
	b->count--;
	return bit;
}

static int st_getbits(struct st_bits *b, int n)
{
	int value = 0;
	int i;

	for (i = 0; i < n; i++)
	{
		int bit = st_getbit(b);
		if (bit < 0)
			return -1;
		value |= bit << i;
	}
	return value;
}

static void st_align_byte(struct st_bits *b)
{
	b->buffer = 0;
	b->count = 0;
}

static int st_huffman_init(struct st_huffman *h, const unsigned char *lengths, int num)
{
	int bits;
	int code = 0;
	int i;
	int left = 1;
	int maxbits = 0;

	memset(h, 0, sizeof(*h));

	for (i = 0; i < num; i++)
	{
		if (lengths[i] > 15)
			return 0;
		if (lengths[i] > 0)
		{
			if (lengths[i] > maxbits)
				maxbits = lengths[i];
			h->count[lengths[i]]++;
		}
	}

	/* verify the code set is not over-subscribed */
	for (bits = 1; bits <= 15; bits++)
	{
		left <<= 1;
		left -= h->count[bits];
		if (left < 0)
			return 0;
	}

	/* incomplete code sets are only valid with a single one-bit code */
	if (left > 0 && maxbits != 1)
		return 0;

	for (bits = 1; bits <= 15; bits++)
	{
		code = (code + h->count[bits - 1]) << 1;
		h->first[bits] = (short)code;
		h->index[bits] = (short)(h->index[bits - 1] + h->count[bits - 1]);
	}

	for (i = 0; i < num; i++)
	{
		if (lengths[i] != 0)
			h->symbol[h->index[lengths[i]]++] = (short)i;
	}

	for (bits = 1; bits <= 15; bits++)
		h->index[bits] = (short)(h->index[bits - 1] + h->count[bits - 1]);

	return 1;
}

static int st_huffman_decode(struct st_bits *b, const struct st_huffman *h)
{
	int code = 0;
	int bits;

	for (bits = 1; bits <= 15; bits++)
	{
		int bit = st_getbit(b);
		if (bit < 0)
			return -1;

		code = (code << 1) | bit;
		if (code - h->first[bits] >= 0 && code - h->first[bits] < h->count[bits])
			return h->symbol[h->index[bits] + code - h->first[bits]];
	}

	return -1;
}

static int st_emit(struct st_output *out, unsigned char byte)
{
	if (out->len >= out->cap)
	{
		size_t cap = out->cap ? out->cap * 2 : 65536;
		unsigned char *data;

		if (cap > ST_MAX_SYNCTEX_SIZE)
			cap = ST_MAX_SYNCTEX_SIZE;
		if (cap <= out->len)
			return 0;

		data = (unsigned char *)realloc(out->data, cap);
		if (data == NULL)
			return 0;
		out->data = data;
		out->cap = cap;
	}

	out->data[out->len++] = byte;
	return 1;
}

static int st_emit_copy(struct st_output *out, size_t distance, size_t length)
{
	size_t i;

	if (distance == 0 || distance > out->len)
		return 0;

	for (i = 0; i < length; i++)
	{
		if (!st_emit(out, out->data[out->len - distance]))
			return 0;
	}
	return 1;
}

static const short st_length_base[29] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
	31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const short st_length_extra[29] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
	2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const short st_dist_base[30] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
	193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
	6145, 8193, 12289, 16385, 24577
};
static const short st_dist_extra[30] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
	6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};
static const unsigned char st_code_length_order[19] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static int st_inflate_block(struct st_bits *b, struct st_output *out,
	struct st_huffman *lit, struct st_huffman *dist)
{
	for (;;)
	{
		int symbol = st_huffman_decode(b, lit);

		if (symbol < 0)
			return 0;

		if (symbol < 256)
		{
			if (!st_emit(out, (unsigned char)symbol))
				return 0;
		}
		else if (symbol == 256)
		{
			return 1;
		}
		else
		{
			int index = symbol - 257;
			int length, dsymbol, extra;
			size_t distance;

			if (index < 0 || index >= 29)
				return 0;

			extra = st_getbits(b, st_length_extra[index]);
			if (extra < 0)
				return 0;
			length = st_length_base[index] + extra;

			dsymbol = st_huffman_decode(b, dist);
			if (dsymbol < 0 || dsymbol >= 30)
				return 0;

			extra = st_getbits(b, st_dist_extra[dsymbol]);
			if (extra < 0)
				return 0;
			distance = (size_t)st_dist_base[dsymbol] + (size_t)extra;

			if (!st_emit_copy(out, distance, (size_t)length))
				return 0;
		}
	}
}

static int st_dynamic_tables(struct st_bits *b, struct st_huffman *lit, struct st_huffman *dist)
{
	unsigned char lengths[288 + 32];
	unsigned char clengths[19];
	struct st_huffman code_tree;
	int hlit, hdist, hclen;
	int total, i;
	int code, extra, repeat;
	unsigned char prev;

	hlit = st_getbits(b, 5);
	hdist = st_getbits(b, 5);
	hclen = st_getbits(b, 4);
	if (hlit < 0 || hdist < 0 || hclen < 0)
		return 0;
	hlit += 257;
	hdist += 1;
	hclen += 4;

	if (hlit > 288 || hdist > 32 || hclen > 19)
		return 0;

	memset(clengths, 0, sizeof(clengths));
	for (i = 0; i < hclen; i++)
	{
		code = st_getbits(b, 3);
		if (code < 0)
			return 0;
		clengths[st_code_length_order[i]] = (unsigned char)code;
	}

	if (!st_huffman_init(&code_tree, clengths, 19))
		return 0;

	memset(lengths, 0, sizeof(lengths));
	total = hlit + hdist;
	i = 0;
	prev = 0;

	while (i < total)
	{
		code = st_huffman_decode(b, &code_tree);
		if (code < 0)
			return 0;

		if (code < 16)
		{
			lengths[i++] = (unsigned char)code;
			prev = (unsigned char)code;
		}
		else if (code == 16)
		{
			extra = st_getbits(b, 2);
			if (extra < 0 || i == 0)
				return 0;
			repeat = 3 + extra;
			while (repeat-- > 0 && i < total)
				lengths[i++] = prev;
		}
		else if (code == 17)
		{
			extra = st_getbits(b, 3);
			if (extra < 0)
				return 0;
			repeat = 3 + extra;
			while (repeat-- > 0 && i < total)
				lengths[i++] = 0;
			prev = 0;
		}
		else if (code == 18)
		{
			extra = st_getbits(b, 7);
			if (extra < 0)
				return 0;
			repeat = 11 + extra;
			while (repeat-- > 0 && i < total)
				lengths[i++] = 0;
			prev = 0;
		}
		else
		{
			return 0;
		}
	}

	return st_huffman_init(lit, lengths, hlit) &&
		st_huffman_init(dist, lengths + hlit, hdist);
}

static int st_fixed_tables(struct st_huffman *lit, struct st_huffman *dist)
{
	unsigned char lengths[288 + 32];
	int i;

	for (i = 0; i < 144; i++) lengths[i] = 8;
	for (; i < 256; i++) lengths[i] = 9;
	for (; i < 280; i++) lengths[i] = 7;
	for (; i < 288; i++) lengths[i] = 8;
	for (i = 0; i < 30; i++) lengths[288 + i] = 5;

	return st_huffman_init(lit, lengths, 288) &&
		st_huffman_init(dist, lengths + 288, 30);
}

static int st_inflate(struct st_bits *b, struct st_output *out)
{
	int final;

	do
	{
		int type = st_getbits(b, 1);
		if (type < 0)
			return 0;
		final = type;
		type = st_getbits(b, 2);
		if (type < 0)
			return 0;

		if (type == 0)
		{
			int len, nlen;

			st_align_byte(b);
			if (b->pos + 4 > b->size)
				return 0;
			len = b->data[b->pos] | (b->data[b->pos + 1] << 8);
			nlen = b->data[b->pos + 2] | (b->data[b->pos + 3] << 8);
			b->pos += 4;
			if ((len ^ 0xffff) != nlen || b->pos + len > b->size)
				return 0;
			while (len-- > 0)
			{
				if (!st_emit(out, b->data[b->pos++]))
					return 0;
			}
		}
		else if (type == 1 || type == 2)
		{
			struct st_huffman lit, dist;
			int ok;

			if (type == 1)
				ok = st_fixed_tables(&lit, &dist);
			else
				ok = st_dynamic_tables(b, &lit, &dist);

			if (!ok || !st_inflate_block(b, out, &lit, &dist))
				return 0;
		}
		else
		{
			return 0;
		}
	} while (!final);

	return 1;
}

static unsigned char *st_gunzip(const unsigned char *data, size_t size, size_t *decodedSize)
{
	struct st_bits bits;
	struct st_output out;
	size_t pos;
	int flags;

	memset(&out, 0, sizeof(out));

	if (size < 18 || data[0] != 0x1f || data[1] != 0x8b || data[2] != 8)
		return NULL;

	flags = data[3];
	pos = 10;

	if (flags & 4) /* FEXTRA */
	{
		size_t extra;
		if (pos + 2 > size)
			return NULL;
		extra = (size_t)(data[pos] | (data[pos + 1] << 8));
		pos += 2 + extra;
		if (pos > size)
			return NULL;
	}
	if (flags & 8) /* FNAME */
	{
		while (pos < size && data[pos] != 0) pos++;
		if (pos >= size) return NULL;
		pos++;
	}
	if (flags & 16) /* FCOMMENT */
	{
		while (pos < size && data[pos] != 0) pos++;
		if (pos >= size) return NULL;
		pos++;
	}
	if (flags & 2) /* FHCRC */
	{
		pos += 2;
		if (pos > size)
			return NULL;
	}

	bits.data = data;
	bits.size = size;
	bits.pos = pos;
	bits.buffer = 0;
	bits.count = 0;

	if (!st_inflate(&bits, &out))
	{
		free(out.data);
		return NULL;
	}

	if (!st_emit(&out, 0))
	{
		free(out.data);
		return NULL;
	}

	*decodedSize = out.len - 1;
	return out.data;
}

/* ------------------------------------------------------------------ */
/* helpers                                                              */
/* ------------------------------------------------------------------ */

static char *st_strdup(const char *s)
{
	size_t len = strlen(s) + 1;
	char *copy = (char *)malloc(len);
	if (copy != NULL)
		memcpy(copy, s, len);
	return copy;
}

static int st_strcasecmp(const char *a, const char *b)
{
	while (*a != 0 && *b != 0)
	{
		int ca = tolower((unsigned char)*a);
		int cb = tolower((unsigned char)*b);
		if (ca != cb)
			return ca < cb ? -1 : 1;
		a++;
		b++;
	}
	if (*a == 0 && *b == 0)
		return 0;
	return *a == 0 ? -1 : 1;
}

static unsigned char *st_read_file(const char *path, size_t *size)
{
	FILE *f = fopen(path, "rb");
	unsigned char *data = NULL;
	size_t len = 0;
	size_t cap = 0;

	*size = 0;
	if (f == NULL)
		return NULL;

	for (;;)
	{
		size_t got;

		if (len == cap)
		{
			size_t newcap = cap ? cap * 2 : 16384;
			unsigned char *tmp;

			if (newcap > ST_MAX_SYNCTEX_SIZE)
				newcap = ST_MAX_SYNCTEX_SIZE;
			if (newcap <= len)
				break;
			tmp = (unsigned char *)realloc(data, newcap);
			if (tmp == NULL)
				break;
			data = tmp;
			cap = newcap;
		}

		got = fread(data + len, 1, cap - len, f);
		len += got;
		if (got == 0)
			break;
	}

	fclose(f);
	*size = len;

	if (data == NULL)
	{
		data = (unsigned char *)malloc(1);
		if (data != NULL)
			data[0] = 0;
	}
	return data;
}

static char *st_synctex_path(const char *pdfPath, const char *suffix, int quoteLast)
{
	const char *slash = NULL;
	const char *dot;
	const char *p;
	char *result;
	size_t prefixlen;

	if (pdfPath == NULL)
		return NULL;

	for (p = pdfPath; *p != 0; p++)
	{
		if (*p == '/' || *p == '\\' || *p == ':')
			slash = p;
	}
	dot = strrchr(pdfPath, '.');
	if (dot != NULL && (slash == NULL || dot > slash))
		prefixlen = (size_t)(dot - pdfPath);
	else
		prefixlen = strlen(pdfPath);

	if (quoteLast)
	{
		const char *base = slash ? slash + 1 : pdfPath;
		size_t dirlen = (size_t)(base - pdfPath);

		result = (char *)malloc(prefixlen + strlen(suffix) + 3);
		if (result == NULL)
			return NULL;
		memcpy(result, pdfPath, dirlen);
		result[dirlen] = '"';
		memcpy(result + dirlen + 1, base, prefixlen - dirlen);
		result[prefixlen + 1] = '"';
		strcpy(result + prefixlen + 2, suffix);
	}
	else
	{
		result = (char *)malloc(prefixlen + strlen(suffix) + 1);
		if (result == NULL)
			return NULL;
		memcpy(result, pdfPath, prefixlen);
		strcpy(result + prefixlen, suffix);
	}

	return result;
}

static char *st_load_synctex(const char *pdfPath, size_t *size)
{
	static const char *suffixes[] = { ".synctex", ".synctex.gz", NULL };
	int pass;

	*size = 0;

	/* pdftex sometimes quotes the last path component of the sidecar file */
	for (pass = 0; pass < 2; pass++)
	{
		int i;

		for (i = 0; suffixes[i] != NULL; i++)
		{
			char *path = st_synctex_path(pdfPath, suffixes[i], pass);
			unsigned char *data;

			if (path == NULL)
				return NULL;

			data = st_read_file(path, size);
			free(path);

			if (data != NULL && *size >= 2)
			{
				if (data[0] == 0x1f && data[1] == 0x8b)
				{
					unsigned char *text;
					size_t textSize;

					text = st_gunzip(data, *size, &textSize);
					free(data);
					if (text != NULL)
					{
						*size = textSize;
						return (char *)text;
					}
					continue;
				}
				return (char *)data;
			}
			free(data);
		}
	}

	return NULL;
}

/* ------------------------------------------------------------------ */
/* synctex data model                                                   */
/* ------------------------------------------------------------------ */

struct st_input
{
	char *name;
};

struct st_record
{
	char kind;
	int tag;
	int line;
	int column;
	int page;
	int parent;            /* index of the enclosing box, -1 for top level */
	long h, v, w, height, depth;
};

struct st_scanner
{
	struct st_input *inputs;
	int numInputs;

	struct st_record *records;
	int numRecords;
	int recordCap;

	double unit;
	double xOffset;
	double yOffset;
};

static int st_grow_records(struct st_scanner *s)
{
	if (s->numRecords < s->recordCap)
		return 1;

	{
		int cap = s->recordCap ? s->recordCap * 2 : 256;
		struct st_record *records = (struct st_record *)realloc(s->records,
			(size_t)cap * sizeof(*records));
		if (records == NULL)
			return 0;
		s->records = records;
		s->recordCap = cap;
	}
	return 1;
}

/* dimensions in the post scriptum are converted to scaled points */
static long st_dimension_to_sp(const char *text)
{
	char *end;
	double value = strtod(text, &end);
	double factor = 65536.0;

	while (*end == ' ' || *end == '\t')
		end++;

	if (strncmp(end, "in", 2) == 0) factor = 72.27 * 65536.0;
	else if (strncmp(end, "cm", 2) == 0) factor = 72.27 * 65536.0 / 2.54;
	else if (strncmp(end, "mm", 2) == 0) factor = 72.27 * 65536.0 / 25.4;
	else if (strncmp(end, "pt", 2) == 0) factor = 65536.0;
	else if (strncmp(end, "bp", 2) == 0) factor = 72.27 / 72.0 * 65536.0;
	else if (strncmp(end, "pc", 2) == 0) factor = 12.0 * 65536.0;
	else if (strncmp(end, "sp", 2) == 0) factor = 1.0;

	return (long)(value * factor);
}

static void st_set_input(struct st_scanner *s, int tag, const char *name)
{
	struct st_input *inputs;

	if (tag <= 0 || name == NULL || *name == 0)
		return;

	if (tag >= s->numInputs)
	{
		inputs = (struct st_input *)realloc(s->inputs,
			(size_t)(tag + 1) * sizeof(*inputs));
		if (inputs == NULL)
			return;
		memset(inputs + s->numInputs, 0,
			(size_t)(tag + 1 - s->numInputs) * sizeof(*inputs));
		s->inputs = inputs;
		s->numInputs = tag + 1;
	}

	free(s->inputs[tag].name);
	s->inputs[tag].name = st_strdup(name);
}

static int st_parse_int(const char **cursor, long *value)
{
	const char *p = *cursor;
	char *end;

	while (*p == ':' || *p == ',')
		p++;

	if (*p != '-' && (*p < '0' || *p > '9'))
		return 0;

	*value = strtol(p, &end, 10);
	if (end == p)
		return 0;
	*cursor = end;
	return 1;
}

static int st_parse_v(const char **cursor, long lastV, long *value)
{
	const char *p = *cursor;

	while (*p == ':' || *p == ',')
	{
		if (*p == ',' && p[1] == '=')
		{
			*cursor = p + 2;
			*value = lastV;
			return 1;
		}
		p++;
	}

	if (*p != '-' && (*p < '0' || *p > '9'))
		return 0;

	*value = strtol(p, (char **)cursor, 10);
	return 1;
}

static int st_is_record_kind(char c)
{
	return c == '[' || c == '(' || c == 'v' || c == 'h' || c == 'k' ||
		c == 'g' || c == 'r' || c == '$' || c == 'c' || c == 'x' || c == 'f';
}

static int st_is_box(char c)
{
	return c == '[' || c == '(' || c == 'v' || c == 'h';
}

static int st_is_open_box(char c)
{
	return c == '[' || c == '(';
}

/* Kern and rule records are useful for inverse search, but for forward
 * search they often point at invisible glue near a paragraph boundary. */
static int st_is_primary_leaf(char c)
{
	return c != 'k' && c != 'r' && c != 'x';
}

static int st_parse_record(struct st_scanner *s, const char *line, int page,
	long *lastV, int parent)
{
	struct st_record record;
	const char *p = line + 1;
	long value;

	if (page <= 0 || !st_is_record_kind(line[0]))
		return -1;

	memset(&record, 0, sizeof(record));
	record.kind = line[0];
	record.page = page;
	record.parent = parent;

	if (!st_parse_int(&p, &value)) return -1;
	record.tag = (int)value;
	if (!st_parse_int(&p, &value)) return -1;
	record.line = (int)value;

	record.column = -1;
	if (*p == ',')
	{
		if (!st_parse_int(&p, &value)) return -1;
		record.column = (int)value;
	}

	if (*p != ':')
		return -1;
	if (!st_parse_int(&p, &record.h)) return -1;
	if (!st_parse_v(&p, *lastV, &record.v)) return -1;
	*lastV = record.v;

	if (*p == ':')
	{
		if (!st_parse_int(&p, &record.w)) return -1;
		if (*p == ',' && st_parse_int(&p, &record.height))
		{
			if (*p == ',')
				(void)st_parse_int(&p, &record.depth);
		}
	}

	if (record.tag <= 0 || record.line <= 0)
		return -1;

	if (!st_grow_records(s))
		return -1;

	s->records[s->numRecords++] = record;
	return s->numRecords - 1;
}

static void st_parse_text(struct st_scanner *s, char *text)
{
	char *cursor = text;
	int page = 0;
	int inContent = FALSE;
	int inPostScriptum = FALSE;
	long lastV = 0;
	long preUnit = 1;
	long preMag = 1000;
	long preX = 0;
	long preY = 0;
	long postMag = 0;
	long postX = 0;
	long postY = 0;
	int havePostX = FALSE;
	int havePostY = FALSE;
	int boxstack[256];
	int boxdepth = 0;

	while (*cursor != 0)
	{
		char *line = cursor;
		char *end = strchr(cursor, '\n');
		char *p;

		if (end != NULL)
		{
			*end = 0;
			cursor = end + 1;
		}
		else
		{
			cursor += strlen(cursor);
		}

		p = line + strlen(line);
		if (p > line && p[-1] == '\r')
			p[-1] = 0;

		if (strncmp(line, "Input:", 6) == 0)
		{
			int tag = (int)strtol(line + 6, &p, 10);
			if (p != line + 6 && *p == ':')
			{
				p++;
				if (*p == '"')
				{
					char *close;
					p++;
					close = strrchr(p, '"');
					if (close != NULL)
						*close = 0;
				}
				st_set_input(s, tag, p);
			}
		}
		else if (!inPostScriptum && strncmp(line, "Unit:", 5) == 0)
		{
			long unit = strtol(line + 5, NULL, 10);
			if (unit > 0)
				preUnit = unit;
		}
		else if (!inPostScriptum && strncmp(line, "Magnification:", 14) == 0)
		{
			long mag = strtol(line + 14, NULL, 10);
			if (mag > 0)
				preMag = mag;
		}
		else if (!inPostScriptum && strncmp(line, "X Offset:", 9) == 0)
		{
			preX = strtol(line + 9, NULL, 10);
		}
		else if (!inPostScriptum && strncmp(line, "Y Offset:", 9) == 0)
		{
			preY = strtol(line + 9, NULL, 10);
		}
		else if (strncmp(line, "Post scriptum:", 14) == 0 ||
			strncmp(line, "Postscriptum:", 13) == 0)
		{
			inPostScriptum = TRUE;
		}
		else if (inPostScriptum && strncmp(line, "Magnification:", 14) == 0)
		{
			double mag = strtod(line + 14, NULL);
			if (mag > 0)
				postMag = (long)mag;
		}
		else if (inPostScriptum && strncmp(line, "X Offset:", 9) == 0)
		{
			postX = st_dimension_to_sp(line + 9);
			havePostX = TRUE;
		}
		else if (inPostScriptum && strncmp(line, "Y Offset:", 9) == 0)
		{
			postY = st_dimension_to_sp(line + 9);
			havePostY = TRUE;
		}
		else if (strncmp(line, "Content:", 8) == 0)
		{
			inContent = TRUE;
		}
		else if (inContent)
		{
			if (line[0] == '{')
			{
				page = (int)strtol(line + 1, NULL, 10);
				lastV = 0;
				boxdepth = 0;
			}
			else if (line[0] == '}' || line[0] == ')' || line[0] == ']')
			{
				if (boxdepth > 0)
					boxdepth--;
			}
			else if (st_is_record_kind(line[0]))
			{
				int parent = boxdepth > 0 ? boxstack[boxdepth - 1] : -1;
				int index = st_parse_record(s, line, page, &lastV, parent);

				if (index >= 0 && st_is_open_box(line[0]) && boxdepth < 256)
				{
					boxstack[boxdepth++] = index;
				}
			}
		}
	}

	/* final tuning of the coordinate conversion, as in the reference parser */
	s->unit = (postMag > 0 ? (double)postMag : (double)preUnit) / 65781.76;
	s->unit *= (double)preMag / 1000.0;

	if (havePostX)
		s->xOffset = (double)postX / 65781.76;
	else
		s->xOffset = (double)preX * (double)preUnit / 65781.76;

	if (havePostY)
		s->yOffset = (double)postY / 65781.76;
	else
		s->yOffset = (double)preY * (double)preUnit / 65781.76;
}

/* ------------------------------------------------------------------ */
/* queries                                                              */
/* ------------------------------------------------------------------ */

static char *st_normalize(const char *name)
{
	char *copy = st_strdup(name);
	char *out;
	const char *in;
	size_t i;

	if (copy == NULL)
		return NULL;

	in = copy;
	while (in[0] == '.' && (in[1] == '/' || in[1] == '\\'))
		in += 2;

	out = (char *)malloc(strlen(in) + 1);
	if (out == NULL)
	{
		free(copy);
		return NULL;
	}

	i = 0;
	while (*in != 0)
	{
		out[i++] = (*in == '\\') ? '/' : *in;
		in++;
	}
	out[i] = 0;
	free(copy);
	return out;
}

static int st_path_tail_match(const char *stored, const char *wanted)
{
	char *a = st_normalize(stored);
	char *b = st_normalize(wanted);
	int match = 0;
	size_t la, lb;

	if (a == NULL || b == NULL)
	{
		free(a);
		free(b);
		return 0;
	}

	la = strlen(a);
	lb = strlen(b);

	if (st_strcasecmp(a, b) == 0)
	{
		match = 1;
	}
	else if (lb > 0 && la >= lb &&
		st_strcasecmp(a + la - lb, b) == 0 &&
		(a[la - lb] == '/' || b[0] == '/'))
	{
		match = 1;
	}

	free(a);
	free(b);
	return match;
}

static int st_tag_for_file(struct st_scanner *s, const char *file)
{
	int i;
	char *wantedBase;
	const char *slash;

	if (s == NULL || file == NULL)
		return -1;

	for (i = 1; i < s->numInputs; i++)
	{
		if (s->inputs[i].name != NULL && st_path_tail_match(s->inputs[i].name, file))
			return i;
	}

	slash = strrchr(file, '/');
	if (slash == NULL)
		slash = strrchr(file, '\\');
	wantedBase = st_strdup(slash ? slash + 1 : file);
	if (wantedBase == NULL)
		return -1;

	for (i = 1; i < s->numInputs; i++)
	{
		const char *name = s->inputs[i].name;
		const char *base;

		if (name == NULL)
			continue;
		base = strrchr(name, '/');
		if (base == NULL)
			base = strrchr(name, '\\');
		base = base ? base + 1 : name;
		if (st_strcasecmp(base, wantedBase) == 0)
		{
			free(wantedBase);
			return i;
		}
	}

	free(wantedBase);
	return -1;
}

static void st_record_bounds(struct st_scanner *s, const struct st_record *r,
	double *x1, double *y1, double *x2, double *y2)
{
	double h = (double)r->h * s->unit + s->xOffset;
	double v = (double)r->v * s->unit + s->yOffset;
	double w = (double)r->w * s->unit;
	double height = (double)r->height * s->unit;
	double depth = (double)r->depth * s->unit;

	if (w <= 0)
		w = (r->kind == 'x') ? 0.1 : 2.0;

	/* Container records store their real height and depth; zero depth is
	 * perfectly valid and must not be inflated. */
	if (!st_is_open_box(r->kind) || r->height <= 0)
	{
		if (height <= 0)
			height = 5.0;
		if (depth <= 0)
			depth = 2.0;
	}

	*x1 = h;
	*y1 = v - height;
	*x2 = h + w;
	*y2 = v + depth;
}

static int st_count_line_descendants(struct st_scanner *s, int box, int tag,
	int line, int page)
{
	int count = 0;
	int i;

	for (i = 0; i < s->numRecords; i++)
	{
		int index = i;

		if (s->records[i].tag != tag || s->records[i].line != line ||
			s->records[i].page != page || st_is_box(s->records[i].kind))
			continue;

		while (index >= 0)
		{
			if (index == box)
			{
				count++;
				break;
			}
			index = s->records[index].parent;
		}
	}

	return count;
}

static int st_best_enclosing_box(struct st_scanner *s, int index, int tag, int line)
{
	int best = -1;
	int bestCount = -1;
	double bestArea = 0;

	while (index >= 0 && index < s->numRecords)
	{
		struct st_record *box = &s->records[index];

		/* The full line rectangle lives in an hbox close to the source
		 * line.  Page-level vboxes are far too large for a marker. */
		if (box->kind == '(' && abs(box->line - line) <= 2)
		{
			double x1, y1, x2, y2;
			int count = st_count_line_descendants(s, index, tag, line, box->page);

			st_record_bounds(s, box, &x1, &y1, &x2, &y2);

			if (count > bestCount ||
				(count == bestCount && (x2 - x1) * (y2 - y1) > bestArea))
			{
				bestCount = count;
				bestArea = (x2 - x1) * (y2 - y1);
				best = index;
			}
		}
		index = box->parent;
	}

	return best;
}

static int st_kind_penalty(char kind)
{
	if (kind == 'x' || kind == 'g' || kind == 'k' || kind == 'r' ||
		kind == '$' || kind == 'c' || kind == 'f')
		return 0;
	if (kind == 'h' || kind == 'v')
		return 2;
	if (kind == '(')
		return 3;
	return 4;
}

void *synctexOpen(const char *pdfPath)
{
	struct st_scanner *s;
	size_t size;
	char *text;

	if (pdfPath == NULL)
		return NULL;

	text = st_load_synctex(pdfPath, &size);
	if (text == NULL)
		return NULL;

	s = (struct st_scanner *)calloc(1, sizeof(*s));
	if (s == NULL)
	{
		free(text);
		return NULL;
	}

	st_parse_text(s, text);
	free(text);

	if (s->numRecords == 0 || s->numInputs <= 1)
	{
		synctexClose(s);
		return NULL;
	}

	return s;
}

void synctexClose(void *scanner)
{
	struct st_scanner *s = (struct st_scanner *)scanner;
	int i;

	if (s == NULL)
		return;

	for (i = 0; i < s->numInputs; i++)
		free(s->inputs[i].name);
	free(s->inputs);
	free(s->records);
	free(s);
}

int synctexIsOpen(void *scanner)
{
	return scanner != NULL;
}

int synctexForward(void *scanner, const char *file, int line, int column,
	int pageHint, struct SynctexHit *hit)
{
	struct st_scanner *s = (struct st_scanner *)scanner;
	int tag;
	int i;
	int best = -1;
	int bestLine = 0;
	int bestPage = 0;
	double bestScore = 1e30;
	double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	int found = FALSE;
	int pass;

	if (s == NULL || hit == NULL || file == NULL || line <= 0)
		return FALSE;

	(void)column; /* column information is optional in SyncTeX files */

	tag = st_tag_for_file(s, file);
	if (tag <= 0)
		return FALSE;

	/* Prefer leaf records with an exact line match.  Boxes carry the line
	 * of their opening command, which is often one line off from the text
	 * they contain, so they are only used as a fallback.  Invisible kern and
	 * rule records are a lower priority than real boxes. */
	for (pass = 0; pass < 6 && best < 0; pass++)
	{
		for (i = 0; i < s->numRecords; i++)
		{
			struct st_record *r = &s->records[i];
			double rx1, ry1, rx2, ry2;
			double distance, score, area;
			int wantBox = pass == 1 || pass == 3 || pass == 5;
			int exactOnly = pass < 2;

			if (r->tag != tag || r->line <= 0)
				continue;

			if (st_is_box(r->kind) != wantBox)
				continue;
			if (pass == 5 && (r->height <= 0 && r->depth <= 0))
				continue;
			if (wantBox && pass != 5 && (r->height <= 0 && r->depth <= 0))
				continue;
			if (!wantBox && pass < 4 && !st_is_primary_leaf(r->kind))
				continue;

			distance = (double)abs(r->line - line);
			if (exactOnly && distance > 0)
				continue;
			if (!exactOnly && pass < 4 && distance > 10)
				continue;

			st_record_bounds(s, r, &rx1, &ry1, &rx2, &ry2);
			area = (rx2 - rx1) * (ry2 - ry1);

			score = distance * 100000.0;
			score += (double)abs(r->page - pageHint) * 1000.0;
			score += st_kind_penalty(r->kind) * 10.0;
			score += area / 1000.0;

			if (score < bestScore)
			{
				bestScore = score;
				best = i;
				bestLine = r->line;
				bestPage = r->page;
			}
		}
	}

	if (best < 0)
		return FALSE;

	/* Highlight the enclosing box of the hit, when available.  This gives
	 * VPDF the same full-line rectangle that the SyncTeX tool reports. */
	{
		int box = st_best_enclosing_box(s, best, tag, bestLine);

		if (box >= 0)
		{
			st_record_bounds(s, &s->records[box], &x1, &y1, &x2, &y2);
			found = TRUE;
		}
	}

	/* No usable box: union the leaf records of the selected line. */
	if (!found)
	{
		for (i = 0; i < s->numRecords; i++)
		{
			struct st_record *r = &s->records[i];
			double rx1, ry1, rx2, ry2;

			if (r->tag != tag || r->line != bestLine || r->page != bestPage)
				continue;

			st_record_bounds(s, r, &rx1, &ry1, &rx2, &ry2);
			if (!found)
			{
				x1 = rx1; y1 = ry1; x2 = rx2; y2 = ry2;
				found = TRUE;
			}
			else
			{
				if (rx1 < x1) x1 = rx1;
				if (ry1 < y1) y1 = ry1;
				if (rx2 > x2) x2 = rx2;
				if (ry2 > y2) y2 = ry2;
			}
		}
	}

	if (!found)
		return FALSE;

	memset(hit, 0, sizeof(*hit));
	hit->page = bestPage;
	hit->x = x1;
	hit->y = y1;
	hit->width = x2 - x1;
	hit->height = y2 - y1;
	hit->line = bestLine;
	hit->column = s->records[best].column;
	if (s->inputs[tag].name != NULL)
		hit->file = st_strdup(s->inputs[tag].name);

	return TRUE;
}

static double st_point_to_rect_distance(double x, double y,
	double x1, double y1, double x2, double y2)
{
	double dx = 0;
	double dy = 0;

	if (x < x1) dx = x1 - x;
	else if (x > x2) dx = x - x2;
	if (y < y1) dy = y1 - y;
	else if (y > y2) dy = y - y2;

	return sqrt(dx * dx + dy * dy);
}

int synctexInverse(void *scanner, int page, double x, double y,
	struct SynctexHit *hit)
{
	struct st_scanner *s = (struct st_scanner *)scanner;
	int i;
	int pass;
	int best = -1;
	double bestScore = 1e30;
	double bx1 = 0, by1 = 0, bx2 = 0, by2 = 0;

	if (s == NULL || hit == NULL || page <= 0)
		return FALSE;

	/* Glyph and kern records are the most precise hits.  If the point is in
	 * a margin or other empty area, fall back to boxes even when a distant
	 * glyph exists. */
	for (pass = 0; pass < 2; pass++)
	{
		if (pass == 1 && best >= 0 && bestScore <= 40.0)
			break;

		for (i = 0; i < s->numRecords; i++)
		{
			struct st_record *r = &s->records[i];
			double rx1, ry1, rx2, ry2;
			double distance, score, area;

			if (r->page != page || r->tag <= 0 || r->line <= 0)
				continue;
			if (pass == 0 && st_is_box(r->kind))
				continue;
			if (pass == 1 && !st_is_box(r->kind))
				continue;

			st_record_bounds(s, r, &rx1, &ry1, &rx2, &ry2);
			distance = st_point_to_rect_distance(x, y, rx1, ry1, rx2, ry2);
			area = (rx2 - rx1) * (ry2 - ry1);

			/* prefer the smallest record containing the point */
			score = distance + st_kind_penalty(r->kind) * 0.5 + sqrt(area) * 0.01;

			if (score < bestScore)
			{
				bestScore = score;
				best = i;
				bx1 = rx1; by1 = ry1; bx2 = rx2; by2 = ry2;
			}
		}
	}

	if (best < 0)
		return FALSE;

	{
		struct st_record *r = &s->records[best];

		memset(hit, 0, sizeof(*hit));
		hit->page = page;
		hit->x = bx1;
		hit->y = by1;
		hit->width = bx2 - bx1;
		hit->height = by2 - by1;
		hit->line = r->line;
		hit->column = r->column;
		if (r->tag < s->numInputs && s->inputs[r->tag].name != NULL)
			hit->file = st_strdup(s->inputs[r->tag].name);
	}

	return TRUE;
}

static int st_page_rotation(int rotation)
{
	rotation %= 360;
	if (rotation < 0)
		rotation += 360;
	return rotation;
}

void synctexPageToDisplay(int rotation, double width, double height,
	double x, double y, double *displayX, double *displayY)
{
	switch (st_page_rotation(rotation))
	{
		case 90:
			*displayX = height - y;
			*displayY = x;
			break;
		case 180:
			*displayX = width - x;
			*displayY = height - y;
			break;
		case 270:
			*displayX = y;
			*displayY = width - x;
			break;
		default:
			*displayX = x;
			*displayY = y;
			break;
	}
}

void synctexDisplayToPage(int rotation, double width, double height,
	double displayX, double displayY, double *x, double *y)
{
	switch (st_page_rotation(rotation))
	{
		case 90:
			*x = displayY;
			*y = height - displayX;
			break;
		case 180:
			*x = width - displayX;
			*y = height - displayY;
			break;
		case 270:
			*x = width - displayY;
			*y = displayX;
			break;
		default:
			*x = displayX;
			*y = displayY;
			break;
	}
}

void synctexFreeHit(struct SynctexHit *hit)
{
	if (hit == NULL)
		return;
	free(hit->file);
	hit->file = NULL;
}
