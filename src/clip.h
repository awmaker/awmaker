/* clip.h- Clip module for AWindowMaker
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

#ifndef WMCLIP_H_
#define WMCLIP_H_

WDock *clip_create(virtual_screen *vscr, WMPropList *state);
void clip_destroy(WDock *dock);
Bool clip_attach_icon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon);
Bool clip_snap_icon(WDock *dock, WAppIcon *icon, int req_x, int req_y, int *ret_x, int *ret_y, int redocking);
void clip_autolaunch(int vscrno);
void wClipIconPaint(WAppIcon *aicon);
RImage *wClipMakeTile(RImage *normalTile);
void restore_clip_position(WDock *dock, WMPropList *state);
void clip_enter_notify(WObjDescriptor *desc, XEvent *event);
void clip_leave_notify(WObjDescriptor *desc, XEvent *event);
void clip_leave(WDock *dock);
void clip_icon_mouse_down(WObjDescriptor *desc, XEvent *event);
void clip_map(WDock *dock, WMPropList *state);
void clip_unmap(WDock *dock);
WAppIcon *clip_icon_create(virtual_screen *vscr);
void clip_icon_map(virtual_screen *vscr);
void clip_icon_unmap(virtual_screen *vscr);
void clipAutoRaise(void *cdata);
void clipAutoExpand(void *cdata);
int wClipMakeIconOmnipresent(WAppIcon *aicon, int omnipresent);
void clipAutoLower(void *cdata);
void clip_icon_expose(WObjDescriptor *desc, XEvent *event);
void handleClipChangeWorkspace(virtual_screen *vscr, XEvent *event);
int getClipButton(int px, int py);
WMPropList *wClipSaveWorkspaceState(virtual_screen *vscr, int workspace);
void wClipUpdateForWorkspaceChange(virtual_screen *vscr, int workspace);
#endif
