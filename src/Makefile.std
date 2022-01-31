# Makefile for xv

# your C compiler (and options) of choice
CC = cc
#CC = gcc -ansi
# note that -ansi kills __USE_MISC (gcc 2.95.3), which, at least on Linux,
# determines whether stdlib.h includes prototypes for mktemp(), random(), etc.
# (i.e., if you use it, you will get unnecessary compiler warnings)
#CC = gcc

# use this if you're using 'cc' on a DEC Alpha (OSF/1) or MIPS (Ultrix) system:
# CC = cc -std1 -Olimit 750

# this is what I personally use on an OSF Alpha.  Not that I recommend it.
# CC = gcc -g -ansi -pedantic -W -Wreturn-type  -Wmissing-prototypes \
#       -Wstrict-prototypes -Waggregate-return -Wconversion \
#       -Wpointer-arith -Wcomment -Wformat -Wchar-subscripts \
#       -Wuninitialized -Wparentheses


CCOPTS = -O
#
# these are the usual optimization and warning options for gcc; all such
# warnings but one (mktemp() use) have been eliminated (at least on Linux):
#CCOPTS = -O3 -Wall
#
# slightly more warnings... older code often made non-const pointers to
# static strings (nothing should blow up unless something tries to write
# to them):
#CCOPTS = -O3 -Wall -Wpointer-arith -Wcast-align -Wwrite-strings -Wnested-externs
#
# for the next step up in gcc noise, try adding -W (but note that it adds a
# huge number of unused-parameter and signed/unsigned comparison warnings):
#CCOPTS = -O3 -Wall -W

### NOTE: Sun running OpenWindows:
### if you're using a SUN running OPENWINDOWS, you need to add these two
### options to the CCOPTS line, so it finds the libs and include files
###   -L/usr/openwin/lib -I/usr/openwin/include
###
### In general, if your X11 include files and libX11.a library aren't in the
### 'standard' places in which the C compiler looks, you should add '-L' and
### '-I' options on the CCOPTS line to tell the compiler where said files are.


# older Unixen don't support the -p option, but its lack may mean installation
# will fail (if more than one directory level is missing)
MKDIR = mkdir -p


# BeOS _may_ need to use a different version (below), but probably not
CLEANDIR = cleandir


### Installation locations
### NOTE: Users of old K&R compilers (i.e., any version not supporting C89
### string concatenation, such as "fub" "ar" => "fubar") should update
### xvtext.c:1831 (or thereabouts) if either PREFIX or DOCDIR changes:
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
MANSUF = 1
DOCDIR = $(PREFIX)/share/doc/xv
LIBDIR = $(PREFIX)/lib/xv
SYSCONFDIR = /etc
DESTDIR =


buildit: all


########################### CONFIGURATION OPTIONS ############################
### NOTE: be sure to check 'config.h', for a few other configuration options
##############################################################################

###
### if, for whatever reason, you're unable to get the TIFF library to compile
### on your machine, *COMMENT OUT* the following lines
###
### GRR 20050319:  USE_TILED_TIFF_BOTLEFT_FIX enables an experimental fix for
###   tiled TIFFs with ORIENTATION_BOTLEFT.  It may break other tiled TIFFs,
###   or it may be required for certain other TIFF types (e.g., strips with
###   ORIENTATION_BOTLEFT).  I don't have a sufficient variety of TIFF test
###   images at hand.
###
#TIFF    = -DDOTIFF
TIFF    = -DDOTIFF -DUSE_TILED_TIFF_BOTLEFT_FIX
###
#TIFFDIR = tiff
TIFFDIR = /usr
#TIFFDIR = /usr/local
#TIFFDIR = ../../libtiff
###
TIFFINC = -I$(TIFFDIR)/include
#TIFFINC = -I$(TIFFDIR)
###
### libtiff 3.5 and up may be compiled with zlib and libjpeg, but the
### dependency is properly handled in LIBS line ~247 lines below
###
TIFFLIB = -L$(TIFFDIR)/lib -ltiff
#TIFFLIB = $(TIFFDIR)/lib/libtiff.a
#TIFFLIB = -L$(TIFFDIR) -ltiff
#TIFFLIB = $(TIFFDIR)/libtiff.a
###
### this is intended to build the ancient version (3.3.016 beta) that's
### included in the "tiff" subdir of XV, not an arbitrary copy of libtiff:
###
#$(TIFFLIB):
#	( cd $(TIFFDIR) ; make CC='$(CC)' COPTS='$(CCOPTS) $(MCHN)' )


###
### if, for whatever reason, you're unable to get the JPEG library to compile
### on your machine, *COMMENT OUT* the following lines
###
### NOTE: /usr/sfw can be used on Solaris with "Sun freeware" installed
###
JPEG    = -DDOJPEG
#JPEGDIR = jpeg
JPEGDIR = /usr
#JPEGDIR = /usr/local
#JPEGDIR = ../../libjpeg
#JPEGDIR = /usr/sfw
###
JPEGINC = -I$(JPEGDIR)/include
#JPEGINC = -I$(JPEGDIR)
###
JPEGLIB = -L$(JPEGDIR)/lib -ljpeg
#JPEGLIB = -L$(JPEGDIR) -ljpeg
#JPEGLIB = $(JPEGDIR)/libjpeg.a
###
### this is intended to build the ancient version (5a) that's included in the
### "jpeg" subdir of XV, not an arbitrary copy of libjpeg:
###
#$(JPEGDIR)/jconfig.h:
#	cd $(JPEGDIR) ; ./configure CC='$(CC)'
#$(JPEGLIB):  $(JPEGDIR)/jconfig.h
#	cd $(JPEGDIR) ; make


###
### if, for whatever reason, you're unable to get the PNG library to compile
### on your machine, *COMMENT OUT* the following lines
###
PNG    = -DDOPNG
PNGDIR = /usr
#PNGDIR = /usr/local
#PNGDIR = ../../libpng
###
PNGINC = -I$(PNGDIR)/include
#PNGINC = -I$(PNGDIR)
###
PNGLIB = -L$(PNGDIR)/lib -lpng
#PNGLIB = -L$(PNGDIR) -lpng
#PNGLIB = $(PNGDIR)/libpng.a


###
### if, for whatever reason, you're unable to get both the PNG library and
### (newer versions of) the TIFF library to compile on your machine, *COMMENT
### OUT* the following lines
###
ZLIBDIR = /usr
#ZLIBDIR = /usr/local
#ZLIBDIR = ../../zlib
###
ZLIBINC = -I$(ZLIBDIR)/include
#ZLIBINC = -I$(ZLIBDIR)
###
ZLIBLIB = -L$(ZLIBDIR)/lib -lz
#ZLIBLIB = -L$(ZLIBDIR) -lz
#ZLIBLIB = $(ZLIBDIR)/libz.a


###
### if, for whatever reason, you're unable to get the JasPer JPEG-2000 library
### to compile on your machine, *COMMENT OUT* the following lines
###
JP2K    = -DDOJP2K
###
#JP2KDIR = ../../jasper
JP2KDIR = /usr/local/lib
###
#JP2KINC = -I$(JP2KDIR)
JP2KINC = -I/usr/local/include
###
#JP2KLIB = -L$(JP2KDIR) -ljasper
JP2KLIB = $(JP2KDIR)/libjasper.a


###
### if, for whatever reason, you're unable to get the PDS/VICAR support
### to compile (xvpds.c, and vdcomp.c), *COMMENT OUT* the following line,
### and also remove 'vdcomp' from the 'all:' dependency
###
PDS = -DDOPDS


#----------System V----------

# if you are running on a SysV-based machine, such as HP, Silicon Graphics,
# Solaris, etc.; uncomment one of the following lines to get you *most* of
# the way there.  SYSV means System V R3.
# UNIX = -DSVR4
# UNIX = -DSYSV


#----------Machine-Specific Configurations----------

### If you are using a BeOS system, uncomment the following line
#MCHN = -DUSE_GETCWD -I/usr/X11/include -L/usr/X11/lib
###
### The stock version of cleandir now should work for BeOS, too, so try
### leaving this commented out:
#CLEANDIR = cleandir.BeOS


### If you are using an SGI system, uncomment the following line
#MCHN = -Dsgi


### For HP-UX, uncomment the following line
#MCHN= -Dhpux -D_HPUX_SOURCE
# To use old HP compilers (HPUX 7.0 or so), you may need
#MCHN= -Dhpux -D_HPUX_SOURCE +Ns4000
#
# Also, if you're using HP's compiler, add '-Aa' to whichever of those
# two lines you're using, to turn on ANSI C mode.  Or so I'm told.
#
# Note:  You may need to add '-I/usr/include/X11R5' (or R6, or whatever)
# to whichever of those lines you used, as HP tends to store their X11
# include files in a non-standard place...
#
# And you probably have to add '-lV3' to the end of the LIBS def when
# using XV's AUTO_EXPAND option.


### for Linux, uncomment one of the following lines:
#MCHN = -DLINUX -L/usr/X11R6/lib
#MCHN = -DLINUX -L/usr/X11R6/lib64


# For SCO 1.1 (UNIX 3.2v2) machines, uncomment the following:
#MCHN = -Dsco -DPOSIX
#
# For ODT 2.0 (UNIX 3.2v4) machines, uncomment the following:
#MCHN= -Dsco -DPOSIX -DNO_RANDOM
#
# Also, you should add '-lc -lx' to the end of the LIBS def below
# -lx must be after -lc so you get the right directory routines.


# for UMAX V by Encore Computers uncomment the following line for
# the portable C compiler, system-specific definitions and
# location of local X11 library (if site-specific, modify -L option)
# No other switches should be necessary, or so I'm told...
#
#MCHN = -q extensions=pcc_c -D__UMAXV__ -L/usr2/usr/lib/X11 -DSVR4

# For Interactive/SunSoft Unix ISC 4.0  (whatever *that* is!)
#MCHN = -DSVR4 -DBSDTYPES


#----------'Roll Your Own' Options----------


# if your machine doesn't have 'random()', but does have 'rand()',
# uncomment the following line:
#
#RAND = -DNO_RANDOM


# if your system *doesn't* have /usr/include/dirent.h, (ie, isn't POSIX
# compliant, then you may have to uncomment the following line to use the
# 'old-style' directory-handling structures
#
#NODIRENT = -DNODIRENT


# if your machine has the usleep() function, uncomment the following line:
# if it doesn't, or you're not sure, leave this line alone.
#TIMERS = -DUSLEEP


# if XV locks up whenever you click on *any* of the buttons, the Timer()
# function in xvmisc.c is going out to lunch.  A simple workaround is to
# uncomment the following line:
#TIMERS = -DNOTIMER


# if you are running under DXWM, I pity you.  XV doesn't work correctly
# under DXWM.  You should probably be running MWM.  However, if such is
# not an option for you, try uncommenting the following line.  The
# behavior won't be 'right', but it will be less 'wrong'.
#DXWM = -DDXWM


# if, during compilation, your system complains about the types
# 'u_long', 'u_short', 'u_int', etc. as being undefined, uncomment the
# following line:
#BSDTYPES = -DBSDTYPES


# if your machine doesn't have 'vprintf()' or 'vsprintf()'
#          (see vprintf.c for more information, if needed)
#
#  (for BSD 4.3 VAX, uncomment the following line)
#VPRINTF = -DNEED_VPRINTF -DINTSPRINTF -DLONGINT -DNOVOID
#  (for (stock) IBM RT AOS 4.3, uncomment the following line)
#VPRINTF = -DNEED_VPRINTF -DLONGINT -DNOSTDHDRS
#  (for Sequent running DYNIX 3.1.4, uncomment the following line)
#VPRINTF = -DNEED_VPRINTF -DLONGINT -DNOSTDHDRS


# if your machine puts the prototype for 'malloc()' in malloc.h rather than
# stdlib.h, uncomment the following line:
#
#MALLOC = -DNEED_MALLOC_H


# if your X Window System compiled with -DX_LOCALE, 
# uncomment the following line:
# TVL10N = -DX_LOCALE

# Install directory of xv_mgcsfx.sample.
MGCSFXDIR = $(LIBDIR)
# Directory of default configuration file.
MGCSFX = -DMGCSFXDIR=\"$(MGCSFXDIR)\"




################ END OF CONFIGURATION OPTIONS #################




CFLAGS = $(CCOPTS) $(PNG) $(PNGINC) $(ZLIBINC) $(JPEG) $(JPEGINC) \
	$(TIFF) $(TIFFINC) $(PDS) $(JP2K) $(JP2KINC) $(TVL10N) $(MGCSFX) \
	$(UNIX) $(BSDTYPES) $(RAND) $(MALLOC) $(DXWM) $(MCHN) $(NODIRENT) \
	$(VPRINTF) $(TIMERS) -DDOCDIR=\"$(DOCDIR)\" \
	-DSYSCONFDIR=\"$(SYSCONFDIR)\" -DXVEXECPATH=\"$(LIBDIR)\"

### remove -lm for BeOS:
LIBS = $(TIFFLIB) $(JPEGLIB) $(PNGLIB) $(ZLIBLIB) $(JP2KLIB) -L/usr/X11R6/lib -lX11 -lm
#LIBS = $(TIFFLIB) $(JPEGLIB) $(PNGLIB) $(ZLIBLIB) $(JP2KLIB) -lX11

OBJS = 	xv.o xvevent.o xvroot.o xvmisc.o xvimage.o xvcolor.o xvsmooth.o \
	xv24to8.o xvgif.o xvpm.o xvinfo.o xvctrl.o xvscrl.o xvalg.o \
	xvgifwr.o xvdir.o xvbutt.o xvpbm.o xvxbm.o xvgam.o xvbmp.o \
	xvdial.o xvgraf.o xvsunras.o xvjpeg.o xvps.o xvpopup.o xvdflt.o \
	xvtiff.o xvtiffwr.o xvpds.o xvrle.o xviris.o xvgrab.o vprintf.o \
	xvbrowse.o xvtext.o xvpcx.o xviff.o xvtarga.o xvxpm.o xvcut.o \
	xvxwd.o xvfits.o xvpng.o xvzx.o xvwbmp.o xvpcd.o xvhips.o \
	xvmag.o xvpic.o xvmaki.o xvpi.o xvpic2.o xvvd.o xvmgcsfx.o \
	xvml.o xvjp2k.o

MISC = README INSTALL CHANGELOG IDEAS



.c.o:	; $(CC) $(CFLAGS) -c $*.c



#all: $(JPEGLIB) $(TIFFLIB) xv bggen vdcomp xcmap xvpictoppm
all: xv bggen vdcomp xcmap xvpictoppm


#xv: $(OBJS) $(JPEGLIB) $(TIFFLIB)
xv: $(OBJS)
	$(CC) -o xv $(CFLAGS) $(OBJS) $(LIBS)

bggen: bggen.c
	$(CC) $(CFLAGS) -o bggen bggen.c $(LIBS)

vdcomp: vdcomp.c
	$(CC) $(CFLAGS) -o vdcomp vdcomp.c

xcmap:  xcmap.c
	$(CC) $(CFLAGS) -o xcmap xcmap.c $(LIBS)

xvpictoppm:  xvpictoppm.c
	$(CC) $(CFLAGS) -o xvpictoppm xvpictoppm.c



xvclean:
	rm -f $(OBJS) xv

clean:  xvclean
	rm -f bggen vdcomp xcmap xvpictoppm
#	clean only local jpeg and tiff dirs, not user's or system's copies:
	./$(CLEANDIR) jpeg
	rm -f jpeg/jconfig.h jpeg/Makefile
	./$(CLEANDIR) tiff


# could also do some shell trickery here to attempt mkdir only if dir is
# missing (e.g., "test -d <dir> || $(MKDIR) <dir>")
install: all
	$(MKDIR) $(DESTDIR)$(BINDIR)
	cp xv bggen vdcomp xcmap xvpictoppm $(DESTDIR)$(BINDIR)/.
	chmod 755 $(DESTDIR)$(BINDIR)/xv $(DESTDIR)$(BINDIR)/bggen \
	  $(DESTDIR)$(BINDIR)/vdcomp $(DESTDIR)$(BINDIR)/xcmap \
	  $(DESTDIR)$(BINDIR)/xvpictoppm
#
	$(MKDIR) $(DESTDIR)$(MANDIR)
	cp docs/xv.man     $(DESTDIR)$(MANDIR)/xv.$(MANSUF)
	cp docs/bggen.man  $(DESTDIR)$(MANDIR)/bggen.$(MANSUF)
	cp docs/xcmap.man  $(DESTDIR)$(MANDIR)/xcmap.$(MANSUF)
	cp docs/xvp2p.man  $(DESTDIR)$(MANDIR)/xvpictoppm.$(MANSUF)
	cp docs/vdcomp.man $(DESTDIR)$(MANDIR)/vdcomp.$(MANSUF)
	chmod 644 $(DESTDIR)$(MANDIR)/xv.$(MANSUF) \
	  $(DESTDIR)$(MANDIR)/bggen.$(MANSUF) \
	  $(DESTDIR)$(MANDIR)/xcmap.$(MANSUF) \
	  $(DESTDIR)$(MANDIR)/xvpictoppm.$(MANSUF) \
	  $(DESTDIR)$(MANDIR)/vdcomp.$(MANSUF)
#
	$(MKDIR) $(DESTDIR)$(DOCDIR)		# or $(DESTDIR)$(LIBDIR)
	cp README.jumbo docs/xvdocs.pdf docs/xvdocs.ps $(DESTDIR)$(DOCDIR)/.
	chmod 644 $(DESTDIR)$(DOCDIR)/README.jumbo \
	  $(DESTDIR)$(DOCDIR)/xvdocs.pdf $(DESTDIR)$(DOCDIR)/xvdocs.ps
#
	#$(MKDIR) $(DESTDIR)$(SYSCONFDIR)
	#cp xv_mgcsfx.sample $(DESTDIR)$(SYSCONFDIR)/xv_mgcsfx
	#chmod 644 $(DESTDIR)$(SYSCONFDIR)/xv_mgcsfx


tar:
#	tar only local jpeg and tiff dirs, not user's or system's copies:
	tar cvf xv.tar Makefile* Imakefile *.c *.h bits \
		docs unsupt vms jpeg tiff $(MISC)

xvtar:
	tar cvf xv.tar Makefile* Imakefile *.c *.h bits

$(OBJS):   xv.h config.h


################# bitmap dependencies ####################

xv.o:		bits/icon bits/iconmask bits/runicon bits/runiconm
xv.o:		bits/cboard50 bits/gray25

xvbrowse.o:	bits/br_file bits/br_dir bits/br_exe bits/br_chr bits/br_blk
xvbrowse.o:	bits/br_sock bits/br_fifo bits/br_error # bits/br_unknown
xvbrowse.o:	bits/br_cmpres bits/br_bzip2 bits/br_gif bits/br_pm bits/br_pbm
xvbrowse.o:	bits/br_sunras bits/br_bmp bits/br_utah bits/br_iris
xvbrowse.o:	bits/br_pcx bits/br_jfif bits/br_tiff bits/br_pds bits/br_pcd
xvbrowse.o:	bits/br_ps bits/br_iff bits/br_targa bits/br_xpm bits/br_xwd
xvbrowse.o:	bits/br_fits bits/br_png bits/br_zx bits/br_mag bits/br_maki
xvbrowse.o:	bits/br_pic bits/br_pi bits/br_pic2 bits/br_mgcsfx
xvbrowse.o:	bits/br_jp2 bits/br_jpc
xvbrowse.o:	bits/br_trash bits/fcurs bits/fccurs bits/fdcurs bits/fcursm

xvbutt.o:	bits/cboard50 bits/rb_frame bits/rb_frame1 bits/rb_top
xvbutt.o:	bits/rb_bot bits/rb_dtop bits/rb_dbot bits/rb_body
xvbutt.o:	bits/rb_dot bits/cb_check bits/mb_chk

xvctrl.o:	bits/gray25 bits/gray50 bits/i_fifo bits/i_chr bits/i_dir
xvctrl.o:	bits/i_blk bits/i_lnk bits/i_sock bits/i_exe bits/i_reg
xvctrl.o:	bits/h_rotl bits/h_rotr bits/fliph bits/flipv bits/p10
xvctrl.o:	bits/m10 bits/cut bits/copy bits/paste bits/clear
xvctrl.o:	bits/uicon bits/oicon1 bits/oicon2 bits/icon
xvctrl.o:	bits/padimg bits/annot

xvcut.o:	bits/cut bits/cutm bits/copy bits/copym

xvdflt.o:	bits/logo_top bits/logo_bot bits/logo_out bits/xv_jhb
xvdflt.o:	bits/xv_cpyrt bits/xv_rev bits/xv_ver
xvdflt.o:	bits/xf_left bits/xf_right bits/font5x9.h
xvdflt.o:	xvdflt.h

xvdial.o:	bits/dial_cw1 bits/dial_ccw1 bits/dial_cw2 bits/dial_ccw2

xvdir.o:	bits/d_load bits/d_save

xvevent.o:	bits/dropper bits/dropperm bits/pen bits/penm
xvevent.o:	bits/blur bits/blurm

xvgam.o:	bits/h_rotl bits/h_rotr bits/h_flip bits/h_sinc bits/h_sdec
xvgam.o:	bits/h_sat bits/h_desat

xvgraf.o:	bits/gf1_addh bits/gf1_delh bits/gf1_line bits/gf1_spln
xvgraf.o:	bits/gf1_rst bits/gf1_gamma

xvinfo.o:	bits/penn bits/pennnet

xvmisc.o:	bits/fc_left bits/fc_leftm bits/fc_left1 bits/fc_left1m
xvmisc.o:	bits/fc_mid bits/fc_midm bits/fc_right1 bits/fc_right1m
xvmisc.o:	bits/fc_right bits/fc_rightm

xvpopup.o:	bits/icon

xvroot.o:	bits/root_weave

xvscrl.o:	bits/up bits/down bits/up1 bits/down1 bits/uph bits/downh
xvscrl.o:	bits/uph1 bits/downh1 bits/scrlgray

################# end bitmap dependencies ####################



