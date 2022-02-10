$ If F$Mode () .eqs. "INTERACTIVE"
$   Then
$       VERIFY = F$Verify (0)
$   Else
$       VERIFY = F$Verify (1)
$ EndIf
$ On Control_Y Then GoTo EXIT
$ On Error     Then GoTo EXIT
$!========================================================================
$!
$!  Name      : MAKE_XV.COM
$!
$!  Purpose   : Compile and Link XV (v3.00) under VMS
$!  Suggested usage: @ MAKE_XV.COM
$!                OR
$!                   Submit /NoPrint /Log = Sys$Disk:[] /Notify MAKE_XV.COM
$!
$!  Created:  9-JAN-1992   by David Jones (jonesd@kcgl1.eng.ohio-state.edu)
$!  Updated: 19-JAN-1992   by Rick Dyson  (dyson@iowasp.physics.uiowa.edu)
$!  Updated:  9-MAR-1992   by Rick Dyson  for xv v2.11
$!  Updated: 28-APR-1992   by Rick Dyson  for xv v2.20a
$!  Updated: 30-APR-1992   by Rick Dyson  for xv v2.21
$!  Updated: 12-FEB-1993   by Rick Dyson  for xv v2.21b and VMS ALPHA support
$!                                        ALPHA support from Clark B. Merrill
$!                                                   (merrill@stsci.edu)
$!  Updated: 24-FEB-1993   by Rick Dyson  for xv v2.45
$!  Updated: 24-MAR-1993   by Rick Dyson  for xv v3.00
$!  Updated: 19-AUG-1994   by Rick Dyson  for xv v3.09beta
$!  Updated:  6-Dec-1994   by Rick Dyson  for xv v3.10
$!
$!========================================================================
$ THIS_PATH = F$Element (0, "]", F$Environment ("PROCEDURE")) + "]"
$ Set Default 'THIS_PATH'
$ If F$Trnlnm ("X11") .eqs. "" Then Define X11 DECW$Include
$! Test for ALPHA or VAX
$ If F$GetSyi ("HW_MODEL") .gt. 1023 
$   Then        ! it's an ALPHA
$       Define /NoLog Sys DECC$Library_Include
$       QUALIFIERS = "/NoDebug /Optimize /Standard = VAXC"
$       OPTIONS = "DECC_"
$   Else        ! it's a VAX
$               ! check for DEC C  if DEC C use the VAX C option
$       If F$Trnlnm ("DECC$Library_Include") .nes. ""
$           Then
$               QUALIFIERS = "/Standard = VAXC /NoDebug /Optimize"
$               OPTIONS = "DECC_"
$           Else
$               QUALIFIERS = "/NoDebug /Optimize"
$               OPTIONS = "VAXC_"
$       Endif
$       Define /NoLog Sys Sys$Share
$   EndIf
$!
$!USER CUSTOMIZING POINT!!!!!
$!
$!
$! for MOTIF users:
$ DEFS = "/Define = (VMS, HAVE_JPEG, HAVE_PDS, HAVE_TIFF)"
$!
$! for XUI users:
$! DEFS = "/Define = (VMS, HAVE_JPEG, HAVE_PDS, HAVE_TIFF, HAVE_XUI)"
$ INCS = "/Include = ([],[.JPEG],[.TIFF])"
$!
$ CC := CC 'QUALIFIERS' 'DEFS' 'INCS' /NoList
$!
$ sources = "xvevent,xvroot,xvmisc,xvimage,xvcolor,xvsmooth,xv24to8,"     + -
            "xvgif,xvpm,xvinfo,xvctrl,xvscrl,xvalg,xvgifwr,xvdir,xvbutt," + -
            "xvpbm,xvxbm,xvgam,xvbmp,xvdial,xvgraf,xvsunras,xvjpeg,xvps," + -
            "xvpopup,xvdflt,xvtiff,xvtiffwr,xvpds,xvrle,xviris,xvgrab,"   + -
            "xvbrowse,xvtext,xvpcx,xvtarga,xvcut,xviff,xvxwd,xvxpm,xvfits,vms"
$!
$ If F$Search ("[.bits]annot.h") .eqs. "" Then GoSub RENAME_BITMAPS
$ new_objects = ""
$ If F$Search ("[.jpeg]libjpeg.olb") .eqs. ""
$   Then
$       Set Default [.jpeg]
$       Write Sys$Output "Building JPEG library..."
$       @ [-]MAKE_JPEG.COM
$       Set Default [-]
$       new_objects = ",[.JPEG]LIBJPEG.OLB/Library"
$ EndIf
$ If F$Search ("[.tiff]libtiff.olb") .eqs. ""
$   Then
$       Set Default [.tiff]
$       Write Sys$Output "Building TIFF library..."
$       @ [-]MAKE_TIFF.COM
$       Set Default [-]
$       new_objects = ",[.TIFF]LIBTIFF.OLB/Library"
$ EndIf
$!
$   Write Sys$Output "Making object library ..."
$      If F$Search ("LIBXV.OLB") .eqs. "" Then Library /Create LIBXV.OLB
$!
$!  search for missing object files.
$!
$ sndx = 0
$NEXT_SOURCE:
$   sfile = F$Element (sndx, ",", sources)
$   sndx = sndx + 1
$   If sfile .eqs. "," Then GoTo SOURCES_DONE
$   ofile = F$Parse (".OBJ", sfile)
$   If F$Search (ofile) .nes. "" Then GoTo NEXT_SOURCE
$   Write Sys$Output "Compiling ", sfile, ".c ..."
$   CC 'sfile'.c
$   If F$Search (ofile) .nes. "" Then new_objects = new_objects + "," + sfile
$   Library /Replace LIBXV.OLB 'sfile'.obj
$   GoTo NEXT_SOURCE
$!
$SOURCES_DONE:
$   If new_objects .eqs. "" .and. p1 .eqs. "" Then GoTo EXIT
$   new_objects = new_objects - ","
$!
$   Write Sys$Output "Building xvpictoppm.c ..."
$   CC xvpictoppm.c
$   Link /NoMap /Executable = xvpictoppm.exe xvpictoppm,'OPTIONS'Options/Option
$   Write Sys$Output "Building decompress.c ..."
$   CC decompress.c
$   Link /NoMap /Executable = decompress.exe decompress,'OPTIONS'Options/Option
$   Write Sys$Output "Building vdcomp.c ..."
$   CC vdcomp.c
$   Link /NoMap /Executable = vdcomp.exe vdcomp,'OPTIONS'Options/Option
$   Write Sys$Output "Building bggen.c ..."
$   CC bggen.c
$   Link /NoMap /Executable = bggen.exe bggen,'OPTIONS'Options/Option
$   Write Sys$Output "Building xcmap.c ..."
$   CC xcmap.c
$   Write Sys$Output "Linking new XCMap image..."
$   Link /NoMap /Executable = xcmap.exe xcmap,'OPTIONS'Options/Option 'p1'
$   Write Sys$Output "Building xv.c ..."
$   CC xv.c
$   Write Sys$Output "Linking new XV image..."
$   Link /NoMap /Executable = xv.exe xv,'OPTIONS'Options/Option 'p1'
$   If "''F$Search ("xv.hlb")'" .eqs. "" Then Library /Create /Help xv.hlb
$   Library /Replace /Help xv.hlb xv.hlp
$   GoTo Exit
$!
$! subroutine to generate new bitmaps.h file.
$!
$RENAME_BITMAPS:
$!
$   Set Default [.Bits]
$   Set Protection = Owner:RWED *.*;*
$   Rename *. *.h
$   Set Protection = Owner:RWE *.*;*
$   Set Default [-]
$   Return
$!
$EXIT:
$   VERIFY = F$Verify (VERIFY)
$   Exit
