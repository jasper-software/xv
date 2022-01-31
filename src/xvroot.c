/*
 * xvroot.c - '-root' related code for XV
 *
 *  Contains:
 *            MakeRootPic()
 *            ClearRoot()
 *            SaveRootInfo()
 *            KillOldRootInfo()
 */

#include "copyright.h"

#include "xv.h"

#include "bits/root_weave"


/* local function pre-definitions */
static void killRootPix PARM((void));


/***********************************/
void MakeRootPic()
{
  /* called after 'epic' has been generated (if we're using root).
     creates the XImage and the pixmap, sets the root to the new
     pixmap, and refreshes the display */

  Pixmap       tmpPix;
  int          i, j, k, rmode;
  unsigned int rpixw, rpixh;

  killRootPix();

  rmode = rootMode;
  /* if eWIDE,eHIGH == dispWIDE,dispHIGH just use 'normal' mode to save mem */
  if (rmode>=RM_CENTER && eWIDE==dispWIDE && eHIGH==dispHIGH) rmode=RM_NORMAL;

  /* determine how big tmpPix should be based on rootMode */
  switch (rmode) {
  case RM_NORMAL:
  case RM_CENTILE:
  case RM_TILE:    rpixw = eWIDE;    rpixh = eHIGH;    break;
  case RM_MIRROR:
  case RM_IMIRROR: rpixw = 2*eWIDE;  rpixh = 2*eHIGH;  break;
  case RM_CSOLID:
  case RM_UPLEFT:
  case RM_CWARP:
  case RM_CBRICK:  rpixw = dispWIDE; rpixh = dispHIGH; break;

  case RM_ECENTER:
  case RM_ECMIRR:  rpixw = dispWIDE; rpixh = dispHIGH;  break;

  default:         rpixw = eWIDE;    rpixh = eHIGH;    break;
  }
  if (nolimits) { RANGE(rpixw, 1, maxWIDE);  RANGE(rpixh, 1, maxHIGH); }
           else { RANGE(rpixw, 1, dispWIDE);  RANGE(rpixh, 1, dispHIGH); }

  /* create tmpPix */
  xerrcode = 0;
  tmpPix = XCreatePixmap(theDisp, mainW, rpixw, rpixh, dispDEEP);
  XSync(theDisp, False);
  if (xerrcode || !tmpPix) {
    ErrPopUp("Insufficient memory in X server to store root pixmap.",
	     "\nDarn!");
    return;
  }


  if (rmode == RM_NORMAL || rmode == RM_TILE) {
    XPutImage(theDisp, tmpPix, theGC, theImage, 0,0, 0,0,
	      (u_int) eWIDE, (u_int) eHIGH);
  }

  else if (rmode == RM_MIRROR || rmode == RM_IMIRROR) {
    /* quadrant 2 */
    XPutImage(theDisp, tmpPix, theGC, theImage, 0,0, 0,0,
	      (u_int) eWIDE, (u_int) eHIGH);
    if (epic == NULL) FatalError("epic == NULL in RM_MIRROR code...\n");

    /* quadrant 1 */
    FlipPic(epic, eWIDE, eHIGH, 0);   /* flip horizontally */
    CreateXImage();
    XPutImage(theDisp, tmpPix, theGC, theImage, 0,0, eWIDE,0,
	      (u_int) eWIDE, (u_int) eHIGH);

    /* quadrant 4 */
    FlipPic(epic, eWIDE, eHIGH, 1);   /* flip vertically */
    CreateXImage();
    XPutImage(theDisp, tmpPix, theGC, theImage, 0,0, eWIDE,eHIGH,
	      (u_int) eWIDE, (u_int) eHIGH);

    /* quadrant 3 */
    FlipPic(epic, eWIDE, eHIGH, 0);   /* flip horizontally */
    CreateXImage();
    XPutImage(theDisp, tmpPix, theGC, theImage, 0,0, 0,eHIGH,
	      (u_int) eWIDE, (u_int) eHIGH);

    FlipPic(epic, eWIDE, eHIGH, 1);   /* flip vertically  (back to orig) */
    CreateXImage();                   /* put back to original state */
  }


  else if (rmode == RM_CENTER || rmode == RM_CENTILE || rmode == RM_CSOLID ||
	   rmode == RM_CWARP || rmode == RM_CBRICK || rmode == RM_UPLEFT) {
    /* do some stuff to set up the border around the picture */

    if (rmode != RM_CENTILE) {
      XSetForeground(theDisp, theGC, rootbg);
      XFillRectangle(theDisp, tmpPix, theGC, 0,0, dispWIDE, dispHIGH);
    }

    if (rmode == RM_CENTILE) {    /* hagan-style tiling */
      int x, y, w, h, ax, ay, w1, h1, offx, offy;

      w = eWIDE;  h = eHIGH;

      /* compute anchor pt (top-left coords of top-left-most pic) */
      ax = (dispWIDE-w)/2;  ay = (dispHIGH-h)/2;
      while (ax>0) ax = ax - w;
      while (ay>0) ay = ay - h;

      for (i=ay; i < (int) eHIGH; i+=h) {
	for (j=ax; j < (int) eWIDE; j+=w) {
	  /* if image goes off tmpPix, only draw subimage */

	  x = j;  y = i;  w1 = w;  h1 = h;  offx = offy = 0;
	  if (x<0)           { offx = -x;  w1 -= offx;  x = 0; }
	  if (x+w1>eWIDE) { w1 = (eWIDE-x); }

	  if (y<0)           { offy = -y;  h1 -= offy;  y = 0; }
	  if (y+h1>eHIGH)    { h1 = (eHIGH-y); }

	  XPutImage(theDisp, tmpPix, theGC, theImage, offx, offy,
		    x, y, (u_int) w1, (u_int) h1);
	}
      }
    }

    else if (rmode == RM_CSOLID) { }

    else if (rmode == RM_UPLEFT) {

      XPutImage(theDisp, tmpPix, theGC, theImage, 0,0, 0,0,
		(u_int) eWIDE, (u_int) eHIGH);
    }

    else if (rmode == RM_CWARP) {          /* warp effect */
      XSetForeground(theDisp, theGC, rootfg);
      for (i=0; i<=dispWIDE; i+=8)
	XDrawLine(theDisp,tmpPix,theGC, i,0, (int) dispWIDE-i,(int) dispHIGH);
      for (i=0; i<=dispHIGH; i+=8)
	XDrawLine(theDisp,tmpPix,theGC, 0,i, (int) dispWIDE, (int) dispHIGH-i);
    }

    else if (rmode == RM_CBRICK) {         /* brick effect */
      XSetForeground(theDisp, theGC, rootfg);
      for (i=k=0; i<dispHIGH; i+=20,k++) {
	XDrawLine(theDisp, tmpPix, theGC, 0, i, (int) dispWIDE, i);
	for (j=(k&1) * 20 + 10; j<dispWIDE; j+=40)
	  XDrawLine(theDisp, tmpPix, theGC, j,i,j,i+20);
      }
    }


    /* draw the image centered on top of the background */
    if ((rmode != RM_CENTILE) && (rmode != RM_UPLEFT))
      XPutImage(theDisp, tmpPix, theGC, theImage, 0,0,
		((int) dispWIDE-eWIDE)/2, ((int) dispHIGH-eHIGH)/2,
		(u_int) eWIDE, (u_int) eHIGH);
  }


  else if (rmode == RM_ECENTER || rmode == RM_ECMIRR) {
    int fliph, flipv;

    fliph = 0;  flipv = 0;

    if (dispWIDE == eWIDE) {
      /* horizontal center line */
      int y, ay;

      y = eHIGH - ((dispHIGH/2)%eHIGH); /* Starting point in picture to copy */
      ay = 0;    /* Vertical anchor point */
      while (ay < dispHIGH) {
	XPutImage(theDisp, tmpPix, theGC, theImage, 0,y,
		  0,ay, (u_int) eWIDE, (u_int) eHIGH);
	ay += eHIGH - y;
	y = 0;
	if (rmode == RM_ECMIRR) {
	  FlipPic(epic, eWIDE, eHIGH, 1);   flipv = !flipv;
	  CreateXImage();
	}
      }
    }
    else if (dispHIGH == eHIGH) {
      /* vertical centerline */
      int x, ax;

      x = eWIDE - ((dispWIDE/2)%eWIDE); /* Starting point in picture to copy */
      ax = 0;    /* Horizontal anchor point */
      while (ax < dispWIDE) {
	XPutImage(theDisp, tmpPix, theGC, theImage, x,0,
		  ax,0, (u_int) eWIDE, (u_int) eHIGH);
	ax += eWIDE - x;
	x = 0;
	if (rmode == RM_ECMIRR) {
	  FlipPic(epic, eWIDE, eHIGH, 0);   fliph = !fliph;
	  CreateXImage();
	}
      }
    }
    else {
      /* vertical and horizontal centerlines */
      int x,y, ax,ay;

      y = eHIGH - ((dispHIGH/2)%eHIGH); /* Starting point in picture to copy */
      ay = 0;    /* Vertical anchor point */

      while (ay < dispHIGH) {
	x = eWIDE - ((dispWIDE/2)%eWIDE);/* Starting point in picture to cpy */
	ax = 0;    /* Horizontal anchor point */
	while (ax < dispWIDE) {
	  XPutImage(theDisp, tmpPix, theGC, theImage, x,y,
		    ax,ay, (u_int) eWIDE, (u_int) eHIGH);
	  if (rmode == RM_ECMIRR) {
	    FlipPic(epic, eWIDE, eHIGH, 0);  fliph = !fliph;
	    CreateXImage();
	  }
	  ax += eWIDE - x;
	  x = 0;
	}
	if (rmode == RM_ECMIRR) {
	  FlipPic(epic, eWIDE, eHIGH, 1);   flipv = !flipv;
	  if (fliph) {   /* leftmost image is always non-hflipped */
	    FlipPic(epic, eWIDE, eHIGH, 0);   fliph = !fliph;
	  }
	  CreateXImage();
	}
	ay += eHIGH - y;
	y = 0;
      }
    }

    /* put epic back to normal */
    if (fliph) FlipPic(epic, eWIDE, eHIGH, 0);
    if (flipv) FlipPic(epic, eWIDE, eHIGH, 1);
  }


  XSetWindowBackgroundPixmap(theDisp, mainW, tmpPix);
  XFreePixmap(theDisp, tmpPix);

  XClearWindow(theDisp, mainW);
}



/************************************************************************/
void ClearRoot()
{
  killRootPix();
  XClearWindow(theDisp, vrootW);
  XFlush(theDisp);
}


/************************************************************************/
static void killRootPix()
{
  Pixmap pix, bitmap;
  GC gc;
  XGCValues gc_init;

  if (vrootW == rootW) {
    XSetWindowBackgroundPixmap(theDisp, rootW, None);
  }

  else {
    bitmap = XCreateBitmapFromData(theDisp, vrootW, (char *) root_weave_bits,
				   root_weave_width, root_weave_height);

    gc_init.foreground = BlackPixel(theDisp, theScreen);
    gc_init.background = WhitePixel(theDisp, theScreen);
    gc = XCreateGC(theDisp, vrootW, GCForeground|GCBackground, &gc_init);
    pix = XCreatePixmap(theDisp, vrootW, root_weave_width,
			root_weave_height,
			(unsigned int) DefaultDepth(theDisp, theScreen));

    XCopyPlane(theDisp, bitmap, pix, gc, 0,0, root_weave_width,
	       root_weave_height, 0,0, (unsigned long) 1);
    XSetWindowBackgroundPixmap(theDisp, vrootW, pix);

    XFreeGC(theDisp, gc);
    XFreePixmap(theDisp, bitmap);
    XFreePixmap(theDisp, pix);
  }
}



/***********************************/
void SaveRootInfo()
{
  /* called when using root window.  stores the pixmap ID used to draw the
     root window in a property.  This will be used later to free all resources
     allocated by this instantiation of xv (ie, the alloc'd colors).  These
     resources are kept alloc'ed after client exits so that rainbow effect
     is avoided */

  Atom          prop;
  static Pixmap riPix = (Pixmap) NULL;

  if ( !(theVisual->class & 1)) return;  /* no colormap to worry about */
  if (riPix) return;                     /* it's already been saved once */

  riPix = XCreatePixmap(theDisp, vrootW, 1, 1, 1);
  if (!riPix) return;   /* unable to save.  thankfully, unlikely to happen */

  prop = XInternAtom(theDisp, "_XSETROOT_ID", False);
  if (!prop) FatalError("couldn't create _XSETROOT_ID atom");

  XChangeProperty(theDisp, vrootW, prop, XA_PIXMAP, 32, PropModeReplace,
		  (unsigned char *) &riPix, 1);

  XSetCloseDownMode(theDisp, RetainPermanent);
}


/***********************************/
void KillOldRootInfo()
{
  /* get the pixmap ID from the _XSETROOT_ID property, and kill it */

  Atom           prop, type;
  int            format;
  unsigned long  length, after;
  unsigned char *data;

  prop = XInternAtom(theDisp, "_XSETROOT_ID", True);
  if (prop == None) return;    /* no old pixmap to kill */

  if (XGetWindowProperty(theDisp, vrootW, prop, 0L, 1L, True,
			 AnyPropertyType, &type, &format, &length,
			 &after, &data) == Success) {

    if (type==XA_PIXMAP && format==32 && length==1 && after==0 && data) {
      XKillClient(theDisp, *((Pixmap *)data));
      XDeleteProperty(theDisp, vrootW, prop);
    }

    if (data) XFree((char *) data);
  }
}




