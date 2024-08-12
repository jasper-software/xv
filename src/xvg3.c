/* xvg3.c - load in a Group 3 FAX file for XV
 *
 * This is simply a modified g3topbm.c.
 * Modified by Todd_Atkins@EE-CF.Stanford.EDU
 */

/* g3topbm.c - read a Group 3 FAX file and produce a portable bitmap
**
** Copyright (C) 1989 by Paul Haeberli <paul@manray.sgi.com>.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "xv.h"

#ifdef HAVE_G3

#include "xvg3.h"

#if __STDC__
#define ARGS(alist) alist
#else /*__STDC__*/
#define ARGS(alist) ()
#define const
#endif /*__STDC__*/

#define WHITE 0
#define BLACK 1

#define TABSIZE(tab) (sizeof(tab)/sizeof(struct tableentry))
#define MAXCOLS 1728
#define MAXROWS 4300	/* up to two pages long */

static int endoffile;
static int eols;
static int rawzeros;
static int shdata;
static int shbit;
static int kludge;
static int reversebits;

#define WHASHA 3510
#define WHASHB 1178

#define BHASHA 293
#define BHASHB 2695

#define HASHSIZE 1021
static tableentry* whash[HASHSIZE];
static tableentry* bhash[HASHSIZE];

static int addtohash ARGS(( tableentry* hash[], tableentry* te, int n, int a, int b ));
static tableentry* hashfind ARGS(( tableentry* hash[], int length, int code, int a, int b ));
static int getfaxrow ARGS(( FILE* inf, int row, byte* bitrow ));
static void skiptoeol ARGS(( FILE* file ));
static int rawgetbit ARGS(( FILE* file ));

extern int lowresfax;
extern int highresfax;

int
LoadG3 (char *fname, PICINFO *pinfo)
{
    FILE *fp;
    int rows, last_allocated_row, cols, row, col, i;
    byte* bytes[MAXROWS];
    byte* bp;

    endoffile = 0;
    eols = 0;
    rawzeros = 0;
    shdata = 0;
    shbit = 0;
    kludge = 0;
    reversebits = 0;

    pinfo->pic = (byte *) NULL;
    pinfo->comment = (char *) NULL;
    pinfo->numpages = 0;

    if ( (fp = fopen(fname, "r")) == NULL )
	{
	SetISTR(ISTR_WARNING, "Cannot open %s", fname);
	return 0;
	}

    eols = 0;

    SetISTR(ISTR_INFO,"Loading Fax(G3) hash tables ...");

    if ( kludge )
	{
	/* Skip extra lines to get in sync. */
	skiptoeol( fp );
	skiptoeol( fp );
	skiptoeol( fp );
	}
    skiptoeol( fp );
    for ( i = 0; i < HASHSIZE; ++i )
	whash[i] = bhash[i] = (tableentry*) 0;
    if (addtohash( whash, twtable, TABSIZE(twtable), WHASHA, WHASHB ))
	return 0;
    if (addtohash( whash, mwtable, TABSIZE(mwtable), WHASHA, WHASHB ))
	return 0;
    if (addtohash( whash, extable, TABSIZE(extable), WHASHA, WHASHB ))
	return 0;
    if (addtohash( bhash, tbtable, TABSIZE(tbtable), BHASHA, BHASHB ))
	return 0;
    if (addtohash( bhash, mbtable, TABSIZE(mbtable), BHASHA, BHASHB ))
	return 0;
    if (addtohash( bhash, extable, TABSIZE(extable), BHASHA, BHASHB ))
	return 0;
 
    SetISTR(ISTR_INFO,"Reading Fax(G3) image...");

    cols = 0;
    bytes[0] = NULL;
    rows = 0;
    for ( ; ; ) {
	if ((bytes[rows] = (byte*) 
               malloc(MAXCOLS * sizeof(byte))) == (byte*) NULL)
	    return 0;
	col = getfaxrow( fp, rows, bytes[rows] );
	if ( endoffile )
	    break;
	if ( col > cols )
	    cols = col;
	if ( lowresfax ) {
	    if ( rows + 1 >= MAXROWS ) {
		/* invalid file */
	        break;
	    }
	    bytes[rows + 1] = bytes[rows];
	    ++rows;
	}
	if ( rows + 1 >= MAXROWS ) {
	    break;
	}
	rows++;
    }

    fclose( fp );

    /* JPD / routines seem to produce 2 rows in excess ... */
    last_allocated_row = rows;
    rows -= 2;

    if ((pinfo->pic = (byte *) malloc( rows*cols*sizeof(byte) ))==(byte*) NULL)
	return 0;

    for ( row = 0, bp = pinfo->pic; row < rows; ++row, bp += cols )
	memcpy(bp, bytes[row], cols);	

    for ( row = 0; row <= last_allocated_row; ++row ) {
	if (row==0 || (row>0 && bytes[row]!=bytes[row-1])) free(bytes[row]);
    }

    pinfo->frmType = F_G3;
    pinfo->type = PIC8;
    pinfo->colType = F_BWDITHER;
    pinfo->w = cols;
    pinfo->h = rows;
    pinfo->r[0] = pinfo->g[0] = pinfo->b[0] = 255;
    pinfo->r[1] = pinfo->g[1] = pinfo->b[1] = 0;
    sprintf(pinfo->shrtInfo, "Fax G3 %dx%d", cols, rows);
    sprintf(pinfo->fullInfo, "Fax G3 %dx%d, black & white image", cols, rows);
    return 1;
}

static int
addtohash(tableentry **hash, tableentry *te, int n, int a, int b)
{
	unsigned int pos;

	while (n--) {
		pos = ((te->length+a)*(te->code+b))%HASHSIZE;
		if (hash[pos] != 0) {
			SetISTR(ISTR_WARNING,"LoadG3 internal error: addtohash fatal hash collision" );
			return 0;
		}
		hash[pos] = te;
		te++;
	}
	return 0;
}

static tableentry*
hashfind(tableentry **hash, int length, int code, int a, int b)
{
    unsigned int pos;
    tableentry* te;

    pos = ((length+a)*(code+b))%HASHSIZE;
    if (pos >= HASHSIZE) {
	SetISTR(ISTR_WARNING,
	    "LoadG3 internal error: bad hash position, length %d code %d pos %d",
	    length, code, pos );
        return 0;
        }
    te = hash[pos];
    return ((te && te->length == length && te->code == code) ? te : 0);
}

static int
getfaxrow(FILE *inf, int row, byte *bitrow)
{
	int col;
	byte* bP;
	int curlen, curcode, nextbit;
	int count, color;
	tableentry* te;

	for ( col = 0, bP = bitrow; col < MAXCOLS; ++col, ++bP )
	    *bP = WHITE;
	col = 0;
	rawzeros = 0;
	curlen = 0;
	curcode = 0;
	color = 1;
	count = 0;
	while (!endoffile) {
		if (col >= MAXCOLS) {
			skiptoeol(inf);
			return (col); 
		}
		do {
			if (rawzeros >= 11) {
				nextbit = rawgetbit(inf);
				if (nextbit) {
					if (col == 0)
						/* XXX should be 6 */
						endoffile = (++eols == 3);
					else
						eols = 0;
#ifdef notdef
					if (col && col < 1728)
						SetISTR(ISTR_WARNING,
					       "LoadG3: warning, row %d short (len %d)",
						    row, col );
#endif /*notdef*/
					return (col); 
				}
			} else
				nextbit = rawgetbit(inf);
			curcode = (curcode<<1) + nextbit;
			curlen++;
		} while (curcode <= 0);
		if (curlen > 13) {
			SetISTR(ISTR_WARNING,
	  "bad code word at row %d, col %d (len %d code 0x%x), skipping to EOL",
			    row, col, curlen, curcode, 0 );
			skiptoeol(inf);
			return (col);
		}
		if (color) {
			if (curlen < 4)
				continue;
			te = hashfind(whash, curlen, curcode, WHASHA, WHASHB);
		} else {
			if (curlen < 2)
				continue;
			te = hashfind(bhash, curlen, curcode, BHASHA, BHASHB);
		}
		if (!te)
			continue;
		switch (te->tabid) {
		case TWTABLE:
		case TBTABLE:
			count += te->count;
			if (col+count > MAXCOLS) 
				count = MAXCOLS-col;
			if (count > 0) {
				if (color) {
					col += count;
					count = 0;
				} else {
					for ( ; count > 0; --count, ++col )
						bitrow[col] = BLACK;
				}
			}
			curcode = 0;
			curlen = 0;
			color = !color;
			break;
		case MWTABLE:
		case MBTABLE:
			count += te->count;
			curcode = 0;
			curlen = 0;
			break;
		case EXTABLE:
			count += te->count;
			curcode = 0;
			curlen = 0;
			break;
		default:
			SetISTR(ISTR_WARNING, "internal bad poop" );
			return (0);
		}
	}
	return (0);
}

static void
skiptoeol(FILE *file)
{
    while ( rawzeros < 11 )
	(void) rawgetbit( file );
    for ( ; ; )
	{
	if ( rawgetbit( file ) )
	    break;
	}
    }

static int
rawgetbit(FILE *file)
{
    int b;

    if ( ( shbit & 0xff ) == 0 )
	{
	shdata = getc( file );
	if ( shdata == EOF ) {
	    char errmsg[64];
	    sprintf(errmsg, "LoadG3: EOF / read error at line %d", eols);
            FatalError(errmsg);
	    return 0;
	    }
	shbit = reversebits ? 0x01 : 0x80;
	}
    if ( shdata & shbit )
	{
	rawzeros = 0;
	b = 1;
	}
    else
	{
	rawzeros++;
	b = 0;
	}
    if ( reversebits )
	shbit <<= 1;
    else
	shbit >>= 1;
    return b;
    }
#endif /* HAVE_G3 */

