/*
 * config.h  -  configuration file for optional XV components & such
 */


/***************************************************************************
 * GZIP'd file support
 *
 * if you have the gnu uncompression utility 'gunzip', XV can use it to
 * automatically 'unzip' any gzip'd files.  To enable this feature,
 * change 'undef' to 'define' in the following line.  Needless to say, if 
 * your gunzip is installed elsewhere on your machine, change the 'GUNZIP'
 * definition appropriately. (use 'which gunzip' to find if you have gunzip, 
 * and where it lives)
 */
#undef USE_GUNZIP

#ifdef USE_GUNZIP
#  ifdef VMS
#    define GUNZIP "UNCOMPRESS"
#  else
#    define GUNZIP "/usr/local/bin/gunzip -q"
#  endif
#endif


/***************************************************************************
 * compress'd file support
 *
 * if you have GUNZIP defined above, just ignore this, as 'gunzip' can
 * handle 'compress'ed files as well as 'gzip'ed files.  Otherwise, you
 * should define UNCOMPRESS so that it points to your uncompress program.
 * (use 'which uncompress' to find out if you have uncompress, and where
 * it lives).  Note that you probably won't have to change anything,
 * as it tries to be clever on systems where uncompress lives in an unusual
 * location.
 */
#define UNCOMPRESS "/usr/ucb/uncompress"

#if defined(hpux) || defined(SVR4) || defined(__386BSD__)
#  undef  UNCOMPRESS
#  define UNCOMPRESS "/usr/bin/uncompress"
#endif

#if defined(sgi)
#  undef UNCOMPRESS
#  define UNCOMPRESS "/usr/bsd/uncompress"
#endif

#ifdef VMS
  /* The default is to use the provided Decompress program.
     If you have the GNU UnZip program, define it (above)
     to be "UnCompress" and it will be selected here */
#  undef UNCOMPRESS
#  ifdef GUNZIP
#    define UNCOMPRESS GUNZIP
#  else
#    define UNCOMPRESS "Decompress"
#  endif
#endif


#ifdef GUNZIP
#  undef  UNCOMPRESS
#  define UNCOMPRESS GUNZIP
#endif


/***************************************************************************
 * PostScript file input support:
 *
 * if you have the 'ghostscript' package installed (version 2.6 or later),
 * XV can use it to read and display PostScript files.  To do so, 
 * uncomment the '#define GS_PATH' line, below.  You probably will not
 * need to modify the GS_LIB or GS_DEV lines, but if you do modify them,
 * be sure to uncomment them, as well.
 *
 * the ghostscript package can be acquired via anonymous ftp on 
 * prep.ai.mit.edu, in the 'pub/gnu' directory
 *
 * GS_PATH specifies the complete path to your gs executable.  
 *
 * GS_LIB should be set if there's some other gs libs that should be 
 * searched, but aren't by default.  (In which case you should probably 
 * just fix your 'gs' so it looks in the right places without being told...)
 *
 * GS_DEV is the file format that ghostscript will convert PS into.  It
 * should not need to be changed
 */

/* #define GS_PATH "/usr/local/bin/gs" */
/* #define GS_LIB  "."                 */
/* #define GS_DEV  "ppmraw"            */


/***************************************************************************
 * 'old-style' XV logo image:
 *
 * XV now has a nifty, new logo image.  The downside is that it increases
 * the size of the 'xv' executable by 250K or so, and it's possible that 
 * your compiler may choke while compiling 'xvdflt.c'.  If you're compiler
 * can't handle it, or you're running Linux on a system with minimal memory,
 * change 'undef' to 'define' in the following line
 */

#undef USEOLDPIC


/***************************************************************************
 * Backing Store:
 * 
 * XV can request that 'Backing Store' may be turned on ('WhenMapped') for 
 * several of its windows, which may help performance over a slow network
 * connection.  However, it has been known to behave strangely (or crash)
 * on some X servers, so it's left here as an option.  If you run into trouble
 * (strange behavior with the image window), you might want to turn it off by
 * changing 'define' to 'undef' in the following line:
 */

#define BACKING_STORE

