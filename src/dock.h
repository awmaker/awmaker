/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMDOCK_H_
#define WMDOCK_H_

WDock *dock_create(virtual_screen *vscr);
Bool dock_attach_icon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon);
Bool dock_snap_icon(WDock *dock, WAppIcon *icon, int req_x, int req_y, int *ret_x, int *ret_y, int redocking);
void dock_autolaunch(int vscrno);
void dockIconPaint(WAppIcon *btn);
void dock_enter_notify(WObjDescriptor *desc, XEvent *event);
void dock_leave_notify(WObjDescriptor *desc, XEvent *event);
void dock_leave(WDock *dock);
void dock_icon_mouse_down(WObjDescriptor *desc, XEvent *event);
void dock_map(WDock *dock, WMPropList *dock_state);
void dock_unmap(WDock *dock);
void wDockSaveState(virtual_screen *vscr, WMPropList *old_state);
void dock_icon_mouse_down(WObjDescriptor *desc, XEvent *event);
void dock_icon_expose(WObjDescriptor *desc, XEvent *event);
#endif
