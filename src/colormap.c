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

#include "WindowMaker.h"
#include <X11/Xatom.h>

#include "window.h"
#include "framewin.h"
#include "colormap.h"


void wColormapInstallForWindow(virtual_screen *vscr, WWindow *wwin)
{
	int i, done = 0;
	XWindowAttributes attributes;
	Window xwin = None;

	vscr->screen_ptr->cmap_window = wwin;

	/* install colormap for all windows of the client */
	if (wwin && wwin->cmap_window_no > 0 && wwin->cmap_windows) {
		for (i = wwin->cmap_window_no - 1; i >= 0; i--) {
			Window w;

			w = wwin->cmap_windows[i];
			if (w == wwin->client_win)
				done = 1;

			XGetWindowAttributes(dpy, w, &attributes);
			if (attributes.colormap == None)
				attributes.colormap = vscr->screen_ptr->colormap;

			if (vscr->screen_ptr->current_colormap != attributes.colormap) {
				vscr->screen_ptr->current_colormap = attributes.colormap;
				/*
				 * ICCCM 2.0: some client requested permission
				 * to install colormaps by itself and we granted.
				 * So, we can't install any colormaps.
				 */
				if (!vscr->screen_ptr->flags.colormap_stuff_blocked)
					XInstallColormap(dpy, attributes.colormap);
			}
		}
	}

	if (!done) {
		if (wwin)
			xwin = wwin->client_win;
		else
			xwin = vscr->screen_ptr->root_win;

		attributes.colormap = None;

		if (xwin != None)
			XGetWindowAttributes(dpy, xwin, &attributes);

		if (attributes.colormap == None)
			attributes.colormap = vscr->screen_ptr->colormap;

		if (vscr->screen_ptr->current_colormap != attributes.colormap) {
			vscr->screen_ptr->current_colormap = attributes.colormap;
			if (!vscr->screen_ptr->flags.colormap_stuff_blocked)
				XInstallColormap(dpy, attributes.colormap);
		}
	}

	XSync(dpy, False);
}

void wColormapAllowClientInstallation(virtual_screen *vscr, Bool starting)
{
	vscr->screen_ptr->flags.colormap_stuff_blocked = starting;
	/*
	 * Client stopped managing the colormap stuff. Restore the colormap
	 * that would be installed if the client did not request colormap
	 * stuff.
	 */
	if (!starting) {
		XInstallColormap(dpy, vscr->screen_ptr->current_colormap);
		XSync(dpy, False);
	}
}
