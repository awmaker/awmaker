/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "awconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <wraster.h>

#include "WindowMaker.h"
#include "resources.h"
#include "screen.h"

static unsigned long scale_color_component(unsigned short value, unsigned long mask)
{
	unsigned long m = mask;
	int shift = 0, bits = 0;

	if (!m)
		return 0;

	while (!(m & 1)) {
		shift++;
		m >>= 1;
	}
	while (m) {
		bits++;
		m >>= 1;
	}

	return ((unsigned long)(value >> (16 - bits)) & ((1UL << bits) - 1)) << shift;
}

int wGetColorForColormap(WScreen *scr, Colormap colormap, const char *color_name, XColor *color)
{
	if (!XParseColor(dpy, colormap, color_name, color)) {
		wwarning(_("could not parse color \"%s\""), color_name);
		return False;
	}

	if (scr->w_visual->class == TrueColor) {
		/* Compute pixel directly from RGB components using the visual's
		 * channel masks, avoiding a colormap cell allocation. */
		color->pixel = scale_color_component(color->red, scr->w_visual->red_mask)
		             | scale_color_component(color->green, scr->w_visual->green_mask)
		             | scale_color_component(color->blue, scr->w_visual->blue_mask);
		return True;
	}

	if (!XAllocColor(dpy, colormap, color)) {
		wwarning(_("could not allocate color \"%s\""), color_name);
		return False;
	}
	return True;
}

int wGetColor(WScreen *scr, const char *color_name, XColor *color)
{
	return wGetColorForColormap(scr, scr->w_colormap, color_name, color);
}

void wFreeColor(WScreen * scr, unsigned long pixel)
{
	if (scr->w_visual->class == TrueColor)
		return;

	if (pixel != scr->white_pixel && pixel != scr->black_pixel) {
		unsigned long colors[1];

		colors[0] = pixel;
		XFreeColors(dpy, scr->w_colormap, colors, 1, 0);
	}
}
