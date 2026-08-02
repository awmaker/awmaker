/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMWINMENU_H
#define WMWINMENU_H

void window_menu_create(virtual_screen *vscr);
void OpenWindowMenu(WWindow *wwin, int x, int y, int keyboard);
void windowmenu_at_switchmenu_open(WWindow *wwin, int x, int y);
void DestroyWindowMenu(virtual_screen *vscr);

#endif /* WMWINMENU_H */
