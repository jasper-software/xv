/* Some unix emulation procedures to make XV happy */
/* 1-NOV-1990 GJC@MITECH.COM */

#include <stdio.h>

#ifdef __ALPHA
#include <unixio.h>
#endif

#include <string.h>
#include <descrip.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <stat.h>


int xv_bcmp ( s1, s2, size )
    char *s1, *s2;
    int size;
{
    int i;
    for ( i = 0; i < size; i++ ) if ( *s1++ != *s2++ ) {
	if ( *(--s1) > *(--s2) ) return 1; else return -1;
    }
    return 0;
}

bcopy(x,y,n)
     char *x,*y;
     long n;
{memmove(y,x,n);}	/* reverse order of arguments */

static char *getwd_tmp = NULL;

char *getwd(p)
     char *p;
{int c;
 char *root_dir,*l2;
 getcwd(p,512,0);	/* get current working directory in unix format*/

 root_dir = strstr ( p, "/000000" );
 if ( root_dir != NULL ) {
    /* trim root directory out of specification */
    if ( (strlen(root_dir) == 7) && 
	 (strpbrk(p+1,"/") == root_dir) ) *root_dir = '\0';
 }
 /* special kludge for "/" directory */
 if ( strcmp ( p, "/XV_Root_Device" ) == 0 ) strcpy  ( p, "/" );
 return(p);
}

unlink(p)
     char *p;
{printf("unlink: '%s'\n",p); delete(p);}

rmdir(p)
     char *p;
{printf("rmdir: Sorry, not available on VMS '%s' is unchanged!\n",p);}

rindex(p,c)
     char *p;
     int c;
{return(strrchr(p,c));}

int lstat(f,st)		/* fake a stat operation to return file type */
   char *f;
   stat_t *st;
{
    char *dirext, *name;
    int extlen;

    st->st_mode = S_IFREG;	/* default to normal file */
    name = strrchr ( f, '/' );	/* locate rightmost slash */
    if ( name == NULL ) name = f; else name++;

    dirext = strstr ( name, ".DIR" );
    if ( dirext != NULL ) {
	/* make it an exact match */
	extlen = strcspn(&dirext[1],".;");
        if ( (extlen == 0) || (extlen == 3) ) {
	    st->st_mode = S_IFDIR;
	    if ( strncmp ( name, "000000.", 7 ) == 0 ) return 0;
	    else return (stat ( f, st ));
	}
    }
    return 0;
}

do_vms_wildcard(pargc,pargv)
     int *pargc;
     char ***pargv;
{int j,vsize;
 int argc; char **argv;
 argc = *pargc;
 argv = *pargv;
 *pargc = 0;
 vsize = 3;
 *pargv = (char **) malloc(sizeof (char *) * vsize);
 for(j=0;j<argc;++j)
   vms_wild_putargs(argv[j],pargc,pargv,&vsize);}

vms_wild_putargs(s,pargc,pargv,pvsize)
     char *s; int *pargc; char ***pargv; int *pvsize;
{if ( (!strchr(s,'*')) && (!strchr(s,'%')) )
   vms_wild_put_one(s,pargc,pargv,pvsize);
 else
   vms_wild_put_wild(s,pargc,pargv,pvsize);}


vms_wild_put_one(s,pargc,pargv,pvsize)
     char *s; int *pargc; char ***pargv; int *pvsize;
{int nvsize,i;
#ifdef __ALPHA
# define SHELL$TRANSLATE_VMS   decc$translate_vms 
#else
 char *SHELL$TRANSLATE_VMS();
#endif
 char ** nargv, *uname;
 if (*pargc == *pvsize)
   {nvsize = 2 * *pvsize;
    nargv = (char **) malloc(sizeof (char *) * nvsize);
    for(i=0;i < *pargc; ++i) nargv[i] = (*pargv)[i];
    free(*pargv);
    *pargv = nargv;
    *pvsize = nvsize;}
 if ( uname = SHELL$TRANSLATE_VMS ( s ) ) {
    /* printf("vms: '%s' -> unix: '%s'\n", s, uname ); */
    if ( strlen(s) >= strlen(uname) ) { strcpy(s,uname); free(uname); }
    else s = uname;  /* will lose s's old allocation */
 } 
 (*pargv)[(*pargc)++] = s;}


set_dsc(x,buff,len)
 struct dsc$descriptor *x;
 char *buff;
 int len;
{(*x).dsc$w_length = len;
 (*x).dsc$a_pointer = buff;
 (*x).dsc$b_class = DSC$K_CLASS_S;
 (*x).dsc$b_dtype = DSC$K_DTYPE_T;
 return(x);}

 struct dsc$descriptor *
set_dsc_cst(x,buff)
 struct dsc$descriptor *x;
 char *buff;
{return(set_dsc(x,buff,strlen(buff)));}


vms_wild_put_wild(s,pargc,pargv,pvsize)
     char *s; int *pargc; char ***pargv; int *pvsize;
{struct dsc$descriptor fnamed,foutd,rfnamed;
 char *ns,*p;
 int rval;
 long context;
 set_dsc_cst(&rfnamed,";");
 set_dsc_cst(&fnamed,s);
 set_dsc(&foutd,0,0);
 foutd.dsc$b_class = DSC$K_CLASS_D;
 context = 0;
 while(1)
  {rval = lib$find_file(&fnamed,&foutd,&context,0,&rfnamed,0,0);
   if (rval == RMS$_NMF) break;
   if (rval == RMS$_FNF) break;
   if (rval != RMS$_NORMAL) exit(rval);
   ns = (char *) malloc(foutd.dsc$w_length + 1);
   ns[foutd.dsc$w_length] = 0;
   memcpy(ns,foutd.dsc$a_pointer,foutd.dsc$w_length);
   /*if (p = strchr(ns,']')) ns = p+1;*/
   /* if (p = strchr(ns,';')) *p = 0; */
   vms_wild_put_one(ns,pargc,pargv,pvsize);}
 if (foutd.dsc$a_pointer) lib$sfree1_dd(&foutd);
 if (context)
   {rval = lib$find_file_end(&context);
    if (rval != SS$_NORMAL) exit(rval);}}

/*
 * Define substitue qsort for one that dec broke.  Only handle case where
 * element size is 4 (same as integer).
 */
#ifdef qsort
#undefine qsort
#endif
void xv_qsort ( array, size, unit, compare )
   int array[1000];	/* array to be sorted */
   int size;		/* size of array to sort, should be at least 100 */
   int unit;		/* Size of array element */
   int compare();	/* comaparison function */
{
   int stack[68], *top;	/* work array, depth of stack is bounded */
   int start, finish, lbound, hbound, pivot, temp, i, j;

   if ( unit != sizeof(int) ) {	/* punt */
	qsort ( array, size, unit, compare );
	return;
    }
   if ( size <= 1 ) return;
   /* set up initial partition on top of stack */
   top = &stack[68];
   *--top = 0;		/* push lbound */
   *--top = size-1;	/* push initial hbound */

   /* loop until stack is emtpy */

   while ( top < &stack[68] ) {

      /* pop next range from stack and see if it has at least 3 elements */

      finish = *top++; start = *top++;
      pivot = array[start];
      if ( finish > start + 1 ) {
         /*
          *  more than 2 elements, split range into 2 sections according to
          *  the relation to the pivot value.
          */
	 array[start] = array[(start+finish)/2];	/* avoid sequence */
	 array[(start+finish)/2] = pivot; pivot = array[start];
         lbound = start; hbound = finish;
         while ( lbound < hbound ) {
            if ( compare(&pivot, &array[lbound+1]) > 0 ) {
               lbound++;
            } else {
               temp = array[hbound];
               array[hbound] = array[lbound+1];
               hbound--;
               array[lbound+1] = temp;
            }
         }
         /* determine which parition is bigger and push onto stack */
         if ( lbound + lbound < (start+finish) ) {
            /* push high partition first. */
            *--top = lbound + 1;
            *--top = finish;
            finish = start;		/* skip add step below */
             /* either push low partition or sort by inspection */
            if ( lbound - start > 1 ) {
               *--top = start;
               *--top = lbound;
            } else {
               /* 2 element parition (start+1=lbound), sort by looking */
               if ( pivot > array[lbound] ) {
                  array[start] = array[lbound];
                  array[lbound] = pivot;
               }
            }
         } else if ( lbound != finish ) {
            /* either push low partition or sort by inspection */
            if ( lbound - start > 1 ) {
               *--top = start;
               *--top = lbound;
            } else if ( lbound  > start ) {
               /* 2 element parition (start+1=lbound), sort by looking */
               if ( compare(&pivot, &array[lbound]) > 0 ) {
                  array[start] = array[lbound];
                  array[lbound] = pivot;
               }
            }
            /* push high partition or sort by inspection */
            if ( lbound < finish - 2 ) {
               *--top = lbound + 1;
               *--top = finish;
            } else if ( lbound < finish - 1 ) {  /* 2 in partition */
               if ( compare(&array[lbound+1], &array[finish]) > 0 ) {
                  temp = array[lbound+1];
                  array[lbound+1] = array[finish];
                  array[finish] = temp;
               }
            }
         } else {
            /*
             * Special case: high partition is empty, indicating that pivot
             * value is at maximum.  Move to end and re-push remainder.
             */
            array[start] = array[finish];
            array[finish] = pivot;
            *--top = start;
            *--top = finish - 1;
         }

      } else {
         /* only 2 elements in partition, sort inline */
         if ( compare(&pivot,&array[finish]) > 0 ) {
            array[start] = array[finish];
            array[finish] = pivot;
         }
      }
   }
} 

/*****************************************************************************/

/*
**  VMS readdir() routines.
**  Written by Rich $alz, <rsalz@bbn.com> in August, 1990.
**  This code has no copyright.
*/

/* 12-NOV-1990 added d_namlen field and special case "." name -GJC@MITECH.COM 
 */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <descrip.h>
#include <rmsdef.h>
#include "dirent.h"

    /* Uncomment the next line to get a test routine. */
/*#define TEST*/

    /* Number of elements in vms_versions array */
#define VERSIZE(e)	(sizeof e->vms_versions / sizeof e->vms_versions[0])

    /* Linked in later. */
extern char	*strrchr();
extern char	*strcpy();
/*  Don't need this when all these programs are lumped together.    RLD
extern char	*malloc();
*/


/*
**  Open a directory, return a handle for later use.
*/
DIR *
opendir(name)
    char	*name;
{
    DIR		*dd;

    /* Get memory for the handle, and the pattern. */
    if ((dd = (DIR *)malloc(sizeof *dd)) == NULL) {
	errno = ENOMEM;
	return NULL;
    }
    
    if (strcmp(".",name) == 0) name = "";
    
    dd->pattern = malloc((unsigned int)(strlen(name) + sizeof "*.*" + 1));
    if (dd->pattern == NULL) {
	free((char *)dd);
	errno = ENOMEM;
	return NULL;
    }

    /* Fill in the fields; mainly playing with the descriptor. */
    (void)sprintf(dd->pattern, "%s*.*", name);
    dd->context = 0;
    dd->vms_wantversions = 0;
    dd->pat.dsc$a_pointer = dd->pattern;
    dd->pat.dsc$w_length = strlen(dd->pattern);
    dd->pat.dsc$b_dtype = DSC$K_DTYPE_T;
    dd->pat.dsc$b_class = DSC$K_CLASS_S;

    return dd;
}


/*
**  Set the flag to indicate we want versions or not.
*/
void
vmsreaddirversions(dd, flag)
    DIR		*dd;
    int		flag;
{
    dd->vms_wantversions = flag;
}


/*
**  Free up an opened directory.
*/
void
closedir(dd)
    DIR		*dd;
{
    free(dd->pattern);
    free((char *)dd);
}


/*
**  Collect all the version numbers for the current file.
*/
static void
collectversions(dd)
    DIR				*dd;
{
    struct dsc$descriptor_s	pat;
    struct dsc$descriptor_s	res;
    struct dirent		*e;
    char			*p;
    char			buff[sizeof dd->entry.d_name];
    int				i;
    char			*text;
    long			context;

    /* Convenient shorthand. */
    e = &dd->entry;

    /* Add the version wildcard, ignoring the "*.*" put on before */
    i = strlen(dd->pattern);
    text = malloc((unsigned int)(i + strlen(e->d_name)+ 2 + 1));
    if (text == NULL)
	return;
    (void)strcpy(text, dd->pattern);
    (void)sprintf(&text[i - 3], "%s;*", e->d_name);

    /* Set up the pattern descriptor. */
    pat.dsc$a_pointer = text;
    pat.dsc$w_length = strlen(text);
    pat.dsc$b_dtype = DSC$K_DTYPE_T;
    pat.dsc$b_class = DSC$K_CLASS_S;

    /* Set up result descriptor. */
    res.dsc$a_pointer = buff;
    res.dsc$w_length = sizeof buff - 2;
    res.dsc$b_dtype = DSC$K_DTYPE_T;
    res.dsc$b_class = DSC$K_CLASS_S;

    /* Read files, collecting versions. */
    for (context = 0; e->vms_verscount < VERSIZE(e); e->vms_verscount++) {
	if (lib$find_file(&pat, &res, &context) == RMS$_NMF || context == 0)
	    break;
	buff[sizeof buff - 1] = '\0';
	if (p = strchr(buff, ';'))
	    e->vms_versions[e->vms_verscount] = atoi(p + 1);
	else
	    e->vms_versions[e->vms_verscount] = -1;
    }

    free(text);
}


/*
**  Read the next entry from the directory.
*/
struct dirent *
readdir(dd)
    DIR				*dd;
{
    struct dsc$descriptor_s	res;
    char			*p;
    char			buff[sizeof dd->entry.d_name];
    int				i;

    /* Set up result descriptor, and get next file. */
    res.dsc$a_pointer = buff;
    res.dsc$w_length = sizeof buff - 2;
    res.dsc$b_dtype = DSC$K_DTYPE_T;
    res.dsc$b_class = DSC$K_CLASS_S;
    if (lib$find_file(&dd->pat, &res, &dd->context) == RMS$_NMF
     || dd->context == 0L)
	/* None left... */
	return NULL;

    /* Force the buffer to end with a NUL. */
    buff[sizeof buff - 1] = '\0';
    for (p = buff; !isspace(*p); p++)
	;
    *p = '\0';

    /* Skip any directory component and just copy the name. */
    if (p = strchr(buff, ']'))
	(void)strcpy(dd->entry.d_name, p + 1);
    else
	(void)strcpy(dd->entry.d_name, buff);

    /* Clobber the version. */
    if (p = strchr(dd->entry.d_name, ';'))
	*p = '\0';

    dd->entry.d_namlen = strlen(dd->entry.d_name);

    dd->entry.vms_verscount = 0;
    if (dd->vms_wantversions)
	collectversions(dd);
    return &dd->entry;
}


/*
**  Return something that can be used in a seekdir later.
*/
long
telldir(dd)
    DIR		*dd;
{
    return dd->context;
}


/*
**  Return to a spot where we used to be.
*/
void
seekdir(dd, pos)
    DIR		*dd;
    long	pos;
{
    dd->context = pos;
}


#ifdef	TEST
main()
{
    char		buff[256];
    DIR			*dd;
    struct dirent	*dp;
    int			i;
    int			j;

    for ( ; ; ) {
	printf("\n\nEnter dir:  ");
	(void)fflush(stdout);
	(void)gets(buff);
	if (buff[0] == '\0')
	    break;
	if ((dd = opendir(buff)) == NULL) {
	    perror(buff);
	    continue;
	}

	/* Print the directory contents twice, the second time print
	 * the versions. */
	for (i = 0; i < 2; i++) {
	    while (dp = readdir(dd)) {
		printf("%s%s", i ? "\t" : "    ", dp->d_name);
		for (j = 0; j < dp->vms_verscount; j++)
		    printf("  %d", dp->vms_versions[j]);
		printf("\n");
	    }
	    rewinddir(dd);
	    vmsreaddirversions(dd, 1);
	}
	closedir(dd);
    }
    exit(0);
}
#endif	/* TEST */

/*****************************************************************************/

/*
	pseudo_root - Return the Window ID of the Pseudo Root

	NOTE: This routine is required for DECwindows
	because the true root window is hidden behind
	a pseudo-root window created by the window manager.

	Calling Sequence:
		root_win = pseudo_root(display, screen);
	where:
		display must already be opened and
		screen is usually DefaultScreen(display).

*/

#include <X11/Xlib.h>

#ifdef NULL
#undef NULL
#define	NULL	0L
#endif

static void chase_root();

static int done;
static Window last_win;
static int root_x, root_y, root_width, root_height;
static int root_bw, root_depth;


Window pseudo_root(dpy, screen)
Display *dpy;
int screen;
{
    Window root, win;

    /* Start at the real root */
    root = RootWindow(dpy, screen);

    /* Get the geometry of the root */
    if (XGetGeometry(dpy, root, &win, &root_x, &root_y, 
			&root_width, &root_height,
			&root_bw, &root_depth))
    {
	/* Set up for the tree walk */
	done = False;
	last_win = root;

	/* Run down the tree to find the pseudo root */
	chase_root(dpy, root);
	return last_win;
    }
    else
	/* Classic case of "this should never happen" */
	return root;

} /*** End pseudo_root() ***/


/*
	chase_root - Internal to this module

	This is a recursive routine for descending the window tree.

	It looks for the first window which does NOT overlay the root,
	then returns with the ID of the last window it saw in the
	global variable last_win.

	NOTE: The parameters of the root window must be set up before
	calling chase_root().
*/

static void chase_root (dpy, w)
Display *dpy;
Window w;
{
    Window root, parent;
    unsigned int nchildren;
    Window *children = NULL;
    Status status;
    int n;
    int x, y, rx, ry;
    unsigned int width, height, bw, depth;

    /* Insurance */
    if (done)
	return;

    if (XGetGeometry(dpy, w, &root, &x, &y, &width, &height, &bw, &depth))
    {
	/*
	    If this window does not exactly overlay the root
	    then we have gone one too far, i.e., we have finished.
	*/
	if ( (x != root_x) ||
	     (y != root_y) ||
	     (width  != root_width) ||
	     (height != root_height) )
	{
	    done = True;
	    return;
	}

	/* Otherwise, remember where we got up to and continue */
	else
	    last_win = w;
    }

    else
	/* We are in trouble if this happens!!! */
	return;

    if (!XQueryTree (dpy, w, &root, &parent, &children, &nchildren))
	/* Likewise, we hope we never get here!!! */
	return;

    for (n = 0; n < nchildren; n++)
    {
	chase_root (dpy, children[n]);
	if (done)
	    break;
    }
#ifdef VMS
    if (children) XFree ((char *) children);
#else
    if (children) free ((char *) children);
#endif
    return;

} /*** End chase_root() ***/

/*****************************************************************************/

/*
 * @(#)argproc.c 1.0 89/02/01		Mark Pizzolato (mark@infopiz.uucp)	
 */

#ifndef lint
char argproc_version[] = "@(#)argproc.c VMS uucp Version infopiz-1.0";
#endif

#include "includes.h"		/* System include files, system dependent */


/*
 * getredirection() is intended to aid in porting C programs
 * to VMS (Vax-11 C) which does not support '>' and '<'
 * I/O redirection, along with a command line pipe mechanism
 * using the '|' AND background command execution '&'.
 * The piping mechanism will probably work with almost any 'filter' type
 * of program.  With suitable modification, it may useful for other
 * portability problems as well.
 *
 * Author:  Mark Pizzolato	mark@infopiz.UUCP
 */
struct list_item
    {
    struct list_item *next;
    char *value;
    };

int
getredirection(ac, av)
int		*ac;
char		***av;
/*
 * Process vms redirection arg's.  Exit if any error is seen.
 * If getredirection() processes an argument, it is erased
 * from the vector.  getredirection() returns a new argc and argv value.
 * In the event that a background command is requested (by a trailing "&"),
 * this routine creates a background subprocess, and simply exits the program.
 *
 * Warning: do not try to simplify the code for vms.  The code
 * presupposes that getredirection() is called before any data is
 * read from stdin or written to stdout.
 *
 * Normal usage is as follows:
 *
 *	main(argc, argv)
 *	int		argc;
 *    	char		*argv[];
 *	{
 *		getredirection(&argc, &argv);
 *	}
 */
{
    int			argc = *ac;	/* Argument Count	  */
    char		**argv = *av;	/* Argument Vector	  */
    char		*ap;   		/* Argument pointer	  */
    int	       		j;		/* argv[] index		  */
    extern int		errno;		/* Last vms i/o error 	  */
    int			item_count = 0;	/* Count of Items in List */
    int			new_file;	/* flag, true if '>' used */
    struct list_item 	*list_head = 0;	/* First Item in List	    */
    struct list_item	*list_tail;	/* Last Item in List	    */
    char 		*in = NULL;	/* Input File Name	    */
    char 		*out = NULL;	/* Output File Name	    */
    char 		*outmode = "w";	/* Mode to Open Output File */
    int			cmargc = 0;    	/* Piped Command Arg Count  */
    char		**cmargv = NULL;/* Piped Command Arg Vector */
    stat_t		statbuf;	/* fstat buffer		    */

    /*
     * First handle the case where the last thing on the line ends with
     * a '&'.  This indicates the desire for the command to be run in a
     * subprocess, so we satisfy that desire.
     */
    ap = argv[argc-1];
    if (0 == strcmp("&", ap))
	exit(background_process(--argc, argv));
    if ('&' == ap[strlen(ap)-1])
	{
	ap[strlen(ap)-1] = '\0';
	exit(background_process(argc, argv));
	}
    /*
     * Now we handle the general redirection cases that involve '>', '>>',
     * '<', and pipes '|'.
     */
    for (new_file = 0, j = 0; j < argc; ++j)
	{
	if (0 == strcmp("<", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		errno = EINVAL;
		perror("No input file");
		exit(EXIT_ERR);
		}
	    in = argv[++j];
	    continue;
	    }
	if ('<' == *(ap = argv[j]))
	    {
	    in = 1 + ap;
	    continue;
	    }
	if (0 == strcmp(">", ap))
	    {
	    if (j+1 >= argc)
		{
		errno = EINVAL;
		perror("No output file");
		exit(EXIT_ERR);
		}
	    out = argv[++j];
	    new_file = 1;
	    continue;
	    }
	if ('>' == *ap)
	    {
	    if ('>' == ap[1])
		{
		outmode = "a";
		if ('\0' == ap[2])
		    out = argv[++j];
		else
		    out = 2 + ap;
		}
	    else
		{ out = 1 + ap;  new_file = 1; }
	    continue;
	    }
	if (0 == strcmp("|", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		errno = EPIPE;
		perror("No command to Pipe to");
		exit(EXIT_ERR);
		}
	    cmargc = argc-(j+1);
	    cmargv = &argv[j+1];
	    argc = j;
	    outmode = "wb";	/* pipes are binary mode devices */
	    continue;
	    }
	if ('|' == *(ap = argv[j]))
	    {
	    ++argv[j];
	    cmargc = argc-j;
	    cmargv = &argv[j];
	    argc = j;
	    outmode = "wb";	/* pipes are binary mode devices */
	    continue;
	    }
	expand_wild_cards(ap, &list_head, &list_tail, &item_count);
	}
    /*
     * Allocate and fill in the new argument vector, Some Unix's terminate
     * the list with an extra null pointer.
     */
    argv = *av = calloc(item_count+1, sizeof(char *));
    for (j = 0; j < item_count; ++j, list_head = list_head->next)
	argv[j] = list_head->value;
    *ac = item_count;
    if (cmargv != NULL)
	{
	char subcmd[1024];
	static char *pipe_and_fork();

	if (out != NULL)
	    {
	    errno = EINVAL;
	    perror("Invalid '|' and '>' specified");
	    exit(EXIT_ERR);
	    }
	strcpy(subcmd, cmargv[0]);
	for (j = 1; j < cmargc; ++j)
	    {
	    strcat(subcmd, " \"");
	    strcat(subcmd, cmargv[j]);
	    strcat(subcmd, "\"");
	    }
	out = pipe_and_fork(subcmd);
	outmode = "wb";
	}
	
    /* Check for input from a pipe (mailbox) */

    if(fstat(0, &statbuf) == 0){
	if(strncmp(statbuf.st_dev, "MB", 2) == 0 || 
	    strncmp(statbuf.st_dev, "_MB", 3) == 0){

	    /* Input from a pipe, reopen it in binary mode to disable	*/
	    /* carriage control processing.				*/

	    if (in != NULL){
		errno = EINVAL;
		perror("Invalid '|' and '<' specified");
		exit(EXIT_ERR);
		}
	    freopen(statbuf.st_dev, "rb", stdin);
	    }
	}
    else {
	perror("fstat failed");
	exit(EXIT_ERR);
	}


#ifdef __ALPHA
        /*, "mbc=32", "mbf=2"))) blows up on the ALPHA cbm 11/08/92 */
    if ((in != NULL) && (NULL == freopen(in, "r", stdin)))
        {
#else
    if ((in != NULL) && (NULL == freopen(in, "r", stdin, "mbc=32", "mbf=2")))
	{
#endif
	perror(in);    	       	/* Can't find file		*/
	exit(EXIT_ERR);		/* Is a fatal error		*/
	}
#ifdef __ALPHA
    if ((out != NULL) && (NULL == freopen(out, outmode, stdout)))
        {
#else
    if ((out != NULL) && (NULL == freopen(out, outmode, stdout, "mbc=32", "mbf=2")))
	{	
#endif
	perror(ap);		/* Error, can't write or append	*/
	exit(EXIT_ERR);		/* Is a fatal error		*/
	}

     if ( new_file ) {
	/*
	 * We are making an explicit output file, fstat the file and
         * declare exit handler to be able change the file to fixed length
	 * records if necessary. 
	 */
	char fname[256];
	static int outfile_rundown(), check_outfile_filetype();
	static stat_t ofile_stat;
	static struct exit_control_block {
    	    struct exit_control_block *flink;
    	    int	(*exit_routine)();
	    int arg_count;
	    int *status_address;	/* arg 1 */
	    stat_t *stat;		/* arg 2 */
	    int exit_status;
	    int skew[128];
	} exit_block = 
	    { 0, outfile_rundown, 2, &exit_block.exit_status, &ofile_stat, 0 };

	if ( fstat ( fileno(stdout), &ofile_stat ) == 0 )
	     sys$dclexh ( &exit_block );
	else fprintf(stderr,"Error fstating stdout - %s\n",
		strerror(errno,vaxc$errno) );

	if ( fgetname ( stdout, fname, 0 ) ) check_outfile_filetype ( fname );
     }
#ifdef DEBUG
    fprintf(stderr, "Arglist:\n");
    for (j = 0; j < *ac;  ++j)
	fprintf(stderr, "argv[%d] = '%s'\n", j, argv[j]);
#endif
}

static int binary_outfile = 0;
void set_outfile_binary() { binary_outfile = 1; return; }

/*
 * Check if output file should be set to binary (fixed 512) on exit based
 * upon the filetype.
 */
static int check_outfile_filetype ( name )
    char *name;
{
    char *binary_filetypes, *ext, *p, *t;
    binary_filetypes = (char *) getenv ( "PBM_BINARY_FILETYPES" );
    if ( binary_filetypes == NULL ) return 0;

    ext = strchr ( name, '.' );  if ( ext == NULL ) return 0;
    ext++;
    t = strrchr ( name, '.' );   if ( t != NULL ) *t = '\0';

    for ( p = binary_filetypes; *p != '\0'; p++ ) {
	for ( t = p;
	      (*p != '\0' ) && (strchr ( ", 	", *p ) == NULL); 
	     p++ ) *p = toupper(*p);
	*p = '\0';

	if ( strcmp ( t, ext ) == 0 ) {
	    binary_outfile = 1;
	    break;
	}
    }
    return binary_outfile;
}


/*
 * Exit handler to set output file to binary on image exit.
 */
static int outfile_rundown ( reason, statbuf )
    int *reason;
    stat_t *statbuf;
{
    int code, channel, status, LIB$GETDVI(), sys$assign(), sys$qiow();
    long int fib_desc[2], devchar;
    short int iosb[4];
    $DESCRIPTOR(device, statbuf->st_dev);
    struct fibdef fib;		/* File information block (XQP) */
    struct atrdef atr[2];
    struct fat {
      unsigned char rtype, ratattrib;
      unsigned short int rsize, hiblk[2], efblk[2], ffbyte, maxrec, defext, gbc;
      unsigned short int reserved[4], versions;
    } rat;

    if ( !binary_outfile ) return 1;	/* leave file alone */
    /*
     * Assign channel to device listed in stat block and test if it is
     * a directory structured device, returning if not.
     */
    device.dsc$w_length = strlen ( statbuf->st_dev );
    status = sys$assign ( &device, &channel, 0, 0 );
    if ((status & 1) == 0) { fprintf(stderr, 
	"assign error to %s (%d)\n", device.dsc$a_pointer, status);
		return status; }
    code = DVI$_DEVCHAR;
    status = LIB$GETDVI ( &code, &channel, 0, &devchar );
    if ((status & 1) == 0) { fprintf(stderr, "getdvi error: %d\n", status);
		return status; }
    if ( (devchar & DEV$M_DIR) == 0 ) return 1;
    /*
     * Build File Information Block and Atrribute block.
     */
#if defined(__ALPHA) || defined(__DECC)
    fib.fib$w_fid[0] = statbuf->st_ino[0];
    fib.fib$w_fid[1] = statbuf->st_ino[1];
    fib.fib$w_fid[2] = statbuf->st_ino[2];
#else
    fib.fib$r_fid_overlay.fib$w_fid[0] = statbuf->st_ino[0];
    fib.fib$r_fid_overlay.fib$w_fid[1] = statbuf->st_ino[1];
    fib.fib$r_fid_overlay.fib$w_fid[2] = statbuf->st_ino[2];
#endif

    atr[0].atr$w_size = sizeof(rat);
    atr[0].atr$w_type = ATR$C_RECATTR;
    atr[0].atr$l_addr = &rat;
    atr[1].atr$w_size = 0; atr[1].atr$w_type = 0;
    /*
     * Perform access function with read_attributes sub-function.
     */
    freopen ( "SYS$OUTPUT:", "a", stdout );
    fib_desc[0] = 10; fib_desc[1] = (long int) &fib;
#if defined(__ALPHA) || defined(__DECC)
    fib.fib$l_acctl = FIB$M_WRITE;
#else
    fib.fib$r_acctl_overlay.fib$l_acctl = FIB$M_WRITE;
#endif
    status = sys$qiow ( 0, channel, IO$_ACCESS|IO$M_ACCESS,
		 &iosb, 0, 0, &fib_desc, 0, 0, 0, &atr, 0 );
    /*
     * If status successful, update record byte and perform a MODIFY.
     */
    if ( (status&1) == 1 ) status = iosb[0];
    if ( (status&1) == 1 ) {
      rat.rtype = 1;		/* fixed length records */
      rat.rsize = 512;  	/* 512 byte block recrods */
      rat.ratattrib = 0;        /* Record attributes: none */

     status = sys$qiow
	( 0, channel, IO$_MODIFY, &iosb, 0, 0, &fib_desc, 0, 0, 0, &atr, 0 );
       if ( (status&1) == 1 ) status = iosb[0];
   }
   sys$dassgn ( channel );
   if ( (status & 1) == 0 ) fprintf ( stderr,
	"Failed to convert output file to binary format, status: %d\n", status);
   return status;
}


static add_item(head, tail, value, count)
struct list_item **head;
struct list_item **tail;
char *value;
int *count;
{
    if (*head == 0)
	{
	if (NULL == (*head = calloc(1, sizeof(**head))))
	    {
	    errno = ENOMEM;
	    perror("");
	    exit(EXIT_ERR);
	    }
	*tail = *head;
	}
    else
	if (NULL == ((*tail)->next = calloc(1, sizeof(**head))))
	    {
	    errno = ENOMEM;
	    perror("");
	    exit(EXIT_ERR);
	    }
	else
	    *tail = (*tail)->next;
    (*tail)->value = value;
    ++(*count);
}

static expand_wild_cards(item, head, tail, count)
char *item;
struct ltem_list **head;
struct ltem_list **tail;
int *count;
{
int expcount = 0;
int context = 0;
int status;
int status_value;
int had_version;
$DESCRIPTOR(filespec, item);
$DESCRIPTOR(defaultspec, "SYS$DISK:[]*.*;");
$DESCRIPTOR(resultspec, "");

    if (strcspn(item, "*%") == strlen(item))
	{
	add_item(head, tail, item, count);
	return;
	}
    resultspec.dsc$b_dtype = DSC$K_DTYPE_T;
    resultspec.dsc$b_class = DSC$K_CLASS_D;
    resultspec.dsc$a_pointer = NULL;
    filespec.dsc$w_length = strlen(item);
    /*
     * Only return version specs, if the caller specified a version
     */
    had_version = strchr(item, ';');
    while (1 == (1&lib$find_file(&filespec, &resultspec, &context,
    				 &defaultspec, 0, &status_value, &0)))
	{
	char *string;
	char *c;

	if (NULL == (string = calloc(1, resultspec.dsc$w_length+1)))
	    {
	    errno = ENOMEM;
	    perror("");
	    exit(EXIT_ERR);
	    }
	strncpy(string, resultspec.dsc$a_pointer, resultspec.dsc$w_length);
	string[resultspec.dsc$w_length] = '\0';
	if (NULL == had_version)
	    *((char *)strrchr(string, ';')) = '\0';
	/*
	 * Be consistent with what the C RTL has already done to the rest of
	 * the argv items and lowercase all of these names.
	 */
	for (c = string; *c; ++c)
	    if (isupper(*c))
		*c = tolower(*c);
	add_item(head, tail, string, count);
	++expcount;
	}
    if (expcount == 0)
	add_item(head, tail, item, count);
    lib$sfree1_dd(&resultspec);
    lib$find_file_end(&context);
}

static int child_st[2];	/* Event Flag set when child process completes	*/

static short child_chan;/* I/O Channel for Pipe Mailbox			*/

static exit_handler(status)
int *status;
{
short iosb[4];

    if (0 == child_st[0])
	{
#ifdef DEBUG
	fprintf(stderr, "Waiting for Child Process to Finnish . . .\n");
#endif
	fflush(stdout);	    /* Have to flush pipe for binary data to	*/
			    /* terminate properly -- <tp@mccall.com>	*/
#ifdef DEBUG
	fprintf(stderr, "    stdout flushed. . .\n");
#endif
	sys$qio(0, child_chan, IO$_WRITEOF, iosb, 0, 0, 0, 0, 0, 0, 0, 0);
#ifdef DEBUG
	fprintf(stderr, "    Pipe terminated. . .\n");
#endif
	fclose(stdout);
#ifdef DEBUG
	fprintf(stderr, "    stdout closed. . .\n");
#endif
	sys$synch(0, child_st);
	sys$dassgn(child_chan);
	}
#ifdef DEBUG
	fprintf(stderr, "    sync done. . .\n");
#endif
}

#include <syidef>		/* System Information Definitions	*/

static sig_child(chan)
int chan;
{
#ifdef DEBUG
    fprintf(stderr, "Child Completion AST, st: %x\n", child_st[0] );
#endif
    if (child_st[0] == 0)
	{ child_st[0] = 1; }
    sys$setef ( 0 );
}

static struct exit_control_block
    {
    struct exit_control_block *flink;
    int	(*exit_routine)();
    int arg_count;
    int *status_address;
    int exit_status;
    } exit_block =
    {
    0,
    exit_handler,
    1,
    &exit_block.exit_status,
    0
    };

static char *pipe_and_fork(cmd)
char *cmd;
{
    $DESCRIPTOR(cmddsc, cmd);
    static char mbxname[64], ef = 0;
    $DESCRIPTOR(mbxdsc, mbxname);
    short iosb[4];
    int status;
    int pid;
    struct
	{
	short dna_buflen;
	short dna_itmcod;
	char *dna_buffer;
	short *dna_retlen;
	int listend;
	} itmlst =
	{
	sizeof(mbxname),
	DVI$_DEVNAM,
	mbxname,
	&mbxdsc.dsc$w_length,
	0
	};
    int mbxsize;
    struct
	{
	short mbf_buflen;
	short mbf_itmcod;
	int *mbf_maxbuf;
	short *mbf_retlen;
	int listend;
	} syiitmlst =
	{
	sizeof(mbxsize),
	SYI$_MAXBUF,
	&mbxsize,
	0,
	0
	};

    cmddsc.dsc$w_length = strlen(cmd);
    /*
     * Get the SYSGEN parameter MAXBUF, and the smaller of it and 2048 as
     * the size of the 'pipe' mailbox.
     */
    if (1 == (1&(vaxc$errno = sys$getsyiw(0, 0, 0, &syiitmlst, iosb, 0, 0, 0))))
	vaxc$errno = iosb[0];
    if (0 == (1&vaxc$errno))
	{
 	errno = EVMSERR;
	perror("Can't get SYSGEN parameter value for MAXBUF");
	exit(EXIT_ERR);
	}
    if (mbxsize > 2048)
	mbxsize = 2048;
    if (0 == (1&(vaxc$errno = sys$crembx(0, &child_chan, mbxsize, mbxsize, 0, 0, 0))))
	{
	errno = EVMSERR;
	perror("Can't create pipe mailbox");
	exit(EXIT_ERR);
	}
    if (1 == (1&(vaxc$errno = sys$getdviw(0, child_chan, 0, &itmlst, iosb,
    					  0, 0, 0))))
	vaxc$errno = iosb[0];
    if (0 == (1&vaxc$errno))
	{
 	errno = EVMSERR;
	perror("Can't get pipe mailbox device name");
	exit(EXIT_ERR);
	}
    mbxname[mbxdsc.dsc$w_length] = '\0';
#ifdef DEBUG
    fprintf(stderr, "Pipe Mailbox Name = '%s'\n", mbxname);
#endif
    if (0 == (1&(vaxc$errno = lib$spawn(&cmddsc, &mbxdsc, 0, &1,
    					0, &pid, child_st, &ef, sig_child,
    					&child_chan))))
	{
	errno = EVMSERR;
	perror("Can't spawn subprocess");
	exit(EXIT_ERR);
	}
#ifdef DEBUG
    fprintf(stderr, "Subprocess's Pid = %08X\n", pid);
#endif
    sys$dclexh(&exit_block);
    return(mbxname);
}

background_process(argc, argv)
int argc;
char **argv;
{
char command[2048] = "$";
$DESCRIPTOR(value, command);
$DESCRIPTOR(cmd, "BACKGROUND$COMMAND");
$DESCRIPTOR(null, "NLA0:");
int pid;

    strcat(command, argv[0]);
    while (--argc)
	{
	strcat(command, " \"");
	strcat(command, *(++argv));
	strcat(command, "\"");
	}
    value.dsc$w_length = strlen(command);
    if (0 == (1&(vaxc$errno = lib$set_symbol(&cmd, &value))))
	{
	errno = EVMSERR;
	perror("Can't create symbol for subprocess command");
	exit(EXIT_ERR);
	}
    if (0 == (1&(vaxc$errno = lib$spawn(&cmd, &null, 0, &17, 0, &pid))))
	{
	errno = EVMSERR;
	perror("Can't spawn subprocess");
	exit(EXIT_ERR);
	}
#ifdef DEBUG
    fprintf(stderr, "%s\n", command);
#endif
    fprintf(stderr, "%08X\n", pid);
    return(EXIT_OK);
}

/* got this off net.sources */

#ifdef	VMS
#define	index	strchr
#endif	/*VMS*/

/*
 * get option letter from argument vector
 */
int	opterr = 1,		/* useless, never set or used */
	optind = 1,		/* index into parent argv vector */
	optopt;			/* character checked for validity */
char	*optarg;		/* argument associated with option */

#define BADCH	(int)'?'
#define EMSG	""
#define tell(s)	fputs(progname,stderr);fputs(s,stderr); \
		fputc(optopt,stderr);fputc('\n',stderr);return(BADCH);

getopt(nargc,nargv,ostr)
int	nargc;
char	**nargv,
	*ostr;
{
	static char	*place = EMSG;	/* option letter processing */
	register char	*oli;		/* option letter list index */
	char	*index();
	char *progname;

	if(!*place) {			/* update scanning pointer */
		if(optind >= nargc || *(place = nargv[optind]) != '-' || !*++place) return(EOF);
		if (*place == '-') {	/* found "--" */
			++optind;
			return(EOF);
		}
	}				/* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' || !(oli = index(ostr,optopt))) {
		if(!*place) ++optind;
#ifdef VMS
		progname = strrchr(nargv[0],']');
#else
		progname = rindex(nargv[0],'/');
#endif
		if (!progname) progname = nargv[0]; else progname++;
		tell(": illegal option -- ");
	}
	if (*++oli != ':') {		/* don't need argument */
		optarg = NULL;
		if (!*place) ++optind;
	}
	else {				/* need an argument */
		if (*place) optarg = place;	/* no white space */
		else if (nargc <= ++optind) {	/* no arg */
			place = EMSG;
#ifdef VMS
			progname = strrchr(nargv[0],']');
#else
			progname = rindex(nargv[0],'/');
#endif
			if (!progname) progname = nargv[0]; else progname++;
			tell(": option requires an argument -- ");
		}
	 	else optarg = nargv[optind];	/* white space */
		place = EMSG;
		++optind;
	}
	return(optopt);			/* dump back option letter */
}

#ifndef __DECC
/*
 *  This was stolen from xterm v2.1 for VMS by Rick Dyson
*/

/*
 *	gethostname(2) - VMS Version of the UNIX routine
 *
 *	This routine simply translates the system logical
 *	name SYS$NODE and returns the result. This does not
 *	exactly match UNIX behaviour. In particular, the
 *	double colon at the end of the node name may cause
 *	problems in ported code.
 */

#include <ssdef.h>
#include <lnmdef.h>
#include <descrip.h>
#include <errno.h>

#define	NULL	0L


int gethostname(name, namelen)		/*--- Get node name ---*/
char name[];
int  namelen;
{
    /* Logical name to translate */
    char logical[] = "SYS$NODE";
    $DESCRIPTOR(logical_dsc, logical);

    /* These descriptors is necessary to select the logical name table */
    char sys_log_table[] = "LNM$SYSTEM_TABLE";
    $DESCRIPTOR(sys_log_table_dsc, sys_log_table);

    /* Item list as used by SYS$TRNLNM() */
    struct ITEM_LIST
    {
	unsigned short buflen;
	unsigned short code;
	int buffaddr;
	int retaddr;
    };

    struct ITEM_LIST log_list[2];

    unsigned short retlen;		/* Must be a WORD */

    /* 
	Buffer for the translation

	Max return length from SYS$TRNLNM is 255,
	but add a couple to be safe.
    */
    char string[257];

    int status, i;


    if (name == NULL)
    {
	errno = EFAULT;
	return (-1);
    }

    /* Setup the required item list for SYS$TRNLNM */

    log_list[0].buflen = 255;
    log_list[0].code   = LNM$_STRING;
    log_list[0].buffaddr = &string;
    log_list[0].retaddr  = &retlen;
    log_list[1].buflen = 0;
    log_list[1].code   = 0;

    /* Time to look for a logical name ... */

    status = SYS$TRNLNM(0, &sys_log_table_dsc,
		    &logical_dsc, 0, &log_list);

    if (status == SS$_NORMAL)
    {
	/* Got a translation */
	/* Copy the name back to the calling program */
	for (i=0; i<retlen && i<namelen; i++)
	{
	    name[i] = string[i];
	}
	if (i < namelen)
	    name[i] = 0;		/* Append a null */

	return (0);

    }
    else
    {
	errno = EFAULT;
	vaxc$errno = status;
	return (-1);
    }
}
#endif /* __DECC */
