/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <wraster.h>

#include <stdlib.h>
#include <string.h>
#include "WindowMaker.h"
#include "pixmap.h"

/*
 *----------------------------------------------------------------------
 * wPixmapCreateFromXPMData--
 * 	Creates a WPixmap structure and initializes it with the supplied
 * XPM structure data.
 *
 * Returns:
 * 	A WPixmap structure or NULL on failure.
 *
 * Notes:
 * 	DEF_XPM_CLOSENESS specifies the XpmCloseness
 *----------------------------------------------------------------------
 */
WPixmap *wPixmapCreateFromXPMData(WScreen *scr, char **data)
{
	RImage *image;
	WPixmap *pix;

	image = RGetImageFromXPMData(scr->rcontext, data);
	if (!image)
		return NULL;

	pix = wmalloc(sizeof(WPixmap));

	RConvertImageMask(scr->rcontext, image, &pix->image, &pix->mask, 128);

	pix->width = image->width;
	pix->height = image->height;
	pix->depth = scr->w_depth;

	RReleaseImage(image);

	return pix;
}

/*
 *----------------------------------------------------------------------
 * wPixmapCreateFromXBMData--
 * 	Creates a WPixmap structure and initializes it with the supplied
 * XBM structure data, size and mask.
 *
 * Returns:
 * 	A WPixmap structure or NULL on failure.
 *
 *----------------------------------------------------------------------
 */
WPixmap *wPixmapCreateFromXBMData(WScreen *scr, char *data, char *mask,
				  int width, int height, unsigned long fg, unsigned long bg)
{
	WPixmap *pix;

	pix = wmalloc(sizeof(WPixmap));
	pix->image = XCreatePixmapFromBitmapData(dpy, scr->w_win, data, width, height, fg, bg, scr->w_depth);
	if (pix->image == None) {
		wfree(pix);
		return NULL;
	}

	if (mask)
		pix->mask = XCreateBitmapFromData(dpy, scr->w_win, mask, width, height);
	else
		pix->mask = None;

	pix->width = width;
	pix->height = height;
	pix->depth = scr->w_depth;

	return pix;
}

WPixmap *wPixmapCreate(Pixmap image, Pixmap mask)
{
	WPixmap *pix;
	Window foo;
	int bar;
	unsigned int width, height, depth, baz;

	pix = wmalloc(sizeof(WPixmap));
	pix->image = image;
	pix->mask = mask;
	if (!XGetGeometry(dpy, image, &foo, &bar, &bar, &width, &height, &baz, &depth)) {
		wwarning("XGetGeometry() failed during wPixmapCreate()");
		wfree(pix);
		return NULL;
	}

	pix->width = width;
	pix->height = height;
	pix->depth = depth;

	return pix;
}

/*
 *----------------------------------------------------------------------
 * wPixmapDestroy--
 * 	Destroys a WPixmap structure and the pixmap/mask it holds.
 *
 * Returns:
 * 	None
 *----------------------------------------------------------------------
 */
void wPixmapDestroy(WPixmap *pix)
{
	if (!pix->shared) {
		if (pix->mask && !pix->client_owned_mask)
			XFreePixmap(dpy, pix->mask);

		if (pix->image && !pix->client_owned)
			XFreePixmap(dpy, pix->image);
	}

	wfree(pix);
}

void destroy_pixmap(Pixmap pix)
{
	if ((pix) != None) {
		XFreePixmap(dpy, pix);
		pix = None;
	}
}
