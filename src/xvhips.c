/*
 * xvhips.c - load routine for 'HIPS' format pictures
 *
 * LoadHIPS(fname, numcols)
 */

/*
 * Copyright 1989, 1990 by the University of Pennsylvania
 *
 * Permission to use, copy, and distribute for non-commercial purposes,
 * is hereby granted without fee, providing that the above copyright
 * notice appear in all copies and that both the copyright notice and this
 * permission notice appear in supporting documentation.
 *
 * The software may be modified for your own purposes, but modified versions
 * may not be distributed.
 *
 * This software is provided "as is" without any express or implied warranty.
 */

#include "xv.h"

#ifdef HAVE_HIPS

#define Boolean FREDDIE
#include "xvhips.h"
#undef Boolean

#include <alloca.h>

#define LINES 100
#define LINELENGTH 132

static int   fread_header(int fd, struct header *hd);
static char  *getline(int fd, char **s, int *l);
static int   dfscanf(int fd);
static void  make_grayscale(char *r, char *g, char *b);
static float hls_value (float n1, float n2, float hue);
static void  hls_to_rgb(float h, float l, float s,
                        float *r, float *g, float *b);
static void  make_huescale(char *r, char *g, char *b);
static void  make_heatscale(char *r, char *g, char *b);
static int   load_colourmap(char *filestem, int max_colours,
                            char *r, char *g, char *b);

/************************************************************************
 *
 * Read Header routines
 *
 ************************************************************************/

static char *ssave[LINES];
static int slmax[LINES];
static int lalloc = 0;
//extern char *calloc();



static int fread_header(fd, hd)
  int fd;
  struct header *hd;
{
  int lineno, len, i;
  char *s;

/*fprintf(stderr,"fread_header: entered\n");*/
  if(lalloc<1) {
    ssave[0] = calloc(LINELENGTH, sizeof (char));
    slmax[0] = LINELENGTH;
    lalloc = 1;
  }
/*fprintf(stderr,"fread_header: ssave allocated\n");*/
  getline(fd,&ssave[0],&slmax[0]);
  hd->orig_name = calloc(strlen(ssave[0])+1, sizeof (char));
  strcpy(hd->orig_name,ssave[0]);
  getline(fd,&ssave[0],&slmax[0]);
  hd->seq_name = calloc(strlen(ssave[0])+1, sizeof (char));
  strcpy(hd->seq_name,ssave[0]);
  hd->num_frame = dfscanf(fd);
  getline(fd,&ssave[0],&slmax[0]);
  hd->orig_date = calloc(strlen(ssave[0])+1, sizeof (char));
  strcpy(hd->orig_date,ssave[0]);
  hd->rows = dfscanf(fd);
  hd->cols = dfscanf(fd);
  hd->bits_per_pixel = dfscanf(fd);
  hd->bit_packing = dfscanf(fd);
  hd->pixel_format = dfscanf(fd);
  lineno = 0;
  len = 1;
  getline(fd,&ssave[0],&slmax[0]);
  s = ssave[0];
  while(*(s += strlen(s)-3) == '|') {
    len += strlen(ssave[lineno]);
    lineno++;
    if (lineno >= LINES)
      fprintf(stderr, "Too many lines in header history");
    if(lineno >= lalloc) {
      ssave[lineno] = calloc(LINELENGTH, sizeof (char));
      slmax[lineno] = LINELENGTH;
      lalloc++;
    }
    getline(fd,&ssave[lineno],&slmax[lineno]);
    s = ssave[lineno];
  }
  len += strlen(ssave[lineno]);
  hd->seq_history = calloc(len, sizeof (char));
  hd->seq_history[0] = '\0';
  for (i=0;i<=lineno;i++)
    strcat(hd->seq_history,ssave[i]);
  lineno = 0;
  len = 1;
  while(strcmp(getline(fd,&ssave[lineno],&slmax[lineno]),".\n")) {
    len += strlen(ssave[lineno]);
    lineno++;
    if (lineno >= LINES)
      fprintf(stderr, "Too many lines in header desc.");
    if(lineno >= lalloc) {
      ssave[lineno] = calloc(LINELENGTH, sizeof (char));
      slmax[lineno] = LINELENGTH;
      lalloc++;
    }
  }
  hd->seq_desc = calloc(len, sizeof (char));
  *hd->seq_desc = '\0';
  for (i=0;i<lineno;i++)
    strcat(hd->seq_desc,ssave[i]);
/*fprintf(stderr,"fread_header: exiting\n");*/
  return 0;
}



static char *getline(fd,s,l)
  int fd;
  char **s;
  int *l;
{
  int i,m;
  char c,*s1,*s2;

  i = 0;
  s1 = *s;
  m = *l;
  while(read(fd,&c,1) == 1 && c != '\n') {
    if (m-- <= 2) {
      s2 = calloc(LINELENGTH+*l,sizeof (char));
      strcpy(s2,*s);
      *s = s2;
      *l += LINELENGTH;
      m = LINELENGTH;
      s1 = s2 + strlen(s2);
    }
    *s1++ = c;
  }
  if (c == '\n') {
    *s1++ = '\n';
    *s1 = '\0';
    return *s;
  }
  fprintf(stderr, "Unexpected EOF while reading header.");
  return NULL;
}



static int dfscanf(fd)
  int fd;
{
  int i;

  getline(fd,&ssave[0],&slmax[0]);
  sscanf(ssave[0],"%d",&i);
  return(i);
}



/*******************************************/
int LoadHIPS(fname,pinfo)
     char *fname;
     PICINFO * pinfo;
/*******************************************/
{
  FILE  *fp;
  struct header h;
  char * pic;

  /* open the stream, if necesary */
  fp=fopen(fname,"r");
  if (!fp) return 0;

  if (!fread_header(fileno(fp), &h)) {
    SetISTR(ISTR_WARNING,"Can't read HIPS header");
    return 0;
  }

  pinfo->w = h.cols;
  pinfo->h = h.rows;
  pic = pinfo->pic = (byte *) malloc(h.rows * h.cols);   // GRR POSSIBLE OVERFLOW / FIXME
  if (!pic) FatalError("couldn't malloc HIPS file");

  if (!fread(pic, 1, h.cols*h.rows, fp)) {
    SetISTR(ISTR_WARNING,"Error reading HIPS data.\n");
    return 0;
  }
  fclose (fp);

  pinfo->frmType = F_SUNRAS;
  pinfo->colType = F_FULLCOLOR;
  sprintf(pinfo->fullInfo, "HIPS file (%d bytes)", h.cols*h.rows);
  sprintf(pinfo->shrtInfo, "HIPS file.");
  pinfo->comment = (char *) NULL;

  {
    char cmapname[256];
    /* Check header for colormap spec */
    char * s = h.seq_desc - 1;
    char * cmaptag = "+COLORMAP";
    int sl = strlen(cmaptag);
    cmapname[0] = 0;
    while (*++s)
      if (*s == '+')
	if (strncmp(s, cmaptag, sl) == 0) {
	  char * p = s + sl;
	  while (*p && (*p == ' ' || *p == '\n' || *p == '\t')) p++;
	  sscanf(p, "%s", cmapname);
	  SetISTR(ISTR_INFO, cmapname);
	  fprintf(stderr, "Colormap = [%s]\n", cmapname);
	}

    if (strcmp(cmapname, "gray") == 0 || strcmp(cmapname, "grey") == 0)
      make_grayscale(pinfo->r, pinfo->g, pinfo->b);
    else if (strcmp(cmapname, "heat") == 0)
      make_heatscale(pinfo->r, pinfo->g, pinfo->b);
    else if (strcmp(cmapname, "hues") == 0)
      make_huescale(pinfo->r, pinfo->g, pinfo->b);
    else if (!cmapname[0] || !load_colourmap(cmapname, 256, pinfo->r, pinfo->g, pinfo->b))
      make_grayscale(pinfo->r, pinfo->g, pinfo->b);
    sprintf(pinfo->fullInfo, "HIPS file (%d x %d), Colormap = [%s]", h.cols, h.rows, cmapname);
  }

  return 1;
}



static void make_grayscale(char * r, char * g, char * b)
{
  int i;
  /* default grayscale colors */
  r[0] = 40; g[0] = 150; b[0] = 100; /* "green4" background */
  for(i = 1; i < 256; i++)
    r[i] = g[i] = b[i] = i;
}



static float hls_value (n1, n2, hue)
  float n1,n2,hue;
{
	if (hue>360.0)
	  hue-=360.0 ;
	else if (hue<0.0)
	  hue+=360.0 ;

	if (hue<60.0)
	  return( n1+(n2-n1)*hue/60.0 ) ;
	else if (hue<180.0)
	  return ( n2 ) ;
	else if (hue<240.0)
	  return ( n1+(n2-n1)*(240.0-hue)/60.0 ) ;
	else
	  return (n1) ;
}



static void hls_to_rgb(h,l,s,  r,g,b)
  float h, l, s;
  float *r, *g, *b;
{
	static float m1, m2 ;

	if (l<=0.5)
		m2=l*(1+s) ;
	else
		m2=l+s-l*s ;
	m1=2.0*l-m2 ;
	if (s==0.0) *r=*g=*b=l ;
	else {
		*r=hls_value(m1,m2,h+120.0) ;
		*g=hls_value(m1,m2,h) ;
		*b=hls_value(m1,m2,h-120.0) ;
	}

}



static void make_huescale(char * r, char * g, char * b)
{
  int j;
  r[0] = g[0] = b[0] = 0;
  for (j = 1; j<256; j++)
    {
      float fr, fg, fb;
      hls_to_rgb((double)(256.0-j)*360.0/256.0, 0.5, 1.0, &fr, &fg, &fb);
      r[j] = rint(255*fr);
      g[j] = rint(255*fg);
      b[j] = rint(255*fb);
    }
}



static void make_heatscale(char * r, char * g, char * b)
{
  int j;
  r[0] = g[0] = b[0] = 0;
  for (j = 1; j<256; j++)
    {
      if(j<255/2)
	r[j] = j*255/(255/2-1);
      else
	r[j]=255;
      if (j>=255/2+255/3)
	g[j] = 255;
      else if (j>255/3)
	g[j] = (j-255/3)*255/(255/2-1);
      else
	g[j] = 0;
      if (j>255/2)
	b[j] = (j-255/2)*255/(255-255/2-1);
      else
	b[j] = 0;
    }
}



static int load_colourmap(char *filestem, int max_colours,
                          char *r, char *g, char *b)
{
  FILE * fp;
  int numread=0;
  char * filename;
  char str[200];
  int num_colors;
  /*
   * Look for palette file in local directory
   */

  filename = (char*)alloca(strlen(filestem) + 5);
  strcpy(filename, filestem);
  strcat(filename, ".PAL"); /* Add the PAL suffix to the name specified */
  fp = fopen(filename,"r");
  if (!fp) {
    /*
     * If not found, try in $IM2HOME/etc/palettes
     */
    char * im2home = (char*)getenv("IM2HOME");
    char * palette_subdirectory = "etc/palettes";
    char * fullfilename;
    if (!im2home)
      {
	im2home = "/home/jewel/imagine2";
	fprintf(stderr,"IM2HOME environment variable not set -- using [%s]\n",im2home);
      }
    fullfilename = alloca(strlen(im2home)+strlen(palette_subdirectory)+strlen(filename)+5);
    sprintf(fullfilename, "%s/%s/%s",im2home,palette_subdirectory,filename);
    fp = fopen(fullfilename,"r");
    if (!fp)
      {
	fprintf(stderr,"Couldn't find any palette file -- looked for [%s] and [%s].\n",
		filename,fullfilename);
	perror("Last system error message was");
	return 0;
      }
  }

  strcpy(str,"(null)");
  if (!fscanf(fp,"%s\n",str) || strncmp(str,"Palette",7) != 0) {
    fprintf(stderr,"error: First line of palette file should be `Palette', not [%s]\n", str);
    return 0;
  }

  fscanf(fp,"%[^\n]",str) ; /* Scan to end of line */
  fscanf (fp,"%d",&num_colors);/* Read the number of colours in the file */
  fgets(str,120,fp) ; /* Skip the text description, and general info lines */
  fgets(str,120,fp) ;

  while ((numread<max_colours)&&(numread<num_colors)) {
    int rc, gc, bc;
    fscanf (fp,"%d %d %d -", &rc, &gc, &bc) ; /* Get the (r,g,b) tuples */
    r[numread] = rc;
    g[numread] = gc;
    b[numread] = bc;
    numread++;
    fgets(str,120,fp) ; /* Skip the description, if present */
  }

  SetISTR(ISTR_INFO,"Read %d colors from palette file [%s]", numread, filename);
  return (numread) ; /* Return the number of colours ACTUALLY READ */
}

#endif /* HAVE_HIPS */
