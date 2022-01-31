/*
 * xvpds.c - load routine for astronomical PDS/VICAR format pictures
 * version of 6-4-93
 *
 * Anthony A. Datri
 * Siemens Corporate Research
 * 755 College Road East
 * Princeton NJ 08540
 * aad@scr.siemens.com
 *
 * Alexandra Aarhus-Dusenberg
 * alexd@world.std.com
 *
 * 5-3-93    added leading spaces back into sscanf's -- some huffmanized
 	       files have them.
 * 5-2-93    slapped in Peter G. Ford <pgf@space.mit.edu>'s patches for
 	     xv 3.00:
   Adds code to Makefile to compile under SunOS 4.0.3 (as if anybody cares).
   Improves PDS/VICAR file recognition.
   Writes current directory name in Doggie's window title.
   Handles (most) PDS and VICAR images correctly.
   Choice of algorithm for 16->8 bit conversion--linear or histogram stretch.
    (adds CONV24_HIST item in "24/8 bit" pull-down menu.)
   Uses any "palette.tab" file in cwd to color PDS/VICAR image.

 * 9-2-91    began integration.	 Much of this code is lifted from vicar.c,
 	     which I wrote for xloadimage.  This is a little simpler, though.

 * 10-17-91  pdsuncomp is called with system(), which typically feeds the
 	     commandline to sh.  Make sure that your .profile adds wherever
 	     you have pdsuncomp to the PATH, like

 		PATH=$PATH:/usr/local/bin

 * 11-15-91  substituted vdcomp from Viking CD's for pdsuncomp. I added
             recognition of - and shut off various messages

 * 1-5-92    merged into xv rel 2

 * 3-11-92   cleaned up some comments

 * 3-24-92   Got some new CD's from NASA of mosics and other processed Viking
             stuff.  There are actually records terminated with CRNLCR in
             these images, as well as ones that identify the spacecraft name
             as {VIKING_ORBITER_1, VIKING_ORBITER_2}.  I hacked up the code
             yet further to deal with these.  There's a Sun 4 XView binary for
             an image display program on these discs, but it's nowhere near as
             neat as the good Mr. Bradley's XV.


 * Sources of these CD's:
 *
 *  National Space Science Data Center
 *  Goddard Space Flight Center
 *  Code 933.4
 *  Greenbelt, Maryland
 *  (301) 286-6695
 *   or call
 *  (301) 286-9000 (300,1200,2400 bps)
 *   or telnet
 *  nssdca.gsfc.nasa.gov (128.183.10.4) and log in as 'NODIS' (no password).
 *
 *  Randy Davis
 *  LASP
 *  University of Colorado
 *  Boulder CO 80309-0392
 *  (303) 492-6867
 *
 * These CD's are reasonably priced.  Encourage the continue production
 * of them by buying a set.
 *
 * There are three types of files that we deal with here.  I'll call them
 * PDS-labeled, PDS-labeled Huffman-encoded, and VICAR.  Each consists of data
 * prefixed with a set of ASCII headers.  PDS-labeled and VICAR files are raw
 * grayscale data, the dimensions of which can be found from the headers.
 * PDS-labeled, Huffman-encoded files have the image information adaptively
 * Huffman-encoded, and the encoding histogram follows the ASCII headers.
 * To decode these, we use a slightly modified version of "vdcomp.c" from the
 * NASA Viking CD-ROMS.  For xv to work, you need to have vdcomp compiled
 * and in your search path.  vdcomp.c should be included with this distribution.
 *
 * I've heard that newer discs have FITS images on them.  If they do, support
 * for them will be added when I get one.  Until then, you can use fitstopgm.
 *
 * LoadPDS(fname, numcols)  -  coerces a PDS/VICAR image
 * WriteVICAR(fp, pic, w, h, r,g,b, numcols, style)
 */

/*
 * Copyright 1989, 1990 by Anthony A. Datri
 *
 * Permission to use, copy, and distribute for non-commercial purposes,
 * is hereby granted without fee, providing that the above copyright
 * notice appear in all copies, that both the copyright notice and this
 * permission notice appear in supporting documentation.
 *
 * In exception to the above, permission to John Bradley is hereby granted to
 * distribute this code as he sees fit within the context of his "xv" image
 * viewer.
 *
 * This software is provided "as is" without any express or implied warranty.
 */

#define  NEEDSDIR       /* for S_IRUSR|S_IWUSR */
#include "xv.h"

#ifdef HAVE_PDS


#define PDSFIXED        (1)
#define PDSVARIABLE     (2)
#define PDSTRASH        (-1)
#define VICAR           (3)
#define VIKINGFIXED     (4) /* Viking discs have a unique variant */
#define VIKINGVARIABLE  (5)


#ifndef TRUE
#define TRUE (1)
#define FALSE (0)
#endif

#define MAX_SIZE	20480   /* a guess -- even magellan images aren't
				    bigger than 2k x 2k */
#define RTBUFFSIZE	62	/* small buffer for magic */
#define FNAMESIZE	1024	/* too hard to generalize really getting it */
#define PDSUNCOMP	"vdcomp" /* filter to un-Huffmanize */

/* This is arbitrary.  Everything I've seen so far fits in 50 chars */
#define COMMENTSIZE	50
#define INOTESIZE	1000


static int	lastwasinote = FALSE;
static char	scanbuff         [MAX_SIZE+1],
                rtbuff         [RTBUFFSIZE+1],
		inote	        [INOTESIZE+1],
                infobuff      [COMMENTSIZE+1],
		spacecraft    [COMMENTSIZE+1],
		target        [COMMENTSIZE+1],
		filtname      [COMMENTSIZE+1],
		gainmode      [COMMENTSIZE+1],
		editmode      [COMMENTSIZE+1],
		scanmode      [COMMENTSIZE+1],
		exposure      [COMMENTSIZE+1],
		shuttermode   [COMMENTSIZE+1],
		mphase        [COMMENTSIZE+1],
		iname         [COMMENTSIZE+1],
		itime         [COMMENTSIZE+1],
                garbage       [1024],
		*tmptmp,
		pdsuncompfname[FNAMESIZE];

#define SSTR(l)			"%" #l "s"
#define S(l)			SSTR(l)

byte *image;
static int elaphe;



/* local function declarations */
static int getpdsrec          PARM((FILE *, char *));
static int Convert16BitImage  PARM((char *, PICINFO *, int));
static int LoadPDSPalette     PARM((char *, PICINFO *));



/* Get a NULL-, CR-, or CRLF-terminated record from a PDS file  */
/* The method of termination is unpredictable.  A pox on VMS.   */
/* Sometimes we even get mixtures of CR's and LF's.  Sigh.      */

static int getpdsrec(f,buff)
     FILE *f;
     char *buff;
{
  static char *bp;
  static int count;
  int c;

  /* read any leading CR's or LF's (sigh) */
  *buff='\0';
  bp=buff;
  count = 0;
  while (count==0) {
    c=fgetc(f);
    switch (c) {
    case EOF: return(0);
    case '\r':
    case '\n': continue;
    default:  ungetc(c,f);
      count = -1;
    }
  }

  count=0;
  bp=buff;
  while (1) {
    c=fgetc(f);
    switch (c) {

    case '\r':  *bp='\0';
                switch (c=fgetc(f)) {
		  case EOF:
		  case '\n':  break;
                  default:    ungetc(c,f);
		  }
                return(count);

    case EOF:	*bp='\0';  return(count);

    case '\0':  return(count);

    default:	count++;  *bp++ = c;
    }
  }
}


/* A VICAR image seems to begin with a one-line header describing all sorts
 * of things, almost all of which don't mean much to Joe User, who just
 * wants to see what Venus looks like.  The first thing in this label is a
 * string defining the size of the label, like "LBLSIZE=2048".  Then there's
 * other crud of variable usefulness.  Important to us are the NL and NS
 * fields, which seem to tell us the number of columns and rows, respectively.
 * Among these other fields seem to be the local map coordinates of the region.
 * These files are easy -- you can even convert one to usefulness by grabbing
 * the dimensions and lblsize and feeding to rawtopgm.  Example, if lblsize,
 * nl, and ns are 2048, 1536, and 2048, respectively, use "rawtopgm -headerskip
 * 2048 2048 1536 <foo.img >foo.pgm.  All of the samples I've seen are 8 bits
 * deep, so we assume that.  Yeah, yeah, that's nasty and evil, and I'm
 * terribly ashamed of it, but I'm not going to bother with bit fiddling
 * until I see an image that needs it.  I once had a 16-bit Galileo image of
 * Venus, but it got lost (sigh).
 *
 * These images are typically fairly big, so it makes sense to verify the
 * format before we try to read it all in.
 *
 * PDS images are prefixed like this:
 *
 *  od -a c2061139.imq | head -a
 *  0000000    - nul   N   J   P   L   1   I   0   0   P   D   S   1   0   0
 *
 * so we'll read off the first two chars, which seem to be a length field
 * of some kind, then look for NJPL..  Images on the Sampler
 * disc seem to leave off the first two bytes.  Sigh.  This may sometimes be
 * a distinction between the fixed and variable-record files.
 */

/*******************************************/
int LoadPDS(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
{
  /* returns '1' on success, '0' on failure */

  int tempnum, bytewidth, bufsize;
#ifndef USE_MKSTEMP
  int tmpfd;
#endif
  FILE	*zf;
  static int isfixed,teco,i,j,itype,vaxbyte,
             recsize,hrecsize,irecsize,isimage,labelrecs,labelsofar,
             w,h,lpsize,lssize,samplesize,returnp,labelsize,yy;
  char	*tmp;
  const char   *ftypstr;
  unsigned long filesize;
  char  sampletype[64];

  pinfo->type = PIC8;
  isfixed = TRUE;
  returnp = isimage = FALSE;
  itype   = PDSTRASH;

  teco = i = j = recsize = hrecsize = irecsize = labelrecs = w = h = 0;
  lpsize = lssize = samplesize = labelsize = labelsofar = 0;

  (*pdsuncompfname) = (*iname) = (*target) = (*filtname) = (*garbage) = '\0';
  (*itime) = (*spacecraft) = (*scanbuff) = (*mphase) = (*rtbuff) = '\0';
  (*sampletype) = (*inote) = (*infobuff) = (*gainmode) = (*editmode) = '\0';
  (*scanmode) = (*exposure) = (*shuttermode) = '\0';

  /* there may be some wisdom in statically mallocing space for an 800x800
     image, since that's what the Voyager cd's have, or even a 2kx2k, since
     some of the Magellan images from Ames are that size */

  zf = xv_fopen(fname,"r");
  if (!zf) {
    SetISTR(ISTR_WARNING,"LoadPDS: can't open %s\n",fname);
    return 0;
  }

  /* figure out the file size (for Informational Purposes Only) */
  fseek(zf, 0L, 2);
  filesize = ftell(zf);
  fseek(zf, 0L, 0);

  /* read the magic.  If it's got two bytes of crud in front of it, it's
     a screwy "variable-length-record" file. We have to jump through
     bloody VAX hoops to find out how long the record is.  Sigh.  Have
     people never heard of newlines? */

  if ((teco=fread(rtbuff,(size_t) 13, (size_t) 1,zf)) != 1) {
    SetISTR(ISTR_WARNING,"LoadPDS: couldn't read magic #: %d,%s",teco,fname);
    fclose(zf);
    return 0;
  }

  /* this makes sscanf so much happier */
  rtbuff[RTBUFFSIZE-1] = '\0';

  if (strncmp(rtbuff, "NJPL1I00PDS", (size_t) 11) == 0) {
    itype=PDSFIXED;
  } else if (strncmp(rtbuff+2,"NJPL1I00PDS", (size_t) 11) == 0) {
    itype=PDSVARIABLE;
  } else if (strncmp(rtbuff,"CCSD3Z", (size_t) 6) == 0) {
    itype=VIKINGFIXED; /* unique variant of PDS on Viking discs (browse) */
  } else if (strncmp(rtbuff+2,"CCSD3Z", (size_t) 6) == 0) {
    itype=VIKINGVARIABLE;
  } else if (sscanf(rtbuff,"LBLSIZE = %d%n",&labelsize,&labelsofar) == 1) {
    itype=VICAR;
  } else {
    SetISTR(ISTR_WARNING,"LoadPDS: can't handle this file, for some reason\n");
    fclose(zf);
    return 0;
  }

  switch (itype) {
  case PDSFIXED:
  case VICAR:
  case VIKINGFIXED:     isfixed=TRUE;
                        sprintf(pinfo->fullInfo,
				"PDS/VICAR, 8 bits per pixel. (%ld bytes)",
				filesize);
                        break;
  case PDSVARIABLE:
  case VIKINGVARIABLE:  isfixed=FALSE;
                        sprintf(pinfo->fullInfo,
			"Huffman-encoded PDS, 8 bits per pixel. (%ld bytes)",
				filesize);
  }

  if ((itype == PDSFIXED) || (itype == PDSVARIABLE) ||
      (itype == VIKINGFIXED) || (itype == VIKINGVARIABLE)) {

    if (isfixed == FALSE) {
      teco = ((*rtbuff) + ((*(rtbuff+1)) << 8));
      fread(rtbuff, (size_t) ((teco%2 ? teco+1 : teco) - 11),(size_t) 1, zf);
    }

    /* We know that we've got a PDS file of some sort.  We have to
       search through the labels looking for things because usage doesn't seem
       to be consistent, or even predictable. */

    for (;;) {
      if (isfixed) {
	if (getpdsrec(zf,scanbuff) == 0) {
	  SetISTR(ISTR_WARNING,"corrupt or incomplete PDS labels (low fread)");
	  break;
	}
      } else {
	/* AAAARGGGGH!  We've got a bloody variable-length file where
	 * the headers are preceded by a byte count, as a VAX 16-bit
	 * integer of all things.
  	 *
	 * "A compressed image file is composed of variable-length records
	 * defined according to the ISO standard.  Each variable length
	 * record starts with a record length indicator, stored as a 16-bit
	 * integer, followed by the number of bytes indicated in the record
	 * length indicator. If the length indicator is odd, then a pad byte
	 * is appended to the end of the record so that all records contain
	 * an even number of bytes." */

	i=getc(zf);
	j=getc(zf);
	if (j == EOF) {
	  SetISTR(ISTR_WARNING,"LoadPDS: j is EOF\n");
	  fclose(zf);
	  return 0;
	}

	teco = i + (j << 8);
	if (teco % 2) teco++;

	if (fread(scanbuff, (size_t) teco, (size_t) 1, zf) != 1) {
	  SetISTR(ISTR_WARNING,"LoadPDS: bad fread reading labels\n");
	  fclose(zf);
	  return 0;
	}

	scanbuff[teco]='\0';
      }

      /* otay, we've managed to wrestle a header of some sort from the
	 file -- now we grep through until we hit the END, at which point
	 we break out of the loop and see what we've got.  In the future,
	 we'll check for informational headers and write them out if the
	 fool user wants them.  There seems to be disagreement about what
	 the headers are called, since we might find, for example, either
	 TARGET_BODY or TARGET_NAME.  Bloody 'ell.  I think the problem
	 is that there isn't an actual *standard* for these formats, or
	 the standard isn't widely available, so
	 people kinda make it up as they go along. */

      if (strcmp(scanbuff,"END") == 0) {
	break;
      } else if (sscanf(scanbuff, " RECORD_TYPE = " S(RTBUFFSIZE), rtbuff) == 1) {
	if (strncmp(rtbuff,"VARIABLE_LENGTH", (size_t) 15) == 0) {
	  /*		itype=PDSVARIABLE; */
	} else if (strncmp(rtbuff,"FIXED_LENGTH", (size_t) 12) == 0) {
	  /*		itype=PDSFIXED;*/
	} else {
	  rtbuff[RTBUFFSIZE-1]='\0'; /* juuuust in case */
	  SetISTR(ISTR_WARNING,
		  "LoadPDS: unsupported record type \"%s\"\n",rtbuff);
	  fclose(zf);
	  return 0;
	}
	lastwasinote=FALSE;
      } else if (sscanf(scanbuff," RECORD_BYTES = %d",&recsize) == 1) {
	/* these default to RECORD_BYTES unless explicitly set */
	if (hrecsize == 0) hrecsize=recsize;
	    if (irecsize == 0) irecsize=recsize;
	lastwasinote=FALSE;
	continue;
      } else if (sscanf(scanbuff, " FILE_TYPE = " S(RTBUFFSIZE), rtbuff) != 0) {
	lastwasinote=FALSE;
	if (strncmp(rtbuff,"IMAGE", (size_t) 5) == 0) {
	  isimage=TRUE;
	  continue;
	} else {
	  isimage=FALSE;
	  break;
	}
      } else if ((sscanf(scanbuff," HEADER_RECORDS = %d",&labelrecs) == 1) ||
		 (sscanf(scanbuff," LABEL_RECORDS = %d", &labelrecs) == 1)) {
	lastwasinote=FALSE;
	continue;
      } else if (sscanf(scanbuff," IMAGE_LINES = %d",&h) == 1) {
	isimage=TRUE; lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff," LINE_SAMPLES = %d",&w) == 1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff," LINES = %d",&h) == 1) {
	isimage=TRUE; lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff," HEADER_RECORD_BYTES = %d",&hrecsize)==1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff," IMAGE_RECORD_BYTES = %d",&irecsize)==1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff," LINE_PREFIX_BYTES = %d",&lpsize)==1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff," LINE_SUFFIX_BYTES = %d",&lssize)==1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff," SAMPLE_BITS = %d", &samplesize) == 1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff, " SAMPLE_TYPE = " S(64), sampletype) == 1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff," SPACECRAFT_NAME = " S(COMMENTSIZE) " " S(1023),
			spacecraft,garbage) == 2 ) {
	const char *tmp = xv_strstr(scanbuff, spacecraft) + strlen(spacecraft);
	strncat(spacecraft, tmp, COMMENTSIZE - strlen(spacecraft));
	lastwasinote=FALSE;  continue;
      } else if (sscanf(scanbuff, " SPACECRAFT_NAME = " S(COMMENTSIZE), spacecraft) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " TARGET_NAME = " S(COMMENTSIZE), target) == 1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff, " TARGET_BODY = " S(COMMENTSIZE), target) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " MISSION_PHASE_NAME = " S(COMMENTSIZE), mphase) == 1) {
	lastwasinote=FALSE; continue;
      } else if (sscanf(scanbuff, " MISSION_PHASE = " S(COMMENTSIZE), mphase) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " INSTRUMENT_NAME = " S(COMMENTSIZE), iname) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " GAIN_MODE_ID = " S(COMMENTSIZE), gainmode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " INSTRUMENT_GAIN_STATE = " S(COMMENTSIZE), gainmode) ==1 ) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " EDIT_MODE_ID = " S(COMMENTSIZE), editmode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " INSTRUMENT_EDIT_MODE = " S(COMMENTSIZE), editmode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " SCAN_MODE_ID = " S(COMMENTSIZE), scanmode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " INSTRUMENT_SCAN_RATE = " S(COMMENTSIZE), scanmode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " SHUTTER_MODE_ID = " S(COMMENTSIZE), shuttermode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " INSTRUMENT_SHUTTER_MODE = " S(COMMENTSIZE), shuttermode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " SCAN_MODE_ID = " S(COMMENTSIZE), scanmode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " INSTRUMENT_SCAN_RATE = " S(COMMENTSIZE), scanmode) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " SPACECRAFT_EVENT_TIME = " S(COMMENTSIZE), itime) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " IMAGE_TIME = " S(COMMENTSIZE), itime) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " FILTER_NAME = " S(COMMENTSIZE), filtname) == 1) {
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, " INSTRUMENT_FILTER_NAME = " S(COMMENTSIZE), filtname) == 1) {
	lastwasinote=FALSE; continue;

      } else if ((sscanf(scanbuff, " EXPOSURE_DURATION = " S(COMMENTSIZE), exposure) == 1)
	      || (sscanf(scanbuff, " INSTRUMENT_EXPOSURE_DURATION = " S(COMMENTSIZE),
			 exposure)) == 1) {
	tmptmp = (char *) index(scanbuff,'=');
	tmptmp++;
	while((*tmptmp) == ' ')
	    tmptmp++;
	strcpy(exposure,tmptmp);
	lastwasinote=FALSE; continue;

      } else if (sscanf(scanbuff, "NOTE = " S(INOTESIZE), inote) == 1) {
	tmptmp = (char *) index(scanbuff,'='); tmptmp++;
	while (((*tmptmp) == ' ') || ((*tmptmp)  == '"')) tmptmp++;
	strncpy(inote, tmptmp, INOTESIZE - 1);
	strcat(inote," ");

	/*   evil and somewhat risky:  A "note" (really, any textual
	 *    field) can span records until " is reached.  If I ever
	 *     get my hands on the clown who designed this format...
	 *                             What we basically assume here
	 *        is that a NOTE record that doesn't end with a " is
	 *    followed by some number of continuations, one of which
	 *   will have a " in it.  If this turns out to not be true,
	 *          well, we'll segmentation fault real soon. We use
	 * lastwasinote as a semaphore to indicate that the previous
	 *       record was an unfinished NOTE record.  We clear the
	 *      flag in each of the above record types for potential
	 *   error recovery, although it really breaks up the beauty
	 * of the cascading sscanfs.  Dykstra'd love me for this one */

	if (inote[strlen(inote)-1] != '"')
	    lastwasinote=TRUE;
	else lastwasinote=FALSE;
	continue;

      } else if (lastwasinote) {
	tmptmp=scanbuff;
	while (((*tmptmp) == ' ') || ((*tmptmp)  == '"')) tmptmp++;
	strncat(inote, tmptmp, INOTESIZE - strlen(inote) - 1);
	strcat(inote," ");
	if (index(tmptmp,'"') != NULL)
	    lastwasinote=FALSE;
	continue;
      }
    }

    elaphe=strlen(inote)-1;
    while ( (inote[elaphe] == ' ') || (inote[elaphe] == '"')) {
	inote[elaphe--]='\0';
    }
    if ((isimage == TRUE) && (labelsize=(labelrecs * hrecsize))) {
      ;
    } else {
      SetISTR(ISTR_WARNING,
	      "This looks like a PDS/VICAR image, but not enough.\n");
      fclose(zf);
      return 0;
    }

    vaxbyte = strncmp(sampletype, "VAX_", (size_t) 4) == 0 ||
      strncmp(sampletype, "LSB_", (size_t) 4) == 0;

  } else if (itype == VICAR) {
    /* we've got a VICAR file.  Let's find out how big the puppy is */
    ungetc(' ', zf);
    --labelsofar;

    if (fread(scanbuff, (size_t) (labelsize-labelsofar),(size_t) 1,zf) == 1) {
      if ((tmp = (char *) xv_strstr(scanbuff," NL=")) == NULL) {
	SetISTR(ISTR_WARNING,"LoadPDS: bad NL in VICAR\n");
	returnp=TRUE;
      }

      if (sscanf(tmp," NL = %d",&h) != 1) {
	SetISTR(ISTR_WARNING,"LoadPDS: bad scan NL in VICAR\n");
	returnp=TRUE;
      }

      if ((tmp = (char *) xv_strstr(scanbuff, " NS=")) == NULL) {
	SetISTR(ISTR_WARNING,"LoadPDS: bad NS in VICAR\n");
	returnp=TRUE;
      }

      if (sscanf(tmp, " NS = %d",&w) != 1) {
	SetISTR(ISTR_WARNING,"LoadPDS: bad scan NS in VICAR\n");
	returnp=TRUE;
      }

      if ( (tmp=(char *) xv_strstr(scanbuff, " NBB=")))
        if (sscanf(tmp, " NBB = %d",&lpsize) != 1) {
	  SetISTR(ISTR_WARNING,"LoadPDS: bad scan NBB in VICAR\n");
	  returnp=TRUE;
        }

      vaxbyte = (xv_strstr(scanbuff, " INTFMT='HIGH'") == NULL);

      if (xv_strstr(scanbuff, " FORMAT='BYTE'"))
        samplesize = 8;
      else if (xv_strstr(scanbuff, " FORMAT='HALF'"))
        samplesize = 16;
      else if (xv_strstr(scanbuff, " FORMAT=")) {
	SetISTR(ISTR_WARNING,"LoadPDS: unsupported FORMAT in VICAR\n");
	returnp=TRUE;
      } else {
	SetISTR(ISTR_WARNING,"LoadPDS: bad FORMAT in VICAR\n");
	returnp=TRUE;
      }

    }

  } else {
    SetISTR(ISTR_WARNING,"LoadPDS: Unable to parse data.\n");
    returnp=TRUE;
  }

  /* samplesize can be arbitrarily large (up to int limit) in non-VICAR files */
  if (samplesize != 8 && samplesize != 16) {
    SetISTR(ISTR_WARNING,"LoadPDS: %d bits per pixel not supported",
      samplesize);
    returnp=TRUE;
  }

  if (returnp) {
    fclose(zf);
    return 0;
  }

  /* PDS files tend to have lots of information like this in them.  The
   * following are a few of the more interesting headers.  We'd do more, but
   * there's only so much space in the info box.  I tried to pick a
   * reasonable subset of these to display; if you want others, it's easy
   * to change the set.  XV 3.00 has a comment window where we write this
   * stuff in glorious detail.  Now, if I only had a clue as to what these
   * fields actually *mean* ... */

  *infobuff='\0';
  if (*spacecraft) {
    strncat(infobuff, spacecraft, sizeof(infobuff) - 1);
  }

  if (*target) {
    strncat(infobuff, ", ", sizeof(infobuff) - strlen(infobuff) - 1);
    strncat(infobuff, target, sizeof(infobuff) - strlen(infobuff) - 1);
  }

  if (*filtname) {
    strncat(infobuff, ", ", sizeof(infobuff) - strlen(infobuff) - 1);
    strncat(infobuff, filtname, sizeof(infobuff) - strlen(infobuff) - 1);
  }

  if (*itime) {
    strncat(infobuff, ", ", sizeof(infobuff) - strlen(infobuff) - 1);
    strncat(infobuff, itime, sizeof(infobuff) - strlen(infobuff) - 1);
  }

  SetISTR(ISTR_WARNING, "%s", infobuff);

  strncpy(pdsuncompfname,fname,sizeof(pdsuncompfname) - 1);
  ftypstr = "";

  switch (itype) {
  case VICAR:
    sprintf(pinfo->fullInfo,"VICAR, %d bits per pixel. (%ld bytes)",
      samplesize, filesize);
    ftypstr = "VICAR";
    rewind(zf);
    break;

  case PDSFIXED:
  case VIKINGFIXED:
    sprintf(pinfo->fullInfo,"PDS, %d bits per pixel. (%ld bytes)",
      samplesize, filesize);
    ftypstr = "PDS";
    rewind(zf);
    break;

  case PDSVARIABLE:
  case VIKINGVARIABLE:
    sprintf(pinfo->fullInfo,
	    "PDS, %d bits per pixel, Huffman-encoded. (%ld bytes)",
               samplesize, filesize);
    ftypstr = "PDS (Huffman)";
    fclose(zf);

#ifndef VMS
    snprintf(pdsuncompfname, sizeof(pdsuncompfname) - 1, "%s/xvhuffXXXXXX", tmpdir);
#else
    strcpy(pdsuncompfname,"sys$disk:[]xvhuffXXXXXX");
#endif

#ifdef USE_MKSTEMP
    close(mkstemp(pdsuncompfname));
#else
    mktemp(pdsuncompfname);
    tmpfd = open(pdsuncompfname,O_WRONLY|O_CREAT|O_EXCL,S_IRWUSR);
    if (tmpfd < 0) {
      SetISTR(ISTR_WARNING,"Unable to create temporary file.");
      return 0;
    }
    close(tmpfd);
#endif

#ifndef VMS
    sprintf(scanbuff,"%s '%s' - 4 > %s", PDSUNCOMP, fname, pdsuncompfname);
#else
    sprintf(scanbuff,"%s %s %s 4",PDSUNCOMP,fname,pdsuncompfname);
#endif

    SetISTR(ISTR_INFO,"De-Huffmanizing '%s'...",fname);

    /* pdsuncomp filters to a raw file */
#ifndef VMS
    if ( (tempnum=system(scanbuff)) ) {
      SetISTR(ISTR_WARNING,"Unable to de-Huffmanize '%s'.",fname);
      return 0;
    }
#else
    if ( (tempnum = !system(scanbuff)) ) {
      SetISTR(ISTR_WARNING,"Unable to de-Huffmanize '%s'.",fname);
      return 0;
    }
#endif


    zf = xv_fopen(pdsuncompfname,"r");
    if (!zf) {
      SetISTR(ISTR_WARNING,"LoadPDS: can't open uncompressed file %s\n",
	      pdsuncompfname);
      return 0;
    }
  }

  /* skip headers */

  if ( isfixed == TRUE ) {
    fread(scanbuff, (size_t) labelsize, (size_t) 1, zf);
  }

  /* samplesize is bits per pixel; guaranteed at this point to be 8 or 16 */
  bytewidth = w * (samplesize/8);
  bufsize = bytewidth * h;
  if (w <= 0 || h <= 0 || bytewidth/w != (samplesize/8) ||
      bufsize/bytewidth != h)
  {
    SetISTR(ISTR_WARNING,"LoadPDS: image dimensions out of range (%dx%dx%d)",
      w, h, samplesize/8);
    fclose(zf);
    return 0;
  }

  image = (byte *) malloc((size_t) bufsize);
  if (image == NULL) {
    fclose(zf);
    if (isfixed == FALSE)
      unlink(pdsuncompfname);
    FatalError("LoadPDS: can't malloc image buffer");
  }

  if ((lssize || lpsize) &&
       ((itype == PDSFIXED) || (itype == VIKINGFIXED) || (itype == VICAR)) ) {
    /* ARrrrgh.  Some of these images have crud intermixed with the image, */
    /* preventing us from freading in one fell swoop */
    /* (whatever a fell swoop is) */

    for (yy=0; yy<h; yy++) {
      if (lpsize &&
	  (teco=fread(scanbuff,(size_t) lpsize,(size_t) 1,zf)) != 1) {
	SetISTR(ISTR_WARNING, "LoadPDS: unexpected EOF reading prefix");
	fclose(zf);
	return 0;
      }

      teco = fread(image+(yy*bytewidth), (size_t) bytewidth, (size_t) 1,zf);
      if (teco != 1) {
	SetISTR(ISTR_WARNING, "LoadPDS: unexpected EOF reading line %d",yy);
	fclose(zf);
	return 0;
      }

      if (lssize &&
	  (teco=fread(scanbuff,(size_t) lssize,(size_t) 1,zf)) != 1) {
	SetISTR(ISTR_WARNING, "LoadPDS: unexpected EOF reading suffix");
	fclose(zf);
	return 0;
      }
    }

  } else if ((yy=fread(image, (size_t) bytewidth*h, (size_t) 1, zf)) != 1) {
    SetISTR(ISTR_WARNING,"LoadPDS: error reading image data");
    fclose(zf);
    if (itype==PDSVARIABLE || itype==VIKINGVARIABLE)
      unlink(pdsuncompfname);
    return 0;
  }

  fclose(zf);


  if (isfixed == FALSE)
    unlink(pdsuncompfname);

  pinfo->pic = image;
  pinfo->w   = w;   /* true pixel-width now (no longer bytewidth!) */
  pinfo->h   = h;

  if (samplesize == 16)
     if (Convert16BitImage(fname, pinfo,
	  vaxbyte ^ (*(char *)&samplesize == 16)) == 0)
        return 0;

  pinfo->frmType = -1;   /* can't save as PDS */
  pinfo->colType = F_GREYSCALE;
  sprintf(pinfo->shrtInfo, "%dx%d %s. ", pinfo->w, pinfo->h, ftypstr);

  pinfo->comment = (char *) malloc((size_t) 2000);
  if (pinfo->comment) {
    char tmp[256];
    *(pinfo->comment) = '\0';

    sprintf(tmp, "Spacecraft: %-28.28sTarget: %-32.32s\n", spacecraft, target);
    strncat(pinfo->comment, tmp, 2000 - strlen(pinfo->comment) - 1);

    sprintf(tmp, "Filter: %-32.32sMission phase: %-24.24s\n", filtname, mphase);
    strncat(pinfo->comment, tmp, 2000 - strlen(pinfo->comment) - 1);

    sprintf(tmp, "Image time: %-28.28sGain mode: %-29.29s\n", itime, gainmode);
    strncat(pinfo->comment, tmp, 2000 - strlen(pinfo->comment) - 1);

    sprintf(tmp, "Edit mode: %-29.29sScan mode: %-29.29s\n", editmode, scanmode);
    strncat(pinfo->comment, tmp, 2000 - strlen(pinfo->comment) - 1);

    sprintf(tmp, "Exposure: %-30.30sShutter mode: %-25.25s\n", exposure,shuttermode);
    strncat(pinfo->comment, tmp, 2000 - strlen(pinfo->comment) - 1);

    sprintf(tmp, "Instrument: %-28.28sImage time: %-28.28s\n", iname, itime);
    strncat(pinfo->comment, tmp, 2000 - strlen(pinfo->comment) - 1);

    sprintf(tmp, "Image Note: %-28.28s", inote);
    strncat(pinfo->comment, tmp, 2000 - strlen(pinfo->comment) - 1);
  }

  if (LoadPDSPalette(fname, pinfo))  return 1;

  /* these are grayscale, so cobble up a ramped colormap */
  /* Viking images on the CD's seem to be inverted.  Sigh */

  switch (itype) {
  case PDSVARIABLE:
  case PDSFIXED:
  case VICAR:
    for (yy=0; yy<=255; yy++) {
      pinfo->r[yy] = pinfo->g[yy] = pinfo->b[yy] = yy;
    }
    break;

  case VIKINGFIXED:
  case VIKINGVARIABLE:
    for (yy=0; yy<=255; yy++) {
      pinfo->r[yy] = pinfo->g[yy] = pinfo->b[yy] = (255-yy);
    }
  }

  pinfo->normw = pinfo->w;   pinfo->normh = pinfo->h;
  return 1;
}



/*
 *   16-bit to 8-bit conversion
 */

/*******************************************/
static int Convert16BitImage(fname, pinfo, swab)
     char *fname;
     PICINFO *pinfo;
     int swab;
{
  unsigned short *pShort;
  long i, j, k, l, n, *hist, nTot;
  size_t m;
  byte *lut, *pPix8;
  FILE *fp;
  char  name[1024], *c;

  m = 65536 * sizeof(byte);
  lut = (byte *) malloc(m);
  if (lut == NULL) {
    FatalError("LoadPDS: can't malloc LUT buffer");
  }

  /* allocate histogram table */
  m = 65536 * sizeof(long);
  hist = (long *) malloc(m);
  if (hist == NULL) {
    free(lut);
    FatalError("LoadPDS: can't malloc histogram buffer");
  }

  /* check whether histogram file exists */
#ifdef VMS
  c = (char *) rindex(strcpy(name,
			     (c = (char *) rindex(fname, ':')) ? c+1 : fname),
		      ']');
#else
  c = (char *) rindex(strcpy(name, fname), '/');
#endif /* VMS */
  (void)strcpy(c ? c+1 : name, "hist.tab");

  /* read the histogram file which is always LSB_INTEGER */
  if ((fp = xv_fopen(name, "r")) != NULL) {
    for (n = k = nTot = 0; n < 65536 && k != EOF; n++, nTot += j) {
      for (i = j = 0; i < 4 && (k = getc(fp)) != EOF; i++)
        j = ((j >> 8) & 0xffffff) | ((k & 255) << 24);
      hist[swab ? ((n & 255) << 8) | ((n >> 8) & 255) : n] = j;
    }
    fclose(fp);
    if (k == EOF)
      fp = NULL;
  }

  /* if no external histogram, make one */
  if (fp == NULL) {
    for (n = 0; n < 65536; n++)
      hist[n] = 0;
    nTot = n = pinfo->h * pinfo->w;
    pShort = (unsigned short *)pinfo->pic;
    while (--n >= 0) {
      hist[*pShort++]++;
    }
  }

  /* use either histogram equalization or linear stretch */
  if (0) {      /* was: (conv24MB.flags[CONV24_HIST]) */

    /* construct lookup table to invert the histogram */
    m = l = hist[0];
    i = (nTot - m) >> 8;
    for (n = j = 1; n < 65536; n++) {
      k = swab ? ((n & 255) << 8) | ((n >> 8) & 255) : n;
      if ((l += hist[k]) > m && j < 255)
        j++, m += i;
      lut[k] = j;
    }

  } else {

    /* find min and max of histogram */
    n = nTot/10000;
    for (m = 0, j = 1; j < 65536; j++)
      if ((m += hist[swab ? ((j & 255) << 8) | ((j >> 8) & 255) : j]) > n)
        break;
    for (m = 0, k = 65535; --k > 1; )
      if ((m += hist[swab ? ((k & 255) << 8) | ((k >> 8) & 255) : k]) > n)
        break;

    /* construct stretch table */
    for (n = 0; n < 65536; n++) {
      i = swab ? (((n & 255) << 8) | ((n >> 8) & 255)) : n;
      m = 1 + (254 * (n - j)) / (k - j);
      lut[i] = n < j ? 0 : (n > k ? 255 : m);
    }
  }

  free(hist);

  /* allocate new 8-bit image */
  n = pinfo->w * pinfo->h;
  if (pinfo->w <= 0 || pinfo->h <= 0 || n/pinfo->w != pinfo->h) {
    SetISTR(ISTR_WARNING,"LoadPDS: image dimensions out of range (%dx%d)",
      pinfo->w, pinfo->h);
    free(lut);
    return 0;
  }
  pPix8 = (byte *)malloc(n*sizeof(byte));
  if (pPix8 == NULL) {
    free(lut);
    FatalError("LoadPDS: can't malloc 16-to-8-bit conversion buffer");
  }

  /* convert the 16-bit image to 8-bit */
  lut[0] = 0;
  free(pShort = (unsigned short *)pinfo->pic);
  pinfo->pic = pPix8;
  while(--n >= 0)
    *pPix8++ = lut[*pShort++];
  free(lut);
  return 1;
}

/*
 *   If there is a PALETTE.TAB file in the same directory, use it
 */

/*******************************************/
static int LoadPDSPalette(fname, pinfo)
     char    *fname;
     PICINFO *pinfo;
{
  FILE    *fp;
  char    name[1024], buf[256], *c;
  int     i, n, r, g, b;

#ifdef VMS
  c = (char *) rindex(strcpy(name,
			     (c = (char *) rindex(fname, ':')) ? c+1 : fname),
		      ']');
#else
  c = (char *) rindex(strcpy(name, fname), '/');
#endif /* VMS */
  (void)strcpy(c ? c+1 : name, "palette.tab");

  if ((fp = xv_fopen(name, "r")) == NULL)
    return 0;
  for (i = 0; i < 256; i++) {
    if (fgets(c = buf, (int) (sizeof(buf)-1), fp) == NULL)
      break;
    while (*c && isspace(*c))
      c++;
    if (*c == '\0' || *c == '#')
      continue;
    if (sscanf(buf, "%d %d %d %d", &n, &r, &g, &b) != 4)
      continue;
    if (n < 0 || n > 255)
      continue;
    pinfo->r[n] = r;
    pinfo->g[n] = g;
    pinfo->b[n] = b;
    if (r != g && r != b)
      pinfo->colType = F_FULLCOLOR;
  }
  (void)fclose(fp);
  return 1;
}


#endif /* HAVE_PDS */
