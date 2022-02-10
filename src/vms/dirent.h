/*
**  Header file for VMS readdir() routines.
**  Written by Rich $alz, <rsalz@bbn.com> in August, 1990.
**  This code has no copyright.
**
**  You must #include <descrip.h> before this file.
*/

/* 12-NOV-1990 added d_namlen field -GJC@MITECH.COM */

    /* Data structure returned by READDIR(). */
struct dirent {
    char	d_name[100];		/* File name		*/
    int         d_namlen;
    int		vms_verscount;		/* Number of versions	*/
    int		vms_versions[20];	/* Version numbers	*/
};

    /* Handle returned by opendir(), used by the other routines.  You
     * are not supposed to care what's inside this structure. */
typedef struct _dirdesc {
    long			context;
    int				vms_wantversions;
    char			*pattern;
    struct dirent		entry;
    struct dsc$descriptor_s	pat;
} DIR;


#define rewinddir(dirp)		seekdir((dirp), 0L)


extern DIR		*opendir();
extern struct dirent	*readdir();
extern long		telldir();
extern void		seekdir();
extern void		closedir();
extern void		vmsreaddirversions();
