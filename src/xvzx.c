/*
 * xvzx.c - load routine for Spectrum screen$
 *
 * John Elliott, 7 August 1998
 *
 * LoadZX(fname, pinfo)  -  load file
 * WriteZX(fp,pic,ptype,w,h,r,g,b,numcols,style,cmt,comment) - convert to
 *                          256x192 SCREEN$ and save.
 */

#include "copyright.h"

#include "xv.h"



/*
 * comments on error handling:
 * a file with a bad header checksum is a warning error.
 *
 * not being able to malloc is a Fatal Error.  The program is aborted.
 */


#define TRUNCSTR "File appears to be truncated."

static int zxError PARM((const char *, const char *));

static const char *bname;

/*******************************************/
int LoadZX(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*******************************************/
{
  /* returns '1' on success */

  FILE  *fp;
  unsigned int    c, c1;
  int   x,y, trunc;
  byte  *zxfile;

  bname = BaseName(fname);

  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;

  /* Allocate memory for a 256x192x8bit image */

  pinfo->pic     = (byte *)malloc(256*192);
  if (!pinfo->pic) FatalError("malloc failure in xvzx.c LoadZX");

  /* Allocate 1B80h bytes and slurp the whole file into memory */

  zxfile         = (byte *)malloc(7040);
  if (!zxfile)     FatalError("malloc failure in xvzx.c LoadZX");

  /* open the file */
  fp = xv_fopen(fname,"r");
  if (!fp) return (zxError(bname, "can't open file"));

  /* Load it in en bloc */
  memset(zxfile, 0, 7040);
  if (fread(zxfile, 1, 7040, fp) < 7040) trunc = 1;

  /* Transform to 8-bit */

  for (y = 0; y < 192; y++) for (x = 0; x < 256; x++)
  {
     /* Spectrum screen layout: three 2k segments at y=0, y=64, y=128 */
     /* In each segment: Scan lines 0,8,16,...,56,1,9,...,57 etc.  Each
        scanline is 32 bytes, so line 1 is 256 bytes after line 0

        So address of line start is ((y>>6) * 2048) + ((y & 7) * 256)
        + ((y & 0x38) * 4)

       The colour byte for a cell is at screen + 6k + (y >> 3)*32 + (x>>3)

       */

     int offset;
     byte *dst  = pinfo->pic + 256*y + x;
     byte attr, pt, mask;

     offset  = (y >> 6)   * 2048;
     offset += (y & 7)    * 256;
     offset += (y & 0x38) * 4;
     offset += (x >> 3);

     pt = zxfile[offset + 128];	/* Ink/paper map */

     offset = 0x1880;
     offset += (y >> 3) * 32;
     offset += (x >> 3);

     attr = zxfile[offset]; 	/* Colours for cell */

     mask = 0x80;

     if (x & 7) mask >>= (x & 7);

     if (pt & mask)  *dst = attr & 7;        /* Ink */
     else            *dst = (attr >> 3) & 7; /* Paper */

     if (attr & 0x40) *dst |= 8; /* High intensity */
  }

  /* Picture bytes converted; now build the colour maps */

  pinfo->normw = pinfo->w = 256;
  pinfo->normh = pinfo->h = 192;
  pinfo->type = PIC8;

  for (c = 0; c < 16; c++)
  {
    if (c < 8) c1 = 192; else c1 = 255;	/* low-intensity colours use 192 */
					/* high-intensity colours use 255 */
    pinfo->b[c] = (c & 1 ? c1 : 0);
    pinfo->r[c] = (c & 2 ? c1 : 0);
    pinfo->g[c] = (c & 4 ? c1 : 0);
  }

  pinfo->colType = F_FULLCOLOR;
  pinfo->frmType = F_ZX;		/* Save as SCREEN$ */
  sprintf(pinfo->fullInfo, "Spectrum SCREEN$, load address %04x",
                                 zxfile[16]+256*zxfile[17]);
  strcpy(pinfo->shrtInfo, "Spectrum SCREEN$.");

  /* Almost as an afterthought, check that the +3DOS header is valid.

     If it isn't, then odds are that the file isn't a graphic. But it
     had the right magic number, so it might be. Let them see it anyway.

     The check is: Byte 127 of the header should be the 8-bit sum of bytes
                   0-126 of the header. The header should also have the
                   +3DOS magic number, but we know it does or we wouldn't
                   have got this far.
  */

  c1 = 0;
  for (c1 = c = 0; c < 127; c++) c1 = ((c1 + zxfile[c]) & 0xFF);
  if (c1 != zxfile[127]) zxError(bname, "Bad header checksum.");

  fclose(fp);
  free(zxfile);
  return 1;
}





/*******************************************/
static int zxError(fname, st)
     const char *fname, *st;
{
  SetISTR(ISTR_WARNING,"%s:  %s", fname, st);
  return 0;
}


/* Spectrum screen file header. The first 18 bytes are used in the magic
   number test */

byte ZXheader[128] =
{
    'P', 'L', 'U', 'S', '3', 'D', 'O', 'S', 26,	 /* Spectrum +3DOS file */
      1,   0, 					 /* Header type 1.0 */
    128,  27,  0,   0,				 /* 7040 bytes */
      3,	                                 /* Binary format */
      0, 27,   					 /* 6912 data bytes */
      0, 64					 /* load address 0x4000 */
};



/* Get the Spectrum colour/bright byte (0-15) from a pixel */

static int PointZX(pic, w, h, rmap, gmap, bmap, x, y)
	byte *pic;
	int w,h;
	byte *rmap, *gmap, *bmap;
	int x,y;
{
	int index, r, g, b, zxc;

	/* If the picture is smaller than the screen, pad out the edges
           with "bright black" - a colour not otherwise returned */

	if (x >= w || y >= h) return 8;

	/* Get colour index */

	index = pic[y*w + x];

	/* Convert to rgb */

	r = rmap[index];
	g = gmap[index];
	b = bmap[index];
	zxc = 0;

	/* Work out Spectrum colour by a simplistic "nearest colour" method */

	if (b >= 160) zxc |= 1;	/* Blue */
	if (r >= 160) zxc |= 2;	/* Red */
	if (g >= 160) zxc |= 4;	/* Green */
	if (r > 208 || g >= 208 || b >= 208) zxc |= 8; /* High intensity */

	return zxc;
}


/* Work out what colours should be used in a cell */

static void CellZX(pic, w, h, rmap, gmap, bmap, cx, cy, zxfile)
        byte *pic;
        int w,h;
        byte *rmap, *gmap, *bmap;
        int cx,cy;
	byte *zxfile;
{
	byte counts[16];	/* Count of no. of colours */
	int offset, ink, paper, n, m, x, y, x0, y0, di, dp;

	x0 = cx * 8;		/* Convert from cell to pixel coords */
	y0 = cy * 8;

	for (n = 0; n < 16; n++) counts[n] = 0;	 /* Reset all counts */

	/* Count no. of pixels of various colours */

	for (y = y0; y < y0+8; y++) for (x = x0; x < x0+8; x++)
	{
		m = PointZX(pic, w, h, rmap, gmap, bmap, x, y);

		counts[m]++;
	}
	counts[8] = 0;	/* Discard Bright Black (pixels not in the picture area)
                         */

	/* Assign the most popular colour as ink */
	for (n = m = ink = 0; n < 16; n++) if (counts[n] > m)
	{
		ink = n;
		m   = counts[n];
	}
	counts[ink] = 0;

	/* Assign the next most popular colour as paper */
	for (n = m = paper = 0; n < 16; n++) if (counts[n] > m)
	{
		paper = n;
		m     = counts[n];
	}
	/* We have ink and paper. Set cell's attributes */

	offset = cy*32 + cx + 0x1880;

	/* Set the high-intensity bit if ink is high-intensity */
	if (ink & 8) zxfile[offset] = 0x40; else zxfile[offset] = 0;
	zxfile[offset] |= ((paper & 7) << 3);
	zxfile[offset] |=  (ink & 7);

	/* Plot the points */
	for (y = y0; y < y0+8; y++)
	{
	     byte mask = 0x80;

	     offset  = (y >> 6)   * 2048;
	     offset += (y & 7)    * 256;
	     offset += (y & 0x38) * 4;
	     offset += (x0 >> 3);

	     for (x = x0; x < x0+8; x++)
	     {
		/* Work out whether the point should be plotted as ink or
                   paper */
		m = PointZX(pic, w, h, rmap, gmap, bmap, x, y);

		di = (ink & 7)   - (m & 7);	/* "Difference" from ink */
		dp = (paper & 7) - (m & 7);	/* "Difference" from paper */

		if (di < 0) di = -di;
		if (dp < 0) dp = -dp;

		if (di < dp)	/* Point is more like ink */
			zxfile[offset+128] |= mask;

		mask = (mask >> 1);
             }
	}

}



/*******************************************/
int WriteZX(fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,comment)
     FILE *fp;
     byte *pic;
     int   ptype, w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
     char *comment;
{
	int rv, x, y;
	byte *zxfile;
	byte *pic8;
	byte  rtemp[256],gtemp[256],btemp[256];

	/* To simplify matters, reduce 24-bit to 8-bit. Since the Spectrum
           screen is 3.5-bit anyway, it doesn't make much difference */

	if (ptype == PIC24)
	{
		pic8 = Conv24to8(pic, w, h, 256, rtemp,gtemp,btemp);
		if (!pic8) FatalError("Unable to malloc in WriteZX()");
		rmap = rtemp;  gmap = gtemp;  bmap = btemp;  numcols=256;
  	}
	else pic8 = pic;

	ZXheader[127] = 0x71;	/* The correct checksum. */

	/* Create a memory image of the SCREEN$ */

	zxfile         = (byte *)malloc(7040);
	if (!zxfile)     FatalError("malloc failure in xvzx.c WriteZX");

	memset(zxfile, 0, 7040);	/* Reset all points to black */
	memcpy(zxfile, ZXheader, 128);	/* Create +3DOS header */

        /* Convert the image, character cell by character cell */
        for (y = 0; y < 24; y++) for (x = 0; x < 32; x++)
        {
                CellZX(pic8, w, h, rmap, gmap, bmap, x, y, zxfile);
        }
	rv = 0;
	if (fwrite(zxfile, 1, 7040, fp) < 7040) rv = -1;

	if (ptype == PIC24) free(pic8);
	free(zxfile);

	if (ferror(fp)) rv = -1;

	return rv;
}

