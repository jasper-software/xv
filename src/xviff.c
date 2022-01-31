/*
 * xviff.c - load routine for IFF ILBM format pictures
 *
 * LoadIFF(fname, pinfo)  -  loads an IFF file
 *
 *
 * This loader module for xv will load most types of IFF ILBM pictures.
 * Apart from any 'normal' ILBM format, this loader understands
 * HAM (Hold-And-Modify), HAM8 (extended HAM), EHB (Extra Half Brite)
 * and 24-Bit.
 * Multipalette ILBMs are not supported yet, but I'll be working on that!
 *
 *
 * History:
 *  30-May-93: Project started
 *             ILBMs can now be loaded from xv
 *             EHB support added
 *             HAM support added
 *  01-May-93: Started HAM8 support
 *  04-Jun-93: HAM8 finally works! =:)
 *             24 Bit support works!
 */


/* Copyright Notice
 * ================
 * Copyright 1993 by Thomas Meyer
 *
 * Author:
 *   Thomas Meyer
 *   i03a@alf.zfn.uni-bremen.de
 *
 * Permission is hereby granted to use this code only as a part of
 * the xv distribution by John Bradley.
 */


#include "xv.h"

static long filesize;

/* static int           readID       PARM((FILE *, char *));  DOES NOT EXIST */
static int           iffError     PARM((const char *, const char *));
static void          decomprle    PARM((byte *, byte *, long, long));
static unsigned int  iff_getword  PARM((byte *));
static unsigned long iff_getlong  PARM((byte *));


static const char *bname;


/* Define internal ILBM types */
#define ILBM_NORMAL     0
#define ILBM_EHB        1
#define ILBM_HAM        2
#define ILBM_HAM8       3
#define ILBM_24BIT      4



/*******************************************/
int LoadIFF(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
/*******************************************/
{
  /* returns '1' on success */

  register byte bitmsk, rval, gval, bval;
  register long col, colbit;
  FILE          *fp;
  int           rv;
  int           BMHDok, CMAPok, CAMGok;
  int           bmhd_width, bmhd_height, bmhd_bitplanes, bmhd_transcol;
  int           i, j, k, lineskip, colors, fmt;
  int           npixels = 0; /* needs to be initialized _outside_ while-loop */
  byte          bmhd_masking, bmhd_compression;
  long          chunkLen, camg_viewmode;
  byte          *databuf, *dataptr, *cmapptr, *picptr, *pic, *bodyptr;
  byte          *workptr, *workptr2, *workptr3, *decomp_mem;

  BMHDok = CAMGok = bmhd_width = bmhd_height = bmhd_bitplanes = colors = 0;
  bmhd_compression = 0;
  camg_viewmode = 0;

  bname = BaseName(fname);
  databuf = picptr = decomp_mem = NULL;

  pinfo->pic     = (byte *) NULL;
  pinfo->comment = (char *) NULL;

  /* open the file */
  fp = xv_fopen(fname,"r");
  if (!fp) return (iffError(bname, "cannot open file"));

  /* compute file length */
  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);

  /* allocate memory for complete file */
  if((databuf = (byte *) malloc((size_t) filesize)) == NULL) {
    fclose(fp);
    FatalError("xviff: cannot malloc file buffer");
  }

  fread(databuf, (size_t) filesize, (size_t) 1, fp);
  fclose(fp);


  /* initialize work pointer. used to trace the buffer for IFF chunks */
  dataptr = databuf;


  /* check if we really got an IFF file */
  if(strncmp((char *) dataptr, "FORM", (size_t) 4) != 0) {
    free(databuf);
    return(iffError(bname, "not an IFF file"));
  }


  dataptr = dataptr + 8;                  /* skip ID and length of FORM */

  /* check if the IFF file is an ILBM (picture) file */
  if(strncmp((char *) dataptr, "ILBM", (size_t) 4) != 0) {
    free(databuf);
    return(iffError(bname, "not an ILBM file"));
  }

  if (DEBUG) fprintf(stderr,"IFF ILBM file recognized\n");

  dataptr = dataptr + 4;                                /* skip ID */

  /* initialize return value to -1. used to indicate when we are finished with
     decoding the picture (happens when BODY chunk was found */
  rv = -1;

  /* main decoding loop. searches IFF chunks and handles them. terminates when
     BODY chunk was found or dataptr ran over end of file */

  while ((rv<0) && (dataptr < (databuf + filesize))) {
    chunkLen = (iff_getlong(dataptr + 4) + 1) & 0xfffffffe; /* make even */

    if (strncmp((char *) dataptr, "BMHD", (size_t) 4)==0) { /* BMHD chunk? */
      bmhd_width       = iff_getword(dataptr + 8);      /* width of picture */
      bmhd_height      = iff_getword(dataptr + 8 + 2);  /* height of picture */
      bmhd_bitplanes   = *(dataptr + 8 + 8);            /* # of bitplanes */
      bmhd_masking     = *(dataptr + 8 + 9);
      bmhd_compression = *(dataptr + 8 + 10);           /* get compression */
      bmhd_transcol    = iff_getword(dataptr + 8 + 12);
      BMHDok = 1;                                       /* got BMHD */
      dataptr += 8 + chunkLen;                          /* to next chunk */

      npixels = bmhd_width * bmhd_height;  /* 65535*65535 max */
      if (bmhd_width <= 0 || bmhd_height <= 0
          || npixels/bmhd_width != bmhd_height)
        return (iffError(bname, "xviff: image dimensions out of range"));
    }


    else if (strncmp((char *) dataptr, "CMAP", (size_t) 4)==0) { /* CMAP ? */
      cmapptr = dataptr + 8;
      colors = chunkLen / 3;                            /* calc no of colors */

      /* copy colors to color map */
      for (i=0; i < colors; i++) {
	pinfo->r[i] = *cmapptr++;
	pinfo->g[i] = *cmapptr++;
	pinfo->b[i] = *cmapptr++;
      }

      CMAPok = 1;                                       /* got CMAP */
      dataptr += 8 + chunkLen;                          /* to next chunk */
    }


    else if (strncmp((char *) dataptr, "CAMG", (size_t) 4)==0) {  /* CAMG ? */
      camg_viewmode = iff_getlong(dataptr + 8);             /* get viewmodes */
      CAMGok = 1;                                       /* got CAMG */
      dataptr += 8 + chunkLen;                          /* to next chunk */
    }


    else if (strncmp((char *) dataptr, "BODY", (size_t) 4)==0) { /* BODY ? */
      int byte_width = (((bmhd_width + 15) >> 4) << 1);  /* 8192 max */

      bodyptr = dataptr + 8;                            /* -> BODY data */

      if (BMHDok) {                                     /* BMHD found? */
	/* if BODY is compressed, allocate buffer for decrunched BODY and
	   decompress it (run length encoding) */

	if (bmhd_compression == 1) {
	  /* calc size of decrunch buffer - (size of the actual picture
	     decompressed in interleaved Amiga bitplane format) */

	  int bytes_per_bitplane = byte_width * bmhd_height; /* 536862720 max */
          long decomp_bufsize = bytes_per_bitplane * bmhd_bitplanes;

	  if (byte_width <= 0 || bmhd_height <= 0 ||
	      bytes_per_bitplane/byte_width != bmhd_height ||
	      decomp_bufsize/bytes_per_bitplane != bmhd_bitplanes)
	  {
	    return (iffError(bname, "xviff: image dimensions out of range"));
	  }

	  if ((decomp_mem = (byte *)malloc((size_t) decomp_bufsize)) != NULL) {
	    decomprle(dataptr + 8, decomp_mem, chunkLen, decomp_bufsize);
	    bodyptr = decomp_mem;                 /* -> uncompressed BODY */
	    free(databuf);                        /* free old data buffer */
	    databuf = NULL;
	  }
	  else {
	    free(databuf);
	    FatalError("xviff: cannot malloc() decrunch buffer");
	  }
	}


	/* the following determines the type of the ILBM file.
	   it's either NORMAL, EHB, HAM, HAM8 or 24BIT */

	fmt = ILBM_NORMAL;                        /* assume normal ILBM */
	/* FIXME:  does ILBM_NORMAL really support up to 255 bitplanes? */

	if      (bmhd_bitplanes == 24) fmt = ILBM_24BIT;
	else if (bmhd_bitplanes == 8) {
	  if (CAMGok && (camg_viewmode & 0x800)) fmt = ILBM_HAM8;
	}

	else if ((bmhd_bitplanes > 5) && CAMGok) {
	  if (camg_viewmode & 0x80) fmt = ILBM_EHB;
	  else if (camg_viewmode & 0x800) fmt = ILBM_HAM;
	}


	if (DEBUG) {
	  fprintf(stderr, "LoadIFF: %s %dx%d, planes=%d (%d cols), comp=%d\n",
		  (fmt==ILBM_NORMAL) ? "Normal ILBM" :
		  (fmt==ILBM_HAM)    ? "HAM ILBM" :
		  (fmt==ILBM_HAM8)   ? "HAM8 ILBM" :
		  (fmt==ILBM_EHB)    ? "EHB ILBM" :
		  (fmt==ILBM_24BIT)  ? "24BIT ILBM" : "unknown ILBM",
		  bmhd_width, bmhd_height, bmhd_bitplanes,
		  1<<bmhd_bitplanes, bmhd_compression);
	}


	if ((fmt==ILBM_NORMAL) || (fmt==ILBM_EHB) || (fmt==ILBM_HAM)) {
	  if (DEBUG) fprintf(stderr,"Converting CMAP from normal ILBM CMAP\n");

	  switch(fmt) {
	  case ILBM_NORMAL: colors = 1 << bmhd_bitplanes; break;
	  case ILBM_EHB:    colors = 32; break;
	  case ILBM_HAM:    colors = 16; break;
	  }

	  for (i=0; i < colors; i++) {
	    pinfo->r[i] = (pinfo->r[i] >> 4) * 17;
	    pinfo->g[i] = (pinfo->g[i] >> 4) * 17;
	    pinfo->b[i] = (pinfo->b[i] >> 4) * 17;
	  }
	}


	if ((fmt == ILBM_HAM) || (fmt == ILBM_HAM8) || (fmt == ILBM_24BIT)) {
	  int bufsize = 3 * npixels;

	  if (bufsize/3 != npixels) {
	    if (databuf)    free(databuf);
	    if (decomp_mem) free(decomp_mem);
	    return (iffError(bname, "xviff: image dimensions out of range"));
	  }
	  if ((picptr=(byte *) malloc((size_t) bufsize)) == NULL) {
	    if (databuf)    free(databuf);
	    if (decomp_mem) free(decomp_mem);
	    return (iffError(bname, "xviff: no memory for decoded picture"));
	  }

	  else {
	    pic = picptr;
	    workptr = bodyptr;
	    lineskip = byte_width;

	    for (i=0; i<bmhd_height; i++) {
	      bitmsk = 0x80;
	      workptr2 = workptr;

	      /* at start of each line, init RGB values to background */
	      rval = pinfo->r[0];
	      gval = pinfo->g[0];
	      bval = pinfo->b[0];

	      for (j=0; j<bmhd_width; j++) {
		col = 0;
		colbit = 1;
		workptr3 = workptr2;
		for (k=0; k<bmhd_bitplanes; k++) {
		  if (*workptr3 & bitmsk) col += colbit;
		  workptr3 += lineskip;
		  colbit <<= 1;
		}

		if (fmt==ILBM_HAM) {
		  switch (col & 0x30) {
		  case 0x00: rval = pinfo->r[col & 0x0f];
		             gval = pinfo->g[col & 0x0f];
		             bval = pinfo->b[col & 0x0f];
		             break;

		  case 0x10: bval = (col & 0x0f) * 17;
                             break;

		  case 0x20: rval = (col & 0x0f) * 17;
                             break;

		  case 0x30: gval = (col & 0x0f) * 17;
		  }
		}

		else if (fmt == ILBM_HAM8) {
		  switch(col & 0xc0) {
		  case 0x00: rval = pinfo->r[col & 0x3f];
		             gval = pinfo->g[col & 0x3f];
                             bval = pinfo->b[col & 0x3f];
                             break;

		  case 0x40: bval = (bval & 3) | ((col & 0x3f) << 2);
                             break;

		  case 0x80: rval = (rval & 3) | ((col & 0x3f) << 2);
                             break;

		  case 0xc0: gval = (rval & 3) | ((col & 0x3f) << 2);
		  }
		}

		else {
		  rval = col & 0xff;
		  gval = (col >> 8) & 0xff;
		  bval = (col >> 16) & 0xff;
		}

		*pic++ = rval;
		*pic++ = gval;
		*pic++ = bval;

		bitmsk = bitmsk >> 1;
		if (bitmsk == 0) {
		  bitmsk = 0x80;
		  workptr2++;
		}
	      }
	      workptr += lineskip * bmhd_bitplanes;
	    }

	    pinfo->type = PIC24;
	  }
	}  /* HAM, HAM8, or 24BIT */


	else if ((fmt == ILBM_NORMAL) || (fmt == ILBM_EHB)) {
	  /* if bmhd_width and bmhd_height are OK (checked in BMHD block
	   * above; guaranteed by BMHDok), then npixels is OK, too */
	  if ((picptr = (byte *) malloc((size_t) npixels)) == NULL) {
	    if (databuf) free(databuf);
	    if (decomp_mem) free(decomp_mem);
	    return (iffError(bname, "xviff: no memory for decoded picture"));
	  }

	  else if (fmt == ILBM_EHB) {
	    if (DEBUG) fprintf(stderr,"Doubling CMAP for EHB mode\n");

	    for (i=0; i<32; i++) {
	      pinfo->r[i + colors] = pinfo->r[i] >> 1;
	      pinfo->g[i + colors] = pinfo->g[i] >> 1;
	      pinfo->b[i + colors] = pinfo->b[i] >> 1;
	    }
	  }

	  pic = picptr;             /* ptr to chunky buffer */
	  workptr = bodyptr;        /* ptr to uncmp'd pic, planar format */
	  lineskip = byte_width;

	  for (i=0; i<bmhd_height; i++) {
	    bitmsk = 0x80;                      /* left most bit (mask) */
	    workptr2 = workptr;                 /* work ptr to source */
	    for (j=0; j<bmhd_width; j++) {
	      col = 0;
	      colbit = 1;
	      workptr3 = workptr2;              /* ptr to byte in 1st pln */

	      for (k=0; k<bmhd_bitplanes; k++) {
		if (*workptr3 & bitmsk)          /* if bit set in this pln */
		  col = col + colbit;           /* add bit to chunky byte */
		workptr3 += lineskip;           /* go to next line */
		colbit <<= 1;                   /* shift color bit */
	      }

	      *pic++ = col;                     /* write to chunky buffer */
	      bitmsk = bitmsk >> 1;             /* shift mask to next bit */
	      if (bitmsk == 0) {                /* if mask is zero */
		bitmsk = 0x80;                  /* reset mask */
		workptr2++;                     /* mv ptr to next byte */
	      }
	    }  /* for j ... */

	    workptr += lineskip * bmhd_bitplanes;  /* to next line */
	  }  /* for i ... */

	  pinfo->type = PIC8;
	}  /* if NORMAL or EHB */


	/* fill up PICINFO */

	pinfo->pic = picptr;
	pinfo->w = bmhd_width;
	pinfo->h = bmhd_height;
	pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;
	pinfo->colType = F_FULLCOLOR;
	pinfo->frmType = -1;

	sprintf(pinfo->fullInfo, "%s (%ld bytes)",
		(fmt==ILBM_NORMAL) ? "IFF ILBM" :
		(fmt==ILBM_HAM)    ? "HAM ILBM" :
		(fmt==ILBM_HAM8)   ? "HAM8 ILBM" :
		(fmt==ILBM_EHB)    ? "EHB ILBM" :
		(fmt==ILBM_24BIT)  ? "24BIT ILBM" : "unknown ILBM",
		filesize);

	sprintf(pinfo->shrtInfo, "%dx%d ILBM.", bmhd_width, bmhd_height);

	rv = 1;

      }  /* if BMHDok */

      else rv = 0;                   /* didn't get BMHD header */

    }  /* "BODY" chunk */

    else {
      if (DEBUG)
	fprintf(stderr,"Skipping unknown chunk '%c%c%c%c'\n",
                *dataptr, *(dataptr+1), *(dataptr+2), *(dataptr+3));

      dataptr = dataptr + 8 + chunkLen;             /* skip unknown chunk */
    }
  }


  if (decomp_mem) free(decomp_mem);
  if (databuf)    free(databuf);

  if (rv != 1) free(picptr);

  return rv;
}





/**************************************************************************
  void decomprle(source, destination, source length, buffer size)

  Decompress run-length encoded data from source to destination. Terminates
  when source is decoded completely or destination buffer is full.

  The decruncher is as optimized as I could make it, without risking
  safety in case of corrupt BODY chunks.
***************************************************************************/


/*******************************************/
static void decomprle(sptr, dptr, slen, dlen)
     byte *sptr, *dptr;
     register long slen, dlen;
{
  register byte codeByte, dataByte;

  while ((slen > 0) && (dlen > 0)) {

    /* read control byte */
    codeByte = *sptr++;

    if (codeByte < 0x80) {
      codeByte++;
      if ((slen > (long) codeByte) && (dlen >= (long) codeByte)) {
        slen -= codeByte + 1;
        dlen -= codeByte;
        while (codeByte > 0) {
          *dptr++ = *sptr++;
          codeByte--;
        }
      }
      else slen = 0;
    }

    else if (codeByte > 0x80) {
      codeByte = 0x81 - (codeByte & 0x7f);
      if ((slen > (long) 0) && (dlen >= (long) codeByte)) {
	dataByte = *sptr++;
	slen -= 2;
	dlen -= codeByte;
	while (codeByte > 0) {
	  *dptr++ = dataByte;
	  codeByte--;
	}
      }
      else slen = 0;
    }
  }
}



/*******************************************/
static unsigned int iff_getword(ptr)
     byte *ptr;
{
  register unsigned int v;

  v = *ptr++;
  v = (v << 8) + *ptr;

  return v;
}


/*******************************************/
static unsigned long iff_getlong(ptr)
     byte *ptr;
{
  register unsigned long l;

  l = *ptr++;
  l = (l << 8) + *ptr++;
  l = (l << 8) + *ptr++;
  l = (l << 8) + *ptr;

  return l;
}


/*******************************************/
static int iffError(fname, st)
     const char *fname, *st;
{
  SetISTR(ISTR_WARNING,"%s:  %s", fname, st);
  return 0;
}
