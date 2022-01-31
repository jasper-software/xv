/*
 * xvxpm.c - load routine for X11 XPM v3 format pictures
 *
 * LoadXPM(fname,pinfo)
 * WriteXPM(fp,pic,ptype,w,h,rp,gp,bp,nc,col,name)
 */

/*
 * Written by Chris P. Ross (cross@eng.umd.edu) to support XPM v3
 * format images.
 *
 * Thanks go to Sam Yates (syates@spam.maths.adelaide.edu.au) for
 * provideing inspiration.
 */

#define VALUES_LEN	80	/* Max length of values line */
#define TOKEN_LEN	8	/* Max length of color token */

#include "xv.h"


/*
 * File format:
 *  (A file format def should go here)
 *
 */

typedef struct hent {
  char        token[TOKEN_LEN];
  union {
    byte      index;
    byte      rgb[3];
  } color_val;
  struct hent *next;
} hentry;

#define cv_index	color_val.index
#define cv_rgb		color_val.rgb



/* Local (Global) Variables */
static byte	hex[256];
static hentry **hashtab;        /* Hash table */
static int      hash_len;       /* number of hash buckets */
static int	bufchar;	/* Buffered character from XpmGetc */
static short	in_quote;	/* Is the current point in the file in */
                                /*  a quoted string? */

/* Local Functions */
static int     XpmLoadError  PARM((char*, char*));
static int     XpmGetc	     PARM((FILE*));
static int     hash          PARM((char *));
static int     hash_init     PARM((int));
static int     hash_insert   PARM((hentry *));
static hentry *hash_search   PARM((char *));
static void    hash_destroy  PARM((void));


/**************************************/
int LoadXPM(fname, pinfo)
     char *fname;
     PICINFO *pinfo;
{
  /* returns '1' on success */
  
  FILE    *fp;
  hentry   item;
  int      c;
  char    *bname;
  char     values[VALUES_LEN];
  byte    *pic;
  byte    *i_sptr;		/* image search pointer */
  long     filesize;
  int      w, h, nc, cpp, line_pos;
  short    i, j, k;		/* for() loop indexes */
  hentry  *clmp;		/* colormap hash-table */
  hentry  *c_sptr;		/* cmap hash-table search pointer*/
  XColor   col;
  
  bname = BaseName(fname);
  fp = fopen(fname, "r");
  if (!fp)
    return (XpmLoadError(bname, "couldn't open file"));
  
  if (DEBUG)
    printf("LoadXPM(): Loading xpm from %s\n", fname);
  
  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);
  
  bufchar = -2;
  in_quote = FALSE;
  
  /* Read in the values line.  It is the first string in the
   * xpm, and contains four numbers.  w, h, num_colors, and
   * chars_per_pixel. */
  
  /* First, get to the first string */
  while (((c = XpmGetc(fp))!=EOF) && (c != '"')) ;
  line_pos = 0;
  
  /* Now, read in the string */
  while (((c = XpmGetc(fp))!=EOF) && (line_pos < VALUES_LEN) && (c != '"')) {
    values[line_pos++] = c;
  }
  if (c != '"')
    return (XpmLoadError(bname, "error parsing values line"));
  
  values[line_pos] = '\0';
  sscanf(values, "%d%d%d%d", &w, &h, &nc, &cpp);
  if (nc <= 0 || cpp <= 0)
    return (XpmLoadError(bname, "No colours in Xpm?"));
  
  if (nc > 256)
    pinfo->type = PIC24;
  else
    pinfo->type = PIC8;
  
  if (DEBUG)
    printf("LoadXPM(): reading a %dx%d image (%d colors)\n", w, h, nc);
  
  /* We got this far... */
  WaitCursor();
  
  if (!hash_init(nc))
    return (XpmLoadError(bname, "Not enough memory to hash colormap"));
  
  clmp = (hentry *) malloc(nc * sizeof(hentry)); /* Holds the colormap */
  if (pinfo->type == PIC8) pic = (byte *) malloc((size_t) (w*h));
                      else pic = (byte *) malloc((size_t) (w*h*3));
  
  if (!clmp || !pic)
    return (XpmLoadError(bname, "Not enough memory to load pixmap"));
  
  c_sptr = clmp;
  i_sptr = pic;
  
  /* initialize the 'hex' array for zippy ASCII-hex -> int conversion */
  
  for (i = 0 ; i < 256 ; i++)   hex[i] = 0;
  for (i = '0'; i <= '9' ; i++) hex[i] = i - '0';
  for (i = 'a'; i <= 'f' ; i++) hex[i] = i - 'a' + 10;
  for (i = 'A'; i <= 'F' ; i++) hex[i] = i - 'A' + 10;
  
  /* Again, we've made progress. */
  WaitCursor();
  
  /* Now, we need to read the colormap. */
  pinfo->colType = F_BWDITHER;
  for (i = 0 ; i < nc ; i++) {
    while (((c = XpmGetc(fp))!=EOF) && (c != '"')) ;
    if (c != '"')
      return (XpmLoadError(bname, "Error reading colormap"));
    
    for (j = 0 ; j < cpp ; j++)
      c_sptr->token[j] = XpmGetc(fp);
    c_sptr->token[j] = '\0';
    
    while (((c = XpmGetc(fp))!=EOF) && ((c == ' ') || (c == '\t'))) ;
    if (c == EOF)		/* The failure condition of getc() */
      return (XpmLoadError(bname, "Error parsing colormap line"));
    
    do {
      char  key[3];
      char  color[40];	/* Need to figure a good size for this... */
      short hd;		/* Hex digits per R, G, or B */
      
      for (j=0; j<2 && (c != ' ') && (c != '\t') && (c != EOF); j++) {
	key[j] = c;
	c = XpmGetc(fp);
      }
      key[j] = '\0';

      while (((c = XpmGetc(fp))!=EOF) && ((c == ' ') || (c == '\t'))) ;
      if (c == EOF)	/* The failure condition of getc() */
	return (XpmLoadError(bname, "Error parsing colormap line"));

      for (j=0; j<39 && (c!=' ') && (c!='\t') && (c!='"') && c!=EOF; j++) {
	color[j] = c;
	c = XpmGetc(fp);
      }
      color[j]='\0';

      while ((c == ' ') || (c == '\t'))
	c = XpmGetc(fp);
      
      if (DEBUG > 1)
	printf("LoadXPM(): Got color key '%s', color '%s'\n",
	       key, color);
      
      if (key[0] == 's')	/* Don't find a color for a symbolic name */
	continue;
      
      if (XParseColor(theDisp,theCmap,color,&col)) {
	if (pinfo->type == PIC8) {
	  pinfo->r[i] = col.red >> 8;
	  pinfo->g[i] = col.green >> 8;
	  pinfo->b[i] = col.blue >> 8;
	  c_sptr->cv_index = i;

	  /* Is there a better way to do this? */
	  if (pinfo->colType != F_FULLCOLOR)
	    if (pinfo->colType == F_GREYSCALE)
	      if (pinfo->r[i] == pinfo->g[i] &&
		  pinfo->g[i] == pinfo->b[i])
		/* Still greyscale... */
		;
	      else
		/* It's color */
		pinfo->colType = F_FULLCOLOR;
	    else
	      if (pinfo->r[i] == pinfo->g[i] &&
		  pinfo->g[i] == pinfo->b[i])
		if ((pinfo->r[i] == 0 || pinfo->r[i] == 0xff) &&
		    (pinfo->g[i] == 0 || pinfo->g[i] == 0xff) &&
		    (pinfo->b[i] == 0 || pinfo->b[i] == 0xff))
		  /* It's B/W */
		  ;
		else
		  /* It's greyscale */
		  pinfo->colType = F_GREYSCALE;
	      else
		/* It's color */
		pinfo->colType = F_FULLCOLOR;
	  
	}
	else {   /* PIC24 */
	  c_sptr->cv_rgb[0] = col.red >> 8;
	  c_sptr->cv_rgb[1] = col.green >> 8;
	  c_sptr->cv_rgb[2] = col.blue >> 8;
	}
      }

      else {      /* 'None' or unrecognized color spec */
	int rgb;

	if (strcmp(color, "None") == 0) rgb = 0xb2c0dc;  /* infobg */
	else {
	  SetISTR(ISTR_INFO, "%s:  unknown color spec '%s'", bname, color);
	  Timer(1000);
	  rgb = 0x808080;
	}
	
	if (pinfo->type == PIC8) {
	  pinfo->r[i] = (rgb>>16) & 0xff;
	  pinfo->g[i] = (rgb>> 8) & 0xff;
	  pinfo->b[i] = (rgb>> 0) & 0xff;
	  c_sptr->cv_index = i;
	}
	else {
	  c_sptr->cv_rgb[0] = (rgb>>16) & 0xff;
	  c_sptr->cv_rgb[1] = (rgb>> 8) & 0xff;
	  c_sptr->cv_rgb[2] = (rgb>> 0) & 0xff;
	}
      }

      
      xvbcopy((char *) c_sptr, (char *) &item, sizeof(item));
      hash_insert(&item);
      
      if (DEBUG > 1) 
	printf("LoadXPM():  Cmap entry %d, 0x%02x 0x%02x 0x%02x, token '%s'\n",
	       i, pinfo->r[i], pinfo->g[i], pinfo->b[i], c_sptr->token);
      
      if (*key == 'c') {	/* This is the color entry, keep it. */
	while (c!='"' && c!=EOF) c = XpmGetc(fp);
	break;
      }
      
    } while (c != '"');
    c_sptr++;

    if (!(i%13)) WaitCursor();
  } /* for */
  

  if (DEBUG)
    printf("LoadXPM(): Read and stored colormap.\n");
  
  /* Now, read the pixmap. */
  for (i = 0 ; i < h ; i++) {
    while (((c = XpmGetc(fp))!=EOF) && (c != '"')) ;
    if (c != '"')
      return (XpmLoadError(bname, "Error reading colormap"));
    
    for (j = 0 ; j < w ; j++) {
      char pixel[TOKEN_LEN];
      hentry *mapentry;
      
      for (k = 0 ; k < cpp ; k++)
	pixel[k] = XpmGetc(fp);
      pixel[k] = '\0';

      if (!(mapentry = (hentry *) hash_search(pixel))) {
	/* No colormap entry found.  What do we do?  Bail for now */
	if (DEBUG)
	  printf("LoadXPM(): Found token '%s', can't find entry in colormap\n",
		 pixel);
	return (XpmLoadError(bname, "Can't map resolve into colormap"));
      }
      
      if (pinfo->type == PIC8)
	*i_sptr++ = mapentry->cv_index;
      else {
	*i_sptr++ = mapentry->cv_rgb[0];
	*i_sptr++ = mapentry->cv_rgb[1];
	*i_sptr++ = mapentry->cv_rgb[2];
      }
    }  /* for ( j < w ) */
    (void)XpmGetc(fp);		/* Throw away the close " */
  
    if (!(i%7)) WaitCursor();
  }  /* for ( i < h ) */
  
  pinfo->pic = pic;
  pinfo->normw = pinfo->w = w;
  pinfo->normh = pinfo->h = h;
  pinfo->frmType = F_XPM;

  if (DEBUG) printf("LoadXPM(): pinfo->colType is %d\n", pinfo->colType);
  
  sprintf(pinfo->fullInfo, "Xpm v3 Pixmap (%ld bytes)", filesize);
  sprintf(pinfo->shrtInfo, "%dx%d Xpm.", w, h);
  pinfo->comment = (char *)NULL;
  
  hash_destroy();
  free(clmp);
  
  if (fp != stdin)
    fclose(fp);
  
  return(1);
}


/***************************************/
static int XpmLoadError(fname, st)
     char *fname, *st;
{
  SetISTR(ISTR_WARNING, "%s:  %s", fname, st);
  return 0;
}


/***************************************/
static int XpmGetc(f)
     FILE *f;
{
  int	c, d, lastc;
  
  if (bufchar != -2) {
    /* The last invocation of this routine read the character... */
    c = bufchar;
    bufchar = -2;
    return(c);
  }
  
  if ((c = getc(f)) == EOF)
    return(EOF);
  
  if (c == '"')
    in_quote = !in_quote;
  else if (!in_quote && c == '/') {	/* might be a C-style comment */
    if ((d = getc(f)) == EOF)
      return(EOF);
    if (d == '*') {				/* yup, it *is* a comment */
      if ((lastc = getc(f)) == EOF)
	return(EOF);
      do {				/* skip through to the end */
	if ((c = getc(f)) == EOF)
	  return(EOF);
	if (lastc != '*' || c != '/')	/* end of comment */
	  lastc = c;
      } while (lastc != '*' || c != '/');
      if ((c = getc(f)) == EOF)
	return(EOF);
    } else					/* nope, not a comment */
      bufchar = d;
  }
  return(c);
}


/***************************************/
/*         hashing functions           */
/***************************************/


/***************************************/
static int hash(token) 
     char *token;
{
  int i, sum;

  for (i=sum=0; token[i] != '\0'; i++)
    sum += token[i];
  
  sum = sum % hash_len;
  return (sum);
}


/***************************************/
static int hash_init(hsize)
     int hsize;
{
  /*
   * hash_init() - This function takes an arg, but doesn't do anything with
   * it.  It could easily be expanded to figure out the "optimal" number of
   * buckets.  But, for now, a hard-coded 257 will do.  (Until I finish the
   * 24-bit image writing code.  :-)
   */

  int i;
  
  hash_len = 257;

  hashtab = (hentry **) malloc(sizeof(hentry *) * hash_len);
  if (!hashtab) {
    SetISTR(ISTR_WARNING, "Couldn't malloc hashtable in LoadXPM()!\n");
    return 0;
  }

  for (i = 0 ; i < hash_len ; i++)
    hashtab[i] = NULL;
  
  return 1;
}


/***************************************/
static int hash_insert(entry)
     hentry *entry;
{
  int     key;
  hentry *tmp;
  
  key = hash(entry->token);
  
  tmp = (hentry *) malloc(sizeof(hentry));
  if (!tmp) {
    SetISTR(ISTR_WARNING, "Couldn't malloc hash entry in LoadXPM()!\n");
    return 0;
  }
  
  xvbcopy((char *)entry, (char *)tmp, sizeof(hentry));
  
  if (hashtab[key]) tmp->next = hashtab[key];
               else tmp->next = NULL;
  
  hashtab[key] = tmp;
  
  return 1;
}


/***************************************/
static hentry *hash_search(token)
     char *token;
{
  int     key;
  hentry *tmp;
  
  key = hash(token);
  
  tmp = hashtab[key];
  while (tmp && strcmp(token, tmp->token)) {
    tmp = tmp->next;
  }

  return tmp;
}


/***************************************/
static void hash_destroy()
{
  int     i;
  hentry *tmp;
  
  for (i=0; i<hash_len; i++) {
    while (hashtab[i]) {
      tmp = hashtab[i]->next;
      free(hashtab[i]);
      hashtab[i] = tmp;
    }
  }
  
  free(hashtab);
  return;
}



/**************************************************************************/
int WriteXPM(fp, pic, ptype, w, h, rp, gp, bp, nc, col, name, comments)
     FILE *fp;			/* File to write to */
     byte *pic;			/* Image data */
     int ptype;			/* PIC8 or PIC24 */
     int w, h;			/* width, & height */
     byte *rp, *gp, *bp;	/* Colormap pointers */
     int nc, col;		/* number of colors, & colorstyle */
     char *name;		/* name of file (/image) */
     char *comments;		/* image comments (not currently used */
{
  /* Note here, that tokenchars is assumed to contain 64 valid token */
  /* characters.  It's hardcoded to assume this for benefit of generating */
  /* tokens, when there are more than 64^2 colors. */
  
  short	i, imax, j;	/* for() loop indices */
  short	cpp = 0;
  char	*tokenchars = 
            ".#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  char	*tokens;
  char	image_name[256], *foo;
  byte	grey;

#ifndef USE_UNFINISHED_24BIT_WRITING_CODE
  byte	*pic8, *sptr;
  byte	rtemp[256], gtemp[256], btemp[256], hist[256], trans[256];
  long	li;		/* for() loop index */
  int	numcol;
#endif
  
  if (DEBUG)
    if (ptype == PIC8)
      printf("WriteXPM(): Write a %d color, colortype %d, PIC8 image.\n",
	     nc, col);
    else
      printf("WriteXPM(): Write a colortype %d, PIC24 image.\n", col);
  
  foo = BaseName(name);
  strcpy(image_name, foo);
  foo = (char *)strchr(image_name, '.');
  if (foo)
    *foo = '\0';		/* Truncate name at first '.' */
  
#ifdef USE_UNFINISHED_24BIT_WRITING_CODE
  if (ptype == PIC24)
    return -1;
#else
  /* XXX - The colormap returned from Conv24to8 is not a specific  */
  /* size (and Conv24to8 doesn't tell me how many entries there    */
  /* are, or not in this rev of code at least) and not necessarily */
  /* 'packed'.  Code in here to do that should be removed if       */
  /* Conv24to8 is "fixed" to do this...                            */
  /*    Chris P. Ross (cross@eng.umd.edu)  28-Sept-94              */
  
  numcol = 0;
  
  if (ptype == PIC24) {
    /* Reduce to an 8-bit image.  Would be nice to actually write */
    /* the 24-bit image.  I'll have to code that someday...       */
    pic8 = Conv24to8(pic, w, h, 256, rtemp, gtemp, btemp);
    if (!pic8) {
      SetISTR(ISTR_WARNING, 
	      "%s:  Unable to convert to 8-bit image in WriteXPM()",
	      image_name);
      return 1;
    }
    if (DEBUG) printf("WriteXPM(): Converted 24bit image.\n");

    /* This will count the number of colors in use, and build an */
    /* index array into the colormap (which may not be 'packed') */
    /* Thanks to John Bradley for this code..                    */

    xvbzero((char *) hist, sizeof(hist));
    numcol = 0;

    if (DEBUG)
      printf("WriteXPM(): Counting colors (width: %d, height: %d)...\n", w, h);

    for (li = 0, sptr = pic8 ; li < (w*h) ; li++, sptr++) {
      hist[*sptr] = 1;
    }

    if (DEBUG)
      printf("WriteXPM(): building translation table...\n");
    for (i = 0 ; i < 256 ; i++) {
      trans[i] = numcol;
      rtemp[numcol] = rtemp[i];
      gtemp[numcol] = gtemp[i];
      btemp[numcol] = btemp[i];
      numcol += hist[i];
    }
    rp=rtemp; gp=gtemp; bp=btemp;
    if (DEBUG)
      printf("WriteXPM(): Converted 24bit image now has %d colors.\n", numcol);
  } else {
    pic8 = pic;
    numcol = nc;
  }
#endif

  
#ifdef USE_UNFINISHED_24BIT_WRITING_CODE
  if (ptype == PIC24) cpp = 4;
  else if (numcol > 64) cpp = 2;
  else cpp = 1;
#else
  if (numcol > 64) cpp = 2;
  else cpp = 1;
#endif

  fprintf(fp, "/* XPM */\n");
  fprintf(fp, "static char *%s[] = {\n", image_name);
  fprintf(fp, "/* width height num_colors chars_per_pixel */\n");
  fprintf(fp, "\"   %3d   %3d   %6d            %1d\",\n", w, h, numcol, cpp);
  fprintf(fp, "/* colors */\n");
  
  switch (cpp) {

  case 1:			/* <= 64 colors; index into tokenchars */
    if (col == F_GREYSCALE)
      for (i = 0 ; i < numcol ; i ++) {
	grey = MONO(rp[i], gp[i], bp[i]);
	fprintf(fp, "\"%c c #%02x%02x%02x\",\n", tokenchars[i],
		grey, grey, grey);
      }
    else
      for (i = 0 ; i < numcol ; i ++)
	fprintf(fp, "\"%c c #%02x%02x%02x\",\n", tokenchars[i],
		rp[i], gp[i], bp[i]);
    fprintf(fp, "/* pixels */\n");
    for (i = 0 ; i < h ; i ++) {
      fprintf(fp, "\"");
      if (!(i%20))
	WaitCursor();
      for (j = 0 ; j < w ; j++) {
	if (rp == rtemp)
	  fprintf(fp, "%c", tokenchars[trans[*pic8++]]);
	else
	  fprintf(fp, "%c", tokenchars[*pic8++]);
      }
      if (i < h-1)
	fprintf(fp, "\",\n");
    }
    break;

  case 2:			/* 64 < colors < 64^2; build new token list */
    tokens = (char *) malloc((size_t) ((2*numcol) + 1) );
    if (numcol & 0x3f)
      imax = (numcol >> 6) + 1;
    else
      imax = (numcol >> 6);
    for (i = 0 ; i < imax ; i++)
      for (j =  0 ; j < 64 && ((i<<6)+j) < numcol ; j++) {
	*(tokens+((i<<6)+j)*2) = tokenchars[i];
	*(tokens+((i<<6)+j)*2+1) = tokenchars[j];
      }
    if (col == F_GREYSCALE)
      for (i = 0 ; i < numcol ; i++) {
	grey = MONO(rp[i], gp[i], bp[i]);
	fprintf(fp, "\"%c%c c #%02x%02x%02x\",\n", tokens[i*2],
		tokens[i*2+1], grey, grey, grey);
      }
    else
      for (i = 0 ; i < numcol ; i++)
	fprintf(fp, "\"%c%c c #%02x%02x%02x\",\n", tokens[i*2],
		tokens[i*2+1], rp[i], gp[i], bp[i]);
    fprintf(fp, "/* pixels */\n");
    for (i = 0 ; i < h ; i++) {
      fprintf(fp, "\"");
      if (!(i%13))
	WaitCursor();
      for (j = 0 ; j < w ; j++) {
	if (rp == rtemp)
	  fprintf(fp, "%c%c", tokens[trans[*pic8]*2],
		  tokens[(trans[*pic8]*2)+1]);
	else
	  fprintf(fp, "%c%c", tokens[(*pic8*2)],
		  tokens[(*pic8*2)+1]);
	pic8++;
      }
      if (i < h-1)
	fprintf(fp, "\",\n");
    }
    break;

  case 4:
    /* Generate a colormap */
    
    break;
  default:
    break;
  }
  
  if (fprintf(fp, "\"\n};\n") == EOF) {
    return 1;
  } else
    return 0;
}




