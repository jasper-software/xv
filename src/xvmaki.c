/*
 * xvmaki.c - load routine for `MAKI' format pictures.
 *
 * The `MAKI' format was used by some Japanese personal computer users.
 */

#include "xv.h"
#include <setjmp.h>

#ifdef HAVE_MAKI

typedef unsigned short data16;
typedef unsigned int data32;

struct maki_info {
    jmp_buf jmp;
    FILE *fp;
    long fsize;
    int x0, y0, x1, y1;
    int width, height;
    float aspect;
    long fb_size;
    long pa_size, pb_size;
    int m_maki01b, m_200, m_dig8;
    data16 ext_flag;
    byte *fa, *fb, *pa, *pb;
    byte *vs;
    int numcols;
    byte *forma, *formb;
};


static void maki_open_file             PARM((struct maki_info*, char*));
static void maki_check_id              PARM((struct maki_info*));
static void maki_skip_comment          PARM((struct maki_info*));
static void maki_read_header           PARM((struct maki_info*));
static void maki_read_palette          PARM((struct maki_info*,
					     byte*, byte*, byte*));
static void maki_read_flags            PARM((struct maki_info*));
static void maki_read_pixel_data       PARM((struct maki_info*));
static void maki_expand_virtual_screen PARM((struct maki_info*));
static void maki_expand_pixel_data     PARM((struct maki_info*, byte**));
static void maki_init_info             PARM((struct maki_info*));

static void maki_make_pixel_data       PARM((struct maki_info*, byte*));
static void maki_make_virtual_screen   PARM((struct maki_info*));
static void maki_make_flags            PARM((struct maki_info*));
static void maki_write_check_id        PARM((struct maki_info*));
static void maki_write_comment         PARM((struct maki_info*));
static void maki_write_header          PARM((struct maki_info*));
static void maki_write_palette         PARM((struct maki_info*,
					     byte*, byte*, byte*, int));
static void maki_write_flags           PARM((struct maki_info*));
static void maki_write_pixel_data      PARM((struct maki_info*));

static void maki_cleanup_maki_info     PARM((struct maki_info*, int));
static void maki_cleanup_pinfo         PARM((PICINFO*));
static void maki_memory_error          PARM((char*, char*));
static void maki_error                 PARM((struct maki_info*, int));
static void maki_file_error            PARM((struct maki_info*, int));
static void maki_file_warning          PARM((struct maki_info*, int));
static void maki_show_maki_info        PARM((struct maki_info*));
static void *maki_malloc               PARM((size_t, char*));
static void *maki_realloc              PARM((void *, size_t, char*));

static char maki_id_a[] = "MAKI01A ";
static char maki_id_b[] = "MAKI01B ";

static char *maki_msgs[] = {
    NULL,
#define MAKI_OPEN 1
    "can't open file.",
#define MAKI_CORRUPT 2
    "file corrupted.",
#define MAKI_FORMAT 3
    "not MAKI format.",
#define MAKI_BAD_DATA 4
    "bad data.",
#define MAKI_COMMENT 5
    "no '^Z' after comment.",
#define MAKI_SIZE 6
    "bad size.",
#define MAKI_WRITE 7
    "write failed.",
};

#define H4(b) ((b) >> 4 & 0xf)
#define L4(b) ((b)      & 0xf)
#define error(msg_num) longjmp(mi->jmp, msg_num)

int LoadMAKI(fname, pinfo)
    char *fname;
    PICINFO *pinfo;
{
    struct maki_info maki;
    int e;

    if(DEBUG) fputs("LoadMAKI:\n", stderr);

    pinfo->comment = NULL;
    maki_init_info(&maki);
    if((e = setjmp(maki.jmp)) != 0){
	/* When an error occurs, comes here. */
	maki_cleanup_maki_info(&maki, 0);
	maki_cleanup_pinfo(pinfo);
	return 0;
    }

    maki_open_file(&maki, fname);
    maki_check_id(&maki);
    maki_skip_comment(&maki);
    maki_read_header(&maki);
    maki_read_palette(&maki, pinfo->r, pinfo->g, pinfo->b);
    maki_read_flags(&maki);
    maki_read_pixel_data(&maki);
    maki_expand_virtual_screen(&maki);
    maki_expand_pixel_data(&maki, &pinfo->pic);

    pinfo->w = pinfo->normw = maki.width;
    pinfo->h = pinfo->normh = maki.height;
    pinfo->type = PIC8;
    pinfo->frmType = F_MAKI;
    pinfo->colType = F_FULLCOLOR;
    sprintf(pinfo->fullInfo, "MAKI, 16 colors (%ld bytes)", maki.fsize);
    sprintf(pinfo->shrtInfo, "%dx%d MAKI", maki.width, maki.height);
    normaspect = maki.aspect;

    maki_cleanup_maki_info(&maki, 0);
    return 1;
}

static void maki_open_file(mi, fname)
    struct maki_info *mi;
    char *fname;
{
    if((mi->fp = fopen(fname, "rb")) == NULL)
	maki_file_error(mi, MAKI_OPEN);
    fseek(mi->fp, (size_t) 0, SEEK_END);
    mi->fsize = ftell(mi->fp);
    fseek(mi->fp, (size_t) 0, SEEK_SET);
}

static void maki_check_id(mi)
    struct maki_info *mi;
{
    char buf[8];
    if(fread(buf, (size_t) 8, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_CORRUPT);
    if(strncmp(buf, maki_id_a, (size_t) 8) != 0 &&
       strncmp(buf, maki_id_b, (size_t) 8) != 0)
	maki_error(mi, MAKI_FORMAT);
    mi->m_maki01b = (buf[6] == 'B');
}

static void maki_skip_comment(mi)
    struct maki_info *mi;
{
    int i;
    int c;

    for(i = 0; i < 24; i++){
	if((c = fgetc(mi->fp)) == EOF)
	    maki_file_error(mi, MAKI_CORRUPT);
	if(c == '\032')	/* ^Z, 0x1a */
	    break;
    }
    if(c != '\032')
	maki_file_error(mi, MAKI_COMMENT);

    fseek(mi->fp, 32L, SEEK_SET);
}

static void maki_read_header(mi)
    struct maki_info *mi;
{
    byte buf[16];

    if(fread(buf, (size_t) 16, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_CORRUPT);

    mi->fb_size  = (long)buf[ 0] << 8 | (long)buf[ 1];
    mi->pa_size  = (long)buf[ 2] << 8 | (long)buf[ 3];
    mi->pb_size  = (long)buf[ 4] << 8 | (long)buf[ 5];
    mi->ext_flag = (long)buf[ 6] << 8 | (long)buf[ 7];
    mi->x0       = (long)buf[ 8] << 8 | (long)buf[ 9];
    mi->y0       = (long)buf[10] << 8 | (long)buf[11];
    mi->x1       = (long)buf[12] << 8 | (long)buf[13];
    mi->y1       = (long)buf[14] << 8 | (long)buf[15];

    mi->width  = mi->x1-- - mi->x0;
    mi->height = mi->y1-- - mi->y0;
    mi->m_200  = mi->ext_flag & 1;
    mi->m_dig8 = mi->ext_flag & 2;
    mi->aspect = mi->m_200 ? 0.5 : 1.0;

    if(DEBUG) maki_show_maki_info(mi);
}

static void maki_read_palette(mi, r, g, b)
    struct maki_info *mi;
    byte *r, *g, *b;
{
    byte buf[48], *p;

    if(fread(buf, (size_t) 48, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_CORRUPT);

    for(p = buf; p < &buf[48]; ){
	*g++ = *p++;
	*r++ = *p++;
	*b++ = *p++;
    }
}

static void maki_read_flags(mi)
    struct maki_info *mi;
{
    mi->fa = maki_malloc((size_t) 1000       , "maki_read_flags#1");
    mi->fb = maki_malloc((size_t) mi->fb_size, "maki_read_flags#2");

    if(fread(mi->fa, (size_t) 1000, (size_t) 1, mi->fp) != 1)
	maki_file_warning(mi, MAKI_CORRUPT);
    if(fread(mi->fb, (size_t) mi->fb_size, (size_t) 1, mi->fp) != 1)
	maki_file_warning(mi, MAKI_CORRUPT);
}

static void maki_read_pixel_data(mi)
    struct maki_info *mi;
{
    mi->pa = maki_malloc((size_t) mi->pa_size, "maki_read_pixel_data#1");
    mi->pb = maki_malloc((size_t) mi->pb_size, "maki_read_pixel_data#2");

    if(fread(mi->pa, (size_t) mi->pa_size, (size_t) 1, mi->fp) != 1)
	maki_file_warning(mi, MAKI_CORRUPT);
    if(fread(mi->pb, (size_t) mi->pb_size, (size_t) 1, mi->fp) != 1)
	maki_file_warning(mi, MAKI_CORRUPT);
}

static void maki_expand_virtual_screen(mi)
    struct maki_info *mi;
{
    int x, y, fai, fbi;
    int bpl = mi->width / 2 / 8;		/* bytes per line */
    byte mask;
    mi->vs = maki_malloc((size_t) bpl * mi->height,  // GRR POSSIBLE OVERFLOW / FIXME
			 "maki_expand_virtual_screen");

    fai = fbi = 0;
    mask = 0x80;
    for(y = 0; y < mi->height; y += 4){
	for(x = 0; x < mi->width / 2; x += 4){
	    if(mi->fa[fai] & mask){
		byte bh, bl;
		bh = mi->fb[fbi++];
		bl = mi->fb[fbi++];
		if(x % 8 == 0){
		    mi->vs[ y      * bpl + x / 8] = H4(bh) << 4;
		    mi->vs[(y + 1) * bpl + x / 8] = L4(bh) << 4;
		    mi->vs[(y + 2) * bpl + x / 8] = H4(bl) << 4;
		    mi->vs[(y + 3) * bpl + x / 8] = L4(bl) << 4;
		}else{
		    mi->vs[ y      * bpl + x / 8] |= H4(bh);
		    mi->vs[(y + 1) * bpl + x / 8] |= L4(bh);
		    mi->vs[(y + 2) * bpl + x / 8] |= H4(bl);
		    mi->vs[(y + 3) * bpl + x / 8] |= L4(bl);
		}
	    }else{
		if(x % 8 == 0){
		    mi->vs[ y      * bpl + x / 8] = 0;
		    mi->vs[(y + 1) * bpl + x / 8] = 0;
		    mi->vs[(y + 2) * bpl + x / 8] = 0;
		    mi->vs[(y + 3) * bpl + x / 8] = 0;
		}else{
/*		    mi->vs[ y      * bpl + x / 8] |= 0;
		    mi->vs[(y + 1) * bpl + x / 8] |= 0;
		    mi->vs[(y + 2) * bpl + x / 8] |= 0;
		    mi->vs[(y + 3) * bpl + x / 8] |= 0; */
		}
	    }

	    if((mask >>= 1) == 0){
		mask = 0x80;
		fai++;
	    }
	}
    }
}

static void maki_expand_pixel_data(mi, pic)
    struct maki_info *mi;
    byte **pic;
{
    int x, y;
    int vsi, pi, max_pi;
    byte *p;
    byte mask;
    int gap;
    *pic = maki_malloc((size_t) mi->width * mi->height,  // GRR POSSIBLE OVERFLOW / FIXME
		       "maki_expand_pixel_data");

    vsi = pi = 0;
    p = mi->pa;
    max_pi = mi->pa_size - 1;
    mask = 0x80;
    for(y = 0; y < mi->height; y++){
	for(x = 0; x < mi->width; x += 2){
	    if(mi->vs[vsi] & mask){
		if(pi > max_pi){
		    if(p == mi->pb)
			maki_error(mi, MAKI_BAD_DATA);
		    pi = 0;
		    p = mi->pb;
		    max_pi = mi->pb_size - 1;
		}
		(*pic)[y * mi->width + x    ] = H4(p[pi]);
		(*pic)[y * mi->width + x + 1] = L4(p[pi]);
		pi++;
	    }else{
		(*pic)[y * mi->width + x    ] = 0;
		(*pic)[y * mi->width + x + 1] = 0;
	    }

	    if((mask >>= 1) == 0){
		mask = 0x80;
		vsi++;
	    }
	}
    }

    gap = mi->m_maki01b ? 4 : 2;

    for(y = gap; y < mi->height; y++){
	for(x = 0; x < mi->width; x++)
	    (*pic)[y * mi->width + x] ^= (*pic)[(y - gap) * mi->width + x];
    }
}


int WriteMAKI(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle)
    FILE *fp;
    byte *pic;
    int ptype, w, h;
    byte *rmap, *gmap, *bmap;
    int numcols, colorstyle;
{
    byte rtemp[256], gtemp[256], btemp[256];
    struct maki_info maki, *mi = &maki;
    int e;

    if(DEBUG) fputs("WriteMAKI:\n", stderr);

    maki_init_info(&maki);
    if((e = setjmp(maki.jmp)) != 0){
	/* An error occurs */
	maki_cleanup_maki_info(&maki, 1);
	return -1;
    }

    if(w != 640 || h != 400) {
        char  str[512];
        sprintf(str,"MAKI: %s Should be 640x400", maki_msgs[MAKI_SIZE]);
	ErrPopUp(str, "\nBummer!");
	maki_error(mi, MAKI_SIZE);
    }

    maki.fp = fp;
    maki.width = w;
    maki.height = h;
    maki.x1 = w - 1;
    maki.y1 = h - 1;

    if(ptype == PIC24){
	if(!(pic = Conv24to8(pic, w, h, 16, rtemp, gtemp, btemp)))
	    maki_memory_error("Conv24to8#1", "WriteMAKI");
	rmap = rtemp;
	gmap = gtemp;
	bmap = btemp;
    }else if(numcols > 16){
	if(!(pic = Conv8to24(pic, w, h, rmap, gmap, bmap)))
	    maki_memory_error("Conv8to24", "WriteMAKI");
	if(!(pic = Conv24to8(pic, w, h, 16, rtemp, gtemp, btemp)))
	    maki_memory_error("Conv24to8#2", "WriteMAKI");
	rmap = rtemp;
	gmap = gtemp;
	bmap = btemp;
    }else
	maki.numcols = numcols;

    maki_make_pixel_data(&maki, pic);
    maki_make_virtual_screen(&maki);
    maki_make_flags(&maki);
    maki_write_check_id(&maki);
    maki_write_comment(&maki);
    maki_write_header(&maki);
    maki_write_palette(&maki, rmap, gmap, bmap, colorstyle == F_GREYSCALE);
    maki_write_flags(&maki);
    maki_write_pixel_data(&maki);

    maki_cleanup_maki_info(&maki, 1);
    return 0;
}

static void maki_make_pixel_data(mi, pic)
    struct maki_info *mi;
    byte *pic;
{
    int x, y, i;
    int nza, nzb;

    mi->forma = maki_malloc((size_t) mi->width / 2 * mi->height,  // GRR POSSIBLE OVERFLOW / FIXME
			    "maki_make_pixel_data#1");
    mi->formb = maki_malloc((size_t) mi->width / 2 * mi->height,  // GRR POSSIBLE OVERFLOW / FIXME
			    "maki_make_pixel_data#2");

    for(y = 0; y < mi->height; y++){
	for(x = 0; x < mi->width; x += 2){
	    byte b;
	    b = pic[y * mi->width + x] << 4 | pic[y * mi->width + x + 1];
	    mi->forma[y * mi->width / 2 + x / 2] = b;
	    mi->formb[y * mi->width / 2 + x / 2] = b;
	}
    }

    for(y = mi->height - 1; y >= 2; y--){
	for(x = 0; x < mi->width / 2; x++){
	    mi->forma[y * mi->width / 2 + x] ^=
		mi->forma[(y - 2) * mi->width / 2 + x];
	}
    }

    for(y = mi->height - 1; y >= 4; y--){
	for(x = 0; x < mi->width / 2; x++){
	    mi->formb[y * mi->width / 2 + x] ^=
		mi->formb[(y - 4) * mi->width / 2 + x];
	}
    }

    nza = nzb = 0;
    for(i = 0; i < mi->width / 2 * mi->height; i++){
	if(mi->forma[i] != 0)
	    nza++;
	if(mi->formb[i] != 0)
	    nzb++;
    }
    if(nza > nzb){
	mi->m_maki01b = 1;
	free(mi->forma);
	mi->forma = NULL;
    }else{
	mi->m_maki01b = 0;
	free(mi->formb);
	mi->formb = NULL;
    }
}

static void maki_make_virtual_screen(mi)
    struct maki_info *mi;
{
    int bpl = mi->width / 2 / 8;
    int vsi, pai, pbi, max_pai, max_pbi;
    byte mask;
    byte *pixels;
    int x, y;

    mi->vs = maki_malloc((size_t) bpl * mi->height,  // GRR POSSIBLE OVERFLOW / FIXME
			 "maki_make_virtual_screen#1");

    if(mi->m_maki01b)
	pixels = mi->formb;
    else
	pixels = mi->forma;

    vsi = pai = pbi = 0;
    max_pai = max_pbi = -1;
    mask = 0x80;
    for(y = 0; y < mi->height; y++){
	for(x = 0; x < mi->width / 2; x++){
	    if(pixels[y * mi->width / 2 + x] == 0){
		mi->vs[vsi] &= ~mask;
	    }else{
		mi->vs[vsi] |= mask;
		if(y < 200){
		    if(pai > max_pai){
			max_pai += 1024;
			mi->pa = maki_realloc(mi->pa, (size_t) max_pai + 1,
					      "maki_make_virtual_screen#2");
		    }
		    mi->pa[pai++] = pixels[y * mi->width / 2 + x];
		}else{
		    if(pbi > max_pbi){
			max_pbi += 1024;
			mi->pb = maki_realloc(mi->pb, (size_t) max_pbi + 2,
					      "maki_make_virtual_screen#3");
		    }
		    mi->pb[pbi++] = pixels[y * mi->width / 2 + x];
		}
	    }

	    if((mask >>= 1) == 0){
		mask = 0x80;
		vsi++;
	    }
	}
    }

    mi->pa_size = pai;
    mi->pb_size = pbi;
}

static void maki_make_flags(mi)
    struct maki_info *mi;
{
    int bpl = mi->width / 2 / 8;
    int fbi, max_fbi;
    int fai;
    int x, y;
    byte mask;

    mi->fa = maki_malloc((size_t) bpl * mi->height, "maki_make_flags#1");  // GRR POSSIBLE OVERFLOW / FIXME

    fbi = fai = 0;
    max_fbi = -1;
    mask = 0x80;
    for(y = 0; y < mi->height; y += 4){
	for(x = 0; x < mi->width / 2; x += 4){
	    if(x % 8 == 0){
		if(H4(mi->vs[ y      * bpl + x / 8]) == 0 &&
		   H4(mi->vs[(y + 1) * bpl + x / 8]) == 0 &&
		   H4(mi->vs[(y + 2) * bpl + x / 8]) == 0 &&
		   H4(mi->vs[(y + 3) * bpl + x / 8]) == 0){
		    mi->fa[fai] &= ~mask;
		}else{
		    mi->fa[fai] |= mask;
		    if(fbi + 1 > max_fbi){
			max_fbi += 1024;
			mi->fb = maki_realloc(mi->fb, (size_t) max_fbi + 1,
					      "maki_make_flags#2");
		    }
		    mi->fb[fbi++] = H4(mi->vs[ y      * bpl + x / 8]) << 4
				  | H4(mi->vs[(y + 1) * bpl + x / 8]);
		    mi->fb[fbi++] = H4(mi->vs[(y + 2) * bpl + x / 8]) << 4
				  | H4(mi->vs[(y + 3) * bpl + x / 8]);
		}
	    }else{
		if(L4(mi->vs[ y      * bpl + x / 8]) == 0 &&
		   L4(mi->vs[(y + 1) * bpl + x / 8]) == 0 &&
		   L4(mi->vs[(y + 2) * bpl + x / 8]) == 0 &&
		   L4(mi->vs[(y + 3) * bpl + x / 8]) == 0){
		    mi->fa[fai] &= ~mask;
		}else{
		    mi->fa[fai] |= mask;
		    if(fbi + 1 > max_fbi){
			max_fbi += 1024;
			mi->fb = maki_realloc(mi->fb, (size_t) max_fbi + 1,
					      "maki_make_flags#3");
		    }
		    mi->fb[fbi++] = L4(mi->vs[ y      * bpl + x / 8]) << 4
				  | L4(mi->vs[(y + 1) * bpl + x / 8]);
		    mi->fb[fbi++] = L4(mi->vs[(y + 2) * bpl + x / 8]) << 4
				  | L4(mi->vs[(y + 3) * bpl + x / 8]);
		}
	    }

	    if((mask >>= 1) == 0){
		mask = 0x80;
		fai++;
	    }
	}
    }

    mi->fb_size = fbi;
}

static void maki_write_check_id(mi)
    struct maki_info *mi;
{
    char *id = mi->m_maki01b ? maki_id_b : maki_id_a;
    if(fwrite(id, (size_t) 8, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_WRITE);
}

static void maki_write_comment(mi)
    struct maki_info *mi;
{
    char buf[24];
    char *p;
    int i = 0;

    strcpy(buf, "XV   ");

    if((p = (char *) getenv("USER")) == NULL)
	p = "????????";
    for(i = 5; i < 23; i++){
	if(*p == '\0')
	    break;
	buf[i] = *p++;
    }
    for( ; i < 23; i++)
	buf[i] = ' ';

    buf[i] = '\032';	/* ^Z, 0x1a */

    if(fwrite(buf, (size_t) 24, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_WRITE);
}

static void maki_write_header(mi)
    struct maki_info *mi;
{
    byte buf[16];

    if(DEBUG) maki_show_maki_info(mi);

#define set_word(i, v) {buf[i]=(v)>>8&0xff;buf[i+1]=(v)&0xff;}
    set_word(0, mi->fb_size);
    set_word(2, mi->pa_size);
    set_word(4, mi->pb_size);
    set_word(6, mi->ext_flag);
    set_word(8, mi->x0);
    set_word(10, mi->y0);
    set_word(12, mi->x1 + 1);
    set_word(14, mi->y1 + 1);
#undef set_word

    if(fwrite(buf, (size_t) 16, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_WRITE);
}

static void maki_write_palette(mi, r, g, b, grey)
    struct maki_info *mi;
    byte *r, *g, *b;
    int grey;
{
    int i;
    char buf[3];
    for(i = 0; i < mi->numcols; i++){
	buf[0] = *g++;
	buf[1] = *r++;
	buf[2] = *b++;
	if(grey)
	    buf[0] = buf[1] = buf[2] = MONO(buf[1], buf[0], buf[2]);
	if(fwrite(buf, (size_t) 3, (size_t) 1, mi->fp) != 1)
	    maki_file_error(mi, MAKI_WRITE);
    }
    for( ; i < 16; i++){
	if(fwrite(buf, (size_t) 3, (size_t) 1, mi->fp) != 1)
	    maki_file_error(mi, MAKI_WRITE);
    }
}

static void maki_write_flags(mi)
    struct maki_info *mi;
{
    int bpl = mi->width / 2 / 8;
    if(fwrite(mi->fa, (size_t) bpl * mi->height / 16, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_WRITE);

    if(fwrite(mi->fb, (size_t) mi->fb_size, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_WRITE);
}

static void maki_write_pixel_data(mi)
    struct maki_info *mi;
{
    if(fwrite(mi->pa, (size_t) mi->pa_size, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_WRITE);

    if(fwrite(mi->pb, (size_t) mi->pb_size, (size_t) 1, mi->fp) != 1)
	maki_file_error(mi, MAKI_WRITE);
}



static void maki_init_info(mi)
    struct maki_info *mi;
{
    xvbzero((char *)mi, sizeof(struct maki_info));
    mi->fp = NULL;
    mi->fsize = 0;
    mi->x0 = mi->y0 = mi->x1 = mi->y1 = 0;
    mi->width = mi->height = 0;
    mi->aspect = 1.0;
    mi->fb_size = mi->pa_size = mi->pb_size = 0;
    mi->m_maki01b = mi->m_200 = mi->m_dig8 = 0;
    mi->ext_flag = 0;
    mi->fa = mi->fb = mi->pa = mi->pb = NULL;
    mi->vs = NULL;
    mi->numcols = 16;
    mi->forma = mi->formb = NULL;
}

static void maki_cleanup_maki_info(mi, writing)
    struct maki_info *mi;
    int writing;
{
    if(mi->fp && !writing)
	fclose(mi->fp);
    if(mi->fa)
	free(mi->fa);
    if(mi->fb)
	free(mi->fb);
    if(mi->pa)
	free(mi->pa);
    if(mi->pb)
	free(mi->pb);
    if(mi->vs)
	free(mi->vs);
    if(mi->forma)
	free(mi->forma);
    if(mi->formb)
	free(mi->formb);
}

static void maki_cleanup_pinfo(pi)
    PICINFO *pi;
{
    if(pi->pic){
	free(pi->pic);
	pi->pic = NULL;
    }
}

static void maki_memory_error(scm, fn)
    char *scm, *fn;
{
    char buf[128];
    sprintf(buf, "%s: coulndn't allocate memory. (%s)", scm, fn);
    FatalError(buf);
}

static void maki_error(mi, mn)
    struct maki_info *mi;
    int mn;
{
    SetISTR(ISTR_WARNING, "%s", maki_msgs[mn]);
    longjmp(mi->jmp, 1);
}

static void maki_file_error(mi, mn)
    struct maki_info *mi;
    int mn;
{
    if(feof(mi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", maki_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", maki_msgs[mn], ERRSTR(errno));
    longjmp(mi->jmp, 1);
}

static void maki_file_warning(mi, mn)
    struct maki_info *mi;
    int mn;
{
    if(feof(mi->fp))
	SetISTR(ISTR_WARNING, "%s (end of file)", maki_msgs[mn]);
    else
	SetISTR(ISTR_WARNING, "%s (%s)", maki_msgs[mn], ERRSTR(errno));
}

static void maki_show_maki_info(mi)
    struct maki_info *mi;
{
    fprintf(stderr, "  file size: %ld.\n", mi->fsize);
    fprintf(stderr, "  image size: %dx%d.\n", mi->width, mi->height);
    fprintf(stderr, "  aspect: %f.\n", mi->aspect);
    fprintf(stderr, "  flag B size: %ld.\n", mi->fb_size);
    fprintf(stderr, "  pixel data size: A:%ld, B:%ld.\n",
	    mi->pa_size, mi->pb_size);
    fprintf(stderr, "  MAKI01B: %s.\n", mi->m_maki01b ? "true" : "false");
    fprintf(stderr, "  200 line mode: %s.\n", mi->m_200 ? "true" : "false");
    fprintf(stderr, "  digital 8 colors: %s.\n", mi->m_dig8 ? "true" : "false");
}

static void *maki_malloc(n, fn)
    size_t n;
    char *fn;
{
    void *r = (void *) malloc(n);
    if(r == NULL)
	maki_memory_error("malloc", fn);
    return r;
}

static void *maki_realloc(p, n, fn)
    void *p;
    size_t n;
    char *fn;
{
    void *r = (p == NULL) ? (void *) malloc(n) : (void *) realloc(p, n);
    if(r == NULL)
	maki_memory_error("realloc", fn);
    return r;
}
#endif /* HAVE_MAKI */
