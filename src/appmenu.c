/* appmenu.c- application defined menu
 *
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
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "WindowMaker.h"
#include "menu.h"
#include "actions.h"
#include "appmenu.h"
#include "framewin.h"


typedef struct {
	short code;
	short tag;
	Window window;
} WAppMenuData;

enum {
	wmBeginMenu = 1,
	wmEndMenu = 2,
	wmNormalItem = 10,
	wmDoubleItem = 11,
	wmSubmenuItem = 12
};

static void notifyClient(WMenu *menu, WMenuEntry *entry)
{
	WAppMenuData *data = entry->clientdata;
	XEvent event;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	event.xclient.type = ClientMessage;
	event.xclient.message_type = w_global.atom.wmaker.menu;
	event.xclient.format = 32;
	event.xclient.display = dpy;
	event.xclient.window = data->window;
	event.xclient.data.l[0] = w_global.timestamp.last_event;
	event.xclient.data.l[1] = 1;
	event.xclient.data.l[2] = data->tag;
	event.xclient.data.l[3] = 0;
	XSendEvent(dpy, data->window, False, NoEventMask, &event);
	XFlush(dpy);
}

static WMenu *parseMenuCommand(virtual_screen *vscr, Window win, char **slist, int count, int *index)
{
	WMenu *menu;
	int command;
	int code, pos;
	char title[300];
	char rtext[300];

	if (sscanf(slist[*index], "%i %i %n", &command, &code, &pos) < 2 || command != wmBeginMenu) {
		wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"), slist[*index], win);
		return NULL;
	}

	if (wstrlcpy(title, &slist[*index][pos], sizeof(title)) >= sizeof(title)) {
		wwarning("appmenu: menu command size exceeded in window %lx", win);
		return NULL;
	}

	menu = menu_create(vscr, title);
	menu->flags.app_menu = 1;
	menu_map(menu);

	*index += 1;
	while (*index < count) {
		int ecode, etag, enab;

		if (sscanf(slist[*index], "%i", &command) != 1) {
			wMenuDestroy(menu);
			wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"), slist[*index], win);
			return NULL;
		}

		if (command == wmEndMenu) {
			*index += 1;
			break;

		} else if (command == wmNormalItem || command == wmDoubleItem) {
			WAppMenuData *data;
			WMenuEntry *entry;

			if (command == wmNormalItem) {
				if (sscanf(slist[*index], "%i %i %i %i %n",
					   &command, &ecode, &etag, &enab, &pos) != 4 || ecode != code) {
					wMenuDestroy(menu);
					wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"),
						 slist[*index], win);
					return NULL;
				}
				wstrlcpy(title, &slist[*index][pos], sizeof(title));
				rtext[0] = 0;
			} else {
				if (sscanf(slist[*index], "%i %i %i %i %s %n",
					   &command, &ecode, &etag, &enab, rtext, &pos) != 5 || ecode != code) {
					wMenuDestroy(menu);
					wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"),
						 slist[*index], win);
					return NULL;
				}
				wstrlcpy(title, &slist[*index][pos], sizeof(title));
			}
			data = malloc(sizeof(WAppMenuData));
			if (data == NULL) {
				wwarning(_("appmenu: out of memory creating menu for window %lx"), win);
				wMenuDestroy(menu);
				return NULL;
			}
			data->code = code;
			data->tag = etag;
			data->window = win;
			entry = wMenuAddCallback(menu, title, notifyClient, data);
			if (!entry) {
				wMenuDestroy(menu);
				wwarning(_("appmenu: out of memory creating menu for window %lx"), win);
				wfree(data);
				return NULL;
			}
			if (rtext[0] != 0)
				entry->rtext = wstrdup(rtext);
			else
				entry->rtext = NULL;
			entry->free_cdata = free;
			*index += 1;

		} else if (command == wmSubmenuItem) {
			int ncode;
			WMenuEntry *entry;
			WMenu *submenu;

			if (sscanf(slist[*index], "%i %i %i %i %i %n",
				   &command, &ecode, &etag, &enab, &ncode, &pos) != 5 || ecode != code) {
				wMenuDestroy(menu);
				wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"), slist[*index], win);

				return NULL;
			}
			wstrlcpy(title, &slist[*index][pos], sizeof(title));
			*index += 1;

			submenu = parseMenuCommand(vscr, win, slist, count, index);

			entry = wMenuAddCallback(menu, title, NULL, NULL);

			if (!entry) {
				wMenuDestroy(menu);
				wMenuDestroy(submenu);
				wwarning(_("appmenu: out of memory creating menu for window %lx"), win);
				return NULL;
			}

			wMenuEntrySetCascade_create(menu, entry, submenu);
		} else {
			wMenuDestroy(menu);
			wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"), slist[*index], win);
			return NULL;
		}
	}

	return menu;
}

void create_app_menu(virtual_screen *vscr, WApplication *wapp)
{
	WWindow *wwin = NULL;
	Window window = wapp->main_window;
	XTextProperty text_prop;
	int count, i;
	char **slist;
	WMenu *menu;

	wwin = wapp->main_window_desc;
	if (!wwin)
		return;

	if (!XGetTextProperty(dpy, window, &text_prop, w_global.atom.wmaker.menu))
		return;

	if (!XTextPropertyToStringList(&text_prop, &slist, &count) || count < 1) {
		XFree(text_prop.value);
		return;
	}

	XFree(text_prop.value);
	if (strcmp(slist[0], "WMMenu 0") != 0) {
		wwarning(_("appmenu: unknown version of WMMenu in window %lx: %s"), window, slist[0]);
		XFreeStringList(slist);
		return;
	}

	i = 1;
	menu = parseMenuCommand(vscr, window, slist, count, &i);
	if (menu)
		menu->parent = NULL;

	XFreeStringList(slist);

	wMenuRealize(menu);
	wAppMenuMap(menu, wwin);
	wapp->app_menu = menu;
}

void wAppMenuMap(WMenu *menu, WWindow *wwin)
{
	int x, min;

	if (!menu || !wwin)
		return;

	if (menu->flags.mapped)
		return;

	x = 0;
	min = 20;	/* Keep at least 20 pixels visible */

	if (wPreferences.focus_mode != WKF_CLICK) {
		if (wwin->frame_x > min)
			x = wwin->frame_x - menu->frame->width;
		else
			x = min - menu->frame->width;
	}

	menu->x_pos = x;
	menu->y_pos = wwin->frame_y;

	wMenuMapAt(wwin->vscr, menu, False);
}

void destroy_app_menu(WApplication *wapp)
{
	if (!wapp || !wapp->app_menu)
		return;

	wMenuDestroy(wapp->app_menu);
	wapp->app_menu = NULL;
}
