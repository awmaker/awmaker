/*
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
	int width;			/* size of the window */
	int height;
	virtual_screen *vscr;		/* ptr to screen of the window */

	WObjDescriptor descriptor;
	WStacking *stacking;		/* window stacking information */
} WCoreWindow;

void wCoreConfigure(WCoreWindow *core, int req_x, int req_y,
		    int req_w, int req_h);

WCoreWindow *wcore_create(int width, int height);
void wcore_destroy(WCoreWindow *core);

void wcore_map_toplevel(WCoreWindow *core, virtual_screen *vscr, int x, int y,
			int bwidth, int depth, Visual *visual,
			Colormap colormap, WMPixel border_pixel);
void wcore_map(WCoreWindow *core, WCoreWindow *parent, virtual_screen *vscr,
	       int x, int y, int bwidth, int depth, Visual *visual,
	       Colormap colormap);
#endif
