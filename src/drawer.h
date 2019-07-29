/* drawer.h- Drawer module for AWindowMaker
 *
 *  AWindowMaker window manager
 *
 *  Copyright (c) 2019 Rodolfo García Peñas
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

#ifndef WMDRAWER_H_
#define WMDRAWER_H_

WDock *drawer_create(virtual_screen *vscr, const char *name);
void drawerDestroy(WDock *drawer);
Bool drawer_attach_icon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon);
Bool drawer_snap_icon(WDock *dock, WAppIcon *icon, int req_x, int req_y, int *ret_x, int *ret_y, int redocking);
void drawers_autolaunch(int vscrno);
void wDrawerIconPaint(WAppIcon *aicon);
RImage *wDrawerMakeTile(virtual_screen *vscr, RImage *normalTile);
void drawer_enter_notify(WObjDescriptor *desc, XEvent *event);
void drawer_leave_notify(WObjDescriptor *desc, XEvent *event);
void drawer_leave(WDock *dock);
void drawer_icon_mouse_down(WObjDescriptor *desc, XEvent *event);
int wIsADrawer(WAppIcon *aicon);
void swapDrawers(virtual_screen *vscr, int new_x);
WDock *getDrawer(virtual_screen *vscr, int y_index);
void wDrawerFillTheGap(WDock *drawer, WAppIcon *aicon, Bool redocking);
void wDrawersSaveState(virtual_screen *vscr);
void wDrawersRestoreState(virtual_screen *vscr);
void wDrawersRestoreState_map(virtual_screen *vscr);
int addADrawer(virtual_screen *vscr);
void removeDrawerCallback(WMenu *menu, WMenuEntry *entry);
void addADrawerCallback(WMenu *menu, WMenuEntry *entry);
WDock *drawerRestoreState(virtual_screen *vscr, WMPropList *drawer_state);
int indexOfHole(WDock *drawer, WAppIcon *moving_aicon, int redocking);
#endif
