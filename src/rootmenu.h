/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMROOTMENU_H
#define WMROOTMENU_H

Bool wRootMenuPerformShortcut(XEvent *event);
void wRootMenuBindShortcuts(Window window);
void OpenRootMenu(virtual_screen *vscr, int x, int y, int keyboard);
WMenu *create_rootmenu(virtual_screen *vscr);
void rootmenu_destroy(virtual_screen *vscr);
void rebindKeygrabs(virtual_screen *vscr);

#endif /* WMROOTMENU_H */
