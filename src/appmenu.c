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

enum {
	wmSelectItem = 1
};

static void sendMessage(Window window, int what, int tag)
{
	XEvent event;

	event.xclient.type = ClientMessage;
	event.xclient.message_type = w_global.atom.wmaker.menu;
	event.xclient.format = 32;
	event.xclient.display = dpy;
	event.xclient.window = window;
	event.xclient.data.l[0] = w_global.timestamp.last_event;
	event.xclient.data.l[1] = what;
	event.xclient.data.l[2] = tag;
	event.xclient.data.l[3] = 0;
	XSendEvent(dpy, window, False, NoEventMask, &event);
	XFlush(dpy);
}

static void notifyClient(WMenu * menu, WMenuEntry * entry)
{
	WAppMenuData *data = entry->clientdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	sendMessage(data->window, wmSelectItem, data->tag);
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

	menu = menu_create(title);
	if (!menu)
		return NULL;

	menu->flags.app_menu = 1;
	menu_map(menu, vscr);

	*index += 1;
	while (*index < count) {
		int ecode, etag, enab;

		if (sscanf(slist[*index], "%i", &command) != 1) {
			wMenuDestroy(menu, True);
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
					wMenuDestroy(menu, True);
					wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"),
						 slist[*index], win);
					return NULL;
				}
				wstrlcpy(title, &slist[*index][pos], sizeof(title));
				rtext[0] = 0;
			} else {
				if (sscanf(slist[*index], "%i %i %i %i %s %n",
					   &command, &ecode, &etag, &enab, rtext, &pos) != 5 || ecode != code) {
					wMenuDestroy(menu, True);
					wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"),
						 slist[*index], win);
					return NULL;
				}
				wstrlcpy(title, &slist[*index][pos], sizeof(title));
			}
			data = malloc(sizeof(WAppMenuData));
			if (data == NULL) {
				wwarning(_("appmenu: out of memory creating menu for window %lx"), win);
				wMenuDestroy(menu, True);
				return NULL;
			}
			data->code = code;
			data->tag = etag;
			data->window = win;
			entry = wMenuAddCallback(menu, title, notifyClient, data);
			if (!entry) {
				wMenuDestroy(menu, True);
				wwarning(_("appmenu: out of memory creating menu for window %lx"), win);
				free(data);
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
				wMenuDestroy(menu, True);
				wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"), slist[*index], win);

				return NULL;
			}
			wstrlcpy(title, &slist[*index][pos], sizeof(title));
			*index += 1;

			submenu = parseMenuCommand(vscr, win, slist, count, index);

			entry = wMenuAddCallback(menu, title, NULL, NULL);

			if (!entry) {
				wMenuDestroy(menu, True);
				wMenuDestroy(submenu, True);
				wwarning(_("appmenu: out of memory creating menu for window %lx"), win);
				return NULL;
			}

			wMenuEntrySetCascade_create(menu, entry, submenu);
		} else {
			wMenuDestroy(menu, True);
			wwarning(_("appmenu: bad menu entry \"%s\" in window %lx"), slist[*index], win);
			return NULL;
		}
	}

	return menu;
}

WMenu *wAppMenuGet(virtual_screen *vscr, Window window)
{
	XTextProperty text_prop;
	int count, i;
	char **slist;
	WMenu *menu;

	if (!XGetTextProperty(dpy, window, &text_prop, w_global.atom.wmaker.menu))
		return NULL;

	if (!XTextPropertyToStringList(&text_prop, &slist, &count) || count < 1) {
		XFree(text_prop.value);
		return NULL;
	}

	XFree(text_prop.value);
	if (strcmp(slist[0], "WMMenu 0") != 0) {
		wwarning(_("appmenu: unknown version of WMMenu in window %lx: %s"), window, slist[0]);
		XFreeStringList(slist);
		return NULL;
	}

	i = 1;
	menu = parseMenuCommand(vscr, window, slist, count, &i);
	if (menu)
		menu->parent = NULL;

	XFreeStringList(slist);

	wMenuRealize(menu);
	return menu;
}

void wAppMenuDestroy(WMenu *menu)
{
	if (menu)
		wMenuDestroy(menu, True);
}

void wAppMenuMap(WMenu *menu, WWindow *wwin)
{
	int x, min;

	if (!menu)
		return;

	x = 0;
	min = 20;	/* Keep at least 20 pixels visible */

	if (wwin && (wPreferences.focus_mode != WKF_CLICK)) {
		if (wwin->frame_x > min)
			x = wwin->frame_x - menu->frame->core->width;
		else
			x = min - menu->frame->core->width;
	}

	if (!menu->flags.mapped)
		wMenuMapAt(wwin->vscr, menu, x, wwin->frame_y, False);
}

void wAppMenuUnmap(WMenu *menu)
{
	if (menu)
		wMenuUnmap(menu);
}
