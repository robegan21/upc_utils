/* The MIT License

   Copyright (c) 2008, 2009, 2011 Attractive Chaos <attractor@live.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/* Last Modified: 05MAR2012 */

#ifndef AC_KSEQ_H
#define AC_KSEQ_H

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define KS_SEP_SPACE 0 // isspace(): \t, \n, \v, \f, \r
#define KS_SEP_TAB   1 // isspace() && !' '
#define KS_SEP_LINE  2 // line separator: "\n" (Unix) or "\r\n" (Windows)
#define KS_SEP_MAX   2

#define __KS_TYPE(type_t)						\
	typedef struct __kstream_t {				\
		unsigned char *buf;						\
		int begin, end, is_eof;					\
		size_t file_offset, file_size;			\
		type_t f;								\
	} kstream_t;


#define ks_eof(ks) ((ks)->is_eof && (ks)->begin >= (ks)->end)
#define ks_rewind(ks) ((ks)->is_eof = (ks)->begin = (ks)->end = 0)
#define ks_tell(ks) ((ks)->file_offset - (ks)->end + (ks)->begin)

#define __KS_BASIC(type_t, __bufsize)								\
	static inline kstream_t *ks_init(type_t f)						\
	{																\
		kstream_t *ks = (kstream_t*)calloc(1, sizeof(kstream_t));	\
		ks->f = f;													\
		ks->buf = (unsigned char*)malloc(__bufsize);				\
		return ks;													\
	}																\
	static inline void ks_destroy(kstream_t *ks)					\
	{																\
		if (ks) {													\
			free(ks->buf);											\
			free(ks);												\
		}															\
	}

#define __KS_GETC(__read, __bufsize)						\
	static inline int ks_getc(kstream_t *ks)				\
	{														\
		if (ks->is_eof && ks->begin >= ks->end) return -1;	\
		if (ks->begin >= ks->end) {							\
			ks->begin = 0;									\
			ks->end = __read(ks->f, ks->buf, __bufsize);	\
			ks->file_offset += ks->end;							\
			if (ks->end == 0) { ks->is_eof = 1; return -1;}	\
		}													\
		return (int)ks->buf[ks->begin++];					\
	}

#ifndef KSTRING_T
#define KSTRING_T kstring_t
typedef struct __kstring_t {
	size_t l, m;
	char *s;
} kstring_t;
static void __kstring_swap(kstring_t *a, kstring_t *b) {
	kstring_t tmp = *a;
	*a = *b;
	*b = tmp;
}
#endif

#ifndef kroundup32
#define kroundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))
#endif

#define __KS_GETUNTIL(__read, __bufsize)								\
	static int ks_getuntil2(kstream_t *ks, int delimiter, kstring_t *str, int *dret, int append) \
	{																	\
		int gotany = 0;													\
		if (dret) *dret = 0;											\
		str->l = append? str->l : 0;									\
		for (;;) {														\
			int i;														\
			if (ks->begin >= ks->end) {									\
				if (!ks->is_eof) {										\
					ks->begin = 0;										\
					ks->end = __read(ks->f, ks->buf, __bufsize);		\
					ks->file_offset += ks->end;							\
					if (ks->end == 0) { ks->is_eof = 1; break; }		\
				} else break;											\
			}															\
			if (delimiter == KS_SEP_LINE) { \
				for (i = ks->begin; i < ks->end; ++i) \
					if (ks->buf[i] == '\n') break; \
			} else if (delimiter > KS_SEP_MAX) {						\
				for (i = ks->begin; i < ks->end; ++i)					\
					if (ks->buf[i] == delimiter) break;					\
			} else if (delimiter == KS_SEP_SPACE) {						\
				for (i = ks->begin; i < ks->end; ++i)					\
					if (isspace(ks->buf[i])) break;						\
			} else if (delimiter == KS_SEP_TAB) {						\
				for (i = ks->begin; i < ks->end; ++i)					\
					if (isspace(ks->buf[i]) && ks->buf[i] != ' ') break; \
			} else i = 0; /* never come to here! */						\
			if (str->m - str->l < (size_t)(i - ks->begin + 1)) {		\
				str->m = str->l + (i - ks->begin) + 1;					\
				kroundup32(str->m);										\
				str->s = (char*)realloc(str->s, str->m);				\
			}															\
			gotany = 1;													\
			memcpy(str->s + str->l, ks->buf + ks->begin, i - ks->begin); \
			str->l = str->l + (i - ks->begin);							\
			ks->begin = i + 1;											\
			if (i < ks->end) {											\
				if (dret) *dret = ks->buf[i];							\
				break;													\
			}															\
		}																\
		if (!gotany && ks_eof(ks)) return -1;							\
		if (str->s == 0) {												\
			str->m = 1;													\
			str->s = (char*)calloc(1, 1);								\
		} else if (delimiter == KS_SEP_LINE && str->l > 1 && str->s[str->l-1] == '\r') --str->l; \
		str->s[str->l] = '\0';											\
		return str->l;													\
	} \
	static inline int ks_getuntil(kstream_t *ks, int delimiter, kstring_t *str, int *dret) \
	{ return ks_getuntil2(ks, delimiter, str, dret, 0); }

#define KSTREAM_INIT(type_t, __read, __bufsize) \
	__KS_TYPE(type_t)							\
	__KS_BASIC(type_t, __bufsize)				\
	__KS_GETC(__read, __bufsize)				\
	__KS_GETUNTIL(__read, __bufsize)

#define kseq_rewind(ks) ((ks)->last_char = (ks)->f->is_eof = (ks)->f->begin = (ks)->f->end = 0)

#define __KSEQ_BASIC(SCOPE, type_t)										\
	SCOPE kseq_t *kseq_init(type_t fd)									\
	{																	\
		kseq_t *s = (kseq_t*)calloc(1, sizeof(kseq_t));					\
		s->f = ks_init(fd);												\
		return s;														\
	}																	\
	SCOPE void kseq_destroy(kseq_t *ks)									\
	{																	\
		if (!ks) return;												\
		free(ks->name.s); free(ks->comment.s); free(ks->seq.s);	free(ks->qual.s); \
		ks_destroy(ks->f);												\
		free(ks);														\
	}

/* Return value:
   >=0  length of the sequence (normal)
   -1   end-of-file
   -2   truncated quality string
 */
#define __KSEQ_READ(SCOPE) \
	SCOPE int kseq_read(kseq_t *seq) \
	{ \
		int c; \
		kstream_t *ks = seq->f; \
		if (seq->last_char == 0) { /* then jump to the next header line */ \
			while ((c = ks_getc(ks)) != -1 && c != '>' && c != '@'); \
			if (c == -1) return -1; /* end of file */ \
			seq->last_char = c; \
		} /* else: the first header char has been read in the previous call */ \
		seq->comment.l = seq->seq.l = seq->qual.l = 0; /* reset all members */ \
		if (ks_getuntil(ks, 0, &seq->name, &c) < 0) return -1; /* normal exit: EOF */ \
		if (c != '\n') ks_getuntil(ks, KS_SEP_LINE, &seq->comment, 0); /* read FASTA/Q comment */ \
		if (seq->seq.s == 0) { /* we can do this in the loop below, but that is slower */ \
			seq->seq.m = 256; \
			seq->seq.s = (char*)malloc(seq->seq.m); \
		} \
		while ((c = ks_getc(ks)) != -1 && c != '>' && c != '+' && c != '@') { \
			if (c == '\n') continue; /* skip empty lines */ \
			seq->seq.s[seq->seq.l++] = c; /* this is safe: we always have enough space for 1 char */ \
			ks_getuntil2(ks, KS_SEP_LINE, &seq->seq, 0, 1); /* read the rest of the line */ \
		} \
		if (c == '>' || c == '@') seq->last_char = c; /* the first header char has been read */	\
		if (seq->seq.l + 1 >= seq->seq.m) { /* seq->seq.s[seq->seq.l] below may be out of boundary */ \
			seq->seq.m = seq->seq.l + 2; \
			kroundup32(seq->seq.m); /* rounded to the next closest 2^k */ \
			seq->seq.s = (char*)realloc(seq->seq.s, seq->seq.m); \
		} \
		seq->seq.s[seq->seq.l] = 0;	/* null terminated string */ \
		if (c != '+') return seq->seq.l; /* FASTA */ \
		if (seq->qual.m < seq->seq.m) {	/* allocate memory for qual in case insufficient */ \
			seq->qual.m = seq->seq.m; \
			seq->qual.s = (char*)realloc(seq->qual.s, seq->qual.m); \
		} \
		while ((c = ks_getc(ks)) != -1 && c != '\n'); /* skip the rest of '+' line */ \
		if (c == -1) return -2; /* error: no quality string */ \
		while (ks_getuntil2(ks, KS_SEP_LINE, &seq->qual, 0, 1) >= 0 && seq->qual.l < seq->seq.l); \
		seq->last_char = 0;	/* we have not come to the next header line */ \
		if (seq->seq.l != seq->qual.l) return -2; /* error: qual string is of a different length */ \
		return seq->seq.l; \
	}


#define __KSEQ_INIT_SEEK(SCOPE, __seek) \
	SCOPE int kseq_isseq(char *str, int len) \
	{ /* returns true if the set of characters could be a fasta sequence */ \
		int i; \
		char c; \
		for(i = 0; i<len; i++) { \
			c = str[i]; \
			if ( !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '.' || c == '-' || c == '*' ) ) \
				return 0; \
		} \
		return 1; \
	}\
	SCOPE size_t kseq_nextRecordFasta(kseq_t *seq) \
	{ /* find and seek to the next record after the last byte accessed */\
		size_t offset = 0; \
		int c; \
		kstream_t *ks = seq->f; \
		while ((c = ks_getc(ks)) != 1 && c != '>') offset++; \
		if (c == -1) return -1; /* end of file */ \
		seq->last_char = c; \
		return offset; \
	} \
	SCOPE size_t kseq_nextRecordFastq(kseq_t *seq, size_t filepos) \
	{ /* find and seek to the next record after the last byte accessed */ \
		size_t offset = 0, recordLen = 0, maxLines = 8; \
		int i, c; \
		kstream_t *ks = seq->f; \
		kstring_t lines[4];\
		for(i = 0; i < 4; i++) { \
			lines[i].l = lines[i].m = 0; \
			lines[i].s = NULL; \
			if (ks_getuntil(ks, KS_SEP_LINE, &lines[i], &c) < 0) { \
				while (i) { free(lines[i--].s); } \
				return -1; /* normal exit: EOF */ \
			} \
		} \
		for(i = 0; i < maxLines; i++) { \
			if (lines[0].l > 0 && lines[1].l > 0 && lines[2].l > 0 && lines[3].l > 0) { \
				if (lines[0].s[0] == '@' && kseq_isseq(lines[1].s, lines[1].l) \
					&& lines[2].s[0] == '+' && lines[1].l == lines[3].l) { \
					recordLen = lines[0].l + lines[1].l + lines[2].l + lines[3].l + 4; \
					if (ks->begin > recordLen) { \
						ks->begin -= recordLen; \
						seq->last_char = 0; \
					} else { \
						ks_rewind(ks); \
						__seek(ks->f, filepos + offset, SEEK_SET); \
						ks->file_offset = filepos + offset; \
					} \
					break; /* found the record start */ \
				} \
			} \
			offset += lines[0].l + 1; \
			__kstring_swap(lines+0, lines+1); \
			__kstring_swap(lines+1, lines+2); \
			__kstring_swap(lines+2, lines+3); \
			if (ks_getuntil(ks, KS_SEP_LINE, &lines[3], &c) < 0) { /* eof */ offset = -1; break; } \
		} \
		for(c = 0; c < 4; c++) free(lines[c].s); \
		if (i == maxLines) return -1; \
		else return offset; \
	} \
	SCOPE void ks_setFileSize(kstream_t *ks) \
	{ /* set the ks->file_size attribute */ \
		size_t prev, size; \
		if (ks->file_size > 0) return; \
		prev = __seek(ks->f, 0, SEEK_CUR); \
		size = __seek(ks->f, 0L, SEEK_END); \
		if (__seek(ks->f, prev, SEEK_SET) != prev) { \
			fprintf(stderr, "Problem seeking in the kseq file!\n"); \
			return; \
		} \
		ks->file_size = size; \
	} \
	SCOPE size_t kseq_seek(kseq_t *seq, size_t filepos, char recordSep) \
	{ /* seek to the next record at or after filepos */ \
		size_t offset = 0; \
		if (recordSep != '@' && recordSep != '>') { \
			fprintf(stderr, "Invalid use of kseq_seek.  Must choose '>' or '@' for recordSep\n"); \
			filepos = 0; \
		} \
		kstream_t *ks = seq->f; \
		ks_setFileSize(ks); \
		ks_rewind(ks); \
		__seek(ks->f, filepos, SEEK_SET); \
		ks->file_offset = filepos; \
		offset = recordSep == '@' ? kseq_nextRecordFastq(seq, filepos) : kseq_nextRecordFasta(seq); \
		if (offset >= 0) { \
			return filepos + offset; \
		} else { \
			return -1; \
		} \
	}

/* TODO add pairing identification and is_interleaved method or variable at init... */

#define __KSEQ_TYPE(type_t)						\
	typedef struct {							\
		kstring_t name, comment, seq, qual;		\
		int last_char;							\
		kstream_t *f;							\
	} kseq_t;

#define KSEQ_INIT2(SCOPE, type_t, __read)		\
	KSTREAM_INIT(type_t, __read, 16384)			\
	__KSEQ_TYPE(type_t)							\
	__KSEQ_BASIC(SCOPE, type_t)					\
	__KSEQ_READ(SCOPE)

#define KSEQ_INIT(type_t, __read) KSEQ_INIT2(static, type_t, __read)

#define KSEQ_INIT_SEEKABLE(type_t, __read, __fseek) \
	KSEQ_INIT2(static, type_t, __read)          \
	__KSEQ_INIT_SEEK(static, __fseek)

#define KSEQ_DECLARE(type_t) \
	__KS_TYPE(type_t) \
	__KSEQ_TYPE(type_t) \
	extern kseq_t *kseq_init(type_t fd); \
	void kseq_destroy(kseq_t *ks); \
	int kseq_read(kseq_t *seq);

#define KSEQ_DECLARE_SEEKABLE(type_t) \
	KSEQ_DECLARE(type_t) \
	size_t kseq_seek(kseq_t *seq, size_t filepos, char recordSep);

#endif
