/* dock.h- Dock module for AWindowMaker
 *
 *  AWindowMaker window manager
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
