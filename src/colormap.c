/* colormap.c - colormap handling code
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 1998-2003 Alfredo K. Kojima
 *
 *  This code slightly based on fvwm code,
 *  Copyright (c) Rob Nation and others
 *  but completely rewritten.
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
