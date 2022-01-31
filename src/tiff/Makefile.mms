# MMS Description file for the Tag Image File Format Library
#
# Tag Image File Format Library
#
# Copyright (c) 1988, 1989, 1990, 1991, 1992 Sam Leffler
# Copyright (c) 1991, 1992 Silicon Graphics, Inc.
#
# Permission to use, copy, modify, distribute, and sell this software and
# its documentation for any purpose is hereby granted without fee, provided
# that (i) the above copyright notices and this permission notice appear in
# all copies of the software and related documentation, and (ii) the names of
# Sam Leffler and Silicon Graphics may not be used in any advertising or
# publicity relating to the software without the specific, prior written
# permission of Stanford and Silicon Graphics.
#
# THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
# EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
# WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
#
# IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
# ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
# OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
# WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
# LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
# OF THIS SOFTWARE.
#
# This MMS description file was written by Rick Dyson on or before 11-NOV-1991
#	(dyson@IowaSP.Physics.UIowa.EDU)
#	Updated: 28-APR-1992 -- LIBTIFF v3.0  (19-FEB-1992) for XV (v2.21)
#	          2-MAR-1993 -- LIBTIFF v3.21 (29-OCT-1992) for XV (v3.00)
#                14-JUL-1994 -- Added Alpha Support RLD
#
# For DEC C (or ALPHA) users, you must add a MACRO qualifier to the
# command line, i.e.,
#       MMS /Description = Makefile.mms /Macro = "ALPHA = 1"
#
# It is based on the Unix Makefile by Leffler
#

NULL =

IPATH =	[]
#
# Library-wide configuration defines:
#    MMAP_SUPPORT	add support for memory mapping read-only files
#    COLORIMETRY_SUPPORT add support for 6.0 colorimetry tags
#    JPEG_SUPPORT	add support for 6.0 JPEG tags & JPEG algorithms
#    YCBCR_SUPPORT	add support for 6.0 YCbCr tags
#    CMYK_SUPPORT	add support for 6.0 CMYK tags
#
# Compression configuration defines:
#    CCITT_SUPPORT	add support for CCITT Group 3 & 4 algorithms
#    PACKBITS_SUPPORT	add support for Macintosh PackBits algorithm
#    LZW_SUPPORT	add support for LZW algorithm
#    THUNDER_SUPPORT	add support for ThunderScan 4-bit RLE algorithm
#    NEXT_SUPPORT	add support for NeXT 2-bit RLE algorithm
#    JPEG_SUPPORT	add support for JPEG DCT algorithm
#
# Note that if you change the library-wide configuration, you'll
# need to manual force a full rebuild.  Changing the configuration
# of which compression algorithms are included in the library is
# automatically handled (i.e. tif_compress.o has a dependency on
# the Makefile).
#

.ifdef ALPHA
FLAGS =		/Warnings = NoInformationals /Standard = VAXC
DEFS = 		/Define = ("BSDTYPES=1", "USE_VARARGS=1", "USE_PROTOTYPES=0", \
		"USE_CONST",__STDC__)
.else
FLAGS =		
DEFS = 		/Define = ("BSDTYPES=1", "USE_VARARGS=1", "USE_PROTOTYPES=0", \
		"USE_CONST", __STDC__)
.endif

.ifdef DEBUG
OPTIMIZE = 	/NoOptimize
DBG = 		/Debug
.else
OPTIMIZE = 	/Optimize
DBG = 		/NoDebug
.endif

INCS =		/Include_Directory = ($(IPATH))
CFLAGS = 	$(CFLAGS) $(FLAGS) $(DEFS) $(INCS) $(OPTIMIZE) $(DBG)

INCLUDES =	tiff.h tiffio.h

SRCS =	tif_fax3.c \
	tif_fax4.c \
	tif_aux.c \
	tif_ccittrle.c \
	tif_close.c \
	tif_compress.c \
	tif_dir.c \
	tif_dirinfo.c \
	tif_dirread.c \
	tif_dirwrite.c \
	tif_dumpmode.c \
	tif_error.c \
	tif_getimage.c \
	tif_jpeg.c \
	tif_flush.c \
	tif_lzw.c \
        tif_next.c \
	tif_open.c \
	tif_packbits.c \
	tif_print.c \
	tif_read.c \
	tif_swab.c \
	tif_strip.c \
	tif_thunder.c \
	tif_tile.c \
	tif_version.c \
        tif_vms.c \
	tif_warning.c \
	tif_write.c

OBJS =	tif_fax3.obj \
	tif_fax4.obj \
	tif_aux.obj \
	tif_ccittrle.obj \
	tif_close.obj \
	tif_compress.obj \
	tif_dir.obj \
	tif_dirinfo.obj \
	tif_dirread.obj \
	tif_dirwrite.obj \
	tif_dumpmode.obj \
	tif_error.obj \
	tif_getimage.obj \
	tif_jpeg.obj \
	tif_flush.obj \
	tif_lzw.obj \
        tif_next.obj \
	tif_open.obj \
	tif_packbits.obj \
	tif_print.obj \
	tif_read.obj \
	tif_strip.obj \
	tif_swab.obj \
	tif_thunder.obj \
	tif_tile.obj \
	tif_version.obj \
        tif_vms.obj \
	tif_warning.obj \
	tif_write.obj

OBJLIST = tif_fax3.obj,tif_fax4.obj,tif_aux.obj,tif_ccittrle.obj,tif_close.obj,tif_compress.obj,tif_dir.obj,tif_dirinfo.obj,tif_dirread.obj,tif_dirwrite.obj,tif_dumpmode.obj,tif_error.obj,tif_getimage.obj,tif_jpeg.obj,tif_flush.obj,tif_lzw.obj,tif_next.obj,tif_open.obj,tif_packbits.obj,tif_print.obj,tif_read.obj,tif_strip.obj,tif_swab.obj,tif_thunder.obj,tif_tile.obj,tif_version.obj,tif_vms.obj,tif_warning.obj,tif_write.obj

TIFFLIB =	libtiff.olb

.first
.ifdef ALPHA
	@- Define /NoLog Sys DECC$Library_Include
.else
	@- Define /NoLog Sys Sys$Library
.endif

all :		lib
	@ continue

lib :		$(TIFFLIB)
	@ continue

libtiff.olb :		$(OBJS)
        If "''F$Search ("$(TIFFLIB)")'" .eqs. "" Then Library /Create $(TIFFLIB)
	Library /Replace $(TIFFLIB) $(OBJLIST)

$(OBJS) : 	tiffio.h tiff.h tiffcomp.h tiffiop.h tiffconf.h

tif_fax3.obj :	tif_fax3.c g3states.h t4.h tif_fax3.h

mkg3states.exe :	mkg3states.c t4.h
	- $(CC) $(CFLAGS) mkg3states.c
.ifdef ALPHA
.else
        - $ Define /User_Mode LNK$Library Sys$Library:VAXCRTL
.endif
	- $(LINK) $(LINKFLAGS) mkg3states

g3states.h :		mkg3states.exe
	Define /User_Mode Sys$Output g3states.h
	Run mkg3states.exe
#	Mcr Sys$Disk:[]mkg3states.exe > g3states.h
#   if you want to use the Unix-style redirection in mkg3states, you will
#   need to add the call to getredirection in mkg3states.  Since this is only
#   run once, I didn't think it was really necessary anymore.   RLD 24-FEB-1993
#[-]argproc.obj :	[-]argproc.c [-]includes.h
#	$(CC) $(CFLAGS) [-]argproc.c

clean :
	@- Set Protection = Owner:RWED *.*;-1,*.obj
	- Purge /NoLog /NoConfirm *.*
	- Delete /NoLog /NoConfirm *.obj;*,*.exe;*,*.olb;
	- Delete /NoLog /NoConfirm g3states.h;*
