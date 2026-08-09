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

#ifdef USER_MENU

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
#include "keybind.h"
#include "xmodifier.h"
#include "misc.h"
#include "appmenu.h"

#define MAX_SHORTCUT_LENGTH 64


typedef struct {
	virtual_screen *screen;
	WShortKey *key;
	int key_no;
} WUserMenuData;

static void notifyClient(WMenu *menu, WMenuEntry *entry)
{
	XEvent event;
	WUserMenuData *data = entry->clientdata;
	virtual_screen *vscr = data->screen;
	Window window;
	int i;

	(void) menu;

	window = vscr->window.focused->client_win;

	for (i = 0; i < data->key_no; i++) {
		event.xkey.type = KeyPress;
		event.xkey.display = dpy;
		event.xkey.window = window;
		event.xkey.root = DefaultRootWindow(dpy);
		event.xkey.subwindow = (Window) None;
		event.xkey.x = 0x0;
		event.xkey.y = 0x0;
		event.xkey.x_root = 0x0;
		event.xkey.y_root = 0x0;
		event.xkey.keycode = data->key[i].keycode;
		event.xkey.state = data->key[i].modifier;
		event.xkey.same_screen = True;
		event.xkey.time = CurrentTime;
		if (XSendEvent(dpy, window, False, KeyPressMask, &event)) {
			event.xkey.type = KeyRelease;
			event.xkey.time = CurrentTime;
			XSendEvent(dpy, window, True, KeyReleaseMask, &event);
		}
	}
}

static void removeUserMenudata(void *menudata)
{
	WUserMenuData *data = menudata;
	if (data->key)
		wfree(data->key);

	wfree(data);
}

static WUserMenuData *convertShortcuts(virtual_screen *vscr, WMPropList *shortcut)
{
	WUserMenuData *data;
	KeySym ksym;
	char *k, buf[MAX_SHORTCUT_LENGTH], *b;
	int keycount, i, j, mod;

	if (WMIsPLString(shortcut))
		keycount = 1;
	else if (WMIsPLArray(shortcut))
		keycount = WMGetPropListItemCount(shortcut);
	else
		return NULL;

	data = wmalloc(sizeof(WUserMenuData));
	if (!data)
		return NULL;

	data->key = wmalloc(sizeof(WShortKey) * keycount);
	if (!data->key) {
		wfree(data);
		return NULL;
	}

	for (i = 0, j = 0; i < keycount; i++) {
		data->key[j].modifier = 0;
		if (WMIsPLArray(shortcut))
			wstrlcpy(buf, WMGetFromPLString(WMGetFromPLArray(shortcut, i)), MAX_SHORTCUT_LENGTH);
		else
			wstrlcpy(buf, WMGetFromPLString(shortcut), MAX_SHORTCUT_LENGTH);

		b = (char *) buf;

		while ((k = strchr(b, '+')) != NULL) {
			*k = 0;
			mod = wXModifierFromKey(b);
			if (mod < 0)
				break;

			data->key[j].modifier |= mod;
			b = k + 1;
		}

		ksym = XStringToKeysym(b);
		if (ksym == NoSymbol)
			continue;

		data->key[j].keycode = XKeysymToKeycode(dpy, ksym);
		if (data->key[j].keycode)
			j++;
	}

	/* get key */
	if (!j) {
		puts("fatal j");
		wfree(data->key);
		wfree(data);
		return NULL;
	}

	data->key_no = j;
	data->screen = vscr;

	return data;
}

static void add_shortcut_entry(virtual_screen *vscr, WMenu *menu, WMPropList *title,
			       WMPropList *elem, WMPropList *params, int idx)
{
	WMPropList *instances = 0;
	WUserMenuData *data;
	WMenuEntry *entry;

	data = convertShortcuts(vscr, params);

	if (!data)
		return;

	entry = wMenuAddCallback(menu, WMGetFromPLString(title), notifyClient, data);
	if (!entry)
		return;

	if (WMIsPLString(params))
		entry->rtext = GetShortcutString(WMGetFromPLString(params));

	entry->free_cdata = removeUserMenudata;

	if (WMGetPropListItemCount(elem) < 4)
		return;

	instances = WMGetFromPLArray(elem, idx++);
	if (!WMIsPLArray(instances))
		return;

	if (WMGetPropListItemCount(instances))
		entry->instances = WMRetainPropList(instances);
}

static WMenu *configureUserMenu(virtual_screen *vscr, WMPropList *plum)
{
	char *mtitle;
	WMenu *menu = NULL;
	WMPropList *elem, *title, *command, *params = NULL;
	int count, i;

	if (!plum)
		return NULL;

	if (!WMIsPLArray(plum))
		return NULL;

	count = WMGetPropListItemCount(plum);
	if (!count)
		return NULL;

	elem = WMGetFromPLArray(plum, 0);
	if (!WMIsPLString(elem))
		return NULL;

	mtitle = WMGetFromPLString(elem);
	menu = menu_create(vscr, mtitle);
	menu->flags.app_menu = 1;
	menu_map(menu);

	for (i = 1; i < count; i++) {
		elem = WMGetFromPLArray(plum, i);
		if (WMIsPLArray(WMGetFromPLArray(elem, 1))) {
			WMenu *submenu;
			WMenuEntry *mentry = NULL;

			submenu = configureUserMenu(vscr, elem);
			if (submenu)
				mentry = wMenuAddCallback(menu, submenu->title, NULL, NULL);

			wMenuEntrySetCascade_create(menu, mentry, submenu);
		} else {
			int idx = 0;

			title = WMGetFromPLArray(elem, idx++);
			command = WMGetFromPLArray(elem, idx++);
			if (WMGetPropListItemCount(elem) >= 3)
				params = WMGetFromPLArray(elem, idx++);

			if (!title || !command)
				return menu;

			if (!strcmp("SHORTCUT", WMGetFromPLString(command)))
				add_shortcut_entry(vscr, menu, title, elem, params, idx);
		}
	}

	return menu;
}

void create_user_menu(virtual_screen *vscr, WApplication *wapp)
{
	WWindow *wwin = NULL;
	WMenu *menu = NULL;
	WMPropList *plum;
	char *tmp, *file_name = NULL;
	int len;

	/*
	 * TODO: kix
	 * There is a bug in wmaker about the windows in the WApplication
	 * The original Window includes an WWindow valid with all the info
	 * but WApplication creates a new WWindow and some info is lost, like
	 * the position of the window on the screen.
	 * instance and class are set, we can use it here.
	 * We need avoid create the WWindow in the WApplication and select the
	 * previosly WWindow. With one-window applications is easy, but with
	 * applications with multiple windows, is more difficult.
	 */
	wwin = wapp->main_window_desc;
	if (!wwin || !wwin->wm_instance || !wwin->wm_class)
		return;

	len = strlen(wwin->wm_instance) + strlen(wwin->wm_class) + 7;
	tmp = wmalloc(len);
	snprintf(tmp, len, "%s.%s.menu", wwin->wm_instance, wwin->wm_class);
	file_name = wfindfile(DEF_USER_MENU_PATHS, tmp);
	wfree(tmp);
	if (!file_name)
		return;

	plum = WMReadPropListFromFile(file_name);
	if (!plum) {
		wfree(file_name);
		return;
	}

	wfree(file_name);

	menu = configureUserMenu(vscr, plum);
	WMReleasePropList(plum);
	wMenuRealize(menu);

	/*
	 * TODO: kix
	 * Because the preovious comment, the info in wwin about possition is wrong
	 * and contains values (x = 0, y = 0). If we move the window, values do not
	 * change too. We need solve the WApplication problem to solve this problem.
	 * Now, the menu window is painted in 0,0
	 */

	/* Using the same function that app_menu */
	wAppMenuMap(menu, wwin);

	wapp->user_menu = menu;
}

void destroy_user_menu(WApplication *wapp)
{
	if (!wapp || !wapp->user_menu)
		return;

	wMenuUnmap(wapp->user_menu);
	wMenuDestroy(wapp->user_menu);
	wapp->user_menu = NULL;
}

#endif				/* USER_MENU */
