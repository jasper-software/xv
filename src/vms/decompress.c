/* 
 *  D e c o m p r e s s   
 *
 *  Data decompression program for LZW compression
 *
 */

/* Get the usual include files */

#include <stdio.h>
#include <ctype.h>
#include <types.h>
#include <stat.h>

/* Define some useful constants */

#define BITS          16
#define HSIZE      69001            /* 95% occupancy */
#define BIT_MASK    0x1F
#define INIT_BITS      9            /* initial number of bits/code */
#define BLOCK_MASK  0x80
#define FIRST        257            /* first free entry */
#define CLEAR        256            /* table clear output code */
#ifndef VMS
#define GOOD_EXIT      0
#define BAD_EXIT       1
#else
#define GOOD_EXIT      1
#define BAD_EXIT       0
#endif

/* Define some useful macros */

#define MAXCODE(n_bits)     ((1 << (n_bits)) - 1)
#define htabof(i)           htab[i]
#define codetabof(i)        codetab[i]
#define tab_prefixof(i)     codetabof (i)
#define tab_suffixof(i)     ((char_type *) (htab))[i]
#define de_stack            ((char_type *) &tab_suffixof (1 << BITS))

/* Set up our typedefs */

typedef long int code_int;
typedef long int count_int;
typedef unsigned char char_type;

/* Declare the global variables */

char_type magic_header[] = {"\037\235"};        /* 1F 9D */
char_type rmask[9] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};

int n_bits;                             /* number of bits/code */
int maxbits = BITS;                     /* user settable max # bits/code */
code_int maxcode;                       /* maximum code, given n_bits */
code_int maxmaxcode = 1 << BITS;        /* should NEVER generate this code */

count_int htab [HSIZE];
unsigned short codetab [HSIZE];

code_int free_ent = 0;                  /* first unused entry */

/* Define our function prototypes */

code_int getcode ( );

/*
 *  Block compression parameters -- after all codes are used up,
 *  and compression rate changes, start over.
 *
 */

int block_compress = BLOCK_MASK;
int clear_flg = 0;
char ofname [100];

/*
 *  m a i n
 *
 *  Algorithm from "A Technique for High Performance Data Compression",
 *  Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 *  Algorithm:
 *
 *      Modified Lempel-Ziv method (LZW).  Basically finds common
 *  substrings and replaces them with a variable size code.  This is
 *  deterministic, and can be done on the fly.  Thus, the decompression
 *  procedure needs no input table, but tracks the way the table was built.
 *
 */

int main (argc, argv)

int argc; 
char *argv[];

{
    char tempname[100];
    char *fileptr;

    if (argc != 2) {
        printf ("Usage: decompress filename.\n");
        exit (BAD_EXIT);
    }

    /* Get input file (no extension) */

    fileptr = argv[1];     

    /* Add .Z suffix */

    strcpy (tempname, fileptr);
    strcat (tempname, ".Z");
    fileptr = tempname;
                
    /* Open input file */

    if ((freopen (fileptr, "r", stdin)) == NULL) {
        perror (fileptr);
        exit (BAD_EXIT);
    }

    /* Check the magic number */

    if ((getchar ( ) != (magic_header[0] & 0xFF)) ||
        (getchar ( ) != (magic_header[1] & 0xFF))) {
        fprintf (stderr, "%s: not in compressed format\n", fileptr);
        exit (BAD_EXIT);
    }

    maxbits = getchar ( );        /* set bits from file */
    block_compress = maxbits & BLOCK_MASK;
    maxbits &= BIT_MASK;
    maxmaxcode = 1 << maxbits;

    if (maxbits > BITS) {
        fprintf (stderr, "%s: compressed with %d bits, can only handle %d bits\n",
          fileptr, maxbits, BITS);
        exit (BAD_EXIT);
    }

    /* Generate output filename */

    strcpy (ofname, fileptr);
    ofname[strlen (fileptr) - 2] = '\0';  /* Strip off .Z */
    
    /* Open output file */

    if (freopen (ofname, "w", stdout) == NULL) {
        perror (ofname);
        exit (BAD_EXIT);
    }

    /* Actually do the decompression */

    decompress ( );

    fclose (stdout);
    exit (GOOD_EXIT);
}

writeerr ( )
{
    perror (ofname);
    exit (BAD_EXIT);
}

/*
 *  d e c o m p r e s s
 *
 *  Decompress stdin to stdout.  This routine adapts to the codes in the
 *  file building the "string" table on-the-fly; requiring no table to
 *  be stored in the compressed file.  
 *
 */

decompress ( ) 
{
    register char_type *stackp;
    register int finchar;
    register code_int code, oldcode, incode;

    /* Initialize the first 256 entries in the table */

    maxcode = MAXCODE (n_bits = INIT_BITS);

    for (code = 255; code >= 0; code--) {
        tab_prefixof (code) = 0;
        tab_suffixof (code) = (char_type) code;
    }

    free_ent = ((block_compress) ? FIRST : 256);

    finchar = oldcode = getcode ( );

    if (oldcode == -1)                  /* EOF already? */
        return;                         /* Get out of here */

    putchar ((char) finchar);           /* first code must be 8 bits = char */

    if (ferror (stdout))                /* Crash if can't write */
        writeerr ( );

    stackp = de_stack;

    while ((code = getcode ( )) > -1) {

        if ((code == CLEAR) && block_compress) {

            for (code = 255; code >= 0; code--)
                tab_prefixof (code) = 0;

            clear_flg = 1;
            free_ent = FIRST - 1;

            if ((code = getcode ( )) == -1)     /* O, untimely death! */
                break;
        }

        incode = code;

        /* Special case for KwKwK string */

        if (code >= free_ent) {
            *stackp++ = finchar;
            code = oldcode;
        }

        /* Generate output characters in reverse order */

        while (code >= 256) {
            *stackp++ = tab_suffixof (code);
            code = tab_prefixof (code);
        }

        *stackp++ = finchar = tab_suffixof (code);

        /* And put them out in forward order */

        do {
            putchar (*--stackp);
        } while (stackp > de_stack);

        /* Generate the new entry */

        if ((code = free_ent) < maxmaxcode) {
            tab_prefixof (code) = (unsigned short) oldcode;
            tab_suffixof (code) = finchar;
            free_ent = code + 1;
        } 

        /* Remember previous code */

        oldcode = incode;
    }

    fflush (stdout);

    if (ferror (stdout))
        writeerr ( );
}

/*
 *  g e t c o d e
 *
 *  Read one code from the standard input.  If EOF, return -1.
 *
 */

code_int getcode ( ) 
{
    register code_int code;
    static int offset = 0, size = 0;
    static char_type buf[BITS];
    register int r_off, bits;
    register char_type *bp = buf;

    if (clear_flg > 0 || offset >= size || free_ent > maxcode) {

        /*
         * If the next entry will be too big for the current code
         * size, then we must increase the size.  This implies reading
         * a new buffer full, too.
         *
         */

        if (free_ent > maxcode) {
            n_bits++;

            if (n_bits == maxbits)
                maxcode = maxmaxcode;   /* Won't get any bigger now */
            else
                maxcode = MAXCODE (n_bits);
        }

        if (clear_flg > 0) {
            maxcode = MAXCODE (n_bits = INIT_BITS);
            clear_flg = 0;
        }

        size = fread (buf, 1, n_bits, stdin);

        if (size <= 0)
            return (-1);                /* End of file */

        offset = 0;

        /* Round size down to an integral number of codes */

        size = (size << 3) - (n_bits - 1);
    }

    r_off = offset;
    bits = n_bits;

    /* Get to the first byte */

    bp += (r_off >> 3);
    r_off &= 7;

    /* Get first part (low order bits) */

    code = (*bp++ >> r_off);
    bits -= (8 - r_off);
    r_off = 8 - r_off;                  /* Now, offset into code word */

    /* Get any 8 bit parts in the middle (<=1 for up to 16 bits) */

    if (bits >= 8) {
        code |= *bp++ << r_off;
        r_off += 8;
        bits -= 8;
    }

    /* Handle the high order bits */

    code |= (*bp & rmask[bits]) << r_off;
    offset += n_bits;

    return (code);
}
