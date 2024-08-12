/*
 * Support for reading basic STRING selections without using Xt.
 * Copyright 2003-2005,2012 Ross Combs
 * This file is dual-licensed: the GPL (version 2 or later) and the MIT license.
 *
 * The MIT license:
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "xv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct test_param
{
    Atom selection;
    Window requestor;
};

static Atom clipboard=None;


static Bool TestForNotify(Display *dpy, XEvent *ev, XPointer arg)
{
    struct test_param const *param = (struct test_param const *)arg;

    XV_UNUSED(dpy);

    if (ev->type==SelectionNotify &&
        ev->xselection.requestor==param->requestor &&
        ev->xselection.selection==param->selection)
        return True;

    if (ev->type==SelectionRequest &&
        ev->xselectionrequest.selection==param->selection)
        return True;

    return False;
}


static void *ReadSelection(Display *dpy, Atom selection, Atom targetformat, Window reqw, Atom destprop, Atom *datatype, int *len, int timeout)
{
    if (reqw == None)
        return NULL;

    if (destprop == None)
        return NULL;

    if (XConvertSelection(dpy, selection, targetformat, destprop, reqw, CurrentTime) == False)
    {
        if (DEBUG) fprintf(stderr, "XConvertSelection() failed\n");
        return NULL;
    }

    {
        XEvent ev;
        struct test_param param;
        param.requestor = reqw;
        param.selection = selection;
        for (;;)
        {
            while (XCheckIfEvent(dpy, &ev, TestForNotify, (XPointer)&param)==False)
            {
                fd_set read_fds;
                struct timeval tv;
                int fd;

                fd = ConnectionNumber(dpy);
                FD_ZERO(&read_fds);
                FD_SET(fd, &read_fds);
                tv.tv_sec = timeout;
                tv.tv_usec = 0;
                if (select(fd+1, &read_fds, NULL, NULL, &tv)==0)
                {
                    if (DEBUG) fprintf(stderr, "timeout waiting for selection notification\n");
                    return NULL;
                }
            }

            if (ev.type==SelectionRequest)
            {
              XSelectionRequestEvent const *xsrevt = &ev.xselectionrequest;
              char const *text;

              if (xsrevt->owner == dirW) {
                if (xsrevt->target == XA_STRING)
                  text = TextOfSelection(xsrevt->selection);
                else
                  text = NULL; /* we don't handle any other format */
              } else {
                text = NULL; /* we can't handle other windows outside of the main loop */
              }

              SendSelection(xsrevt->selection, xsrevt->requestor, xsrevt->property, xsrevt->target, xsrevt->time, text);
            }

            else if (ev.type==SelectionNotify)
            {
                if (ev.xselection.requestor != reqw)
                    continue;
                if (ev.xselection.selection != selection)
                    continue;
                if (ev.xselection.property == None)
                {
                    if (DEBUG) fprintf(stderr, "notification property is None; conversion failed\n");
                    return NULL;
                }
                break;
            }
        }
    }

    {
        int format;
        Atom type;
        unsigned long nitems, bytes_after;
        unsigned long length;
        unsigned char *rawbytes;
        void *data;

        /* get the size first so we can ask for it all in one chunk */
        nitems = 0UL;
        rawbytes = NULL;
        if (XGetWindowProperty(dpy, reqw, destprop, 0L, 0L,
                               False, AnyPropertyType,
                               &type, &format, &nitems, &bytes_after,
                               &rawbytes)!=Success)
        {
            if (rawbytes)
                XFree(rawbytes);
            XDeleteProperty(dpy, reqw, destprop);
            return NULL;
        }
        if (rawbytes)
            XFree(rawbytes);

        /* rounding up should be ok because this is the maximum length we want */
        length = (bytes_after+3UL)/4UL; /* convert to next multiple of 32 bits */
        nitems = 0UL;
        rawbytes = NULL;
        if (XGetWindowProperty(dpy, reqw, destprop, 0L, length,
                               False, AnyPropertyType, &type,
                               &format, &nitems, &bytes_after,
                               &rawbytes)!=Success || nitems<1UL || !rawbytes)
        {
            if (rawbytes)
                XFree(rawbytes);
            XDeleteProperty(dpy, reqw, destprop);
            return NULL;
        }

        /* sadly we have to have this ugly hack because Xlib lies about the format of longs
         * (Time, Atoms, Integers, etc.).  On 64 bit platforms it uses 64 bits in the
         * interface and does the right thing internally, but still reports 32 bits for the
         * format.
         */
        if (format==32 && sizeof(long)>4)
            format *= sizeof(long)/4;
        length = (nitems*format)/8UL; /* convert to multiple of 8 bits */

        /* FIXME: for really large data, we would have to use a loop and incremental transfers */
        XDeleteProperty(dpy, reqw, destprop);
        if (!(data = malloc(length+1UL)))
        {
            XFree(rawbytes);
            return NULL;
        }
        memcpy(data, rawbytes, length+1UL); /* copy the extra NUL byte as well so it can be treated as a string */
        XFree(rawbytes);
        *datatype = type;
        *len = (int)length;
        return data;
    }
}


static char *ReadTextSelection(Display *dpy, Atom selection, int timeout)
{
    static Window reqw=None;
    static Atom destprop=None;
    int len;
    Atom datatype;
    char *text;
    len = 0;
    datatype = None;

    if (reqw == None)
    {
        XSetWindowAttributes attr;
        unsigned long valuemask;
        attr.event_mask = NoEventMask; /* we get the selection notification no matter what */
        valuemask = CWEventMask;
        attr.override_redirect = True;
        valuemask |= CWOverrideRedirect;
        attr.backing_store = NotUseful;
        valuemask |= CWBackingStore;
        attr.save_under = False;
        valuemask |= CWSaveUnder;
        attr.background_pixmap = None;
        valuemask |= CWBackPixmap;
        attr.win_gravity = StaticGravity;
        valuemask |= CWWinGravity;
        attr.bit_gravity = StaticGravity;
        valuemask |= CWBitGravity;
        if ((reqw = XCreateWindow(dpy, DefaultRootWindow(dpy),
                                  -1, -1, 1U, 1U, 0U,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  valuemask, &attr))==None)
            return NULL;
        XStoreName(dpy, reqw, "xv selection reader");
    }

    if (destprop == None)
        if ((destprop = XInternAtom(dpy, "READ", False)) == None)
            return NULL;

    /* FIXME: sorry: this implementation is incomplete:
     *   no format negotiation
     *   no handling of COMPOUND_TEXT or UTF8_STRING, only basic STRING
     *   no handling of incremental transfers
     */
    text = ReadSelection(dpy, selection, XA_STRING, reqw, destprop, &datatype, &len, timeout);

    return text;
}


extern char *GetClipboardText(void)
{
    char *text;

    if (clipboard == None)
        if ((clipboard = XInternAtom(theDisp, "CLIPBOARD", False)) == None)
            return NULL;

    text = ReadTextSelection(theDisp, clipboard, 3);
    if (!text)
        text = ReadTextSelection(theDisp, XA_PRIMARY, 3);
    if (!text)
        if (DEBUG) fprintf(stderr, "failed to read CLIPBOARD and PRIMARY selections\n");

    return text;
}


extern char *GetPrimaryText(void)
{
    char *text;

    text = ReadTextSelection(theDisp, XA_PRIMARY, 3);
    if (!text)
        if (DEBUG) fprintf(stderr, "failed to read PRIMARY selection\n");

    return text;
}


static char *clipboard_text=NULL;
static char *primary_text=NULL;


extern int SetClipboardText(Window w, char const *text, int len)
{
    char *newtext;

    if (clipboard == None)
        if ((clipboard = XInternAtom(theDisp, "CLIPBOARD", False)) == None)
            return 0;

    if (!(newtext = strdup(text)))
        return 0;
    newtext[len] = '\0';

    if (XSetSelectionOwner(theDisp, clipboard, w, CurrentTime) == False)
    {
        free(newtext);
        if (DEBUG) fprintf(stderr, "failed to set CLIPBOARD selection\n");
        return 0;
    }

    if (clipboard_text) free(clipboard_text);
    clipboard_text = newtext;

    XSync(theDisp, False);

    return 1;
}


extern int SetPrimaryText(Window w, char const *text, int len)
{
    char *newtext;

    if (!(newtext = strdup(text)))
        return 0;
    newtext[len] = '\0';

    if (XSetSelectionOwner(theDisp, XA_PRIMARY, w, CurrentTime) == False)
    {
        free(newtext);
        if (DEBUG) fprintf(stderr, "failed to set PRIMARY selection\n");
        return 0;
    }

    if (primary_text) free(primary_text);
    primary_text = newtext;

    XSync(theDisp, False);

    return 1;
}


extern char const *TextOfSelection(Atom selection)
{
    if (selection == XA_PRIMARY)
        return primary_text;

    if (clipboard == None)
        if ((clipboard = XInternAtom(theDisp, "CLIPBOARD", False)) == None)
            return NULL;

    if (selection == clipboard)
        return clipboard_text;

    return NULL;
}


extern int ReleaseSelection(Atom selection)
{
    if (selection == XA_PRIMARY) {
        free(primary_text);
        primary_text = NULL;
        return 1;
    }

    if (clipboard == None)
        if ((clipboard = XInternAtom(theDisp, "CLIPBOARD", False)) == None)
            return 0;

    if (selection == clipboard) {
        free(clipboard_text);
        clipboard_text = NULL;
        return 1;
    }

    return 0;
}

