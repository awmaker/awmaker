/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef WMCORE_H_
#define WMCORE_H_

#include "screen.h"

typedef struct WStacking {
	struct _WCoreWindow *above;
	struct _WCoreWindow *under;
	short window_level;
	struct _WCoreWindow *child_of;	/* owner for transient window */
} WStacking;

typedef struct _WCoreWindow {
	Window window;

	WObjDescriptor descriptor;
	WStacking *stacking;		/* window stacking information */
} WCoreWindow;

void wCoreConfigure(WCoreWindow *core, int req_x, int req_y,
		    int req_w, int req_h);

WCoreWindow *wcore_create();
void wcore_destroy(WCoreWindow *core);

void wcore_map_toplevel(WCoreWindow *core, virtual_screen *vscr, int x, int y,
			int width, int height,
			int bwidth, int depth, Visual *visual,
			Colormap colormap, WMPixel border_pixel);
void wcore_map(WCoreWindow *core, WCoreWindow *parent, virtual_screen *vscr,
	       int x, int y, int width, int height, int bwidth,
	       int depth, Visual *visual, Colormap colormap);
void wcore_unmap(WCoreWindow *core);
#endif
