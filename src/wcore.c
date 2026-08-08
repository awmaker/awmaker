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

	/* A dimension <= 0 means "keep the current size" (matching upstream),
	 * so only request a size change for axes that carry a real value.
	 * The old code clamped < 0 to 0 and always set CWWidth|CWHeight, which
	 * would have sized a window down to 0 if a caller ever passed 0. */
	if (req_w > 0) {
		mask |= CWWidth;
		xwc.width = req_w;
	}

	if (req_h > 0) {
		mask |= CWHeight;
		xwc.height = req_h;
	}

	XConfigureWindow(dpy, core->window, mask, &xwc);
}
