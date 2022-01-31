/*
 * xvwbmp.c - i/o routings for WBMP files
 * defined by OMA (http://www.openmobilealliance.com)
 * as a standard for images for micro devices.
 *
 * exports :
 *
 * LoadWBMP(fname, numcols);
 * WriteWBMP(fp, pic, ptype, w, h, r, g, b, numcols, style);
 *
 * author: Pawel S. Veselov <vps@manticore.2y.net>
 *	      http://manticore.2y.net/
 *
 */

#include "xv.h"

typedef	short int16;
typedef unsigned char uint8;
typedef	unsigned short uint16;	/* sizeof (uint16) must == 2 */
#if defined(__alpha) || _MIPS_SZLONG == 64
typedef	int int32;
typedef	unsigned int uint32;	/* sizeof (uint32) must == 4 */
#else
typedef	long int32;
typedef	unsigned long uint32;	/* sizeof (uint32) must == 4 */
#endif

#define MUST(a)	            if (!(a)) {\
				close(fd); \
				return fail(st_fname, st_err); }
#define READU8(fd,u)	    if ((read(fd, &u, 1)<1)) {\
				myfree(); \
				close(fd); \
				return fail(st_fname, err_ueof); }
#define SREADU8(fd, u)	    if ((read(fd, &u, 1,)<1)) {\
				{ st_err = err_ueof; return 0; }

#define SREADC(fd, str, l)  {	\
    str = (char*)mymalloc(l);	\
    if (!str) {			\
	myfree();		\
	FatalError("LoadWBMP: can't malloc extension buffer");	\
    }				\
    if (read(fd, str, l)<l) {	\
	st_err = err_ueof;	\
	return 0;		\
    }

static const char err_ueof[] = "Unexpected EOF";
static const char err_unst[] = "Unsupported image type";
static const char err_extf[] = "Extensions are forbidden";
static const char err_inmb[] = "Invalid multibyte integer";

static const char *st_fname;
static const char *st_err;

static int    fail	PARM((const char *, const char *));
static int    read_mb	PARM((int *, int));
static void   write_mb	PARM((uint32, FILE *));
static int    read_ext	PARM((int, uint8));
static void  *mymalloc	PARM((int));
static void   myfree	PARM((void));
static uint8 *render1	PARM((uint8 *, int, int));

static void **mymem = NULL;
static int    mymems = 0;


int LoadWBMP(fname, pinfo)
     char *fname;
     PICINFO *pinfo;
{
    int fd;
    int im_type;	/* image type (only type 0 supported) */
    uint8 fix_header;	/* fixed header field */
    int width, height;
    int npixels, raw_size, aux;
    uint8 * raw;

    st_fname = fname;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
	return fail(fname, "Couldn't open the file");
    }

    MUST(read_mb(&im_type, fd));
    if (im_type) {
	return fail(fname, err_unst);
    }

    READU8(fd, fix_header);

    MUST(read_ext(fd, fix_header));

    MUST(read_mb(&width, fd));
    MUST(read_mb(&height, fd));

    npixels = width * height;
    raw_size = (npixels+7) / 8;
    if (width <= 0 || height <= 0 || npixels/width != height ||
        npixels+7 < npixels)
    {
	return fail(fname, "image dimensions out of range");
    }

    raw = mymalloc(raw_size);
    if (!raw) {
	myfree();
	FatalError("LoadWBMP: can't malloc image buffer");
    }

    aux = read(fd, raw, raw_size);
    if (aux < raw_size) {
	fail(fname, "Image size shrank");
	raw_size = aux;
    }

    pinfo->r[0] = 0;
    pinfo->g[0] = 0;
    pinfo->b[0] = 0;
    pinfo->r[1] = 255;
    pinfo->g[1] = 255;
    pinfo->b[1] = 255;

    pinfo->pic = render1(raw, raw_size, npixels);
    pinfo->type = PIC8;

    pinfo->w = pinfo->normw = width;
    pinfo->h = pinfo->normh = height;
    pinfo->frmType = F_BWDITHER;

    sprintf(pinfo->fullInfo, "WBMP, 1 bit per pixel, %d bytes", raw_size);
    sprintf(pinfo->shrtInfo, "%dx%d WBMP (WAP/OMA).", width, height);
    pinfo->comment = (char*)NULL;

    close(fd);

    myfree();
    return 1;
}


int WriteWBMP(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle)
     FILE *fp;
     byte *pic;
     int ptype, w, h;
     byte *rmap, *gmap, *bmap;
     int numcols, colorstyle;
{
    int count = 0;
    uint8 bit = 0;
    int i;

    write_mb(0, fp);	/* type : always 0 */
    putc(0, fp);	/* fixed header : always 0 for type 0 */
    write_mb((uint32)w, fp);
    write_mb((uint32)h, fp);

    /* ready to write data */

    for (i=0; i<w*h; i++) {
	bit |= (((pic[i]&1)<<(7-(count++))));
	if (count == 8) {
	    putc(bit, fp);
	    count = 0;
	}
    }

    if (!count) {
	putc(bit, fp);
    }

    return 0;
}


static int fail(name, msg)
     const char *name, *msg;
{
    SetISTR(ISTR_WARNING, "%s : %s", name, msg);
    return 0;
}


static void write_mb(data, f)
     uint32 data;
     FILE *f;
{
    int i = 32;
    uint32 aux = data;
    int no;

    if (!aux) {
	i = 1;
    } else {
	while (!(aux & 0x80000000)) {
	    aux <<= 1;
	    i--;
	}
    }

    /* i tells us how many bits are left to encode */

    no = (i / 7 + ((i % 7)?1:0))-1;

    /*
    fprintf(stderr, "writing %x, bits to write=%d, passes=%d\n",
	    data, i, no);
    */

    do {
	uint8 value = no?0x80:0x0;
	value |= ((data >> (no*7)) & 0x7f);
	putc(value, f);
    } while ((no--)>0);

}


static int read_mb(dst, fd)
     int *dst, fd;
{
    int ac = 0;
    int ct = 0;

    while (1) {
	uint8 bt;
	if ((ct++)==6) {
	    st_err = err_inmb;
	    return 0;
	}

	if ((read(fd, &bt, 1)) < 1) {
	    st_err = err_ueof;
	    return 0;
	}
	ac = (ac << 7) | (bt & 0x7f);   /* accumulates up to 42 bits?? FIXME */
	if (!(bt & 0x80))
	    break;
    }
    *dst = ac;
    return 1;
}


static int read_ext(fd, fixed)
     int fd;
     uint8 fixed;
{
    if (!(fixed&0x7f)) {    /* no extensions */
	return 1;
    }

    /*
     * The only described type is WBMP 0, that must not
     * have extensions.
     */

    st_err = err_extf;
    return 0;

    /*

    fixed = (fixed >> 5)&0x3;

    switch (fixed) {
    case 0:
	while (true) {
	    SREADU8(fd, fixed);
	    if (!(fixed & 0x7f)) { break; }
	}
	break;
    case 0x3:
	{
	    char * par;
	    char * val;
	    SREADU8(fd, fixed);
	    SREADC(fd, par, (fixed>>4)&0x6);
	    SREADC(fd, val, fixed&0xf);
	}
	break;
    }
    */
}


static void *mymalloc(numbytes)
     int numbytes;
{
    mymem = (void**)realloc(mymem, mymems+1);
    if (!mymem)
	FatalError("LoadWBMP: can't realloc buffer");
    return (mymem[mymems++] = malloc(numbytes));
}


static void myfree()
{
    int i;

    if (mymem) {
	for (i=0; i<mymems; i++) {
	    if (mymem[i])
	        free(mymem[i]);
	}
	free(mymem);
    }
    mymem = (void**)NULL;
    mymems = 0;
}


static uint8 *render1(data, size, npixels)
     uint8 *data;
     int size, npixels;
{
    byte * pic;
    int i;
    int cnt = 0;
    uint8 cb = *data;

    pic = calloc(npixels,1);   /* checked for overflow by caller */
    if (!pic) {
	myfree();
	FatalError("LoadWBMP: can't allocate 'pic' buffer");
    }

    /* expand bits into bytes */
    /* memset(pic, 0, npixels); */

    for (i=0; i<npixels; i++) {

	pic[i] = (cb>>7)&1;

	if ((++cnt)==8) {
	    cb = *(++data);
	    cnt = 0;
	} else {
	    cb <<=1;
	}
    }
    return pic;
}
