/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMSWITCHMENU_H
#define WMSWITCHMENU_H

void switchmenu_additem(WMenu *menu, WWindow *wwin);
void switchmenu_delitem(WMenu *menu, WWindow *wwin);
void switchmenu_handle_notification(WMenu *menu, const char *name, int workspace);
void switchmenu_handle_notification_wwin(WMenu *menu, WWindow *wwin,
					 const char *name, char *data);

WMenu *switchmenu_create(virtual_screen *vscr);
void switchmenu_destroy(virtual_screen *vscr);
void OpenSwitchMenu(virtual_screen *vscr, int x, int y, int keyboard);

WPixmap *switchMenuIconForWindow(virtual_screen *vscr, WWindow *wwin);

#endif /* WMSWITCHMENU_H */
