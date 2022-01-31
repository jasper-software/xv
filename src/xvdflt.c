/*
 * xvdflt.c - load routine for 'default' XV image
 *
 * LoadDfltPic()  -  loads up 'pic'  note:  can't fail(!)
 */

#include "copyright.h"

#include "xv.h"

#include "bits/logo_top"
#include "bits/logo_bot"
#include "bits/logo_out"
#include "bits/xv_jhb"
#include "bits/xv_cpyrt"
#include "bits/xv_rev"
#include "bits/xv_ver"
#include "bits/xf_left"
/* #include "bits/xf_right"	not used */
#include "bits/font5x9.h"


#ifndef USEOLDPIC
#  include "xvdflt.h"
#endif



#define DWIDE 480
#define DHIGH 270

static void loadOldDfltPic  PARM((PICINFO *));
static void setcolor       PARM((PICINFO *, int, int, int, int));
static void gen_bg         PARM((byte *, PICINFO *));


/*******************************************/
void LoadDfltPic(pinfo)
     PICINFO *pinfo;
{
  char  str[256];
  byte *dfltpic;
  int   i, j, k, xdpline, nbytes;

#ifdef USEOLDPIC

  loadOldDfltPic(pinfo);

#else /* !USEOLDPIC */

  if (!ncols) {
    loadOldDfltPic(pinfo);
    return;
  }

  dfltpic = (byte *) calloc((size_t) XVDFLT_WIDE * XVDFLT_HIGH, (size_t) 1);
  if (!dfltpic) FatalError("couldn't malloc 'dfltpic' in LoadDfltPic()");


  /* copy image from xvdflt_pic[] array */
  xdpline = 0;
  for (i=0; i<XVDFLT_HIGH; i++) {
    nbytes = 0;
    while (nbytes < XVDFLT_WIDE) {
      const char *sp;
      byte *dp;

      j = XVDFLT_WIDE - nbytes;
      if (j > XVDFLT_PARTLEN) j = XVDFLT_PARTLEN;

      sp = xvdflt_pic[xdpline];
      dp = dfltpic + i*XVDFLT_WIDE + nbytes;
      for (k=0; k<j; k++) {
	int c1,c2;
	c1 = *sp++;  c2 = *sp++;
	if (c1>='a') c1 = 10+(c1-'a');
                else c1 = c1-'0';

	if (c2>='a') c2 = 10+(c2-'a');
                else c2 = c2-'0';

	*dp++ = (byte) ((c1<<4) | c2);
      }

      nbytes += j;
      xdpline++;
    }
  }

  /* load up colormaps */
  for (i=0; i<256; i++) {
    pinfo->r[i] = xvdflt_r[i];
    pinfo->g[i] = xvdflt_g[i];
    pinfo->b[i] = xvdflt_b[i];
  }


  setcolor(pinfo, 250, 248,184,120);   /* revdate */
  setcolor(pinfo, 251, 255,255,255);   /* regstr  */
  setcolor(pinfo, 252,   0,  0,  0);   /* black background for text */


  xbm2pic((byte *) xv_cpyrt_bits, xv_cpyrt_width, xv_cpyrt_height,
	   dfltpic, DWIDE, DHIGH, DWIDE/2+1, 203+1, 252);
  xbm2pic((byte *) xv_cpyrt_bits, xv_cpyrt_width, xv_cpyrt_height,
	   dfltpic, DWIDE, DHIGH, DWIDE/2,   203, 250);

  i = xv_ver_width + xv_rev_width + 30;

  xbm2pic((byte *) xv_ver_bits, xv_ver_width, xv_ver_height,
       dfltpic, DWIDE, DHIGH, DWIDE/2 - (i/2) + xv_ver_width/2+1, 220+1,252);
  xbm2pic((byte *) xv_rev_bits, xv_rev_width, xv_rev_height,
       dfltpic, DWIDE, DHIGH, DWIDE/2 + (i/2) - xv_rev_width/2+1, 220+1,252);

  xbm2pic((byte *) xv_ver_bits, xv_ver_width, xv_ver_height,
	   dfltpic, DWIDE, DHIGH, DWIDE/2 - (i/2) + xv_ver_width/2, 220, 250);
  xbm2pic((byte *) xv_rev_bits, xv_rev_width, xv_rev_height,
	   dfltpic, DWIDE, DHIGH, DWIDE/2 + (i/2) - xv_rev_width/2, 220, 250);

  strcpy(str,"Press <right> mouse button for menu.");
  DrawStr2Pic(str, DWIDE/2+1, 241+1, dfltpic, DWIDE, DHIGH, 252);
  DrawStr2Pic(str, DWIDE/2, 241, dfltpic, DWIDE, DHIGH, 250);


#ifdef REGSTR
  strcpy(str,REGSTR);
#else
  strcpy(str,"UNREGISTERED COPY:  See 'About XV' for registration info.");
#endif

  DrawStr2Pic(str, DWIDE/2+1, 258+1, dfltpic, DWIDE, DHIGH, 252);
  DrawStr2Pic(str, DWIDE/2, 258, dfltpic, DWIDE, DHIGH, 251);


  pinfo->pic     = dfltpic;
  pinfo->w       = XVDFLT_WIDE;
  pinfo->h       = XVDFLT_HIGH;
  pinfo->type    = PIC8;
#ifdef HAVE_PNG
  pinfo->frmType = F_PNG;
#else
  pinfo->frmType = F_GIF;
#endif
  pinfo->colType = F_FULLCOLOR;

  pinfo->normw   = pinfo->w;
  pinfo->normh   = pinfo->h;

  sprintf(pinfo->fullInfo, "<8-bit internal>");
  sprintf(pinfo->shrtInfo, "%dx%d image.", XVDFLT_WIDE, XVDFLT_HIGH);
  pinfo->comment = (char *) NULL;

#endif /* !USEOLDPIC */
}



/*******************************************/
static void loadOldDfltPic(pinfo)
     PICINFO *pinfo;
{
  /* load up the stuff XV expects us to load up */

  char str[256];
  byte *dfltpic;
  int   i, j, k;

  dfltpic = (byte *) calloc((size_t) DWIDE * DHIGH, (size_t) 1);
  if (!dfltpic) FatalError("couldn't malloc 'dfltpic' in LoadDfltPic()");


  if (ncols) {    /* draw fish texture */
    for (i=k=0; i<DHIGH; i+=xf_left_height) {
      for (j=0; j<DWIDE; j+=xf_left_width) {
	k++;
	if (k&1)
	  xbm2pic((byte *) xf_left_bits, xf_left_width, xf_left_height,
		  dfltpic, DWIDE, DHIGH, j + xf_left_width/2,
		  i + xf_left_height/2, 1);
      }
    }
  }



  xbm2pic((byte *) xvpic_logo_out_bits, xvpic_logo_out_width,
	  xvpic_logo_out_height, dfltpic, DWIDE, DHIGH, DWIDE/2 + 10, 80, 103);

  xbm2pic((byte *) xvpic_logo_top_bits, xvpic_logo_top_width,
	  xvpic_logo_top_height, dfltpic, DWIDE, DHIGH, DWIDE/2 + 10, 80, 100);

  xbm2pic((byte *) xvpic_logo_bot_bits, xvpic_logo_bot_width,
	  xvpic_logo_bot_height, dfltpic, DWIDE, DHIGH, DWIDE/2 + 10, 80, 101);



  xbm2pic((byte *) xv_jhb_bits, xv_jhb_width, xv_jhb_height,
	   dfltpic, DWIDE, DHIGH, DWIDE/2, 160, 102);

  xbm2pic((byte *) xv_cpyrt_bits, xv_cpyrt_width, xv_cpyrt_height,
	   dfltpic, DWIDE, DHIGH, DWIDE/2, 203, 102);

  i = xv_ver_width + xv_rev_width + 30;

  xbm2pic((byte *) xv_ver_bits, xv_ver_width, xv_ver_height,
	   dfltpic, DWIDE, DHIGH, DWIDE/2 - (i/2) + xv_ver_width/2, 220, 102);

  xbm2pic((byte *) xv_rev_bits, xv_rev_width, xv_rev_height,
	   dfltpic, DWIDE, DHIGH, DWIDE/2 + (i/2) - xv_rev_width/2, 220, 102);

  strcpy(str,"Press <right> mouse button for menu.");
  DrawStr2Pic(str, DWIDE/2, 241, dfltpic, DWIDE, DHIGH, 102);

  strcpy(str,"UNREGISTERED COPY:  See 'About XV' for registration info.");

#ifdef REGSTR
  strcpy(str,REGSTR);
#endif

  DrawStr2Pic(str, DWIDE/2, 258, dfltpic, DWIDE, DHIGH, 104);

  setcolor(pinfo, 0, 225,  150, 255);  /* top-left fish color */
  setcolor(pinfo, 15, 55,    0,  77);  /* bot-right fish color */
  setcolor(pinfo, 16, 150, 150, 255);  /* top-left background color */
  setcolor(pinfo, 63,   0,   0,  77);  /* bottom-right background color */

  if (ncols) gen_bg(dfltpic, pinfo);

  /* set up colormap */
  setcolor(pinfo, 100, 255,213, 25);   /* XV top half */
  setcolor(pinfo, 101, 255,000,000);   /* XV bottom half */
  setcolor(pinfo, 102, 255,208,000);   /* jhb + fish + revdate */
  setcolor(pinfo, 103, 220,220,220);   /* XV backlighting */
  setcolor(pinfo, 104, 255,255,255);   /* registration string */

  if (ncols==0) {
    setcolor(pinfo,   0, 0, 0, 0);
    setcolor(pinfo, 102,255,255,255);
    setcolor(pinfo, 103,255,255,255);
    setcolor(pinfo, 104,255,255,255);
  }

  pinfo->pic     = dfltpic;
  pinfo->w       = DWIDE;
  pinfo->h       = DHIGH;
  pinfo->type    = PIC8;
#ifdef HAVE_PNG
  pinfo->frmType = F_PNG;
#else
  pinfo->frmType = F_GIF;
#endif
  pinfo->colType = F_FULLCOLOR;

  sprintf(pinfo->fullInfo, "<8-bit internal>");
  sprintf(pinfo->shrtInfo, "%dx%d image.",DWIDE, DHIGH);
  pinfo->comment = (char *) NULL;
}



/*******************************************/
void xbm2pic(bits, bwide, bhigh, pic, pwide, phigh, cx, cy, col)
     byte *bits;
     byte *pic;
     int   bwide, bhigh, pwide, phigh, cx, cy, col;
/*******************************************/
{
  /* draws an X bitmap into an 8-bit 'pic'.  Only '1' bits from the bitmap
     are drawn (in color 'col').  '0' bits are ignored */

  int     i, j, k, bit, x, y;
  byte   *pptr, *bptr;

  y = cy - bhigh/2;

  for (i=0; i<bhigh; i++,y++) {
    if ( (y>=0) && (y<phigh) ) {
      pptr = pic + y * pwide;
      bptr = (byte *) bits + i * ((bwide+7)/8);
      x = cx - bwide/2;

      k = *bptr;
      for (j=0,bit=0; j<bwide; j++, bit = (bit+1)&7, x++) {
	if (!bit) k = *bptr++;
	if ( (k&1) && (x>=0) && (x<pwide))
	  pptr[x] = col;

	k = k >> 1;
      }
    }
  }
}


/*******************************************/
static void setcolor(pinfo, i, rv, gv, bv)
     PICINFO *pinfo;
     int i, rv, gv, bv;
{
  pinfo->r[i] = rv;
  pinfo->g[i] = gv;
  pinfo->b[i] = bv;
}


/*******************************************/
static void gen_bg(dfltpic, pinfo)
     byte    *dfltpic;
     PICINFO *pinfo;
{
  int i,j, dr, dg, db;
  byte *pp;

  pp = dfltpic;
  for (i=0; i<DHIGH; i++)
    for (j=0; j<DWIDE; j++, pp++) {
      if (*pp == 0) {
	*pp = 16 + ((i+j) * 48) / (DHIGH + DWIDE);
      }
      else if (*pp == 1) {
	*pp = ((i+j) * 16) / (DHIGH + DWIDE);
      }
    }


  /* color gradient in cells 0-15 */
  for (i=1; i<15; i++) {
    dr = (int) pinfo->r[15] - (int) pinfo->r[0];
    dg = (int) pinfo->g[15] - (int) pinfo->g[0];
    db = (int) pinfo->b[15] - (int) pinfo->b[0];

    setcolor(pinfo, i, (int) pinfo->r[0] + (dr * i) / 15,
	               (int) pinfo->g[0] + (dg * i) / 15,
                       (int) pinfo->b[0] + (db * i) / 15);
  }

  /* color gradient in cells 16-63 */
  for (i=17, j=1; i<63; i++,j++) {
    dr = (int) pinfo->r[63] - (int) pinfo->r[16];
    dg = (int) pinfo->g[63] - (int) pinfo->g[16];
    db = (int) pinfo->b[63] - (int) pinfo->b[16];

    setcolor(pinfo, i, (int) pinfo->r[16] + (dr * j)/47,
                       (int) pinfo->g[16] + (dg * j)/47,
                       (int) pinfo->b[16] + (db * j)/47);
  }
}



/*******************************************/
void DrawStr2Pic(str, cx, cy, pic, pw, ph, col)
     char *str;
     byte *pic;
     int   cx, cy, pw, ph, col;
{
  /* draw string (in 5x9 font) centered around cx,cy, in color 'col' */

  int  i;

  i = strlen(str);
  if (!i) return;

  cx -= ((i-1) * 3);

  for ( ; *str; str++, cx+=6) {
    i = (byte) *str;
    if (i >= 32 && i < 128)
      xbm2pic(font5x9[i - 32], 5, 9, pic, pw, ph, cx, cy, col);
  }
}
