/*
 * xvpictoppm.c - reads XV 'thumbnail' files from stdin, writes standard PPM
 *   files to stdout
 */

/*
 *  THUMBNAIL FILE FORMAT:
 *
 * <magic number 'P7 332' >
 * <comment identifying version of XV that wrote this file>
 * <comment identifying type & size of the full-size image>
 * <OPTIONAL comment identifying this as a BUILT-IN icon, in which case
 *    there is no width,height,maxval info, nor any 8-bit data>
 * <comment signifying end of comments>
 * <width, height, and maxval of this file >
 * <raw binary 8-bit data, in 3/3/2 Truecolor format>
 *
 * Example:
 *    P7 332
 *    #XVVERSION:Version 2.28  Rev: 9/26/92
 *    #IMGINFO:512x440 Color JPEG
 *    #END_OF_COMMENTS
 *    48 40 255
 *    <binary data>
 *
 * alternately:
 *    P7 332
 *    #XVVERSION:Version 2.28 Rev: 9/26/92
 *    #BUILTIN:UNKNOWN
 *    #IMGINFO:
 *    #END_OF_COMMENTS
 */


#include "xv.h"
#include "copyright.h"



/*************** Function Declarations *************/
       int   main          PARM((int, char **));
static void  errexit       PARM((void));
static byte *loadThumbFile PARM((int *, int *));
static void  writePPM      PARM((byte *, int, int));



/****************************/
int main(argc, argv)
     int    argc;
     char **argv;
{
  byte *pic;
  int   w,h;

  pic = loadThumbFile(&w, &h);
  writePPM(pic, w, h);

  return 0;
}


/****************************/
static void errexit()
{
  perror("Unable to convert thumbnail file");
  exit(-1);
}


/****************************/
static byte *loadThumbFile(wptr, hptr)
     int  *wptr, *hptr;
{
  /* read a thumbnail file from stdin */

  FILE *fp;
  byte  *icon8, *pic24, *ip, *pp;
  char  buf[256];
  int   i, builtin, w, h, maxval, npixels, p24sz;

  fp = stdin;
  builtin = 0;

  if (!fgets(buf, 256, fp))               errexit();
  if (strncmp(buf, "P7 332", (size_t) 6)) errexit();

  /* read comments until we see '#END_OF_COMMENTS', or hit EOF */
  while (1) {
    if (!fgets(buf, 256, fp)) errexit();

    if (!strncmp(buf, "#END_OF_COMMENTS", (size_t) 16)) break;

    else if (!strncmp(buf, "#BUILTIN:",   (size_t)  9)) {
      builtin = 1;
      fprintf(stderr, "Built-in icon:  no image to convert\n");
      exit(1);
    }
  }


  /* read width, height, maxval */
  if (!fgets(buf, 256, fp) || sscanf(buf, "%d %d %d", &w, &h, &maxval) != 3)
    errexit();

  npixels = w * h;
  p24sz = 3 * npixels;

  if (w <= 0 || h <= 0 || maxval != 255 || npixels/w != h || p24sz/3 != npixels)
  {
    fprintf(stderr, "Thumbnail dimensions out of range\n");
    exit(1);
  }


  /* read binary data */
  icon8 = (byte *) malloc((size_t) npixels);
  if (!icon8) errexit();

  i = fread(icon8, (size_t) 1, (size_t) npixels, fp);
  if (i != npixels) errexit();


  /* make 24-bit version of icon */
  pic24 = (byte *) malloc((size_t) p24sz);
  if (!pic24) errexit();

  /* convert icon from 332 to 24-bit image */
  for (i=0, ip=icon8, pp=pic24;  i<npixels;  i++, ip++, pp+=3) {
    pp[0] = ( ((int) ((*ip >> 5) & 0x07)) * 255) / 7;
    pp[1] = ( ((int) ((*ip >> 2) & 0x07)) * 255) / 7;
    pp[2] = ( ((int) ((*ip >> 0) & 0x03)) * 255) / 3;
  }

  free(icon8);

  *wptr = w;  *hptr = h;
  return pic24;
}


/*******************************************/
static void writePPM(pic, w, h)
     byte *pic;
     int   w,h;
{
  FILE *fp;
  byte *pix;
  int   i,j,len;

  fp = stdout;

  fprintf(fp,"P6\n");
  fprintf(fp,"%d %d %d\n", w, h, 255);

  if (ferror(fp)) errexit();

  for (i=0, pix=pic, len=0; i<h; i++) {
    for (j=0; j<w; j++, pix+=3) {
      putc(pix[0],fp);  putc(pix[1],fp);  putc(pix[2],fp);
    }
  }

  if (ferror(fp)) errexit();
}
