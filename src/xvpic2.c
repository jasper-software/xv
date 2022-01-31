/*
 * $Id: xvpic2.c,v 2.9.1.14 1995/04/24 15:34:15 ikeyan Exp $
 * xvpic2.c - load and save routines for `PIC2' format pictures.
 *
 *
 * Outline
 * =======
 * xvpic2.c supports the PIC2 format image file. It is used some
 * Japanese personal computer users.
 *
 * The PIC2 format is designed by A.Yanagisawa. It is an excellent
 * format except for its encode/decode speed. ;-)
 *
 * The features of the PIC2 format:
 * - Powerful header information (included author, filename, title,
 *   saver, product number, created date and comment).
 * - Reversible compression, and very high compression ratio (in many
 *   cases, a higher compression ratio than the JPEG compression;
 *   because of its compression method, PIC2 is especially good at
 *   pictures like cell animation).
 * - Can handle full-color (24 bits) image.
 * - Can include multi image blocks into one PIC2 file.
 * - Have four different block format (P2SS, P2SF, P2BM and
 *   P2BI). P2SS format uses arithmetic compression for storing
 *   data. P2SF uses normal run-length compression. P2BM and P2BI is
 *   raw image format. Select any one according to the situation.
 *
 * Explanation of the PIC2 compression:

 * - In the first place, try to record pixel color, uses color caches
 *   which keep some recent colors, and formed according to color's
 *   frequency.  PIC2 has some color cache spaces that are switched by
 *   upper pixel value of current pixel.  If cache is hit, record
 *   that.
 * - Unfortunately, in the case of color cache didn't hit, record the
 *   difference from the value estimated with the value of upper and
 *   left pixel of current pixel (similar to PNG's AVG predictor).
 * - And extract image's color chain if exist, and record that (it
 *   results in image's outline).
 * - In all cases, arithmetic compression is used in the final stage
 *   before writing the file, which in theory produces the ideal
 *   compression ratio (P2SS).
 *
 * Features
 * ========
 * - Support 3,6,9,12,15,18,21,24bit PIC2 format (Load/Save).
 * - Support all image block formats of PIC2 (Load/Save).
 * - Support multi block PIC2 file (Load/Save).
 *
 *
 * Bugs
 * ====
 * - Unsupport 8bit PIC2 image file.
 *
 * If you find other bugs (surely exist :-)), send me bug-report.
 *
 *
 * Author
 * ======
 * IKEMOTO Masahiro <ikeyan@airlab.cs.ritsumei.ac.jp>
 */

#define PIC2_IGNORE_UNUSED_FUNCTIONS
#define NEEDSDIR

#include "xv.h"
#include <setjmp.h>

#ifdef HAVE_PIC2

typedef unsigned long pixel;

#define pic2_cextoshort(addr) ( \
    (((short) (((byte *) addr)[0])) <<  8) | \
    ( (short) (((byte *) addr)[1])) \
)
#define pic2_cextolong(addr) ( \
    (((long)  (((byte *) addr)[0])) << 24) | \
    (((long)  (((byte *) addr)[1])) << 16) | \
    (((long)  (((byte *) addr)[2])) <<  8) | \
    ( (long)  (((byte *) addr)[3])) \
)
#define pic2_shorttocex(addr, n) { \
    ((byte *) addr)[0] = (((unsigned short) (n) >>  8) & 0xff); \
    ((byte *) addr)[1] = ( (unsigned short) (n)        & 0xff); \
}
#define pic2_longtocex(addr, n) { \
    ((byte *) addr)[0] = (((unsigned long)  (n) >> 24) & 0xff); \
    ((byte *) addr)[1] = (((unsigned long)  (n) >> 16) & 0xff); \
    ((byte *) addr)[2] = (((unsigned long)  (n) >>  8) & 0xff); \
    ((byte *) addr)[3] = ( (unsigned long)  (n)        & 0xff); \
}
#define pic2_shift_bits(b, n) (((n) > 0) ? ((b) << (n)) : ((b) >> -(n)))

#define PIC2_READ_MODE		0
#define PIC2_WRITE_MODE		1

#define PIC2_ARITH_CACHE	32
#define PIC2_ARITH_CONTEXT	128
#define PIC2_FAST_CACHE		64

#define PIC2_HEADER_SIZE	124
#define PIC2_BLOCK_HEADER_SIZE	26

struct pic2_header {
    char magic[4];
    char name[18];
    char subtitle[8];
    char crlf0[2];
    char title[30];
    char crlf1[2];
    char saver[30];
    char crlf2[2];
    char eof[1];
    char reserve0[1];
    short flag;
    short no;
    long time;
    long size;
    short depth;
    short x_aspect;
    short y_aspect;
    short x_max;
    short y_max;
    long reserve1;
};

struct pic2_block {
    char id[4];
    long size;
    short flag;
    short x_wid;
    short y_wid;
    short x_offset;
    short y_offset;
    long opaque;
    long reserve;
};

struct pic2_info {
    jmp_buf jmp;
    FILE *fp;
    struct {
	int rest;
	byte cur;
	int bits;
	char zero;
    }bs;
    long fsize;
    struct pic2_header *header;
    struct pic2_block *block;
    int n_pal;
    int pal_bits;
    byte pal[256][3];
    char *comment;
    char mode;
    long next_pos;
    long block_pos;
    short x_max;
    short y_max;
    int ynow;
    byte *buf;
    pixel *vram_prev;
    pixel *vram_now;
    pixel *vram_next;
    short *flag_now;
    short *flag_next;
    short *flag2_now;
    short *flag2_next;
    short *flag2_next2;
    pixel (*cache)[PIC2_ARITH_CACHE];
    unsigned short *cache_pos;
    unsigned short *mulu_tab;
    long aa;
    long cc;
    long dd;
    char cache_hit_c;
    int (*next_line) PARM((struct pic2_info *, pixel **));
    char writing_grey;
    char pagebname[64];
    int pnum;
};

static void pic2_open_file             PARM((struct pic2_info*,char*));
static void pic2_read_header           PARM((struct pic2_info*));
static void pic2_read_block_header1    PARM((struct pic2_info*));
static void pic2_read_block_header2    PARM((struct pic2_info*));
static short pic2_arith_decode_bit     PARM((struct pic2_info*,int));
static short pic2_arith_decode_nn      PARM((struct pic2_info*,int));
static void pic2_arith_expand_chain    PARM((struct pic2_info*,int,int,pixel));
static short pic2_arith_get_number     PARM((struct pic2_info*,int,int));
static pixel pic2_arith_read_color     PARM((struct pic2_info*,int));
static int pic2_arith_expand_line      PARM((struct pic2_info*,pixel**));
static int pic2_arith_loader_init      PARM((struct pic2_info*));
static int pic2_fast_read_length       PARM((struct pic2_info*));
static void pic2_fast_expand_chain     PARM((struct pic2_info*,int,pixel));
static pixel pic2_fast_read_color      PARM((struct pic2_info*,pixel));
static int pic2_fast_expand_line       PARM((struct pic2_info*,pixel**));
static int pic2_fast_loader_init       PARM((struct pic2_info*));
static int pic2_beta_expand_line       PARM((struct pic2_info*,pixel**));
static int pic2_beta_loader_init       PARM((struct pic2_info*));
static void pic2_make_xvpic            PARM((struct pic2_info*,byte**,
					     byte*,byte*,byte*));
static void pic2_make_pagefile         PARM((struct pic2_info*,char*,int));
static void pic2_setup_pic2_info       PARM((struct pic2_info*,
					     char*,char*,char*,char*,
					     int,int,int,int,int,int,char *));
static void pic2_append                PARM((struct pic2_info*));
static void pic2_write_header1         PARM((struct pic2_info*));
static void pic2_write_header2         PARM((struct pic2_info*));
static void pic2_write_block_header    PARM((struct pic2_info*));
static void pic2_arith_write_zero_bit  PARM((struct pic2_info*));
static void pic2_arith_flush_bit_buf   PARM((struct pic2_info*));
static void pic2_arith_carry_bit       PARM((struct pic2_info*));
static void pic2_arith_encode_bit      PARM((struct pic2_info*,int,int));
static void pic2_arith_encode_nbyte    PARM((struct pic2_info*,int,int,int));
static void pic2_arith_encode_nn       PARM((struct pic2_info*,int,int));
static void pic2_arith_press_chain     PARM((struct pic2_info*,int));
static void pic2_arith_put_number      PARM((struct pic2_info*,int,int,int));
static void pic2_arith_write_color     PARM((struct pic2_info*,int));
static void pic2_arith_press_line2     PARM((struct pic2_info*));
static int pic2_arith_press_line       PARM((struct pic2_info*,pixel**));
static int pic2_arith_saver_init       PARM((struct pic2_info*,pixel**));
static void pic2_fast_write_length     PARM((struct pic2_info*,int));
static void pic2_fast_press_chain      PARM((struct pic2_info*,int));
static void pic2_fast_press_chain2     PARM((struct pic2_info*,int));
static void pic2_fast_flush_chain      PARM((struct pic2_info*));
static void pic2_fast_write_color      PARM((struct pic2_info*,int));
static void pic2_fast_press_line2      PARM((struct pic2_info*));
static int pic2_fast_press_line        PARM((struct pic2_info*,pixel**));
static int pic2_fast_saver_init        PARM((struct pic2_info*,pixel**));
static int pic2_beta_press_line        PARM((struct pic2_info*,pixel**));
static int pic2_beta_saver_init        PARM((struct pic2_info*,pixel**));
static void pic2_write_data            PARM((struct pic2_info*,byte*,
					     int,int,int,int,int,
					     byte*,byte*,byte*,int,int));
static int pic2_next_line              PARM((struct pic2_info*,pixel**));
static int pic2_next_block             PARM((struct pic2_info*));
static int pic2_find_block             PARM((struct pic2_info*));
static int pic2_load_block             PARM((struct pic2_info*));
static int pic2_save_block             PARM((struct pic2_info*,pixel**,
					     int,int,int,int,char*,pixel));
#ifndef PIC2_IGNORE_UNUSED_FUNCTIONS
static void pic2_read_palette          PARM((struct pic2_info*,
					     byte*,byte*,byte*));
static void pic2_write_palette         PARM((struct pic2_info*,int,int,
					     byte*,byte*,byte*));
#endif /* !PIC2_IGNORE_UNUSED_FUNCTIONS */
static byte pic2_convert_color_bits    PARM((int,int,int));
static byte pic2_pad_color_bits        PARM((int,int,int));
static byte pic2_reduce_color_bits     PARM((int,int,int));
static pixel pic2_exchange_rg          PARM((pixel,int));
static void pic2_handle_para           PARM((struct pic2_info*,int));
static int pic2_alloc_buffer           PARM((struct pic2_info*));
static void pic2_free_buffer           PARM((struct pic2_info*));
static long pic2_seek_file             PARM((struct pic2_info*,long,int));
static long pic2_tell_file             PARM((struct pic2_info*));
static int pic2_read_file              PARM((struct pic2_info*,void*,size_t));
static long pic2_read_long             PARM((struct pic2_info*));
static short pic2_read_short           PARM((struct pic2_info*));
static char pic2_read_char             PARM((struct pic2_info*));
static int pic2_write_file             PARM((struct pic2_info*,void*,size_t));
static int pic2_write_long             PARM((struct pic2_info*,long));
static int pic2_write_short            PARM((struct pic2_info*,int));
static int pic2_write_char             PARM((struct pic2_info*,int));
static unsigned long pic2_read_bits    PARM((struct pic2_info*,int));
static void pic2_write_bits            PARM((struct pic2_info*,
					     unsigned long,int));
static void pic2_flush_bits            PARM((struct pic2_info*));
static void pic2_memory_error          PARM((char*,char*));
static void pic2_error                 PARM((struct pic2_info*,int));
static void pic2_file_error            PARM((struct pic2_info*,int));
static void pic2_init_info             PARM((struct pic2_info*));
static void pic2_cleanup_pic2_info     PARM((struct pic2_info*,int));
static void pic2_cleanup_pinfo         PARM((PICINFO*));
static void pic2_show_pic2_info        PARM((struct pic2_info*));
static char *pic2_strncpy              PARM((char*,char*,size_t));
static void *pic2_malloc               PARM((size_t,char*));
static void *pic2_new                  PARM((size_t,char*));

static int WritePIC2                   PARM((FILE*,byte*,int,int,int,
					     byte*,byte*,byte*,int,int,char*,
					     int,int,int,int,int,char*));

static char *pic2_id = "P2DT";

/* Error Messages */
static char *pic2_msgs[] = {
    NULL,
#define PIC2_OPEN 1
    "can't open file.",
#define PIC2_CORRUPT 2
    "file corrupted.",
#define PIC2_FORMAT 3
    "not PIC2 format.",
#define PIC2_DEPTH 4
    "bit depths not divisible by 3 are unsupported.",
#define PIC2_TMPFILE 5
    "unable to create temporary filename???",
#define PIC2_PAGE 6
    "couldn't load the page.",
#define PIC2_APPEND 7
    "cannot append.",
#define PIC2_WRITE 8
    "write failed.",
};

struct _form_tab {
    char *id;
    int (*loader_init) PARM((struct pic2_info *));
    int (*saver_init) PARM((struct pic2_info *, pixel **));
} form_tab[] = {
	{ "P2SS", pic2_arith_loader_init, pic2_arith_saver_init},
	{ "P2SF", pic2_fast_loader_init, pic2_fast_saver_init},
	{ "P2BM", pic2_beta_loader_init, pic2_beta_saver_init},
	{ "P2BI", pic2_beta_loader_init, pic2_beta_saver_init},
};
#define	n_form_tab (sizeof(form_tab) / sizeof(struct _form_tab))
#define P2SS 0
#define P2SF 1
#define P2BM 2
#define P2BI 3

/* The main routine to load a PIC2 file. */
int LoadPIC2(fname, pinfo, quick)
char *fname;
PICINFO *pinfo;
int quick;
{
    int e, i, block;
    struct pic2_info pic2;

    if (DEBUG)
	fputs("LoadPIC2:\n", stderr);

    pic2_init_info(&pic2);

    if ((e = setjmp(pic2.jmp)) != 0){
	/* When an error occurs, comes here. */
	pic2_free_buffer(&pic2);
	pic2_cleanup_pic2_info(&pic2, 0);
	pic2_cleanup_pinfo(pinfo);
	if (pic2split)
	    KillPageFiles(pic2.pagebname, pic2.pnum);
	SetCursors(-1);
	if (DEBUG)
	    fputs("\n", stderr);
	return (0);
    }
    pic2_open_file(&pic2, fname);
    pic2_read_header(&pic2);

    if ((i = pic2_find_block(&pic2)) == 0)
	pic2_file_error(&pic2, PIC2_CORRUPT);

    block = 1;
    while(i == 2) {
	SetISTR(ISTR_WARNING, "unknown or invalid block #%d.", block);
	i = pic2_next_block(&pic2);
	block++;
    }

    if (pic2split && !quick) {
	char firstpage[512];
	struct stat st;
#ifndef USE_MKSTEMP
	int tmpfd;
#endif

#ifndef VMS
	sprintf(pic2.pagebname, "%s/xvpic2XXXXXX", tmpdir);
#else
	sprintf(pic2.pagebname, "Sys$Scratch:xvpic2XXXXXX");
#endif
#ifdef USE_MKSTEMP
	close(mkstemp(pic2.pagebname));
#else
	mktemp(pic2.pagebname);
	tmpfd = open(pic2.pagebname, O_WRONLY|O_CREAT|O_EXCL, S_IRWUSR);
	if (tmpfd < 0) FatalError("LoadPIC2(): can't create temporary file");
	close(tmpfd);
#endif
	if (pic2.pagebname[0] == '\0')
	    pic2_error(&pic2, PIC2_TMPFILE);
	strcat(pic2.pagebname, ".");

	sprintf(firstpage, "%s%d", pic2.pagebname, 1);
	if (stat(firstpage, &st)) {
	    for (pic2.pnum = 1; i >= 1; pic2.pnum++) {
		pic2_load_block(&pic2);
		pic2_make_pagefile(&pic2, pic2.pagebname, pic2.pnum);
		while(block++, (i = pic2_next_block(&pic2)) == 2)
		    SetISTR(ISTR_WARNING,
			    "unknown or invalid block #%d.", block);
	    }
            pinfo->numpages = --pic2.pnum;
            if (!LoadPIC2(firstpage, pinfo, 1))
		pic2_error(&pic2, PIC2_PAGE);
	    if (pic2.pnum == 1)
		unlink(firstpage);
	    else
		strcpy(pinfo->pagebname, pic2.pagebname);
	} else
            if (!LoadPIC2(fname, pinfo, 1))
		pic2_error(&pic2, PIC2_PAGE);
    } else {
	char buf[128], format[64];
	int j;

	pinfo->w = pic2.x_max;
	pinfo->h = pic2.y_max;
	pinfo->normw = pinfo->w;
	pinfo->normh = pinfo->h;
	pinfo->type = PIC24;
	for (j = 0; j < n_form_tab; j++) {
	    if (xvbcmp(pic2.block->id, form_tab[j].id, (size_t) 4) == 0)
		break;
	}
	pinfo->frmType = F_PIC2;
	pinfo->colType = F_FULLCOLOR;
	pinfo->comment = pic2.comment;

	if (pic2split) {
	    pic2_make_xvpic(&pic2, &pinfo->pic, pinfo->r, pinfo->g, pinfo->b);
	    strcpy(format, form_tab[j].id);
	} else {
	    for (pic2.pnum = 1; i >= 1; pic2.pnum++) {
		SetISTR(ISTR_INFO, "composing block #%d", block);
		pic2_make_xvpic(&pic2, &pinfo->pic,
				pinfo->r, pinfo->g, pinfo->b);
		while(block++, (i = pic2_next_block(&pic2)) == 2)
		    SetISTR(ISTR_WARNING,
			    "unknown or invalid block #%d.", block);
	    }
	    if (--block > 1)
		if (block != --pic2.pnum)
		    sprintf(format, "MultiBlock[%d/%d]", block, pic2.pnum);
		else
		    sprintf(format, "MultiBlock[%d]", block);
	    else
		strcpy(format, form_tab[j].id);
	}
	sprintf(buf, "PIC2(%s). %d colors (%ld bytes)", format,
		(int) 1 << pic2.header->depth, pic2.fsize);
	strcat(pinfo->fullInfo, buf);
	sprintf(pinfo->shrtInfo, "%dx%d(aspect %4.2f) PIC2(%s).",
		pinfo->w, pinfo->h,
		(float) pic2.header->x_aspect / (float) pic2.header->y_aspect,
		format);
	if (!nopicadjust)
	    normaspect = (float) pic2.header->x_aspect
			 / (float) pic2.header->y_aspect;
    }
    pic2_cleanup_pic2_info(&pic2, 0);
    SetCursors(-1);
    if (DEBUG)
	fputs("\n", stderr);
    return (1);
}

/*
 * This function opens the file, and set its size.
 */
static void pic2_open_file(pi, fname)
    struct pic2_info *pi;
    char *fname;
{
    if ((pi->fp = fopen(fname, "rb")) == NULL)
	pic2_file_error(pi, PIC2_OPEN);
    fseek(pi->fp, (size_t) 0, SEEK_END);
    pi->fsize = ftell(pi->fp);
    fseek(pi->fp, (size_t) 0, SEEK_SET);
}

/*
 * These functions read the PIC2 header informations.
 * pic2_read_header:
 *	reads the PIC2 header.
 * pic2_read_block_header1:
 *	reads the id number of block header and the size of block.
 * pic2_read_block_header2:
 *	reads the rest of block header.
 */
static void pic2_read_header(pi)
struct pic2_info *pi;
{
    long s_comment;

    pi->mode = PIC2_READ_MODE;

    /* read header image */
    pic2_read_file(pi, pi->header->magic, 4);
    pic2_read_file(pi, pi->header->name, 18);
    pic2_read_file(pi, pi->header->subtitle, 8);
    pic2_read_file(pi, pi->header->crlf0, 2);
    pic2_read_file(pi, pi->header->title, 30);
    pic2_read_file(pi, pi->header->crlf1, 2);
    pic2_read_file(pi, pi->header->saver, 30);
    pic2_read_file(pi, pi->header->crlf2, 2);
    pic2_read_file(pi, pi->header->eof, 1);
    pic2_read_file(pi, pi->header->reserve0, 1);
    pi->header->flag = pic2_read_short(pi);
    pi->header->no = pic2_read_short(pi);
    pi->header->time = pic2_read_long(pi);
    pi->header->size = pic2_read_long(pi);
    pi->header->depth = pic2_read_short(pi);
    pi->header->x_aspect = pic2_read_short(pi);
    pi->header->y_aspect = pic2_read_short(pi);
    pi->header->x_max = pic2_read_short(pi);
    pi->header->y_max = pic2_read_short(pi);
    pi->header->reserve1 = pic2_read_long(pi);

    /* check magic number */
    if (strncmp(pi->header->magic, pic2_id, (size_t) 4) != 0)
        pic2_error(pi, PIC2_FORMAT);

    /* read palette data, if exists */
    if (pi->header->flag & 1) {
	pi->pal_bits = pic2_read_char(pi);
	pi->n_pal = pic2_read_short(pi);
	pic2_read_file(pi, pi->pal, (size_t) (pi->n_pal * 3));
    }

    /* read comments */
    s_comment = pi->header->size - pic2_tell_file(pi);
    pi->comment = pic2_new(s_comment + 1, "pic2_read_header");
    pic2_read_file(pi, pi->comment, (size_t) s_comment);
    pi->comment[s_comment] = '\0';

    pi->x_max = pi->header->x_max;
    pi->y_max = pi->header->y_max;

    /* set initial block point */
    pi->next_pos = pic2_tell_file(pi);
}

static void pic2_read_block_header1(pi)
struct pic2_info *pi;
{
    pic2_read_file(pi, pi->block->id, 4);
    pi->block->size = pic2_read_long(pi);
}

static void pic2_read_block_header2(pi)
struct pic2_info *pi;
{
    pi->block->flag = pic2_read_short(pi);
    pi->block->x_wid = pic2_read_short(pi);
    pi->block->y_wid = pic2_read_short(pi);
    pi->block->x_offset = pic2_read_short(pi);
    pi->block->y_offset = pic2_read_short(pi);
    pi->block->opaque = pic2_read_long(pi);
    pi->block->reserve = pic2_read_long(pi);
}

/*
 * These functions are arithmetic pic2 format extractor.
 */
static short pic2_arith_decode_bit(pi, c)
struct pic2_info *pi;
int c;
{
    unsigned short pp;

    pp = pi->mulu_tab[(pi->aa & 0x7f00) / 2 + c];
    if (pi->dd >= (int) pp) {
	pi->dd -= pp;
	pi->aa -= pp;

	while ((short) pi->aa >= 0) {
	    pi->dd *= 2;
	    if (pic2_read_bits(pi, 1))
		pi->dd++;
	    pi->aa *= 2;
	}
	return (1);
    } else {
	pi->aa = pp;

	while ((short) pi->aa >= 0) {
	    pi->dd *= 2;
	    if (pic2_read_bits(pi, 1))
		pi->dd++;
	    pi->aa *= 2;
	}
	return (0);
    }
}

static short pic2_arith_decode_nn(pi, c)
struct pic2_info *pi;
int c;
{
    int n;

    if (pic2_arith_decode_bit(pi, c)) {
	/* n < 1 */
	n = 0;
    } else if (pic2_arith_decode_bit(pi, c + 1)) {
	/* n < 1 + 2 */
	n = 1;
	if (pic2_arith_decode_bit(pi, c + 8))
	    n += 1;
    } else if (pic2_arith_decode_bit(pi, c + 2)) {
	/* n < 1 + 2 + 4 */
	n = 1 + 2;
	if (pic2_arith_decode_bit(pi,  c + 8))
	    n += 1;
	if (pic2_arith_decode_bit(pi,  c + 9))
	    n += 2;
    } else if (pic2_arith_decode_bit(pi, c + 3)) {
	/* n < 1 + 2 + 4 + 8 */
	n = 1 + 2 + 4;
	if (pic2_arith_decode_bit(pi, c + 8))
	    n += 1;
	if (pic2_arith_decode_bit(pi, c + 9))
	    n += 2;
	if (pic2_arith_decode_bit(pi, c + 10))
	    n += 4;
    } else if (pic2_arith_decode_bit(pi, c + 4)) {
	/* n < 1 + 2 + 4 + 8 + 16 */
	n = 1 + 2 + 4 + 8;
	if (pic2_arith_decode_bit(pi, c + 8))
	    n += 1;
	if (pic2_arith_decode_bit(pi, c + 9))
	    n += 2;
	if (pic2_arith_decode_bit(pi, c + 10))
	    n += 4;
	if (pic2_arith_decode_bit(pi, c + 11))
	    n += 8;
    } else if (pic2_arith_decode_bit(pi,  c + 5)) {
	/* n < 1 + 2 + 4 + 8 + 16 + 32 */
	n = 1 + 2 + 4 + 8 + 16;
	if (pic2_arith_decode_bit(pi, c + 8))
	    n += 1;
	if (pic2_arith_decode_bit(pi, c + 9))
	    n += 2;
	if (pic2_arith_decode_bit(pi, c + 10))
	    n += 4;
	if (pic2_arith_decode_bit(pi, c + 11))
	    n += 8;
	if (pic2_arith_decode_bit(pi, c + 12))
	    n += 16;

    } else if (pic2_arith_decode_bit(pi, c + 6)) {
	/* n < 1 + 2 + 4 + 8 + 16 + 32 + 64 */
	n = 1 + 2 + 4 + 8 + 16 + 32;
	if (pic2_arith_decode_bit(pi, c + 8))
	    n += 1;
	if (pic2_arith_decode_bit(pi, c + 9))
	    n += 2;
	if (pic2_arith_decode_bit(pi, c + 10))
	    n += 4;
	if (pic2_arith_decode_bit(pi, c + 11))
	    n += 8;
	if (pic2_arith_decode_bit(pi, c + 12))
	    n += 16;
	if (pic2_arith_decode_bit(pi, c + 13))
	    n += 32;

    } else if (pic2_arith_decode_bit(pi, c + 7)) {
	/* n < 1 + 2 + 4 + 8 + 16 + 32 + 64 + 128 */
	n = 1 + 2 + 4 + 8 + 16 + 32 + 64;
	if (pic2_arith_decode_bit(pi, c + 8))
	    n += 1;
	if (pic2_arith_decode_bit(pi, c + 9))
	    n += 2;
	if (pic2_arith_decode_bit(pi, c + 10))
	    n += 4;
	if (pic2_arith_decode_bit(pi, c + 11))
	    n += 8;
	if (pic2_arith_decode_bit(pi, c + 12))
	    n += 16;
	if (pic2_arith_decode_bit(pi, c + 13))
	    n += 32;
	if (pic2_arith_decode_bit(pi, c + 14))
	    n += 64;

    } else {
	n = 1 + 2 + 4 + 8 + 16 + 32 + 64 + 128;
    }
    return (n);
}

static void pic2_arith_expand_chain(pi, x, y, cc)
struct pic2_info *pi;
int x, y;
pixel cc;
{
    static const unsigned short c_tab[] = {
	80 + 6 * 5,	/* -5 */
	80 + 6 * 4,
	80 + 6 * 3,
	80 + 6 * 2,
	80 + 6 * 1,
	80 + 6 * 0,	/* 0  */
	80 + 6 * 0,	/* 1  */
    };
    unsigned short b;

    b = c_tab[pi->flag_now[x] + 5];
    if (!pic2_arith_decode_bit(pi, b++)) {
	if (pic2_arith_decode_bit(pi, b++))      {	/* down */
	    pi->vram_next[x    ] = cc;
	    pi->flag_next[x    ] = -1;
	} else if (pic2_arith_decode_bit(pi, b++)) {	/* left */
	    pi->vram_next[x - 1] = cc;
	    pi->flag_next[x - 1] = -2;
	} else if (pic2_arith_decode_bit(pi, b++)) {	/* right */
	    pi->vram_next[x + 1] = cc;
	    pi->flag_next[x + 1] = -3;
	} else if (pic2_arith_decode_bit(pi, b++)) {	/* left2 */
	    pi->vram_next[x - 2] = cc;
	    pi->flag_next[x - 2] = -4;
	} else {					/* right2 */
	    pi->vram_next[x + 2] = cc;
	    pi->flag_next[x + 2] = -5;
	}
    }
}

static short pic2_arith_get_number(pi, c, bef)
struct pic2_info *pi;
int c, bef;
{
    unsigned short n;
    byte maxcol;

    maxcol = 0xff >> (8 - pi->header->depth / 3);

    n = pic2_arith_decode_nn(pi, c);
    if (bef > ((int) maxcol >> 1)) {
	if (n > ((int) maxcol - bef) * 2)
	    n = maxcol - n;
	else if (n & 1)
	    n = n / 2 + bef + 1;
	else
	    n = bef - n / 2;
    } else {
	if ((int) n > (bef * 2))
	    n = n;
	else if (n & 1)
	    n = n / 2 + bef + 1;
	else
	    n = bef - n / 2;
    }
    return (n);
}

static pixel pic2_arith_read_color(pi, x)
struct pic2_info *pi;
int x;
{
    pixel c1, c2, cc;
    unsigned short i, j, k, m;
    short r, g, b, r0, g0, b0;
    short colbits;
    pixel rmask, gmask, bmask;
    byte maxcol;

    colbits = pi->header->depth / 3;
    rmask = (0xff >> (8 - colbits)) << (colbits * 2);
    gmask = (0xff >> (8 - colbits)) <<  colbits;
    bmask = (0xff >> (8 - colbits));
    maxcol = (byte) bmask;

    c1 = pi->vram_prev[x];
    k = ((c1 >> ((colbits - 3) * 3)) & 0x1c0)
      | ((c1 >> ((colbits - 3) * 2)) & 0x038)
      | ((c1 >>  (colbits - 3)     ) & 0x007);
    if (colbits == 5)
	k = pic2_exchange_rg(k, 3);

    if (pic2_arith_decode_bit(pi, pi->cache_hit_c)) {	/* ouch */
	pi->cache_hit_c = 16;

        c2 = pi->vram_now[x - 1];
	r = ((c1 & rmask) + (c2 & rmask)) >> (colbits * 2 + 1);
	g = ((c1 & gmask) + (c2 & gmask)) >> (colbits     + 1);
	b = ((c1 & bmask) + (c2 & bmask)) >> (              1);

	g0 = pic2_arith_get_number(pi, 32, g);
        r = r + g0 - g;
	if (r > (short) maxcol)
	    r = maxcol;
	else if (r < 0)
	    r = 0;

        b = b + g0 - g;
	if (b > (short) maxcol)
	    b = maxcol;
	else if (b < 0)
	    b = 0;

	r0 = pic2_arith_get_number(pi, 48, r);
	b0 = pic2_arith_get_number(pi, 64, b);

	pi->cache_pos[k] = j = (pi->cache_pos[k] - 1) & (PIC2_ARITH_CACHE - 1);
	pi->cache[k][j] = cc = (r0 << (colbits * 2)) | (g0 << colbits) | b0;
    } else {
	pi->cache_hit_c = 15;

	j = pic2_arith_decode_nn(pi, 17);
	m = pi->cache_pos[k];
	i = (m + j / 2) & (PIC2_ARITH_CACHE - 1);
	j = (m + j) & (PIC2_ARITH_CACHE - 1);

	cc = pi->cache[k][j];
	pi->cache[k][j] = pi->cache[k][i];
	pi->cache[k][i] = pi->cache[k][m];
	pi->cache[k][m] = cc;
    }
    return (cc);
}

static int pic2_arith_expand_line(pi, line)
struct pic2_info *pi;
pixel **line;
{
    int ymax;
    int x, xw;
    pixel cc;

    pic2_handle_para(pi, 0);

    xw = pi->block->x_wid;
    ymax = pi->block->y_wid - 1;

    if (pi->ynow > ymax)
	return (-2);					/* end */

    /* set right end of previous line before left end of current line. */
    if (pi->ynow == 0) {
	cc = 0;
    } else
	cc = pi->vram_prev[xw - 1];
    pi->vram_now[-1] = cc;

    /* clear flag for change point */
    xvbzero((char *) pi->flag_next, xw * sizeof(pi->flag_next[0]));

    /* clear flag for position probability space */
    xvbzero((char *) pi->flag2_next2, xw * sizeof(pi->flag2_next2[0]));

    for (x = 0; x < xw; x++) {
	if (pi->flag_now[x] < 0) {
	    cc = pi->vram_now[x];
	    if (pi->ynow < ymax)
		pic2_arith_expand_chain(pi, x, pi->ynow, cc);
	} else if (pic2_arith_decode_bit(pi, pi->flag2_now[x])) {
	    /* ajust probability space around of change point */
	    pi->flag2_now  [x + 1]++;
	    pi->flag2_now  [x + 2]++;
	    pi->flag2_next [x - 1]++;
	    pi->flag2_next [x    ]++;
	    pi->flag2_next [x + 1]++;
	    pi->flag2_next2[x - 1]++;
	    pi->flag2_next2[x    ]++;
	    pi->flag2_next2[x + 1]++;

	    pi->vram_now[x] = cc = pic2_arith_read_color(pi, x);
	    if (pi->ynow < ymax)
		pic2_arith_expand_chain(pi, x, pi->ynow, cc);
	} else
	    pi->vram_now[x] = cc;
    }
    if (line != NULL)
	*line = pi->vram_now;
    pi->ynow++;

    pic2_handle_para(pi, 1);

    return (pi->ynow - 1);
}

static int pic2_arith_loader_init(pi)
struct pic2_info *pi;
{
    unsigned short p2b[256];
    int i, xw;

    pi->ynow = 0;

    /* check the color depth */
    if (pi->header->depth % 3)
	pic2_error(pi, PIC2_DEPTH);

    /* set function for extract next line */
    pi->next_line = pic2_arith_expand_line;

    /* clear cache and flags */
    xw = pi->block->x_wid;
    xvbzero((char *) pi->cache, 8 * 8 * 8 * sizeof(pi->cache[0]));
    xvbzero((char *) pi->cache_pos, 8 * 8 * 8 * sizeof(pi->cache_pos[0]));

    xvbzero((char *) pi->flag_now, xw * sizeof(pi->flag_now[0]));
    xvbzero((char *) pi->flag2_now, 8 + xw * sizeof(pi->flag2_now[0]));
    xvbzero((char *) pi->flag2_next, 8 + xw * sizeof(pi->flag2_next[0]));

    /* go to picture data field */
    pic2_seek_file(pi, pi->block_pos + PIC2_BLOCK_HEADER_SIZE, SEEK_SET);

    /* clear bit field marker */
    pi->bs.rest = 0;
    pi->bs.cur = 0;

    /* read probability table */
    for (i = 0; i < PIC2_ARITH_CONTEXT; i++)
	p2b[i] = pic2_read_short(pi);

    /* make multiplication table */
    for (i = 0; i < 16384; i++) {
	pi->mulu_tab[i] = (long) (i / 128 + 128) * (int) p2b[i & 127] / 256;
	if (pi->mulu_tab[i] == 0) pi->mulu_tab[i] = 1;
    }
    /* initialize some valuables */
    pi->aa = 0xffff;
    pi->dd = 0;
    for (i = 0; i < 16; i++) {
	pi->dd *= 2;
	if (pic2_read_bits(pi, 1))
	    pi->dd |= 1;
    }
    pi->cache_hit_c = 16;

    return (0);
}

/*
 * These functions are fast pic2 compression extractor.
 */
static int pic2_fast_read_length(pi)
struct pic2_info *pi;
{
    int a;

    a = 0;
    while (pic2_read_bits(pi, 1)) {
	a++;
    }
    if (a == 0)
	return (0);
    return (pic2_read_bits(pi, a) + (1 << a) - 1);
}

static void pic2_fast_expand_chain(pi, x, cc)
struct pic2_info *pi;
int x;
pixel cc;
{
    if (pic2_read_bits(pi, 1) != 0) {
	if (pic2_read_bits(pi, 1) != 0) {		/* down */
	    pi->vram_next[x] = cc;
	    pi->flag_next[x] = -1;
	} else if (pic2_read_bits(pi, 1) != 0) {
	    if (pic2_read_bits(pi, 1) == 0) {		/* left2down */
		pi->vram_next[x - 2] = cc;
		pi->flag_next[x - 2] = -1;
	    } else {					/* left1down */
		pi->vram_next[x - 1] = cc;
		pi->flag_next[x - 1] = -1;
	    }
	} else {
	    if (pic2_read_bits(pi, 1) == 0) {		/* right2down */
		pi->vram_next[x + 2] = cc;
		pi->flag_next[x + 2] = -1;
	    } else {					/* left1down */
		pi->vram_next[x + 1] = cc;
		pi->flag_next[x + 1] = -1;
	    }
	}
    }
}

static pixel pic2_fast_read_color(pi, bc)
struct pic2_info *pi;
pixel bc;
{
    pixel cc;
    unsigned short j, k, m;
    short depth, colbits;
    pixel (*cache)[PIC2_FAST_CACHE];

    depth = pi->header->depth;
    colbits = depth / 3;
    cache = (pixel (*)[PIC2_FAST_CACHE]) pi->cache;

    bc = pic2_exchange_rg(bc, colbits);
    k = pic2_shift_bits(bc, 8 - depth);
    if (pic2_read_bits(pi, 1) == 0) {
	pi->cache_pos[k] = m = (pi->cache_pos[k] - 1) & (PIC2_FAST_CACHE - 1);
	cc = pic2_read_bits(pi, depth);
	cc = pic2_exchange_rg(cc, colbits);
	cache[k][m] = cc;
    } else {
	j = pic2_read_bits(pi, 6);		/* 6= log2(PIC2_FAST_CACHE) */
	m = pi->cache_pos[k];
	cc = cache[k][(m + j) & (PIC2_FAST_CACHE - 1)];
    }
    return (cc);
}

static int pic2_fast_expand_line(pi, line)
struct pic2_info *pi;
pixel **line;
{
    int ymax;
    int x, xw;
    pixel cc;

    pic2_handle_para(pi, 0);

    xw = pi->block->x_wid;
    ymax = pi->block->y_wid - 1;

    if (pi->ynow > ymax)
	return (-2);

    if (pi->ynow == 0) {
	pi->dd = 0;
	pi->aa = pic2_fast_read_length(pi);
	if (pi->aa == 1023)
	    pi->dd = 1023;
	else if (pi->aa > 1023)
	    pi->aa--;
	cc = 0;
    } else
	cc = pi->vram_prev[xw - 1];

    xvbzero((char *) pi->flag_next, xw * sizeof(pi->flag_next[0]));

    for (x = 0; x < xw; x++) {
	if (pi->dd > 0) {
	    if (pi->flag_now[x] < 0) {			/* on chain ? */
		cc = pi->vram_now[x];
		pic2_fast_expand_chain(pi, x, cc);
		if (--pi->dd == 0) {
		    pi->aa = pic2_fast_read_length(pi);
		    if (pi->aa == 1023)
			pi->dd = 1023;
		    else if (pi->aa > 1023)
			pi->aa--;
		}
	    } else
		pi->vram_now[x] = cc;
	} else {
	    if (pi->flag_now[x] < 0) {			/* on chain ? */
		cc = pi->vram_now[x];
		pic2_fast_expand_chain(pi, x, cc);
	    } else if (--pi->aa < 0) {
		cc = pi->vram_now[x] = pic2_fast_read_color(pi, cc);
		pic2_fast_expand_chain(pi, x, cc);
		pi->aa = pic2_fast_read_length(pi);
		if (pi->aa == 1023)
		    pi->dd = 1023;
		else if (pi->aa > 1023)
		    pi->aa--;
	    } else
		pi->vram_now[x] = cc;
	}
    }
    if (line != NULL)
	*line = pi->vram_now;
    pi->ynow++;

    pic2_handle_para(pi, 1);

    return (pi->ynow - 1);
}

static int pic2_fast_loader_init(pi)
struct pic2_info *pi;
{
    int xw;

    pi->ynow = 0;

    /* check the color depth */
    if (pi->header->depth % 3)
	pic2_error(pi, PIC2_DEPTH);

    /* set function for extract next line */
    pi->next_line = pic2_fast_expand_line;

    /* clear cache and flags */
    xw = pi->block->x_wid;
    xvbzero((char *) pi->cache, sizeof(pi->cache[0]) * 256);
    xvbzero((char *) pi->cache_pos, sizeof(pi->cache_pos[0]) * 8 * 8 * 8);
    xvbzero((char *) pi->flag_now, (xw + 8) * sizeof(pi->flag_now[0]));
    xvbzero((char *) pi->flag_next, (xw + 8) * sizeof(pi->flag_next[0]));

    /* go to picture data field */
    pic2_seek_file(pi, pi->block_pos + PIC2_BLOCK_HEADER_SIZE, SEEK_SET);

    /* clear bit field marker */
    pi->bs.rest = 0;
    pi->bs.cur = 0;

    return (0);
}

/*
 * These functions are beta pic2 format extractor.
 */
static int pic2_beta_expand_line(pi, line)
struct pic2_info *pi;
pixel **line;
{
    int i, xw, ymax;
    byte a, b, c, *p;
    pixel *pc;
    short depth, pixbyte, colbits;

    depth = pi->header->depth;
    pixbyte = depth / 8 + ((depth % 8) > 0);
    colbits = depth / 3;

    xw = pi->block->x_wid;
    ymax = pi->block->y_wid - 1;

    if (pi->ynow > ymax)
	return (-2);					/* end */

    pc = pi->vram_now;
    p = (byte *) pi->vram_prev;
    if (pixbyte == 3) {
	pic2_read_file(pi, pi->vram_prev, (size_t) (xw * pixbyte));
	for (i = 0; i < xw; i++, pc++) {
	    a = *p++;
	    b = *p++;
	    c = *p++;
	    *pc = ((pixel) a << 16) | ((pixel) b << 8) | (pixel) c;
	}
    } else if (pixbyte == 2) {
	pic2_read_file(pi, pi->vram_prev, (size_t) (xw * 2));
	if (strncmp(pi->block->id, "P2BM", 4) == 0) {
	    for (i = 0; i < xw; i++, pc++) {
		a = *p++;
		b = *p++;
		*pc = ((pixel) a << 8) | (pixel) b;
		if (colbits == 5) {
		    *pc >>= 1;
		    *pc = pic2_exchange_rg(*pc, colbits);
		}
	    }
	} else {
	    for (i = 0; i < xw; i++, pc++) {
		a = *p++;
		b = *p++;
		*pc = ((pixel) b << 8) | (pixel) a;
		if (colbits == 5) {
		    *pc >>= 1;
		    *pc = pic2_exchange_rg(*pc, colbits);
		}
	    }
	}
    } else {
	pic2_read_file(pi, pi->vram_prev, (size_t) xw);
	for (i = 0; i < xw; i++)
	    *pc++ = *p++;
    }
    if (line != NULL)
	*line = pi->vram_now;

    pc = pi->vram_prev;
    pi->vram_prev = pi->vram_now;
    pi->vram_now = pi->vram_next;
    pi->vram_next = pc;

    pi->ynow++;
    return (pi->ynow - 1);
}

static int pic2_beta_loader_init(pi)
struct pic2_info *pi;
{
    pi->ynow = 0;
    pi->next_line = pic2_beta_expand_line;
    pic2_seek_file(pi, pi->block_pos + PIC2_BLOCK_HEADER_SIZE, SEEK_SET);
    return (0);
}

/*
 * Make a picture from the expanded data.
 */
static void pic2_make_xvpic(pi, xp, rp, gp, bp)
struct pic2_info *pi;
byte **xp, *rp, *gp, *bp;
{
    int line, i;
    pixel *linep, opaque;
    short colbits;
    byte colmask;

    if (*xp == NULL)
	*xp = pic2_new((size_t) pi->x_max * pi->y_max * 3, "pic2_make_xvpic");   // GRR POSSIBLE OVERFLOW / FIXME

    if (pi->block->flag & 1)
	opaque = pi->block->opaque;
    else
	opaque = 0xffffffff;

    colbits = pi->header->depth / 3;
    colmask = 0xff >> (8 - colbits);

    line = pic2_load_block(pi);
    for (;;) {
	int pic_idx;

	line = pic2_next_line(pi, &linep);
	if (line < 0)
	    break;
	pic_idx = ((line + pi->block->y_offset) * pi->x_max
		   + pi->block->x_offset) * 3;

	for (i = 0; i < pi->block->x_wid; i++, linep++) {
	    byte r, g, b;

	    if (*linep != opaque) {
		r = ((*linep >> (colbits * 2)) & colmask);
		r = pic2_convert_color_bits(r, colbits, 8);
		g = ((*linep >>  colbits     ) & colmask);
		g = pic2_convert_color_bits(g, colbits, 8);
		b = ( *linep                   & colmask);
		b = pic2_convert_color_bits(b, colbits, 8);
		(*xp)[pic_idx++] = r;
		(*xp)[pic_idx++] = g;
		(*xp)[pic_idx++] = b;
	    } else
	        pic_idx += 3;

	    WaitCursor();
	}
    }
}

/*
 * This function splits a multiblock PIC2 file into several pages.
 */
static void pic2_make_pagefile(pi, pagebname, pnum)
struct pic2_info *pi;
char *pagebname;
int pnum;
{
    struct pic2_info pic2;
    FILE *fp;
    char pagefile[64], *buf;
    size_t imagesize;

    sprintf(pagefile, "%s%d", pagebname, pnum);
    if ((fp = fopen(pagefile, "wb")) == NULL)
	pic2_error(pi, PIC2_WRITE);

    xvbcopy((char *) pi, (char *) &pic2, sizeof(struct pic2_info));
    pic2.fp = fp;

    pic2_write_header1(&pic2);

    pic2_write_block_header(&pic2);

    imagesize = pi->block->size - PIC2_BLOCK_HEADER_SIZE;
    buf = (char *) pic2_malloc(imagesize, "pic2_make_pagefile");

    pic2_seek_file(pi, pi->block_pos + PIC2_BLOCK_HEADER_SIZE, SEEK_SET);
    if (fread(buf, (size_t) 1, imagesize, pi->fp) < imagesize) {
	free(buf);
	pic2_file_error(pi, PIC2_CORRUPT);
    }
    if (fwrite(buf, (size_t) 1, imagesize, fp) < imagesize) {
	free(buf);
	pic2_error(pi, PIC2_WRITE);
    }
    free(buf);

    pic2.next_pos = pic2_tell_file(&pic2);
    pic2_write_header2(&pic2);

    fclose(fp);
}

/* The main routine to save a PIC2 file. */
static int WritePIC2(fp, pic0, ptype, w, h, rmap, gmap, bmap, numcols,
		     colorstyle, fname, type, depth, x_offset, y_offset,
		     append, comment)
FILE *fp;
byte *pic0;
int ptype, w, h;
byte *rmap, *gmap, *bmap;
int numcols, colorstyle;
char *fname;
int type, depth;
int x_offset, y_offset;
int append;
char *comment;
{
    struct pic2_info pic2;
    char creator[256], title[256], saver[256];
    int e;

    if (DEBUG)
	fputs("WritePIC2:\n", stderr);

    pic2_init_info(&pic2);
    pic2.fp = fp;
    pic2.writing_grey = (colorstyle == F_GREYSCALE);

    if ((e = setjmp(pic2.jmp)) != 0){
	/* When an error occurs while writing, comes here. */
	pic2_free_buffer(&pic2);
	pic2_cleanup_pic2_info(&pic2, 1);
	SetCursors(-1);
	if (DEBUG)
	    fputs("\n", stderr);
	return (-1);
    }
    sprintf(creator, "XV Version %s", VERSTR);
    pic2_strncpy(title, comment, 30);
    sprintf(saver, "XV %s/UNIX/Bradley", VERSTR);

    if (!append) {
	pic2_setup_pic2_info(&pic2, creator, fname, title, saver,
			     0, depth, 1, 1, w, h, comment);
	pic2_write_header1(&pic2);
    } else {
	pic2_read_header(&pic2);
	pic2_append(&pic2);
	free(pic2.comment);
	pic2_setup_pic2_info(&pic2, creator, fname, title, saver,
			     0, depth, 1, 1, w, h, comment);
    }

    pic2_write_data(&pic2, pic0, ptype, x_offset, y_offset, w, h,
		    rmap, gmap, bmap, type, depth);
    pic2_write_header2(&pic2);

    pic2_cleanup_pic2_info(&pic2, 1);
    SetCursors(-1);
    if (DEBUG)
	fputs("\n", stderr);
    return (0);
}

/*
 * This function initializes pic2_info.
 */
static void pic2_setup_pic2_info(pi, name, fname, title, saver, no, depth,
			      x_aspect, y_aspect, x_max, y_max, comment)
struct pic2_info *pi;
char *name, *fname, *title, *saver;
int no, depth;
int x_aspect, y_aspect;
int x_max, y_max;
char *comment;
{
    char basename[256], *suffix;

    pi->mode = PIC2_WRITE_MODE;

    /* set magic number */
    strncpy(pi->header->magic, pic2_id, 4);

    /* set creator's name */
    pic2_strncpy(pi->header->name, (char *) name, 18);

    /* set title and subtitle */
    pic2_strncpy(pi->header->title, (char *) title, 30);
    strcpy(basename, BaseName(fname));
    suffix = (char *) rindex(basename, '.');
    if (suffix) {
	suffix++;
	if (!strcmp(suffix, "p2") || !strcmp(suffix, "P2"))
	    *(suffix - 1) = '\0';
    }
    pic2_strncpy(pi->header->subtitle, basename, 8);

    /* set saver */
    pic2_strncpy(pi->header->saver, saver, 30);

    /* set picture number */
    pi->header->no = no;

    /* import comment */
    pi->comment = comment;

    /* set some picture's info */
    pi->header->depth = depth;
    pi->header->x_aspect = x_aspect;
    pi->header->y_aspect = y_aspect;
    pi->header->x_max = x_max;
    pi->header->y_max = y_max;

    /* set some gaps */
    pi->header->crlf0[0] = pi->header->crlf1[0] = pi->header->crlf2[0] = 0x0d;
    pi->header->crlf0[1] = pi->header->crlf1[1] = pi->header->crlf2[1] = 0x0a;

    pi->header->eof[0] = 0x1a;
    pi->header->reserve0[0] = 0;
    pi->header->reserve1 = 0;

    /* set palettes */
    if (pi->n_pal > 0)
	pi->header->flag = 1;
    else
	pi->header->flag = 0;
}

/*
 * This function appends to existing pic2 file.
 */
static void pic2_append(pi)
struct pic2_info *pi;
{
    int block;

    block = pic2_find_block(pi);
    while (block > 0)
	block = pic2_next_block(pi);

    if (block != 0)
	pic2_error(pi, PIC2_APPEND);
}

/*
 * These functions write the PIC2 header.
 * pic2_write_header1:
 *	write palette data and comment.
 * pic2_write_header2:
 *	write the terminate block and rest header.
 * pic2_write_block_header:
 *	write the block header.
 */
static void pic2_write_header1(pi)
struct pic2_info *pi;
{
    char *comment;

    /* seek to block start position */
    pic2_seek_file(pi, PIC2_HEADER_SIZE, SEEK_SET);

    /* write palette */
    if (pi->n_pal > 0) {
	pic2_write_char(pi, pi->pal_bits);
	pic2_write_short(pi, pi->n_pal);
	pic2_write_file(pi, pi->pal, (size_t) (pi->n_pal * 3));
    }
    /* save comment */
    comment = pi->comment;
    if (pi->comment != NULL) {
	for (comment = pi->comment; *comment; comment++) {
	    if (*comment == '\n') {
		pic2_write_char(pi, '\r');
		pic2_write_char(pi, '\n');
	    } else if (*comment != '\r')
		pic2_write_char(pi, *comment);
	}
	pic2_write_char(pi, 0);
    }
    /* set the next block position */
    pi->next_pos = pic2_tell_file(pi);
    pi->header->size = pi->next_pos;
}

static void pic2_write_header2(pi)
struct pic2_info *pi;
{
    pic2_seek_file(pi, pi->next_pos, SEEK_SET);

    /* write terminate block */
    pic2_write_long(pi, 0);
    pic2_write_long(pi, 0);

    /* set some header information */
    if (pi->header->x_max < pi->x_max)
	pi->header->x_max = pi->x_max;
    if (pi->header->y_max < pi->x_max)
	pi->header->y_max = pi->y_max;

    pi->header->time = time(NULL);
    pic2_seek_file(pi, 0, SEEK_SET);

    /* write header image */
    pic2_write_file(pi, pi->header->magic, 4);
    pic2_write_file(pi, pi->header->name, 18);
    pic2_write_file(pi, pi->header->subtitle, 8);
    pic2_write_file(pi, pi->header->crlf0, 2);
    pic2_write_file(pi, pi->header->title, 30);
    pic2_write_file(pi, pi->header->crlf1, 2);
    pic2_write_file(pi, pi->header->saver, 30);
    pic2_write_file(pi, pi->header->crlf2, 2);
    pic2_write_file(pi, pi->header->eof, 1);
    pic2_write_file(pi, pi->header->reserve0, 1);
    pic2_write_short(pi, pi->header->flag);
    pic2_write_short(pi, pi->header->no);
    pic2_write_long(pi, pi->header->time);
    pic2_write_long(pi, pi->header->size);
    pic2_write_short(pi, pi->header->depth);
    pic2_write_short(pi, pi->header->x_aspect);
    pic2_write_short(pi, pi->header->y_aspect);
    pic2_write_short(pi, pi->header->x_max);
    pic2_write_short(pi, pi->header->y_max);
    pic2_write_long(pi, pi->header->reserve1);
}

static void pic2_write_block_header(pi)
struct pic2_info *pi;
{
    pic2_write_file(pi, pi->block->id, 4);
    pic2_write_long(pi, pi->block->size);
    pic2_write_short(pi, pi->block->flag);
    pic2_write_short(pi, pi->block->x_wid);
    pic2_write_short(pi, pi->block->y_wid);
    pic2_write_short(pi, pi->block->x_offset);
    pic2_write_short(pi, pi->block->y_offset);
    pic2_write_long(pi, pi->block->opaque);
    pic2_write_long(pi, pi->block->reserve);
}

/*
 * These functions implement the arithmetic-format compressor.
 */
#define	pic2_arith_write_one_bit(pi)	(pi->bs.bits++)

static void pic2_arith_write_zero_bit(pi)
struct pic2_info *pi;
{
    if (pi->bs.zero)
	pic2_write_bits(pi, 0, 1);

    while (pi->bs.bits--)
	pic2_write_bits(pi, 1, 1);

    pi->bs.bits = 0;
    pi->bs.zero = 1;
}

static void pic2_arith_flush_bit_buf(pi)
struct pic2_info *pi;
{
    int	i;

    for (i = 0; i < 16; i++) {
	if (pi->cc & 0x8000)
	    pic2_arith_write_one_bit(pi);
	else
	    pic2_arith_write_zero_bit(pi);
	pi->cc <<= 1;
    }
    pic2_arith_write_zero_bit(pi);
    pic2_flush_bits(pi);
}

static void pic2_arith_carry_bit(pi)
struct pic2_info *pi;
{
    pic2_write_bits(pi, 1, 1);

    if (pi->bs.bits == 0) {
	pi->bs.zero = 0;
    } else {
	while (--pi->bs.bits)
	    pic2_write_bits(pi, 0, 1);
	pi->bs.zero = 1;
    }
}

static void pic2_arith_encode_bit(pi, n, c)
struct pic2_info *pi;
int n, c;
{
    int	pp;
    long *c_sum, *c_0_sum;

    c_sum = (long *) pi->mulu_tab;
    c_0_sum = c_sum + PIC2_ARITH_CONTEXT + 1;

    if (pi->dd  == 0) {
	c_sum[c]++;
	if (n == 0)
	    c_0_sum[c]++;
	return;
    }
    pp = pi->mulu_tab[(pi->aa & 0x7f00) / 2 + c];
    if (n != 0) {
	pi->cc = pi->cc + pp;
	if (pi->cc > 0xffff) {
	    pic2_arith_carry_bit(pi);
	    pi->cc = pi->cc & 0xffff;
	}
	pi->aa = pi->aa - pp;
	while (pi->aa < 0x8000) {
	    if (pi->cc & 0x8000)
		pic2_arith_write_one_bit(pi);
	    else
		pic2_arith_write_zero_bit(pi);
	    pi->cc = (pi->cc * 2) & 0xffff;
	    pi->aa = pi->aa * 2;
	}
    } else {
	pi->aa = pp;

	while (pi->aa < 0x8000) {
	    if (pi->cc & 0x8000)
		pic2_arith_write_one_bit(pi);
	    else
		pic2_arith_write_zero_bit(pi);
	    pi->cc = (pi->cc * 2) & 0xffff;
	    pi->aa = pi->aa * 2;
	}
    }
}

static void pic2_arith_encode_nbyte(pi, n, c, max)
struct pic2_info *pi;
int n, c, max;
{
    short i;

    for (i = 0; i < n; i++) {
	pic2_arith_encode_bit(pi, 0, c + i);
    }
    if (n < max)
	pic2_arith_encode_bit(pi, 1, c + n);
}

static void pic2_arith_encode_nn(pi, n, c)
struct pic2_info *pi;
int n, c;
{
    if (n < 1) {
	pic2_arith_encode_bit(pi, 1, c);
    } else if (n < 1 + 2) {
	pic2_arith_encode_bit(pi, 0, c);
	pic2_arith_encode_bit(pi, 1, c + 1);
	n -= 1;
	pic2_arith_encode_bit(pi, n & 1, c + 8);
    } else if (n < 1 + 2 + 4) {
	pic2_arith_encode_bit(pi, 0, c);
	pic2_arith_encode_bit(pi, 0, c + 1);
	pic2_arith_encode_bit(pi, 1, c + 2);
	n -= 1 + 2;
	pic2_arith_encode_bit(pi, n & 1, c + 8);
	pic2_arith_encode_bit(pi, n & 2, c + 9);
    } else if (n < 1 + 2 + 4 + 8) {
	pic2_arith_encode_bit(pi, 0, c);
	pic2_arith_encode_bit(pi, 0, c + 1);
	pic2_arith_encode_bit(pi, 0, c + 2);
	pic2_arith_encode_bit(pi, 1, c + 3);
	n -= 1 + 2 + 4;
	pic2_arith_encode_bit(pi, n & 1, c + 8);
	pic2_arith_encode_bit(pi, n & 2, c + 9);
	pic2_arith_encode_bit(pi, n & 4, c + 10);
    } else if (n < 1 + 2 + 4 + 8 + 16) {
	pic2_arith_encode_bit(pi, 0, c);
	pic2_arith_encode_bit(pi, 0, c + 1);
	pic2_arith_encode_bit(pi, 0, c + 2);
	pic2_arith_encode_bit(pi, 0, c + 3);
	pic2_arith_encode_bit(pi, 1, c + 4);
	n -= 1 + 2 + 4 + 8;
	pic2_arith_encode_bit(pi, n & 1, c + 8);
	pic2_arith_encode_bit(pi, n & 2, c + 9);
	pic2_arith_encode_bit(pi, n & 4, c + 10);
	pic2_arith_encode_bit(pi, n & 8, c + 11);
    } else if (n < 1 + 2 + 4 + 8 + 16 + 32) {
	pic2_arith_encode_bit(pi, 0, c);
	pic2_arith_encode_bit(pi, 0, c + 1);
	pic2_arith_encode_bit(pi, 0, c + 2);
	pic2_arith_encode_bit(pi, 0, c + 3);
	pic2_arith_encode_bit(pi, 0, c + 4);
	pic2_arith_encode_bit(pi, 1, c + 5);
	n -= 1 + 2 + 4 + 8 + 16;
	pic2_arith_encode_bit(pi, n & 1, c + 8);
	pic2_arith_encode_bit(pi, n & 2, c + 9);
	pic2_arith_encode_bit(pi, n & 4, c + 10);
	pic2_arith_encode_bit(pi, n & 8, c + 11);
	pic2_arith_encode_bit(pi, n & 16, c + 12);
    } else if (n < 1 + 2 + 4 + 8 + 16 + 32 + 64) {
	pic2_arith_encode_bit(pi, 0, c);
	pic2_arith_encode_bit(pi, 0, c + 1);
	pic2_arith_encode_bit(pi, 0, c + 2);
	pic2_arith_encode_bit(pi, 0, c + 3);
	pic2_arith_encode_bit(pi, 0, c + 4);
	pic2_arith_encode_bit(pi, 0, c + 5);
	pic2_arith_encode_bit(pi, 1, c + 6);
	n -= 1 + 2 + 4 + 8 + 16 + 32;
	pic2_arith_encode_bit(pi, n & 1, c + 8);
	pic2_arith_encode_bit(pi, n & 2, c + 9);
	pic2_arith_encode_bit(pi, n & 4, c + 10);
	pic2_arith_encode_bit(pi, n & 8, c + 11);
	pic2_arith_encode_bit(pi, n & 16, c + 12);
	pic2_arith_encode_bit(pi, n & 32, c + 13);
    } else if (n < 1 + 2 + 4 + 8 + 16 + 32 + 64 + 128) {
	pic2_arith_encode_bit(pi, 0, c);
	pic2_arith_encode_bit(pi, 0, c + 1);
	pic2_arith_encode_bit(pi, 0, c + 2);
	pic2_arith_encode_bit(pi, 0, c + 3);
	pic2_arith_encode_bit(pi, 0, c + 4);
	pic2_arith_encode_bit(pi, 0, c + 5);
	pic2_arith_encode_bit(pi, 0, c + 6);
	pic2_arith_encode_bit(pi, 1, c + 7);
	n -= 1 + 2 + 4 + 8 + 16 + 32 + 64;
	pic2_arith_encode_bit(pi, n & 1, c + 8);
	pic2_arith_encode_bit(pi, n & 2, c + 9);
	pic2_arith_encode_bit(pi, n & 4, c + 10);
	pic2_arith_encode_bit(pi, n & 8, c + 11);
	pic2_arith_encode_bit(pi, n & 16, c + 12);
	pic2_arith_encode_bit(pi, n & 32, c + 13);
	pic2_arith_encode_bit(pi, n & 64, c + 14);
    } else {
	pic2_arith_encode_bit(pi, 0, c);
	pic2_arith_encode_bit(pi, 0, c + 1);
	pic2_arith_encode_bit(pi, 0, c + 2);
	pic2_arith_encode_bit(pi, 0, c + 3);
	pic2_arith_encode_bit(pi, 0, c + 4);
	pic2_arith_encode_bit(pi, 0, c + 5);
	pic2_arith_encode_bit(pi, 0, c + 6);
	pic2_arith_encode_bit(pi, 0, c + 7);
    }
}

static void pic2_arith_press_chain(pi, x)
struct pic2_info *pi;
int x;
{
    int b, d;
    pixel c;

    b = -(pi->flag_now[x]);
    c = pi->vram_now[x];
    d = 0;

    if (b < 0)
	b = 0;

    if (pi->flag_next[x] == 1 && pi->vram_next[x] == c) {
	d = 1;
	pi->flag_next[x] = -1;
    } else if (pi->flag_next[x - 1] == 1 && pi->vram_next[x - 1] == c) {
	d = 2;
	pi->flag_next[x - 1] = -2;
    } else if (pi->flag_next[x + 1] == 1 && pi->vram_next[x + 1] == c) {
	d = 3;
	pi->flag_next[x + 1] = -3;
    } else if (pi->flag_next[x - 2] == 1 && pi->vram_next[x - 2] == c) {
	d = 4;
	pi->flag_next[x - 2] = -4;
    } else if (pi->flag_next[x + 2] == 1 && pi->vram_next[x + 2] == c) {
	if ((pi->flag_now[x + 2] != 0 && pi->vram_now[x + 2] == c)
	    || (pi->flag_now[x + 1] != 0 && pi->vram_now[x + 1] == c)
	    || (pi->flag_now[x + 3] != 0 && pi->vram_now[x + 3] == c)) {
	    pic2_arith_encode_nbyte(pi, 0, 80 + 6 * b, 5);
	    return;
	}
	d = 5;
	pi->flag_next[x + 2] = -5;
    }
    pic2_arith_encode_nbyte(pi, d, 80 + 6 * b, 5);
}

static void pic2_arith_put_number(pi, xn, xa, xb)
struct pic2_info *pi;
int xn, xa, xb;
{
    short n;
    byte maxcol;

    maxcol = 0xff >> (8 - pi->header->depth / 3);

    if (xa > ((int) maxcol >> 1)) {
	if (xb > xa)
	    n = (xb - xa) * 2 - 1;
	else if (xa - ((int) maxcol - xa)  > xb)
	    n = maxcol - xb;
	else
	    n = (xa - xb) * 2;
    } else {
	if (xb <= xa)
	    n = (xa - xb) * 2;
	else if (2 * xa < xb)
	    n = xb;
 	else
	    n = (xb - xa) * 2 - 1;
    }
    pic2_arith_encode_nn(pi, n, xn);
}

static void pic2_arith_write_color(pi, x)
struct pic2_info *pi;
int x;
{
    pixel c1, c2, cc;
    short g0, r0, b0, r, g, b;
    int i, j;
    unsigned short k;
    pixel *p, *pp;
    short colbits;
    pixel rmask, gmask, bmask;
    byte maxcol;

    colbits = pi->header->depth / 3;
    rmask = (0xff >> (8 - colbits)) << (colbits * 2);
    gmask = (0xff >> (8 - colbits)) <<  colbits;
    bmask = (0xff >> (8 - colbits));
    maxcol = (byte) bmask;

    cc = pi->vram_now[x];
    c1 = pi->vram_prev[x];
    k = ((c1 >> ((colbits - 3) * 3)) & 0x1c0)
      | ((c1 >> ((colbits - 3) * 2)) & 0x038)
      | ((c1 >>  (colbits - 3)     ) & 0x007);
    if (colbits == 5)
	k = pic2_exchange_rg(k, 3);

    p = pi->cache[k];
    for (i = 0; i < (PIC2_ARITH_CACHE - 1); i++) {
	if (cc == *p++)
	    break;
    }
    if (i == (PIC2_ARITH_CACHE - 1)) {
	pp = p - 1;
	for (j = i; j > 0; j--) {
	    *--p = *--pp;
	}
	pi->cache[k][0] = cc;
	pic2_arith_encode_bit(pi, 1, pi->cache_hit_c);
	pi->cache_hit_c = 16;

	c2 = pi->vram_now[x - 1];
	r = ((c1 & rmask) + (c2 & rmask)) >> (colbits * 2 + 1);
	g = ((c1 & gmask) + (c2 & gmask)) >> (colbits     + 1);
	b = ((c1 & bmask) + (c2 & bmask)) >> (              1);

	r0 = (cc >> (colbits * 2)) & maxcol;
	g0 = (cc >>  colbits     ) & maxcol;
	b0 =  cc                   & maxcol;

	r = r + g0 - g;
	if (r < 0)
	    r = 0;
	else if (r > (short) maxcol)
	    r = maxcol;

	b = b + g0 - g;
	if (b < 0)
	    b = 0;
	else if (b > (short) maxcol)
	    b = maxcol;

	pic2_arith_put_number(pi, 32, g, g0);
	pic2_arith_put_number(pi, 48, r, r0);
	pic2_arith_put_number(pi, 64, b, b0);
    } else {
	*--p = pi->cache[k][i / 2];
	pi->cache[k][i / 2] = pi->cache[k][0];
	pi->cache[k][0] = cc;

	pic2_arith_encode_bit(pi, 0, pi->cache_hit_c);
	pi->cache_hit_c = 15;
	pic2_arith_encode_nn(pi, i, 17);
    }
}

static void pic2_arith_press_line2(pi)
struct pic2_info *pi;
{
    int x, xw, ymax;
    pixel cc;

    xw = pi->block->x_wid;
    ymax = pi->block->y_wid -1;
    cc = pi->vram_now[xw - 1];			/* last color */
    pi->vram_next[-1] = cc;

    /* mark change point */
    for (x = 0; x < xw; x++)
	if (cc != pi->vram_next[x]) {
	    pi->flag_next[x] = 1;
	    cc = pi->vram_next[x];
	} else
	    pi->flag_next[x] = 0;

    for (x = 0; x < xw; x++) {
	if (pi->flag_now[x] == 1) {			/* change point */
	    pi->flag2_now  [x + 1]++;
	    pi->flag2_now  [x + 2]++;
	    pi->flag2_next [x - 1]++;
	    pi->flag2_next [x    ]++;
	    pi->flag2_next [x + 1]++;
	    pi->flag2_next2[x - 1]++;
	    pi->flag2_next2[x    ]++;
	    pi->flag2_next2[x + 1]++;

	    /* write change point */
	    pic2_arith_encode_bit(pi, 1, pi->flag2_now[x]);

	    /* write color */
	    pic2_arith_write_color(pi, x);

	    /* if not last line, write chain */
	    if (pi->ynow - 1 < ymax)
		pic2_arith_press_chain(pi, x);
	} else if (pi->flag_now[x] == 0)		/* not on chain */
	    /* write change point */
	    pic2_arith_encode_bit(pi, 0, pi->flag2_now[x]);
	else				/* on chain */
	     /* if not on last line, write next chain */
	     if (pi->ynow - 1 < ymax)
		 pic2_arith_press_chain(pi, x);
    }
}

static int pic2_arith_press_line(pi, line)
struct pic2_info *pi;
pixel **line;
{
    int i, xw, ymax;
    long *c_sum, *c_0_sum;

    xw = pi->block->x_wid;
    ymax = pi->block->y_wid -1;
    c_sum = (long *) pi->mulu_tab;
    c_0_sum = c_sum + PIC2_ARITH_CONTEXT +1;

    pic2_handle_para(pi, 0);

    xvbzero((char *) pi->flag2_next2 - 4,
	    (8 + xw) * sizeof(pi->flag2_next2[0]));

    if (pi->ynow == 0) {			/* first line */
	int x;
	pixel cc = 0;

	if (pi->dd != 0) {			/* compress pass */
	    unsigned short c_tab[PIC2_ARITH_CONTEXT];

	    for (i = 0; i < PIC2_ARITH_CONTEXT; i++) {
		unsigned long a, b;
		a = c_0_sum[i];
		b = c_sum[i];
		while (a > 32767) {
		    a /= 2;
		    b /= 2;
		}
		if (a == b)
		    c_tab[i] = 0xffff;		/* b==0 here, too */
		else
		    c_tab[i] = (65536 * a) / b; /* a < b, so less 65536 */
	    }
	    for (i = 0; i < 16384; i++) {
		pi->mulu_tab[i] = (long) (i / 128 + 128) * (int) c_tab[i & 127] / 256;
		if (pi->mulu_tab[i] == 0)
		    pi->mulu_tab[i] = 1;	/* 0 is wrong */
	    }
	    for (i = 0; i < PIC2_ARITH_CONTEXT; i++)
		pic2_write_short(pi, c_tab[i]);

	    xvbzero((char *) pi->vram_now, xw * sizeof(pi->vram_now[0]));
	} else {				/* statistical pass */
	    xvbzero((char *) c_0_sum, PIC2_ARITH_CONTEXT * sizeof(c_0_sum[0]));
	    xvbzero((char *) c_sum, PIC2_ARITH_CONTEXT * sizeof(c_sum[0]));
	}

	/* initialize flags */
	xvbzero((char *) pi->cache, 8 * 8 * 8 * sizeof(pi->cache[0]));
	xvbzero((char *) pi->cache_pos, 8 * 8 * 8 * sizeof(pi->cache_pos[0]));

	xvbzero((char *) pi->flag2_next - 4,
		(8 + xw) * sizeof(pi->flag2_next[0]));
	xvbzero((char *) pi->flag2_next2 - 4,
		(8 + xw) * sizeof(pi->flag2_next2[0]));

	pi->vram_next[-1] = cc;
	for (x = 0; x < xw; x++)
	    if (cc != pi->vram_next[x]) {
		pi->flag_next[x] = 1;
		cc = pi->vram_next[x];
	    } else
		pi->flag_next[x] = 0;

	pi->aa = 0xffff;
	cc = 0;
	pi->cache_hit_c = 16;
    } else					/* after second line */
	pic2_arith_press_line2(pi);

    if (pi->ynow ==  ymax) {
	pi->ynow++;
	pic2_handle_para(pi, 1);
	pic2_handle_para(pi, 0);
	pic2_arith_press_line2(pi);
    }
    /* line buffer for next data */
    if (line != NULL)
	*line = pi->vram_prev;

    pi->ynow++;

    if (pi->ynow - 1 < ymax) {
	pic2_handle_para(pi, 1);
	return (pi->ynow);
    } else {					/* end */
	if (pi->dd == 0) {			/* statistical pass */
	    pi->dd = 1;
	    pi->ynow = 0;
	    pic2_handle_para(pi, 1);
	    return (0);
	} else {
	    pic2_handle_para(pi, 1);
	    pic2_arith_flush_bit_buf(pi);
	    return (-2);			/* end */
	}
    }
}

static int pic2_arith_saver_init(pi, line)
struct pic2_info *pi;
pixel **line;
{
    pi->ynow = 0;

    /* check the color depth */
    if (pi->header->depth % 3)
	pic2_error(pi, PIC2_DEPTH);

    /* set next line function */
    pi->next_line = pic2_arith_press_line;

    if (line != NULL)
	*line = pi->vram_next + 4;

    pic2_seek_file(pi, pi->next_pos + PIC2_BLOCK_HEADER_SIZE, SEEK_SET);

    /* clear bit field marker */
    pi->bs.rest = 0;
    pi->bs.cur = 0;
    pi->bs.zero = 0;
    pi->bs.bits = 0;

    return (0);
}

/*
 * These functions are fast pic2 format compressor.
 */
static void pic2_fast_write_length(pi, n)
struct pic2_info *pi;
int n;
{
    int	a, b;
    static const unsigned short len_data[8][2] = {
	{1, 0},
	{1, 0},
	{3, 4},
	{3, 5},
	{5, 24},
	{5, 25},
	{5, 26},
	{5, 27},
    };

    n++;
    if (n < 8)
	pic2_write_bits(pi, len_data[n][1], len_data[n][0]);
    else {
	a = 0;
	b = 2;
	while (n > b - 1) {
	    a = a + 1;
	    b = b * 2;
	}
	pic2_write_bits(pi, 0xfffffffe, a + 1);
	if (a > 0)
	    pic2_write_bits(pi, n - b / 2, a);
    }
}

static void pic2_fast_press_chain(pi, x)
struct pic2_info *pi;
int x;
{
    int ymax;
    pixel cc;

    ymax = pi->block->y_wid -1;
    cc = pi->vram_now[x];

    if (pi->ynow - 1 == ymax) {
	pic2_write_bits(pi, 0, 1);
	return;
    }
    if (pi->flag_next[x] == 1 && pi->vram_next[x] == cc) {
	pi->flag_next[x] = -1;
	pic2_write_bits(pi, 3, 2);
    } else if (pi->flag_next[x - 1] == 1 && pi->vram_next[x - 1] == cc) {
	pi->flag_next[x - 1] = -1;
	pic2_write_bits(pi, 11, 4);
    } else if (pi->flag_next[x + 1] == 1 && pi->vram_next[x + 1] == cc) {
	pi->flag_next[x + 1] = -1;
	pic2_write_bits(pi, 9, 4);
    } else if (pi->flag_next[x - 2] == 1 && pi->vram_next[x - 2] == cc) {
	pi->flag_next[x - 2] = -1;
	pic2_write_bits(pi, 10, 4);
    } else if ((pi->flag_next[x + 2] == 1 && pi->vram_next[x + 2] == cc)
		&& !(pi->flag_now[x + 2] != 0 && pi->vram_now[x + 2] == cc)) {
	pi->flag_next[x + 2] = -1;
	pic2_write_bits(pi, 8, 4);
    } else
	pic2_write_bits(pi, 0, 1);
}

static void pic2_fast_press_chain2(pi, x)
struct pic2_info *pi;
int x;
{
    int ymax;
    pixel cc;
    char *chain_buff;

    ymax = pi->block->y_wid -1;
    chain_buff = (char *) pi->mulu_tab;
    cc = pi->vram_now[x];

    if (pi->ynow - 1 == ymax) {
	chain_buff[pi->cc++] = 0;
	return;
    }
    if (pi->flag_next[x] == 1 && pi->vram_next[x] == cc) {
	pi->flag_next[x] = -1;
	chain_buff[pi->cc++] = 1;
    } else if (pi->flag_next[x - 1] == 1 && pi->vram_next[x - 1] == cc) {
	pi->flag_next[x - 1] = -1;
	chain_buff[pi->cc++] = 2;
    } else if (pi->flag_next[x + 1] == 1 && pi->vram_next[x + 1] == cc) {
	pi->flag_next[x + 1] = -1;
	chain_buff[pi->cc++] = 3;
    } else if (pi->flag_next[x - 2] == 1 && pi->vram_next[x - 2] == cc) {
	pi->flag_next[x - 2] = -1;
	chain_buff[pi->cc++] = 4;
    } else if ((pi->flag_next[x + 2] == 1 && pi->vram_next[x + 2] == cc)
	       && !(pi->flag_now[x + 2] != 0 && pi->vram_now[x + 2] == cc)) {
	pi->flag_next[x + 2] = -1;
	chain_buff[pi->cc++] = 5;
    } else
	chain_buff[pi->cc++] = 0;
}

static void pic2_fast_flush_chain(pi)
struct pic2_info *pi;
{
    int i;
    char *chain_buf;

    chain_buf = (char *) pi->mulu_tab;
    for (i = 0; i < pi->cc; i++){
	switch (chain_buf[i]) {
	case 0:
	    pic2_write_bits(pi, 0, 1);
	    break;
	case 1:
	    pic2_write_bits(pi, 3, 2);
	    break;
	case 2:
	    pic2_write_bits(pi, 11, 4);
	    break;
	case 3:
	    pic2_write_bits(pi, 9, 4);
	    break;
	case 4:
	    pic2_write_bits(pi, 10, 4);
	    break;
	case 5:
	    pic2_write_bits(pi, 8, 4);
	    break;
	}
    }
    pi->cc = 0;
}

static void pic2_fast_write_color(pi, x)
struct pic2_info *pi;
int x;
{
    pixel cc, bc;
    unsigned short j, k, m;
    short depth, colbits;
    pixel (*cache)[PIC2_FAST_CACHE];

    depth = pi->header->depth;
    colbits = depth / 3;
    cache = (pixel (*)[PIC2_FAST_CACHE]) pi->cache;

    bc = pi->vram_now[x - 1];
    bc = pic2_exchange_rg(bc, colbits);
    k = pic2_shift_bits(bc, 8 - depth);
    cc = pi->vram_now[x];
    m = pi->cache_pos[k];

    for (j = 0; j < PIC2_FAST_CACHE; j++)
	if (cache[k][(m + j) & (PIC2_FAST_CACHE - 1)] == cc)
	    break;

    if (j == PIC2_FAST_CACHE) {
	m = (m - 1) & (PIC2_FAST_CACHE - 1);
	pi->cache_pos[k] = m;
	cache[k][m] = cc;

	cc = pic2_exchange_rg(cc, colbits);
	pic2_write_bits(pi, 0, 1);
	pic2_write_bits(pi, cc, depth);
    } else {
	pic2_write_bits(pi, 1, 1);
	pic2_write_bits(pi, j, 6);
    }
}

static void pic2_fast_press_line2(pi)
struct pic2_info *pi;
{
    int x, xw;
    pixel cc;

    xw = pi->block->x_wid;
    cc = pi->vram_now[xw - 1];			/* last color */
    pi->vram_next[-1] = cc;

    /* mark change point */
    for (x = 0; x < xw; x++)
	if (cc != pi->vram_next[x]) {
	    pi->flag_next[x] = 1;
	    cc = pi->vram_next[x];
	} else
	    pi->flag_next[x] = 0;

    for (x = 0; x < xw; x++)
	if (pi->flag_now[x] == 1) {			/* change point */
	    if (pi->aa >= 1023)
		pi->aa++;
	    pic2_fast_write_length(pi, pi->aa);
	    pic2_fast_flush_chain(pi);
	    pi->aa = 0;
	    pic2_fast_write_color(pi, x);
	    pic2_fast_press_chain(pi, x);
	} else if (pi->flag_now[x] == 0) {
	    pi->aa++;
	} else {
	    pic2_fast_press_chain2(pi, x);
	    if (pi->cc == 1023) {
		pic2_fast_write_length(pi, 1023);
		pic2_fast_flush_chain(pi);
		pi->aa = 0;
	    }
	}
}

static int pic2_fast_press_line(pi, line)
struct pic2_info *pi;
pixel **line;
{
    int xw, ymax;

    xw = pi->block->x_wid;
    ymax = pi->block->y_wid -1;

    pic2_handle_para(pi, 0);

    if (pi->ynow == 0) {			/* first line */
	int x;
	pixel cc = 0;

	/* initialize flags */
	xvbzero((char *) pi->cache, 256 * sizeof(pi->cache[0]));
	xvbzero((char *) pi->cache_pos,
		PIC2_FAST_CACHE * sizeof(pi->cache_pos[0]));

	/* mark change point */
	pi->vram_next[-1] = cc;
	for (x = 0; x < xw; x++)
	    if (cc != pi->vram_next[x]) {
		pi->flag_next[x] = 1;
		cc = pi->vram_next[x];
	    } else
		pi->flag_next[x] = 0;

	pi->cc = 0;
	pi->aa = 0;
    } else					/* after second line */
	pic2_fast_press_line2(pi);

    if (pi->ynow ==  ymax) {
	pi->ynow++;
	pic2_handle_para(pi, 1);
	pic2_handle_para(pi, 0);
	pic2_fast_press_line2(pi);
    }
    /* line buffer for next data */
    if (line != NULL)
	*line = pi->vram_prev;

    pi->ynow++;

    if (pi->ynow - 1 < ymax) {
	pic2_handle_para(pi, 1);
	return (pi->ynow);
    } else {					/* end */
	pic2_handle_para(pi, 1);
	if (pi->aa >= 1023)
	    pi->aa++;
	pic2_fast_write_length(pi, pi->aa);
	pic2_fast_flush_chain(pi);
	return (-2);				/* end */
    }
}

static int pic2_fast_saver_init(pi, line)
struct pic2_info *pi;
pixel **line;
{
    pi->ynow = 0;

    /* check the color depth */
    if (pi->header->depth % 3)
	pic2_error(pi, PIC2_DEPTH);

    /* set next line function */
    pi->next_line = pic2_fast_press_line;
    if (line != NULL)
	*line = pi->vram_next + 4;

    pic2_seek_file(pi, pi->next_pos + PIC2_BLOCK_HEADER_SIZE, SEEK_SET);

    /* clear bit field marker */
    pi->bs.rest = 0;
    pi->bs.cur = 0;

    return (0);
}

/*
 * These functions are beta pic2 format compressor.
 */
static int pic2_beta_press_line(pi, line)
struct pic2_info *pi;
pixel **line;
{
    int i, xw, ymax;
    byte *p;
    pixel *pc;
    short depth, pixbyte, colbits;

    depth = pi->header->depth;
    pixbyte = depth / 8 + ((depth % 8) > 0);
    colbits = depth / 3;

    xw = pi->block->x_wid;
    ymax = pi->block->y_wid - 1;

    pc = pi->vram_now;
    p = (byte *) pi->vram_prev;
    if (pixbyte == 3) {
	for (i = 0; i < xw; i++, pc++) {
	    *p++ = *pc >> 16;
	    *p++ = *pc >>  8;
	    *p++ = *pc;
	}
	pic2_write_file(pi, pi->vram_prev, (size_t) (xw * 3));
    } else if (pixbyte == 2) {
	if (strncmp(pi->block->id, "P2BM", 4) == 0)
	    for (i = 0; i < xw; i++, pc++) {
		if (colbits == 5) {
		    *pc = pic2_exchange_rg(*pc, colbits);
		    *pc <<= 1;
		}
		*p++ = *pc >> 8;
		*p++ = *pc;
	    }
	else
	    for (i = 0; i < xw; i++, pc++) {
		if (colbits == 5) {
		    *pc = pic2_exchange_rg(*pc, colbits);
		    *pc <<= 1;
		}
		*p++ = *pc;
		*p++ = *pc >> 8;
	    }
	pic2_write_file(pi, pi->vram_prev, (size_t) (xw * 2));
    } else {
	for (i = 0; i < xw; i++, pc++)
	    *p++ = *pc;
	pic2_write_file(pi, pi->vram_prev, (size_t) xw);
    }
    if (line != NULL)
	*line = pi->vram_now;

    pi->ynow++;
    if (pi->ynow > ymax)
	return (-2);
    return (pi->ynow);
}

static int pic2_beta_saver_init(pi, line)
struct pic2_info *pi;
pixel **line;
{
    pi->ynow = 0;

    *line = pi->vram_now;
    pi->next_line = pic2_beta_press_line;
    pic2_seek_file(pi, pi->next_pos + PIC2_BLOCK_HEADER_SIZE, SEEK_SET);
    return (0);
}

/*
 * This function saves compressed data.
 */
static void pic2_write_data(pi, data, ptype, x_offset, y_offset, w, h,
			    rmap, gmap, bmap, type, depth)
struct pic2_info *pi;
byte *data;
int ptype;
int x_offset, y_offset;
int w, h;
byte *rmap, *gmap, *bmap;
int type, depth;
{
    int i, line;
    pixel *linep;
    short colbits;

    colbits = pi->header->depth / 3;

    line = pic2_save_block(pi, &linep, x_offset, y_offset, w, h,
			   form_tab[type].id, 0xffffffff);
    while (line >= 0) {
	byte r, g, b;
	int pic_idx;

	pic_idx = line * w * ((ptype == PIC24) ? 3 : 1);

	for (i = 0; i < w; i++) {
	    if (ptype != PIC24) {
		r = rmap[data[pic_idx]];
		g = gmap[data[pic_idx]];
		b = bmap[data[pic_idx]];
		pic_idx++;
	    } else {
		r = data[pic_idx++];
		g = data[pic_idx++];
		b = data[pic_idx++];
	    }
	    if (pi->writing_grey)
		r = g = b = MONO(r, g, b);

	    r = pic2_convert_color_bits(r, 8, colbits);
	    g = pic2_convert_color_bits(g, 8, colbits);
	    b = pic2_convert_color_bits(b, 8, colbits);

	    linep[i] = ((pixel) r << (colbits * 2))
		     | ((pixel) g <<  colbits     )
		     | ((pixel) b                 );
	}
	line = pic2_next_line(pi, &linep);
	WaitCursor();
    }
}

/*
 * This function compresses/extracts one line buffer.
 */
static int pic2_next_line(pi, line)
struct pic2_info *pi;
pixel **line;
{
    int res;

    res = pi->next_line(pi, line);
    if (res == -2) {
	if (pi->mode == PIC2_WRITE_MODE) {
	    long new_pos;

	    new_pos = pic2_tell_file(pi);
	    pi->block->size = new_pos - pi->next_pos;
	    pic2_seek_file(pi, pi->next_pos, SEEK_SET);
	    pic2_write_block_header(pi);
	    pi->next_pos = new_pos;
	    if (DEBUG)
		pic2_show_pic2_info(pi);
	}
	pic2_free_buffer(pi);
    }
    return (res);
}

/*
 * These functions find the pic2 image block.
 * pic2_next_block:
 *	moves the file pointer to the next image block.
 * pic2_find_block:
 *	finds the first image block and moves the file pointer there.
 */
static int pic2_next_block(pi)
struct pic2_info *pi;
{
    int i;

    if (pi->mode != PIC2_READ_MODE)
	return (-1);

    /* go to block for read */
    pic2_seek_file(pi, pi->next_pos, SEEK_SET);

    /* read the head of block header */
    pic2_read_block_header1(pi);

    /* end block ? */
    if (pi->block->id[0] == 0)
	return (0);

    /* set current block */
    pi->block_pos = pi->next_pos;

    /* set next block */
    pi->next_pos += pi->block->size;

   /* check block id */
    for (i = 0; i < n_form_tab; i++) {
	if (xvbcmp(pi->block->id, form_tab[i].id, (size_t) 4) == 0)
	    break;
    }
    if (i == n_form_tab)
	return (2);

    /* read the rest of block header */
    pic2_read_block_header2(pi);

    if (pi->block->x_offset + pi->block->x_wid > pi->x_max)
	pi->x_max = pi->block->x_offset + pi->block->x_wid;

    if (pi->block->y_offset + pi->block->y_wid > pi->y_max)
	pi->y_max = pi->block->y_offset + pi->block->y_wid;

    if (DEBUG)
	pic2_show_pic2_info(pi);
    return (1);
}

static int pic2_find_block(pi)
struct pic2_info *pi;
{
    if (pi->mode != PIC2_READ_MODE)
	return (-1);

    pi->next_pos = pi->header->size;
    return (pic2_next_block(pi));
}

/*
 * These functions load/save the pic2 image block.
 * pic2_load_block:
 *	initializes loader information with current block information.
 * pic2_save_block:
 *	initializes saver information.
 */
static int pic2_load_block(pi)
struct pic2_info *pi;
{
    int i;

    for (i = 0; i < n_form_tab; i++) {
	if (xvbcmp(pi->block->id, form_tab[i].id, (size_t) 4) == 0)
	    break;
    }
    if (i == n_form_tab)
	return (2);

    pic2_alloc_buffer(pi);
    return (form_tab[i].loader_init(pi));
}

static int pic2_save_block(pi, line, x, y, xw, yw, id, opaque)
struct pic2_info *pi;
pixel **line;
int x, y, xw, yw;
char *id;
pixel opaque;
{
    int i;

    for (i = 0; i < n_form_tab; i++) {
	if (xvbcmp(id, form_tab[i].id, (size_t) 4) == 0)
	    break;
    }
    if (i == n_form_tab)
	return (2);

    strncpy(pi->block->id, id, 4);
    pi->block->x_wid = xw;
    pi->block->y_wid = yw;
    pi->block->x_offset = x;
    pi->block->y_offset = y;
    pi->block->reserve = 0;

    if (x < 0)
	x = 0;
    if (y < 0)
	y = 0;
    if (x + xw > pi->x_max)
	pi->x_max = x + xw;
    if (y + yw > pi->y_max)
	pi->y_max = y + yw;

    if (opaque != 0xffffffff) {
	pi->block->flag = 1;
	pi->block->opaque = opaque;
    } else {
	pi->block->flag = 0;
	pi->block->opaque = 0;
    }
    pic2_alloc_buffer(pi);

    return (form_tab[i].saver_init(pi, line));
}

/*
 * These functions set/get palettes.
 * pic2_read_palette:
 *	copy the palettes from pic2_info to PICINFO.
 * pic2_write_palette:
 *	copy the palettes from PICINFO to pic2_info.
 */
#ifndef PIC2_IGNORE_UNUSED_FUNCTIONS
static void pic2_read_palette(pi, r, g, b)
struct pic2_info *pi;
byte *r, *g, *b;
{
    int i;

    if (pi->n_pal > 256)
	pi->n_pal = 256;

    if (pi->pal_bits > 8)
	pi->pal_bits = 8;

    for (i = 0; i < pi->n_pal; i++) {
	*r++ =pic2_convert_color_bits(pi->pal[i][0] >> (8 - pi->pal_bits),
				      pi->pal_bits, 8);
	*g++ =pic2_convert_color_bits(pi->pal[i][1] >> (8 - pi->pal_bits),
				      pi->pal_bits, 8);
	*b++ =pic2_convert_color_bits(pi->pal[i][2] >> (8 - pi->pal_bits),
				      pi->pal_bits, 8);
    }
}

static void pic2_write_palette(pi, n_pal, pal_bits, r, g, b)
struct pic2_info *pi;
int n_pal, pal_bits;
byte *r, *g, *b;
{
    int i;

    if (n_pal > 256)
	pi->n_pal = 256;
    else
	pi->n_pal = n_pal;

    if (pal_bits > 8)
	pi->pal_bits = 8;
    else
	pi->pal_bits = pal_bits;

    for (i = 0; i < n_pal; i++) {
	pi->pal[i][0] = pic2_convert_color_bits(*r++, 8, pal_bits)
	    << (8 - pal_bits);
	pi->pal[i][1] = pic2_convert_color_bits(*g++, 8, pal_bits)
	    << (8 - pal_bits);
	pi->pal[i][2] = pic2_convert_color_bits(*b++, 8, pal_bits)
	    << (8 - pal_bits);
    }
}
#endif /* PIC2_IGNORE_UNUSED_FUNCTIONS */

/*
 * These functions handle color bits.
 * pic2_convert_color_bits:
 *	converts color bits.
 * pic2_pad_color_bits:
 *	pads color bits.
 * pic2_reduce_color_bits:
 *	reduces color bits.
 * pic2_exchange_rg:
 *      exchanges red and green values.
 */
static byte pic2_convert_color_bits(c, from, to)
int c, from, to;
{
    if (from == to)
	return ((byte) c);
    else if (from < to)
	return (pic2_pad_color_bits(c, from, to));
    else
	return (pic2_reduce_color_bits(c, from, to));
}

static byte pic2_pad_color_bits(c, from, to)
int c, from, to;
{
    byte p = 0;

    do {
	to -= from;
	p |= pic2_shift_bits(c, to);
    } while (to >= 0);
    return (p);
}

static byte pic2_reduce_color_bits(c, from, to)
int c, from, to;
{
    return ((byte) (c >> (from - to)));
}

static pixel pic2_exchange_rg(p, colbits)
pixel p;
int colbits;
{
    pixel rmask, gmask, bmask;

    rmask = (0xff >> (8 - colbits)) << (colbits * 2);
    gmask = (0xff >> (8 - colbits)) <<  colbits;
    bmask = (0xff >> (8 - colbits));

    p = ((p << colbits) & rmask)
      | ((p >> colbits) & gmask)
      | ( p             & bmask);
    return (p);
}

/*
 * This function handles work memory buffer.
 */
static void pic2_handle_para(pi, mode)
struct pic2_info *pi;
int mode;
{
    static pixel *vram_prev, *vram_now, *vram_next;
    static short *flag_now, *flag_next;
    static short *flag2_now, *flag2_next, *flag2_next2;

    switch (mode) {
    case 0:
	vram_prev = pi->vram_prev;
	vram_now = pi->vram_now;
	vram_next = pi->vram_next;
	flag_now = pi->flag_now;
	flag_next = pi->flag_next;
	flag2_now = pi->flag2_now;
	flag2_next = pi->flag2_next;
	flag2_next2 = pi->flag2_next2;
	pi->vram_prev += 4;
	pi->vram_now += 4;
	pi->vram_next += 4;
	pi->flag_now += 4;
	pi->flag_next += 4;
	pi->flag2_now += 4;
	pi->flag2_next += 4;
	pi->flag2_next2 += 4;
	break;
    case 1:
	pi->vram_prev = vram_now;
	pi->vram_now = vram_next;
	pi->vram_next = vram_prev;
	pi->flag_now = flag_next;
	pi->flag_next = flag_now;
	pi->flag2_now = flag2_next;
	pi->flag2_next = flag2_next2;
	pi->flag2_next2 = flag2_now;
	break;
    }
}

/*
 * These functions alloc/free work memory.
 * pic2_alloc_buffer:
 *	alloc work memory buffer.
 * pic2_free_buffer:
 *	free work memory buffer.
 */
static int pic2_alloc_buffer(pi)
struct pic2_info *pi;
{
    int wid;
    byte *p;

    if (pi->buf != NULL)
	return (-1);

    wid = pi->block->x_wid;

    p = pi->buf = (byte *) pic2_new((wid + 8) * sizeof(pixel) * 3   // GRR POSSIBLE OVERFLOW / FIXME
				    + sizeof(pi->cache[0]) * 8 * 8 * 8
				    + sizeof(pi->cache_pos[0]) * 8 * 8 * 8
				    + sizeof(pi->mulu_tab[0]) * 16384
				    + sizeof(pi->flag_now[0]) * ((wid+8) * 5),
				    "pic2_alloc_buffer");

    pi->vram_prev = (pixel *) p;
    p += (wid + 8) * sizeof(pixel);
    pi->vram_now = (pixel *) p;
    p += (wid + 8) * sizeof(pixel);
    pi->vram_next = (pixel *) p;
    p += (wid + 8) * sizeof(pixel);
    pi->cache = (pixel (*)[PIC2_ARITH_CACHE]) p;
    p += sizeof(pi->cache[0]) * 8 * 8 * 8;
    pi->cache_pos = (unsigned short *) p;
    p += sizeof(pi->cache_pos[0]) * 8 * 8 * 8;
    pi->mulu_tab = (unsigned short *) p;
    p += sizeof(pi->mulu_tab[0]) * 16384;
    pi->flag_now = (short *) p;
    p += sizeof(pi->flag_now[0]) * (wid + 8);
    pi->flag_next = (short *) p;
    p += sizeof(pi->flag_next[0]) * (wid + 8);
    pi->flag2_now = (short *) p;
    p += sizeof(pi->flag2_now[0]) * (wid + 8);
    pi->flag2_next = (short *) p;
    p += sizeof(pi->flag2_next[0]) * (wid + 8);
    pi->flag2_next2 = (short *) p;
    p += sizeof(pi->flag2_next2[0]) * (wid + 8);
    return (0);
}

static void pic2_free_buffer(pi)
struct pic2_info *pi;
{
    free(pi->buf);
    pi->buf = NULL;
}

/*
 * These functions handle the file pointer.
 * pic2_seek_file:
 *	moves the file pointer.
 * pic2_tell_file:
 *	tells the location of the file pointer.
 */
static long pic2_seek_file(pi, offset, whence)
struct pic2_info *pi;
long offset;
int whence;
{
    long n;

    n = fseek(pi->fp, offset, whence);
    if (n < 0)
	pic2_file_error(pi, PIC2_CORRUPT);

    return (n);
}

static long pic2_tell_file(pi)
struct pic2_info *pi;
{
    return (ftell(pi->fp));
}

/*
 * These functions handle file.
 * pic2_read_file:
 *	reads data from the file.
 * pic2_read_long:
 *	reads long word data from the file and converts to internal expression.
 * pic2_read_short:
 *	reads word data from the file and converts to internal expression.
 * pic2_read_char:
 *	reads byte data from the file.
 * pic2_write_file:
 *	writes data to the file.
 * pic2_write_long:
 *	converts long word data to common expression and writes to the file.
 * pic2_write_short:
 *	converts word data to common expression and writes to the file.
 * pic2_write_char:
 *	writes byte data to the file.
 */
static int pic2_read_file(pi, buf, size)
struct pic2_info *pi;
void *buf;
size_t size;
{
    if (fread(buf, (size_t) 1, size, pi->fp) < size)
	pic2_file_error(pi, PIC2_CORRUPT);
    return (0);
}

static long pic2_read_long(pi)
struct pic2_info *pi;
{
    byte buf[4];

    if (fread(buf, (size_t) 4, (size_t) 1, pi->fp) < 1)
	pic2_file_error(pi, PIC2_CORRUPT);
    return (pic2_cextolong(buf));
}

static short pic2_read_short(pi)
struct pic2_info *pi;
{
    byte buf[2];

    if (fread(buf, (size_t) 2, (size_t) 1, pi->fp) < 1)
	pic2_file_error(pi, PIC2_CORRUPT);
    return (pic2_cextoshort(buf));
}

static char pic2_read_char(pi)
struct pic2_info *pi;
{
    int c;

    if ((c = fgetc(pi->fp)) == EOF)
	pic2_file_error(pi, PIC2_CORRUPT);
    return ((char) c);
}

static int pic2_write_file(pi, buf, size)
struct pic2_info *pi;
void *buf;
size_t size;
{
    if (fwrite(buf, (size_t) 1, size, pi->fp) < size)
	pic2_error(pi, PIC2_WRITE);
    return (0);
}

static int pic2_write_long(pi, n)
struct pic2_info *pi;
long n;
{
    byte buf[4];

    pic2_longtocex(buf, n);
    if (fwrite(buf, (size_t) 4, (size_t) 1, pi->fp) < 1)
	pic2_error(pi, PIC2_WRITE);
    return (0);
}

static int pic2_write_short(pi, n)
struct pic2_info *pi;
int n;
{
    byte buf[2];

    pic2_shorttocex(buf, n);
    if (fwrite(buf, (size_t) 2, (size_t) 1, pi->fp) < 1)
	pic2_error(pi, PIC2_WRITE);
    return (0);
}

static int pic2_write_char(pi, c)
struct pic2_info *pi;
int c;
{
    if (fputc(c, pi->fp) == EOF)
	pic2_error(pi, PIC2_WRITE);
    return (0);
}

/*
 * These functions access the bit stream.
 * pic2_read_bits:
 *	reads the specified bits from the file.
 * pic2_write_bits:
 *	writes the specified bits to the file.
 * pic2_flush_bits:
 *	flushes bit buffer to the file.
 */
static unsigned long pic2_read_bits(pi, bits)
struct pic2_info *pi;
int bits;
{
    unsigned long r = 0;

    while (bits > 0) {
	while (pi->bs.rest > 0 && bits > 0) {
	    r = (r << 1) | (pi->bs.cur & 0x80 ? 1 : 0);
	    pi->bs.cur <<= 1;
	    pi->bs.rest--;
	    bits--;
	}
	if (bits > 0) {
	    int c;
	    if ((c = fgetc(pi->fp)) == EOF)
		pic2_file_error(pi, PIC2_CORRUPT);
	    pi->bs.cur  = (byte) c;
	    pi->bs.rest = 8;
	}
    }
    return r;
}

static void pic2_write_bits(pi, dat, bits)
struct pic2_info *pi;
unsigned long dat;
int bits;
{
    unsigned long dat_mask = 1 << (bits - 1);

    while (bits > 0) {
	while (pi->bs.rest < 8 && bits > 0) {
	    pi->bs.cur <<= 1;
	    if (dat & dat_mask)
		pi->bs.cur |= 1;
	    pi->bs.rest++;
	    bits--;
	    dat_mask >>= 1;
	}
	if (pi->bs.rest >= 8) {
	    if ((fputc((int) pi->bs.cur, pi->fp)) == EOF)
		pic2_error(pi, PIC2_WRITE);
	    pi->bs.cur  = 0;
	    pi->bs.rest = 0;
	}
    }
}

static void pic2_flush_bits(pi)
struct pic2_info *pi;
{
    if (pi->bs.rest < 8) {
	pi->bs.cur <<= 8 - pi->bs.rest;
	if (fputc((int) pi->bs.cur, pi->fp) == EOF)
		pic2_error(pi, PIC2_WRITE);
	pi->bs.cur  = 0;
	pi->bs.rest = 0;
    }
}

/*
 * These functions initialize or clean up structures.
 * pic2_init_info:
 *	initializes a pic2_info structure.
 * pic2_cleanup_pic2_info:
 *	cleans up a pic_info structure.
 * pic2_cleanup_pinfo:
 *	cleans up a PICINFO structure.
 */
static void pic2_init_info(pi)
struct pic2_info *pi;
{
    xvbzero((char *) pi, sizeof(struct pic2_info));
    pi->header = pic2_new(sizeof(struct pic2_header), "pic2_init_info#1");
    pi->block = pic2_new(sizeof(struct pic2_block), "pic2_init_info#2");
}

static void pic2_cleanup_pic2_info(pi, writing)
struct pic2_info *pi;
int writing;
{
    if (!writing && pi->fp)
	fclose(pi->fp);
    if (pi->header)
	free(pi->header);
    if (pi->block)
	free(pi->block);
    pi->fp = NULL;
    pi->header = NULL;
    pi->block = NULL;
    pi->comment = NULL;
}

static void pic2_cleanup_pinfo(pinfo)
PICINFO *pinfo;
{
    if (pinfo->pic){
	free(pinfo->pic);
	pinfo->pic = NULL;
    }
    if (pinfo->comment){
	free(pinfo->comment);
	pinfo->comment = NULL;
    }
}

/*
 * Error Handlers.
 * pic2_memory_error:
 *	shows an error message and terminates.
 * pic2_error:
 *	shows a non-file error message and jumps to the entry for errors.
 * pic2_file_error:
 *	shows a file error message and jumps to the entry for errors.
 */
static void pic2_memory_error(scm, fn)
char *scm, *fn;
{
    char buf[128];
    sprintf(buf, "%s: can't allocate memory. (%s)", scm, fn);
    FatalError(buf);
}

static void pic2_error(pi, mn)
struct pic2_info *pi;
int mn;
{
    SetISTR(ISTR_WARNING, "%s", pic2_msgs[mn]);
    longjmp(pi->jmp, 1);
}

static void pic2_file_error(pi, mn)
    struct pic2_info *pi;
    int mn;
{
    if (feof(pi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", pic2_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", pic2_msgs[mn], ERRSTR(errno));
    longjmp(pi->jmp, 1);
}

static void pic2_show_pic2_info(pi)
    struct pic2_info *pi;
{
    fprintf(stderr, "file size: %ld.\n", pi->fsize);
    fprintf(stderr, "full image size: %dx%d\n", pi->x_max, pi->y_max);
    fprintf(stderr, "number of palettes: %d\n", pi->n_pal);
    fprintf(stderr, "depth of palettes: %d\n", pi->pal_bits);
    fprintf(stderr, "current block position: %ld\n", pi->block_pos);
    fprintf(stderr, "next block position: %ld\n\n", pi->next_pos);

    fprintf(stderr, "header flag: %x\n", pi->header->flag);
    fprintf(stderr, "header size: %ld\n", pi->header->size);
    fprintf(stderr, "x_aspect: %d, y_aspect: %d\n",
	    pi->header->x_aspect, pi->header->y_aspect);
    fprintf(stderr, "number of color bits: %d\n\n", pi->header->depth);

    fprintf(stderr, "image block id: %s\n", pi->block->id);
    fprintf(stderr, "image block size: %ld\n", pi->block->size);
    fprintf(stderr, "block flag: %x\n", pi->block->flag);

    fprintf(stderr, "block image size: %dx%d\n",
	    pi->block->x_wid, pi->block->y_wid);
    fprintf(stderr, "x_offset: %d\n", pi->block->x_offset);
    fprintf(stderr, "y_offset: %d\n", pi->block->y_offset);
    fprintf(stderr, "opaque color: %lx\n\n", pi->block->opaque);
}

/*
 * This function is similar to strncpy.
 * But this pads with whitespace after the null character.
 */
static char *pic2_strncpy(dest, src, n)
char *dest, *src;
size_t n;
{
    char *r;

    r = dest;
    while (n--)
	if ((src != NULL) && (*src != '\r') && (*src != '\n') && *src)
	    *dest++ = *src++;
	else
	    *dest++ = ' ';
    return (r);
}

/*
 * These functions create a memory block.
 */
static void *pic2_malloc(size, fn)
size_t size;
char *fn;
{
    void *p;

    p = (void *) malloc(size);
    if (p == NULL)
	pic2_memory_error("malloc", fn);
    return (p);
}

static void *pic2_new(size, fn)
size_t size;
char *fn;
{
    void *p;

    p = (void *) pic2_malloc(size, fn);
    xvbzero((char *) p, size);
    return (p);
}




/**** Stuff for PIC2Dialog box ****/

#define TWIDE    320
#define THIGH	 178
#define T_NBUTTS 2
#define T_BOK    0
#define T_BCANC  1
#define BUTTH    24

static void drawTD    PARM((int,int,int,int));
static void clickTD   PARM((int,int));
static void doCmd     PARM((int));
static void writePIC2 PARM((void));

/* local variables */
static FILE  *fp;
static char  *filename;
static int   colorType;
static int   append;
static int   x_offset;
static int   y_offset;
static BUTT  tbut[T_NBUTTS];
static RBUTT *typeRB;
static RBUTT *depthRB;



/***************************************************/
void CreatePIC2W()
{
    int	     y;

    pic2W = CreateWindow("xv pic2", "XVpic2", NULL,
			TWIDE, THIGH, infofg, infobg, 0);
    if (!pic2W)
	FatalError("can't create pic2 window!");

    XSelectInput(theDisp, pic2W,
		 ExposureMask | ButtonPressMask | KeyPressMask);

    BTCreate(&tbut[T_BOK], pic2W, TWIDE-140-1, THIGH-10-BUTTH-1, 60, BUTTH,
	     "Ok", infofg, infobg, hicol, locol);

    BTCreate(&tbut[T_BCANC], pic2W, TWIDE-70-1, THIGH-10-BUTTH-1, 60, BUTTH,
	     "Cancel", infofg, infobg, hicol, locol);

    y = 55;
    typeRB = RBCreate(NULL, pic2W, 36, y,          "P2SS",
		      infofg, infobg,hicol,locol);
    RBCreate(typeRB, pic2W, 36, y+18,              "P2SF",
	     infofg, infobg,hicol,locol);
    RBCreate(typeRB, pic2W, 36, y+36,              "P2BM",
	     infofg, infobg, hicol, locol);
    RBCreate(typeRB, pic2W, 36, y+54,              "P2BI",
	     infofg, infobg, hicol, locol);

    depthRB = RBCreate(NULL, pic2W, TWIDE/2-16, y, "  3bit",
		       infofg, infobg,hicol,locol);
    RBCreate(depthRB, pic2W, TWIDE/2-16, y+18,     "  6bit",
	     infofg, infobg,hicol,locol);
    RBCreate(depthRB, pic2W, TWIDE/2-16, y+36,     "  9bit",
	     infofg, infobg, hicol, locol);
    RBCreate(depthRB, pic2W, TWIDE/2-16, y+54,     "12bit",
	     infofg, infobg, hicol, locol);
    RBCreate(depthRB, pic2W, TWIDE/4*3-16, y,      "15bit",
	     infofg, infobg, hicol, locol);
    RBCreate(depthRB, pic2W, TWIDE/4*3-16, y+18,   "18bit",
	     infofg, infobg, hicol, locol);
    RBCreate(depthRB, pic2W, TWIDE/4*3-16, y+36,   "21bit",
	     infofg, infobg, hicol, locol);
    RBCreate(depthRB, pic2W, TWIDE/4*3-16, y+54,   "24bit",
	     infofg, infobg, hicol, locol);

    XMapSubwindows(theDisp, pic2W);
}


/***************************************************/
void PIC2Dialog(vis)
int vis;
{
    if (vis) {
	CenterMapWindow(pic2W, tbut[T_BOK].x + tbut[T_BOK].w/2,
			tbut[T_BOK].y + tbut[T_BOK].h/2, TWIDE, THIGH);
    }
    else     XUnmapWindow(theDisp, pic2W);
    pic2Up = vis;
}


/***************************************************/
int PIC2CheckEvent(xev)
XEvent *xev;
{
    /* check event to see if it's for one of our subwindows.  If it is,
       deal accordingly and return '1'.  Otherwise, return '0'. */

    int rv;
    rv = 1;

    if (!pic2Up)
	return (0);

    if (xev->type == Expose) {
	int x,y,w,h;
	XExposeEvent *e = (XExposeEvent *) xev;
	x = e->x;  y = e->y;  w = e->width;  h = e->height;

	if (e->window == pic2W)       drawTD(x, y, w, h);
	else rv = 0;
    }

    else if (xev->type == ButtonPress) {
	XButtonEvent *e = (XButtonEvent *) xev;
	int x,y;
	x = e->x;  y = e->y;

	if (e->button == Button1) {
	    if      (e->window == pic2W)     clickTD(x,y);
	    else rv = 0;
	}  /* button1 */
	else rv = 0;
    }  /* button press */


    else if (xev->type == KeyPress) {
	XKeyEvent *e = (XKeyEvent *) xev;
	char buf[128];  KeySym ks;  XComposeStatus status;
	int stlen;

	stlen = XLookupString(e,buf,128,&ks,&status);
	buf[stlen] = '\0';

	if (e->window == pic2W) {
	    if (stlen) {
		if (buf[0] == '\r' || buf[0] == '\n') { /* enter */
		    FakeButtonPress(&tbut[T_BOK]);
		}
		else if (buf[0] == '\033') {            /* ESC */
		    FakeButtonPress(&tbut[T_BCANC]);
		}
	    }
	}
	else rv = 0;
    }
    else rv = 0;

    if (rv == 0 && (xev->type == ButtonPress || xev->type == KeyPress)) {
	XBell(theDisp, 50);
	rv = 1;   /* eat it */
    }

    return (rv);
}


/***************************************************/
int PIC2SaveParams(fname, col)
char *fname;
int col;
{
    filename = fname;
    colorType = col;

    /* see if we can open the output file before proceeding */
    fp = pic2_OpenOutFile(filename, &append);
    if (!fp)
	return (-1);

    RBSetActive(typeRB,0,1);
    RBSetActive(typeRB,1,1);
    RBSetActive(typeRB,2,1);
    RBSetActive(typeRB,3,1);
    RBSelect(typeRB,0);


    if (append) {
	struct pic2_info pic2;

	pic2_init_info(&pic2);
	pic2.fp = fp;
	pic2_read_header(&pic2);

	RBSetActive(depthRB,0,0);
	RBSetActive(depthRB,1,0);
	RBSetActive(depthRB,2,0);
	RBSetActive(depthRB,3,0);
	RBSetActive(depthRB,4,0);
	RBSetActive(depthRB,5,0);
	RBSetActive(depthRB,6,0);
	RBSetActive(depthRB,7,0);

	switch (pic2.header->depth) {
	case  3:
	    RBSetActive(depthRB,0,1);
	    RBSelect(depthRB,0);
	    RBSetActive(typeRB,3,0);
	    break;
	case  6:
	    RBSetActive(depthRB,1,1);
	    RBSelect(depthRB,1);
	    RBSetActive(typeRB,3,0);
	    break;
	case  9:
	    RBSetActive(depthRB,2,1);
	    RBSelect(depthRB,2);
	    break;
	case 12:
	    RBSetActive(depthRB,3,1);
	    RBSelect(depthRB,3);
	    break;
	case 15:
	    RBSetActive(depthRB,4,1);
	    RBSelect(depthRB,4);
	    break;
	case 18:
	    RBSetActive(depthRB,5,1);
	    RBSelect(depthRB,5);
	    RBSetActive(typeRB,3,0);
	    break;
	case 21:
	    RBSetActive(depthRB,6,1);
	    RBSelect(depthRB,6);
	    RBSetActive(typeRB,3,0);
	    break;
	case 24:
	    RBSetActive(depthRB,7,1);
	    RBSelect(depthRB,7);
	    RBSetActive(typeRB,3,0);
	    break;
	default: {
	    char str[512];
	    sprintf(str, "unsupported PIC2 file '%s'.", filename);
	    ErrPopUp(str, "\nBummer");
	    CloseOutFile(fp, filename, 0);
	    fp = OpenOutFile(fname);
	    if (!fp)
		return (-1);
	    break;
	}
	}
	pic2_seek_file(&pic2, 0, SEEK_SET);
	pic2_cleanup_pic2_info(&pic2, 1);
    } else {
	RBSetActive(depthRB,0,1);
	RBSetActive(depthRB,1,1);
	RBSetActive(depthRB,2,1);
	RBSetActive(depthRB,3,1);
	RBSetActive(depthRB,4,1);
	RBSetActive(depthRB,5,1);
	RBSetActive(depthRB,6,1);
	RBSetActive(depthRB,7,1);
	RBSelect(depthRB,7);
	RBSetActive(typeRB,3,0);
    }
    return (0);
}


/***************************************************/
static void drawTD(x,y,w,h)
int x,y,w,h;
{
    char *title  = "Save PIC2 file...";
    int  i;
    XRectangle xr;

    xr.x = x;  xr.y = y;  xr.width = w;  xr.height = h;
    XSetClipRectangles(theDisp, theGC, 0,0, &xr, 1, Unsorted);

    XSetForeground(theDisp, theGC, infofg);
    XSetBackground(theDisp, theGC, infobg);

    for (i = 0; i < T_NBUTTS; i++)
	BTRedraw(&tbut[i]);

    ULineString(pic2W, typeRB->x-16, typeRB->y-3-DESCENT,   "FormatType");
    ULineString(pic2W, depthRB->x-16, depthRB->y-3-DESCENT, "ColorDepth");
    RBRedraw(typeRB, -1);
    RBRedraw(depthRB, -1);

    DrawString(pic2W, 20, 29, title);

    XSetClipMask(theDisp, theGC, None);
}

static void clickTD(x,y)
int x,y;
{
    int i;
    BUTT *bp;

    /* check BUTTs */

    /* check the RBUTTS first, since they don't DO anything */
    if ((i = RBClick(typeRB, x,y)) >= 0) {
	(void) RBTrack(typeRB, i);
	return;
    } else if ((i = RBClick(depthRB, x,y)) >= 0) {
	(void) RBTrack(depthRB, i);
	if ((2 <= i) && (i <= 4))
	    RBSetActive(typeRB,3,1);
	else {
	    RBSetActive(typeRB,3,0);
	    if (RBWhich(typeRB) == 3)
		RBSelect(typeRB,0);
	return;
	}
    }
    for (i = 0; i < T_NBUTTS; i++) {
	bp = &tbut[i];
	if (PTINRECT(x, y, bp->x, bp->y, bp->w, bp->h))
	    break;
    }
    if (i < T_NBUTTS)  /* found one */
	if (BTTrack(bp))
	    doCmd(i);
}



/***************************************************/
static void doCmd(cmd)
int cmd;
{
    switch (cmd) {
    case T_BOK: {
	char              *fullname;
	char               buf[64], *x_offsetp, *y_offsetp;
	static const char *labels[] = { "\nOk", "\033Cancel" };
        XEvent             event;
	int                i;

	strcpy(buf, "0,0");
	i = GetStrPopUp("Enter offset (x,y):", labels, 2, buf, 64,
			"01234567890,", 1);

	if (i)
	    return;
	if (strlen(buf)==0)
	    return;

	x_offsetp = buf;
	y_offsetp = index(buf, ',');
	if (!y_offsetp)
	    return;
	*(y_offsetp++) = '\0';
	if ((*x_offsetp == '\0') || (*y_offsetp == '\0'))
	    return;
	x_offset = atoi(x_offsetp);
	y_offset = atoi(y_offsetp);

        XNextEvent(theDisp, &event);
	HandleEvent(&event, &i);

	writePIC2();
	PIC2Dialog(0);

	fullname = GetDirFullName();
	if (!ISPIPE(fullname[0])) {
	    XVCreatedFile(fullname);
	    StickInCtrlList(0);
	}
    }
	break;
    case T_BCANC:
	pic2_KillNullFile(fp);
	PIC2Dialog(0);
	break;
    default:
	break;
    }
}


/*******************************************/
static void writePIC2()
{
    int   w, h, nc, rv, type, depth, ptype, pfree;
    byte *inpix, *rmap, *gmap, *bmap;


    WaitCursor();
    inpix = GenSavePic(&ptype, &w, &h, &pfree, &nc, &rmap, &gmap, &bmap);

    if (colorType == F_REDUCED)
	colorType = F_FULLCOLOR;

    switch (RBWhich(typeRB)) {
    case 0: type = P2SS;  break;
    case 1: type = P2SF;  break;
    case 2: type = P2BM;  break;
    case 3: type = P2BI;  break;
    default: type = P2SS; break;
    }
    switch (RBWhich(depthRB)) {
    case 0: depth =  3;  break;
    case 1: depth =  6;  break;
    case 2: depth =  9;  break;
    case 3: depth = 12;  break;
    case 4: depth = 15;  break;
    case 5: depth = 18;  break;
    case 6: depth = 21;  break;
    case 7: depth = 24;  break;
    default: depth = 24; break;
    }
    rv = WritePIC2(fp, inpix, ptype, w, h,
		   rmap, gmap, bmap, nc, colorType, filename,
		   type, depth, x_offset, y_offset, append, picComments);

    if (CloseOutFile(fp, filename, rv) == 0)
	DirBox(0);

    if (pfree)
	free(inpix);
}
#endif /* HAVE_PIC2 */
