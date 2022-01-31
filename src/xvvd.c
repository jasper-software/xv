
/*
 * xvvd.c - extract archived file automatically and regard it as a
 *          (virtual) directory.
 */

#define NEEDSDIR

#include "xv.h"

#ifdef AUTO_EXPAND

static void  vd_Dirtovd    		PARM((char *));
static void  vd_Vdtodir    		PARM((char *));
static int   vd_Mkvdir    		PARM((char *));
static int   vd_Rmvdir     		PARM((char *));
static int   vd_Movevdir  		PARM((char *, char *));
static void  vd_addvdtable		PARM((char *));
static void  vd_packvdtable		PARM((void));
static int   vd_recursive_mkdir		PARM((char *));
static int   vd_recursive_rmdir		PARM((char *));
static void  vd_optimize_path		PARM((char *));
static int   vd_ftype			PARM((char *));
static int   vd_compp			PARM((char *, char *));
static int   vd_UncompressFile		PARM((char *, char *));
static int   vd_tarc			PARM((char *));
static u_int vd_tar_sumchk		PARM((char *));

#define VD_VDTABLESIZE	100

#define VD_ERR -2
#define VD_UKN -1

static char *ext_command[] = {
/* KEEP 0 */
    NULL,
#define VD_ARC 1
    "arc xo %s",
#define VD_ARJ 2
    "unarj x %s",
#define VD_LZH 3
    "lha -xf %s",
#define VD_TAR 4
    "tar xvf %s",
#define VD_ZIP 5
    "unzip -xo %s",
#define VD_ZOO 6
    "zoo xOS %s",
};

int vdcount = 0;

static char vdroot[MAXPATHLEN+1];
static char *vdtable[VD_VDTABLESIZE];

/*
 * These functions initialize and settle virtual directory system.
 * Vdinit:
 *	creates root of virtual directory.
 * Vdsettle:
 *	sweeps virtual directories.
 */
void Vdinit()
{
#ifndef VMS
    char tmp[MAXPATHLEN+1];

    xv_getwd(tmp, MAXPATHLEN+1);
    if (chdir(tmpdir)) {
	fprintf(stderr, "Warning: cannot chdir to tmpdir = '%s'.\n", tmpdir);
	fprintf(stderr,
		"         I will use current directory '%s' instead of tmpdir.\n",
		tmp);
    }
    xv_getwd(vdroot, MAXPATHLEN+1);
    strcat(vdroot, "/.xvvdXXXXXX");
    chdir(tmp);
#else
    sprintf(vdroot, "Sys$Scratch:xvvdXXXXXX");
#endif /* VMS */
#ifdef USE_MKSTEMP
    close(mkstemp(vdroot));
#else
    mktemp(vdroot);
#endif

    if (!vd_recursive_mkdir(vdroot))
	tmpdir = vdroot;
}

void Vdsettle()
{
    int i;

    for (i = 0; i < vdcount; i++)
	free(vdtable[i]);

    vdcount = 0;

    vd_recursive_rmdir(vdroot);
}

/*
 * This function chdir to virtual directory, if specified path is in
 * virtual directlry.
 */
int Chvdir(dir)
char *dir;
{
    char buf[MAXPATHLEN+1];

    if (Mkvdir(dir) == VD_ERR)
	return -1;

    strcpy(buf, dir);
    Dirtovd(buf);

    return (chdir(buf));
}

/*
 * These functions convert directory <-> virtual directory.
 * Dirtovd:
 *	front interface of vd_Dirtovd.
 * vd_Dirtovd:
 *	converts directory to virtual directory.
 * Vdtodir:
 *	front interface of vd_Vdtodir.
 * vd_Vdtodir:
 *	converts virtual directory to normal directory.
 * Dirtosubst:
 *	converts directory to substance of archive.
 */
void Dirtovd(dir)
char *dir;
{
    vd_optimize_path(dir);

    vd_Dirtovd(dir);
}

static void vd_Dirtovd(dir)
char *dir;
{
    int i;

    for (i = 0; i < vdcount; i++)
	if (!strncmp(dir, vdtable[i], strlen(vdtable[i]))) {
	    char tmp[MAXPATHLEN+1];

	    sprintf(tmp, "%s%s", vdroot, dir);
	    strcpy(dir, tmp);
	    Dirtovd(dir);
	}
}

void Vdtodir(dir)
char *dir;
{
    vd_optimize_path(dir);

    vd_Vdtodir(dir);
}

static void vd_Vdtodir(vd)
char *vd;
{
    int i;
    char tmp[MAXPATHLEN+1];

    for (i = vdcount-1; i >= 0; i--) {
	sprintf(tmp, "%s%s", vdroot, vdtable[i]);
	if(!strncmp(vd, tmp, strlen(tmp))) {
	    strcpy(tmp, vd+strlen(vdroot));
	    strcpy(vd, tmp);
	    Vdtodir(vd);
	}
    }
}

void Dirtosubst(dir)
char *dir;
{
    char tmp[MAXPATHLEN+1];

    Dirtovd(dir);

    strcpy(tmp, dir+strlen(vdroot));

    if (Isarchive(tmp))
	strcpy(dir, tmp);
}

/*
 * These functions make virtual directory and extracts archive, if
 * specified path is archive.
 * Mkvdir:
 *	front interface of vd_Mkvdir.
 * vd_Mkvdir:
 *	does real work.
 * Mkvdir_force: (used by makeThumbDir(in xvbrowse.c) only)
 *	make virtual directory by force.
 */
int Mkvdir(dir)
char *dir;
{
    char dir1[MAXPATHLEN+1], dir2[MAXPATHLEN+1];
    char *d1, *d2;
    int rv;

#if defined(SYSV) || defined(SVR4) || defined(__USE_XOPEN_EXTENDED)
    sighold(SIGHUP);
    sighold(SIGCHLD);
#else
    int mask;
    mask = sigblock(sigmask(SIGHUP)|sigmask(SIGCHLD));
#endif

    strcpy(dir1, dir);
    vd_optimize_path(dir1);

    if ((rv = vd_Mkvdir(dir1)) != VD_ERR)
	goto MKVDIR_END;

    strcpy(dir2, dir1);
    d2 = dir2 + strlen(dir2);
    while (rv == VD_ERR) {
	d2--;
	while (*d2 != '/')
	    d2--;
	*d2 = '\0';
	rv = vd_Mkvdir(dir2);
    }
    d1 = dir1 + strlen(dir2);
    while ((rv != VD_ERR) && (*d1 != '\0')) {
	*d2++ = *d1++;
	while ((*d1 != '/') && (*d1 != '\0'))
	    *d2++ = *d1++;
	*d2 = '\0';
	rv = vd_Mkvdir(dir2);
    }

MKVDIR_END:
#if defined(SYSV) || defined(SVR4) || defined(__USE_XOPEN_EXTENDED)
    sigrelse(SIGHUP);
    sigrelse(SIGCHLD);
#else
    sigsetmask(mask);
#endif

    return rv;
}

static int vd_Mkvdir(dir)
char *dir;
{
    char dir1[MAXPATHLEN+1], dir2[MAXPATHLEN+1], tmp[MAXPATHLEN+1];
    int ftype, i;
    struct stat st;
    FILE *pfp;

    strcpy(dir1, dir);
    Dirtovd(dir1);
    strcpy(dir2, dir1);

    WaitCursor();

    if ((ftype = vd_ftype(dir1)) < 0) {
	SetCursors(-1);
	return ftype;
    }
    if (ftype == RFT_COMPRESS) {
	if (!(ftype = vd_compp(dir1, tmp))) {
	    SetCursors(-1);
	    return ftype;
	}
	strcpy(dir1, tmp);
    }

    if (!stat(dir1, &st)) {
	for(i = 0; i < vdcount; i++)
	    if (!strcmp(vdtable[i], dir2)) {
		SetCursors(-1);
		return 0;
	    }

	if (!S_ISDIR(st.st_mode)) {
	    char origdir[MAXPATHLEN+1], buf[MAXPATHLEN+10], buf1[100];

	    if (vdcount >= VD_VDTABLESIZE) {
		ErrPopUp("Sorry, you can't make virtual directory any more.",
			 "\nBummer!");
		goto VD_MKVDIR_ERR;
	    }

	    WaitCursor();

	    xv_getwd(origdir, MAXPATHLEN+1);

	    sprintf(tmp, "%s%s", vdroot, dir2);
	    if (vd_recursive_mkdir(tmp) || chdir(tmp)) {
		SetISTR(ISTR_INFO, "fail to make virtual directory.");
		Warning();
		goto VD_MKVDIR_ERR;
	    }
	    sprintf(buf, ext_command[ftype], dir1);

	    WaitCursor();

	    if((pfp = popen(buf, "r")) == NULL) {
		SetISTR(ISTR_INFO, "fail to extract archive '%s'.",
			BaseName(dir2));
		Warning();
		goto VD_MKVDIR_ERR;
	    }
	    while (1) {
		if (fread(buf1, 1, sizeof(buf1), pfp) < sizeof(buf1))
		    break;
		WaitCursor();
	    }
	    if (!feof(pfp)) {
		SetISTR(ISTR_INFO, "Pipe was broken.");
		Warning();
		pclose(pfp);
		goto VD_MKVDIR_ERR;
	    }
	    pclose(pfp);

	    if (strcmp(dir1, dir2))
		unlink(dir1);

	    vd_addvdtable(dir2);
	    Dirtovd(origdir);
	    chdir(origdir);
	    SetCursors(-1);
	    return 0;

VD_MKVDIR_ERR:
	    if (strcmp(dir1, dir2))
		unlink(dir1);
	    SetCursors(-1);
	    return VD_ERR;
	}
    }
    SetCursors(-1);
    return VD_ERR;
}

#ifdef VIRTUAL_TD
void Mkvdir_force(dir)
char *dir;
{
    char tmp[MAXPATHLEN+1];

    if (vdcount >= VD_VDTABLESIZE) {
      ErrPopUp("Sorry, you can't make virtual directory any more.",
	       "\nBummer!");
      return;
    }

    sprintf(tmp, "%s%s", vdroot, dir);
    if (vd_recursive_mkdir(tmp)) {
      SetISTR(ISTR_INFO, "Failed to make virtual directory.");
      Warning();
      return;
    }

    vd_addvdtable(dir);
}
#endif /* VIRTUAL_TD */

/*
 * These functions remove virtual directory, if exists.
 * Rmvdir:
 *	front interface of vd_Rmvdir.
 * vd_Rmvdir:
 *	remove virtual directory function.
 */
int Rmvdir(dir)
char *dir;
{
    int rv;
    char buf[MAXPATHLEN+1];

    strcpy(buf, dir);
    vd_optimize_path(buf);

    rv = vd_Rmvdir(buf);
    vd_packvdtable();
    return rv;
}

static int vd_Rmvdir(dir)
char *dir;
{
    int i;
    char tmp[MAXPATHLEN+1];

    for(i = 0; i < vdcount; i++)
	if (!strncmp(dir, vdtable[i], strlen(dir))) {
	    sprintf(tmp, "%s%s", vdroot, vdtable[i]);
	    if (vd_Rmvdir(tmp))
		return 1;
	    if (vd_recursive_rmdir(tmp))
		return 1;
	    vdtable[i][0] = '\0';
	}
    return 0;
}

/*
 * These functions move virtual directory, if exists.
 * Movevdir:
 *	front interface of move virtual directory function.
 * vd_Movevdir:
 *	does real work.
 */
int Movevdir(src, dst)
char *src, *dst;
{
/*
    char sbuf[MAXPATHLEN+1], dbuf[MAXPATHLEN+1];

    strcpy(sbuf, src);
    vd_optimize_path(sbuf);

    strcpy(dbuf, dst);
    vd_optimize_path(dbuf);

    return (vd_Movevdir(sbuf, dbuf));
*/
    return (vd_Movevdir(src, dst));
}

static int vd_Movevdir(src, dst)
char *src, *dst;
{
    int i;
    char *p, *pp;
    char tmp[MAXPATHLEN+1], tmps[MAXPATHLEN+1], tmpd[MAXPATHLEN+1];

    for (i = 0; i < vdcount; i++)
	if (!strncmp(src, vdtable[i], strlen(src))) {
	    sprintf(tmps, "%s%s", vdroot, vdtable[i]);
	    sprintf(tmp, "%s%s", dst, vdtable[i]+strlen(src));
	    sprintf(tmpd, "%s%s", vdroot, tmp);

	    if (vd_Movevdir(tmps, tmpd))
		return 1;

	    pp = vdtable[i];
	    p = (char *) malloc(strlen(tmp)+1);
	    strcpy(p, tmp);
	    vdtable[i] = p;

	    strcpy(tmp, tmpd);
	    for (p = tmp+strlen(tmp); *p != '/'; p--)
		;
	    *p = '\0';

	    if (vd_recursive_mkdir(tmp))
		goto VD_MOVEVDIR_ERR;

	    if (rename(tmps, tmpd) < 0)
		goto VD_MOVEVDIR_ERR;

	    free(pp);
	}
    return 0;

VD_MOVEVDIR_ERR:
    free(vdtable[i]);
    vdtable[i] = pp;
    return 1;
}

/*
 * These functions handle table of virtual directories.
 * vd_addvdtable:
 *	adds virtual directory to table.
 * vd_packvdtable:
 *	removes disused virtual directories from table.
 */
static void vd_addvdtable(vd)
char *vd;
{
    char *p;
    p = (char *) malloc(strlen(vd)+1);
    strcpy(p, vd);
    vdtable[vdcount] = p;
    vdcount++;
}

static void vd_packvdtable()
{
    int i, j;

    for (i = j = 0; i < vdcount; i++)
	if (vdtable[i][0] != '\0')
	    vdtable[j++] = vdtable[i];
	else
	    free(vdtable[i]);

    vdcount = j;
}

/*
 * These are utility functions.
 * vd_recursive_mkdir:
 *	makes directories recursively.
 * vd_recursive_rmdir
 *	removes directories recursively.
 */
static int vd_recursive_mkdir(dir)
char *dir;
{
    char buf[MAXPATHLEN+1], *p;
    struct stat st;

    strcpy(buf, dir);

    if (buf[strlen(buf) - 1] == '/')
	buf[strlen(buf) - 1] = '\0';

    p = rindex(buf, '/');
    *p = '\0';

    if (stat(buf, &st) < 0)
	if (vd_recursive_mkdir(buf) < 0)
	    return (-1);

    *p = '/';
    if (mkdir(buf, 0700) < 0)
	return (-1);

    return (0);
}

static int vd_recursive_rmdir(dir)
char *dir;
{
    char buf[MAXPATHLEN+1], buf2[MAXPATHLEN+1];
    DIR *dp;
    struct dirent *di;

    strcpy(buf, dir);

    if (buf[strlen(buf) - 1] == '/')
	buf[strlen(buf) - 1] = '\0';

    if ((dp = opendir(buf)) == NULL)
	return (-1);

    while ((di = readdir(dp)) != NULL) {
	struct stat st;

	if (!strcmp(di->d_name, ".") || !strcmp(di->d_name, ".."))
	    continue;

	sprintf(buf2, "%s/%s", dir, di->d_name);

	stat(buf2, &st);
	if (S_ISDIR(st.st_mode)) {
	    if (vd_recursive_rmdir(buf2) < 0)
		goto VD_RECURSIVE_RMDIR_ERR;
	} else
	    unlink(buf2);
    }
    if (rmdir(buf) < 0)
	goto VD_RECURSIVE_RMDIR_ERR;

    closedir(dp);
    return (0);

VD_RECURSIVE_RMDIR_ERR:
    closedir(dp);
    return (-1);
}

/*
 * These functions test specified path.
 * Isarchive:
 *	tests whether it's an archive?
 * Isvdir:
 *	tests whether it's in the virtual directory?
 */
int Isarchive(path)
char *path;
{
    int ftype;

    if ((ftype = vd_ftype(path)) < 0)
	return 0;

    if (ftype == RFT_COMPRESS)
	if (!(ftype = vd_compp(path, NULL)))
	    return 0;

    return ftype;
}

int Isvdir(path)
char *path;
{
    int rv = 0;
    char tmp1[MAXPATHLEN+1], tmp2[MAXPATHLEN+1];
    int archive1, archive2;

    strcpy(tmp1, path);
    strcpy(tmp2, path);

    vd_optimize_path(tmp1);
    Dirtovd(tmp2);

    archive1 = Isarchive(tmp1);
    archive2 = Isarchive(tmp2);

    if (strcmp(tmp1, tmp2)) {
	char tmp3[MAXPATHLEN+1], tmp4[MAXPATHLEN+1];
	int archive3, archive4;

	sprintf(tmp3, "%s%s", vdroot, tmp1);
	strcpy(tmp4, tmp2+strlen(vdroot));

	archive3 = Isarchive(tmp3);
	archive4 = Isarchive(tmp4);

	if (archive4 && !strcmp(tmp1, tmp4)) {
	    rv |= 06;
	    return rv;
	}
	rv |= 01;
	if (archive2)
	    rv |= 02;
	else if (archive4)
	    rv |= 06;
	return rv;
    }
    if (archive1)
	rv |= 02;

    return rv;
}

/*
 * This function optimizes given path.
 * Expand '~' to home directory and removes '.', and treat '..'.
 */
static void vd_optimize_path(path)
char *path;
{
    char *tmp, *reserve;

    if (!strcmp(path, STDINSTR))
	return;

    if (*path == '\0') {
	xv_getwd(path, MAXPATHLEN+1);
	return;
    }
    if (*path == '~')
	Globify(path);
    if (*path != '/') {
	char tmp[MAXPATHLEN+1];

	strcpy(tmp, path);
	xv_getwd(path, MAXPATHLEN+1);
	strcat(path, "/");
	strcat(path, tmp);
    }

    reserve = tmp = path;
    while(*path != '\0') {
	if (*path == '/') {
	    *tmp++ = *path;
	    while (*++path == '/')
		;
	    continue;
	}
	if ((*path == '.') && (*(path-1) == '/')) {
	    if (*(path+1) == '/') {
		tmp--;
		path++;
		continue;
	    } else if (*(path+1) == '\0') {
		tmp--;
		break;
	    } else if (*(path+1) == '.') {
		if (*(path+2) == '/') {
		    if ((tmp - reserve) > 1)
			for (tmp-=2; (*tmp != '/'); tmp--)
			    ;
		    else
			tmp = reserve;
		    path+=2;
		    continue;
		} else if (*(path+2) == '\0') {
		    if ((tmp - reserve) > 1)
			for (tmp-=2; (*tmp != '/'); tmp--)
			    ;
		    else
			tmp = reserve+1;
		    break;
		}
	    }
	}
	*tmp++ = *path++;
    }
    if (((tmp - reserve) > 1) && *(tmp-1) == '/')
	tmp--;
    if (tmp == reserve)
	*tmp++ = '/';

    *tmp = '\0';
}

/*
 * These functions detect file type.
 */
static int vd_ftype(fname)
char *fname;
{
    /* check archive type */

    FILE *fp;
    byte  magicno[30];    /* first 30 bytes of file */
    int   rv, n;
    struct stat st;

    if (!fname) return VD_ERR;   /* shouldn't happen */

    if ((!stat(fname, &st)) && (st.st_mode & S_IFMT) == S_IFDIR)
	return VD_UKN;
    fp = xv_fopen(fname, "r");
    if (!fp) return VD_ERR;

    n = fread(magicno, (size_t) 1, (size_t) 30, fp);
    fclose(fp);

    if (n<30) return VD_UKN;    /* files less than 30 bytes long... */

    rv = VD_UKN;

    if (magicno[0] == 0x60 && magicno[1]==0xea) rv = VD_ARJ;

    else if (magicno[2] == '-' && magicno[3] == 'l' &&
	     magicno[4] == 'h') rv = VD_LZH;

    else if (strncmp((char *) magicno,"PK", (size_t) 2)==0) rv = VD_ZIP;

    else if (magicno[20]==0xdc && magicno[21]==0xa7 &&
	     magicno[22]==0xc4 && magicno[23]==0xfd) rv = VD_ZOO;

    else if (vd_tarc(fname)) rv = VD_TAR;

    else if (magicno[0]==0x1f && magicno[1]==0x9d) rv = RFT_COMPRESS;

    else if (!strncmp((char *) &magicno[11], "MAJYO", (size_t) 5))
	     rv = VD_UKN; /* XXX */

    else if (magicno[0] == 26) rv = VD_ARC;

#ifdef GUNZIP
    else if (magicno[0]==0x1f && magicno[1]==0x8b) rv = RFT_COMPRESS;/* gzip */
    else if (magicno[0]==0x1f && magicno[1]==0x9e) rv = RFT_COMPRESS;/*  old */
    else if (magicno[0]==0x1f && magicno[1]==0x1e) rv = RFT_COMPRESS;/* pack */
#endif

    return rv;
}

static int vd_compp(path, newpath)
char *path, *newpath;
{
    /*
     * uncompress and check archive type.
     *
     * If newpath is NULL, uncompress only 512 byte of 'path' and
     * check archive type, so it is for SPEED-UP strategy.
     * In this case, caller this function does not have to unlink
     * tempoary file.
     * Unfortunately it does not work in VMS system.
     */

    int file_type, r;
    char uncompname[128], basename[128];
    int comptype;

    if (newpath) *newpath = '\0';
    strncpy(basename, path, 127);
    comptype = ReadFileType(path);
#if (defined(VMS) && !defined(GUNZIP))
    /* VMS decompress doesn't like the file to have a trailing .Z in fname
    however, GUnZip is OK with it, which we are calling UnCompress */
    *rindex (basename, '.') = '\0';
#endif
#ifdef VMS
    if (UncompressFile(basename, uncompname)) {
#else
    if (newpath == NULL)
	r = vd_UncompressFile(basename, uncompname);
    else
	r = UncompressFile(basename, uncompname, comptype);
    if (r) {
#endif
	if ((file_type = vd_ftype(uncompname)) < 0) {
	    unlink(uncompname);
	    return 0;
	}
	if (newpath) strcpy(newpath, uncompname);
	else unlink(uncompname);
    } else {
	return 0;
    }
    return file_type;
}

#define HEADERSIZE 512

static void  vd_Dirtovd    		PARM((char *));
static int   stderr_on			PARM((void));
static int   stderr_off			PARM((void));
static FILE  *popen_nul			PARM((char *, char *));

static int vd_UncompressFile(name, uncompname)
char *name, *uncompname;
{
    /* Yap, I`m nearly same as original `UncompnameFile' function, but,
       1) I extract `name' file ONLY first 512 byte.
       2) I'm called only from UNIX and UNIX like OS, *NOT* VMS */
    /* returns '1' on success, with name of uncompressed file in uncompname
       returns '0' on failure */

    char namez[128], *fname, buf[512], tmp[HEADERSIZE];
    int n, tmpfd;
    FILE *pfp, *tfp;

    fname = name;
    namez[0] = '\0';


#ifndef GUNZIP
    /* see if compressed file name ends with '.Z'.  If it *doesn't*, we need
       to temporarily rename it so it *does*, uncompress it, and rename it
       *back* to what it was.  necessary because uncompress doesn't handle
       files that don't end with '.Z' */

    if (strlen(name) >= (size_t) 2            &&
	strcmp(name + strlen(name)-2,".Z")!=0 &&
	strcmp(name + strlen(name)-2,".z")!=0) {
	strcpy(namez, name);
	strcat(namez,".Z");

	if (rename(name, namez) < 0) {
	    sprintf(buf, "Error renaming '%s' to '%s':  %s",
		    name, namez, ERRSTR(errno));
	    ErrPopUp(buf, "\nBummer!");
	    return 0;
	}

	fname = namez;
    }
#endif   /* not GUNZIP */

    sprintf(uncompname, "%s/xvuXXXXXX", tmpdir);
#ifdef USE_MKSTEMP
    tmpfd = mkstemp(uncompname);
#else
    mktemp(uncompname);
#endif

    sprintf(buf,"%s -c %s", UNCOMPRESS, fname);
    SetISTR(ISTR_INFO, "Uncompressing Header '%s'...", BaseName(fname));
    if ((pfp = popen_nul(buf, "r")) == NULL) {
	SetISTR(ISTR_INFO, "Cannot extract for archive '%s'.",
		BaseName(fname));
	Warning();
#ifdef USE_MKSTEMP
	if (tmpfd >= 0)
	    close(tmpfd);
#endif
	return 0;
    }
#ifdef USE_MKSTEMP
    if (tmpfd < 0)
#else
    if ((tmpfd = open(uncompname,O_WRONLY|O_CREAT|O_EXCL,S_IRWUSR)) < 0)
#endif
    {
	SetISTR(ISTR_INFO, "Unable to create temporary file.",
		BaseName(uncompname));
	Warning();
	pclose(pfp);
    }
    if ((tfp = fdopen(tmpfd, "w")) == NULL) {
	SetISTR(ISTR_INFO, "Unable to create temporary file.",
		BaseName(uncompname));
	Warning();
	close(tmpfd);
	pclose(pfp);
	return 0;
    }
    if ((n = fread(tmp, 1, sizeof(tmp), pfp)) != HEADERSIZE) {
	SetISTR(ISTR_INFO, "Unable to read '%s'.",
		BaseName(fname));
	Warning();
	pclose(pfp);
	fflush(tfp);
	fclose(tfp);
	close(tmpfd);
	return 0;
    }
    fwrite(tmp, 1, n, tfp);
    fflush(tfp);
    fclose(tfp);
    close(tmpfd);
    pclose(pfp);

    /* if we renamed the file to end with a .Z for the sake of 'uncompress',
       rename it back to what it once was... */

    if (strlen(namez)) {
	if (rename(namez, name) < 0) {
	    sprintf(buf, "Error renaming '%s' to '%s':  %s",
		    namez, name, ERRSTR(errno));
	    ErrPopUp(buf, "\nBummer!");
	}
    }

    return 1;
}

#define TARBLOCK 512
#define CKSTART 148 /* XXX */
#define CKSIZE 8

/*
 * Tar file: 1, other: 0
 */
static int vd_tarc(fname)
char *fname;
{
    FILE *fp;
    unsigned int sum;
    char *ckp, buf[TARBLOCK];

    if ((fp = fopen(fname, "r")) == NULL)
	return 0;

    fread(buf, TARBLOCK, 1, fp);
    fclose(fp);

    for (sum = 0, ckp = buf + CKSTART;
	 (ckp < buf + CKSTART + CKSIZE) && *ckp != '\0';
	 ckp++) {
	sum *= 8;
	if (*ckp == ' ')
	    continue;
	if (*ckp < '0' || '7' < *ckp)
	    return 0;
	sum += *ckp - '0';
    }
    if (sum != vd_tar_sumchk(buf))
	return 0;

    return 1;
}

static unsigned int vd_tar_sumchk(buf)
char *buf;
{
    int i;
    unsigned int sum = 0;

    for (i = 0; i < CKSTART; i++) {
	sum += *(buf + i);
    }
    sum += ' ' * 8;
    for (i += 8; i < TARBLOCK; i++) {
	sum += *(buf + i);
    }
    return sum;
}


static int stde = -1;        /*  fd of stderr    */
static int nul = -1;         /*  fd of /dev/null */

/*
 * switch off the output to stderr(bypass to /dev/null).
 */
static int stderr_off()
{
    if (nul < 0)
      nul = open("/dev/null", O_RDONLY);
    if (nul < 0) {
	fprintf(stderr, "/dev/null open failure\n");
	return -1;
    }
    if (stde < 0)
	stde = dup(2);
    if (stde < 0) {
	fprintf(stderr, "duplicate stderr failure\n");
	return -1;
    }
    close(2);
    dup(nul);
    return 0;
}

/*
 * turn on stderr output.
 */
static int stderr_on()
{
    if ((stde < 0) || (nul < 0)) {
	fprintf(stderr, "stderr_on should call after stderr_off\n");
	return -1;
    }
    close(2);
    dup(stde);
    return 0;
}

/*
 * popen with no output to stderr.
 */
static FILE *popen_nul(prog, mode)
char *prog, *mode;
{
    FILE *fp;

    stderr_off();
    fp = popen(prog, mode);
    stderr_on();
    return fp;
}

/*
 * These functions are for SIGNAL.
 * If XV end by C-c, there are dust of directory which name is .xvvd???,
 * made by xvvd. Then, I handle SIGINT, and add good finish.
 */
void vd_HUPhandler()
{
#if defined(SYSV) || defined(SVR4) || defined(__USE_XOPEN_EXTENDED)
    sighold(SIGHUP);
#else
    int mask;
    mask = sigblock(sigmask(SIGHUP));
#endif

  Vdsettle();

#if defined(SYSV) || defined(SVR4) || defined(__USE_XOPEN_EXTENDED)
    sigrelse(SIGHUP);
    signal(SIGHUP, (void (*)PARM((int))) vd_HUPhandler);
#else
    sigsetmask(mask);
#endif
}

void vd_handler(sig)
int sig;
{
#if defined(SYSV) || defined(SVR4) || defined(__USE_XOPEN_EXTENDED)
    sighold(sig);
#else
    sigblock(sigmask(sig));
#endif

    Quit(1); /*exit(1);*/
}

int vd_Xhandler(disp,event)
Display *disp;
XErrorEvent *event;
{
    Quit(1); /*exit(1);*/
    return (1); /* Not reached */
}

int vd_XIOhandler(disp)
Display *disp;
{
    fprintf(stderr, "XIO  fatal IO error ? (?) on X server\n");
    fprintf(stderr, "You must exit normally in xv usage.\n");
    Quit(1); /*exit(1);*/
    return (1); /* Not reached */
}

void vd_handler_setup()
{
    signal(SIGHUP, (void (*)PARM((int))) vd_HUPhandler);
    signal(SIGINT, (void (*)PARM((int))) vd_handler);
    signal(SIGTERM,(void (*)PARM((int))) vd_handler);

    (void)XSetErrorHandler(vd_Xhandler);
    (void)XSetIOErrorHandler(vd_XIOhandler);
}
#endif /* AUTO_EXPAND */
