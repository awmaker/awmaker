/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
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
