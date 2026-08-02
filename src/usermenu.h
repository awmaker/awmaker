/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifdef USER_MENU

#ifndef _WUSERMENU_H_
#define _WUSERMENU_H_

void create_user_menu(virtual_screen *vscr, WApplication *wapp);
void destroy_user_menu(WApplication *wapp);

#endif
#endif /* USER_MENU */
