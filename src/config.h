/*
 * config.h  -  configuration file for optional XV components & such
 */


/***************************************************************************
 * GZIP'd file support
 *
 * if you have the GNU uncompression utility 'gunzip' (or 'gzip' itself,
 * which is just a link to gunzip), XV can use it to automatically 'unzip'
 * any gzip'd files.  To enable this feature, change 'undef' to 'define' in
 * the following line.  Needless to say, if your gunzip is installed elsewhere
 * on your machine, change the 'GUNZIP' definition appropriately. (use
 * 'which gunzip' to find if you have gunzip, and where it lives; ditto for
 * gzip)
 */
#define USE_GUNZIP

#ifdef USE_GUNZIP
#  ifdef VMS
#    define GUNZIP "UNCOMPRESS"
#  else
#    define GUNZIP "gzip -dq"
#  endif
#endif


/***************************************************************************
 * BZIP2'd file support
 *
 * if you have the uncompression utility 'bunzip2' (or 'bzip2' itself, which
 * is just a link to bunzip2), XV can use it to automatically 'unzip' any
 * bzip2'd files.  To enable this feature, change 'undef' to 'define' in the
 * following line (if not already done).  Use 'which bunzip2' or 'which bzip2'
 * to find if you have bzip2/bunzip2, and where it lives.
 */
#define USE_BUNZIP2

#ifdef USE_BUNZIP2
#  define BUNZIP2 "bzip2 -d"  /* should this include the full path? */
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

#if defined(hpux) || defined(SVR4) || \
    defined(__386BSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__linux__)
    /*
     I want to use BSD macro for checking if this OS is *BSD or not,
     but the macro is defined in <sys/parm.h>, which I don't know all
     machine has or not.
     */
#  undef  UNCOMPRESS
#  define UNCOMPRESS "uncompress"
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
#define GS_PATH "gs"
/* #define GS_LIB  "."                 */
/* #define GS_DEV  "ppmraw"            */


/***************************************************************************
 * 'old-style' XV logo image:
 *
 * XV now has a nifty, new logo image.  The downside is that it increases
 * the size of the 'xv' executable by 250K or so, and it's possible that
 * your compiler may choke while compiling 'xvdflt.c'.  If your compiler
 * can't handle it, or you're running Linux on a system with minimal memory,
 * change 'undef' to 'define' in the following line:
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


/***************************************************************************
 * TIFF YCbCr-to-RGB conversion:
 *
 * Newer versions of libtiff can be compiled with libjpeg for JPEG-in-TIFF
 * support, and according to Scott Marovich, "the IJG JPEG Library...sometimes
 * seems to produce slightly more accurate results" (one known example:  the
 * 'quad-jpeg.tif' test image).  In addition, libtiff can be compiled with
 * "old JPEG" support, although its configure script will not enable that by
 * default.  Change 'define' and 'undef' in the following lines as you wish,
 * but note that defining LIBTIFF_HAS_OLDJPEG_SUPPORT when such is _not_ the
 * case will result in crashes when encountering old-JPEG TIFFs:
 */

#define USE_LIBJPEG_FOR_TIFF_YCbCr_RGB_CONVERSION
#undef LIBTIFF_HAS_OLDJPEG_SUPPORT


/***************************************************************************
 * PhotoCD/MAG/PIC/MAKI/Pi/PIC2/HIPS format Support:
 *
 * if, for whatever reason--say, security concerns--you don't want to
 * include support for one or more of the PhotoCD, MAG/MAKI/Pi/PIC/PIC2
 * (Japanese), or HIPS (astronomical) image formats, change the relevant
 * 'define' to 'undef' in the following lines.  Conversely, if you *do*
 * want them, change 'undef' to 'define' as appropriate.
 */

#define HAVE_PCD	/* believed to be reasonably safe */

#undef HAVE_MAG		/* probable security issues */
#undef HAVE_MAKI	/* probable security issues */
#undef HAVE_PI		/* probable security issues */
#undef HAVE_PIC		/* probable security issues */
#undef HAVE_PIC2	/* probable security issues */

#undef HAVE_HIPS	/* probable security issues */


/***************************************************************************
 * MacBinary file support:
 *
 * if you want XV to be able to handle ``MacBinary'' files (which have
 * 128 byte info file header at the head), change 'undef' to 'define'
 * in the following line.
 */

#undef MACBINARY


/***************************************************************************
 * Auto Expand support:
 *
 * if you want to extract archived file automatically and regard it as
 * a directory, change 'undef' to 'define' in the AUTO_EXPAND line.
 *
 * Virtual Thumbdir support:
 *
 * if you want Virtual directory based Thumbdir(It means that XV
 * doesn't forget builded Icons still be quited even if the directory
 * is read-only), change 'undef' to 'define' the VIRTUAL_TD line.
 */

#undef AUTO_EXPAND
#undef VIRTUAL_TD

#if defined(VIRTUAL_TD) && !defined(AUTO_EXPAND)
#  undef VIRTUAL_TD
#endif


/***************************************************************************
 * Adjust the aspect ratio of Icons:
 *
 * if you want to adjust the aspect ratio of the icons in Visual
 * Schnauzer, change 'undef' to 'define' in the following line.
 */

#undef VS_ADJUST


/***************************************************************************
 * Restore original colormap:
 *
 * if you want to restore original colormap when icons in Visual
 * Shunauzer is double-clicked, change 'undef' to 'define' in the
 * following line.
 */

#undef VS_RESCMAP


/***************************************************************************
 * TextViewer l10n support:
 *
 * if you want XV to show the text in Japanese on TextViewer, change
 * 'undef' to 'define' in the following line.
 */

#undef TV_L10N

#ifdef TV_L10N
/*
 * if you want to change the default code-set used in case that XV
 * fails to select correct code-set, uncomment the '#define
 * LOCALE_DEFAULT' line and change the 'LOCALE_DEFAULT' definition
 * appropriately.
 * (0:ASCII, 1:EUC-j, 2:JIS, 3:MS Kanji) */

/* #  define LOCALE_DEFAULT 0 */

/*
 * Uncomment and edit the following lines, if your X Window System was
 * not compiled with -DX_LOCALE and you failed to display the Japanese
 * text in TextViewer.  You don't have to write locale name of JIS code-set
 * and Microsoft code-set, if your system doesn't support those code-sets.
 */

/*
#  define LOCALE_NAME_EUC     "ja_JP.EUC"
#  define LOCALE_NAME_JIS     "ja_JP.JIS"
#  define LOCALE_NAME_MSCODE  "ja_JP.SJIS"
*/

/*
 * if your system doesn't have the Japanese fonts in the sizes,
 * Uncomment and edit the following font size entries.
 */

/* #  define TV_FONTSIZE 14,16,24 */

/*
 * If you need, uncomment and modify the following font name.
 */

/* #  define TV_FONTSET "-*-fixed-medium-r-normal--%d-*" */
#endif /* TV_L10N */


/***************************************************************************
 * User definable filter support:
 *
 * Use the filters as input and output method for load and save unsupported
 * image format file. The filter command is recognized by definition of
 * magic number or suffix in "~/.xv_mgcsfx" .
 * To enable this feature, change 'undef' to 'define' in the following line.
 */
#undef HAVE_MGCSFX

#ifdef HAVE_MGCSFX
/*
 * Support symbol 'auto' as <input image type> in startup file. This type
 * cannot use pipe as input; it writes to a temporary file and recognizes
 * the actual filetype by XV processing.
 */
#  define HAVE_MGCSFX_AUTO

/*
 * The startup file of definition for MgcSfx. 'MGCSFX_SITE_RC' is read
 * first and '~/MGCSFX_RC' is second. So same definitions in both files
 * are overridden by '~/MGCSFX_RC'
 * To define startup file, see the sample of startup file 'xv_mgcsfx.sample'.
 */
#  define MGCSFX_SITE_RC  "xv_mgcsfx"
#  define MGCSFX_RC       ".xv_mgcsfx"

/*
 * If you want startup file to pass preprocessor in reading time, then
 * change 'undef' to 'define' in the following line.
 *
 * WARNING : If you decide to use preprocessor, you must not write
 *           '# <comment>' style comment in startup file. Because,
 *           preprocessor can't recognize.  */
#  undef USE_MGCSFX_PREPROCESSOR

#  ifdef USE_MGCSFX_PREPROCESSOR
/*
 * This is used like "system("MGCSFX_PREPROCESSOR MGCSFX_RC > tmp_name");",
 * and read tmp_name instead of MGCSFX_RC.
 */
#    define MGCSFX_PREPROCESSOR "/usr/lib/cpp"
/* #    define MGCSFX_PREPROCESSOR "cc -E" */

#  endif /* USE_MGCSFX_PREPROCESSOR */

/*
 * Default string of command. If input command is required for undefined file,
 * dialog is popuped with 'MGCSFX_DEFAULT_INPUT_COMMAND'. And, if output
 * command is required in save dialog of MgcSfx, dialog is popuped with
 * 'MGCSFX_DEFAULT_OUTPUT_COMMAND'.
 *
 * WARNING : Now, supported only 'PNM' image format, when command input is
 *           required. You should define filter which use 'PNM' image format
 *           as input or output.
 */
#  define MGCSFX_DEFAULT_INPUT_COMMAND  "tifftopnm"
#  define MGCSFX_DEFAULT_OUTPUT_COMMAND "pnmtotiff"

#endif /* HAVE_MGCSFX */


/***************************************************************************
 * Multi-Lingual TextViewer
 *
 * if you want XV to show the text in multi-lingual on TextViewer, change
 * 'undef' to 'define' in the following line.
 */

#undef TV_MULTILINGUAL

#define TV_DEFAULT_CODESET TV_EUC_JAPAN

#ifdef TV_MULTILINGUAL
#  undef TV_L10N
#endif
