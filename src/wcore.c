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

#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdlib.h>
#include <string.h>

#include "WindowMaker.h"
#include "wcore.h"

WCoreWindow *wcore_create()
{
	return wmalloc(sizeof(WCoreWindow));
}

void wcore_destroy(WCoreWindow *core)
{
	wfree(core);
}

void wcore_map_toplevel(WCoreWindow *core, virtual_screen *vscr, int x, int y,
			int width, int height, int bwidth, int depth,
			Visual *visual, Colormap colormap, WMPixel border_pixel)
{
	int vmask;
	XSetWindowAttributes attribs;

	vmask = CWBorderPixel | CWCursor | CWEventMask | CWOverrideRedirect | CWColormap;
	attribs.override_redirect = True;
	attribs.cursor = wPreferences.cursor[WCUR_NORMAL];
	attribs.background_pixmap = None;
	attribs.background_pixel = vscr->screen_ptr->black_pixel;
	attribs.border_pixel = border_pixel;
	attribs.event_mask = SubstructureRedirectMask | ButtonPressMask |
			     ButtonReleaseMask | ButtonMotionMask |
			     ExposureMask | EnterWindowMask | LeaveWindowMask;

	attribs.colormap = colormap;

	if (wPreferences.use_saveunders) {
		vmask |= CWSaveUnder;
		attribs.save_under = True;
	}

	core->window = XCreateWindow(dpy, vscr->screen_ptr->root_win, x, y, width, height,
				     bwidth, depth, CopyFromParent, visual, vmask, &attribs);
	core->descriptor.self = core;

	XClearWindow(dpy, core->window);
	XSaveContext(dpy, core->window, w_global.context.client_win, (XPointer) & core->descriptor);
}

void wcore_map(WCoreWindow *core, WCoreWindow *parent, virtual_screen *vscr,
	       int x, int y, int width, int height, int bwidth, int depth,
	       Visual *visual, Colormap colormap)
{
	int vmask;
	XSetWindowAttributes attribs;

	vmask = CWBorderPixel | CWCursor | CWEventMask | CWColormap;
	attribs.cursor = wPreferences.cursor[WCUR_NORMAL];
	attribs.background_pixmap = None;
	attribs.background_pixel = vscr->screen_ptr->black_pixel;
	attribs.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
			     ButtonReleaseMask | ButtonMotionMask |
			     ExposureMask | EnterWindowMask | LeaveWindowMask;
	attribs.colormap = colormap;
	core->window = XCreateWindow(dpy, parent->window, x, y, width, height, bwidth,
				     depth, CopyFromParent, visual, vmask, &attribs);

	core->descriptor.self = core;

	XSaveContext(dpy, core->window, w_global.context.client_win, (XPointer) & core->descriptor);
}

void wcore_unmap(WCoreWindow *core)
{
	if (core) {
		XDeleteContext(dpy, core->window, w_global.context.client_win);
		XDestroyWindow(dpy, core->window);
	}
}

void wCoreConfigure(WCoreWindow * core, int req_x, int req_y, int req_w, int req_h)
{
	XWindowChanges xwc;
	unsigned int mask;

	mask = CWX | CWY;
	xwc.x = req_x;
	xwc.y = req_y;

	if (req_w < 0)
		req_w = 0;

	if (req_h < 0)
		req_h = 0;

	mask |= CWWidth | CWHeight;
	xwc.width = req_w;
	xwc.height = req_h;

	XConfigureWindow(dpy, core->window, mask, &xwc);
}
