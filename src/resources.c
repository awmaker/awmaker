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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <wraster.h>

#include "WindowMaker.h"
#include "resources.h"
#include "screen.h"

int wGetColorForColormap(Colormap colormap, const char *color_name, XColor *color)
{
	if (!XParseColor(dpy, colormap, color_name, color)) {
		wwarning(_("could not parse color \"%s\""), color_name);
		return False;
	}
	if (!XAllocColor(dpy, colormap, color)) {
		wwarning(_("could not allocate color \"%s\""), color_name);
		return False;
	}
	return True;
}

int wGetColor(WScreen *scr, const char *color_name, XColor *color)
{
	return wGetColorForColormap(scr->w_colormap, color_name, color);
}

void wFreeColor(WScreen * scr, unsigned long pixel)
{
	if (pixel != scr->white_pixel && pixel != scr->black_pixel) {
		unsigned long colors[1];

		colors[0] = pixel;
		XFreeColors(dpy, scr->w_colormap, colors, 1, 0);
	}
}
