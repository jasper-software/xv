/*
 * xvpic.c - load routine for `PIC' format pictures.
 *
 * The `PIC' format is used by many Japanese personal computer users.
 */

#include "xv.h"
#include <setjmp.h>

#ifdef HAVE_PIC

typedef unsigned short data16;
typedef unsigned int data32;

struct pic_info {
    jmp_buf jmp;
    FILE *fp;
    struct {
	int rest;
	byte cur;
    }bs;
    long fsize;
    int type, mode;
    int width, height;
    float aspect;
    int cbits;
    int cmapped;
    byte *cmap;
    int cached;
    struct cache_t {
	int newest;
	struct cachenode_t {
	    data32 dat;
	    int newer, older;
	} *node;
    } cache;
    int g_bits, r_bits, b_bits, i_bits;
    int inv_gr;
    int tiled256;
    int numcols;
    int writing_grey;
    data32 *data;
};

static void pic_open_file              PARM((struct pic_info*,char*));
static void pic_check_id               PARM((struct pic_info*));
static void pic_read_comment           PARM((struct pic_info*, char**));
static void pic_read_header            PARM((struct pic_info*));
static void pic_expand_data            PARM((struct pic_info*));
static int pic_expanding_read_len      PARM((struct pic_info*));
static data32 pic_expanding_read_color PARM((struct pic_info*));
static void pic_expanding_read_chain
			PARM((struct pic_info*, int, int, data32));
static void pic_make_xvpic
			PARM((struct pic_info*, byte**, byte*, byte*, byte*));

static void pic_write_id               PARM((struct pic_info*));
static void pic_write_comment          PARM((struct pic_info*, char*));
static void pic_write_header           PARM((struct pic_info*));
static void pic_write_palette
			PARM((struct pic_info*, byte*, byte*, byte*));
static void pic_make_sparse_data       PARM((struct pic_info*, byte*));
static void pic_write_data             PARM((struct pic_info*));
static void pic_write_length           PARM((struct pic_info*, data32));
static void pic_write_color            PARM((struct pic_info*, data32));
static void pic_write_chain
			PARM((struct pic_info*, int, int, data32));

static data32 pic_read_rgb             PARM((struct pic_info*));
static data32 pic_read_color_code      PARM((struct pic_info*));
static void pic_write_rgb              PARM((struct pic_info*, data32));
static void pic_write_color_code       PARM((struct pic_info*, data32));

static void pic_cache_init             PARM((struct pic_info*));
static data32 pic_cache_get_value      PARM((struct pic_info*, int));
static void pic_cache_add_value        PARM((struct pic_info*, data32));
static int pic_cache_lookup            PARM((struct pic_info*, data32));

static data32 pic_read_bits            PARM((struct pic_info*, int));
static void pic_write_bits             PARM((struct pic_info*, data32, int));
static byte pic_pad_bit                PARM((int, data32));

static void pic_init_info              PARM((struct pic_info*));
static void pic_cleanup_pic_info       PARM((struct pic_info*, int));
static void pic_cleanup_pinfo          PARM((PICINFO*));
static void pic_memory_error           PARM((char*, char*));
static void pic_error                  PARM((struct pic_info*, int));
static void pic_file_error             PARM((struct pic_info*, int));
static void pic_file_warning           PARM((struct pic_info*, int));
static void pic_show_pic_info          PARM((struct pic_info*));
static void *pic_malloc                PARM((size_t, char*));
static void *pic_realloc               PARM((void*, size_t, char*));


static char *pic_id = "PIC";

/* Error Messages */
static char *pic_msgs[] = {
    NULL,
#define PIC_OPEN 1
    "can't open file.",
#define PIC_CORRUPT 2
    "file corrupted.",
#define PIC_FORMAT 3
    "not PIC format.",
#define PIC_SUPPORT 4
    "unsupported type.",
#define PIC_COMMENT 5
    "can't read comment.",
#define PIC_TYPE 6
    "bad machine type.",
#define PIC_MODE 7
    "bad machine-dependent mode.",
#define PIC_NUM_COLORS 8
    "bad number of colors.",
#define PIC_SIZE 9
    "bad size.",
#define PIC_ASPECT 10
    "bad aspect.",
#define PIC_WRITE 11
    "write failed.",
};

#define H4(b) (((b) >> 4) & 0x0f)
#define L4(b) ( (b)       & 0x0f)


/* The main routine to load a PIC file. */
int LoadPIC(fname, pinfo)
    char *fname;
    PICINFO *pinfo;
{
    int e;
    struct pic_info pic;
    char buf[128];

    if(DEBUG) fputs("LoadPIC:\n", stderr);

    pic_init_info(&pic);

    pinfo->comment = NULL;
    if((e = setjmp(pic.jmp)) != 0){
	/* When an error occurs, comes here. */
	pic_cleanup_pic_info(&pic, 0);
	pic_cleanup_pinfo(pinfo);
	if(DEBUG) fputs("\n", stderr);
	return 0;
    }

    pic_open_file(&pic, fname);
    pic_check_id(&pic);
    pic_read_comment(&pic, &pinfo->comment);
    pic_read_header(&pic);
    pic_expand_data(&pic);
    pic_make_xvpic(&pic, &pinfo->pic, pinfo->r, pinfo->g, pinfo->b);

    pinfo->w = pic.width;
    if(pic.tiled256)
	pinfo->h = pic.height * 2;
    else
	pinfo->h = pic.height;
    pinfo->normw = pinfo->w;
    pinfo->normh = pinfo->h;
    pinfo->type = pic.cmapped ? PIC8 : PIC24;
    pinfo->frmType = F_PIC;
    pinfo->colType = F_FULLCOLOR;
    strcpy(pinfo->fullInfo, "PIC");
    switch(pic.type){
    case 0x0:
	strcat(pinfo->fullInfo, ", X68k");
	break;
    case 0x1:
	strcat(pinfo->fullInfo, ", PC-88VA");
	if(pic.mode & 1)
	    strcat(pinfo->fullInfo, ", HR");
	if(pic.mode & 2)
	    strcat(pinfo->fullInfo, ", tiled 256");
	break;
    case 0x2:
	strcat(pinfo->fullInfo, ", FM-TOWNS");
	if(pic.mode == 0x5){
	    strcat(pinfo->fullInfo, ", low-resolution");
	}else{
	    strcat(pinfo->fullInfo, ", high-resolution");
	}
	break;
    case 0x3:
	strcat(pinfo->fullInfo, ", Macintosh");
	break;
    case 0xf:
	;
    }
    sprintf(buf, " (%ld bytes)", pic.fsize);
    strcat(pinfo->fullInfo, buf);
    sprintf(pinfo->shrtInfo, "%dx%d(aspect %4.2f) PIC.",
	    pinfo->w, pinfo->h, pic.aspect);
    if (!nopicadjust)
	normaspect = pic.aspect;

    pic_cleanup_pic_info(&pic, 0);
    if(DEBUG) fputs("\n", stderr);
    return 1;
}

static void pic_open_file(pi, fname)
    struct pic_info *pi;
    char *fname;
{
    if((pi->fp = fopen(fname, "rb")) == NULL)
	pic_file_error(pi, PIC_OPEN);
    fseek(pi->fp, (size_t) 0, SEEK_END);
    pi->fsize = ftell(pi->fp);
    fseek(pi->fp, (size_t) 0, SEEK_SET);
}

static void pic_check_id(pi)
    struct pic_info *pi;
{
    char buf[3];
    if(fread(buf, (size_t) 3, (size_t) 1, pi->fp) != 1)
	pic_file_error(pi, PIC_CORRUPT);
    if(strncmp(buf, pic_id, (size_t) 3) != 0)
	pic_error(pi, PIC_FORMAT);
}

static void pic_read_comment(pi, comm)
    struct pic_info *pi;
    char **comm;
{
    /* The comment field is like:
     * comment-string ^Z dummy \0 \0
     */
    int max = -1, i = 0;
    int c;

    while(1){
	if((c = fgetc(pi->fp)) == EOF)
	    pic_file_error(pi, PIC_CORRUPT);
	if(c == '\032')			/* 0x1a, '^Z' */
	    break;
	if(max < i){
	    max += 32;
	    *comm = pic_realloc(*comm, (size_t) max + 1, "pic_read_comment#1");
	}
	(*comm)[i++] = c;
    }

    if(max < i){
	max++;
	*comm = pic_realloc(*comm, (size_t) max + 1, "pic_read_comment#2");
    }
    (*comm)[i] = '\0';

    while((c = fgetc(pi->fp)) != '\0'){	/* skip the dummy area */
	if(c == EOF)
	    pic_file_error(pi, PIC_CORRUPT);
    }

    if(fgetc(pi->fp) != '\0')		/* check the reserved byte */
	pic_error(pi, PIC_SUPPORT);
}

static void pic_read_header(pi)
    struct pic_info *pi;
{
    pi->mode   = pic_read_bits(pi, 4);
    pi->type   = pic_read_bits(pi, 4);
    pi->cbits  = pic_read_bits(pi, 16);
    pi->width  = pic_read_bits(pi, 16);
    pi->height = pic_read_bits(pi, 16);

    /* machine-dependent setup. */
    switch(pi->type){
    case 0x0:				/* X68K */
	if(pi->mode != 0)
	    pic_error(pi, PIC_MODE);
	switch(pi->cbits){
	case 4:
	    pi->aspect = 1.0;
	    pi->g_bits = pi->r_bits = pi->b_bits = 5;
	    pi->i_bits = 1;
	    pi->cmapped = 1;
	    break;

	case 8:
	    pi->aspect = 4.0 / 3.0;
	    pi->g_bits = pi->r_bits = pi->b_bits = 5;
	    pi->i_bits = 1;
	    pi->cmapped = 1;
	    break;

	case 15:
	    pi->aspect = 4.0 / 3.0;
	    pi->g_bits = pi->r_bits = pi->b_bits = 5;
	    pi->cached = 1;
	    break;

	case 16:
	    pi->aspect = 4.0 / 3.0;
	    pi->g_bits = pi->r_bits = pi->b_bits = 5;
	    pi->i_bits = 1;
	    pi->cached = 1;
	    break;

	default:
	    pic_error(pi, PIC_NUM_COLORS);
	}
	break;

    case 0x1:				/* PC-88VA */
	if(pi->height > 1000)
	    pic_error(pi, PIC_SIZE);
	switch(pi->width * 1000 + pi->height){
	case 640400:
	case 640204:
	case 640200:
	case 320408:
	case 320400:
	case 320200:
	    break;
	default:
	    pic_error(pi, PIC_SIZE);
	}
	pi->aspect = 400.0 / pi->height;
	pi->aspect *= pi->width / 640.0;
	if(pi->mode & 0x1)		/* HR mode */
	    pi->aspect *= 2.0;
	if(pi->mode & 0x2){		/* tiled 256 format */
	    if(pi->cbits != 16)
		pic_error(pi, PIC_NUM_COLORS);
	    pi->tiled256 = 1;
	}
	switch(pi->cbits){
	case 8:
	    pi->g_bits = pi->r_bits = 3;
	    pi->b_bits = 2;
	    break;

	case 12:
	    pi->g_bits = pi->r_bits = pi->b_bits = 4;
	    pi->cached = 1;
	    break;

	case 16:
	    pi->g_bits = 6;
	    pi->r_bits = pi->b_bits = 5;
	    pi->cached = 1;
	    break;

	default:
	    pic_error(pi, PIC_NUM_COLORS);
	}
	break;

    case 0x2:				/* FM-TOWNS */
	if(pi->cbits != 15)
	    pic_error(pi, PIC_NUM_COLORS);
	switch(pi->mode){
	case 0x5:
	case 0xc:
	    break;
	default:
	    pic_error(pi, PIC_MODE);
	}
	pi->g_bits = pi->r_bits = pi->b_bits = 5;
	pi->cached = 1;
	break;

    case 0x3:				/* MAC */
	if(pi->cbits != 15)
	    pic_error(pi, PIC_NUM_COLORS);
	pi->r_bits = pi->g_bits = pi->b_bits = 5;
	pi->inv_gr = 1;
	break;

    case 0xf:				/* misc */
	{
	    byte ah, al;

	    switch(pi->mode){
	    case 0x0:
		break;
	    case 0x1:
		pi->aspect = 4.0 / 3.0;
		break;
	    case 0xf:
		break;
	    default:
		pic_error(pi, PIC_MODE);
	    }
	    pic_read_bits(pi, 16);	/* x */
	    pic_read_bits(pi, 16);	/* y */
	    ah = pic_read_bits(pi, 8);
	    al = pic_read_bits(pi, 8);
	    if(ah > 0 && al > 0)
		pi->aspect = (float) al / (int) ah;
	    else if(pi->mode == 0xf)
		pic_error(pi, PIC_ASPECT);
	    switch(pi->cbits){
	    case 4:
	    case 8:
		pi->g_bits = pi->r_bits = pi->b_bits = pic_read_bits(pi, 8);
		pi->cmapped = 1;
		break;

	    case 12:
		pi->g_bits = pi->r_bits = pi->b_bits = 4;
		pi->cached = 1;
		break;

	    case 15:
		pi->g_bits = pi->r_bits = pi->b_bits = 5;
		pi->cached = 1;
		break;

	    case 16:
		pi->g_bits = pi->r_bits = pi->b_bits = 5;
		pi->i_bits = 1;
		pi->cached = 1;
		break;

	    case 24:
		pi->g_bits = pi->r_bits = pi->b_bits = 8;
		pi->cached = 1;
		break;

	    case 32:
		pic_error(pi, PIC_SUPPORT);
		break;

	    default:
		pic_error(pi, PIC_NUM_COLORS);
	    }
	}
	break;

    default:
	pic_error(pi, PIC_TYPE);
    }

    pi->numcols = 1 << pi->cbits;

    /* read palette data */
    if(pi->cmapped){
	int i;
	pi->cmap = pic_malloc((size_t) 3 * pi->numcols, "pic_read_header#1");
	for(i = 0; i < pi->numcols; i++){
	    data32 c = pic_read_rgb(pi);
	    pi->cmap[i * 3    ] = c >> 16 & 0xff;
	    pi->cmap[i * 3 + 1] = c >>  8 & 0xff;
	    pi->cmap[i * 3 + 2] = c       & 0xff;
	}
    }

    /* setup color code cache */
    if(pi->cached)
	pic_cache_init(pi);


    pi->data = pic_malloc(sizeof(data32) * pi->width * pi->height,   // GRR POSSIBLE OVERFLOW / FIXME
			  "pic_read_header#2");
    {
	int i;
	for(i = 0; i < pi->width * pi->height; i++)
	    pi->data[i] = 0xffffffff;
    }

    if(DEBUG)
	pic_show_pic_info(pi);
}

/* The main routine to expand a PIC file. */
static void pic_expand_data(pi)
    struct pic_info *pi;
{
    int cnt;
    data32 c;
    pi->data[0] = c = 0;
    for(cnt = -1; cnt < pi->width * pi->height; ){
	int len = pic_expanding_read_len(pi);
	cnt += len;
	if(cnt < pi->width * pi->height){
	    int x = cnt % pi->width;
	    int y = cnt / pi->width;
	    data32 c = pic_expanding_read_color(pi);
	    pic_expanding_read_chain(pi, x, y, c);
	}
    }
}

static int pic_expanding_read_len(pi)
    struct pic_info *pi;
{
    int len;
    byte bits;
    for(len = 2, bits = 1; pic_read_bits(pi, 1) == 1; bits++)
	len <<= 1;
    return len - 1 + pic_read_bits(pi, bits);
}

static data32 pic_expanding_read_color(pi)
    struct pic_info *pi;
{
    if(pi->cached){
	byte b = pic_read_bits(pi, 1);
	if(b){
	    return pic_cache_get_value(pi, (int) pic_read_bits(pi, 7));
	}else{
	    data32 c = pic_read_color_code(pi);
	    pic_cache_add_value(pi, c);
	    return c;
	}
    }
    return pic_read_color_code(pi);
}

static void pic_expanding_read_chain(pi, x, y, c)
    struct pic_info *pi;
    int x, y;
    data32 c;
{
    pi->data[y * pi->width + x] = c;
    if(pic_read_bits(pi, 1) == 1){
	int fin = 0;
	while(!fin){
	    switch(pic_read_bits(pi, 2)){
	    case 1:	/* left */
		pi->data[(++y) * pi->width + (--x)] = c;
		break;
	    case 2:	/* middle */
		pi->data[(++y) * pi->width +    x ] = c;
		break;
	    case 3:	/* right */
		pi->data[(++y) * pi->width + (++x)] = c;
		break;
	    case 0:	/* far or nothing */
		if(pic_read_bits(pi, 1) == 0)
		    fin = 1;
		else{
		    if(pic_read_bits(pi, 1) == 0)
			pi->data[(++y) * pi->width + (x -= 2)] = c;
		    else
			pi->data[(++y) * pi->width + (x += 2)] = c;
		}
	    }
	}
    }
}

/*
 * Make a picture from the expanded data.
 */
static void pic_make_xvpic(pi, xp, rp, gp, bp)
    struct pic_info *pi;
    byte **xp, *rp, *gp, *bp;
{
    if(pi->cmapped){
	if(pi->tiled256)
	    *xp = pic_malloc((size_t) pi->width * pi->height * 2,   // GRR POSSIBLE OVERFLOW / FIXME
			     "pic_make_xvpic#1");
	else
	    *xp = pic_malloc((size_t) pi->width * pi->height,   // GRR POSSIBLE OVERFLOW / FIXME
			     "pic_make_xvpic#2");
    }else
	*xp = pic_malloc((size_t) pi->width * pi->height * 3,   // GRR POSSIBLE OVERFLOW / FIXME
			 "pic_make_xvpic#3");

    if(pi->cmapped){
	int i;

	for(i = 0; i < pi->numcols; i++){
	    rp[i] = pi->cmap[i * 3    ];
	    gp[i] = pi->cmap[i * 3 + 1];
	    bp[i] = pi->cmap[i * 3 + 2];
	}

	if(pi->tiled256){
	    int pic_idx = 0, dat_idx;
	    data16 col = 0;
	    for(dat_idx = 0; dat_idx < pi->width * pi->height; dat_idx++){
		if(pi->data[dat_idx] != 0xffffffff)
		    col = pi->data[dat_idx];
		(*xp)[pic_idx++] = col      & 0xff;
		(*xp)[pic_idx++] = col >> 8 & 0xff;
		dat_idx++;
	    }
	}else{
	    int pic_idx = 0, dat_idx;
	    byte col = 0;
	    for(dat_idx = 0; dat_idx < pi->width * pi->height; dat_idx++){
		if(pi->data[dat_idx] != 0xffffffff)
		    col = pi->data[dat_idx];
		(*xp)[pic_idx++] = col;
	    }
	}
    }else{
	int pic_idx = 0, dat_idx;
	byte r = 0, g = 0, b = 0;
	for(dat_idx = 0; dat_idx < pi->width * pi->height; dat_idx++){
	    if(pi->data[dat_idx] != 0xffffffff){
		data32 col = pi->data[dat_idx];
		r = col >> 16 & 0xff;
		g = col >>  8 & 0xff;
		b = col       & 0xff;
	    }
	    (*xp)[pic_idx++] = r;
	    (*xp)[pic_idx++] = g;
	    (*xp)[pic_idx++] = b;
	}
    }
}


/* The main routine to write PIC file. */
int WritePIC(fp, pic0, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle,
	     comment)
    FILE *fp;
    byte *pic0;
    int ptype, w, h;
    byte *rmap, *gmap, *bmap;
    int numcols, colorstyle;
    char *comment;
{
    struct pic_info pic;
    int e;

    if(DEBUG) fputs("WritePIC:\n", stderr);

    pic_init_info(&pic);
    pic.fp = fp;
    pic.width  = w;
    pic.height = h;
    pic.writing_grey = (colorstyle == F_GREYSCALE);
    if(ptype != PIC24){		/* PIC8  */
	pic.cmapped = 1;
	pic.cached  = 0;
	pic.cbits   = 8;
	pic.g_bits  =
	pic.r_bits  =
	pic.b_bits  = 8;
	pic.i_bits  = 0;
	pic.numcols = numcols;
    }else{			/* PIC24 */
	pic.cmapped = 0;
	pic.cached  = 1;
	pic.cbits   = 24;
	pic.g_bits  =
	pic.r_bits  =
	pic.b_bits  = 8;
	pic.i_bits  = 0;
	pic.numcols = 1 << 24;
	pic_cache_init(&pic);
    }

    if((e = setjmp(pic.jmp)) != 0){
	/* When an error occurs while writing, comes here. */
	pic_cleanup_pic_info(&pic, 1);
	if(DEBUG) fputs("\n", stderr);
	return -1;
    }

    pic_write_id(&pic);
    pic_write_comment(&pic, comment);
    pic_write_header(&pic);
    if(pic.cmapped)
	pic_write_palette(&pic, rmap, gmap, bmap);
    pic_make_sparse_data(&pic, pic0);
    pic_write_data(&pic);
    pic_write_bits(&pic, 0, 8);

    pic_cleanup_pic_info(&pic, 1);
    if(DEBUG) fputs("\n", stderr);
    return 0;
}

static void pic_write_id(pi)
    struct pic_info *pi;
{
    if(fwrite("PIC", (size_t) 3, (size_t) 1, pi->fp) != 1)
	pic_file_error(pi, PIC_WRITE);
}

static void pic_write_comment(pi, comm)
    struct pic_info *pi;
    char *comm;
{
    if(comm){
	while(*comm){
	    int c = *comm;
	    if(c == '\032')
		c = ' ';
	    if(fputc(*comm, pi->fp) == EOF)
		pic_file_error(pi, PIC_WRITE);
	    comm++;
	}
    }
    /* write ^Z, 0, and reserved. */
    if(fwrite("\032\0\0", (size_t)3, (size_t) 1, pi->fp) != 1)
	pic_file_error(pi, PIC_WRITE);
}

static void pic_write_header(pi)
    struct pic_info *pi;
{
    if(DEBUG) pic_show_pic_info(pi);
    pic_write_bits(pi, (data32) 0, 4);			/* mode:  1:1 */
    pic_write_bits(pi, (data32) 0xf, 4);		/* type: misc */
    pic_write_bits(pi, (data32) pi->cbits, 16);		/* bits */
    pic_write_bits(pi, (data32) pi->width, 16);		/* width */
    pic_write_bits(pi, (data32) pi->height, 16);	/* height */
    pic_write_bits(pi, (data32) 0xffff, 16);		/* x: unused */
    pic_write_bits(pi, (data32) 0xffff, 16);		/* y: unused */
    pic_write_bits(pi, (data32) 0x0101, 16);		/* real aspect */
}

static void pic_write_palette(pi, r, g, b)
    struct pic_info *pi;
    byte *r, *g, *b;
{
    int i;
    data32 rgb = 0;
    pic_write_bits(pi, (data32) pi->g_bits, 8);
    for(i = 0; i < pi->numcols; i++){
	rgb = (data32) *r++ << 16 | (data32) *g++ << 8 | (data32) *b++;
	pic_write_rgb(pi, rgb);
    }
    for( ; i < 256; i++)
	pic_write_rgb(pi, rgb);
}

static void pic_make_sparse_data(pi, dat)
    struct pic_info *pi;
    byte *dat;
{
    int i;
    data32 c;

    pi->data = pic_malloc(sizeof(data32) * pi->width * pi->height,   // GRR POSSIBLE OVERFLOW / FIXME
			  "pic_make_sparse_data");

    if(pi->cmapped){
	c = 0;
	for(i = 0; i < pi->width * pi->height; i++){
	    if(c != dat[i])
		c = pi->data[i] = dat[i];
	    else
		pi->data[i] = 0xffffffff;
	}
    }else{
	int j = 0;
	c = 0;
	for(i = 0; i < pi->width * pi->height; i++){
	    data32 r, g, b, t;
	    r = dat[j++];
	    g = dat[j++];
	    b = dat[j++];
	    t = r << 16 | g << 8 | b;
	    if(c != t)
		c = pi->data[i] = t;
	    else
		pi->data[i] = 0xffffffff;
	}
    }
}

static void pic_write_data(pi)
    struct pic_info *pi;
{
    int i;
    int max = pi->width * pi->height;
    data32 c = 0;

    i = -1;
    while(i < max){
	int j;
	for(j = i + 1; j < max; j++){
	    if(pi->data[j] != 0xffffffff)
		break;
	}
	pic_write_length(pi, (data32) j - i);
	i = j;
	if(i < max){
	    pic_write_color(pi, c = pi->data[i]);
	    pic_write_chain(pi, i % pi->width, i / pi->width, c);
	}
    }
}

static void pic_write_length(pi, len)
    struct pic_info *pi;
    data32 len;
{
    int bits = 0;	/* leading 1's */
    int max = 2;

    while(len > max){
	max = (max + 1) * 2;
	bits++;
    }
    pic_write_bits(pi, 0xffffffff, bits);
    pic_write_bits(pi, 0, 1);
    pic_write_bits(pi, len - max / 2, bits + 1);
}

static void pic_write_color(pi, c)
    struct pic_info *pi;
    data32 c;
{
    if(pi->cached){
	int idx = pic_cache_lookup(pi, c);
	if(idx < 0){	/* not found */
	    pic_write_bits(pi, 0, 1);
	    pic_write_color_code(pi, c);
	    pic_cache_add_value(pi, c);
	}else{		/* found */
	    pic_write_bits(pi, (data32) 0xffffffff, 1);
	    pic_write_bits(pi, (data32) idx, 7);
	}
    }else
	pic_write_color_code(pi, c);
}

static void pic_write_chain(pi, x, y, c)
    struct pic_info *pi;
    int x, y;
    data32 c;
{
    int ctr = (y + 1) * pi->width + x;

    if(y < pi->height - 1 &&
       (                      pi->data[ctr    ] == c  ||
	(x > 0             && pi->data[ctr - 1] == c) ||
	(x < pi->width - 1 && pi->data[ctr + 1] == c) ||
	(x > 1             && pi->data[ctr - 2] == c) ||
	(x < pi->width - 2 && pi->data[ctr + 2] == c))){
	pic_write_bits(pi, 1, 1);
	while(++y < pi->height){
	    if(pi->data[ctr] == c){				  /* center */
		pic_write_bits(pi, 2, 2);
		pi->data[ctr] = 0xffffffff;
		ctr += pi->width;
	    }else if(x > 0 && pi->data[ctr - 1] == c){		  /* left */
		pic_write_bits(pi, 1, 2);
		pi->data[ctr - 1] = 0xffffffff;
		ctr += pi->width - 1;
	    }else if(x < pi->width - 1 && pi->data[ctr + 1] == c){/* right */
		pic_write_bits(pi, 3, 2);
		pi->data[ctr + 1] = 0xffffffff;
		ctr += pi->width + 1;
	    }else if(x > 1 && pi->data[ctr - 2] == c){		  /* 2-left */
		pic_write_bits(pi, 2, 4);
		pi->data[ctr - 2] = 0xffffffff;
		ctr += pi->width - 2;
	    }else if(x < pi->width - 2 && pi->data[ctr + 2] == c){/* 2-right */
		pic_write_bits(pi, 3, 4);
		pi->data[ctr + 2] = 0xffffffff;
		ctr += pi->width + 2;
	    }else						  /* nothing */
		break;
	}
	pic_write_bits(pi, 0, 3);
    }else
	pic_write_bits(pi, 0, 1);
}


/*
 * These 4 functions read or write a color.
 *
 * pic_read_rgb:
 *	reads an RGB. Each bit length is [rgb]_bits, but
 *	it is expanded to 8bits when returned.
 *
 * pic_read_color_code:
 *	reads a color code, whose length is cbits.
 *	It is the index to the colormap or RGB itself.
 *
 * pic_write_rgb:
 *	writes an RGB value.
 *
 * pic_write_color_code:
 *	writes a color code.
 */
static data32 pic_read_rgb(pi)
    struct pic_info *pi;
{
    int rb = pi->r_bits, gb = pi->g_bits, bb = pi->b_bits;
    byte r, g, b;
    if(pi->inv_gr){
	r = pic_read_bits(pi, rb);
	g = pic_read_bits(pi, gb);
    }else{
	g = pic_read_bits(pi, gb);
	r = pic_read_bits(pi, rb);
    }
    b = pic_read_bits(pi, bb);
    if(pi->i_bits){
	byte i;
	i = pic_read_bits(pi, pi->i_bits);
	r = r << pi->i_bits | i;
	g = g << pi->i_bits | i;
	b = b << pi->i_bits | i;
	rb += pi->i_bits;
	gb += pi->i_bits;
	bb += pi->i_bits;
    }
    r = pic_pad_bit(rb, r);
    g = pic_pad_bit(gb, g);
    b = pic_pad_bit(bb, b);

    return (data32) r << 16 | (data32) g << 8 | (data32) b;
}

static data32 pic_read_color_code(pi)
    struct pic_info *pi;
{
    if(pi->cmapped)
	return pic_read_bits(pi, pi->cbits);
    return pic_read_rgb(pi);
}

static void pic_write_rgb(pi, rgb)
    struct pic_info *pi;
    data32 rgb;
{
    byte r = rgb >> 16;
    byte g = rgb >> 8;
    byte b = rgb;
    if(pi->writing_grey)
	r = g = b = MONO(r, g, b);
    pic_write_bits(pi, g, pi->g_bits);
    pic_write_bits(pi, r, pi->r_bits);
    pic_write_bits(pi, b, pi->b_bits);
}

static void pic_write_color_code(pi, code)
    struct pic_info *pi;
    data32 code;
{
    if(pi->cmapped){
	pic_write_bits(pi, code, pi->cbits);
    }else{
	pic_write_rgb(pi, code);
    }
}


/*
 * These pic_cache_* functions are an implementation of the color cache.
 *
 * pic_cache_init:
 *	initializes the cache.
 *
 * pic_cache_get_value:
 *	gets a color indexed by the argument `idx'.
 *	It updates the `most recently used' time.
 *
 * pic_cache_add_value:
 *	adds a color to the top of the cache list.
 */
static void pic_cache_init(pi)
    struct pic_info *pi;
{
    int i;
    pi->cache.node = pic_malloc(sizeof(struct cachenode_t) * 128,
				"pic_cache_init");
    for(i = 0; i < 128; i++){
	pi->cache.node[i].newer = i + 1;
	pi->cache.node[i].older = i - 1;
	pi->cache.node[i].dat = 0;
    }
    pi->cache.node[  0].older = 127;
    pi->cache.node[127].newer = 0;
    pi->cache.newest = 0;
}

static data32 pic_cache_get_value(pi, idx)
    struct pic_info *pi;
    int idx;
{
    struct cachenode_t *p = pi->cache.node;
    int n = pi->cache.newest;
    if(n != idx){
	p[p[idx].newer].older = p[idx].older;
	p[p[idx].older].newer = p[idx].newer;

	p[p[n].newer].older = idx;
	p[idx].newer = p[n].newer;
	p[n].newer = idx;
	p[idx].older = n;

	pi->cache.newest = idx;
    }
    return pi->cache.node[idx].dat;
}

static void pic_cache_add_value(pi, dat)
    struct pic_info *pi;
    data32 dat;
{
    pi->cache.newest = pi->cache.node[pi->cache.newest].newer;
    pi->cache.node[pi->cache.newest].dat = dat;
}

static int pic_cache_lookup(pi, dat)
    struct pic_info *pi;
    data32 dat;
{
    int i;
    for(i = 0; i < 128; i++){
	if(pi->cache.node[i].dat == dat){
	    pic_cache_get_value(pi, i);
	    return i;
	}
    }
    return -1;
}


/*
 * These pic_{read,write}_bits functions access the bit stream.
 * pic_read_bits:
 *	reads the specified bits from the file.
 *
 * pic_write_bits:
 *	writes the specified bits to the file.
 */
static data32 pic_read_bits(pi, bits)
    struct pic_info *pi;
    int bits;
{
    data32 r = 0;

    while(bits > 0){
	while(pi->bs.rest > 0 && bits > 0){
	    r = (r << 1) | (pi->bs.cur & 0x80 ? 1 : 0);
	    pi->bs.cur <<= 1;
	    pi->bs.rest--;
	    bits--;
	}
	if(bits > 0){
	    int c;
	    if((c = fgetc(pi->fp)) == EOF){
		pic_file_warning(pi, PIC_CORRUPT);
		c = 0;
	    }
	    pi->bs.cur  = c;
	    pi->bs.rest = 8;
	}
    }

    return r;
}

static void pic_write_bits(pi, dat, bits)
    struct pic_info *pi;
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
		pic_error(pi, PIC_WRITE);
	    pi->bs.cur  = 0;
	    pi->bs.rest = 0;
	}
    }
}


/*
 * This function extends sub-8-bit data to 8-bit data using bit-replication.
 */
static byte pic_pad_bit(bits, dat)
    int bits;
    data32 dat;
{
    switch(bits){
    case 1:
	if(dat & 1)
	    dat = 0xff;
	else
	    dat = 0;
	break;
    case 2:
	dat = dat << 6 | dat << 4 | dat << 2 | dat;
	break;
    case 3:
	dat = dat << 5 | dat << 2 | dat >> 1;
	break;
    case 4:
	dat = dat << 4 | dat;
	break;
    case 5:
	dat = dat << 3 | dat >> 2;
	break;
    case 6:
	dat = dat << 2 | dat >> 4;
	break;
    case 7:
	dat = dat << 1 | dat >> 6;
    }

    return dat;
}

/*
 * These functions initialize or clean up structures.
 * pic_init_info:
 *	initializes a pic_info structure.
 * pic_cleanup_pic_info:
 *	cleans up a pic_info structure.
 * pic_cleanup_pinfo:
 *	cleans up a PICINFO structure.
 */
static void pic_init_info(pi)
    struct pic_info *pi;
{
    pi->fp = NULL;
    pi->bs.rest = 0;
    pi->bs.cur = '\0';
    pi->type = pi->mode = 0;
    pi->width = pi->height = 0;
    pi->aspect = 1.0;
    pi->cbits = 0;
    pi->cmapped = pi->cached = 0;
    pi->cache.node = NULL;
    pi->cmap = NULL;
    pi->g_bits = pi->r_bits = pi->b_bits = pi->i_bits = 0;
    pi->inv_gr = 0;
    pi->tiled256 = 0;
    pi->numcols = 0;
    pi->writing_grey = 0;
}

static void pic_cleanup_pic_info(pi, writing)
    struct pic_info *pi;
    int writing;
{
    if(!writing && pi->fp)
	fclose(pi->fp);
    if(pi->cmap)
	free(pi->cmap);
    if(pi->cache.node)
	free(pi->cache.node);
    if(pi->data)
	free(pi->data);
    pi->fp = NULL;
    pi->cmap = NULL;
    pi->cache.node = NULL;
    pi->data = NULL;
}

static void pic_cleanup_pinfo(pinfo)
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
 * Error Handlers.
 * pic_memory_error:
 *	shows an error message and terminates.
 * pic_error:
 *	shows a non-file error message and jumps to the entry for errors.
 * pic_file_error:
 *	shows a file error message and jumps to the entry for errors.
 * pic_file_warning:
 *	shows a file warning message.
 */
static void pic_memory_error(scm, fn)
    char *scm, *fn;
{
    char buf[128];
    sprintf(buf, "%s: can't allocate memory. (%s)", scm, fn);
    FatalError(buf);
}

static void pic_error(pi, mn)
    struct pic_info *pi;
    int mn;
{
    SetISTR(ISTR_WARNING, "%s", pic_msgs[mn]);
    longjmp(pi->jmp, 1);
}

static void pic_file_error(pi, mn)
    struct pic_info *pi;
    int mn;
{
    if(feof(pi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", pic_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", pic_msgs[mn], ERRSTR(errno));
    longjmp(pi->jmp, 1);
}

static void pic_file_warning(pi, mn)
    struct pic_info *pi;
    int mn;
{
    if(feof(pi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", pic_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", pic_msgs[mn], ERRSTR(errno));
}

static void pic_show_pic_info(pi)
    struct pic_info *pi;
{
    fprintf(stderr, "  file size: %ld.\n", pi->fsize);

    fputs("  machine: ", stderr);
    switch(pi->type){
    case 0x0:
	fputs("X68k", stderr);
	break;
    case 0x1:
	fputs("PC-88VA", stderr);
	if(pi->mode & 1)
	    fputs(",HR", stderr);
	if(pi->mode & 2)
	    fputs(",tiled256", stderr);
	break;
    case 0x2:
	fprintf(stderr,
		"FM-TOWNS,%s-resolution", pi->mode == 5 ? "low" : "high");
	break;
    case 0x3:
	fputs("Macintosh", stderr);
	break;
    case 0xf:
	fputs("misc", stderr);
    }
    fputs("\n", stderr);

    fprintf(stderr, "  image size: %dx%d\n", pi->width, pi->height);
    fprintf(stderr, "  aspect: %f\n", pi->aspect);
    fprintf(stderr, "  cache: %s\n", pi->cached ? "on" : "off");
    fprintf(stderr, "  colormap: %s\n", pi->cmapped ? "on" : "off");
    fprintf(stderr, "  number of color bits: %d\n", pi->cbits);
    fprintf(stderr, "  number of RGB bits: R%d,G%d,B%d,I%d\n",
	   pi->r_bits, pi->g_bits, pi->b_bits, pi->i_bits);
    fprintf(stderr, "  inverted G&R: %s\n", pi->inv_gr ? "true" : "false");
    fprintf(stderr, "  number of colors: %d\n", pi->numcols);
}

/* Memory related routines. */
static void *pic_malloc(n, fn)
    size_t n;
    char *fn;
{
    void *r = (void *) malloc(n);
    if(r == NULL)
	pic_memory_error("malloc", fn);
    return r;
}

static void *pic_realloc(p, n, fn)
    void *p;
    size_t n;
    char *fn;
{
    void *r = (p == NULL) ? (void *) malloc(n) : (void *) realloc(p, n);
    if(r == NULL)
	pic_memory_error("realloc", fn);
    return r;
}
#endif /* HAVE_PIC */
