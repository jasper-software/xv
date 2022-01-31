# Makefile for xv

# your C compiler (and options) of choice
CC = cc
# CC = gcc -ansi

# use this if you're using 'cc' on a DEC Alpha (OSF/1) or MIPS (Ultrix) system:
# CC = cc -std1 -Olimit 750

# this is what I personally use on an OSF Alpha.  Not that I recommend it.
# CC = gcc -g -ansi -pedantic -W -Wreturn-type  -Wmissing-prototypes \
#       -Wstrict-prototypes -Waggregate-return -Wconversion \
#       -Wpointer-arith -Wcomment -Wformat -Wchar-subscripts \
#       -Wuninitialized -Wparentheses


CCOPTS = -O 


### NOTE: Sun running OpenWindows:
### if you're using a SUN running OPENWINDOWS, you need to add these two
### options to the CCOPTS line, so it finds the libs and include files
###   -L/usr/openwin/lib -I/usr/openwin/include
###
### In general, if your X11 include files and libX11.a library aren't in the
### 'standard' places in which the C compiler looks, you should add '-L' and
### '-I' options on the CCOPTS line to tell the compiler where said files are.


### Installation locations
BINDIR = /usr/local/bin
MANDIR = /usr/local/man/man1
MANSUF = 1
LIBDIR = /usr/local/lib


buildit: all


########################### CONFIGURATION OPTIONS ############################
### NOTE: be sure to check 'config.h', for a few other configuration options 
##############################################################################

###
### if, for whatever reason, you're unable to get the JPEG library to compile
### on your machine, *COMMENT OUT* the following lines
###
JPEG    = -DDOJPEG
JPEGDIR = jpeg
JPEGINC = -I$(JPEGDIR)
JPEGLIB = $(JPEGDIR)/libjpeg.a
$(JPEGDIR)/jconfig.h:
	cd $(JPEGDIR) ; ./configure CC='$(CC)'
$(JPEGLIB):  $(JPEGDIR)/jconfig.h
	cd $(JPEGDIR) ; make


###
### if, for whatever reason, you're unable to get the TIFF library to compile
### on your machine, *COMMENT OUT* the following lines
###
TIFF    = -DDOTIFF
TIFFDIR = tiff
TIFFINC = -I$(TIFFDIR)
TIFFLIB = $(TIFFDIR)/libtiff.a
$(TIFFLIB):
	( cd $(TIFFDIR) ; make CC='$(CC)' )


###
### if, for whatever reason, you're unable to get the PDS/VICAR support
### to compile (xvpds.c, and vdcomp.c), *COMMENT OUT* the following line,
### and also remove 'vdcomp' from the 'all:' dependancy 
###
PDS = -DDOPDS


#----------System V----------

# if you are running on a SysV-based machine, such as HP, Silicon Graphics,
# Solaris, etc., uncomment the following line to get mostly there.  
#UNIX = -DSVR4


#----------Machine Specific Configurations----------

### If you are using an SGI system, uncomment the following line
#MCHN = -Dsgi


### For HP-UX, uncomment the following line:
#MCHN= -Dhpux -D_HPUX_SOURCE
# To use old HP compilers (HPUX 7.0 or so), you may need
#MCHN= -Dhpux -D_HPUX_SOURCE +Ns4000
#
# also, if you're using HP's compiler, add '-Aa' to whichever of those
# two lines you're using, to turn on ANSI C mode.  Or so I'm told.
#
# note:  You may need to add '-I/usr/include/X11R5' (or R6, or whatever)
# to whichever of those lines you used, as HP tends to store their X11
# include files in a non-standard place...


### for LINUX, uncomment the following line
#MCHN = -DLINUX


# For SCO 1.1 (UNIX 3.2v2) machines, uncomment the following:
#MCHN = -Dsco -DPOSIX
#
# For ODT 2.0 (UNIX 3.2v4) machines, uncomment the following:
#MCHN= -Dsco -DPOSIX -DNO_RANDOM 
#
# Also, you should add '-lc -lx' to the end of the LIBS def below
# -lx must be after -lc so you get the right directory routines.


# for UMAX V by Encore Computers uncomment the following line for
# the portable c compiler, system specific definitions and
# location of local X11 library(if site specific, modify -L option)
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




################ END OF CONFIGURATION OPTIONS #################




CFLAGS = $(CCOPTS) $(JPEG) $(JPEGINC) $(TIFF) $(TIFFINC) $(PDS) \
	$(NODIRENT) $(VPRINTF) $(TIMERS) $(UNIX) $(BSDTYPES) $(RAND) \
	$(DXWM) $(MCHN)

LIBS = -lX11 $(JPEGLIB) $(TIFFLIB) -lm

OBJS = 	xv.o xvevent.o xvroot.o xvmisc.o xvimage.o xvcolor.o xvsmooth.o \
	xv24to8.o xvgif.o xvpm.o xvinfo.o xvctrl.o xvscrl.o xvalg.o \
	xvgifwr.o xvdir.o xvbutt.o xvpbm.o xvxbm.o xvgam.o xvbmp.o \
	xvdial.o xvgraf.o xvsunras.o xvjpeg.o xvps.o xvpopup.o xvdflt.o \
	xvtiff.o xvtiffwr.o xvpds.o xvrle.o xviris.o xvgrab.o vprintf.o \
	xvbrowse.o xvtext.o xvpcx.o xviff.o xvtarga.o xvxpm.o xvcut.o \
	xvxwd.o xvfits.o

MISC = README INSTALL CHANGELOG IDEAS



.c.o:	; $(CC) $(CFLAGS) -c $*.c



all: $(JPEGLIB) $(TIFFLIB) xv bggen vdcomp xcmap xvpictoppm


xv: $(OBJS) $(JPEGLIB) $(TIFFLIB)
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
	./cleandir $(JPEGDIR)
	rm -f $(JPEGDIR)/jconfig.h $(JPEGDIR)/Makefile
	./cleandir $(TIFFDIR)


install: all
	cp xv bggen vdcomp xcmap xvpictoppm $(BINDIR)
	cp docs/xv.man     $(MANDIR)/xv.$(MANSUF)
	cp docs/bggen.man  $(MANDIR)/bggen.$(MANSUF)
	cp docs/xcmap.man  $(MANDIR)/xcmap.$(MANSUF)
	cp docs/xvp2p.man  $(MANDIR)/xvpictoppm.$(MANSUF)
	cp docs/vdcomp.man $(MANDIR)/vdcomp.$(MANSUF)
	cp docs/xvdocs.ps* $(LIBDIR)

tar:
	tar cvf xv.tar Makefile* Imakefile *.c *.h bits \
		docs unsupt vms $(JPEGDIR) $(TIFFDIR) $(MISC) 

xvtar:
	tar cvf xv.tar Makefile* Imakefile *.c *.h bits

$(OBJS):   xv.h config.h


################# bitmap dependencies ####################

xv.o:      	bits/icon bits/iconmask bits/runicon bits/runiconm
xv.o:      	bits/cboard50 bits/gray25 

xvbrowse.o:	bits/br_file bits/br_dir bits/br_exe bits/br_chr bits/br_blk
xvbrowse.o:	bits/br_sock bits/br_fifo bits/br_error bits/br_unknown
xvbrowse.o:	bits/br_cmpres bits/br_gif bits/br_pm bits/br_pbm
xvbrowse.o:	bits/br_sunras bits/br_bmp bits/br_utah bits/br_iris
xvbrowse.o:	bits/br_pcx bits/br_jfif bits/br_tiff bits/br_pds 
xvbrowse.o:	bits/br_ps bits/br_iff bits/br_targa bits/br_xpm
xvbrowse.o:	bits/br_trash bits/fcurs bits/fccurs bits/fdcurs bits/fcursm
xvbrowse.o:     bits/br_xwd

xvbutt.o:	bits/cboard50 bits/rb_frame bits/rb_frame1 bits/rb_top
xvbutt.o:	bits/rb_bot bits/rb_dtop bits/rb_dbot bits/rb_body
xvbutt.o:	bits/rb_dot bits/cb_check bits/mb_chk

xvctrl.o:	bits/gray25 bits/gray50 bits/i_fifo bits/i_chr bits/i_dir
xvctrl.o: 	bits/i_blk bits/i_lnk bits/i_sock bits/i_exe bits/i_reg
xvctrl.o:	bits/h_rotl bits/h_rotr bits/fliph bits/flipv bits/p10
xvctrl.o:	bits/m10 bits/cut bits/copy bits/paste bits/clear 
xvctrl.o:	bits/uicon bits/oicon1 bits/oicon2 bits/icon
xvctrl.o:	bits/padimg bits/annot

xvcut.o:	bits/cut bits/cutm bits/copy bits/copym

xvdflt.o:	bits/logo_top bits/logo_bot bits/logo_out bits/xv_jhb
xvdflt.o:	bits/xv_cpyrt bits/xv_rev bits/xv_ver
xvdflt.o:	bits/xf_left bits/xf_right bits/font5x9.h
xvdflt.o:       xvdflt.h

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



