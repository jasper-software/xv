/*
 * xvml.c  -  makes text item structure for multi-lingual textviewer.
 *
 * Entry Points:
 *  struct ml_text *ml_draw_text()
 *  struct context *ml_create_context()
 *  int ml_set_charsets()
 *  void get_monofont_size()
 */

#include "xv.h"
#include <X11/Xresource.h>

#ifdef TV_MULTILINGUAL	/* whole this file. */

#include "xvml.h"
#define HAVE_STRDUP 1
#define USE_MULE_EXTENSION

#ifndef __STDC__
#define CHAR char
#else
#define CHAR int
#endif

#define CODE_SI  0x0e	/* LS0 */
#define CODE_SO  0x0f	/* LS1 */
#define CODE_SS2 ((unsigned char) 0x8e)
#define CODE_SS3 ((unsigned char) 0x8f)

int ml_tab_width = 64;	/* dots */

struct charset {
    int bpc;		/* bytes per char */
    int noc;		/* number of chars */
    char designator;
    char *registry;
    int bit7;

    int loaded;
    char *fontname;

    XFontStruct *fs;
} charset[] = {
    { 1, 94, 'B', "iso8859-1",       0, 0, NULL, NULL},
    { 1, 96, 'A', "iso8859-1",       1, 0, NULL, NULL},
    { 1, 94, '0', "omron_udc_zh-0",  0, 0, NULL, NULL},
    { 1, 94, '2', "mulearabic-0",    0, 0, NULL, NULL},
    { 1, 94, '3', "mulearabic-1",    0, 0, NULL, NULL},
    { 1, 94, '4', "mulearabic-2",    0, 0, NULL, NULL},
    { 1, 94, 'J', "jisx0201.1976-0", 0, 0, NULL, NULL},
    { 1, 96, '0', "muleipa-1",       1, 0, NULL, NULL},
    { 1, 96, '1', "viscii1.1-1",     1, 0, NULL, NULL},
    { 1, 96, '2', "viscii1.1-1",     1, 0, NULL, NULL},
    { 1, 96, 'B', "iso8859-2",       1, 0, NULL, NULL},
    { 1, 96, 'C', "iso8859-3",       1, 0, NULL, NULL},
    { 1, 96, 'D', "iso8859-4",       1, 0, NULL, NULL},
    { 1, 96, 'T', "tis620.1986-0",   1, 0, NULL, NULL},
    { 1, 96, 'F', "iso8859-7",       1, 0, NULL, NULL},
    { 1, 96, 'G', "iso8859-6",       1, 0, NULL, NULL},
    { 1, 96, 'H', "iso8859-8",       1, 0, NULL, NULL},
    { 1, 94, 'I', "jisx0201.1976-0", 1, 0, NULL, NULL},
    { 1, 96, 'L', "iso8859-5",       1, 0, NULL, NULL},
    { 1, 96, 'M', "iso8859-9",       1, 0, NULL, NULL},
    { 2, 94, '2', "ethio-0",         0, 0, NULL, NULL},
    { 2, 94, '@', "jisx0208.1978",   0, 0, NULL, NULL},
    { 2, 94, 'A', "gb2312.1980-0",   0, 0, NULL, NULL},
    { 2, 94, 'B', "jisx0208.1983-0", 0, 0, NULL, NULL},
    { 2, 94, 'C', "ksc5601.1987-0",  0, 0, NULL, NULL},
    { 2, 94, 'D', "jisx0212.1990-0", 0, 0, NULL, NULL},
    { 2, 94, '0', "big5.eten-0",     0, 0, NULL, NULL},
    { 2, 94, '1', "big5.hku-0",      0, 0, NULL, NULL},
    /* End Mark */
    { 0,  0,   0,        NULL,       0, 0, NULL, NULL},
};
#define NR_CHARSETS ((int) (sizeof charset / sizeof charset[0]))

static struct charset *ascii = NULL;

struct context {
    struct charset *g[4];
    struct charset **gl, **gr;
    struct charset **ss;
    int eol;		/* 0: \n,  1: \r\n,  2: \r,  3: any */
    int valid[4];	/* g[i] is valid? */
    int short_form;	/* allow shortened designator sequence? */
    int lock_shift;	/* allow locking shift? */

    unsigned char *cbuf, *cbp;
    struct ml_text text;
    int line;
    int delta;
    int toolong;

    Display *dpy;
    Screen *scr;
    Window root_win;
};
#define DPY (context->dpy)
#define SCR (context->scr)
#define ROOT_WIN (context->root_win)

static unsigned char *escape_sequence		PARM((unsigned char *));
static unsigned char *designator_sequence	PARM((unsigned char *));
static void locking_shift			PARM((unsigned CHAR));
static void single_shift			PARM((unsigned CHAR));
static void put_unknown_char			PARM((unsigned CHAR));
static struct charset *search_charset		PARM((int, int, int));
static void pack_string				PARM((struct charset *,
						      unsigned char *, int));
static void init_xrm				PARM((void));
static void init_xrm_fonts			PARM((void));
static void init_xrm_tab			PARM((void));
#ifndef HAVE_STRDUP
static char *strdup				PARM((char *));
#endif

static char *default_fonts[] = {		/* default for xrm_fonts */
    "-sony-fixed-medium-r-normal--16-*-*-*-*-*-iso8859-1",
    "-jis-fixed-medium-r-normal--16-*-*-*-*-*-jisx0208.1983-0",
};
static int xrm_nfonts;
static char **xrm_fonts;

static struct context *context;	/* current context */

struct ml_text *ml_draw_text(ctx, string, len)
    struct context *ctx;
    char *string;
    int len;
{
    unsigned char *str = (unsigned char *) string;
    unsigned char *estr = str + len;

    context = ctx;

    if(ascii == NULL){
	fputs("ml_draw_text: call ml_set_charsets, first.\n", stderr);
	return NULL;
    }

    if(!str)
	return &context->text;

    WaitCursor();

    if (context->text.maxlines != 0) {
	struct ml_text *tp = &context->text;
	struct ml_line *lp;
	int i;
	for (i = tp->nlines, lp = tp->lines; i > 0; i--, lp++) {
	    if (lp->maxitems != 0)
		free((char *) lp->items);
	}
	free((char *) tp->lines);
	tp->maxlines = tp->nlines = 0;
    }
    if (context->cbuf != NULL)
	free((char *) context->cbuf);
    context->cbp = (unsigned char *) malloc((size_t) len * 8);/* all \xxx */
    context->cbuf = context->cbp;
    context->line = 0;
    context->delta = 0;
    context->ss = NULL;

    while(str < estr){
	if((*str & 0x80) == 0){				/* left half */
	    struct charset *cs = context->ss ? *context->ss : *context->gl;
	    unsigned char min_char, max_char;
	    if (cs != NULL) {
		if(cs->noc == 94){
		    min_char = 0x21;
		    max_char = 0x7e;
		}else{
		    min_char = 0x20;
		    max_char = 0x7f;
		}
	    }

	    if (cs == NULL)
		put_unknown_char(*str++);
	    else if(*str < min_char || *str > max_char){	/* C1 */
		switch(*str){
		case ' ':
		    {
			unsigned char *p = str + 1;
			while (*p == ' ' && p < estr)
			    p++;
			pack_string(ascii, str, (int) (p - str));
			str = p;
		    }
		    break;

		case '\t':
		    pack_string(ascii, str++, 0);
		    break;

		case '\n':
		    switch (context->eol) {
		    case 0:				/* unix type eol */
			pack_string(ascii, str, 0);
			WaitCursor();
			str++;
			break;
		    case 1:				/* dos type eol */
		    case 2:				/* mac type eol */
			put_unknown_char('\n');
			str++;
			break;
		    case 3:				/* any type eol */
			pack_string(ascii, str++, 0);
			while (*str == '\n' || *str == '\r')
			    str++;
			WaitCursor();
			break;
		    }
		    break;

		case '\r':
		    switch (context->eol) {
		    case 0:
			put_unknown_char('\r');
			str++;
			break;
		    case 1:
			str++;
			if (*str == '\n')
			    pack_string(ascii, str++, 0);
			else
			    put_unknown_char('\r');
			break;
		    case 2:
			pack_string(ascii, str, 0);
			WaitCursor();
			str++;
			break;
		    case 3:
			pack_string(ascii, str++, 0);
			while (*str == '\n' || *str == '\r')
			    str++;
			WaitCursor();
			break;
		    }
		    break;

		case '\033':
		    {
			unsigned char *p;
			str++;
			if((p = escape_sequence(str)) == str)
			    put_unknown_char('\033');
			else
			    str = p;
		    }
		    break;

		case CODE_SI:
		case CODE_SO:
		    if (!context->lock_shift)
			put_unknown_char((unsigned int) *str++);
		    else
			locking_shift((unsigned int) *str++);
		    break;

		default:
		    put_unknown_char((unsigned int) *str++);
		}
	    }else{					/* GL */
		if (context->ss != NULL) {
		    pack_string(cs, str, 1);
		    str += cs->bpc;
		    context->ss = NULL;
		} else {
		    int n;

		    if (cs->bpc == 1) {
			unsigned char *p = str;
			for (n = 0; p < estr; n++) {
			    if (*p < min_char || *p > max_char)
				break;
			    p++;
			}
			pack_string(cs, str, n);
			str = p;
		    } else {
			unsigned char *p = str;
			for (n = 0; p < estr - 1; n++) {
			    if (*p < min_char || *p > max_char ||
				    *(p + 1) < min_char || *(p + 1) > max_char)
				break;
			    p += 2;
			}
			if (n > 0)
			    pack_string(cs, str, n);
			else
			    put_unknown_char(*p++);
			str = p;
		    }
		}
	    }
	}else{						/* right half */
	    struct charset *cs = context->ss ? *context->ss : *context->gr;
	    unsigned char min_char, max_char;
	    if (cs != NULL) {
		if(cs->noc == 94){
		    min_char = 0xa1;
		    max_char = 0xfe;
		}else{
		    min_char = 0xa0;
		    max_char = 0xff;
		}
	    }

	    if (cs == NULL)
		put_unknown_char(*str++);
	    else if(*str < min_char || *str > max_char){	/* C2 */
		unsigned char c = *str++;
		switch(c){
		case CODE_SS2:
		case CODE_SS3:
		    single_shift((unsigned CHAR) c);
		    break;
		default:
		    put_unknown_char(c);
		}
	    }else{					/* GR */
		if (context->ss != NULL) {
		    pack_string(cs, str, 1);
		    str += cs->bpc;
		    context->ss = NULL;
		} else {
		    int n;

		    if (cs->bpc == 1) {
			unsigned char *p = str;
			for (n = 0; p < estr; n++) {
			    if (*p < min_char || *p > max_char)
				break;
			    p++;
			}
			pack_string(cs, str, n);
			str = p;
		    } else {
			unsigned char *p = str;
			for (n = 0; p < estr - 1; n++) {
			    if (*p < min_char || *p > max_char ||
				    *(p + 1) < min_char || *(p + 1) > max_char)
				break;
			    p += 2;
			}
			if (n > 0)
			    pack_string(cs, str, n);
			else
			    put_unknown_char(*p++);
			str = p;
		    }
		}
	    }
	}
    }

    {
	struct ml_text *tp = &context->text;
	struct ml_line *lp;
	int i;

	tp->width = 0;
	tp->height = 0;
	for (lp = tp->lines, i = tp->nlines; i > 0; lp++, i--) {
	    if (lp->nitems == 0) {
		lp->ascent  = ascii->fs->ascent;
		lp->descent = ascii->fs->descent;
	    }
	    if (tp->width < lp->width)
		tp->width = lp->width;
	    tp->height += lp->ascent + lp->descent;
	}
    }

    SetCursors(-1);
    return &context->text;
}

static unsigned char *escape_sequence(str)
    unsigned char *str;
{
    unsigned char *p;
    switch(*str){
    case '$':
    case '(': case ')': case '*': case '+':
    case '-': case '.': case '/': case ',':
	if((p = designator_sequence(str)) == NULL)
	    return str;
	return p;
    case 'n': case 'o': case '~': case '}': case '|':
	if (!context->lock_shift)
	    return str;
	locking_shift(*str);
	return str + 1;
    case 'N': case 'O':
	single_shift(*str);
	return str + 1;
    }
    return str;

}

static unsigned char *designator_sequence(str)
    unsigned char *str;
{
    unsigned char *p = str;
    int noc, bpc, n_g, shortened;
    unsigned char des;
    struct charset *cs;

    if(*p == '$'){
	bpc = 2;
	p++;
    }else
	bpc = 1;

    switch(*p++){
    case '(':	noc = 94; n_g = 0; des = *p++; shortened = 0; break;
    case ')':	noc = 94; n_g = 1; des = *p++; shortened = 0; break;
    case '*':	noc = 94; n_g = 2; des = *p++; shortened = 0; break;
    case '+':	noc = 94; n_g = 3; des = *p++; shortened = 0; break;
#ifdef USE_MULE_EXTENSION
    case ',':	noc = 96; n_g = 0; des = *p++; shortened = 0; break;
#endif
    case '-':	noc = 96; n_g = 1; des = *p++; shortened = 0; break;
    case '.':	noc = 96; n_g = 2; des = *p++; shortened = 0; break;
    case '/':	noc = 96; n_g = 3; des = *p++; shortened = 0; break;
    case '@':	noc = 94; n_g = 0; des = 'B';  shortened = 0; break;
    case 'A':	noc = 94; n_g = 0; des = 'A';  shortened = 1; break;
    case 'B':	noc = 94; n_g = 0; des = 'B';  shortened = 1; break;
    default:	return NULL;
    }
    if (!context->short_form && shortened)
	return NULL;

    if((cs = search_charset(bpc, noc, des)) == NULL){
	if(DEBUG){
	    fprintf(stderr, "designator_sequence: (%d,%d,%c) not found.\n",
		    bpc, noc, des);
	}
	return NULL;
    }
    if (!context->valid[n_g])
	return NULL;
    context->g[n_g] = cs;
    if(DEBUG){
	fprintf(stderr,
		"designator_sequence: G%d is `%s'.\n", n_g, cs->registry);
    }
    return p;
}

static void locking_shift(c)
    unsigned CHAR c;
{
    switch((unsigned char) c){
    case CODE_SI:	context->gl = &context->g[0]; break;
    case CODE_SO:	context->gl = &context->g[1]; break;
    case 'n':		context->gl = &context->g[2]; break;
    case 'o':		context->gl = &context->g[3]; break;
    case '~':		context->gr = &context->g[1]; break;
    case '}':		context->gr = &context->g[2]; break;
    case '|':		context->gr = &context->g[3]; break;
    }
    if(DEBUG){
	fprintf(stderr, "locking_shift: (%d,%d).\n",
		(int)(context->gl - context->g),
		(int)(context->gr - context->g));
    }
}

static void single_shift(c)
    unsigned CHAR c;
{
    switch((unsigned char) c){
    case CODE_SS2:	context->ss = &context->g[2]; break;
    case CODE_SS3:	context->ss = &context->g[3]; break;
    }
}


static void put_unknown_char(chr)
    unsigned CHAR chr;
{
    unsigned char c = chr;

    if(c < 0x20){
	unsigned char buf[2];
	buf[0] = '^';
	buf[1] = c + 0x40;
	pack_string(ascii, buf, 2);
    }else{
	unsigned char buf[4];
	buf[0] = '\\';
	buf[1] =  ((c >> 6) & 07) + '0';
	buf[2] =  ((c >> 3) & 07) + '0';
	buf[3] =  ((c     ) & 07) + '0';
	pack_string(ascii, buf, 4);
    }
}

struct context *ml_create_context(s)
    Screen *s;
{
    context = (struct context *) malloc(sizeof *context);

    context->g[0] = NULL;
    context->g[1] = NULL;
    context->g[2] = NULL;
    context->g[3] = NULL;
    context->gl = NULL;
    context->gr = NULL;
    context->ss = NULL;

    context->cbuf = NULL;
    context->text.maxlines = context->text.nlines = 0;
    context->line = 0;
    context->delta = 0;
    context->toolong = 0;

    DPY = DisplayOfScreen(s);
    SCR = s;
    ROOT_WIN = RootWindowOfScreen(s);

    return context;
}


int ml_set_charsets(ctx, sys)
    struct context *ctx;
    struct coding_system *sys;
{
    int retval = 0;
    int i;

    context = ctx;

    if(ascii == NULL){
	init_xrm();
	if((ascii = search_charset(1, 94, 'B')) == NULL){
	    fputs("ml_set_charsets: ascii charset not found.\n", stderr);
	    Quit(1);
	}
	if (ascii->fs == NULL) {
	    fputs("ml_set_charsets: iso8859-1 font not found.\n", stderr);
	    Quit(1);
	}
    }
    for(i = 0; i < 4; i++){
	switch(sys->design[i].bpc){
	case -1:	/* make G[i] invalid */
	    context->valid[i] = 0;
	    break;

	case 0:		/* don't change */
	    break;

	case 1: case 2:	/* change it */
	    if((context->g[i] = search_charset(sys->design[i].bpc,
					       sys->design[i].noc,
					       sys->design[i].des)) == NULL){
		fputs("ml_set_charsets: ", stderr);
		fprintf(stderr, "(%d,%d,%c) is specified as G%d, ",
			sys->design[i].bpc, sys->design[i].noc,
			sys->design[i].des, i);
		fputs("but not found. using `iso8859-1'.\n", stderr);
		context->g[i] = ascii;
		retval++;
	    }
	    context->valid[i] = 1;
	    break;

	default:	/* error */
	    fprintf(stderr,"ml_set_charsets: bad arguments of G%d. ", i);
	    fputs("using `iso8859-1'.\n", stderr);
	    context->g[i] = ascii;
	    retval++;
	}
    }
    if((unsigned int) sys->gl < 4)
	context->gl = &context->g[sys->gl];
    else{
	fprintf(stderr, "ml_set_charsets: bad number as GL. using G0.\n");
	context->gl = &context->g[0];
    }
    if((unsigned int) sys->gr < 4)
	context->gr = &context->g[sys->gr];
    else{
	fprintf(stderr, "ml_set_charsets: bad number as GR. using G0.\n");
	context->gr = &context->g[0];
    }
    context->eol = sys->eol;
    context->short_form = sys->short_form;
    context->lock_shift = sys->lock_shift;
    return retval;
}

static struct charset *search_charset(bpc, noc, des)
    int bpc, noc;
    int des;
{
    struct charset *cset;
    for(cset = charset; cset->bpc != 0; cset++){
	if(cset->bpc == bpc &&
	   cset->noc == noc &&
	   cset->designator == (char) des){
	    if(!cset->loaded){
#if 0
		int i, l;
		l = strlen(cset->registry);
		for (i = 0; i < xrm_nfonts; i++) {
		    int li = strlen(xrm_fonts[i]);
		    if (li > l) {
			if (xrm_fonts[i][li - l - 1] == '-' &&
				strcmp(xrm_fonts[i] + li - l,
				       cset->registry) == 0) {
			    if ((cset->fs = XLoadQueryFont(DPY, xrm_fonts[i]))
				    != NULL) {
				if (DEBUG) {
				    fprintf(stderr, "%s for %s\n",
					    xrm_fonts[i], cset->registry);
				}
				cset->fontname = xrm_fonts[i];
				break;
			    } else
				SetISTR(ISTR_WARNING,
					"%s: font not found.", xrm_fonts[i]);
			}
		    }
		}
#else
		int i, l;
		l = strlen(cset->registry);
		for (i = 0; i < xrm_nfonts && cset->fs == NULL; i++) {
		    int j, nfnts = 0;
		    char **fnts = XListFonts(DPY, xrm_fonts[i],
					     65535, &nfnts);
		    for (j = 0 ; j < nfnts; j++) {
			int ll = strlen(fnts[j]);
			if (*(fnts[j] + ll - l - 1) == '-' &&
			    strcmp(fnts[j] + ll - l, cset->registry)== 0) {
			    if ((cset->fs = XLoadQueryFont(DPY, fnts[j]))
				!= NULL) {
				if (DEBUG) {
				    fprintf(stderr, "%s for %s\n",
					    fnts[j], cset->registry);
				}
				cset->fontname = strdup(fnts[j]);
				break;
			    } else
				SetISTR(ISTR_WARNING,
					"%s: font not found", fnts[j]);
			}
		    }
		    if (fnts != NULL)
			XFreeFontNames(fnts);
		}
#endif
		if(cset->fs == NULL){
		    SetISTR(ISTR_WARNING,
			    "font for %s not found.\nusing ascii font.",
			    cset->registry);
		    if (ascii != NULL)
			cset->fs = ascii->fs;
		}

		cset->loaded = 1;
	    }
	    return cset;
	}
    }
    return NULL;
}

static void pack_string(cs, str, len)
    struct charset *cs;
    unsigned char *str;
    int len;	/* number of chars(not bytes) */
{
    struct ml_text *mt = &context->text;
    struct ml_line *lp;
    XTextItem16 *ip;

    if (context->line == mt->maxlines) {
	int oldmax = mt->maxlines;
	if (mt->maxlines < 1)
	    mt->maxlines = 1;
	else
	    mt->maxlines = 2 * mt->maxlines;
	if (oldmax == 0)
	    mt->lines = (struct ml_line *)
				malloc(sizeof(struct ml_line) * mt->maxlines);
	else {
	    mt->lines = (struct ml_line *)
				realloc(mt->lines,
					sizeof(struct ml_line) * mt->maxlines);
	}
    }
    lp = &mt->lines[context->line];
    if (mt->nlines == context->line) {
	mt->nlines++;
	lp->maxitems = 0;
	lp->nitems = 0;
	lp->width = 0;
	lp->ascent = lp->descent = 0;
    }

    if (len == 0) {
	switch (*str) {
	case '\n':
	    context->line++;
	    context->delta = 0;
	    context->toolong = 0;
	    break;
	case '\t':
	    {
		int nx, x = lp->width + context->delta;
		nx = (x + ml_tab_width) / ml_tab_width * ml_tab_width;
		context->delta += nx - x;
	    }
	    break;
	}
	return;
    }

    if (context->toolong)
	return;
    if (lp->width > 30000) {
	context->toolong = 1;
	cs = ascii;
	str = (unsigned char *) "...";
	len = 3;
    }

    if (lp->nitems == lp->maxitems) {
	int oldmax = lp->maxitems;
	if (lp->maxitems < 1)
	    lp->maxitems = 1;
	else
	    lp->maxitems = 2 * lp->maxitems;
	if (oldmax == 0)
	    lp->items = (XTextItem16 *)
				malloc(sizeof(XTextItem16) * lp->maxitems);
	else
	    lp->items = (XTextItem16 *)
				realloc(lp->items,
					sizeof(XTextItem16) * lp->maxitems);
    }
    ip = &lp->items[lp->nitems++];
    ip->chars = (XChar2b *) context->cbp;
    ip->nchars = len;
    ip->delta = context->delta;
    ip->font = cs->fs->fid;
    context->cbp += 2 * len;
    context->delta = 0;

    if (cs->bpc == 1) {
	XChar2b *p;
	unsigned char b7 = cs->bit7 ? 0x80 : 0;
	int i;
	for (i = len, p = ip->chars; i > 0; i--, p++) {
	    p->byte1 = '\0';
	    p->byte2 = (*str++ & 0x7f) | b7;
	}
    } else {
	XChar2b *p;
	unsigned char b7 = cs->bit7 ? 0x80 : 0;
	int i;
	for (i = len, p = ip->chars; i > 0; i--, p++) {
	    p->byte1 = (*str++ & 0x7f) | b7;
	    p->byte2 = (*str++ & 0x7f) | b7;
	}
    }

    lp->width += XTextWidth16(cs->fs, ip->chars, ip->nchars);
    if (lp->ascent < cs->fs->ascent)
	lp->ascent = cs->fs->ascent;
    if (lp->descent < cs->fs->descent)
	lp->descent = cs->fs->descent;
}

void get_monofont_size(wide, high)
    int *wide, *high;
{
    if (ascii == NULL) {
	fputs("ml_draw_text: call ml_set_charsets, first.\n", stderr);
	return;
    }
    *wide = ascii->fs->max_bounds.width;
    *high = ascii->fs->ascent + ascii->fs->descent;
}

static void init_xrm()
{
    init_xrm_fonts();
    init_xrm_tab();
}

static void init_xrm_fonts()
{
    char *p, *fns = XGetDefault(theDisp, "xv", "fontSet");
    int n;
    if (fns == NULL) {
	xrm_fonts = default_fonts;
	xrm_nfonts = sizeof default_fonts / sizeof *default_fonts;
	return;
    }
    while(*fns == ' ' || *fns == '\t')
	fns++;
    if (*fns == '\0') {
	xrm_fonts = default_fonts;
	xrm_nfonts = sizeof default_fonts / sizeof *default_fonts;
	return;
    }
    fns = strdup(fns);

    n = 1;
    for (p = fns; *p != '\0'; p++) {
	if (*p == ',')
	    n++;
    }
    xrm_nfonts = n;
    xrm_fonts = (char **) malloc(sizeof (char *) * xrm_nfonts);
    for (n = 0, p = fns; n < xrm_nfonts && *p != '\0'; ) {
	while (*p == ' ' || *p == '\t')
	    p++;
	xrm_fonts[n++] = p;
	while (1) {
	    char *q;
	    while (*p != ' ' && *p != '\t' && *p != ',' && *p != '\0')
		p++;
	    q = p;
	    while (*q == ' ' || *q == '\t')
		q++;
	    if (*q == ',' || *q == '\0') {
		*p = '\0';
		p = q + 1;
		break;
	    } else
		p = q;
	}
    }
    for ( ; n < xrm_nfonts; n++)
	xrm_fonts[n] = "";
}

static void init_xrm_tab()
{
    char *ts = XGetDefault(theDisp, "xv", "tabWidth");
    unsigned short tab;
    if (ts == NULL)
	tab = 64;
    else {
	char *ep;
	long t;
	int bad = 0;
	t = strtol(ts, &ep, 0);
	tab = (unsigned short) t;
	if (ep != NULL) {
	    while (*ep == ' ' && *ep == '\t')
		ep++;
	    if (*ep != '\0')
		bad = 1;
	}
	if (tab != (long) (unsigned long) t)
	    bad = 1;
	if (bad) {
	    SetISTR(ISTR_WARNING, "bad tab width.");
	    tab = 64;
	}
    }
    ml_tab_width = tab;
}


#ifndef HAVE_STRDUP
static char *strdup(str)
    char *str;
{
    return strcpy(malloc(strlen(str) + 1), str);
}
#endif

char *lookup_registry(d, b7)
    struct design d;
    int *b7;
{
    int i;
    for (i = 0; i < NR_CHARSETS; i++) {
	if (charset[i].bpc == d.bpc && charset[i].noc == d.noc &&
	    charset[i].designator == d.des) {
	    *b7 = charset[i].bit7;
	    return charset[i].registry;
	}
    }
    return NULL;
}

struct design lookup_design(registry, b7)
    char *registry;
    int b7;
{
    struct design d;
    int i;
    d.bpc = 0;
    d.noc = 0;
    d.des = '\0';
    for (i = 0; i < NR_CHARSETS; i++) {
	if (strcmp(charset[i].registry, registry) == 0 &&
		charset[i].bit7 == b7) {
	    d.bpc = charset[i].bpc;
	    d.noc = charset[i].noc;
	    d.des = charset[i].designator;
	    break;
	}
    }
    return d;
}

char *sjis_to_jis(orig, len, newlen)
    char *orig;
    int len, *newlen;
{
    unsigned char *new;
    unsigned char *p, *q, *endp;
    if (len == 0) {
	*newlen = 0;
	return (char *) malloc((size_t) 1);
    }
    new = (unsigned char *) malloc((size_t) len * 4);	/* big enough */
    for (p = (unsigned char *) orig, endp = p + len, q = new; p < endp; ) {
	if ((*p & 0x80) == 0)			/* 1 byte char */
	    *q++ = *p++;
	else if (*p >= 0x81 && *p <= 0x9f) {	/* kanji 1st byte */
	    unsigned char c1 = *p++;
	    unsigned char c2 = *p++;
	    if (c2 < 0x40 || c2 > 0xfc) {		/* bad 2nd byte */
		*q++ = CODE_SS2;
		*q++ = c1;
		*q++ = CODE_SS2;
		*q++ = c2;
	    } else {					/* right 2nd byte */
		if (c2 <= 0x9e) {
		    if (c2 > 0x7f)
			c2--;
		    c1 = (c1 - 0x81) * 2 + 1 + 0xa0;
		    c2 = (c2 - 0x40)     + 1 + 0xa0;
		} else {
		    c1 = (c1 - 0x81) * 2 + 2 + 0xa0;
		    c2 = (c2 - 0x9f)     + 1 + 0xa0;
		}
		*q++ = c1;
		*q++ = c2;
	    }
	} else if (*p >= 0xe0 && *p <= 0xef) {	/* kanji 1st byte */
	    unsigned char c1 = *p++;
	    unsigned char c2 = *p++;
	    if (c2 < 0x40 || c2 > 0xfc) {		/* bad 2nd byte */
		*q++ = CODE_SS2;
		*q++ = c1;
		*q++ = CODE_SS2;
		*q++ = c2;
	    } else {					/* right 2nd byte */
		if (c2 <= 0x9e) {
		    c1 = (c1 - 0xe0) * 2 + 63 + 0xa0;
		    c2 = (c2 - 0x40)     +  1 + 0xa0;
		} else {
		    c1 = (c1 - 0xe0) * 2 + 64 + 0xa0;
		    c2 = (c2 - 0x9f)     +  1 + 0xa0;
		}
		*q++ = c1;
		*q++ = c2;
	    }
	} else {				/* katakana or something */
	    *q++ = CODE_SS2;
	    *q++ = *p++;
	}
    }
    *newlen = q - new;

    return (char *) realloc(new, (size_t) *newlen);
}

#endif /* TV_MULTILINGUAL */
