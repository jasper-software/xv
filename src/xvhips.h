/*
 * HIPL Picture Header Format Standard
 *
 * Michael Landy - 2/1/82
 */

#define XHEADER
#ifdef XHEADER
struct extended {
	char *name;
	char *vals;
	};
#endif

struct header {
	char	*orig_name;	/* The originator of this sequence */
	char	*seq_name;	/* The name of this sequence */
	int	num_frame;	/* The number of frames in this sequence */
	char	*orig_date;	/* The date the sequence was originated */
	int	rows;		/* The number of rows in each image */
	int	cols;		/* The number of columns in each image */
	int	bits_per_pixel;	/* The number of significant bits per pixel */
	int	bit_packing;	/* Nonzero if bits were packed contiguously */
	int	pixel_format;	/* The format of each pixel, see below */
	char	*seq_history;	/* The sequence's history of transformations */
	char	*seq_desc;	/* Descriptive information */
#ifdef XHEADER
	struct extended *xheader;
#endif
};

/*
 * Pixel Format Codes
 */

#define	PFBYTE	0		/* Bytes interpreted as integers (8 bits) */
#define PFSHORT	1		/* Short integers (2 bytes) */
#define PFINT	2		/* Integers (4 bytes) */
#define	PFFLOAT	3		/* Float's (4 bytes)*/
#define	PFCOMPLEX 4		/* 2 Float's interpreted as (real,imaginary) */
#define PFASCII	5		/* ASCII rep, with linefeeds after each row */
#define	PFDOUBLE 6		/* Double's (8 byte floats) */
#define	PFDBLCOM 7		/* Double complex's (2 Double's) */
#define PFQUAD	10		/* quad-tree encoding (Mimaging) */
#define PFQUAD1	11		/* quad-tree encoding */
#define PFBHIST	12		/* histogram of byte image (using ints) */
#define PFSPAN	13		/* spanning tree format */
#define PLOT3D	24		/* plot-3d format */
#define PFINTPYR 50		/* integer pyramid */
#define PFFLOATPYR 51		/* float pyramid */
#define PFPOLYLINE 100		/* 2D points */
#define PFCOLVEC 101		/* Set of RGB triplets defining colours */
#define PFUKOOA 102		/* Data in standard UKOOA format */
#define PFTRAINING 104		/* Set of colour vector training examples */
#define PFTOSPACE 105		/* TOspace world model data structure */
#define PFSTEREO 106		/* Stereo sequence (l, r, l, r, ...) */
#define PFRGPLINE 107		/* 2D points with regions */
#define PFRGISPLINE 108		/* 2D points with regions and interfaces */
#define PFCHAIN	200		/* Chain code encoding (Mimaging) */
#define PFLUT	300		/* LUT format (uses Ints) (Mimaging) */
#define PFAHC	400		/* adaptive hierarchical encoding */
#define PFOCT	401		/* oct-tree encoding */
#define	PFBT	402		/* binary tree encoding */
#define PFAHC3	403		/* 3-d adaptive hierarchical encoding */
#define PFBQ	404		/* binquad encoding */
#define PFRLED	500		/* run-length encoding */
#define PFRLEB	501		/* run-length encoding, line begins black */
#define PFRLEW	502		/* run-length encoding, line begins white */
#define PFPOLAR	600		/* rho-theta format (Mimaging) */

/*
 * Bit packing formats
 */

#define	MSBFIRST 1		/* bit packing - most significant bit first */
#define	LSBFIRST 2		/* bit packing - least significant bit first */

#define FBUFLIMIT 30000		/* increase this if you use large PLOT3D
					files */

/*
 * For general readability
 */

#ifndef TRUE
# define	TRUE	1
#endif

#ifndef FALSE
# define	FALSE	0
#endif

typedef	long	Boolean;
extern char *strsave(), *memalloc();

/*
 * image and pyramid type declarations for the pyramid routines.
 *
 * The pyramid utilities are derived from code originally written by
 * Raj Hingorani at SRI/David Sarnoff Research Institute.  The original
 * Gaussian and Laplacian pyramid algorithms were designed by Peter Burt (also
 * currently at SRI/DSRC).  See:  Computer Graphics and Image Processing,
 * Volume 16, pp. 20-51, 1981, and IEEE Transactions on Communications,
 * Volume COM-31, pp. 532-540, 1983.
 */

#define MAXLEV 12


typedef struct {
   float **ptr;
   int nr;
   int nc;
} FIMAGE;

typedef struct {
   int **ptr;
   int nr;
   int nc;
} IIMAGE;

typedef FIMAGE FPYR[MAXLEV];
typedef IIMAGE IPYR[MAXLEV];

typedef struct {
   float *k;
   int taps2;		/* the number of taps from the center rightward,
				total number is 2*taps2-1 */
} FILTER;

/* function definitions */

float		**_read_fimgstr();
int		**_read_iimgstr();
float		**_alloc_fimage();
int		**_alloc_iimage();

/* image macros */

#ifndef MAX
# define MAX(A,B)  ((A) > (B) ? (A) : (B))
#endif /* MAX */
#ifndef MIN
# define MIN(A,B)  ((A) < (B) ? (A) : (B))
#endif /* MIN */
#ifndef ABS
# define ABS(A)    ((A) > 0 ? (A) : (-(A)))
#endif /* ABS */
#ifndef BETWEEN
# define BETWEEN(A,B,C) (((A) < (B)) ? (B) : (((A) > (C)) ? (C) : (A)))
#endif /* BETWEEN */
#ifndef SIGN
# define SIGN(A,B) (((B) > 0) ? (A) : (-(A)))
#endif /* SIGN */
