/*
 * xvfits.c - load/save routines for 'fits' format pictures
 *
 * LoadFITS(fname, pinfo, quick)  -  loads a FITS file
 * WriteFITS(fp,pic,ptype,w,h,r,g,b,numcols,style,raw,cmt,comment)
 */

/*
 * Copyright 1992, 1993, 1994 by David Robinson.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies.  This software is
 * provided "as is" without express or implied warranty.
 */

#define  NEEDSDIR /* for S_IRUSR|S_IWUSR */
#include "xv.h"

#define NCARDS    (36)
#define BLOCKSIZE (2880)

/* data types */
typedef enum datatype { T_INT, T_LOG, T_STR, T_NOVAL } DATTYPE;

typedef struct {
  FILE     *fp;          /* file pointer */
  int       bitpix;      /* number of bits per pixel */
  int       size;        /* Size of each pixel, in bytes */
  int       naxis;       /* number of axes */
  long int  axes[3];     /* size of each axis */
  long int  ndata;       /* number of elements in data */
  long int  cpos;        /* current position in data file */
  char     *comment;     /* malloc'ed comment string, or NULL if none */
} FITS;


/* block workspace of BLOCKSIZE bytes */
static char *fits_block=NULL;


static       int   splitfits PARM((byte *, char *, int, int, int, char *));
static const char *ftopen3d  PARM((FITS *, char *, int *, int *, int *, int *));
static       void  ftclose   PARM((FITS *));
static       int   ftgbyte   PARM((FITS *, byte *, int));
static const char *rdheader  PARM((FITS *));
static const char *wrheader  PARM((FILE *, int, int, char *));
static const char *rdcard    PARM((char *, const char *, DATTYPE, long int *));
static       void  wrcard    PARM((char *, const char *, DATTYPE, int, char *));
static       int   ftgdata   PARM((FITS *, void *, int));
static       void  ftfixdata PARM((FITS *, void *, int));
static       void  flip      PARM((byte *, int, int));



/*******************************************/
int LoadFITS(fname, pinfo, quick)
     char    *fname;
     PICINFO *pinfo;
     int      quick;
/*******************************************/
{
  /* returns '1' on success */

  FITS  fs;
  int   i, nx, ny, nz, bitpix, nrd, ioerror, npixels, bufsize;
  byte *image;
  const char *error;
  char  basename[64];

  if (fits_block == NULL) {
    fits_block = (char *) malloc((size_t) BLOCKSIZE);
    if (!fits_block) FatalError("Insufficient memory for FITS block buffer");
  }

  error = ftopen3d(&fs, fname, &nx, &ny, &nz, &bitpix);
  if (error) {
    SetISTR(ISTR_WARNING, "%s", error);
    return 0;
  }

  if (quick) nz = 1;             /* only load first plane */
  npixels = nx * ny;
  bufsize = nz * npixels;
  if (nx <= 0 || ny <= 0 || npixels/nx != ny || bufsize/nz != npixels) {
    SetISTR(ISTR_WARNING, "FITS image dimensions out of range (%dx%dx%d)",
      nx, ny, nz);
    return 0;
  }

  image = (byte *) malloc((size_t) bufsize);
  if (!image) FatalError("Insufficient memory for image");

  /*
   * Read in image. For three dimensional images, read it in in one go
   * to ensure that we get that same scaling for all planes.
   */

  nrd     = ftgbyte(&fs, image, bufsize);
  ioerror = ferror(fs.fp);
  ftclose(&fs);

  if (nrd == 0) {  /* didn't read any data at all */
    if (ioerror)
      SetISTR(ISTR_WARNING, "%s", "I/O error reading FITS file");
    else
      SetISTR(ISTR_WARNING, "%s", "Unexpected EOF reading FITS file");

    free(image);
    return 0;
  }

  else if (nrd < bufsize) {       /* read partial image */
    if (ioerror)
      SetISTR(ISTR_WARNING, "%s", "Truncated FITS file due to I/O error");
    else
      SetISTR(ISTR_WARNING, "%s", "Truncated FITS file");

    { byte *foo;
      for (foo=image+nrd; foo<image+bufsize; foo++) *foo=0x80;  /* pad with grey */
    }
  }

  if (nz > 1) {
    /* how many planes do we actually have? */
    nz = (nrd-1)/(npixels) + 1;

    /* returns how many sub-files created */
    nz = splitfits(image, fs.comment, nx, ny, nz, basename);
    image = (byte *)realloc(image, (size_t) npixels);  /* toss all but first */
  }

  /* There seems to be a convention that fits files be displayed using
   * a cartesian coordinate system. Thus the first pixel is in the lower left
   * corner. Fix this by reflecting in the line y=ny/2.
   */
  flip(image, nx, ny);

  /* Success! */
  pinfo->pic  = image;
  pinfo->type = PIC8;
  pinfo->w    = pinfo->normw = nx;
  pinfo->h    = pinfo->normh = ny;

  for (i=0; i < 256; i++) pinfo->r[i] = pinfo->g[i] = pinfo->b[i] = i;
  pinfo->frmType = F_FITS;
  pinfo->colType = F_GREYSCALE;

  sprintf(pinfo->fullInfo, "FITS, bitpix: %d", bitpix);
  sprintf(pinfo->shrtInfo, "%dx%d FITS.", nx, ny);
  pinfo->comment = fs.comment;

  if (nz > 1) {
    pinfo->numpages = nz;
    strcpy(pinfo->pagebname, basename);
  }

  return 1;
}



/*******************************************/
int WriteFITS(fp,pic,ptype,w,h,rmap,gmap,bmap,numcols,colorstyle,comment)
     FILE *fp;
     byte *pic;
     int   ptype, w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
     char *comment;
{
  int   i, j, npixels, nend;
  byte *ptr;
  const char *error;
  byte  rgb[256];

  if (!fits_block) {
    fits_block = (char *) malloc((size_t) BLOCKSIZE);
    if (!fits_block) FatalError("Insufficient memory for FITS block buffer");
  }

  error = wrheader(fp, w, h, comment);
  if (error) {
    SetISTR(ISTR_WARNING, "%s", error);
    return -1;
  }

  if (ptype == PIC8) {
    /* If ptype is PIC8, we need to calculate the greyscale colourmap */
    for (i=0; i < numcols; i++) rgb[i] = MONO(rmap[i], gmap[i], bmap[i]);

    for (i=h-1; i >= 0; i--) {     /* flip line ordering when writing out */
      ptr = &pic[i*w];
      for (j=0; j < w; j++, ptr++) putc(rgb[*ptr], fp);
    }
  }

  else {  /* PIC24 */
    for (i=h-1; i >= 0; i--) {     /* flip line ordering when writing out */
      ptr = &pic[i*w*3];
      for (j=0; j < w; j++, ptr += 3) putc(MONO(ptr[0], ptr[1], ptr[2]), fp);
    }
  }

  npixels = w*h;

  /* nend is the number of padding characters at the end of the last block */
  nend = ((npixels+BLOCKSIZE-1)/BLOCKSIZE)*BLOCKSIZE - npixels;
  if (nend) for (i=0; i<nend; i++) putc('\0', fp);

  return 0;
}



/************************************/
static int splitfits(image, comment, nx, ny, nz, basename)
     byte *image;
     char *comment;
     int   nx, ny, nz;
     char *basename;
{
  /*
   * Given a 3-dimensional FITS image, this splits it up into nz 2-d files.
   * It returns the number of files actually stored.
   * If only one file could be written, then no split files are created.
   * It returns the basename of the split files in bname.
   * If there was a problem writing files, then a error message will be set.
   */

  int   i, npixels=nx * ny, nwrt, tmpfd;
  FILE *fp;
  const char *error;
  char  filename[70];

#ifndef VMS
  sprintf(basename, "%s/xvpgXXXXXX", tmpdir);
#else
  sprintf(basename, "Sys$Disk:[]xvpgXXXXXX");
#endif

#ifdef USE_MKSTEMP
  close(mkstemp(basename));
#else
  mktemp(basename);
#endif
  if (basename[0] == '\0') {
    SetISTR(ISTR_WARNING, "%s", "Unable to build temporary filename");
    return 1;
  }

  strcat(basename, ".");
  error = NULL;

  for (i=0; i < nz && !error; i++) {
    sprintf(filename, "%s%d", basename, i+1);
    tmpfd = open(filename,O_WRONLY|O_CREAT|O_EXCL,S_IRWUSR);
    if (tmpfd < 0) {
      error = "Unable to open temporary file";
      break;
    }
    fp = fdopen(tmpfd, "w");
    if (!fp) {
      error = "Unable to open temporary file";
      break;
    }

    if (wrheader(fp, nx, ny, comment)) {
      error = "I/O error writing temporary file";
      fflush(fp);
      fclose(fp);
      unlink(filename);
      close(tmpfd);
      break;
    }

    nwrt = fwrite(image+i*npixels, sizeof(byte), (size_t) npixels, fp);
    fflush(fp);
    fclose(fp);
    close(tmpfd);

    if (nwrt == 0) {  /* failed to write any data */
      error = "I/O error writing temporary file";
      unlink(filename);
      break;
    }
    else if (nwrt < npixels)
      error = "I/O error writing temporary file";
  }


  /* at this point, i is the number of files created */
  if (i == 0) return 1;
  if (i == 1) {
    sprintf(filename, "%s1", basename);
    unlink(filename);
  }

  if (error) SetISTR(ISTR_WARNING, "%s", error);
  return i;
}


/************************************/
static const char *wrheader(fp, nx, ny, comment)
     FILE *fp;
     int nx, ny;
     char *comment;
{
  /* Writes a minimalist FITS file header */

  char *block = fits_block, *bp;
  int   i, j, lenhist;
  char  history[80];

  for (i=0, bp=block; i<BLOCKSIZE; i++, bp++) *bp = ' ';

  sprintf(history, "Written by XV %s", VERSTR);
  lenhist = strlen(history);

  i = 0;
  wrcard(&block[80*i++], "SIMPLE", T_LOG, 1, NULL);   /* write SIMPLE card */
  wrcard(&block[80*i++], "BITPIX", T_INT, 8, NULL);   /* write BITPIX card */
  wrcard(&block[80*i++], "NAXIS",  T_INT, 2, NULL);   /* write NAXIS card */
  wrcard(&block[80*i++], "NAXIS1", T_INT, nx, NULL);  /* write NAXIS1 card */
  wrcard(&block[80*i++], "NAXIS2", T_INT, ny, NULL);  /* write NAXIS2 card */

  /* Write HISTORY keyword */
  wrcard(&block[80*i++], "HISTORY", T_STR, lenhist, history);

  if (comment && *comment != '\0') {
    while (*comment == '\n') comment++;  /* Skip any blank lines */
    while (*comment != '\0') {
      for (j=0; j<72; j++)
	if (comment[j] == '\0' || comment[j] == '\n') break;

      /*
       * Check to see if it is an xv history record; if so, then avoid
       * duplicating it.
       */
      if (j != lenhist || xvbcmp(comment, history, (size_t) j) != 0)
	wrcard(&block[80*i++], "COMMENT", T_STR, j, comment);

      if (i == NCARDS)  {  /* Filled up a block */
	i = fwrite(block, sizeof(char), (size_t) BLOCKSIZE, fp);
	if (i != BLOCKSIZE) return("Error writing FITS file");
	for (i=0, bp=block; i<BLOCKSIZE; i++, bp++) *bp = ' ';
	i = 0;
      }

      comment += j;
      while (*comment == '\n') comment++;  /* Skip any blank lines */
    }
  }

  wrcard(&block[80*i++], "END", T_NOVAL, 0, NULL);    /* write END keyword */
  i = fwrite(block, sizeof(char), (size_t) BLOCKSIZE, fp);

  if (i != BLOCKSIZE) return "Error writing FITS file";
  return NULL;
}



/************************************/
static const char *ftopen3d(fs, file, nx, ny, nz, bitpix)
     FITS *fs;
     char *file;
     int  *nx, *ny, *nz, *bitpix;
{
  /* open a 2 or 3-dimensional fits file.
   * Stores the dimensions of the file in nx, ny and nz, and updates the FITS
   * structure passed in fs.
   * If successful, returns NULL otherwise returns an error message.
   * Will return an error message if the primary data unit is not a
   * 2 or 3-dimensional array.
   */

  FILE *fp;
  int naxis, i;
  const char *error;

  fp = xv_fopen(file, "r");
  if (!fp) return "Unable to open FITS file";

  fs->fp     = fp;
  fs->bitpix = 0;
  fs->naxis  = 0;
  fs->cpos   = 0;

  /* read header */
  error = rdheader(fs);
  if (error) {
    ftclose(fs);
    return error;
  }

  naxis = fs->naxis;

  /* get number of data */
  fs->ndata = 1;
  for (i=0; i<naxis; i++)
    fs->ndata = fs->ndata * fs->axes[i];

  *nx = fs->axes[0];
  *ny = fs->axes[1];
  if (naxis == 2) *nz = 1;
             else *nz = fs->axes[2];

  *bitpix = fs->bitpix;

  return NULL;
}


/************************************/
static void ftclose(fs)
     FITS *fs;
{
  if (fs == NULL) return;
  if (fs->fp != NULL) fclose(fs->fp);
}


/************************************/
static const char *rdheader(fs)
     FITS *fs;
{
  /* reads the fits header, and updates the FITS structure fs.
   * Returns NULL on success, or an error message otherwise.
   */

  int i, j, res, commlen, commsize;
  char name[9];
  char *block=fits_block, *p;
  const char *error;
  long int val;         /* the value */

  fs->comment = NULL;
  commlen     = 0;
  commsize    = 256;

  res = fread(block, sizeof(char), (size_t) BLOCKSIZE, fs->fp);
  if (res != BLOCKSIZE) return "Error reading FITS file";
  i = 0;

  /* read SIMPLE key */
  error = rdcard(block, "SIMPLE", T_LOG, &val);
  if (error) return error;
  if (val == 0) return "Not a SIMPLE FITS file";
  i++;

  /* read BITPIX key */
  error = rdcard(&block[80], "BITPIX", T_INT, &val);
  if (error) return error;

  if (val != 8 && val != 16 && val != 32 && val != 64 && val != -32 &&
      val != -64)
    return "Bad BITPIX value in FITS file";

  j = fs->bitpix = val;
  if (j<0) j = -j;
  fs->size = j/8;
  i++;

  /* read NAXIS key */
  error = rdcard(&block[2*80], "NAXIS", T_INT, &val);
  if (error) return error;
  if (val < 0 || val > 999) return "Bad NAXIS value in FITS file";
  if (val == 0)             return "FITS file does not contain an image";
  if (val < 2)              return "FITS file has fewer than two dimensions";
  fs->naxis = val;
  i++;

  /* read NAXISnnn keys.
   * We allow NAXIS to be > 3 iff the dimensions of the extra axes are 1
   */
  for (j=0; j < fs->naxis; j++) {
    if (i == NCARDS) {
      res = fread(block, sizeof(char), (size_t) BLOCKSIZE, fs->fp);
      if (res != BLOCKSIZE) return "Error reading FITS file";
      i = 0;
    }

    sprintf(name, "NAXIS%d", j+1);
    error = rdcard(&block[i*80], name, T_INT, &val);
    if (error)    return error;
    if (val < 0)  return "Bad NAXISn value in FITS file";
    if (val == 0) return "FITS file does not contain an image";

    if (j < 3)    fs->axes[j] = val;
    else if (val != 1) return "FITS file has more than three dimensions";
    i++;
  }
  if (fs->naxis > 3) fs->naxis = 3;

  /* do remainder, looking for comment cards */
  /* Section 5.2.2.4 of the NOST standard groups COMMENT and HISTORY keywords
   * under 'Commentary Keywords'. Many applications write HISTORY records in
   * preference to COMMENT records, so we recognise both types here.
   */
  for (;;) {
    if (i == NCARDS) {
      res = fread(block, sizeof(char), (size_t) BLOCKSIZE, fs->fp);
      if (res != BLOCKSIZE) return "Unexpected eof in FITS file";
      i = 0;
    }

    p = &block[i*80];
    if (strncmp(p, "END     ", (size_t) 8) == 0) break;
    if (strncmp(p, "HISTORY ", (size_t) 8) == 0 ||
	strncmp(p, "COMMENT ", (size_t) 8) == 0) {
      p += 8;                       /* skip keyword */
      for (j=71; j >= 0; j--) if (p[j] != ' ') break;
      j++;                          /* make j length of comment */
      if (j > 0) {                  /* skip blank comment cards */
	if (fs->comment == NULL) {
	  fs->comment = (char *) malloc((size_t) commsize);  /* initially 256 */
	  if (fs->comment == NULL)
	    FatalError("Insufficient memory for comment buffer");
	}

	if (commlen + j + 2 > commsize) { /* if too small */
	  char *new;
	  commsize += commsize;      /* double size of array */
	  new = (char *) malloc((size_t) commsize);

	  if (new == NULL)
	    FatalError("Insufficient memory for comment buffer");

	  if (commlen) xvbcopy(fs->comment, new, (size_t) commlen);
	  free(fs->comment);
	  fs->comment = new;
	}

	xvbcopy(p, &fs->comment[commlen], (size_t) j);  /* add string */
	commlen += j;
	fs->comment[commlen++] = '\n';       /* with trailing cr */
	fs->comment[commlen] = '\0';
      }
    }
    i++;
  }

  return NULL;
}


/************************************/
static void wrcard(card, name, dtype, kvalue, svalue)
     char *card;
     const char *name;
     DATTYPE dtype;   /* type of value */
     int kvalue;
     char *svalue;
{
  /* write a header record into the 80 byte buffer card.
   * The keyword name is passed in name. The value type is in dtype; this
   * can have the following values:
   *    dtype = T_NOVAL
   *         no keyword value is written
   *    dtype = T_LOG
   *         a logical value, either 'T' or 'F' in column 30 is written
   *    dtype = T_INT
   *         an integer is written, right justified in columns 11-30
   *    dtype = T_STR
   *         a string 'svalue' of length kvalue is written, in columns 9-80.
   */

  int l;
  char *sp;

  for (l=0, sp=card; l<80; l++,sp++) *sp=' ';

  l = strlen(name);
  if (l) xvbcopy(name, card, (size_t) l);   /* copy name */

  if (dtype == T_NOVAL) return;

  if (dtype == T_STR) {
    l = kvalue;
    if (l <= 0) return;
    if (l > 72) l = 72;
    xvbcopy(svalue, &card[8], (size_t) l);
    return;
  }

  card[8] = '=';

  if (dtype == T_LOG)
    card[29] = kvalue ? 'T' : 'F';
  else { /* T_INT */
    sprintf(&card[10], "%20d", kvalue);
    card[30] = ' ';
  }
}


/************************************/
static const char *rdcard(card, name, dtype, kvalue)
     char *card;
     const char *name;
     DATTYPE dtype;   /* type of value */
     long int *kvalue;
{
  /* Read a header record, from the 80 byte buffer card.
   * the keyword name must match 'name'; and parse its value according to
   * dtype. This can have the following values:
   *    dtype = T_LOG
   *        value is logical, either 'T' or 'F' in column 30.
   *    dtype = T_INT
   *        value is an integer, right justified in columns 11-30.
   *
   * The value is stored in kvalue.
   * It returns NULL on success, or an error message otherwise.
   */

  int         i, ptr;
  char        namestr[9];
  static char error[45];

  xvbcopy(card, namestr, (size_t) 8);

  for (i=7; i>=0 && namestr[i] == ' '; i--);
  namestr[i+1] = '\0';

  if (strcmp(namestr, name) != 0) {
    sprintf(error, "Keyword %s not found in FITS file", name);
    return error;
  }


  /* get start of value */
  ptr = 10;
  while (ptr < 80 && card[ptr] == ' ') ptr++;
  if (ptr == 80) return "FITS file has missing keyword value"; /* no value */

  if (dtype == T_LOG) {
    if (ptr != 29 || (card[29] != 'T' && card[29] != 'F'))
      return "Keyword has bad logical value in FITS file";
    *kvalue = (card[29] == 'T');
  }

  else {  /* an integer */
    int j;
    long int ival;
    char num[21];

    if (ptr > 29) return "Keyword has bad integer value in FITS file";
    xvbcopy(&card[ptr], num, (size_t) (30-ptr));
    num[30-ptr] = '\0';
    j = sscanf(num, "%ld", &ival);
    if (j != 1) return "Keyword has bad integer value in FITS file";
    *kvalue = ival;
  }

  return NULL;
}


/************************************/
static int ftgdata(fs, buffer, nelem)
     FITS *fs;
     void *buffer;
     int nelem;
{
  /* reads nelem values into the buffer.
   * returns NULL for success or an error message.
   * Copes with the fact that the last 2880 byte record of the FITS file
   * may be truncated, and should be padded out with zeros.
   *  bitpix   type of data
   *    8        byte
   *   16        short int       (NOT 2-byte integer)
   *   32        int             (NOT 4-byte integer)
   *  -32        float
   *  -64        double
   *
   * Returns the number of elements actually read.
   */

  int res;

  if (nelem == 0) return 0;

  res = fread(buffer, (size_t) fs->size, (size_t) nelem, fs->fp);
  /* if failed to read all the data because at end of file */
  if (res != nelem && feof(fs->fp)) {
    /* nblock is the number of elements in a record.
       size is always a factor of BLOCKSIZE */

    int loffs, nblock=BLOCKSIZE/fs->size;

    /*
     * the last record might be short; check this.
     * loffs is the offset of the start of the last record from the current
     * position.
     */

    loffs = ((fs->ndata + nblock - 1) / nblock - 1) * nblock - fs->cpos;

    /* if we read to the end of the penultimate record */
    if (res >= loffs) {
      /* pad with zeros */
      xvbzero((char *)buffer+res*fs->size, (size_t) ((nelem-res)*fs->size));
      res = nelem;
    }
  }

  fs->cpos += res;
  if (res) ftfixdata(fs, buffer, res);
  return res;
}


/************************************/
static void ftfixdata(fs, buffer, nelem)
     FITS *fs;
     void *buffer;
     int nelem;
{
  /* convert the raw data, as stored in the FITS file, to the format
   * appropiate for the data representation of the host computer.
   * Assumes that
   *  short int = 2 or more byte integer
   *  int       = 4 or more byte integer
   *  float     = 4 byte floating point, not necessarily IEEE.
   *  double    = 8 byte floating point.
   * This can exit for lack of memory on the first call with float or
   * double data.
   */

  int   i, n=nelem;
  byte *ptr=buffer;

  /*
   * conversions. Although the data may be signed, reverse using unsigned
   * variables.
   * Because the native int types may be larger than the types in the file,
   * we start from the end and work backwards to avoid overwriting data
   * prematurely.
   */
  /* convert from big-endian two-byte signed integer to native form */

  if (fs->bitpix == 16) {
    unsigned short int *iptr=(unsigned short int *)ptr;
    iptr += n-1;    /* last short int */
    ptr += (n-1)*2; /* last pair of bytes */
    for (i=0; i < n; i++, ptr-=2, iptr--)
      *iptr = (((int)*ptr) << 8) | (int)(ptr[1]);
    }

  /* convert from big-endian four-byte signed integer to native form */
  else if (fs->bitpix == 32) {
    unsigned int *iptr = (unsigned int *)ptr;
    iptr += n-1;     /* last integer */
    ptr  += (n-1)*4; /* last 4 bytes */
    for (i=0; i < n; i++, ptr-=4, iptr--)
      *iptr = ((unsigned int)ptr[0] << 24) |
	      ((unsigned int)ptr[1] << 16) |
	      ((unsigned int)ptr[2] << 8)  |
	      ((unsigned int)ptr[3]);
  }

  /* convert from IEE 754 single precision to native form */
  else if (fs->bitpix == -32) {
    int j, k, expo;
    static float *exps=NULL;

    if (exps == NULL) {
      exps = (float *)malloc(256 * sizeof(float));
      if (exps == NULL) FatalError("Insufficient memory for exps store");
      exps[150] = 1.;
      for (i=151; i < 256; i++) exps[i] = 2.*exps[i-1];
      for (i=149; i >= 0; i--) exps[i] = 0.5*exps[i+1];
    }

    for (i=0; i < n; i++, ptr+=4) {
      k = (int)*ptr;
      j = ((int)ptr[1] << 16) | ((int)ptr[2] << 8) | (int)ptr[3];
      expo = ((k & 127) << 1) | (j >> 23);
      if ((expo | j) == 0) *(float *)ptr = 0.;
      else *(float *)ptr = exps[expo]*(float)(j | 0x800000);
      if (k & 128) *(float *)ptr = - *(float *)ptr;
    }

  }

  /* convert from IEE 754 double precision to native form */
  else if (fs->bitpix == -64) {
    int expo, k, l;
    unsigned int j;
    static double *exps=NULL;

    if (exps == NULL) {
      exps = (double *)malloc(2048 * sizeof(double));
      if (exps == NULL) FatalError("Insufficient memory for exps store");
      exps[1075] = 1.;
      for (i=1076; i < 2048; i++) exps[i] = 2.*exps[i-1];
      for (i=1074; i >= 0; i--) exps[i] = 0.5*exps[i+1];
    }

    for (i=0; i < n; i++, ptr+=8) {
      k = (int)*ptr;
      j = ((unsigned int)ptr[1] << 24) | ((unsigned int)ptr[2] << 16) |
	((unsigned int)ptr[3] << 8) | (unsigned int)ptr[4];
      l = ((int)ptr[5] << 16) | ((int)ptr[6] << 8) | (int)ptr[7];
      expo = ((k & 127) << 4) | (j >> 28);
      if ((expo | j | l) == 0) *(double *)ptr = 0.;
      else *(double *)ptr = exps[expo] * (16777216. *
		         (double)((j&0x0FFFFFFF)|0x10000000) + (double)l);
      if (k & 128) *(double *)ptr = - *(double *)ptr;
    }
  }
}

#define maxmin(x, max, min) {\
  maxmin_t=(x);  if(maxmin_t > max) max=maxmin_t; \
  if (maxmin_t<min) min=maxmin_t;}


/************************************/
static int ftgbyte(fs, cbuff, nelem)
     FITS *fs;
     byte *cbuff;
     int nelem;
{
  /* Reads a byte image from the FITS file fs. The image contains nelem pixels.
   * If bitpix = 8, then the image is loaded as stored in the file.
   * Otherwise, it is rescaled so that the minimum value is stored as 0, and
   * the maximum is stored as 255.
   * Returns the number of pixels read.
   */

  void *voidbuff;
  int i, n, nrd, bufsize, overflow=0;

  /* if the data is byte, then read it directly */
  if (fs->bitpix == 8)
    return ftgdata(fs, cbuff, nelem);

  /* allocate a buffer to store the image */
  if (fs->bitpix == 16) {
    bufsize = nelem * sizeof(short int);
    if (bufsize/nelem != (int)sizeof(short int))
      overflow = 1;
  } else if (fs->bitpix == 32) {
    bufsize = nelem * sizeof(int);
    if (bufsize/nelem != (int)sizeof(short int))
      overflow = 1;
  } else {
    bufsize = nelem * fs->size;  /* float, double */
    if (bufsize/nelem != fs->size)
      overflow = 1;
  }

  if (overflow) {
    SetISTR(ISTR_WARNING, "FITS image dimensions out of range");
    return 0;
  }

  voidbuff = (void *)malloc((size_t) bufsize);
  if (voidbuff == NULL) {
    char emess[60];
    sprintf(emess, "Insufficient memory for raw image of %d bytes",
	    nelem*fs->size);
    FatalError(emess);
  }

  nrd = ftgdata(fs, voidbuff, nelem);
  if (nrd == 0) return 0;
  n = nrd;

  /* convert short int to byte */
  if (fs->bitpix == 16) {
    short int *buffer=voidbuff;
    int max, min, maxmin_t;
    float scale;

    min = max = buffer[0];
    for (i=1; i < n; i++, buffer++) maxmin(*buffer, max, min);
    scale = (max == min) ? 0. : 255./(float)(max-min);

    /* rescale and convert */
    for (i=0, buffer=voidbuff; i < n; i++)
      cbuff[i] = (byte)(scale*(float)((int)buffer[i]-min));

    /* convert long int to byte */
  }

  else if (fs->bitpix == 32) {
    int *buffer=voidbuff;
    int max, min, maxmin_t;
    float scale, fmin;

    min = max = buffer[0];
    for (i=1; i < n; i++, buffer++) maxmin(*buffer, max, min);
    scale = (max == min) ? 1. : 255./((double)max-(double)min);
    fmin = (float)min;

    /* rescale and convert */
    if (scale < 255./2.1e9) /* is max-min too big for an int ? */
      for (i=0, buffer=voidbuff; i < n; i++)
	cbuff[i] = (byte)(scale*((float)buffer[i]-fmin));
    else /* use integer subtraction */
      for (i=0, buffer=voidbuff; i < n; i++)
	cbuff[i] = (byte)(scale*(float)(buffer[i]-min));


  }

  /* convert float to byte */
  else if (fs->bitpix == -32) {
    float *buffer=voidbuff;
    float max, min, maxmin_t, scale;

    min = max = buffer[0];
    for (i=1; i < n; i++, buffer++) maxmin(*buffer, max, min);
    scale = (max == min) ? 0. : 255./(max-min);

    /* rescale and convert */
    for (i=0, buffer=voidbuff; i < n; i++)
      cbuff[i] = (byte)(scale*(buffer[i]-min));

  }

  /* convert double to byte */
  else if (fs->bitpix == -64) {
    double *buffer=voidbuff;
    double max, min, maxmin_t, scale;

    min = max = buffer[0];
    for (i=1; i < n; i++, buffer++) maxmin(*buffer, max, min);
    scale = (max == min) ? 0. : 255./(max-min);

    /* rescale and convert */
    for (i=0, buffer=voidbuff; i < n; i++)
      cbuff[i] = (byte)(scale*(buffer[i]-min));
  }

  free(voidbuff);
  return n;
}

#undef maxmin


/************************************/
static void flip(buffer, nx, ny)
     byte *buffer;
     int nx;
     int ny;
{
  /* reverse order of lines in image */

  int i;
  int j, v;
  byte *buff1, *buff2;

  for (i=0; i < ny/2; i++) {
    buff1 = &buffer[i*nx];
    buff2 = &buffer[(ny-1-i)*nx];
    for (j=0; j < nx; j++) {
      v = *buff1;
      *(buff1++) = *buff2;
      *(buff2++) = v;
    }
  }
}

