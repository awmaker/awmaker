/* miniwindow.c - Window Icon code
 *
 *  AWindow Maker window manager
 *
 *  Copyright (c) 2019 Rodolfo García Peñas (kix)
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#include "WindowMaker.h"
#include "miniwindow.h"
#include "misc.h"

static void miniwindow_create_minipreview_showerror(WWindow *wwin);

WIcon *miniwindow_create_icon(WWindow *wwin)
{
	WIcon *icon = NULL;

	icon = icon_create_core(wwin->vscr);
	icon->owner = wwin;
	icon->tile_type = TILE_NORMAL;
	set_icon_image_from_database(icon, wwin->wm_instance, wwin->wm_class, NULL);

#ifdef NO_MINIWINDOW_TITLES
	icon->show_title = 0;
#else
	icon->show_title = 1;
#endif

	return icon;
}

void miniwindow_create_minipreview(WWindow *wwin)
{
	Pixmap pixmap;
	int ret;

	ret = create_minipixmap_for_wwindow(wwin->vscr, wwin, &pixmap);
	if (ret) {
		miniwindow_create_minipreview_showerror(wwin);
		return;
	}

	if (wwin->icon->mini_preview != None)
		XFreePixmap(dpy, wwin->icon->mini_preview);

	wwin->icon->mini_preview = pixmap;
}

static void miniwindow_create_minipreview_showerror(WWindow *wwin)
{
	const char *title;
	char title_buf[32];

	if (wwin->title) {
		title = wwin->title;
	} else {
		snprintf(title_buf, sizeof(title_buf), "(id=0x%lx)", wwin->client_win);
		title = title_buf;
	}

	wwarning(_("creation of mini-preview failed for window \"%s\""), title);
}
