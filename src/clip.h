/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMCLIP_H_
#define WMCLIP_H_

WDock *clip_create(virtual_screen *vscr, WMPropList *state);
Bool clip_attach_icon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon);
Bool clip_snap_icon(WDock *dock, WAppIcon *icon, int req_x, int req_y, int *ret_x, int *ret_y, int redocking);
void clip_autolaunch(int vscrno);
void wClipIconPaint(WAppIcon *aicon);
RImage *wClipMakeTile(RImage *normalTile);
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
void clip_destroy(WDock *dock);
#endif
