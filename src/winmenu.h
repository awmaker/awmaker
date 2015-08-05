/*
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *  Copyright (c) 2013 Window Maker Team
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

#ifndef WMWINMENU_H
#define WMWINMENU_H

void window_menu_create(virtual_screen *vscr);
void OpenWindowMenu(WWindow *wwin, int x, int y, int keyboard);
void windowmenu_at_switchmenu_open(WWindow *wwin, int x, int y);
void OpenMiniwindowMenu(WWindow *wwin, int x, int y);
void CloseWindowMenu(virtual_screen *vscr);
void DestroyWindowMenu(virtual_screen *vscr);

#endif /* WMWINMENU_H */
