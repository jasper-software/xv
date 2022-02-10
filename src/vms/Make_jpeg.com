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
$!  Name      : MAKE_JPEG.COM
$!
$!  Purpose   : Compile and build JPEG library (v3) under VMS
$!  Suggested usage: called by @ [-]MAKE_XV.COM
$!
$!  Created:  9-JAN-1992   by David Jones (jonesd@kcgl1.eng.ohio-state.edu)
$!  Updated: 19-JAN-1992   by Rick Dyson  (dyson@iowasp.physics.uiowa.edu)
$!            9-MAR-1992   by Rick Dyson  for XV v2.11
$!           20-MAR-1992   by Rick Dyson  for JPEG v3
$!           28-APR-1992   by Rick Dyson  for XV v2.2a
$!            6-MAY-1992   by Rick Dyson  for XV v2.21 
$!            9-FEB-1993   by Rick Dyson  for XV v2.21b and ALPHA Support
$!                                        (Thanks to merrill@stsci.edu)
$!            2-MAR-1993   by Rick Dyson  for XV v3.00
$!           19-AUG-1994   by Rick Dyson  for XV v3.10 & JPEG v5
$!
$!========================================================================
$ THIS_PATH = F$Element (0, "]", F$Environment ("PROCEDURE")) + "]"
$ If F$Trnlnm ("SYS") .eqs. "" Then Define Sys Sys$Share
$! Test for ALPHA or VAX
$ If F$GetSyi ("HW_MODEL") .gt. 1023
$   Then        ! it's an ALPHA
$       QUALIFIERS = "/Warnings = NoInformationals /Standard = VAXC /NoDebug"
$       Define /NoLog Sys DECC$Library_Include
$   Else        ! it's a VAX
$               ! check for DEC C  if DEC C use the VAX C option
$       If F$Trnlnm ("DECC$Library_Include") .nes. ""
$           Then
$               QUALIFIERS = "/Standard = VAXC /NoDebug /Optimize"
$           Else
$               QUALIFIERS = "/NoDebug /Optimize"
$       Endif
$       Define /NoLog Sys Sys$Share
$   EndIf
$!
$ CC := CC 'QUALIFIERS' /NoList
$ Copy jconfig.vms jconfig.h
$ Purge /NoLog jconfig.h
$!
$ sources = "jcapi,jcparam,jdatadst,jcmaster,jcmarker,jcmainct,jcprepct," + -
            "jccoefct,jccolor,jcsample,jchuff,jcdctmgr,jfdctfst,jfdctflt," + -
            "jfdctint,jdapi,jdatasrc,jdmaster,jdmarker,jdmainct,jdcoefct," + -
            "jdpostct,jddctmgr,jidctfst,jidctflt,jidctint,jidctred,jdhuff," + -
            "jdsample,jdcolor,jquant1,jquant2,jdmerge,jcomapi,jutils," + -
            "jerror,jmemmgr,jmemnobs"
$!  search for missing object files.
$!
$ sndx = 0
$ new_objects = ""
$NEXT_SOURCE:
$   sfile = F$Element (sndx, ",", sources)
$   sndx = sndx + 1
$   If sfile .eqs. "," Then GoTo SOURCES_DONE
$   ofile = F$Parse (".OBJ", sfile)
$   If F$Search (ofile) .nes. "" Then GoTo NEXT_SOURCE
$   Write Sys$Output "Compiling ", sfile, ".c ..."
$   CC 'sfile'.c
$   If F$Search (ofile) .nes. "" Then new_objects = new_objects + "," + sfile
$   GoTo NEXT_SOURCE
$!
$SOURCES_DONE:
$   If new_objects .eqs. "" .and. p1 .eqs. "" Then GoTo FINISHED
$   new_objects = new_objects - ","
$!
$   Write Sys$Output "Building LIBJPEG.OLB..."
$   If "''F$Search ("LIBJPEG.OLB")'" .eqs. "" Then Library /Create LIBJPEG.OLB
$   Library /Replace LIBJPEG.OLB *.OBJ
$   GoTo Exit
$FINISHED:
$   Write Sys$Output "Finished building LIBJPEG.OLB..."
$EXIT:
$   VERIFY = F$Verify (VERIFY)
$   Exit
