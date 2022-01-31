/*
 *  xvmag.c - load routine for `MAG' format pictures.
 *
 *  The `MAG' format is used by many Japanese personal computer users.
 *  This program is based on MAGBIBLE.DOC which is the specification
 *  for `MAG' format written by Woody RINN.  It is written in Japanese,
 *  and exists in some anon-ftp sites.
 */

#include "xv.h"
#include <setjmp.h>

#ifdef HAVE_MAG

typedef unsigned short data16;

struct mag {
    jmp_buf jmp;
    FILE *fp;
    long fsize;
    int m_256, m_dig, m_8, m_200;
    int x1, y1, x2, y2, left_pad, right_pad;
    int p_width, p_height, width, height;
    long h_off, a_off, a_size, b_off, b_size, p_off, p_size;
    byte *a, *b, *p;
};

static void mag_open_file       PARM((struct mag*, char*));
static void mag_read_check_data PARM((struct mag*));
static void mag_read_comment    PARM((struct mag*, char**));
static void mag_read_header     PARM((struct mag*));
static void mag_read_palette    PARM((struct mag*, byte*, byte*, byte*));
static void mag_read_flags      PARM((struct mag*));
static void mag_read_pixel_data PARM((struct mag*));
static void mag_expand_body     PARM((struct mag*, byte**));

static void mag_compress_data    PARM((struct mag*, byte*));
static void mag_write_check_data PARM((struct mag*));
static void mag_write_comment    PARM((struct mag*, char *));
static void mag_write_palette    PARM((struct mag*, int,
				       byte*, byte*, byte*, int));
static void mag_write_flags      PARM((struct mag*));
static void mag_write_pixel_data PARM((struct mag*));
static void mag_write_header     PARM((struct mag*));
static void mag_set_double_word  PARM((long, byte *));

static void mag_init_info        PARM((struct mag*));
static void mag_cleanup_mag_info PARM((struct mag*, int));
static void mag_cleanup_pinfo    PARM((PICINFO*));
static void mag_memory_error     PARM((char*, char*));
static void mag_error            PARM((struct mag*, int));
static void mag_file_error       PARM((struct mag*, int));
static void mag_file_warning     PARM((struct mag*, int));
static void mag_show_struct      PARM((struct mag*));
static void *mag_malloc          PARM((size_t, char*));
static void *mag_realloc         PARM((void*, size_t, char*));


static char *mag_id = "MAKI02  ";
static struct{
    int dx, dy;
}points[16] = {
    { 0,  0}, { 1,  0}, { 2,  0}, { 4,  0},
    { 0,  1}, { 1,  1},
    { 0,  2}, { 1,  2}, { 2,  2},
    { 0,  4}, { 1,  4}, { 2,  4},
    { 0,  8}, { 1,  8}, { 2,  8},
    { 0, 16},
};
static int try[15] = {1, 4, 5, 6, 7, 9, 10, 2, 8, 11, 12, 13, 14, 3, 15};

static char *mag_msgs[] = {
    NULL,
#define MAG_OPEN 1
    "can't open file",
#define MAG_CORRUPT 2
    "file currupted.",
#define MAG_FORMAT 3
    "not MAG format.",
#define MAG_WRITE 4
    "write failed.",
};


#define H4(x) (((int) (x) >> 4) & 0x0f)	/* operates on a byte */
#define L4(x) ((x) & 0x0f)
#define H8(x) (((x) >> 8) & 0xff)	/* operates on a data16 */
#define L8(x) ((x) & 0xff)

#define error(msgnum) longjmp(mi->jmp, msgnum)


/* The main routine to load a MAG file. */
int LoadMAG(fname, pinfo)
    char *fname;
    PICINFO *pinfo;
{
    struct mag mag;
    int e;

    if(DEBUG) fputs("LoadMAG:\n", stderr);

    pinfo->comment = NULL;
    mag_init_info(&mag);
    if((e = setjmp(mag.jmp)) != 0){
	/* When an error occurs, comes here. */
	mag_cleanup_mag_info(&mag, 0);
	mag_cleanup_pinfo(pinfo);
	return 0;
    }

    mag_open_file(&mag, fname);
    mag_read_check_data(&mag);
    mag_read_comment(&mag, &pinfo->comment);
    mag_read_header(&mag);
    mag_read_palette(&mag, pinfo->r, pinfo->g, pinfo->b);
    mag_read_flags(&mag);
    mag_read_pixel_data(&mag);
    mag_expand_body(&mag, &pinfo->pic);

    pinfo->w = pinfo->normw = mag.width;
    pinfo->h = pinfo->normh = mag.height;
    pinfo->type = PIC8;
    pinfo->frmType = F_MAG;
    pinfo->colType = F_FULLCOLOR;
    sprintf(pinfo->fullInfo, "MAG, %d colors%s (%ld bytes)",
	    mag.m_256 ? 256 : (mag.m_8 ? 8 : 16),
	    mag.m_200 ? ", aspect 0.5" : "", mag.fsize);
    sprintf(pinfo->shrtInfo, "%dx%d MAG", mag.width, mag.height);
    if(mag.m_200)
	normaspect = 0.5;

    mag_cleanup_mag_info(&mag, 0);
    return 1;
}

static void mag_open_file(mi, fname)
    struct mag *mi;
    char *fname;
{
    if((mi->fp = fopen(fname, "rb")) == NULL)
	mag_file_error(mi, MAG_OPEN);
    fseek(mi->fp, (size_t) 0, SEEK_END);
    mi->fsize = ftell(mi->fp);
    fseek(mi->fp, (size_t) 0, SEEK_SET);
}

static void mag_read_check_data(mi)
    struct mag *mi;
{
    char buffer[8];

    if(fread(buffer, (size_t) 8, (size_t) 1, mi->fp) != 1)
	mag_file_error(mi, MAG_CORRUPT);
    if(strncmp(buffer, mag_id, (size_t) 8) != 0)
	mag_error(mi, MAG_FORMAT);
}

static void mag_read_comment(mi, p)
    struct mag *mi;
    char **p;
{
    int max = -1, i = 0;
    int c;

    while((c = fgetc(mi->fp)) != EOF){
	if(c == 0x1a)
	    break;
	if(max < i){
	    max += 16;
	    *p = mag_realloc(*p, (size_t) max + 1, "mag_read_comment#1");
	}
	(*p)[i++] = c;
    }

    if(c == EOF)
	mag_file_error(mi, MAG_CORRUPT);

    if(max < i){
	*p = mag_realloc(*p, (size_t) max + 2, "mag_read_comment#2");
    }
    if(i > 24){
	(*p)[i] = '\0';
	strcpy(*p, &(*p)[24]);
    }else{
	(*p)[0] = '\0';
    }
}

static void mag_read_header(mi)
    struct mag *mi;
{
    byte buf[32];

    mi->h_off = ftell(mi->fp);

    if(fread(buf, (size_t) 32, (size_t) 1, mi->fp) != 1)
	mag_file_error(mi, MAG_CORRUPT);

    mi->m_256 = buf[3] & 0x80;
    mi->m_dig = buf[3] & 0x04;
    mi->m_8   = buf[3] & 0x02;
    mi->m_200 = buf[3] & 0x01;

    mi->x1 = buf[ 4] + buf[ 5] * 256;
    mi->y1 = buf[ 6] + buf[ 7] * 256;
    mi->x2 = buf[ 8] + buf[ 9] * 256;
    mi->y2 = buf[10] + buf[11] * 256;

#define get_dword(a, b, c, d) \
    ((long)(a) << 24 | (long)(b) << 16 | (long)(c) << 8 | (long)(d))

    mi->a_off  = get_dword(buf[15], buf[14], buf[13], buf[12]);
    mi->b_off  = get_dword(buf[19], buf[18], buf[17], buf[16]);
    mi->b_size = get_dword(buf[23], buf[22], buf[21], buf[20]);
    mi->p_off  = get_dword(buf[27], buf[26], buf[25], buf[24]);
    mi->p_size = get_dword(buf[31], buf[30], buf[29], buf[28]);
#undef get_dword

    mi->a_size = mi->b_off - mi->a_off;
    mi->a_off += mi->h_off;
    mi->b_off += mi->h_off;
    mi->p_off += mi->h_off;

    mi->width     = mi->x2 - mi->x1 + 1;
    mi->height    = mi->y2 - mi->y1 + 1;
    mi->left_pad  =       mi->x1 & 07;
    mi->right_pad = 07 - (mi->x2 & 07);
    mi->x1       -= mi->left_pad;		/* x1 = 8m   */
    mi->x2       += mi->right_pad;		/* x2 = 8n+7 */
    mi->p_width   = ((mi->x2 + 1) - mi->x1) / (mi->m_256 ? 2 : 4);
    mi->p_height  =  (mi->y2 + 1) - mi->y1;

    if(DEBUG) mag_show_struct(mi);
}

static void mag_read_palette(mi, r, g, b)
    struct mag *mi;
    byte *r, *g, *b;
{
    int num_palettes;
    byte *buf;

    if(mi->m_256)
	num_palettes = 256;
    else
	num_palettes = 16;

    buf = mag_malloc((size_t)num_palettes * 3, "mag_read_palette");

    if(fread(buf, (size_t) 3, (size_t) num_palettes, mi->fp) != num_palettes){
	free(buf);
	mag_file_error(mi, MAG_CORRUPT);
    }

    for(num_palettes--; num_palettes >= 0; num_palettes--){
	g[num_palettes] = buf[num_palettes * 3    ];
	r[num_palettes] = buf[num_palettes * 3 + 1];
	b[num_palettes] = buf[num_palettes * 3 + 2];
    }

    free(buf);
}

static void mag_read_flags(mi)
     struct mag *mi;
{
    mi->a = mag_malloc((size_t) mi->a_size, "mag_read_flags#1");
    mi->b = mag_malloc((size_t) mi->b_size, "mag_read_flags#2");

    fseek(mi->fp, mi->a_off, SEEK_SET);
    if(fread(mi->a, (size_t) mi->a_size, (size_t) 1, mi->fp) != 1)
	mag_file_warning(mi, MAG_CORRUPT);
    if(fread(mi->b, (size_t) mi->b_size, (size_t) 1, mi->fp) != 1)
	mag_file_warning(mi, MAG_CORRUPT);
}

static void mag_read_pixel_data(mi)
    struct mag *mi;
{
    mi->p = mag_malloc((size_t) mi->p_size, "mag_read_pixel_data");

    fseek(mi->fp, mi->p_off, SEEK_SET);
    if(fread(mi->p, (size_t) mi->p_size, (size_t) 1, mi->fp) != 1)
	mag_file_warning(mi, MAG_CORRUPT);
}

/* MAG expanding routine */
static void mag_expand_body(mi, pic0)
    struct mag *mi;
    byte **pic0;
{
    int ai, bi, fi, pi;
    int px, py, x, y;
    byte *flag;
    byte mask;
    data16 *pixel0;

    flag   = mag_malloc((size_t) mi->p_width / 2, "mag_expand_body#1");
    *pic0  = mag_malloc((size_t) mi->width * mi->height, "mag_expand_body#2");  // GRR POSSIBLE OVERFLOW / FIXME
    pixel0 = mag_malloc((size_t) 2 * mi->p_width * 17, "mag_expand_body#3");  // GRR POSSIBLE OVERFLOW / FIXME

#define pixel(x, y) pixel0[(y) % 17 * mi->p_width + (x)]

    ai = bi = pi = 0;
    mask = 0x80;
    for(y = py = 0; py < mi->p_height; py++){
	for(fi = 0; fi < mi->p_width / 2; fi++){
	    if(py == 0){
		if(mi->a[ai] & mask)
		    flag[fi] = mi->b[bi++];
		else
		    flag[fi] = 0;
	    }else{
		if(mi->a[ai] & mask)
		    flag[fi] ^= mi->b[bi++];
	    }
	    if((mask >>= 1) == 0){
		mask = 0x80;
		ai++;
	    }
	}

	for(px = fi = 0; fi < mi->p_width / 2; fi++){
	    int f = H4(flag[fi]);
	    if(f == 0){
		pixel(px, py) = mi->p[pi] + mi->p[pi + 1] * 256;
		px++;
		pi+=2;
	    }else{
		int dx = points[f].dx, dy = points[f].dy;
		pixel(px, py) = pixel(px - dx, py - dy);
		px++;
	    }

	    f = L4(flag[fi]);
	    if(f == 0){
		pixel(px, py) = mi->p[pi] + mi->p[pi + 1] * 256;
		px++;
		pi+=2;
	    }else{
		int dx = points[f].dx, dy = points[f].dy;
		pixel(px, py) = pixel(px - dx, py - dy);
		px++;
	    }
	}

#define inside(x) ((unsigned int)(x) < mi->width)
#define  pic(x, y) (*pic0)[(y) * mi->width + (x)]
	for(x = -mi->left_pad, px = 0; px < mi->p_width; px++){
	    data16 p = pixel(px, py);
	    if(mi->m_256){
		if(inside(x))
		    pic(x, y) = L8(p);
		x++;
		if(inside(x))
		    pic(x, y) = H8(p);
		x++;
	    }else{
		if(inside(x))
		    pic(x, y) = H4(L8(p));
		x++;
		if(inside(x))
		    pic(x, y) = L4(L8(p));
		x++;
		if(inside(x))
		    pic(x, y) = H4(H8(p));
		x++;
		if(inside(x))
		    pic(x, y) = L4(H8(p));
		x++;
	    }
	}
	y++;
    }
#undef pic
#undef inside
#undef pixel

    free(flag);
    free(pixel0);
}


/* The main routine to write a MAG file. */
int WriteMAG(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle,
	      comment)
    FILE *fp;
    byte *pic;
    int ptype, w, h;
    byte *rmap, *gmap, *bmap;
    int numcols, colorstyle;
    char *comment;
{
    byte rtemp[256], gtemp[256], btemp[256];
    struct mag mag;
    int e;

    if(DEBUG) fputs("WriteMag\n", stderr);

    mag_init_info(&mag);
    mag.fp = fp;

    if(ptype == PIC24){
	if(!(pic = Conv24to8(pic, w, h, 256, rtemp, gtemp, btemp)))
	    mag_memory_error("Conv24to8", "WriteMAG");
	rmap = rtemp;
	gmap = gtemp;
	bmap = btemp;
	numcols = 256;
	mag.m_256 = 1;
    }else{
	if(numcols > 16)
	    mag.m_256 = 1;
    }

    if((e = setjmp(mag.jmp)) != 0){
	/* When an error occurs, comes here. */
	mag_cleanup_mag_info(&mag, 1);
	return -1;
    }

    mag.x2 = w - 1;
    mag.y2 = h - 1;
    mag.right_pad = 07 - (mag.x2 & 07);
    mag.p_width = (w + mag.right_pad) / (mag.m_256 ? 2 : 4);
    mag.p_height = h;
    mag.width = w;
    mag.height = h;
    mag.a_size = (mag.p_width * mag.p_height + 15) / 16;	/* x/2/8 */   // GRR POSSIBLE OVERFLOW / FIXME
    if(mag.a_size % 2)
	mag.a_size++;

    mag_compress_data(&mag, pic);
    mag_write_check_data(&mag);
    mag_write_comment(&mag, comment);

    mag.h_off = ftell(mag.fp);

    mag_write_palette(&mag, numcols, rmap, gmap, bmap,
		      colorstyle == F_GREYSCALE);
    mag_write_flags(&mag);
    mag_write_pixel_data(&mag);
    mag_write_header(&mag);

    mag_cleanup_mag_info(&mag, 1);
    return 0;
}

/* MAG compressing routine */
static void mag_compress_data(mi, pic0)
    struct mag *mi;
    byte *pic0;
{
    int ai, bi, pi, i;
    int bmax, pmax;
    byte mask;
    byte *flag0;
    data16 *pixel0;
    int px, py, x, y;

    pixel0 = mag_malloc((size_t) 2 * mi->p_width * mi->p_height,  // GRR POSSIBLE OVERFLOW / FIXME
			"mag_compress_data#1");
    flag0 = mag_malloc((size_t) mi->p_width * mi->p_height,  // GRR POSSIBLE OVERFLOW / FIXME
		       "mag_compress_data#2");

#define pic(x, y) pic0[(y) * mi->width + (x)]
    /* convert dots to pixels */
    i = 0;
    for(y = py = 0; py < mi->p_height; py++){
	for(x = px = 0; px < mi->p_width; px++){
	    data16 p = 0;
	    if(mi->m_256){
		if(x < mi->width)
		    p += pic(x, y);
		x++;
		if(x < mi->width)
		    p += pic(x, y) * 256;
		x++;
	    }else{
		if(x < mi->width)
		    p += pic(x, y) * 16;
		x++;
		if(x < mi->width)
		    p += pic(x, y);
		x++;
		if(x < mi->width)
		    p += pic(x, y) * 4096;
		x++;
		if(x < mi->width)
		    p += pic(x, y) * 256;
		x++;
	    }
	    pixel0[i++] = p;
	}
	y++;
    }
#undef pic

#define pixel(x, y) pixel0[(y) * mi->p_width + (x)]
#define  flag(x, y)  flag0[(y) * mi->p_width + (x)]
    /* get flags */
    pmax = pi = 0;
    for(py = 0; py < mi->p_height; py++){
	for(px = 0; px < mi->p_width; px++){
	    int t;
	    for(t = 0; t < 15; t++){
		int dx = points[try[t]].dx, dy = points[try[t]].dy;
		if(dx <= px && dy <= py){
		    if(pixel(px - dx, py - dy) == pixel(px, py))
			break;
		}
	    }
	    if(t < 15){
		flag(px, py) = try[t];
	    }else{
		flag(px, py) = 0;
		if(pmax <= pi + 1){
		    pmax += 128;
		    mi->p = mag_realloc(mi->p, (size_t) pmax,
					"mag_compress_data#3");
		}
		mi->p[pi++] = L8(pixel(px, py));
		mi->p[pi++] = H8(pixel(px, py));
	    }
	}
    }
#undef flag
#undef pixel

    /* pack 2 flags into 1 byte */
    for(i = 0; i < mi->p_width / 2 * mi->p_height; i++)
	flag0[i] = flag0[i * 2] * 16 + flag0[i * 2 + 1];

#define flag(x, y)  flag0[(y) * mi->p_width / 2 + (x)]
    for(py = mi->p_height - 1; py >= 1; py--){
	for(px = 0; px < mi->p_width / 2; px++)
	    flag(px, py) ^= flag(px, py - 1);
    }
#undef flag

    mask = 0x80;
    ai = bi = bmax = 0;
    mi->a = mag_malloc((size_t) mi->a_size, "mag_compress_data#4");   // GRR POSSIBLE OVERFLOW / FIXME
    for(i = 0; i < mi->p_width / 2 * mi->p_height; i++){
	if(flag0[i] == 0){
	    mi->a[ai] &= ~mask;
	}else{
	    if(bmax == bi){
		bmax += 128;
		mi->b = mag_realloc(mi->b, (size_t) bmax,
				    "mag_compress_data#4");
	    }
	    mi->b[bi++] = flag0[i];
	    mi->a[ai] |= mask;
	}

	if((mask >>= 1) == 0){
	    mask = 0x80;
	    ai++;
	}
    }

    if(bi % 2)
	bi++;
    mi->b_size = bi;

    mi->p_size = pi;

    free(pixel0);
    free(flag0);
}

static void mag_write_check_data(mi)
    struct mag *mi;
{
    if(fwrite(mag_id, (size_t) 8, (size_t) 1, mi->fp) != 1)
	mag_file_error(mi, MAG_WRITE);
}

static void mag_write_comment(mi, comment)
    struct mag *mi;
    char *comment;
{
    char *p;
    int i;

    if(fputs("XV   ", mi->fp) == EOF)
	mag_file_error(mi, MAG_WRITE);

    if((p = (char *) getenv("USER")) == NULL)
	p = "????????";
    for(i = 5; i < 24; i++){
	if(*p == '\0')
	    break;
	if(fputc(*p++, mi->fp) == EOF)
	    mag_file_error(mi, MAG_WRITE);
    }
    for( ; i < 24; i++){
	if(fputc(' ', mi->fp) == EOF)
	    mag_file_error(mi, MAG_WRITE);
    }

    if(comment){
	int l = strlen(comment);
	if(l > 0){
	    int i;
	    for(i = 0; i < l; i++){
		if(comment[i] == 0x1a)
		    comment[i] = ' ';
	    }
	    if(fwrite(comment, (size_t) l, (size_t) 1, mi->fp) != 1)
		mag_file_error(mi, MAG_WRITE);
	}
    }

    if(fputc(0x1a, mi->fp) == EOF)
	mag_file_error(mi, MAG_WRITE);
}

static void mag_write_palette(mi, num, r, g, b, grey)
    struct mag *mi;
    int num;
    byte *r, *g, *b;
    int grey;
{
    int i, left;
    char buf[3];

    fseek(mi->fp, 32L, SEEK_CUR);	/* skip header area */
    for(i = 0; i < num; i++){
	buf[0] = *g++;
	buf[1] = *r++;
	buf[2] = *b++;
	if(grey)
	    buf[0] = buf[1] = buf[2] = MONO(buf[1], buf[0], buf[2]);
	if(fwrite(buf, (size_t) 3, (size_t) 1, mi->fp) != 1)
	    mag_file_error(mi, MAG_WRITE);
    }
    if(num < 16){
	left = 16 - num;
    }else if(num == 16){
	left = 0;
    }else if(num < 256){
	left = 256 - num;
    }else if(num == 256){
	left = 0;
    }else
	left = 0;	/* shouldn't happen */

    if(left > 0){
	for(i = 0; i < left; i++){
	    if(fwrite(buf, (size_t) 3, (size_t) 1, mi->fp) != 1)
		mag_file_error(mi, MAG_WRITE);
	}
    }
}

static void mag_write_flags(mi)
    struct mag *mi;
{
    int i;

    mi->a_off = ftell(mi->fp);
    for(i = 0; i < mi->a_size; i++){
	if(fputc(mi->a[i], mi->fp) == EOF)
	    mag_file_error(mi, MAG_WRITE);
    }

    mi->b_off = ftell(mi->fp);
    for(i = 0; i < mi->b_size; i++){
	if(fputc(mi->b[i], mi->fp) == EOF)
	    mag_file_error(mi, MAG_WRITE);
    }
}

static void mag_write_pixel_data(mi)
    struct mag *mi;
{
    int i;

    mi->p_off = ftell(mi->fp);
    for(i = 0; i < mi->p_size; i++){
	if(fputc(mi->p[i], mi->fp) == EOF)
	    mag_file_error(mi, MAG_WRITE);
    }
}

static void mag_write_header(mi)
    struct mag *mi;
{
    byte buf[32];

    if(DEBUG) mag_show_struct(mi);

    mi->a_off -= mi->h_off;
    mi->b_off -= mi->h_off;
    mi->p_off -= mi->h_off;

    buf[ 0] = buf[1] = buf[2] = 0;
    buf[ 3] = (mi->m_256 ? 0x80 : 0);
    buf[ 4] = buf[5] = 0;
    buf[ 6] = buf[7] = 0;
    buf[ 8] = L8(mi->x2);
    buf[ 9] = H8(mi->x2);
    buf[10] = L8(mi->y2);
    buf[11] = H8(mi->y2);
    mag_set_double_word(mi->a_off,  &buf[12]);
    mag_set_double_word(mi->b_off,  &buf[16]);
    mag_set_double_word(mi->b_size, &buf[20]);
    mag_set_double_word(mi->p_off,  &buf[24]);
    mag_set_double_word(mi->p_size, &buf[28]);

    fseek(mi->fp, mi->h_off, SEEK_SET);
    if(fwrite(buf, (size_t) 32, (size_t) 1, mi->fp) != 1)
	mag_file_error(mi, MAG_WRITE);
}

static void mag_set_double_word(n, p)
    long n;
    byte *p;
{
    p[0] = n % 256;	/* ugly...anything wrong with shift/mask operations? */
    p[1] = n / 256 % 256;		/* (n >> 8) & 0xff */
    p[2] = n / 256 / 256 % 256;	/* (n >> 16) & 0xff */
    p[3] = n / 256 / 256 / 256 % 256;	/* (n >> 24) & 0xff */
}

/*
 * The routines to initialize or clean up.
 * mag_init_info:
 *	initializes a mag structure.
 * mag_cleanup_mag_info:
 *	cleans up a mag structure.
 * mag_cleanup_pinfo:
 *	cleans up a PICINFO structure.
 */
static void mag_init_info(mi)
    struct mag *mi;
{
    mi->fp = NULL;
    mi->fsize = 0;
    mi->m_256 = mi->m_dig = mi->m_8 = mi->m_200 = 0;
    mi->x1 = mi->y1 = mi->x2 = mi->y2 = 0;
    mi->left_pad = mi->right_pad = 0;
    mi->p_width = mi->p_height = mi->width = mi->height = 0;
    mi->h_off = mi->p_off = mi->p_size = 0;
    mi->a_off = mi->a_size = mi->b_off = mi->b_size = 0;
    mi->a = NULL;
    mi->b = NULL;
    mi->p = NULL;
}

static void mag_cleanup_mag_info(mi, writing)
    struct mag *mi;
    int writing;
{
    if(mi->fp && !writing)
	fclose(mi->fp);
    if(mi->a)
	free(mi->a);
    if(mi->b)
	free(mi->b);
    if(mi->p)
	free(mi->p);
}

static void mag_cleanup_pinfo(pinfo)
    PICINFO *pinfo;
{
    if(pinfo->comment){
	free(pinfo->comment);
	pinfo->comment = NULL;
    }
    if(pinfo->pic){
	free(pinfo->pic);
	pinfo->pic = NULL;
    }
}

/*
 * Error handler.
 * mag_memory_error:
 *	shows an error message, and terminates.
 * mag_error:
 *	shows an non-file error message, and jumps to the entry for errors.
 * mag_file_error:
 *	shows an file error message, and jumps to the entry for errors.
 * mag_file_warning:
 *	shows an file warning message.
 */
static void mag_memory_error(scm, fn)
    char *scm, *fn;
{
    char buf[128];
    sprintf(buf, "%s: can't allocate memory. (%s)", scm, fn);
    FatalError(buf);
}

static void mag_error(mi, mn)
    struct mag *mi;
    int mn;
{
    SetISTR(ISTR_WARNING, "%s", mag_msgs[mn]);
    longjmp(mi->jmp, 1);
}

static void mag_file_error(mi, mn)
    struct mag *mi;
    int mn;
{
    if(feof(mi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", mag_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", mag_msgs[mn], ERRSTR(errno));
    longjmp(mi->jmp, 1);
}

static void mag_file_warning(mi, mn)
    struct mag *mi;
    int mn;
{
    if(feof(mi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", mag_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", mag_msgs[mn], ERRSTR(errno));
}

static void mag_show_struct (mi)
    struct mag *mi;
{
    fprintf(stderr, "  256 colors: %s\n", mi->m_256 ? "true" : "false");
    fprintf(stderr, "  8 colors: %s\n", mi->m_8 ? "true" : "false");
    fprintf(stderr, "  digital colors: %s\n", mi->m_dig ? "true" : "false");
    fprintf(stderr, "  aspect ratio: %f\n", mi->m_200 ? 0.5 : 1.0);
    fprintf(stderr, "  image size: %dx%d\n", mi->width, mi->height);
    fprintf(stderr, "  left pad: %d\n", mi->left_pad);
    fprintf(stderr, "  right pad: %d\n", mi->right_pad);
    fprintf(stderr, "  h_off: %ld\n", mi->h_off);
    fprintf(stderr, "  A: off:%ld, size:%ld\n", mi->a_off, mi->a_size);
    fprintf(stderr, "  B: off:%ld, size:%ld\n", mi->b_off, mi->b_size);
    fprintf(stderr, "  P: off:%ld, size:%ld\n", mi->p_off, mi->p_size);
}

/* Memory related routines. */
static void *mag_malloc(n, fn)
    size_t n;
    char *fn;
{
    void *r = (void *) malloc(n);
    if(r == NULL)
	mag_memory_error("malloc", fn);
    return r;
}

static void *mag_realloc(p, n, fn)
    void *p;
    size_t n;
    char *fn;
{
    void *r = (p == NULL) ? (void *) malloc(n) : (void *) realloc(p, n);
    if(r == NULL)
	mag_memory_error("realloc", fn);
    return r;
}
#endif /* HAVE_MAG */
