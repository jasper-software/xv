$! A VMS Command procedure to make the foreign symbol definitions for
$! XV programs.  Keep this command proc in the same directory where the
$! binaries are kept and it will get the pointers setup correctly.
$!
$!  Written by Rick Dyson (dyson@IowaSP.Physics.UIowa.EDU) 19-AUG-1994
$!
$ Write Sys$Output "SETting UP XV (v 3.10)..."
$ THIS_PATH = F$Element (0, "]", F$Environment ("PROCEDURE")) + "]"
$ BgGen         :== $ 'THIS_PATH'BgGen.exe
$ Decompress    :== $ 'THIS_PATH'Decompress.exe
$ UnCompress    :== @ 'THIS_PATH'UnCompress.com
$ VDComp        :== $ 'THIS_PATH'VDComp.exe
$ XCMap         :== $ 'THIS_PATH'XCMap.exe
$ XV            :== $ 'THIS_PATH'XV.exe
$!
$!  Put the help library into the next available help library slot
$!
$ LIB = "Hlp$Library"
$ X = F$Trnlnm (LIB, "Lnm$Process")
$ If X .eqs. "" Then GoTo INSERT
$ If X .eqs. "''THIS_PATH'XV.HLB" Then GoTo EXIT
$ BASE = LIB + "_"
$ N = 1
$NEXTLIB:
$   LIB := 'BASE''N'
$   X = F$Trnlnm (LIB, "Lnm$Process")
$   If X .eqs. "" Then GoTo INSERT
$   If X .eqs. "''THIS_PATH'XV.HLB" Then GoTo EXIT
$   N = N + 1
$   GoTo NEXTLIB
$INSERT:
$   Define 'LIB' 'THIS_PATH'XV.HLB
$EXIT:
$   Exit
