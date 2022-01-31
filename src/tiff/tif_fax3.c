/* $Header: /usr/people/sam/tiff/libtiff/RCS/tif_fax3.c,v 1.93 1994/09/29 23:46:58 sam Exp $ */

/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Sam Leffler
 * Copyright (c) 1991, 1992, 1993, 1994 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library.
 *
 * CCITT Group 3 and Group 4 Compression Support.
 */
#include "tiffiop.h"
#include "tif_fax3.h"
#define	G3CODES
#include "t4.h"
#define	G3STATES
#include "g3states.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
	Fax3BaseState b;
} Fax3DecodeState;

typedef struct {
	Fax3BaseState b;
	short	k;			/* #rows left that can be 2d encoded */
	short	maxk;			/* max #rows that can be 2d encoded */
} Fax3EncodeState;

static	int Fax3Decode1DRow(TIFF*, u_char*, uint32);
static	int Fax3Encode1DRow(TIFF*, u_char*, uint32);
static	int32 find0span(u_char*, int32, int32);
static	int32 find1span(u_char*, int32, int32);

void
TIFFModeCCITTFax3(TIFF* tif, int isClassF)
{
	if (isClassF)
		tif->tif_options |= FAX3_CLASSF;
	else
		tif->tif_options &= ~FAX3_CLASSF;
}

static u_char bitMask[8] =
    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
#define	isBitSet(sp)	((sp)->b.data & bitMask[(sp)->b.bit])

#define	is2DEncoding(tif) \
	(tif->tif_dir.td_group3options & GROUP3OPT_2DENCODING)
#define	fetchByte(tif, sp) \
	((tif)->tif_rawcc--, (sp)->b.bitmap[*(u_char *)(tif)->tif_rawcp++])
#define	isWordAligned(tif) \
	(((long) tif->tif_rawcp & 1) == 0)

#define	BITCASE(b)			\
    case b:				\
	code <<= 1;			\
	if (data & (1<<(7-b))) code |= 1;\
	len++;				\
	if (code > 0) { bit = b+1; break; }

/*
 * Skip over input until an EOL code is found.  The
 * value of len is passed as 0 except during error
 * recovery when decoding 2D data.  Note also that
 * we don't use the optimized state tables to locate
 * an EOL because we can't assume much of anything
 * about our state (e.g. bit position).
 */
static void
skiptoeol(TIFF* tif, int len)
{
	Fax3DecodeState *sp = (Fax3DecodeState *)tif->tif_data;
	int bit = sp->b.bit;
	int data = sp->b.data;
	int code = 0;

	/*
	 * Our handling of ``bit'' is painful because
	 * the rest of the code does not maintain it as
	 * exactly the bit offset in the current data
	 * byte (bit == 0 means refill the data byte).
	 * Thus we have to be careful on entry and
	 * exit to insure that we maintain a value that's
	 * understandable elsewhere in the decoding logic.
	 */
	if (bit == 0)			/* force refill */
		bit = 8;
	for (;;) {
		switch (bit) {
	again:	BITCASE(0);
		BITCASE(1);
		BITCASE(2);
		BITCASE(3);
		BITCASE(4);
		BITCASE(5);
		BITCASE(6);
		BITCASE(7);
		default:
			if (tif->tif_rawcc <= 0)
				return;
			data = fetchByte(tif, sp);
			goto again;
		}
		if (len >= 12 && code == EOL)
			break;
		code = len = 0;
	}
	sp->b.bit = bit > 7 ? 0 : bit;	/* force refill */
	sp->b.data = data;
}

/*
 * Return the next bit in the input stream.  This is
 * used to extract 2D tag values and the color tag
 * at the end of a terminating uncompressed data code.
 */
static int
nextbit(TIFF* tif)
{
	Fax3DecodeState *sp = (Fax3DecodeState *)tif->tif_data;
	int bit;

	if (sp->b.bit == 0 && tif->tif_rawcc > 0)
		sp->b.data = fetchByte(tif, sp);
	bit = isBitSet(sp);
	if (++(sp->b.bit) > 7)
		sp->b.bit = 0;
	return (bit);
}

/*
 * Setup G3-related compression/decompression
 * state before data is processed.  This routine
 * is called once per image -- it sets up different
 * state based on whether or not 2D encoding is used.
 */
static void *
Fax3SetupState(TIFF* tif, int space)
{
	TIFFDirectory *td = &tif->tif_dir;
	Fax3BaseState *sp;
	size_t cc = space;
	long rowbytes, rowpixels;

	if (td->td_bitspersample != 1) {
		TIFFError(tif->tif_name,
		    "Bits/sample must be 1 for Group 3/4 encoding/decoding");
		return (0);
	}
	/*
	 * Calculate the scanline/tile widths.
	 */
	if (isTiled(tif)) {
		rowbytes = TIFFTileRowSize(tif);
		rowpixels = tif->tif_dir.td_tilewidth;
	} else {
		rowbytes = TIFFScanlineSize(tif);
		rowpixels = tif->tif_dir.td_imagewidth;
	}
	if (is2DEncoding(tif) || td->td_compression == COMPRESSION_CCITTFAX4)
		cc += rowbytes+1;
	tif->tif_data = _TIFFmalloc(cc);
	if (tif->tif_data == NULL) {
		TIFFError("Fax3SetupState",
		    "%s: No space for Fax3 state block", tif->tif_name);
		return (0);
	}
	sp = (Fax3BaseState *)tif->tif_data;
	sp->rowbytes = (uint32) rowbytes;
	sp->rowpixels = (uint32) rowpixels;
	sp->bitmap = TIFFGetBitRevTable(tif->tif_fillorder != td->td_fillorder);
	if (is2DEncoding(tif) || td->td_compression == COMPRESSION_CCITTFAX4) {
		/*
		 * 2d encoding/decoding requires a scanline
		 * buffer for the ``reference line''; the
		 * scanline against which delta encoding
		 * is referenced.  The reference line must
		 * be initialized to be ``white'' (done elsewhere).
		 */
		sp->refline = (u_char *)tif->tif_data + space + 1;
		/*
		 * Initialize pixel just to the left of the
		 * reference line to white.  This extra pixel
		 * simplifies the edge-condition logic.
		 */
		sp->refline[-1] = 0x00;
	} else
		sp->refline = 0;
	return (sp);
}

/*
 * Setup state for decoding a strip.
 */
static
Fax3PreDecode(TIFF* tif)
{
	Fax3DecodeState *sp = (Fax3DecodeState *)tif->tif_data;

	if (sp == NULL) {
		sp = (Fax3DecodeState *)Fax3SetupState(tif, sizeof (*sp));
		if (!sp)
			return (0);
	}
	sp->b.bit = 0;			/* force initial read */
	sp->b.data = 0;
	sp->b.tag = G3_1D;
	if (sp->b.refline)
		_TIFFmemset(sp->b.refline, 0x00, sp->b.rowbytes);
	/*
	 * If image has EOL codes, they precede each line
	 * of data.  We skip over the first one here so that
	 * when we decode rows, we can use an EOL to signal
	 * that less than the expected number of pixels are
	 * present for the scanline.
	 */
	if ((tif->tif_options & FAX3_NOEOL) == 0) {
		skiptoeol(tif, 0);
		if (is2DEncoding(tif))
			/* tag should always be 1D! */
			sp->b.tag = nextbit(tif) ? G3_1D : G3_2D;
	}
	return (1);
}

#define	isAligned(p,t)	((((u_long)(p)) & (sizeof (t)-1)) == 0)
/*
 * The FILL macro must handle spans < 2*sizeof(long) bytes.
 * For machines with 64-bit longs this is <16 bytes; otherwise
 * this is <8 bytes.  We optimize the code here to reflect the
 * machine characteristics.
 */
#if defined(__alpha) || _MIPS_SZLONG == 64
#define FILL(n, cp)							    \
    switch (n) {							    \
    case 15:(cp)[14] = 0xff; case 14:(cp)[13] = 0xff; case 13: (cp)[12] = 0xff;\
    case 12:(cp)[11] = 0xff; case 11:(cp)[10] = 0xff; case 10: (cp)[9] = 0xff;\
    case  9: (cp)[8] = 0xff; case  8: (cp)[7] = 0xff; case  7: (cp)[6] = 0xff;\
    case  6: (cp)[5] = 0xff; case  5: (cp)[4] = 0xff; case  4: (cp)[3] = 0xff;\
    case  3: (cp)[2] = 0xff; case  2: (cp)[1] = 0xff;			      \
    case  1: (cp)[0] = 0xff; (cp) += (n); case 0:  ;			      \
    }
#else
#define FILL(n, cp)							    \
    switch (n) {							    \
    case 7: (cp)[6] = 0xff; case 6: (cp)[5] = 0xff; case 5: (cp)[4] = 0xff; \
    case 4: (cp)[3] = 0xff; case 3: (cp)[2] = 0xff; case 2: (cp)[1] = 0xff; \
    case 1: (cp)[0] = 0xff; (cp) += (n); case 0:  ;			    \
    }
#endif
static const unsigned char _fillmasks[] =
    { 0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

/*
 * Fill a span with ones.  Single byte fills are done
 * inline; otherwise _fillspan is called to do the
 * general fill work.
 */
#define	fillspan(cp, x, count) {			\
    if ((count) > 0) {					\
	char* _cp = (cp) + ((x)>>3);			\
	int32 _x = (x) & 7;				\
	if ((count) <= 8-_x)				\
	    _cp[0] |= _fillmasks[(count)]>>_x;		\
	else						\
	    _fillspan(_cp, x, count);			\
    }							\
}

/*
 * Handle fills that span a byte boundary.
 */
static void
_fillspan(char* cp, int32 x, int32 count)
{
	int32 n;

	if (x &= 7) {			/* align to byte boundary */
		*cp++ |= 0xff >> x;
		count -= 8-x;
	}
	if (n = count >> 3) {		/* multiple bytes to fill */
		if ((n/sizeof (long)) > 1) {
			int32 nw;
			long* lp;
			/*
			 * Align to longword boundary and fill longwords.
			 */
			for (; n && !isAligned(cp, long); n--)
				*cp++ = 0xff;
			lp = (long*) cp;
			nw = (int32)(n / sizeof (long));
			n -= nw * sizeof (long);
			do {
				*lp++ = -1L;
			} while (--nw);
			cp = (char*) lp;
		}
		FILL(n, cp);
		count &= 7;
	}
	*cp |= _fillmasks[count];
}
#undef	FILL

/*
 * Decode the requested amount of data.
 */
static
Fax3Decode(TIFF* tif, tidata_t buf, tsize_t occ, tsample_t s)
{
	Fax3DecodeState *sp = (Fax3DecodeState *)tif->tif_data;
	int status;

	_TIFFmemset(buf, 0, occ);	/* decoding only sets non-zero bits */
	while ((long)occ > 0) {
		if (sp->b.tag == G3_1D)
			status = Fax3Decode1DRow(tif, buf, sp->b.rowpixels);
		else
			status = Fax3Decode2DRow(tif, buf, sp->b.rowpixels);
		/*
		 * For premature EOF, stop decoding and return
		 * the buffer with the remainder white-filled.
		 */
		if (status == G3CODE_EOF)
			return (status);
		if (is2DEncoding(tif)) {
			/*
			 * Fetch the tag bit that indicates
			 * whether the next row is 1d or 2d
			 * encoded.  If 2d-encoded, then setup
			 * the reference line from the decoded
			 * scanline just completed.
			 */
			sp->b.tag = nextbit(tif) ? G3_1D : G3_2D;
			if (sp->b.tag == G3_2D)
				_TIFFmemcpy(sp->b.refline, buf, sp->b.rowbytes);
		}
		buf += sp->b.rowbytes;
		occ -= sp->b.rowbytes;
		if (occ != 0)
			tif->tif_row++;
	}
	return (1);
}

/*
 * Decode a code and return the associated run length.
 */
static int32
decode_run(TIFF* tif, /*const*/ u_short fsm[][256])
{
	Fax3DecodeState *sp = (Fax3DecodeState *)tif->tif_data;
	int state = sp->b.bit;
	int action;
	int32 runlen = 0;

	for (;;) {
		if (state == 0) {
	nextbyte:
			if (tif->tif_rawcc <= 0)
				return (G3CODE_EOF);
			sp->b.data = fetchByte(tif, sp);
		}
		state = fsm[state][sp->b.data];
		action = state >> 8; state &= 0xff;
		if (action == ACT_INCOMP)
			goto nextbyte;
		sp->b.bit = state;
		action -= ACT_RUNT;
		if (action < 0)			/* ACT_INVALID or ACT_EOL */
			return (action);
		if (action < 64)
			return (runlen + action);
		runlen += 64*(action-64);
	}
	/*NOTREACHED*/
}

#define	decode_white_run(tif)	decode_run(tif, TIFFFax1DFSM+0)
#define	decode_black_run(tif)	decode_run(tif, TIFFFax1DFSM+8)

/*
 * Process one row of 1d Huffman-encoded data.
 */
static int
Fax3Decode1DRow(TIFF* tif, u_char* buf, uint32 npels)
{
	Fax3DecodeState *sp = (Fax3DecodeState *)tif->tif_data;
	int32 x = 0;
	int32 runlen;
	static const char module[] = "Fax3Decode1D";

	for (;;) {
		runlen = decode_white_run(tif);
		if (runlen < 0)
			goto exception;
		if ((x += runlen) >= npels) {
			x = npels;
			goto done;
		}
		runlen = decode_black_run(tif);
		if (runlen < 0)
			goto exception;
		if (runlen > 0) {
			if (x+runlen >= npels) {
				fillspan((char*)buf, x, npels-x);
				x = npels;
				goto done;
			}
			fillspan((char*)buf, x, runlen);
			x += runlen;
		}
	}
exception:
	switch (runlen) {
	case G3CODE_EOF:
		TIFFWarning(module, "%s: Premature EOF at scanline %d (x %d)",
		    tif->tif_name, tif->tif_row, x);
		return (G3CODE_EOF);
	case G3CODE_INVALID:	/* invalid code */
		/*
		 * An invalid code was encountered.
		 * Flush the remainder of the line
		 * and allow the caller to decide whether
		 * or not to continue.  Note that this
		 * only works if we have a G3 image
		 * with EOL markers.
		 */
		TIFFError(module, "%s: Bad code word at scanline %d (x %d)",
		   tif->tif_name, tif->tif_row, x);
		break;
	case G3CODE_EOL:	/* premature end-of-line code */
		TIFFWarning(module, "%s: Premature EOL at scanline %d (x %d)",
		    tif->tif_name, tif->tif_row, x);
		return (1);	/* try to resynchronize... */
	}
done:
	/*
	 * Cleanup at the end of the row.  This convoluted
	 * logic is merely so that we can reuse the code with
	 * two other related compression algorithms (2 & 32771).
	 *
	 * Note also that our handling of word alignment assumes
	 * that the buffer is at least word aligned.  This is
	 * the case for most all versions of malloc (typically
	 * the buffer is returned longword aligned).
	 */
	if ((tif->tif_options & FAX3_NOEOL) == 0)
		skiptoeol(tif, 0);
	if (tif->tif_options & FAX3_BYTEALIGN)
		sp->b.bit = 0;
	if ((tif->tif_options & FAX3_WORDALIGN) && !isWordAligned(tif))
		(void) fetchByte(tif, sp);
	return (x == npels ? 1 : G3CODE_EOL);
}

/*
 * Group 3 2d Decoding support.
 */

/*
 * Return the next uncompressed mode code word.
 */
static int
decode_uncomp_code(TIFF* tif)
{
	Fax3DecodeState *sp = (Fax3DecodeState *)tif->tif_data;
	int code;

	do {
		if (sp->b.bit == 0 || sp->b.bit > 7) {
			if (tif->tif_rawcc <= 0)
				return (UNCOMP_EOF);
			sp->b.data = fetchByte(tif, sp);
		}
		code = TIFFFaxUncompFSM[sp->b.bit][sp->b.data];
		sp->b.bit = code & 0xff; code >>= 8;
	} while (code == ACT_INCOMP);
	return (code);
}

/*
 * Return the offset of the next bit in the range
 * [bs..be] that is different from the specified
 * color.  The end, be, is returned if no such bit
 * exists.
 */
#define	finddiff(_cp, _bs, _be, _color)	\
	(_bs + (_color ? find1span(_cp,_bs,_be) : find0span(_cp,_bs,_be)))

/*
 * Process one row of 2d encoded data.
 */
int
Fax3Decode2DRow(TIFF* tif, u_char* buf, uint32 npels)
{
#define	PIXEL(buf,ix)	((((buf)[(ix)>>3]) >> (7-((ix)&7))) & 1)
	int state, data;
#define	CACHE_STATE	{ state = sp->b.bit; data = sp->b.data; }
#define	UNCACHE_STATE	{ sp->b.bit = state; sp->b.data = data; }
	Fax3DecodeState *sp = (Fax3DecodeState *)tif->tif_data;
	int32 a0 = -1;
	int32 b1, b2;
	int32 run1, run2;	/* for horizontal mode */
	int mode;
	int color = 0;
	static const char module[] = "Fax3Decode2D";

	CACHE_STATE;
	do {
		do {
			if (state == 0 || state > 7) {
				if (tif->tif_rawcc <= 0) {
					TIFFError(module,
					    "%s: Premature EOF at scanline %d",
					    tif->tif_name, tif->tif_row);
					UNCACHE_STATE;
					return (G3CODE_EOF);
				}
				data = fetchByte(tif, sp);
			}
			mode = TIFFFax2DFSM[state][data];
			state = mode & 0xff; mode >>= 8;
		} while (mode == MODE_NULL);
		switch (mode) {
		case MODE_PASS:
			b2 = finddiff(sp->b.refline, a0, npels, !color);
			b1 = finddiff(sp->b.refline, b2, npels, color);
			b2 = finddiff(sp->b.refline, b1, npels, !color);
			if (color) {
				if (a0 < 0)
					a0 = 0;
				fillspan((char *)buf, a0, b2 - a0);
			}
			a0 = b2;
			break;
		case MODE_HORIZ:
			UNCACHE_STATE;
			if (color == 0) {
				run1 = decode_white_run(tif);
				run2 = decode_black_run(tif);
			} else {
				run1 = decode_black_run(tif);
				run2 = decode_white_run(tif);
			}
			CACHE_STATE;
			if (run1 >= 0 && run2 >= 0) {
				/*
				 * Do the appropriate fill.  Note that we exit
				 * this logic with the same color that we enter
				 * with since we do 2 fills.  This explains the
				 * somewhat obscure logic below.
				 */
				if (a0 < 0)
					a0 = 0;
				if (a0 + run1 > npels)
					run1 = npels - a0;
				if (color)
					fillspan((char *)buf, a0, run1);
				a0 += run1;
				if (a0 + run2 > npels)
					run2 = npels - a0;
				if (!color)
					fillspan((char *)buf, a0, run2);
				a0 += run2;
			}
			break;
		case MODE_VERT_V0:
		case MODE_VERT_VR1:
		case MODE_VERT_VR2:
		case MODE_VERT_VR3:
		case MODE_VERT_VL1:
		case MODE_VERT_VL2:
		case MODE_VERT_VL3:
			b2 = finddiff(sp->b.refline, a0, npels, !color);
			b1 = finddiff(sp->b.refline, b2, npels, color);
			b1 += mode - MODE_VERT_V0;
			if (color) {
				if (a0 < 0)
					a0 = 0;
				fillspan((char *)buf, a0, b1 - a0);
			}
			color = !color;
			a0 = b1;
			break;
	        case MODE_UNCOMP:
			/*
			 * Uncompressed mode: select from the
			 * special set of code words.
			 */
			if (a0 < 0)
				a0 = 0;
			UNCACHE_STATE;
			do {
				mode = decode_uncomp_code(tif);
				switch (mode) {
				case UNCOMP_RUN1:
				case UNCOMP_RUN2:
				case UNCOMP_RUN3:
				case UNCOMP_RUN4:
				case UNCOMP_RUN5:
					run1 = mode - UNCOMP_RUN0;
					fillspan((char *)buf, a0+run1-1, 1);
					a0 += run1;
					break;
				case UNCOMP_RUN6:
					a0 += 5;
					break;
				case UNCOMP_TRUN0:
				case UNCOMP_TRUN1:
				case UNCOMP_TRUN2:
				case UNCOMP_TRUN3:
				case UNCOMP_TRUN4:
					run1 = mode - UNCOMP_TRUN0;
					a0 += run1;
					color = nextbit(tif);
					break;
				case UNCOMP_INVALID:
					TIFFError(module,
				"%s: Bad uncompressed code word at scanline %d",
					    tif->tif_name, tif->tif_row);
					goto bad;
				case UNCOMP_EOF:
					TIFFError(module,
					    "%s: Premature EOF at scanline %d",
					    tif->tif_name, tif->tif_row);
					return (G3CODE_EOF);
				}
			} while (mode < UNCOMP_EXIT);
			CACHE_STATE;
			break;
	        case MODE_ERROR_1:
			if ((tif->tif_options & FAX3_NOEOL) == 0) {
				TIFFWarning(module,
				    "%s: Premature EOL at scanline %d (x %d)",
				    tif->tif_name, tif->tif_row, a0);
				UNCACHE_STATE;
				skiptoeol(tif, 7);	/* seen 7 0's already */
				return (1);		/* try to synchronize */
			}
			/* fall thru... */
	        case MODE_ERROR:
			TIFFError(module,
			    "%s: Bad 2D code word at scanline %d",
			    tif->tif_name, tif->tif_row);
			UNCACHE_STATE;
			goto bad;
	        default:
			TIFFError(module,
			    "%s: Panic, bad decoding state at scanline %d",
			    tif->tif_name, tif->tif_row);
			UNCACHE_STATE;
			return (0);
		}
	} while (a0 < npels);
	UNCACHE_STATE;
bad:
	/*
	 * Cleanup at the end of row.  We check for
	 * EOL separately so that this code can be
	 * reused by the Group 4 decoding routine.
	 */
	if ((tif->tif_options & FAX3_NOEOL) == 0)
		skiptoeol(tif, 0);
	return (a0 >= npels ? 1 : G3CODE_EOL);
#undef	UNCACHE_STATE
#undef	CACHE_STATE
#undef	PIXEL
}

/*
 * CCITT Group 3 FAX Encoding.
 */

/*
 * Write a variable-length bit-value to
 * the output stream.  Values are
 * assumed to be at most 16 bits.
 */
void
Fax3PutBits(TIFF* tif, u_int bits, u_int length)
{
	Fax3BaseState *sp = (Fax3BaseState *)tif->tif_data;
	static const int mask[9] =
	    { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };

	while (length > sp->bit) {
		sp->data |= bits >> (length - sp->bit);
		length -= sp->bit;
		Fax3FlushBits(tif, sp);
	}
	sp->data |= (bits & mask[length]) << (sp->bit - length);
	sp->bit -= length;
	if (sp->bit == 0)
		Fax3FlushBits(tif, sp);
}

/*
 * Write a code to the output stream.
 */
static void
putcode(TIFF* tif, const tableentry* te)
{
	Fax3PutBits(tif, te->code, te->length);
}

/*
 * Write the sequence of codes that describes
 * the specified span of zero's or one's.  The
 * appropriate table that holds the make-up and
 * terminating codes is supplied.
 */
static void
putspan(TIFF* tif, int32 span, const tableentry* tab)
{
	while (span >= 2624) {
		const tableentry *te = &tab[63 + (2560>>6)];
		putcode(tif, te);
		span -= te->runlen;
	}
	if (span >= 64) {
		const tableentry *te = &tab[63 + (span>>6)];
		assert(te->runlen == 64*(span>>6));
		putcode(tif, te);
		span -= te->runlen;
	}
	putcode(tif, &tab[span]);
}

/*
 * Write an EOL code to the output stream.  The zero-fill
 * logic for byte-aligning encoded scanlines is handled
 * here.  We also handle writing the tag bit for the next
 * scanline when doing 2d encoding.
 */
void
Fax3PutEOL(TIFF* tif)
{
	Fax3BaseState *sp = (Fax3BaseState *)tif->tif_data;

	if (tif->tif_dir.td_group3options & GROUP3OPT_FILLBITS) {
		/*
		 * Force bit alignment so EOL will terminate on
		 * a byte boundary.  That is, force the bit alignment
		 * to 16-12 = 4 before putting out the EOL code.
		 */
		int align = 8 - 4;
		if (align != sp->bit) {
			if (align > sp->bit)
				align = sp->bit + (8 - align);
			else
				align = sp->bit - align;
			Fax3PutBits(tif, 0, align);
		}
	}
	Fax3PutBits(tif, EOL, 12);
	if (is2DEncoding(tif))
		Fax3PutBits(tif, sp->tag == G3_1D, 1);
}

static const u_char zeroruns[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,	/* 0x00 - 0x0f */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/* 0x10 - 0x1f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 0x20 - 0x2f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 0x30 - 0x3f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x40 - 0x4f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x50 - 0x5f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x60 - 0x6f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x70 - 0x7f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 - 0x8f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 - 0x9f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 - 0xaf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 - 0xbf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 - 0xcf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 - 0xdf */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 - 0xef */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xf0 - 0xff */
};
static const u_char oneruns[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 - 0x0f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 - 0x1f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x20 - 0x2f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x30 - 0x3f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x40 - 0x4f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x50 - 0x5f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x60 - 0x6f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x70 - 0x7f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x80 - 0x8f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0x90 - 0x9f */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0xa0 - 0xaf */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	/* 0xb0 - 0xbf */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 0xc0 - 0xcf */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 0xd0 - 0xdf */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/* 0xe0 - 0xef */
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8,	/* 0xf0 - 0xff */
};

/*
 * Reset encoding state at the start of a strip.
 */
static
Fax3PreEncode(TIFF* tif)
{
	Fax3EncodeState *sp = (Fax3EncodeState *)tif->tif_data;

	if (sp == NULL) {
		sp = (Fax3EncodeState *)Fax3SetupState(tif, sizeof (*sp));
		if (!sp)
			return (0);
	}
	sp->b.bit = 8;
	sp->b.data = 0;
	sp->b.tag = G3_1D;
	/*
	 * This is necessary for Group 4; otherwise it isn't
	 * needed because the first scanline of each strip ends
	 * up being copied into the refline.
	 */
	if (sp->b.refline)
		_TIFFmemset(sp->b.refline, 0x00, sp->b.rowbytes);
	if (is2DEncoding(tif)) {
		float res = tif->tif_dir.td_yresolution;
		/*
		 * The CCITT spec says that when doing 2d encoding, you
		 * should only do it on K consecutive scanlines, where K
		 * depends on the resolution of the image being encoded
		 * (2 for <= 200 lpi, 4 for > 200 lpi).  Since the directory
		 * code initializes td_yresolution to 0, this code will
		 * select a K of 2 unless the YResolution tag is set
		 * appropriately.  (Note also that we fudge a little here
		 * and use 150 lpi to avoid problems with units conversion.)
		 */
		if (tif->tif_dir.td_resolutionunit == RESUNIT_CENTIMETER)
			res = (res * .3937f) / 2.54f;	/* convert to inches */
		sp->maxk = (res > 150 ? 4 : 2);
		sp->k = sp->maxk-1;
	} else
		sp->k = sp->maxk = 0;
	return (1);
}

/*
 * 1d-encode a row of pixels.  The encoding is
 * a sequence of all-white or all-black spans
 * of pixels encoded with Huffman codes.
 */
static int
Fax3Encode1DRow(TIFF* tif, u_char* bp, uint32 bits)
{
	Fax3EncodeState *sp = (Fax3EncodeState *)tif->tif_data;
	int32 bs = 0, span;

	for (;;) {
		span = find0span(bp, bs, bits);		/* white span */
		putspan(tif, span, TIFFFaxWhiteCodes);
		bs += span;
		if (bs >= bits)
			break;
		span = find1span(bp, bs, bits);		/* black span */
		putspan(tif, span, TIFFFaxBlackCodes);
		bs += span;
		if (bs >= bits)
			break;
	}
	if (tif->tif_options & (FAX3_BYTEALIGN|FAX3_WORDALIGN)) {
		Fax3PutBits(tif, 0, sp->b.bit);
		if ((tif->tif_options & FAX3_WORDALIGN) && !isWordAligned(tif))
			Fax3PutBits(tif, 0, 8);
	}
	return (1);
}

static const tableentry horizcode =
    { 3, 0x1 };		/* 001 */
static const tableentry passcode =
    { 4, 0x1 };		/* 0001 */
static const tableentry vcodes[7] = {
    { 7, 0x03 },	/* 0000 011 */
    { 6, 0x03 },	/* 0000 11 */
    { 3, 0x03 },	/* 011 */
    { 1, 0x1 },		/* 1 */
    { 3, 0x2 },		/* 010 */
    { 6, 0x02 },	/* 0000 10 */
    { 7, 0x02 }		/* 0000 010 */
};

/*
 * 2d-encode a row of pixels.  Consult the CCITT
 * documentation for the algorithm.
 */
int
Fax3Encode2DRow(TIFF* tif, u_char* bp, u_char* rp, uint32 bits)
{
#define	PIXEL(buf,ix)	((((buf)[(ix)>>3]) >> (7-((ix)&7))) & 1)
	int32 a0 = 0;
	int32 a1 = (PIXEL(bp, 0) != 0 ? 0 : finddiff(bp, 0, bits, 0));
	int32 b1 = (PIXEL(rp, 0) != 0 ? 0 : finddiff(rp, 0, bits, 0));
	int32 a2, b2;

	for (;;) {
		b2 = finddiff(rp, b1, bits, PIXEL(rp,b1));
		if (b2 >= a1) {
			int d = b1 - a1;
			if (!(-3 <= d && d <= 3)) {	/* horizontal mode */
				a2 = finddiff(bp, a1, bits, PIXEL(bp,a1));
				putcode(tif, &horizcode);
				if (a0+a1 == 0 || PIXEL(bp, a0) == 0) {
					putspan(tif, a1-a0, TIFFFaxWhiteCodes);
					putspan(tif, a2-a1, TIFFFaxBlackCodes);
				} else {
					putspan(tif, a1-a0, TIFFFaxBlackCodes);
					putspan(tif, a2-a1, TIFFFaxWhiteCodes);
				}
				a0 = a2;
			} else {			/* vertical mode */
				putcode(tif, &vcodes[d+3]);
				a0 = a1;
			}
		} else {				/* pass mode */
			putcode(tif, &passcode);
			a0 = b2;
		}
		if (a0 >= bits)
			break;
		a1 = finddiff(bp, a0, bits, PIXEL(bp,a0));
		b1 = finddiff(rp, a0, bits, !PIXEL(bp,a0));
		b1 = finddiff(rp, b1, bits, PIXEL(bp,a0));
	}
	return (1);
#undef PIXEL
}

/*
 * Encode a buffer of pixels.
 */
static int
Fax3Encode(TIFF* tif, tidata_t bp, tsize_t cc, tsample_t s)
{
	Fax3EncodeState *sp = (Fax3EncodeState *)tif->tif_data;

	while ((long)cc > 0) {
		if ((tif->tif_options & FAX3_NOEOL) == 0)
			Fax3PutEOL(tif);
		if (is2DEncoding(tif)) {
			if (sp->b.tag == G3_1D) {
				if (!Fax3Encode1DRow(tif, bp, sp->b.rowpixels))
					return (0);
				sp->b.tag = G3_2D;
			} else {
				if (!Fax3Encode2DRow(tif, bp, sp->b.refline, sp->b.rowpixels))
					return (0);
				sp->k--;
			}
			if (sp->k == 0) {
				sp->b.tag = G3_1D;
				sp->k = sp->maxk-1;
			} else
				_TIFFmemcpy(sp->b.refline, bp, sp->b.rowbytes);
		} else {
			if (!Fax3Encode1DRow(tif, bp, sp->b.rowpixels))
				return (0);
		}
		bp += sp->b.rowbytes;
		cc -= sp->b.rowbytes;
		if (cc != 0)
			tif->tif_row++;
	}
	return (1);
}

static int
Fax3PostEncode(TIFF* tif)
{
	Fax3BaseState *sp = (Fax3BaseState *)tif->tif_data;

	if (sp->bit != 8)
		Fax3FlushBits(tif, sp);
	return (1);
}

static void
Fax3Close(TIFF* tif)
{
	if ((tif->tif_options & FAX3_CLASSF) == 0) {	/* append RTC */
		int i;
		for (i = 0; i < 6; i++) {
		    Fax3PutBits(tif, EOL, 12);
		    if (is2DEncoding(tif))
			    Fax3PutBits(tif, 1, 1);
		}
		(void) Fax3PostEncode(tif);
	}
}

static void
Fax3Cleanup(TIFF* tif)
{
	if (tif->tif_data) {
		_TIFFfree(tif->tif_data);
		tif->tif_data = NULL;
	}
}

/*
 * Find a span of ones or zeros using the supplied
 * table.  The ``base'' of the bit string is supplied
 * along with the start+end bit indices.
 */

static int32
find0span(u_char* bp, int32 bs, int32 be)
{
	int32 bits = be - bs;
	int32 n, span;

	bp += bs>>3;
	/*
	 * Check partial byte on lhs.
	 */
	if (bits > 0 && (n = (bs & 7))) {
		span = zeroruns[(*bp << n) & 0xff];
		if (span > 8-n)		/* table value too generous */
			span = 8-n;
		if (span > bits)	/* constrain span to bit range */
			span = bits;
		if (n+span < 8)		/* doesn't extend to edge of byte */
			return (span);
		bits -= span;
		bp++;
	} else
		span = 0;
	if (bits >= 2*8*sizeof (long)) {
		long* lp;
		/*
		 * Align to longword boundary and check longwords.
		 */
		while (!isAligned(bp, long)) {
			if (*bp != 0x00)
				return (span + zeroruns[*bp]);
			span += 8, bits -= 8;
			bp++;
		}
		lp = (long*) bp;
		while (bits >= 8*sizeof (long) && *lp == 0) {
			span += 8*sizeof (long), bits -= 8*sizeof (long);
			lp++;
		}
		bp = (u_char*) lp;
	}
	/*
	 * Scan full bytes for all 0's.
	 */
	while (bits >= 8) {
		if (*bp != 0x00)	/* end of run */
			return (span + zeroruns[*bp]);
		span += 8, bits -= 8;
		bp++;
	}
	/*
	 * Check partial byte on rhs.
	 */
	if (bits > 0) {
		n = zeroruns[*bp];
		span += (n > bits ? bits : n);
	}
	return (span);
}

static int32
find1span(u_char* bp, int32 bs, int32 be)
{
	int32 bits = be - bs;
	int32 n, span;

	bp += bs>>3;
	/*
	 * Check partial byte on lhs.
	 */
	if (bits > 0 && (n = (bs & 7))) {
		span = oneruns[(*bp << n) & 0xff];
		if (span > 8-n)		/* table value too generous */
			span = 8-n;
		if (span > bits)	/* constrain span to bit range */
			span = bits;
		if (n+span < 8)		/* doesn't extend to edge of byte */
			return (span);
		bits -= span;
		bp++;
	} else
		span = 0;
	if (bits >= 2*8*sizeof (long)) {
		long* lp;
		/*
		 * Align to longword boundary and check longwords.
		 */
		while (!isAligned(bp, long)) {
			if (*bp != 0xff)
				return (span + oneruns[*bp]);
			span += 8, bits -= 8;
			bp++;
		}
		lp = (long*) bp;
		while (bits >= 8*sizeof (long) && *lp == ~0) {
			span += 8*sizeof (long), bits -= 8*sizeof (long);
			lp++;
		}
		bp = (u_char*) lp;
	}
	/*
	 * Scan full bytes for all 1's.
	 */
	while (bits >= 8) {
		if (*bp != 0xff)	/* end of run */
			return (span + oneruns[*bp]);
		span += 8, bits -= 8;
		bp++;
	}
	/*
	 * Check partial byte on rhs.
	 */
	if (bits > 0) {
		n = oneruns[*bp];
		span += (n > bits ? bits : n);
	}
	return (span);
}

int
TIFFInitCCITTFax3(TIFF* tif)
{
	tif->tif_predecode = Fax3PreDecode;
	tif->tif_decoderow = Fax3Decode;
	tif->tif_decodestrip = Fax3Decode;
	tif->tif_decodetile = Fax3Decode;
	tif->tif_preencode = Fax3PreEncode;
	tif->tif_postencode = Fax3PostEncode;
	tif->tif_encoderow = Fax3Encode;
	tif->tif_encodestrip = Fax3Encode;
	tif->tif_encodetile = Fax3Encode;
	tif->tif_close = Fax3Close;
	tif->tif_cleanup = Fax3Cleanup;
	tif->tif_options |= FAX3_CLASSF;	/* default */
	tif->tif_flags |= TIFF_NOBITREV;	/* we handle bit reversal */
	return (1);
}
