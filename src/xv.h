/*
 *  xv.h  -  header file for xv, but you probably guessed as much
 *
 *  Author:    John Bradley  (bradley@cis.upenn.edu)
 */

#include "copyright.h"
#include "config.h"


/* xv 3.10a:				19941229 */
/* PNG patch 1.2d:			19960731 */
/* GRR orig jumbo fixes patch:		20000213 */
/* GRR orig jumbo enhancements patch:	20000220 */
/* GRR 1st public jumbo F+E patches:	20040531 */
/* GRR 2nd public jumbo F+E patches:	20050410 */
/* GRR 3rd public jumbo F+E patches:	20050501 */
/* GRR 4th public jumbo F+E patch:  	20070520 */
/* GRR 5th public jumbo F+E patch:  	200xxxxx (probably mid-2009) */
#define REVDATE   "version 3.10a-jumboFix+Enh of 20081216 (interim!)"
#define VERSTR    "3.10a-20081216"

/*
 * uncomment the following, and modify for your site, but only if you've
 * actually registered your copy of XV...
 */
/* #define REGSTR "Registered for use at the University of Pennsylvania." */


#ifndef VMS
#  define THUMBDIR     ".xvpics"  /* name of thumbnail file subdirectories */
#  define THUMBDIRNAME ".xvpics"  /* same as THUMBDIR, unlike VMS case... */
#  define CLIPFILE     ".xvclip"  /* name of clipboard file in home directory */
#else
#  define THUMBDIR     "XVPICS"       /* name to use in building paths... */
#  define THUMBDIRNAME "XVPICS.DIR"   /* name from readdir() & stat() */
#  define CLIPFILE     "xvclipbd.dat"
#endif


#undef PARM
#ifdef __STDC__
#  define PARM(a) a
#else
#  define PARM(a) ()
#  define const
#endif



/*************************************************/
/* START OF MACHINE-DEPENDENT CONFIGURATION INFO */
/*************************************************/


#define ENABLE_FIXPIX_SMOOTH	/* GRR 19980607 */


/* Things to make xv more likely to just build, without the user tweaking
   the makefile */

#ifdef hpux        /* HPUX machines (SVR3, (not SVR4) NO_RANDOM) */
#  undef  SVR4
#  undef  SYSV
#  define SYSV
#  undef  NO_RANDOM
#  define NO_RANDOM
#  define USE_GETCWD
#endif


#ifdef sgi         /* SGI machines (SVR4) */
#  undef  SVR4
#  define SVR4
#endif

#if defined(__sony_news) && defined(bsd43) && !defined(__bsd43)
#  define __bsd43
#elif defined(__sony_news) && (defined(SYSTYPE_BSD) || defined(__SYSTYPE_BSD)) && !defined(bsd43) && !defined(__bsd43)
#  define bsd43
#  define __bsd43
#endif

#include <signal.h>      /* for interrupt handling */

/* at least on Linux, the following file (1) includes sys/types.h and
 * (2) defines __USE_BSD (which was not defined before here), so __linux__
 * block is now moved after this #include */
#include <X11/Xos.h>     /* need type declarations immediately */


#ifdef __linux__
#  ifndef _LINUX_LIMITS_H
#    include <linux/limits.h>
#  endif
#  ifndef USLEEP
#    define USLEEP
#  endif
   /* want only one or the other defined, not both: */
#  if !defined(BSDTYPES) && !defined(__USE_BSD)
#    define BSDTYPES
#  endif
#  if defined(BSDTYPES) && defined(__USE_BSD)
#    undef BSDTYPES
#  endif
#endif


/*********************************************************/


/* The BSD typedefs are used throughout.
 * If your system doesn't have them in <sys/types.h>,
 * then define BSDTYPES in your Makefile.
 */
#if defined(BSDTYPES) || defined(VMS)
  typedef unsigned char  u_char;
  typedef unsigned short u_short;
  typedef unsigned int   u_int;
  typedef unsigned long  u_long;
#endif


#ifdef __UMAXV__              /* for Encore Computers UMAXV */
#  include <sys/fs/b4param.h>   /* Get bsd fast file system params*/
#endif


/* things that *DON'T* have dirent.  Hopefully a very short list */
#if defined(__UMAXV__)
#  ifndef NODIRENT
#    define NODIRENT
#  endif
#endif


#if defined(__sony_news) && defined(__bsd43)
#  include <unistd.h>
#endif


#if defined(__FreeBSD__)
#  include <sys/param.h>
#endif


/* include files */
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#ifdef __STDC__
#  include <stddef.h>
#  include <stdlib.h>
#endif

/* note: 'string.h' or 'strings.h' is included by Xos.h, and it
   guarantees index() and rindex() will be available */

#ifndef VMS
#  include <errno.h>
#  ifndef __NetBSD__
#    if !(defined __GLIBC__ && __GLIBC__ >= 2)
       extern int   errno;         /* SHOULD be in errno.h, but often isn't */
       extern char *sys_errlist[]; /* this too... */
#    endif
#  endif
#endif


/* not everyone has the strerror() function, or so I'm told */
#ifdef VMS
#  define ERRSTR(x) strerror(x, vaxc$errno)
#else
#  if defined(__BEOS__) || defined(__linux__) /* or all modern/glibc systems? */
#    define ERRSTR(x) strerror(x)
#  else
#    define ERRSTR(x) sys_errlist[x]
#  endif
#endif




#ifdef VMS   /* VMS config, hacks & kludges */
#  define MAXPATHLEN    512
#  define popUp xv_popup
#  define qsort xv_qsort
#  define random rand
#  define srandom srand
#  define cols xv_cols
#  define gmap xv_gmap
#  define index  strchr
#  define rindex strrchr
#  include <errno.h>
#  include <perror.h>
#endif


/* GRR 20070512:  Very few modern systems even have a malloc.h anymore;
 *                stdlib.h is, well, the standard.  (Former explicitly listed
 *                "don't include" systems:  ibm032, __convex__, non-ultrix vax,
 *                mips, apollo, pyr, sequent, __UMAXV__, aux, bsd43, __bsd43,
 *                __bsdi__, __386BSD__, __FreeBSD__, __OpenBSD__, __NetBSD__,
 *                __DARWIN__, VMS.)  Anyone who _does_ need it can explicitly
 *                define NEED_MALLOC_H in the makefile. */
#ifdef NEED_MALLOC_H
#  if defined(hp300) || defined(hp800) || defined(NeXT)
#    include <sys/malloc.h>    /* it's in "sys" on HPs and NeXT */
#  else
#    include <malloc.h>
#  endif
#endif



#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>

#ifdef TV_L10N
#  include <X11/Xlocale.h>
#endif


#include <sys/types.h>






#ifdef NEEDSTIME
#  include <sys/time.h>

#  ifdef _AIX
#    include <sys/select.h>   /* needed for select() call in Timer() */
#  endif

#  ifdef SVR4
#    include <poll.h>      /* used in SVR4 version of Timer() */
#  endif

#  ifdef sgi               /* need 'CLK_TCK' value for sginap() call */
#    include <limits.h>
#  endif

#  ifdef __BEOS__
#    include <socket.h>
#  endif

/*** for select() call ***/
#  ifdef __hpux
#    define XV_FDTYPE (int *)
#  else
#    define XV_FDTYPE (fd_set *)
#  endif

#endif  /* NEEDSTIME */



#ifdef NEEDSDIR
#  ifdef VMS
#    include <descrip.h>
#    include <stat.h>
#    include "dirent.h"
#  else
#    ifdef NODIRENT
#      include <sys/dir.h>
#    else
#      include <dirent.h>
#    endif

#    if defined(SVR4) || defined(SYSV)
#      include <fcntl.h>
#    endif

#    include <sys/param.h>
#    include <sys/stat.h>

#    if defined(__convex__) && defined (__STDC__)
#      define S_IFMT  _S_IFMT
#      define S_IFDIR _S_IFDIR
#      define S_IFCHR _S_IFCHR
#      define S_IFBLK _S_IFBLK
#    endif
#  endif
#endif


#ifdef NEEDSARGS
#  if defined(__STDC__) && !defined(NOSTDHDRS)
#    include <stdarg.h>
#  else
#    include <varargs.h>
#  endif
#endif



/* Use S_ISxxx macros in stat-related stuff
 * make them if missing, along with a few fictitious ones
 *      Cameron Simpson  (cameron@cse.unsw.edu.au)
 */

#ifndef         S_ISDIR         /* missing POSIX-type macros */
#  define       S_ISDIR(mode)   (((mode)&S_IFMT) == S_IFDIR)
#  define       S_ISBLK(mode)   (((mode)&S_IFMT) == S_IFBLK)
#  define       S_ISCHR(mode)   (((mode)&S_IFMT) == S_IFCHR)
#  define       S_ISREG(mode)   (((mode)&S_IFMT) == S_IFREG)
#endif
#ifndef         S_ISFIFO
#  ifdef        S_IFIFO
#    define     S_ISFIFO(mode)  (((mode)&S_IFMT) == S_IFIFO)
#  else
#    define     S_ISFIFO(mode)  0
#  endif
#endif
#ifndef         S_ISLINK
#  ifdef        S_IFLNK
#    define     S_ISLINK(mode)  (((mode)&S_IFMT) == S_IFLNK)
#  else
#    define     S_ISLINK(mode)  0
#  endif
#endif
#ifndef         S_ISSOCK
#  ifdef        S_IFSOCK
#    define     S_ISSOCK(mode)  (((mode)&S_IFMT) == S_IFSOCK)
#  else
#    define     S_ISSOCK(mode)  0
#  endif
#endif

#ifndef S_IRWUSR
#  define S_IRWUSR	(S_IRUSR|S_IWUSR)	/* or (S_IREAD|S_IWRITE) */
#endif

#ifndef MAXPATHLEN
#  define MAXPATHLEN 256
#endif


#ifdef SVR4
#  define random lrand48
#  define srandom srand48
#else
#  if defined(NO_RANDOM) || (defined(sun) && defined(SYSV))
#    define random()   rand()
#    define srandom(x) srand(x)
#  endif
#endif


#ifndef VMS       /* VMS hates multi-line definitions */
#  if defined(SVR4)  || defined(SYSV) || defined(sco) || \
      defined(XENIX) || defined(__osf__) || defined(__linux__)
#    undef  USE_GETCWD
#    define USE_GETCWD          /* use 'getcwd()' instead of 'getwd()' */
#  endif                        /* >> SECURITY ISSUE << */
#endif


/* GRR 20040430:  This is new and still not fully deployed.  No doubt there
 *                are other systems that have mkstemp() (SUSv3); we can add
 *                them later. */
#ifndef VMS       /* VMS hates multi-line definitions */
#  if defined(__linux__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
      defined(__bsdi__)
#    ifndef USE_MKSTEMP
#      define USE_MKSTEMP       /* use 'mkstemp()' instead of 'mktemp()' */
#    endif                      /* >> SECURITY ISSUE << */
#  endif
#endif


/* GRR 20040503:  This is new and so far tested only under Linux.  But it
 *                allows -wait to work with subsecond values as long as
 *                times() exists and clock_t is a long int (latter matters
 *                only if/when clocks wrap, which for Linux is multiples of
 *                497.11 days since the last reboot). */
#if defined(__linux__)
#  define USE_TICKS             /* use times()/Timer(), not time()/sleep() */
#  include <limits.h>           /* LONG_MAX (really want CLOCK_T_MAX) */
#  include <sys/times.h>        /* times() */
#  ifndef CLK_TCK               /* can be undefined in strict-ANSI mode */
#    define CLK_TCK CLOCKS_PER_SEC   /* claimed to be same thing in time.h */
#  endif
#endif

#if (defined(SYSV) || defined(SVR4) || defined(linux)) && !defined(USE_GETCWD)
#  define USE_GETCWD
#endif

#ifndef SEEK_SET
#  define SEEK_SET 0
#  define SEEK_CUR 1
#  define SEEK_END 2
#endif

#if defined(__mips) && defined(__SYSTYPE_BSD43)
#  define strstr(A,B) pds_strstr((A),(B))
#  undef S_IFIFO
#endif /* !mips_bsd */

/*****************************/
/* END OF CONFIGURATION INFO */
/*****************************/


#ifdef DOJPEG
#  define HAVE_JPEG
#endif

#ifdef DOJP2K
#  define HAVE_JP2K
#endif

#ifdef DOTIFF
#  define HAVE_TIFF
#endif

#ifdef DOPNG
#  define HAVE_PNG
#endif

#ifdef DOPDS
#  define HAVE_PDS
#endif

#ifdef DOG3
#  define HAVE_G3
#endif


#define PROGNAME   "xv"            /* used in resource database */

#define MAXNAMES   65536           /* max # of files in ctrlW list */

#define MAXBRWIN   16              /* max # of vis browser windows */

/* strings in the INFOBOX (used in SetISTR and GetISTR) */
#define NISTR         10    /* number of ISTRs */
#define ISTR_INFO     0
#define ISTR_WARNING  1
#define ISTR_FILENAME 2
#define ISTR_FORMAT   3
#define ISTR_RES      4
#define ISTR_CROP     5
#define ISTR_EXPAND   6
#define ISTR_SELECT   7
#define ISTR_COLOR    8
#define ISTR_COLOR2   9

/* potential values of 'infomode', used in info box drawing routines */
#define INF_NONE 0    /* empty box */
#define INF_STR  1    /* just ISTR_INFO */
#define INF_PART 2    /* filename, format, size and infostr */
#define INF_FULL 3    /* INF_PART + clipping, expansion, colorinfo */


/* buttons in the ctrl window */
#define BNEXT    0
#define BPREV    1
#define BLOAD    2
#define BSAVE    3
#define BDELETE  4
#define BPRINT   5

#define BCOPY    6
#define BCUT     7
#define BPASTE   8
#define BCLEAR   9
#define BGRAB    10
#define BUP10    11
#define BDN10    12
#define BROTL    13
#define BROTR    14
#define BFLIPH   15
#define BFLIPV   16
#define BPAD     17
#define BANNOT   18
#define BCROP    19
#define BUNCROP  20
#define BACROP   21
#define BABOUT   22
#define BQUIT    23
#define BXV      24
#define NBUTTS   25    /* # of butts */


/* buttons in the load/save window */
#define S_LOAD_NBUTTS  4
#define S_NBUTTS   5
#define S_BOK      0
#define S_BCANC    1
#define S_BRESCAN  2
#define S_BLOADALL 3
#define S_BOLDSET  3
#define S_BOLDNAM  4


/* buttons in the 'gamma' window */
#define G_NBUTTS   24
#define G_BAPPLY   0
#define G_BNOGAM   1
#define G_BRESET   2
#define G_BCLOSE   3
#define G_BUP_BR   4
#define G_BDN_BR   5
#define G_BUP_CN   6
#define G_BDN_CN   7
#define G_B1       8
#define G_B2       9
#define G_B3       10
#define G_B4       11
#define G_BSET     12
#define G_BUNDO    13
#define G_BREDO    14
#define G_BCOLREV  15
#define G_BRNDCOL  16
#define G_BHSVRGB  17
#define G_BCOLUNDO 18
#define G_BRV      19
#define G_BMONO    20
#define G_BMAXCONT 21
#define G_BGETRES  22
#define G_BHISTEQ  23


/* constants for setting default 'save mode' in dirW */
#define F_COLORS    0
#define F_FORMAT    1

/* the following list give indices into saveColors[] array in xvdir.c */
#define F_FULLCOLOR 0
#define F_GREYSCALE 1
#define F_BWDITHER  2
#define F_REDUCED   3
#define F_MAXCOLORS 4   /* length of saveColors[] array */


/* The following list gives indices into 'saveFormats[]' array in xvdir.c.
   Note that JPEG, TIFF, and other entries may or may not exist, so the
   following constants have to be adjusted accordingly.  Also, don't worry
   about duplicate cases if, e.g., JPGINC or TIFINC = 0.  All code that
   references F_JPEG, F_TIFF, etc., is #ifdef'd, so it won't be a problem. */

#ifdef HAVE_JPEG
#  define F_JPGINC  1
#else
#  define F_JPGINC  0
#endif

#ifdef HAVE_JP2K
#  define F_JP2INC  1   /* provides both JPC and JP2 */
#else
#  define F_JP2INC  0
#endif

#ifdef HAVE_TIFF
#  define F_TIFINC  1
#else
#  define F_TIFINC  0
#endif

#ifdef HAVE_PNG
#  define F_PNGINC  1
#else
#  define F_PNGINC  0
#endif

#ifdef HAVE_MAG
#  define F_MAGINC  1
#else
#  define F_MAGINC  0
#endif

#ifdef HAVE_PIC
#  define F_PICINC  1
#else
#  define F_PICINC  0
#endif

#ifdef HAVE_MAKI
#  define F_MAKINC  1
#else
#  define F_MAKINC  0
#endif

#ifdef HAVE_PI
#  define F_PAIINC  1
#else
#  define F_PAIINC  0
#endif

#ifdef HAVE_PIC2
#  define F_PC2INC  1
#else
#  define F_PC2INC  0
#endif

#ifdef HAVE_MGCSFX
#  define F_MGCSFXINC  1
#else
#  define F_MGCSFXINC  0
#endif

#ifdef MACBINARY
#  define MACBSIZE 128
#endif

/* NOTE:  order must match saveFormats[] in xvdir.c */
/* [this works best when first one is always present, but...we like PNG :-) ] */
#define F_PNG         0
#define F_JPEG      ( 0 + F_PNGINC)
#define F_JPC       ( 0 + F_PNGINC + F_JPGINC)
#define F_JP2       ( 0 + F_PNGINC + F_JPGINC + F_JP2INC)
#define F_GIF       ( 0 + F_PNGINC + F_JPGINC + F_JP2INC + F_JP2INC)  /* always avail; index varies */
#define F_TIFF      ( 0 + F_PNGINC + F_JPGINC + F_JP2INC + F_JP2INC + F_TIFINC)
#define F_PS        ( 1 + F_TIFF)
#define F_PBMRAW    ( 2 + F_TIFF)
#define F_PBMASCII  ( 3 + F_TIFF)
#define F_XBM       ( 4 + F_TIFF)
#define F_XPM       ( 5 + F_TIFF)
#define F_BMP       ( 6 + F_TIFF)
#define F_SUNRAS    ( 7 + F_TIFF)
#define F_IRIS      ( 8 + F_TIFF)
#define F_TARGA     ( 9 + F_TIFF)
#define F_FITS      (10 + F_TIFF)
#define F_PM        (11 + F_TIFF)
#define F_ZX        (12 + F_TIFF)   /* [JCE] */
#define F_WBMP      (13 + F_TIFF)
#define JP_EXT_F    (F_WBMP)
#define F_MAG       (JP_EXT_F + F_MAGINC)
#define F_PIC       (JP_EXT_F + F_MAGINC + F_PICINC)
#define F_MAKI      (JP_EXT_F + F_MAGINC + F_PICINC + F_MAKINC)
#define F_PI        (JP_EXT_F + F_MAGINC + F_PICINC + F_MAKINC + F_PAIINC)
#define F_PIC2      (JP_EXT_F + F_MAGINC + F_PICINC + F_MAKINC + F_PAIINC + F_PC2INC)
#define F_MGCSFX    (JP_EXT_F + F_MAGINC + F_PICINC + F_MAKINC + F_PAIINC + F_PC2INC + F_MGCSFXINC)
#define JP_EXT_F_END (F_MGCSFX)
#define F_DELIM1    (JP_EXT_F_END + 1)   /* ----- */
#define F_FILELIST  (JP_EXT_F_END + 2)
#define F_MAXFMTS   (JP_EXT_F_END + 3)   /* 27, normally (with all formats) */



/* return values from ReadFileType()
 * positive values are *definitely* readable formats (HAVE_*** is defined)
 * negative values are random files that XV can't read, but display as
 *   different icons in the visual browser
 */
#define RFT_ERROR    -1    /* couldn't open file, or whatever... */
#define RFT_UNKNOWN   0
#define RFT_GIF       1
#define RFT_PM        2
#define RFT_PBM       3
#define RFT_XBM       4
#define RFT_SUNRAS    5
#define RFT_BMP       6
#define RFT_UTAHRLE   7
#define RFT_IRIS      8
#define RFT_PCX       9
#define RFT_JFIF     10
#define RFT_TIFF     11
#define RFT_PDSVICAR 12
#define RFT_COMPRESS 13
#define RFT_PS       14
#define RFT_IFF      15
#define RFT_TARGA    16
#define RFT_XPM      17
#define RFT_XWD      18
#define RFT_FITS     19
#define RFT_PNG      20
#define RFT_ZX       21    /* [JCE] */
#define RFT_WBMP     22
#define RFT_PCD      23
#define RFT_HIPS     24
#define RFT_BZIP2    25
#define RFT_JPC      26
#define RFT_JP2      27
#define RFT_G3       28
#define JP_EXT_RFT   (RFT_G3)
#define RFT_MAG      (JP_EXT_RFT + 1)
#define RFT_MAKI     (JP_EXT_RFT + 2)
#define RFT_PIC      (JP_EXT_RFT + 3)
#define RFT_PI       (JP_EXT_RFT + 4)
#define RFT_PIC2     (JP_EXT_RFT + 5)
#define RFT_MGCSFX   (JP_EXT_RFT + 6)

/* definitions for page up/down, arrow up/down list control */
#define LS_PAGEUP   0
#define LS_PAGEDOWN 1
#define LS_LINEUP   2
#define LS_LINEDOWN 3
#define LS_HOME     4
#define LS_END      5


/* values returned by CursorKey() */
#define CK_NONE     0
#define CK_LEFT     1
#define CK_RIGHT    2
#define CK_UP       3
#define CK_DOWN     4
#define CK_PAGEUP   5
#define CK_PAGEDOWN 6
#define CK_HOME     7
#define CK_END      8


/* values 'epicMode' can take */
#define EM_RAW    0
#define EM_DITH   1
#define EM_SMOOTH 2


/* things EventLoop() can return (0 and above reserved for 'goto pic#') */
#define QUIT      -1   /* exit immediately  */
#define NEXTPIC   -2   /* goto next picture */
#define PREVPIC   -3   /* goto prev picture */
#define NEXTQUIT  -4   /* goto next picture, quit if none (used by 'wait') */
#define LOADPIC   -5   /* load 'named' pic (from directory box) */
#define NEXTLOOP  -6   /* load next pic, loop if we're at end */
#define DFLTPIC   -7   /* load the default image */
#define DELETE    -8   /* just deleted pic.  load 'right' thing */
#define GRABBED   -9   /* just grabbed a pic.  'load' it up */
#define POLLED    -10  /* polling, and image file has changed... */
#define RELOAD    -11  /* 'reload' interrupt came. be happier about errors */
#define THISNEXT  -12  /* load 'current' selection, Next until success */
#define OP_PAGEUP -13  /* load previous page of multi-page document */
#define OP_PAGEDN -14  /* load next page of multi-page document */
#define PADDED    -15  /* just grabbed a pic.  'load' it up */


/* possible values of 'rootMode' */
#define RM_NORMAL  0     /* default X tiling */
#define RM_TILE    1     /* integer tiling */
#define RM_MIRROR  2     /* mirror tiling */
#define RM_IMIRROR 3     /* integer mirror tiling */
#define RM_CENTER  4     /* modes >= RM_CENTER centered on some sort of bg */
#define RM_CENTILE 4     /* centered and tiled.  NOTE: equals RM_CENTER */
#define RM_CSOLID  5     /* centered on a solid bg */
#define RM_CWARP   6     /* centered on a 'warp-effect' bg */
#define RM_CBRICK  7     /* centered on a 'brick' bg */
#define RM_ECENTER 8     /* symmetrical tiled */
#define RM_ECMIRR  9     /* symmetrical mirror tiled */
#define RM_UPLEFT 10     /* just in upper left corner */
#define RM_MAX     RM_UPLEFT


/* values of colorMapMode */
#define CM_NORMAL    0        /* normal RO or RW color allocation */
#define CM_PERFECT   1        /* install own cmap if necessary */
#define CM_OWNCMAP   2        /* install own cmap always */
#define CM_STDCMAP   3        /* use stdcmap */


/* values of haveStdCmap */
#define STD_NONE     0        /* no stdcmap currently defined */
#define STD_111      1        /* 1/1/1 stdcmap is available */
#define STD_222      2        /* 2/2/2 stdcmap is available */
#define STD_232      3        /* 2/3/2 stdcmap is available */
#define STD_666      4        /* 6x6x6 stdcmap is available */
#define STD_332      5        /* 3/3/2 stdcmap is available */


/* values of allocMode */
#define AM_READONLY  0
#define AM_READWRITE 1


/* selections in dispMB */
#define DMB_RAW      0
#define DMB_DITH     1
#define DMB_SMOOTH   2
#define DMB_SEP1     3     /* ---- separator */
#define DMB_COLRW    4
#define DMB_SEP2     5     /* ---- separator */
#define DMB_COLNORM  6
#define DMB_COLPERF  7
#define DMB_COLOWNC  8
#define DMB_COLSTDC  9
#define DMB_MAX      10


/* selections in rootMB */
#define RMB_WINDOW   0
#define RMB_ROOT     1
#define RMB_TILE     2
#define RMB_MIRROR   3
#define RMB_IMIRROR  4
#define RMB_CENTILE  5
#define RMB_CSOLID   6
#define RMB_CWARP    7
#define RMB_CBRICK   8
#define RMB_ECENTER  9
#define RMB_ECMIRR   10
#define RMB_UPLEFT   11
#define RMB_MAX      12


/* indices into conv24MB */
#define CONV24_8BIT  0
#define CONV24_24BIT 1
#define CONV24_SEP1  2
#define CONV24_LOCK  3
#define CONV24_SEP2  4
#define CONV24_FAST  5
#define CONV24_SLOW  6
#define CONV24_BEST  7
#define CONV24_MAX   8

/* values 'picType' can take */
#define PIC8  CONV24_8BIT
#define PIC24 CONV24_24BIT

/* indices into algMB */
#define ALG_NONE      0
#define ALG_SEP1      1  /* separator */
#define ALG_BLUR      2
#define ALG_SHARPEN   3
#define ALG_EDGE      4
#define ALG_TINF      5
#define ALG_OIL       6
#define ALG_BLEND     7
#define ALG_ROTATE    8
#define ALG_ROTATECLR 9
#define ALG_PIXEL     10
#define ALG_SPREAD    11
#define ALG_MEDIAN    12
#define ALG_MAX       13


/* indices into sizeMB */
#define SZMB_NORM     0
#define SZMB_MAXPIC   1
#define SZMB_MAXPECT  2
#define SZMB_DOUBLE   3
#define SZMB_HALF     4
#define SZMB_P10      5
#define SZMB_M10      6
#define SZMB_SEP      7   /* separator */
#define SZMB_SETSIZE  8
#define SZMB_ASPECT   9
#define SZMB_4BY3     10
#define SZMB_INTEXP   11
#define SZMB_MAX      12

/* indices into windowMB */
#define WMB_BROWSE    0
#define WMB_COLEDIT   1
#define WMB_INFO      2
#define WMB_COMMENT   3
#define WMB_TEXTVIEW  4
#define WMB_SEP       5  /* separator */
#define WMB_ABOUTXV   6
#define WMB_KEYHELP   7
#define WMB_MAX       8


/* definitions of first char of dirnames[i] (filetype) */
#define C_FIFO  'f'    /* FIFO special file */
#define C_CHR   'c'    /* character special file */
#define C_DIR   'd'    /* directory */
#define C_BLK   'b'    /* block special file */
#define C_LNK   'l'    /* symbolic link */
#define C_SOCK  's'    /* socket */
#define C_REG   ' '    /* regular file */
#define C_EXE   'x'    /* executable file */


/* values used in Draw3dRect() */
#define R3D_OUT 0  /* rect sticks 'out' from screen */
#define R3D_IN  1  /* rect goes 'in' screen */


/* values 'GetSelType()' (in xvcut.c) can return */
#define SEL_RECT 0

/* mode values for PadPopUp() */
#define PAD_SOLID 0
#define PAD_BGGEN 1
#define PAD_LOAD  2

#define PAD_ORGB  0
#define PAD_OINT  1
#define PAD_OHUE  2
#define PAD_OSAT  3
#define PAD_OMAX  4

/* byte offsets into a 'cimg' (clipboard image) array (SaveToClip()) */
#define CIMG_LEN   0              /* offset to 4-byte length of data */
#define CIMG_W     4              /* offset to 2-byte width of image */
#define CIMG_H     6              /* offset to 2-byte height of image */
#define CIMG_24    8              /* offset to 1-byte 'is24bit?' field */
#define CIMG_TRANS 9              /* offset to 1-byte 'has transparent?' */
#define CIMG_TRVAL 10             /* if trans && !24: trans. pixel val */
#define CIMG_TRR   11             /* if trans && 24: red val of trans */
#define CIMG_TRG   12
#define CIMG_TRB   13
#define CIMG_PIC24 14             /* offset to data(24-bit) */
#define CIMG_CMAP  14             /* offset to cmap (8-bit) */
#define CIMG_PIC8 (CIMG_CMAP + 3*256)   /* offset to data (8-bit) */



#define MBSEP "\001"   /* special string for a --- separator in MBUTT */

/* random string-placing definitions */
#define SPACING 3      /* vertical space between strings */
#define ASCENT   (mfinfo->ascent)
#define DESCENT  (mfinfo->descent)
#define CHIGH    (ASCENT + DESCENT)
#define LINEHIGH (CHIGH + SPACING)


#define STDINSTR "<stdin>"


#ifndef MAIN
#define WHERE extern
#else
#define WHERE
#endif

typedef unsigned char byte;

typedef struct scrl {
                 Window win;            /* window ID */
		 int x,y,w,h;           /* window coords in parent */
		 int len;               /* length of major axis */
		 int vert;              /* true if vertical, else horizontal */
		 int active;            /* true if scroll bar can do anything*/
		 double min,max;        /* min/max values 'pos' can take */
		 double val;            /* 'value' of scrollbar */
		 double page;           /* amt val change on pageup/pagedown */
		 int tpos;              /* thumb pos. (pixels from tmin) */
		 int tmin,tmax;         /* min/max thumb offsets (from 0,0) */
		 int tsize;             /* size of thumb (in pixels) */
		 u_long fg,bg,hi,lo;    /* colors */

		 /* redraws obj controlled by scrl*/
		 void (*drawobj)PARM((int, struct scrl *));

		 int uplit, dnlit;      /* true if up&down arrows are lit */
	       } SCRL;

typedef struct { Window win;            /* window ID */
		 int x,y,w,h;           /* window coords in parent */
		 int active;            /* true if can do anything*/
		 double min,max;        /* min/max values 'pos' can take */
		 double val;            /* 'value' of dial */
		 double inc;            /* amt val change on up/down */
		 double page;           /* amt val change on pageup/pagedown */
		 const char *title;     /* title for this gauge */
		 const char *units;     /* string appended to value */
		 u_long fg,bg,hi,lo;    /* colors */
		 int rad, cx, cy;       /* internals */
		 int bx[4], by[4];      /* more internals */

		 /* redraws obj controlled by dial */
		 void (*drawobj)PARM((void));
	       } DIAL;

typedef struct { Window win;            /* parent window */
		 int x,y;               /* size of button rectangle */
		 unsigned int w,h;
		 int lit;               /* if true, invert colors */
		 int active;            /* if false, stipple gray */
		 int toggle;            /* if true, clicking toggles state */
		 u_long fg,bg,hi,lo;    /* colors */
		 const char *str;       /* string in button */
		 Pixmap pix;            /* use pixmap instead of string */
		 u_int pw,ph;           /* size of pixmap */
		 int colorpix;          /* multi-color pixmap */
		 int style;             /* ... */
		 int fwidth;            /* width of frame */
	       } BUTT;


typedef struct rbutt {
                 Window        win;      /* parent window */
		 int           x,y;      /* position in parent */
		 const char    *str;     /* the message string */
		 int           selected; /* selected or not */
		 int           active;   /* selectable? */
		 struct rbutt *next;     /* pointer to next in group */
		 u_long        fg,bg;    /* colors */
		 u_long        hi,lo;    /* colors */
	       } RBUTT;



typedef struct { Window        win;      /* parent window */
		 int           x,y;      /* position in parent */
		 const char   *str;      /* the message string */
		 int           val;      /* 1=selected, 0=not */
		 int           active;   /* selectable? */
		 u_long        fg,bg;    /* colors */
		 u_long        hi,lo;    /* colors */
	       } CBUTT;


#define MAXMBLEN 32  /* max # of items in an mbutt */
typedef struct { Window        win;            /* parent window */
		 int           x,y;            /* position in parent */
		 unsigned int  w,h;
		 const char   *title;          /* title string in norm state */
		 int           active;         /* selectable? */
		 const char  **list;           /* list of strings in menu */
		 int           nlist;          /* # of strings in menu */
		 byte          flags[MAXMBLEN]; /* checkmarks on items */
		 int           hascheck;       /* leave room for checkmark? */
		 byte          dim[MAXMBLEN];  /* dim individual choices */
		 Pixmap        pix;            /* use pixmap instd of string */
		 int           pw,ph;          /* size of pixmap */
		 u_long        fg,bg,hi,lo;    /* colors */
		 Window        mwin;           /* popup menu window */
	       } MBUTT;


typedef struct { Window       win;       /* window */
		 int          x,y;       /* size of window */
		 unsigned int w,h;
		 u_long       fg,bg;     /* colors */
		 u_long       hi,lo;     /* colors */
		 /* const? */ char **str;   /* ptr to list of strings */
		 int          nstr;      /* number of strings */
		 int          selected;  /* number of 'selected' string */
		 int          nlines;    /* number of lines shown at once */
		 SCRL         scrl;      /* scrollbar that controls list */
		 int          filetypes; /* true if filetype icons to be drawn*/
		 int          dirsonly;  /* if true, only dirs selectable */
	       } LIST;


/* info structure filled in by the LoadXXX() image reading routines */
typedef struct { byte *pic;                  /* image data */
		 int   w, h;                 /* pic size */
		 int   type;                 /* PIC8 or PIC24 */

		 byte  r[256],g[256],b[256];
		                             /* colormap, if PIC8 */

		 int   normw, normh;         /* 'normal size' of image file
					        (normally eq. w,h, except when
						doing 'quick' load for icons */

		 int   frmType;              /* def. Format type to save in */
		 int   colType;              /* def. Color type to save in */
		 char  fullInfo[128];        /* Format: field in info box */
		 char  shrtInfo[128];        /* short format info */
		 char *comment;              /* comment text */

		 byte *exifInfo;             /* image info from digicam */
		 int   exifInfoSize;         /* size of image info */

		 int   numpages;             /* # of page files, if >1 */
		 char  pagebname[64];        /* basename of page files */
	       } PICINFO;

#define MAX_GHANDS 16   /* maximum # of GRAF handles */

#define N_GFB 6
#define GFB_SPLINE 0
#define GFB_LINE   1
#define GFB_ADDH   2
#define GFB_DELH   3
#define GFB_RESET  4
#define GFB_GAMMA  5

#define GVMAX 8

typedef struct {  Window win;               /* window ID */
		  Window gwin;              /* graph subwindow */
		  int    spline;            /* spline curve or lines? */
		  int    entergamma;        /* currently entering gamma value */
		  int    gammamode;         /* currently using gamma function */
		  double gamma;             /* gamma value (if gammamode) */
		  int    nhands;            /* current # of handles */
		  XPoint hands[MAX_GHANDS]; /* positions of handles */
		  byte   func[256];         /* output function of GRAF */
		  BUTT   butts[N_GFB];      /* control buttons */
		  u_long fg,bg;             /* colors */
		  const char *str;          /* title string */
		  char   gvstr[GVMAX+1];    /* gamma value input string */
		  void   (*drawobj)PARM((void));
		} GRAF;

typedef struct {  int    spline;
		  int    entergamma;
		  int    gammamode;
		  double gamma;
		  int    nhands;
		  XPoint hands[MAX_GHANDS];
		  char   gvstr[GVMAX+1];
		} GRAF_STATE;


/* MACROS */
#define CENTERX(f,x,str) ((x)-XTextWidth(f,str, (int) strlen(str))/2)
#define CENTERY(f,y) ((y)-((f->ascent+f->descent)/2)+f->ascent)

/* RANGE forces a to be in the range b..c (inclusive) */
#define RANGE(a,b,c) { if (a < b) a = b;  if (a > c) a = c; }

/* PTINRECT returns '1' if x,y is in rect */
#define PTINRECT(x,y,rx,ry,rw,rh) \
           ((x)>=(rx) && (y)>=(ry) && (x)<((rx)+(rw)) && (y)<((ry)+(rh)))

/* MONO returns total intensity of r,g,b triple (i = .33R + .5G + .17B) */
#define MONO(rd,gn,bl) ( ((int)(rd)*11 + (int)(gn)*16 + (int)(bl)*5) >> 5)

/* ISPIPE returns true if the passed in character is considered the
   start of a 'load-from-pipe' or 'save-to-pipe' string */
#define ISPIPE(c) ((c)=='!' || (c)=='|')

/* CMAPVIS returns true if the visual class has a modifyable cmap */
#define CMAPVIS(v) (((v)->class == PseudoColor) || ((v)->class == GrayScale))


/* X stuff */
WHERE Display       *theDisp;
WHERE int           theScreen;
WHERE unsigned int  ncells, dispDEEP; /* root color sizes */
WHERE unsigned int  dispWIDE, dispHIGH; /* screen sizes */
WHERE unsigned int  vrWIDE, vrHIGH, maxWIDE, maxHIGH; /* virtual root and max image sizes */
WHERE Colormap      theCmap, LocalCmap;
WHERE Window        spec_window, rootW, mainW, vrootW;
WHERE GC            theGC;
WHERE u_long        black, white, fg, bg, infofg, infobg;
WHERE u_long        hicol, locol;
WHERE u_long        blkRGB, whtRGB;
WHERE Font          mfont, monofont;
WHERE XFontStruct   *mfinfo, *monofinfo;
#ifdef TV_L10N
WHERE XFontSet      monofset;
WHERE XFontSetExtents *monofsetinfo;
#endif
WHERE Visual        *theVisual;
WHERE Cursor        arrow, cross, tcross, zoom, inviso, tlcorner;
WHERE Pixmap        iconPix, iconmask;
WHERE Pixmap        riconPix, riconmask;
WHERE int           showzoomcursor;
WHERE u_long        xorMasks[8];

/* XV global vars */
WHERE byte          *pic;                   /* ptr to loaded picture */
WHERE int            pWIDE,pHIGH;           /* size of 'pic' */
WHERE byte           rMap[256],gMap[256],bMap[256];  /* colormap */
WHERE char          *cmd;                   /* program name for printf's */
WHERE int            DEBUG;                 /* print debugging info */
WHERE int            mono;                  /* true if displaying grayscale */
WHERE char           formatStr[80];         /* short-form 'file format' */
WHERE int            picType;               /* CONV24_8BIT,CONV24_24BIT,etc.*/
WHERE char          *picComments;           /* text comments on current pic */
WHERE byte          *picExifInfo;           /* image info from digicam */
WHERE int            picExifInfoSize;       /* size of image info */

#ifdef TV_L10N
WHERE int            xlocale;		    /* true if Xlib supports locale */
#endif

WHERE int            numPages, curPage;     /* for multi-page files */
WHERE char           pageBaseName[64];      /* basename for multi-page files */

WHERE byte          *cpic;         /* cropped version of pic */
WHERE int           cWIDE, cHIGH,  /* size of cropped region */
                    cXOFF, cYOFF;  /* offset of region from 0,0 of pic */

WHERE byte          *epic;         /* expanded version of cpic */
                                   /* points to cpic when at 1:1 expansion */
                                   /* this is converted to 'theImage' */
WHERE int           eWIDE, eHIGH;  /* size of epic */

WHERE byte          *egampic;      /* expanded, gammified cpic
				      (only used in 24-bit mode) */

WHERE int           p_offx, p_offy;  /* offset of reparented windows */
WHERE int           ch_offx,ch_offy; /* ChngAttr ofst for reparented windows */
WHERE int           kludge_offx,     /* WM kludges for SetWindowPos routine */
                    kludge_offy;
WHERE int           winCtrPosKludge; /* kludge for popup positioning... */

WHERE int            ignoreConfigs;  /* an evil kludge... */

WHERE byte           rorg[256], gorg[256], borg[256];  /* ORIGINAL colormap */
WHERE byte           rcmap[256], gcmap[256], bcmap[256]; /*post-cmap-editing*/
WHERE byte           rdisp[256],gdisp[256],bdisp[256];  /* DISPLAYED colors */
WHERE byte           colAllocOrder[256];   /* order to allocate cols */
WHERE unsigned long  freecols[256]; /* list of pixel values to free */
WHERE byte           rwpc2pc[256]; /* mapping of shared pixels in -rw mode */
WHERE int            nfcols;       /* number of colors to free */
WHERE unsigned long  cols[256];    /* maps pic pixel values to X pixel vals */
WHERE int            fc2pcol[256]; /* maps freecols into pic pixel values */
WHERE int            numcols;      /* # of desired colors in picture */
#ifdef MACBINARY
WHERE char           macb_file;    /* True if this file type is MacBinary */
WHERE int            handlemacb;   /* True if we want to handle MacBinary */
#endif
#if defined(HAVE_PIC) || defined(HAVE_PIC2)
WHERE int            nopicadjust;  /* True if we don't want to adjust aspect */
#endif
#ifdef HAVE_PIC2
WHERE int            pic2split;    /* True if we want to split multiblocks */
#endif
#ifdef VS_ADJUST
WHERE int            vsadjust; /* True if we want to adjust aspect of icons */
#endif
#ifdef HAVE_MGCSFX
WHERE int            mgcsfx;    /* True if we want to force use MgcSfx */
WHERE int            nomgcsfx;  /* True if we don't want to use MgcSfx */
#endif

#define FSTRMAX 12   /* Number of function keys to support. */
WHERE char          *fkeycmds[FSTRMAX]; /* command to run when F# is pressed */

/* Std Cmap stuff */
WHERE byte           stdr[256], stdg[256], stdb[256];  /* std 3/3/2 cmap */
WHERE unsigned long  stdcols[256];                     /* 3/3/2 -> X colors */
WHERE byte           stdrdisp[256], stdgdisp[256], stdbdisp[256];
WHERE unsigned long  stdfreecols[256];   /* list of cols to free on exit */
WHERE int            stdnfcols;          /* # of cols in stdfreecols[] */

WHERE int            directConv[256];    /* used with directColor visuals */

/* colormap for 'browser' window */
WHERE byte           browR[256], browG[256], browB[256];  /* used in genIcon */
WHERE unsigned long  browcols[256];   /* maps 3/3/2 colors into X colors */
WHERE int            browPerfect;
WHERE Colormap       browCmap;

WHERE byte           fsgamcr[256]; /* gamma correction curve (for FS dither) */


/* vars that affect how color allocation is done */
WHERE int            allocMode;    /* AM_READONLY, AM_READWRITE */
WHERE int            rwthistime;   /* true if we DID use R/W color cells */
WHERE int            colorMapMode; /* CM_NORMAL, CM_PERFECT, CM_OWMCMAP ... */
WHERE int            haveStdCmap;  /* STD_NONE, STD_222, STD_666, STD_332 */
WHERE int            novbrowse;    /* if true, won't need colors for browser */
WHERE int            defaultCmapMode;  /* last user-selected cmap mode */

WHERE XImage        *theImage;     /* X version of epic */


WHERE int           ncols;         /* max # of (different) colors to alloc */

WHERE char          dummystr[128]; /* dummy string used for error messages */
WHERE char          initdir[MAXPATHLEN];   /* cwd when xv was started */
WHERE char          searchdir[MAXPATHLEN]; /* '-dir' option */
WHERE char          fullfname[MAXPATHLEN]; /* full name of current file */
WHERE char         *winTitle;      /* user-specified mainW title */

WHERE int           bwidth,        /* border width of created windows */
                    fixedaspect,   /* fixed aspect ratio */
                    conv24,        /* 24to8 algorithm to use (CONV24_*) */
                    ninstall,      /* true if using icccm-complaint WM
				      (a WM that will does install CMaps */
                    useroot,       /* true if we should draw in rootW */
		    nolimits,	   /* No limits on picture size */
		    resetroot,     /* true if we should clear in window mode */
                    noqcheck,      /* true if we should NOT do QuickCheck */
                    epicMode,      /* either SMOOTH, DITH, or RAW */
                    autoclose,     /* if true, autoclose when iconifying */
                    polling,       /* if true, reload if file changes */
                    viewonly,      /* if true, ignore any user input */
                    noFreeCols,    /* don't free colors when loading new pic */
                    autoquit,      /* quit in '-root' or when click on win */
                    xerrcode,      /* errorcode of last X error */
                    grabDelay,     /* # of seconds to sleep at start of Grab */
                    startGrab;     /* start immediate grab ? */

WHERE int           state824;      /* displays warning when going 8->24 */

WHERE float         defaspect,     /* default aspect ratio to use */
                    normaspect;    /* normal aspect ratio of this picture */

WHERE u_long        rootbg, rootfg; /* fg/bg for root border */
WHERE u_short       imagebgR;
WHERE u_short       imagebgG;      /* GRR 19980308:  bg for transpar. images */
WHERE u_short       imagebgB;
WHERE int           have_imagebg;
WHERE double        waitsec;       /* secs btwn pics. -1.0=wait for event */
WHERE int           waitloop;      /* loop at end of slide show? */
WHERE int           automax;       /* maximize pic on open */
WHERE int           rootMode;      /* mode used for -root images */

WHERE int           nostat;        /* if true, don't stat() in LdCurDir */

WHERE int           ctrlColor;     /* whether or not to use colored butts */

WHERE char         *def_str;       /* used by rd_*() routines */
WHERE int           def_int;
WHERE char         *tmpdir;        /* equal to "/tmp" or $TMPDIR env var */
WHERE Pixmap        gray25Tile,    /* used for 3d effect on 1-bit disp's */
                    gray50Tile;
WHERE int           autoDelete;    /* delete cmd-line files on exit? */

#define PRINTCMDLEN 256
WHERE char          printCmd[PRINTCMDLEN];

/* stuff used for 'info' box */
WHERE Window        infoW;
WHERE int           infoUp;        /* boolean:  whether infobox is visible */
WHERE int           infoMode;


/* stuff used for 'ctrl' box */
WHERE Window        ctrlW;
WHERE int           ctrlUp;        /* boolean:  whether ctrlbox is visible */
WHERE char         *namelist[MAXNAMES];  /* list of file names from argv */
WHERE char         *origlist[MAXNAMES];  /* only names from argv (autoDelete)*/
WHERE int           orignumnames;
WHERE char         *dispnames[MAXNAMES]; /* truncated names shown in listbox */
WHERE int           numnames, curname;
WHERE LIST          nList;
WHERE BUTT          but[NBUTTS];         /* command buttons in ctrl window */
WHERE Pixmap        grayTile;            /* bg pixmap used on 1-bit systems */
WHERE Pixmap        dimStip;             /* for drawing dim things */
WHERE int           dispMode;

WHERE MBUTT         dispMB;              /* display mode menu button */
WHERE MBUTT         conv24MB;            /* 24-to-8 conversion mode mbutt */
WHERE MBUTT         algMB;               /* Algorithms mbutt */
WHERE MBUTT         rootMB;
WHERE MBUTT         sizeMB;
WHERE MBUTT         windowMB;


/* stuff used for 'directory' box */
WHERE Window        dirW, dnamW;
WHERE int           dirUp;       /* is dirW mapped or not */
WHERE LIST          dList;       /* list of filenames in current directory */
WHERE BUTT          dbut[S_NBUTTS];
WHERE CBUTT         browseCB, savenormCB, saveselCB;


/* stuff used for 'gamma' box */
WHERE Window        gamW;
WHERE int           gamUp;       /* is gamW mapped or not */
WHERE BUTT          gbut[G_NBUTTS];
WHERE int           editColor;   /* currently selected color # */
WHERE int           hsvmode;     /* true if in HSVmode */
WHERE int cellgroup[256], curgroup, maxgroup;  /* grouped colorcell stuff */
WHERE int           cmapInGam;


/* stuff used for 'browse' box */
WHERE int           anyBrowUp;              /* whether *any* browser visible */
WHERE int           incrementalSearchTimeout;

/* stuff used for textview windows */
WHERE int           anyTextUp;              /* are any text windows visible? */
WHERE int           commentUp;              /* comment window up? */

/* stuff used for xvcut.c */
WHERE int           forceClipFile;          /* don't use property clipboard */
WHERE int           clearR, clearG, clearB; /* clear color in 24-bit mode */


/* stuff used for 'ps' box */
WHERE Window        psW;
WHERE int           psUp;         /* is psW mapped, or what? */
WHERE CBUTT         encapsCB, pscompCB;
WHERE const char   *gsDev, *gsGeomStr;
WHERE int           gsRes;


/* stuff used for 'pcd' box */
WHERE Window        pcdW;
WHERE int           pcdUp;        /* is pcdW mapped, or what? */


#ifdef HAVE_JPEG
/* stuff used for 'jpeg' box */
WHERE Window        jpegW;
WHERE int           jpegUp;       /* is jpegW mapped, or what? */
#endif


#ifdef HAVE_JP2K
/* stuff used for 'jp2k' box */
WHERE Window        jp2kW;
WHERE int           jp2kUp;       /* is jp2kW mapped, or what? */
#endif


#ifdef HAVE_TIFF
/* stuff used for 'tiff' box */
WHERE Window        tiffW;
WHERE int           tiffUp;       /* is tiffW mapped, or what? */
#endif


#ifdef HAVE_PNG
/* stuff used for 'png' box */
WHERE Window        pngW;
WHERE int           pngUp;        /* is pngW mapped, or what? */
#endif


#ifdef ENABLE_FIXPIX_SMOOTH
WHERE int           do_fixpix_smooth;  /* GRR 19980607: runtime FS dithering */
#endif

#ifdef HAVE_PIC2
/* stuff used for 'pic2' box */
WHERE Window        pic2W;
WHERE int           pic2Up;      /* is pic2W mapped, or what? */
#endif /* HAVE_PIC2 */

#ifdef HAVE_PCD
/* stuff used for 'pcd' box */
WHERE Window        pcdW;
WHERE int           pcdUp;       /* is pcdW mapped, or what? */
#endif /* HAVE_PCD */

#ifdef HAVE_MGCSFX
/* stuff used for 'mgcsfx' box */
WHERE Window        mgcsfxW;
WHERE Window        mgcsfxNameW;
WHERE int           mgcsfxUp;      /* is mgcsfxW mapped, or what? */
#endif /* HAVE_MGCSFX */

#ifdef TV_L10N
/* stuff used for TextViewer Japanization */
#  define LOCALE_USASCII    0
#  define LOCALE_EUCJ       1
#  define LOCALE_JIS        2
#  define LOCALE_MSCODE     3

#  ifndef LOCALE_DEFAULT
#    define LOCALE_DEFAULT  0
#  endif /* !LOCALE_DEFAULT */

#  ifndef MAIN
     extern char *localeList[];
#  else
#    ifndef LOCALE_NAME_EUC
#      ifndef X_LOCALE
#        if defined(__FreeBSD__)
	   char *localeList[] = {"", "ja_JP.EUC", "none", "none"};
#        elif defined(__linux__)
	   char *localeList[] = {"", "ja_JP.eucJP", "none", "ja_JP.SJIS"};
#        elif defined(__sun) || defined(sun)
	   char *localeList[] = {"", "ja", "none", "none"};
#        elif defined(__sgi)	/* sgi, __sgi, __sgi__ (gcc) */
	   char *localeList[] = {"", "ja_JP.EUC", "none", "none"};
#        elif defined(sony_news)
	   char *localeList[] = {"", "ja_JP.EUC", "none", "ja_JP.SJIS"};
#        elif defined(nec)
	   char *localeList[] = {"", "japan", "none", "none"};
#        elif defined(__hpux)
	   char *localeList[] = {"", "japanese.euc", "none", "japanese"};
#        elif defined(__osf__)
	   char *localeList[] = {"", "ja_JP.deckanji", "none", "ja_JP.SJIS"};
#        elif defined(_AIX)
	   char *localeList[] = {"", "ja_JP", "none", "Ja_JP" };
#        elif defined(__bsdi)
	   char *localeList[] = {"", "Japanese-EUC", "none", "none" };
#        else
	   char *localeList[] = {"", "ja_JP.EUC", "ja_JP.JIS", "ja_JP.SJIS"};
#        endif
#      else
#        if (XlibSpecificationRelease > 5)
           char *localeList[] = {"", "ja_JP.eucJP", "ja_JP.JIS7",
				 "ja_JP.SJIS"};
#        else
           char *localeList[] = {"", "ja_JP.ujis", "ja_JP.jis7",
				 "ja_JP.mscode"};
#        endif
#      endif /* X_LOCALE */
#    else
       char *localeList[] = {"", LOCALE_NAME_EUC,
			     LOCALE_NAME_JIS, LOCALE_NAME_MSCODE};
#    endif /* LOCALE_NAME_EUC */
#  endif /* MAIN */
#endif /* TV_L10N */

#undef WHERE



/* function declarations for externally-callable functions */

/****************************** XV.C ****************************/
int   ReadFileType         PARM((char *));
int   ReadPicFile          PARM((char *, int, PICINFO *, int));
int   UncompressFile       PARM((char *, char *, int));
void  KillPageFiles        PARM((char *, int));
#ifdef MACBINARY          
int   RemoveMacbinary      PARM((char *, char *));
#endif                    

void NewPicGetColors       PARM((int, int));
void FixAspect             PARM((int, int *, int *));
void ActivePrevNext        PARM((void));
int  DeleteCmd             PARM((void));
void StickInCtrlList       PARM((int));
void AddFNameToCtrlList    PARM((const char *, const char *));
void ChangedCtrlList       PARM((void));
void HandleDispMode        PARM((void));
char *lower_str            PARM((char *));
int  rd_int                PARM((const char *));
int  rd_str                PARM((const char *));
int  rd_flag               PARM((const char *));


/*************************** XV24TO8.C **************************/
void Init24to8             PARM((void));
byte *Conv24to8            PARM((byte *, int, int, int,
				 byte *, byte *, byte *));

byte *Conv8to24            PARM((byte *, int, int, byte *, byte *, byte *));


/*************************** XVALG.C ***************************/
void AlgInit               PARM((void));
void DoAlg                 PARM((int));


/*************************** XVBROWSE.C ************************/
void CreateBrowse          PARM((const char *, const char *, const char *,
				 const char *, const char *));
void OpenBrowse            PARM((void));
void HideBrowseWindows     PARM((void));
void UnHideBrowseWindows   PARM((void));
void SetBrowseCursor       PARM((Cursor));
void KillBrowseWindows     PARM((void));
int  BrowseCheckEvent      PARM((XEvent *, int *, int *));
int  BrowseDelWin          PARM((Window));
void SetBrowStr            PARM((const char *));
void RegenBrowseIcons      PARM((void));
void BRDeletedFile         PARM((char *));
void BRCreatedFile         PARM((char *));


/**************************** XVBUTT.C ***************************/
void BTCreate              PARM((BUTT *, Window, int, int, u_int, u_int,
				 const char *, u_long, u_long, u_long, u_long));

void BTSetActive           PARM((BUTT *, int));
void BTRedraw              PARM((BUTT *));
int  BTTrack               PARM((BUTT *));


RBUTT *RBCreate            PARM((RBUTT *, Window, int, int, const char *,
				 u_long, u_long, u_long, u_long));

void   RBRedraw            PARM((RBUTT *, int));
void   RBSelect            PARM((RBUTT *, int));
int    RBWhich             PARM((RBUTT *));
int    RBCount             PARM((RBUTT *));
void   RBSetActive         PARM((RBUTT *, int, int));
int    RBClick             PARM((RBUTT *, int, int));
int    RBTrack             PARM((RBUTT *, int));


void   CBCreate            PARM((CBUTT *, Window, int, int, const char *,
				 u_long, u_long, u_long, u_long));

void   CBRedraw            PARM((CBUTT *));
void   CBSetActive         PARM((CBUTT *, int));
int    CBClick             PARM((CBUTT *,int,int));
int    CBTrack             PARM((CBUTT *));


void   MBCreate            PARM((MBUTT *, Window, int, int, u_int, u_int,
				 const char *, const char * const *, int,
				 u_long, u_long, u_long, u_long));

void   MBRedraw            PARM((MBUTT *));
void   MBSetActive         PARM((MBUTT *, int));
int    MBWhich             PARM((MBUTT *));
void   MBSelect            PARM((MBUTT *, int));
int    MBClick             PARM((MBUTT *, int, int));
int    MBTrack             PARM((MBUTT *));


/*************************** XVCOLOR.C ***************************/
void   SortColormap        PARM((byte *, int, int, int *, byte*,byte*,byte*,
				 byte *, byte *));
void   ColorCompress8      PARM((byte *));
void   AllocColors         PARM((void));
Status xvAllocColor        PARM((Display *, Colormap, XColor *));
void   xvFreeColors        PARM((Display *, Colormap, u_long *, int, u_long));
void   FreeColors          PARM((void));
void   ApplyEditColor      PARM((int));
int    MakeStdCmaps        PARM((void));
void   MakeBrowCmap        PARM((void));
void   ChangeCmapMode      PARM((int, int, int));


/**************************** XVCTRL.C **************************/
void   CreateCtrl          PARM((const char *));
void   SetButtPix          PARM((BUTT *, Pixmap, int, int));
Pixmap MakePix1            PARM((Window, byte *, int, int));

void CtrlBox               PARM((int));
void RedrawCtrl            PARM((int, int, int, int));
int  ClickCtrl             PARM((int, int));
void DrawCtrlNumFiles      PARM((void));
void DrawCtrlStr           PARM((void));
void ScrollToCurrent       PARM((LIST *));

void LSCreate              PARM((LIST *, Window, int, int, int, int, int,
				 char **, int, u_long, u_long, u_long, u_long,
				 void (*)(int, SCRL *), int, int));

void LSRedraw              PARM((LIST *, int));
int  LSClick               PARM((LIST *, XButtonEvent *));
void LSChangeData          PARM((LIST *, char **, int));
void LSNewData             PARM((LIST *, char **, int));
void LSKey                 PARM((LIST *, int));
int  rd_str_cl             PARM((const char *, const char *, int));


/**************************** XVCUT.C ***************************/
int  CutAllowed            PARM((void));
int  PasteAllowed          PARM((void));
void DoImgCopy             PARM((void));
void DoImgCut              PARM((void));
void DoImgClear            PARM((void));
void DoImgPaste            PARM((void));

void SaveToClip            PARM((byte *));
void InitSelection         PARM((void));
int  HaveSelection         PARM((void));
int  GetSelType            PARM((void));
void GetSelRCoords         PARM((int *, int *, int *, int *));
void EnableSelection       PARM((int));
void DrawSelection         PARM((int));
int  DoSelection           PARM((XButtonEvent *));
void MoveGrowSelection     PARM((int, int, int, int));
void BlinkSelection        PARM((int));
void FlashSelection        PARM((int));

void CropRect2Rect         PARM((int*,int*,int*,int*, int,int,int,int));
void CoordE2C              PARM((int, int, int *, int *));
void CoordC2E              PARM((int, int, int *, int *));
void CoordP2C              PARM((int, int, int *, int *));
void CoordC2P              PARM((int, int, int *, int *));
void CoordP2E              PARM((int, int, int *, int *));
void CoordE2P              PARM((int, int, int *, int *));


/*************************** XVDFLT.C ***************************/
void LoadDfltPic           PARM((PICINFO *));
void xbm2pic               PARM((byte *, int, int, byte *, int, int, int, int,
				 int));
void DrawStr2Pic           PARM((char *, int, int, byte *, int, int, int));


/*************************** XVDIAL.C ***************************/
void DCreate               PARM((DIAL *, Window, int, int, int, int,
                                 double, double, double, double, double,
                                 u_long, u_long, u_long, u_long,
                                 const char *, const char *));

void DSetRange             PARM((DIAL *, double,double,double,double,double));
void DSetVal               PARM((DIAL *, double));
void DSetActive            PARM((DIAL *, int));
void DRedraw               PARM((DIAL *));
int  DTrack                PARM((DIAL *, int, int));


/**************************** XVDIR.C ***************************/
void CreateDirW            PARM((char *));
void DirBox                PARM((int));
void RedrawDirW            PARM((int,int,int,int));
int  ClickDirW             PARM((int, int));
void LoadCurrentDirectory  PARM((void));
void GetDirPath            PARM((char *));
int  DirCheckCD            PARM((void));
void RedrawDDirW           PARM((void));
void RedrawDNamW           PARM((void));
void SelectDir             PARM((int));
void TrackDDirW            PARM((int,int));
int  DirKey                PARM((int));
int  DoSave                PARM((void));
void SetDirFName           PARM((const char *));
char *GetDirFName          PARM((void));
char *GetDirFullName       PARM((void));
void SetDirSaveMode        PARM((int, int));
int  Globify               PARM((char *));
FILE *OpenOutFile          PARM((const char *));
int  CloseOutFile          PARM((FILE *, const char *, int));

byte *GenSavePic           PARM((int*, int*,int*, int*, int*,
				 byte**, byte**, byte**));
void GetSaveSize           PARM((int *, int *));

void InitPoll              PARM((void));
int  CheckPoll             PARM((int));
void DIRDeletedFile        PARM((char *));
void DIRCreatedFile        PARM((char *));
FILE *pic2_OpenOutFile     PARM((char *, int *));
void pic2_KillNullFile     PARM((FILE *));
int  OpenOutFileDesc       PARM((char *));


/****************************** XVEVENT.C ****************************/
int  EventLoop             PARM((void));
int  HandleEvent           PARM((XEvent *, int *));

void NewCutBuffer          PARM((char *));
void DrawWindow            PARM((int,int,int,int));
void WResize               PARM((int, int));
void WRotate               PARM((void));
void WCrop                 PARM((int, int, int, int));
void WUnCrop               PARM((void));
void GetWindowPos          PARM((XWindowAttributes *));
void SetWindowPos          PARM((XWindowAttributes *));
void SetEpicMode           PARM((void));
int  xvErrorHandler        PARM((Display *, XErrorEvent *));


/**************************** XVGAM.C **************************/
void CreateGam             PARM((const char *, double, double, double, double,
				 int));
int  GamCheckEvent         PARM((XEvent *));
void GamBox                PARM((int));
void NewCMap               PARM((void));
void RedrawCMap            PARM((void));
void ChangeEC              PARM((int));
void ApplyECctrls          PARM((void));
void GenerateFSGamma       PARM((void));
void DoNorm                PARM((void));
void DoHistEq              PARM((void));
void GammifyColors         PARM((void));
void Gammify1              PARM((int));
void rgb2hsv               PARM((int, int, int, double *, double *, double *));
void hsv2rgb               PARM((double, double, double, int *, int *, int *));

byte *GammifyPic24         PARM((byte *, int, int));
void GamSetAutoApply       PARM((int));


/**************************** XVGRAB.C ***************************/
int Grab                   PARM((void));
int LoadGrab               PARM((PICINFO *));


/**************************** XVGRAF.C ***************************/
void   CreateGraf          PARM((GRAF *, Window, int, int,
				 u_long, u_long, const char *));

void   InitGraf            PARM((GRAF *));
void   RedrawGraf          PARM((GRAF *, int));
int    ClickGraf           PARM((GRAF *, Window, int, int));
int    GrafKey             PARM((GRAF *, char *));
void   GenerateGrafFunc    PARM((GRAF *, int));
void   Graf2Str            PARM((GRAF_STATE *, char *));
int    Str2Graf            PARM((GRAF_STATE *, const char *));
void   GetGrafState        PARM((GRAF *, GRAF_STATE *));
int    SetGrafState        PARM((GRAF *, GRAF_STATE *));
void   InitSpline          PARM((int *, int *, int, double *));
double EvalSpline          PARM((int *, int *, double *, int, double));


/*************************** XVIMAGE.C ***************************/
void Resize                PARM((int, int));
void GenerateCpic          PARM((void));
void GenerateEpic          PARM((int, int));
void DoZoom                PARM((int, int, u_int));
void Crop                  PARM((void));
void UnCrop                PARM((void));
void AutoCrop              PARM((void));
int  DoAutoCrop            PARM((void));
void DoCrop                PARM((int, int, int, int));
void Rotate                PARM((int));
void DoRotate              PARM((int));
void RotatePic             PARM((byte *, int, int *, int *, int));
void Flip                  PARM((int));
void FlipPic               PARM((byte *, int, int, int));
void InstallNewPic         PARM((void));
void DrawEpic              PARM((void));
void KillOldPics           PARM((void));

byte *FSDither             PARM((byte *, int, int, int,
				 byte *, byte *, byte *, int, int));

void CreateXImage          PARM((void));
XImage *Pic8ToXImage       PARM((byte *, u_int, u_int, u_long *,
				 byte *, byte *, byte *));

XImage *Pic24ToXImage      PARM((byte *, u_int, u_int));

void Set824Menus           PARM((int));
void Change824Mode         PARM((int));
void FreeEpic              PARM((void));
void InvertPic24           PARM((byte *, int, int));

byte *XVGetSubImage        PARM((byte *, int, int,int, int,int,int,int));

int  DoPad                 PARM((int, char *, int, int, int, int));
int  LoadPad               PARM((PICINFO *, char *));


/*************************** XVINFO.C ***************************/
void  CreateInfo           PARM((const char *));
void  InfoBox              PARM((int));
void  RedrawInfo           PARM((int, int, int, int));
void  SetInfoMode          PARM((int));
char *GetISTR              PARM((int));

#if defined(__STDC__) && !defined(NOSTDHDRS)
void  SetISTR(int, ...);
#else
void  SetISTR();
#endif


/*************************** XVMISC.C ***************************/
void StoreDeleteWindowProp PARM((Window));
Window CreateWindow        PARM((const char *, const char *, const char *,
				 int, int, u_long, u_long, int));
void DrawString            PARM((Window, int, int, const char *));
void CenterString          PARM((Window, int, int, const char *));
void ULineString           PARM((Window, int, int, const char *));
int  StringWidth           PARM((const char *));
int  CursorKey             PARM((KeySym, int, int));
void FakeButtonPress       PARM((BUTT *));
void FakeKeyPress          PARM((Window, KeySym));
void GenExpose             PARM((Window, int, int, u_int, u_int));
void DimRect               PARM((Window, int, int, u_int, u_int, u_long));

void Draw3dRect            PARM((Window, int, int, u_int, u_int, int, int,
				 u_long, u_long, u_long));

void RemapKeyCheck         PARM((KeySym, char *, int *));
void xvDestroyImage        PARM((XImage *));
void SetCropString         PARM((void));
void SetSelectionString    PARM((void));
void Warning               PARM((void));
void FatalError            PARM((const char *));
void Quit                  PARM((int));
void LoadFishCursors       PARM((void));
void WaitCursor            PARM((void));
void SetCursors            PARM((int));
const char *BaseName       PARM((const char *));

void DrawTempGauge         PARM((Window, int, int, int, int, double, u_long,
				 u_long, u_long, u_long, const char *));
void ProgressMeter         PARM((int, int, int, const char *));
void XVDeletedFile         PARM((char *));
void XVCreatedFile         PARM((char *));
void xvbcopy               PARM((const char *, char *, size_t));
int  xvbcmp                PARM((const char *, const char *, size_t));
void xvbzero               PARM((char *, size_t));
void xv_getwd              PARM((char *, size_t));
char *xv_strstr            PARM((const char *, const char *));
FILE *xv_fopen             PARM((const char *, const char *));
void xv_mktemp             PARM((char *, const char *));
void Timer                 PARM((int));


/*************************** XVPOPUP.C ***************************/
void  CenterMapWindow      PARM((Window, int, int, int, int));
int   PopUp                PARM((const char *, const char **, int));
void  ErrPopUp             PARM((const char *, const char *));
int   GetStrPopUp          PARM((const char *, const char **, int, char *, int,
				 const char *, int));
int   GrabPopUp            PARM((int *, int *));
int   PadPopUp             PARM((int *, char **, int *, int *, int *, int *));
void  ClosePopUp           PARM((void));
void  OpenAlert            PARM((const char *));
void  CloseAlert           PARM((void));
int   PUCheckEvent         PARM((XEvent *));


/**************************** XVROOT.C ****************************/
void MakeRootPic           PARM((void));
void ClearRoot             PARM((void));
void SaveRootInfo          PARM((void));
void KillOldRootInfo       PARM((void));


/*************************** XVSCRL.C ***************************/
void SCCreate              PARM((SCRL *, Window, int, int, int, int,
				 int, int, int, int, u_long, u_long,
				 u_long, u_long, void (*)(int, SCRL *)));

void SCChange              PARM((SCRL *, int, int, int, int, int,
				 int, int, int));

void SCSetRange            PARM((SCRL *, int, int, int, int));
int  SCSetVal              PARM((SCRL *, int));
void SCRedraw              PARM((SCRL *));
void SCTrack               PARM((SCRL *, int, int));


/*************************** XVSMOOTH.C ***************************/
byte *SmoothResize         PARM((byte *, int, int, int, int, byte *, byte *,
				 byte *, byte *, byte *, byte *, int));

byte *Smooth24             PARM((byte *, int, int, int, int, int,
				 byte *, byte *, byte *));

byte *DoColorDither        PARM((byte *, byte *, int, int, byte *, byte *,
				 byte *, byte *, byte *, byte *, int));

byte *Do332ColorDither     PARM((byte *, byte *, int, int, byte *, byte *,
				 byte *, byte *, byte *, byte *, int));


/*************************** XVTEXT.C ************************/
void CreateTextWins        PARM((const char *, const char *));
int  TextView              PARM((const char *));
void OpenTextView          PARM((const char *, int, const char *, int));

void OpenCommentText       PARM((void));
void CloseCommentText      PARM((void));
void ChangeCommentText     PARM((void));

void ShowLicense           PARM((void));
void ShowKeyHelp           PARM((void));

void HideTextWindows       PARM((void));
void UnHideTextWindows     PARM((void));
void RaiseTextWindows      PARM((void));
void SetTextCursor         PARM((Cursor));
void KillTextWindows       PARM((void));
int  TextCheckEvent        PARM((XEvent *, int *, int *));
int  TextDelWin            PARM((Window));

int  CharsetCheckEvent     PARM((XEvent *));
int  CharsetDelWin         PARM((Window));


/**************************** XVVD.C ****************************/
void  Vdinit               PARM((void));
void  Vdsettle             PARM((void));
int   Chvdir               PARM((char *));
void  Dirtovd              PARM((char *));
void  Vdtodir              PARM((char *));
void  Dirtosubst           PARM((char *));
int   Mkvdir               PARM((char *));
void  Mkvdir_force         PARM((char *));
int   Rmvdir               PARM((char *));
int   Movevdir             PARM((char *, char *));
int   Isarchive            PARM((char *));
int   Isvdir               PARM((char *));
void  vd_HUPhandler        PARM((void));
void  vd_handler           PARM((int));
int   vd_Xhandler          PARM((Display *, XErrorEvent *));
int   vd_XIOhandler        PARM((Display *));
void  vd_handler_setup     PARM((void));



/*=======================================================================*/
/*                             IMAGE FORMATS                             */
/*=======================================================================*/

/**************************** XVBMP.C ***************************/
int LoadBMP                PARM((char *, PICINFO *));
int WriteBMP               PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int));

/**************************** XVFITS.C ***************************/
int LoadFITS               PARM((char *, PICINFO *, int));
int WriteFITS              PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int, char *));

/**************************** XVGIF.C ***************************/
int LoadGIF                PARM((char *, PICINFO *));

/**************************** XVGIFWR.C **************************/
int WriteGIF               PARM((FILE *, byte *, int, int, int,
				 byte *, byte *, byte *, int, int, char *));

/**************************** XVHIPS.C ***************************/
int   LoadHIPS             PARM((char *, PICINFO *));

/**************************** XVIFF.C ***************************/
int LoadIFF                PARM((char *, PICINFO *));

/**************************** XVIRIS.C ***************************/
int LoadIRIS               PARM((char *, PICINFO *));
int WriteIRIS              PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int));

/**************************** XVJP2K.C ***************************/
int  LoadJPC               PARM((char *, register PICINFO *, int));
int  LoadJP2               PARM((char *, register PICINFO *, int));
void CreateJP2KW           PARM((void));
void JP2KSaveParams        PARM((int, char *, int));
void JP2KDialog            PARM((int vis));
int  JP2KCheckEvent        PARM((register XEvent *));
void VersionInfoJP2K       PARM((void));		/* GRR 20070304 */

/**************************** XVJPEG.C ***************************/
int  LoadJFIF              PARM((char *, PICINFO *, int));
void CreateJPEGW           PARM((void));
void JPEGDialog            PARM((int));
int  JPEGCheckEvent        PARM((XEvent *));
void JPEGSaveParams        PARM((char *, int));
void VersionInfoJPEG       PARM((void));		/* GRR 19980605 */

/**************************** XVMAG.C ***************************/
int   LoadMAG              PARM((char *, PICINFO *));
int   WriteMAG             PARM((FILE *, byte *, int, int, int,
				 byte *, byte *, byte *, int, int, char *));

/**************************** XVMAKI.C ***************************/
int   LoadMAKI             PARM((char *, PICINFO *));
int   WriteMAKI            PARM((FILE *, byte *, int, int, int,
				 byte *, byte *, byte *, int, int));

/**************************** XVMGCSFX.C ***************************/
int   is_mgcsfx            PARM((char *, unsigned char *, int));
char *mgcsfx_auto_input_com PARM((char *));
int   LoadMGCSFX           PARM((char *, PICINFO *));
void  CreateMGCSFXW        PARM((void));
void  MGCSFXDialog         PARM((int));
int   MGCSFXCheckEvent     PARM((XEvent *));
int   MGCSFXSaveParams     PARM((char *, int));

int getInputCom            PARM((void));
int getOutputCom           PARM((void));

/**************************** XVPBM.C ***************************/
#ifdef HAVE_MGCSFX
int LoadPBM                PARM((char *, PICINFO *, int));
#else
int LoadPBM                PARM((char *, PICINFO *));
#endif
int WritePBM               PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int, int, char *));

/**************************** XVPCD.C ***************************/
int   LoadPCD              PARM((char *, PICINFO *, int));
void  CreatePCDW           PARM((void));
void  PCDDialog            PARM((int));
int   PCDCheckEvent        PARM((XEvent *));
void  PCDSetParamOptions   PARM((const char *));

/**************************** XVPCX.C ***************************/
int LoadPCX                PARM((char *, PICINFO *));

/**************************** XVPDS.C ***************************/
int LoadPDS                PARM((char *, PICINFO *));

/**************************** XVPI.C ***************************/
int   LoadPi               PARM((char *, PICINFO *));
int   WritePi              PARM((FILE *, byte *, int, int, int,
				 byte *, byte *, byte *, int, int, char *));

/**************************** XVPIC.C ***************************/
int   LoadPIC              PARM((char *, PICINFO *));
int   WritePIC             PARM((FILE *, byte *, int, int, int,
				 byte *, byte *, byte *, int, int, char *));

/**************************** XVPIC2.C ***************************/
int   LoadPIC2             PARM((char *, PICINFO *, int));
void  CreatePIC2W          PARM((void));
void  PIC2Dialog           PARM((int));
int   PIC2CheckEvent       PARM((XEvent *));
int   PIC2SetParamOptions  PARM((char *));

/**************************** XVPM.C ****************************/
int LoadPM                 PARM((char *, PICINFO *));
int WritePM                PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int, char *));

/**************************** XVPNG.C ***************************/
int  LoadPNG               PARM((char *, PICINFO *));
void CreatePNGW            PARM((void));
void PNGDialog             PARM((int));
int  PNGCheckEvent         PARM((XEvent *));
void PNGSaveParams         PARM((char *, int));
void VersionInfoPNG        PARM((void));		/* GRR 19980605 */

/**************************** XVPS.C ****************************/
void  CreatePSD            PARM((char *));
void  PSDialog             PARM((int));
int   PSCheckEvent         PARM((XEvent *));
void  PSSaveParams         PARM((char *, int));
void  PSResize             PARM((void));
int   LoadPS               PARM((char *, PICINFO *, int));

/**************************** XVRLE.C ***************************/
int LoadRLE                PARM((char *, PICINFO *));

/**************************** XVSUNRAS.C ***************************/
int LoadSunRas             PARM((char *, PICINFO *));
int WriteSunRas            PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte*, int, int, int));

/**************************** XVTARGA.C ***************************/
int LoadTarga              PARM((char *, PICINFO *));
int WriteTarga             PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int));

/**************************** XVTIFF.C ***************************/
int   LoadTIFF             PARM((char *, PICINFO *, int));
void  CreateTIFFW          PARM((void));
void  TIFFDialog           PARM((int));
int   TIFFCheckEvent       PARM((XEvent *));
void  TIFFSaveParams       PARM((char *, int));
void  VersionInfoTIFF      PARM((void));		/* GRR 19980605 */

/**************************** XVWBMP.C ***************************/
int LoadWBMP               PARM((char *, PICINFO *));
int WriteWBMP              PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int));

/**************************** XVXBM.C ***************************/
int LoadXBM                PARM((char *, PICINFO *));
int WriteXBM               PARM((FILE *, byte *, int, int, byte *, byte *,
				 byte *, char *));

/**************************** XVXPM.C ***************************/
int LoadXPM                PARM((char *, PICINFO *));
int WriteXPM               PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int, char *, char *));

/**************************** XVXWD.C ***************************/
int LoadXWD                PARM((char *, PICINFO *));

/**************************** XVZX.C [JCE] **********************/
int LoadZX                 PARM((char *, PICINFO *));
int WriteZX                PARM((FILE *, byte *, int, int, int, byte *,
				 byte *, byte *, int, int, char *));
