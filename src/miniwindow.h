/*
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

#ifndef WMMINIWINDOW_H
#define WMMINIWINDOW_H

#include "window.h"
#include "icon.h"

typedef struct WMiniWindow {
} WMiniWindow;

WMiniWindow *miniwindow_create(void);
void miniwindow_destroy(WWindow *wwin);
WIcon *miniwindow_create_icon(WWindow *wwin);
void miniwindow_create_minipreview(WWindow *wwin);
void miniwindow_icon_map1(WIcon *icon);
void miniwindow_icon_map2(WIcon *icon);
void miniwindow_destroy(WWindow *wwin);
void miniwindow_updatetitle(WWindow *wwin);
void miniwindow_removeIcon(WWindow *wwin);
void miniwindow_map(WWindow *wwin);
void miniwindow_unmap(WWindow *wwin);
void miniwindow_iconupdate(WWindow *wwin);

void miniwindow_Expose(WObjDescriptor *desc, XEvent *event);
void miniwindow_MouseDown(WObjDescriptor *desc, XEvent *event);

#endif /* WMMINIWINDOW_H */
