#ifndef MLVIEW_H
#define MLVIEW_H

/*
 * What is this?
 *
 *  It is a package to show multi-lingual text.
 *
 * How to use?
 *
 *  1. Call ml_set_screen(Screen *scr);
 *	Tell this package the screen you use.
 *
 *  2. Call ml_set_charsets(struct char_spec spec[4], int gl, int gr);
 *	Tell this package the initial charsets.
 *	Gn is set to the charset specified by spec[n], respectively.
 *	GL and GR are set to G[gl] and G[gr], respectively.
 *	If first call, iso8859-1 font is loaded.
 *
 *  3. Call ml_draw_text(char *string);
 *	It Creates a bitmap, and returns it to you.
 *	If something goes wrong, it returns None.
 *	DON'T free the returned pixmaps!!
 *
 * BUGS:
 *  - Amharic and Tigrigna characters are strange.
 *  - Big5 is not supported.
 *  - Reverse direction is not supported.
 *  - Composing is not supported.
 *  - Cantonese can't be shown.
 *  - Texts which have many lines are buggy.
 *
 * NOTE:
 *  - Shifted JIS and Shifted GB must be converted to iso2022 in advance.
 *
 * Example of parameters to ml_set_charsets:
 *  - EUC-Japan
 *	spec = { {1, 94, 'B'},		G0 is US-ASCII
 *		 {2, 94, 'B'},		G1 is JIS X0208
 *		 {1, 94, 'J'},		G2 is (right-half of)JIS X0201
 *		 {2, 94, 'D'} };	G3 is JIS X0212
 *	gl = 0;				GL is G0
 *	gr = 1;				GR is G1
 *
 *  - Compound Text
 *	spec = { {1, 94, 'B'},		G0 is US-ASCII
 *		 {1, 96, 'A'},		G1 is Latin-1
 *		 {1, 94, 'B'},		G2 is US-ASCII (maybe unused)
 *		 {1, 94, 'B'} };	G3 is US-ASCII (maybe unused)
 *	gl = 0;				GL is G0
 *	gr = 1;				GR is G1
 *
 *  - Korean Mail
 *	spec = { {1, 94, 'B'},		G0 is US-ASCII
 *		 {2, 94, 'C'},		G1 is KSC5601
 *		 {1, 94, 'B'},		G2 is US-ASCII (maybe unused)
 *		 {1, 94, 'B'} };	G3 is US-ASCII (maybe unused)
 *	gl = 0;				GL is G0
 *	gl = 1;				GR is G1
 */

struct coding_system {
    struct design {
	int bpc;	/* byte per char if 1 or 2,
			   don't touch if 0, or
			   don't use if -1.*/
	int noc;	/* number of chars (94 or 96) */
	char des;	/* designator ('A', 'B', ...) */
    } design[4];
    int gl, gr;
    int eol;
    int short_form;
    int lock_shift;
};

struct ml_text {
    int maxlines, nlines;
    struct ml_line {
	int maxitems, nitems;
	int width, ascent, descent;
	XTextItem16 *items;
    } *lines;
    int width, height;
};

struct context;
struct ml_text *ml_draw_text		PARM((struct context *, char *, int));
struct context *ml_create_context	PARM((Screen *));
int ml_set_charsets			PARM((struct context *,
					      struct coding_system *));
void get_monofont_size			PARM((int *, int *));
char *sjis_to_jis			PARM((char *, int, int *));
char *lookup_registry			PARM((struct design, int *));
struct design lookup_design		PARM((char *, int));

#endif
