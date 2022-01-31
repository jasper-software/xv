/*
 * xvpi.c - load routine for `Pi' format pictures.
 *
 * The `Pi' format is made by Yanagisawa.
 * It is common among many Japanese personal computer users.
 *
 */

#include "xv.h"
#include <setjmp.h>

#ifdef HAVE_PI

typedef unsigned short data16;
typedef unsigned int data32;

struct pi_info {
    jmp_buf jmp;
    FILE *fp;
    struct {
	int rest;
	byte cur;
    }bs;
    long fsize;
    byte mode;
    int width, height;
    float aspect;
    int cbits;
    int numcols;
    byte *cmap;
    struct ct_t{
	struct elt_t *top;
	struct elt_t{
	    struct elt_t *old, *recent;
	    byte val;
	} *elt;
    }*ct;
    int defcmap;
    int writing_grey;
};

static void pi_open_file         PARM((struct pi_info*, char*));
static void pi_read_header       PARM((struct pi_info*, char**));
static void pi_check_id          PARM((struct pi_info*));
static void pi_read_comment      PARM((struct pi_info*, char**));
static void pi_read_palette      PARM((struct pi_info*));
static void pi_expand            PARM((struct pi_info*, byte**));
static byte pi_read_color        PARM((struct pi_info*, int));
static int pi_read_position      PARM((struct pi_info*));
static data32 pi_read_length     PARM((struct pi_info*));
static int pi_copy_pixels        PARM((struct pi_info*,
				     byte*, int, int, data32));

static void pi_write_header      PARM((struct pi_info*,
				       char*, byte*, byte*, byte*));
static void pi_write_id          PARM((struct pi_info*));
static void pi_write_comment     PARM((struct pi_info*, char*));
static void pi_write_palette     PARM((struct pi_info*, byte*, byte*, byte*));
static void pi_compress          PARM((struct pi_info*, byte*));
static void pi_write_gabage      PARM((struct pi_info*));
static void pi_write_color       PARM((struct pi_info*, int, int));
static int pi_test_matching      PARM((struct pi_info*,
				       byte*, int, int, data32*));
static void pi_write_position    PARM((struct pi_info*, int));
static void pi_write_length      PARM((struct pi_info*, data32));

static void pi_table_create      PARM((struct pi_info*));
static byte pi_table_get_value   PARM((struct pi_info*, int, int));
static int pi_table_lookup_value PARM((struct pi_info*, int, int));
static data32 pi_read_bits       PARM((struct pi_info*, int));
static void pi_write_bits        PARM((struct pi_info*, data32, int));
static void pi_init_pi_info      PARM((struct pi_info*));
static void pi_cleanup_pi_info   PARM((struct pi_info*, int));
static void pi_cleanup_pinfo     PARM((PICINFO*));
static void pi_memory_error      PARM((char*, char*));
static void pi_error             PARM((struct pi_info*, int));
static void pi_file_error        PARM((struct pi_info*, int));
static void pi_file_warning      PARM((struct pi_info*, int));
static void pi_show_pi_info      PARM((struct pi_info*));
static void *pi_malloc           PARM((size_t, char*));
static void *pi_realloc          PARM((void*, size_t, char*));


static char *pi_id = "Pi";
static char *pi_msgs[] = {
    NULL,
#define PI_OPEN 1
    "couldn't open.",
#define PI_CORRUPT 2
    "file corrupted.",
#define PI_FORMAT 3
    "not PI format.",
#define PI_PLANES 4
    "bad number of planes.",
#define PI_WRITE 5
    "write failed.",
};


/* The main routine of `Pi' loader. */
int LoadPi(fname, pinfo)
    char *fname;
    PICINFO *pinfo;
{
    struct pi_info pi;
    int e;
    int i;
    if(DEBUG) fputs("LoadPi:\n", stderr);

    pinfo->comment = NULL;
    pi_init_pi_info(&pi);
    if((e = setjmp(pi.jmp)) != 0){
	/* When an error occurs, comes here. */
	pi_cleanup_pi_info(&pi, 0);
	pi_cleanup_pinfo(pinfo);
	if(DEBUG) fputs("\n", stderr);
	return 0;
    }

    pi_open_file(&pi, fname);
    pi_read_header(&pi, &pinfo->comment);
    pi_expand(&pi, &pinfo->pic);

    pinfo->normw = pinfo->w = pi.width;
    pinfo->normh = pinfo->h = pi.height;
    pinfo->type = PIC8;
    if(pi.numcols > 256)	/* shouldn't happen. */
	pi.numcols = 256;
    for(i = 0; i < pi.numcols; i++){
	pinfo->r[i] = pi.cmap[i * 3    ];
	pinfo->g[i] = pi.cmap[i * 3 + 1];
	pinfo->b[i] = pi.cmap[i * 3 + 2];
    }
    pinfo->frmType = F_PI;
    pinfo->colType = F_FULLCOLOR;
    sprintf(pinfo->fullInfo, "Pi, %d colors (%ld bytes)",
	    pi.numcols, pi.fsize);
    sprintf(pinfo->shrtInfo, "%dx%d Pi.", pi.width, pi.height);
    normaspect = pi.aspect;

    pi_cleanup_pi_info(&pi, 0);
    if(DEBUG) fputs("\n", stderr);
    return 1;
}

static void pi_open_file(pi, fname)
    struct pi_info *pi;
    char *fname;
{
    if((pi->fp = fopen(fname, "rb")) == NULL)
	pi_file_error(pi, PI_OPEN);
    fseek(pi->fp, (size_t) 0, SEEK_END);
    pi->fsize = ftell(pi->fp);
    fseek(pi->fp, (size_t) 0, SEEK_SET);
}

static void pi_read_header(pi, comm)
    struct pi_info *pi;
    char **comm;
{
    byte buf[10];
    int mda;
    int i;

    pi_check_id(pi);
    pi_read_comment(pi, comm);

    if(fread(buf, (size_t) 10, (size_t) 1, pi->fp) != 1)
	pi_file_error(pi, PI_CORRUPT);

    pi->mode = buf[0];
    pi->defcmap = pi->mode & 0x80;
    if(buf[1] != 0 && buf[2] != 0)
	pi->aspect = (float) buf[2] / (int) buf[1];
    pi->cbits = buf[3];
    pi->numcols = 1 << pi->cbits;

    if(pi->cbits != 4 && pi->cbits != 8)
	pi_error(pi, PI_PLANES);

    mda = (int) buf[8] << 8 | (int) buf[9];
    for(i = 0; i < mda; i++){
	if(fgetc(pi->fp) == EOF)
	    pi_file_error(pi, PI_CORRUPT);
    }

    if(fread(buf, (size_t) 4, (size_t) 1, pi->fp) != 1)
	pi_file_error(pi, PI_CORRUPT);
    pi->width  = (int) buf[0] << 8 | (int) buf[1];
    pi->height = (int) buf[2] << 8 | (int) buf[3];

    pi_read_palette(pi);

    if(DEBUG) pi_show_pi_info(pi);
}

static void pi_check_id(pi)
    struct pi_info *pi;
{
    char buf[2];

    if(fread(buf, (size_t) 2, (size_t) 1, pi->fp) != 1)
	pi_file_error(pi, PI_CORRUPT);
    if(strncmp(buf, pi_id, (size_t) 2) != 0)
	pi_error(pi, PI_FORMAT);
}

static void pi_read_comment(pi, comm)
    struct pi_info *pi;
    char **comm;
{
/*
 * The comment format is like:
 *   comment string `^Z' dummy string `\0'
 */
    int max = -1, i = 0;
    int c;

    while(1){
	if((c = fgetc(pi->fp)) == EOF)
	    pi_file_error(pi, PI_CORRUPT);
	if(c == '\032')		/* 0x1a, '^Z' */
	    break;
	if(max < i){
	    max += 32;
	    *comm = pi_realloc(*comm, (size_t) max + 1, "pi_read_comment(1)");
	}
	(*comm)[i++] = c;
    }
    if(max < i){
	max++;
	*comm = pi_realloc(*comm, (size_t) max + 1, "pi_read_comment(2)");
    }
    (*comm)[i] = '\0';

    while((c = fgetc(pi->fp)) != '\0'){		/* skip the dummy area */
	if(c == EOF)
	    pi_file_error(pi, PI_CORRUPT);
    }
}

static void pi_read_palette(pi)
    struct pi_info *pi;
{
    pi->cmap = pi_malloc((size_t) pi->numcols * 3, "pi_read_palette");
    if(pi->mode & 0x80){
	if(pi->numcols == 16){
	    int i;
	    byte on;

	    on = 0x77;
	    for(i = 0; i < 8; i++){
		pi->cmap[i * 3    ] = i & 2 ? on : 0;
		pi->cmap[i * 3 + 1] = i & 4 ? on : 0;
		pi->cmap[i * 3 + 2] = i & 1 ? on : 0;
	    }
	    on = 0xff;
	    for(; i < 16; i++){
		pi->cmap[i * 3    ] = i & 2 ? on : 0;
		pi->cmap[i * 3 + 1] = i & 4 ? on : 0;
		pi->cmap[i * 3 + 2] = i & 1 ? on : 0;
	    }
	}else{	/* pi->numcols == 256 */
	    int i;
	    byte r, g, b;
	    r = g = b = 0;
	    for(i = 0; i < 256; i++){
		pi->cmap[i * 3    ] = r;
		pi->cmap[i * 3 + 1] = g;
		pi->cmap[i * 3 + 2] = b;
		if((b += 0x40) == 0){
		    if((r += 0x20) == 0)
			g += 0x20;
		}
	    }
	}
    }else{
	if(fread(pi->cmap, (size_t) pi->numcols * 3, (size_t) 1, pi->fp) != 1)
	    pi_file_error(pi, PI_CORRUPT);
    }
}

/* The main routine to expand `Pi' file. */
static void pi_expand(pi, pic)
    struct pi_info *pi;
    byte **pic;
{
    byte prev_col = 0;
    int prev_pos = -1;
    int cnt = 0, max_cnt = pi->width * pi->height;

    *pic = pi_malloc((size_t) max_cnt, "pi_expand");   // GRR POSSIBLE OVERFLOW / FIXME

    pi_table_create(pi);

    if(pi->width > 2){
	(*pic)[0] = pi_read_color(pi, 0);
	(*pic)[1] = pi_read_color(pi, (*pic)[0]);

	while(cnt < max_cnt){
	    int pos = pi_read_position(pi);
	    if(pos != prev_pos){
		data32 len = pi_read_length(pi);
		cnt = pi_copy_pixels(pi, *pic, cnt, pos, len);
		prev_col = (*pic)[cnt - 1];
		prev_pos = pos;
	    }else{
		do{
		    prev_col = pi_read_color(pi, (int) prev_col);
		    (*pic)[cnt++] = prev_col;
		    prev_col = pi_read_color(pi, (int) prev_col);
		    (*pic)[cnt++] = prev_col;
		}while(pi_read_bits(pi, 1) == 1);

		prev_pos = -1;
	    }
	}
    }else{
	while(cnt < max_cnt){
	    prev_col = pi_read_color(pi, (int) prev_col);
	    (*pic)[cnt++] = prev_col;
	}
    }
}

static byte pi_read_color(pi, prev)
    struct pi_info *pi;
    int prev;
{
    byte n;
    if(pi->cbits == 4){
	if(pi_read_bits(pi, 1) == 1)
	    n = pi_read_bits(pi, 1);			/* 1x */
	else{
	    if(pi_read_bits(pi, 1) == 0)
		n = pi_read_bits(pi, 1) + 2;		/* 00x */
	    else{
		if(pi_read_bits(pi, 1) == 0)
		    n = pi_read_bits(pi, 2) + 4;	/* 010xx */
		else
		    n = pi_read_bits(pi, 3) + 8;	/* 011xxx */
	    }
	}
    }else{	/* cbits == 8 */
	if(pi_read_bits(pi, 1) == 1)
	    n = pi_read_bits(pi, 1);
	else{
	    int bits = 0;
	    byte base = 2;
	    while(bits < 6){
		if(pi_read_bits(pi, 1) == 0)
		    break;
		bits++;
		base <<= 1;
	    }
	    n = pi_read_bits(pi, bits + 1) + base;
	}
    }

    return pi_table_get_value(pi, prev, (int) n);
}

static int pi_read_position(pi)
    struct pi_info *pi;
{
    byte r;
    if((r = pi_read_bits(pi, 2)) != 3)
	return (int) r;
    else
	return (int) pi_read_bits(pi, 1) + 3;
}

static data32 pi_read_length(pi)
    struct pi_info *pi;
{
    data32 r = 1;
    int bits = 0;
    while(pi_read_bits(pi, 1) == 1){
	r <<= 1;
	bits++;
    }
    if(bits > 0)
	return r + pi_read_bits(pi, bits);
    return 1;
}

static int pi_copy_pixels(pi, pic, cnt, pos, len)
    struct pi_info *pi;
    byte *pic;
    int cnt, pos;
    data32 len;
{
    int s = 0, d = cnt;
    int max = pi->width * pi->height;
    switch(pos){
    case 0:
	if(cnt < 2){
	    if(pic[0] == pic[1])
		s = cnt - 2;
	    else
		s = cnt - 4;
	}else{
	    if(pic[cnt - 2] == pic[cnt - 1])
		s = cnt - 2;
	    else
		s = cnt - 4;
	}
	break;
    case 1:
	s = cnt - pi->width;
	break;
    case 2:
	s = cnt - pi->width * 2;
	break;
    case 3:
	s = cnt - pi->width + 1;
	break;
    case 4:
	s = cnt - pi->width - 1;
    }

    len *= 2;
    while(s < 0 && len != 0 && d < max){
	pic[d++] = pic[-(s++) % 2];
	len--;
    }
    while(len != 0 && d < max){
	pic[d++] = pic[s++];
	len--;
    }
    return d;
}

/* The main routine of `Pi' saver. */
int WritePi(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle,
	    comment)
    FILE *fp;
    byte *pic;
    int ptype, w, h;
    byte *rmap, *gmap, *bmap;
    int numcols, colorstyle;
    char *comment;
{
    byte rtemp[256], gtemp[256], btemp[256];
    struct pi_info pi;
    int e;

    if(DEBUG) fputs("WritePi\n", stderr);
    pi_init_pi_info(&pi);
    pi.fp = fp;
    pi.width  = w;
    pi.height = h;
    pi.writing_grey = (colorstyle == F_GREYSCALE);
    if(ptype == PIC24){
	if(!(pic = Conv24to8(pic, w, h, 256, rtemp, gtemp, btemp)))
	    pi_memory_error("Conv24to8", "WritePi");
	rmap = rtemp;
	gmap = gtemp;
	bmap = btemp;
	numcols = 256;
    }

    if((e = setjmp(pi.jmp)) != 0){
	/* When an error occurs, comes here. */
	pi_cleanup_pi_info(&pi, 1);
	if(DEBUG) fputs("\n", stderr);
	return -1;
    }

    pi.numcols = numcols;
    pi_write_header(&pi, comment,  rmap, gmap, bmap);
    pi_compress(&pi, pic);
    pi_write_gabage(&pi);

    pi_cleanup_pi_info(&pi, 1);
    if(DEBUG) fputs("\n", stderr);
    return 0;
}

static void pi_write_header(pi, comm, r, g, b)
    struct pi_info *pi;
    char *comm;
    byte *r, *g, *b;
{
    byte buf[14];

    if(DEBUG) pi_show_pi_info(pi);

    pi_write_id(pi);
    pi_write_comment(pi, comm);

    buf[0] = buf[1] = buf[2] = 0;
    buf[3] = pi->cbits = pi->numcols > 16 ? 8 : 4;
    buf[4] = 'X';
    buf[5] = 'V';
    buf[6] = ' ';
    buf[7] = ' ';
    buf[8] = buf[9] = 0;
    buf[10] = pi->width >> 8;
    buf[11] = pi->width;
    buf[12] = pi->height >> 8;
    buf[13] = pi->height;
    if(fwrite(buf, (size_t) 14, (size_t) 1, pi->fp) != 1)
	pi_file_error(pi, PI_WRITE);

    pi_write_palette(pi, r, g, b);
}

static void pi_write_id(pi)
    struct pi_info *pi;
{
    if(fwrite(pi_id, (size_t) 2, (size_t) 1, pi->fp) != 1)
	pi_file_error(pi, PI_WRITE);
}

static void pi_write_comment(pi, comm)
    struct pi_info *pi;
    char *comm;
{
    if(comm){
	int i;
	for(i = 0; comm[i]; i++){
	    if(comm[i] == '\032')	/* 0x1a, '^Z' */
		comm[i] = ' ';
	}
	if(i > 0){
	    if(fwrite(comm, (size_t) i, (size_t) 1, pi->fp) != 1)
		pi_file_error(pi, PI_WRITE);
	}
    }

    if(fwrite("\032\0", (size_t) 2, (size_t) 1, pi->fp) != 1)
	pi_file_error(pi, PI_WRITE);
}

static void pi_write_palette(pi, r, g, b)
    struct pi_info *pi;
    byte *r, *g, *b;
{
    int i;
    int pinum = 1 << pi->cbits;
    char buf[3];

    for(i = 0; i < pi->numcols; i++){
	buf[0] = *r++;
	buf[1] = *g++;
	buf[2] = *b++;
	if(pi->writing_grey)
	    buf[0] = buf[1] = buf[2] = MONO(buf[0], buf[1], buf[2]);
	if(fwrite(buf, (size_t) 3, (size_t) 1, pi->fp) != 1)
	    pi_file_error(pi, PI_WRITE);
    }
    for( ; i < pinum; i++){
	if(fwrite(buf, (size_t) 3, (size_t) 1, pi->fp) != 1)
	    pi_file_error(pi, PI_WRITE);
    }
    pi->numcols = pinum;
}

/* The main routine to compress `Pi' format. */
static void pi_compress(pi, pic)
    struct pi_info *pi;
    byte *pic;
{
    byte prev_col = 0;
    int prev_pos = -1;
    int cnt = 0, max_cnt = pi->width * pi->height;
    pi_table_create(pi);

    if(pi->width > 2){
	int pos;
	data32 len;

	pi_write_color(pi, 0,      pic[0]);
	pi_write_color(pi, pic[0], pic[1]);
	pos = pi_test_matching(pi, pic, prev_pos, cnt, &len);
	while(cnt < max_cnt){
	    if(pos >= 0){
		pi_write_position(pi, pos);
		pi_write_length(pi, len);
		if((cnt += len * 2) >= max_cnt)
		    break;
		prev_col = pic[cnt - 1];
		prev_pos = pos;
		pos = pi_test_matching(pi, pic, prev_pos, cnt, &len);
	    }else{
		pi_write_position(pi, prev_pos);
		prev_pos = -1;
		while(pos < 0){
		    pi_write_color(pi, (int) prev_col, pic[cnt]);
		    prev_col = pic[cnt];
		    if(++cnt >= max_cnt)
			break;
		    pi_write_color(pi, (int) prev_col, pic[cnt]);
		    prev_col = pic[cnt];
		    if(++cnt >= max_cnt)
			break;
		    pos = pi_test_matching(pi, pic, -1, cnt, &len);
		    if(pos < 0)
			pi_write_bits(pi, 1, 1);
		    else
			pi_write_bits(pi, 0, 1);
		}
	    }
	}
    }else{
	while(cnt < max_cnt){
	    pi_write_color(pi, (int) prev_col, pic[cnt]);
	    prev_col = pic[cnt++];
	}
    }
}

static void pi_write_gabage(pi)
    struct pi_info *pi;
{
    pi_write_bits(pi, 0, 32);
}

static void pi_write_color(pi, prev, col)
    struct pi_info *pi;
    int prev, col;
{
    int n = pi_table_lookup_value(pi, prev, col);

    if(pi->cbits == 4){
	if(n < 2)
	    pi_write_bits(pi, (data32) n | 2, 2);
	else if(n < 4)
	    pi_write_bits(pi, (data32) n - 2, 3);
	else if(n < 8)
	    pi_write_bits(pi, (data32) (n - 4) | 8, 5);
	else
	    pi_write_bits(pi, (data32) (n - 8) | 24, 6);
    }else{	/* cbits == 8 */
	if(n < 2){
	    pi_write_bits(pi, (data32) n | 2, 2);
	}else{
	    int bits = 0;
	    byte base = 2;
	    while(bits < 6){
		if(n < (int) base * 2)
		    break;
		bits++;
		base <<= 1;
	    }
	    pi_write_bits(pi, 0, 1);
	    if(bits > 0)
		pi_write_bits(pi, 0xffffffff, bits);
	    if(bits < 6)
		pi_write_bits(pi, 0, 1);
	    pi_write_bits(pi, (data32) n - base, bits + 1);
	}
    }
}

static int pi_test_matching(pi, pic, prev, cnt, len)
    struct pi_info *pi;
    byte *pic;
    int prev, cnt;
    data32 *len;
{
    data32 lens[5];
    int pos, p;
    int s, d = 0;
    int max = pi->width * pi->height;

    for(pos = 0; pos < 5; pos++){
	switch(pos){
	case 0:
	    if(cnt < 2){
		if(pic[0] == pic[1])
		    d = cnt - 2;
		else
		    d = cnt - 4;
	    }else{
		if(pic[cnt - 2] == pic[cnt - 1])
		    d = cnt - 2;
		else
		    d = cnt - 4;
	    }
	    break;
	case 1:
	    d = cnt - pi->width;
	    break;
	case 2:
	    d = cnt - pi->width * 2;
	    break;
	case 3:
	    d = cnt - pi->width + 1;
	    break;
	case 4:
	    d = cnt - pi->width - 1;
	}
	s = cnt;
	lens[pos] = 0;

	if(prev == 0 && pos == 0)
	    continue;

	while(d < max){
	    if(pic[(d < 0) ? (-d) % 2 : d] != pic[s])
		break;
	    lens[pos]++;
	    d++;
	    s++;
	}

    }

    for(pos = 0, p = 1; p < 5; p++){
	if(lens[p] >= lens[pos])
	    pos = p;
    }

    if(lens[pos] / 2 == 0)
	return -1;
    *len = lens[pos] / 2;
    return pos;
}

static void pi_write_position(pi, pos)
    struct pi_info *pi;
    int pos;
{
    switch(pos){
    case 0:
	pi_write_bits(pi, 0, 2);
	break;
    case 1:
	pi_write_bits(pi, 1, 2);
	break;
    case 2:
	pi_write_bits(pi, 2, 2);
	break;
    case 3:
	pi_write_bits(pi, 6, 3);
	break;
    case 4:
	pi_write_bits(pi, 7, 3);
	break;
    }
}

static void pi_write_length(pi, len)
    struct pi_info *pi;
    data32 len;
{
    int bits = 0;
    data32 base = 1;

    while(len >= base * 2){
	bits++;
	base <<= 1;
    }
    if(bits > 0){
	pi_write_bits(pi, 0xffffffff, bits);
	pi_write_bits(pi, 0, 1);
	pi_write_bits(pi, len - base, bits);
    }else
	pi_write_bits(pi, 0, 1);
}

/*
 * These pi_table_* functions manipulate the color table.
 * pi_table_create:
 *	allocates and initializes a color table.
 * pi_table_get_value:
 *	get the specified value, and move it to the top of the list.
 * pi_table_lookup_value:
 *	look up the specified value, and move it to the top of the list.
 */
static void pi_table_create(pi)
    struct pi_info *pi;
{
    struct ct_t *t;
    int i;
    byte mask = pi->numcols - 1;
    pi->ct = pi_malloc(sizeof *pi->ct * pi->numcols, "pi_table_create(1)");
    for(i = 0, t = pi->ct; i < pi->numcols; i++, t++){
	int j;
	byte v = i;
	t->elt = pi_malloc(sizeof *t->elt * pi->numcols, "pi_table_create(2)");
	t->top = &t->elt[pi->numcols - 1];
	for(j = 0; j < pi->numcols; j++){
	    v = (v + 1) & mask;
	    if(j > 0)
		t->elt[j].old    = &t->elt[j - 1];
	    else
		t->elt[0].old    = t->top;
	    if(j < pi->numcols - 1)
		t->elt[j].recent = &t->elt[j + 1];
	    else
		t->elt[j].recent = &t->elt[0];
	    t->elt[j].val    = v;
	}
	t->elt[0].old = t->top;
	t->top->recent = &t->elt[0];
    }
}

static byte pi_table_get_value(pi, left, num)
    struct pi_info *pi;
    int left, num;
{
    struct ct_t *t = &pi->ct[left];
    struct elt_t *e = t->top;
    if(left >= pi->numcols || num >= pi->numcols)
	abort();
    if(num != 0){
	do {
	    e = e->old;
	}while(--num != 0);

	e->old->recent = e->recent;
	e->recent->old = e->old;

	e->recent = t->top->recent;
	e->recent->old = e;
	e->old = t->top;
	t->top->recent = e;

	t->top = e;
    }
    return e->val;
}

static int pi_table_lookup_value(pi, left, v)
    struct pi_info *pi;
    int left, v;
{
    struct ct_t *t = &pi->ct[left];
    struct elt_t *e = t->top;
    int num = 0;

    if(left >= pi->numcols || v >= pi->numcols)
	abort();

    while(e->val != v){
	e = e->old;
	num++;
    }

    if(num != 0){
	e->old->recent = e->recent;
	e->recent->old = e->old;

	e->recent = t->top->recent;
	e->recent->old = e;
	e->old = t->top;
	t->top->recent = e;

	t->top = e;
    }

    return num;
}

/*
 * These 2 functions read or write to a bit stream.
 * pi_read_bits:
 *	reads a specified-bit data from the bit stream.
 * pi_write_bits:
 *	writes a specified-bit data to the bit stream.
 */
static data32 pi_read_bits(pi, numbits)
    struct pi_info *pi;
    int numbits;
{
    data32 r = 0;

    while(numbits > 0){
	while(pi->bs.rest > 0 && numbits > 0){
	    r = (r << 1) | (pi->bs.cur & 0x80 ? 1 : 0);
	    pi->bs.cur <<= 1;
	    pi->bs.rest--;
	    numbits--;
	}
	if(numbits > 0){
	    int c;
	    if((c = fgetc(pi->fp)) == EOF)
		pi_file_warning(pi, PI_CORRUPT);
	    pi->bs.cur  = c;
	    pi->bs.rest = 8;
	}
    }

    return r;
}

static void pi_write_bits(pi, dat, bits)
    struct pi_info *pi;
    data32 dat;
    int bits;
{
    data32 dat_mask = 1 << (bits - 1);
    while(bits > 0){
	while(pi->bs.rest < 8 && bits > 0){
	    pi->bs.cur <<= 1;
	    if(dat & dat_mask)
		pi->bs.cur |= 1;
	    pi->bs.rest++;
	    bits--;
	    dat_mask >>= 1;
	}
	if(pi->bs.rest >= 8){
	    if(fputc((int)pi->bs.cur, pi->fp) == EOF)
		pi_file_error(pi, PI_WRITE);
	    pi->bs.cur  = 0;
	    pi->bs.rest = 0;
	}
    }
}

/*
 * The routines to initialize or clean up.
 * pi_inif_pi_info:
 *	initializes a pi_info structure.
 * pi_cleanup_pi_info:
 *	cleanup pi_info structure. It frees allocated memories.
 * pi_cleanup_pinfo:
 *	cleanup PICINFO structure when an error occurs.
 */
static void pi_init_pi_info(pi)
    struct pi_info *pi;
{
    pi->fp = NULL;
    pi->bs.rest = 0;
    pi->bs.cur = 0;
    pi->fsize = 0;
    pi->mode = 0;
    pi->width = pi->mode = 0;
    pi->aspect = 1.0;
    pi->cbits = 0;
    pi->numcols = 0;
    pi->cmap = NULL;
    pi->ct = NULL;
    pi->defcmap = 0;
    pi->writing_grey = 0;
}

static void pi_cleanup_pi_info(pi, writing)
    struct pi_info *pi;
    int writing;
{
    if(pi->fp && !writing){
	fclose(pi->fp);
	pi->fp = NULL;
    }
    if(pi->cmap){
	free(pi->cmap);
	pi->cmap = NULL;
    }
    if(pi->ct){
	int i;
	for(i = 0; i < pi->numcols; i++)
	    free(pi->ct[i].elt);
	free(pi->ct);
	pi->ct = NULL;
    }
}

static void pi_cleanup_pinfo(pinfo)
    PICINFO *pinfo;
{
    if(pinfo->pic){
	free(pinfo->pic);
	pinfo->pic = NULL;
    }
    if(pinfo->comment){
	free(pinfo->comment);
	pinfo->comment = NULL;
    }
}

/*
 * Error handling routins.
 * pi_memory_error:
 *	shows a error message, and terminates.
 * pi_error:
 *	shows a non-file error message.
 * pi_file_error:
 *	shows a file error message.
 */
static void pi_memory_error(scm, fn)
    char *scm, *fn;
{
    char buf[128];
    sprintf(buf, "%s: couldn't allocate memory. (%s)", scm ,fn);
    FatalError(buf);
}

static void pi_error(pi, mn)
    struct pi_info *pi;
    int mn;
{
    SetISTR(ISTR_WARNING, "%s", pi_msgs[mn]);
    longjmp(pi->jmp, 1);
}

static void pi_file_error(pi, mn)
    struct pi_info *pi;
    int mn;
{
    if(feof(pi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", pi_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", pi_msgs[mn], ERRSTR(errno));
    longjmp(pi->jmp, 1);
}

static void pi_file_warning(pi, mn)
    struct pi_info *pi;
    int mn;
{
    if(feof(pi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", pi_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", pi_msgs[mn], ERRSTR(errno));
}

static void pi_show_pi_info(pi)
    struct pi_info *pi;
{
    fprintf(stderr, "  file size: %ld.\n", pi->fsize);
    fprintf(stderr, "  mode: 0x%02x.\n", pi->mode);
    fprintf(stderr, "  image size: %dx%d.\n", pi->width, pi->height);
    fprintf(stderr, "  aspect: %f.\n", pi->aspect);
    fprintf(stderr, "  number of color bits: %d.\n", pi->cbits);
    fprintf(stderr, "  number of colors: %d.\n", pi->numcols);
    fprintf(stderr, "  using default colormap: %s.\n",
	    pi->defcmap ? "true" : "false");
    fprintf(stderr, "  writing greyscale image: %s.\n",
	    pi->writing_grey ? "true" : "false");
}

/*
 * Memory related routines.  If failed, they calls pi_memory_error.
 */
static void *pi_malloc(n, fn)
    size_t n;
    char *fn;
{
    void *r = (void *) malloc(n);
    if(r == NULL)
	pi_memory_error("malloc", fn);
    return r;
}

static void *pi_realloc(p, n, fn)
    void *p;
    size_t n;
    char *fn;
{
    void *r = (p == NULL) ? (void *) malloc(n) : (void *) realloc(p, n);
    if(r == NULL)
	pi_memory_error("realloc", fn);
    return r;
}
#endif /* HAVE_PI */
