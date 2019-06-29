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
