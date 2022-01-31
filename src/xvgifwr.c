/*
 * xvgifwr.c  -  handles writing of GIF files.  based on flgife.c and
 *               flgifc.c from the FBM Library, by Michael Maudlin
 *
 * Contains: 
 *   WriteGIF(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle,
 *            comment)
 *
 * Note: slightly brain-damaged, in that it'll only write non-interlaced 
 *       GIF files (in the interests of speed, or something)
 *
 */



/*****************************************************************
 * Portions of this code Copyright (C) 1989 by Michael Mauldin.
 * Permission is granted to use this file in whole or in
 * part for any purpose, educational, recreational or commercial,
 * provided that this copyright notice is retained unchanged.
 * This software is available to all free of charge by anonymous
 * FTP and in the UUNET archives.
 *
 *
 * Authors:  Michael Mauldin (mlm@cs.cmu.edu)
 *           David Rowley (mgardi@watdcsu.waterloo.edu)
 *
 * Based on: compress.c - File compression ala IEEE Computer, June 1984.
 *
 *	Spencer W. Thomas       (decvax!harpo!utah-cs!utah-gr!thomas)
 *	Jim McKie               (decvax!mcvax!jim)
 *	Steve Davies            (decvax!vax135!petsd!peora!srd)
 *	Ken Turkowski           (decvax!decwrl!turtlevax!ken)
 *	James A. Woods          (decvax!ihnp4!ames!jaw)
 *	Joe Orost               (decvax!vax135!petsd!joe)
 *****************************************************************/
 

#include "xv.h"

typedef long int        count_int;

static int  Width, Height;
static int  curx, cury;
static long CountDown;
static int  Interlace;
static byte bw[2] = {0, 0xff};

static void putword     PARM((int, FILE *));
static void compress    PARM((int, FILE *, byte *, int));
static void output      PARM((int));
static void cl_block    PARM((void));
static void cl_hash     PARM((count_int));
static void char_init   PARM((void));
static void char_out    PARM((int));
static void flush_char  PARM((void));


static byte pc2nc[256],r1[256],g1[256],b1[256];


/*************************************************************/
int WriteGIF(fp, pic, ptype, w, h, rmap, gmap, bmap, numcols, colorstyle,
	     comment)
     FILE *fp;
     byte *pic;
     int   ptype, w,h;
     byte *rmap, *gmap, *bmap;
     int   numcols, colorstyle;
     char *comment;
{
  int   RWidth, RHeight;
  int   LeftOfs, TopOfs;
  int   ColorMapSize, InitCodeSize, Background, BitsPerPixel;
  int   i,j,nc;
  byte *pic8;
  byte  rtemp[256],gtemp[256],btemp[256];

  if (ptype == PIC24) {  /* have to quantize down to 8 bits */
    pic8 = Conv24to8(pic, w, h, 256, rtemp,gtemp,btemp);
    if (!pic8) FatalError("Unable to malloc in WriteGIF()");
    rmap = rtemp;  gmap = gtemp;  bmap = btemp;  numcols=256;
  }
  else pic8 = pic;



  Interlace = 0;
  Background = 0;


  for (i=0; i<256; i++) { pc2nc[i] = r1[i] = g1[i] = b1[i] = 0; }

  /* compute number of unique colors */
  nc = 0;

  for (i=0; i<numcols; i++) {
    /* see if color #i is already used */
    for (j=0; j<i; j++) {
      if (rmap[i] == rmap[j] && gmap[i] == gmap[j] && 
	  bmap[i] == bmap[j]) break;
    }

    if (j==i) {  /* wasn't found */
      pc2nc[i] = nc;
      r1[nc] = rmap[i];
      g1[nc] = gmap[i];
      b1[nc] = bmap[i];
      nc++;
    }
    else pc2nc[i] = pc2nc[j];
  }


  /* figure out 'BitsPerPixel' */
  for (i=1; i<8; i++)
    if ( (1<<i) >= nc) break;
  
  BitsPerPixel = i;

  ColorMapSize = 1 << BitsPerPixel;
	
  RWidth  = Width  = w;
  RHeight = Height = h;
  LeftOfs = TopOfs = 0;
	
  CountDown = w * h;    /* # of pixels we'll be doing */

  if (BitsPerPixel <= 1) InitCodeSize = 2;
                    else InitCodeSize = BitsPerPixel;

  curx = cury = 0;

  if (!fp) {
    fprintf(stderr,  "WriteGIF: file not open for writing\n" );
    if (ptype == PIC24) free(pic8);
    return (1);
  }

  if (DEBUG) 
    fprintf(stderr,"WrGIF: pic=%lx, w,h=%dx%d, numcols=%d, Bits%d,Cmap=%d\n",
	    (u_long) pic8, w,h,numcols,BitsPerPixel,ColorMapSize);

  if (comment && strlen(comment) > (size_t) 0)
    fwrite("GIF89a", (size_t) 1, (size_t) 6, fp);    /* the GIF magic number */
  else
    fwrite("GIF87a", (size_t) 1, (size_t) 6, fp);    /* the GIF magic number */

  putword(RWidth, fp);           /* screen descriptor */
  putword(RHeight, fp);

  i = 0x80;	                 /* Yes, there is a color map */
  i |= (8-1)<<4;                 /* OR in the color resolution (hardwired 8) */
  i |= (BitsPerPixel - 1);       /* OR in the # of bits per pixel */
  fputc(i,fp);          

  fputc(Background, fp);         /* background color */

  fputc(0, fp);                  /* future expansion byte */


  if (colorstyle == 1) {         /* greyscale */
    for (i=0; i<ColorMapSize; i++) {
      j = MONO(r1[i], g1[i], b1[i]);
      fputc(j, fp);
      fputc(j, fp);
      fputc(j, fp);
    }
  }
  else {
    for (i=0; i<ColorMapSize; i++) {       /* write out Global colormap */
      fputc(r1[i], fp);
      fputc(g1[i], fp);
      fputc(b1[i], fp);
    }
  }

  if (comment && strlen(comment) > (size_t) 0) {   /* write comment blocks */
    char *sp;
    int   i, blen;

    fputc(0x21, fp);     /* EXTENSION block */
    fputc(0xFE, fp);     /* comment extension */

    sp = comment;
    while ( (blen=strlen(sp)) > 0) {
      if (blen>255) blen = 255;
      fputc(blen, fp);
      for (i=0; i<blen; i++, sp++) fputc(*sp, fp);
    }
    fputc(0, fp);    /* zero-length data subblock to end extension */
  }


  fputc( ',', fp );              /* image separator */

  /* Write the Image header */
  putword(LeftOfs, fp);
  putword(TopOfs,  fp);
  putword(Width,   fp);
  putword(Height,  fp);
  if (Interlace) fputc(0x40, fp);   /* Use Global Colormap, maybe Interlace */
            else fputc(0x00, fp);

  fputc(InitCodeSize, fp);
  compress(InitCodeSize+1, fp, pic8, w*h);

  fputc(0,fp);                      /* Write out a Zero-length packet (EOF) */
  fputc(';',fp);                    /* Write GIF file terminator */

  if (ptype == PIC24) free(pic8);

  if (ferror(fp)) return -1;
  return (0);
}




/******************************/
static void putword(w, fp)
int w;
FILE *fp;
{
  /* writes a 16-bit integer in GIF order (LSB first) */
  fputc(w & 0xff, fp);
  fputc((w>>8)&0xff, fp);
}




/***********************************************************************/


static unsigned long cur_accum = 0;
static int           cur_bits = 0;




#define min(a,b)        ((a>b) ? b : a)

#define XV_BITS	12    /* BITS was already defined on some systems */
#define MSDOS	1

#define HSIZE  5003            /* 80% occupancy */

typedef unsigned char   char_type;


static int n_bits;                    /* number of bits/code */
static int maxbits = XV_BITS;         /* user settable max # bits/code */
static int maxcode;                   /* maximum code, given n_bits */
static int maxmaxcode = 1 << XV_BITS; /* NEVER generate this */

#define MAXCODE(n_bits)     ( (1 << (n_bits)) - 1)

static  count_int      htab [HSIZE];
static  unsigned short codetab [HSIZE];
#define HashTabOf(i)   htab[i]
#define CodeTabOf(i)   codetab[i]

static int hsize = HSIZE;            /* for dynamic table sizing */

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i) CodeTabOf(i)
#define tab_suffixof(i)        ((char_type *)(htab))[i]
#define de_stack               ((char_type *)&tab_suffixof(1<<XV_BITS))

static int free_ent = 0;                  /* first unused entry */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
static int clear_flg = 0;

static long int in_count = 1;            /* length of input */
static long int out_count = 0;           /* # of codes output (for debugging) */

/*
 * compress stdin to stdout
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the 
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

static int g_init_bits;
static FILE *g_outfile;

static int ClearCode;
static int EOFCode;


/********************************************************/
static void compress(init_bits, outfile, data, len)
int   init_bits;
FILE *outfile;
byte *data;
int   len;
{
  register long fcode;
  register int i = 0;
  register int c;
  register int ent;
  register int disp;
  register int hsize_reg;
  register int hshift;

  /*
   * Set up the globals:  g_init_bits - initial number of bits
   *                      g_outfile   - pointer to output file
   */
  g_init_bits = init_bits;
  g_outfile   = outfile;

  /* initialize 'compress' globals */
  maxbits = XV_BITS;
  maxmaxcode = 1<<XV_BITS;
  xvbzero((char *) htab,    sizeof(htab));
  xvbzero((char *) codetab, sizeof(codetab));
  hsize = HSIZE;
  free_ent = 0;
  clear_flg = 0;
  in_count = 1;
  out_count = 0;
  cur_accum = 0;
  cur_bits = 0;


  /*
   * Set up the necessary values
   */
  out_count = 0;
  clear_flg = 0;
  in_count = 1;
  maxcode = MAXCODE(n_bits = g_init_bits);

  ClearCode = (1 << (init_bits - 1));
  EOFCode = ClearCode + 1;
  free_ent = ClearCode + 2;

  char_init();
  ent = pc2nc[*data++];  len--;

  hshift = 0;
  for ( fcode = (long) hsize;  fcode < 65536L; fcode *= 2L )
    hshift++;
  hshift = 8 - hshift;                /* set hash code range bound */

  hsize_reg = hsize;
  cl_hash( (count_int) hsize_reg);            /* clear hash table */

  output(ClearCode);
    
  while (len) {
    c = pc2nc[*data++];  len--;
    in_count++;

    fcode = (long) ( ( (long) c << maxbits) + ent);
    i = (((int) c << hshift) ^ ent);    /* xor hashing */

    if ( HashTabOf (i) == fcode ) {
      ent = CodeTabOf (i);
      continue;
    }

    else if ( (long)HashTabOf (i) < 0 )      /* empty slot */
      goto nomatch;

    disp = hsize_reg - i;           /* secondary hash (after G. Knott) */
    if ( i == 0 )
      disp = 1;

probe:
    if ( (i -= disp) < 0 )
      i += hsize_reg;

    if ( HashTabOf (i) == fcode ) {
      ent = CodeTabOf (i);
      continue;
    }

    if ( (long)HashTabOf (i) >= 0 ) 
      goto probe;

nomatch:
    output(ent);
    out_count++;
    ent = c;

    if ( free_ent < maxmaxcode ) {
      CodeTabOf (i) = free_ent++; /* code -> hashtable */
      HashTabOf (i) = fcode;
    }
    else
      cl_block();
  }

  /* Put out the final code */
  output(ent);
  out_count++;
  output(EOFCode);
}


/*****************************************************************
 * TAG( output )
 *
 * Output the given code.
 * Inputs:
 *      code:   A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *              that n_bits =< (long)wordsize - 1.
 * Outputs:
 *      Outputs code to the file.
 * Assumptions:
 *      Chars are 8 bits long.
 * Algorithm:
 *      Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 */

static
unsigned long masks[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
                                  0x001F, 0x003F, 0x007F, 0x00FF,
                                  0x01FF, 0x03FF, 0x07FF, 0x0FFF,
                                  0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

static void output(code)
int code;
{
  cur_accum &= masks[cur_bits];

  if (cur_bits > 0)
    cur_accum |= ((long)code << cur_bits);
  else
    cur_accum = code;
	
  cur_bits += n_bits;

  while( cur_bits >= 8 ) {
    char_out( (int) (cur_accum & 0xff) );
    cur_accum >>= 8;
    cur_bits -= 8;
  }

  /*
   * If the next entry is going to be too big for the code size,
   * then increase it, if possible.
   */

  if (free_ent > maxcode || clear_flg) {

    if( clear_flg ) {
      maxcode = MAXCODE (n_bits = g_init_bits);
      clear_flg = 0;
    }
    else {
      n_bits++;
      if ( n_bits == maxbits )
	maxcode = maxmaxcode;
      else
	maxcode = MAXCODE(n_bits);
    }
  }
	
  if( code == EOFCode ) {
    /* At EOF, write the rest of the buffer */
    while( cur_bits > 0 ) {
      char_out( (int)(cur_accum & 0xff) );
      cur_accum >>= 8;
      cur_bits -= 8;
    }

    flush_char();
	
    fflush( g_outfile );

#ifdef FOO
    if( ferror( g_outfile ) ) 
      FatalError("unable to write GIF file");
#endif
  }
}


/********************************/
static void cl_block ()             /* table clear for block compress */
{
  /* Clear out the hash table */

  cl_hash ( (count_int) hsize );
  free_ent = ClearCode + 2;
  clear_flg = 1;

  output(ClearCode);
}


/********************************/
static void cl_hash(hsize)          /* reset code table */
register count_int hsize;
{
  register count_int *htab_p = htab+hsize;
  register long i;
  register long m1 = -1;

  i = hsize - 16;
  do {                            /* might use Sys V memset(3) here */
    *(htab_p-16) = m1;
    *(htab_p-15) = m1;
    *(htab_p-14) = m1;
    *(htab_p-13) = m1;
    *(htab_p-12) = m1;
    *(htab_p-11) = m1;
    *(htab_p-10) = m1;
    *(htab_p-9) = m1;
    *(htab_p-8) = m1;
    *(htab_p-7) = m1;
    *(htab_p-6) = m1;
    *(htab_p-5) = m1;
    *(htab_p-4) = m1;
    *(htab_p-3) = m1;
    *(htab_p-2) = m1;
    *(htab_p-1) = m1;
    htab_p -= 16;
  } while ((i -= 16) >= 0);

  for ( i += 16; i > 0; i-- )
    *--htab_p = m1;
}


/******************************************************************************
 *
 * GIF Specific routines
 *
 ******************************************************************************/

/*
 * Number of characters so far in this 'packet'
 */
static int a_count;

/*
 * Set up the 'byte output' routine
 */
static void char_init()
{
	a_count = 0;
}

/*
 * Define the storage for the packet accumulator
 */
static char accum[ 256 ];

/*
 * Add a character to the end of the current packet, and if it is 254
 * characters, flush the packet to disk.
 */
static void char_out(c)
int c;
{
  accum[ a_count++ ] = c;
  if( a_count >= 254 ) 
    flush_char();
}

/*
 * Flush the packet to disk, and reset the accumulator
 */
static void flush_char()
{
  if( a_count > 0 ) {
    fputc(a_count, g_outfile );
    fwrite(accum, (size_t) 1, (size_t) a_count, g_outfile );
    a_count = 0;
  }
}	
