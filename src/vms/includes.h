/*
 * @(#)includes.h 1.8 89/04/01		Jamie Hanrahan (simpact!jeh)
 *
 * Version simpact-1.8, for DECUS uucp (VMS portion).  
 * All changes and additions from previous versions (see below) are in
 * the public domain. 
 *
 * Derived from:
 * 
 * includes.h 1.7 87/09/29	Copyright 1987 Free Software Foundation, Inc.
 *
 * Copying and use of this program are controlled by the terms of the
 * GNU Emacs General Public License.
 *
 * Include files for various supported systems:
 * Note that NAMESIZE should be the max length of a file name, including
 * all its directories, drive specifiers, extensions, and the like.
 * E.g. on a Unix with 14-char file names, NAMESIZE is several hundred
 * characters, since the 14-char names can be nested.
 */

#include <atrdef>
#include <ctype>
#include <descrip>
#include <devdef>
#include <dvidef>
#include <errno>
#include <fibdef>
#include <file>
#include <iodef>
#include <math>
#include <setjmp>
#include <signal>
#include <ssdef>
#include <stat>
/*      because I have consolidated ARGPROC.C into VMS.C (for XV) this
        call is reduntant and causes a compiler warning         RLD  24-FEB-1993
#include <stdlib>
*/
#include <stdio>
#include <string>
#include <time>

#define NAMESIZE 255
#define UUXQT_DOORBELL "UUCP_UUXQT_DOORBELL"
#define UUCICO_REQMB "UUCP_REQUESTS"
#define	UUX_QUEUE "UUCP_BATCH_QUEUE"
#define	UUX_FILE "UUCP_BIN:UUXQT_BATCH.COM"
#define DEBUG_LOG_FILE "vmsnet_log:uucico_dbg"
#define	UUX_LOG "UUCP_LOG:UUXQT.LOG"
#define	SYSLOCK_TEMPLATE "UUCP_SYS_%s"
#define STATUS_LNT "LNM$SYSTEM_TABLE"
#define STATUS_TEMPLATE "UUCP_STATUS_%s"
#define MAXLOCK 32
#define LOGLEN 255
#define SEQSIZE 4
#define CONTROL_FILE         "uucp_cfg:control." 
#define	LOGCLOSE	/* Logfile must be closed; VMS locks it when open */
#define EXEDIR  "uucp_bin:"	/* uuxqt executables live here (not used) */
#define NULL_DEVICE "NL:"
#define fork vfork		/* (not used) */
#define STATUS int		/* (not used) */
#define postmaster "UUCP_POSTMASTER"
#define EXIT_OK 1		/* image exit code */
#define EXIT_ERR 0x10000000	/* image exit code */
#define ENABLE 1		/* for $SETAST (and maybe others) */
#define DISABLE 0
#define	time_t	unsigned	/* (not used) */
#define remove delete	/* Remove a file */
#define qsort pqsort	/* Our own version (not used) */

#define FOPEN_W_MODE "w"	/* mode to open files being received */
#define FOPEN_R_MODE "r"	/*  or sent */

#define SS_FAILED(status) (((status)&1) == 0)
#define initdsc(d) d.dsc$b_class = DSC$K_CLASS_S, d.dsc$b_dtype = DSC$K_DTYPE_T
#define fillindsc(d, s) d.dsc$w_length=strlen(s), d.dsc$a_pointer=(s)
#define init_itmlst3(e,i,l,c,a,r) \
	(e[i].len=(l),\
	e[i].code=(c),\
	e[i].address=(a),\
	e[i].retlen=(r))

