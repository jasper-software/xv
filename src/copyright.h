/* Copyright Notice
 * ================
 * Copyright 1989, 1994 by John Bradley
 *
 * Permission to copy and distribute XV in its entirety, for non-commercial
 * purposes, is hereby granted without fee, provided that this license
 * information and copyright notice appear unmodified in all copies.
 *
 * Note that distributing XV 'bundled' in with any product is considered
 * to be a 'commercial purpose'.
 *
 * Also note that any copies of XV that are distributed must be built
 * and/or configured to be in their 'unregistered copy' mode, so that it
 * is made obvious to the user that XV is shareware, and that they should
 * consider donating, or at least reading this License Info.
 *
 * The software may be modified for your own purposes, but modified
 * versions may not be distributed without prior consent of the author.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * If you would like to do something with XV that this copyright
 * prohibits (such as distributing it with a commercial product,
 * using portions of the source in some other program, etc.), please
 * contact the author (preferably via email).  Arrangements can
 * probably be worked out.
 *
 * XV is shareware for PERSONAL USE only.  You may use XV for your own
 * amusement, and if you find it nifty, useful, generally cool, or of
 * some value to you, your non-deductable donation would be greatly
 * appreciated.  $25 is the suggested donation, though, of course,
 * larger donations are quite welcome.  Folks who donate $25 or more
 * can receive a Real Nice bound copy of the XV manual for no extra
 * charge.
 *
 * Commercial, government, and institutional users must register their
 * copies of XV, for the price of $25 per workstation/X terminal or per
 * XV user, whichever is less.  Note that it does NOT say 'simultaneous user',
 * but rather, the total number of people who use XV on any sort of
 * recurring basis. Site licenses are available (and recommended) for those
 * who wish to run XV on a large (>10) number of machines.
 * Contact the author for more details.
 *
 * The author may be contacted via:
 *
 *    Email:    bradley@cis.upenn.edu       (preferred!)
 *
 *    FAX:      (610) 520-2042
 *
 *    US Mail:  John Bradley
 *              1053 Floyd Terrace
 *              Bryn Mawr, PA  19010
 *
 * The author may not be contacted by (voice) phone.  Please don't try.
 *
 */

/*
 * Portions copyright 2000-2007 by Greg Roelofs and contributors:
 *
 *   Andrey A. Chernov [ache]
 *     (http://cvsweb.freebsd.org/ports/graphics/xv/files/patch-ab)
 *   Andreas Dilger (adilger clusterfs.com)
 *   Alexander Lehmann (lehmann usa.net)
 *   Alexey Spiridonov (http://www-math.mit.edu/~lesha/)
 *   Anthony Thyssen (http://www.cit.gu.edu.au/~anthony/)
 *   Bruno Rohee (http://bruno.rohee.com/)
 *   David A. Clunie (http://www.dclunie.com/xv-pcd.html)
 *   Erling A. Jacobsen (linuxcub email.dk)
 *   Egmont Koblinger (egmont users.sourceforge.net)
 *   Fabian Greffrath (fabian debian-unofficial.org)
 *   Greg Roelofs (http://pobox.com/~newt/greg_contact.html)
 *   Guido Vollbeding (http://sylvana.net/guido/)
 *   IKEMOTO Masahiro (ikeyan airlab.cs.ritsumei.ac.jp)
 *   John Cooper (john.cooper third-harmonic.com)
 *   John C. Elliott (http://www.seasip.demon.co.uk/ZX/zxdload.html)
 *   John D. Baker (http://mylinuxisp.com/~jdbaker/)
 *   Jörgen Grahn (jgrahn algonet.se)
 *   John H. Bradley, of course (http://www.trilon.com/xv/)
 *   Jean-Pierre Demailly (http://www-fourier.ujf-grenoble.fr/~demailly/)
 *   John Rochester (http://www.freebsd.org/cgi/query-pr.cgi?pr=2920)
 *   (also http://cvsweb.freebsd.org/ports/graphics/xv/files/patch-af, -ag)
 *   James Roberts Kirkpatrick (uwyo.edu)
 *   Joe Zbiciak (http://spatula-city.org/~im14u2c/)
 *   Kyoichiro Suda (http://www.coara.or.jp/~sudakyo/XV_jp.html)
 *   Landon Curt "chongo" Noll (http://www.isthe.com/chongo/)
 *   Larry Jones (lawrence.jones ugs.com)
 *   Peter Jordan (http://www.ibiblio.org/pub/Linux/apps/graphics/viewers/X/)
 *   Pawel S. Veselov (http://manticore.2y.net/wbmp.html)
 *   Ross Combs (rocombs cs.nmsu.edu)
 *   Robin Humble (http://www.cita.utoronto.ca/~rjh/)
 *   Sean Borman (http://www.nd.edu/~sborman/software/xvwheelmouse.html)
 *   TenThumbs (tenthumbs cybernex.net)
 *   Scott B. Marovich (formerly marovich hpl.hp.com)
 *   Tim Adye (http://hepwww.rl.ac.uk/Adye/xv-psnewstyle.html)
 *   Tim Ramsey (tar pobox.com)
 *   Tetsuya INOUE (tin329 chino.it.okayama-u.ac.jp)
 *   Tavis Ormandy (taviso gentoo.org)
 *   Werner Fink (http://www.suse.de/~werner/)
 *
 * Other credits are as listed on the XV Downloads page or in the respective
 * patches (e.g., the jp-extension patches or within the PNG patch).
 *
 */
