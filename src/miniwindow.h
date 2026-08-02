/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMMINIWINDOW_H
#define WMMINIWINDOW_H

#include "window.h"
#include "icon.h"

typedef struct WMiniWindow {
	struct WIcon *icon;        /* Window icon when miminized else is NULL! */
	int icon_x, icon_y;        /* Position of the icon */
	int icon_w, icon_h;        /* Used by minimize animation */
	RImage *net_icon_image;    /* Window Image */
} WMiniWindow;

WMiniWindow *miniwindow_create(void);
void miniwindow_destroy(WWindow *wwin);
void miniwindow_destroy_icon(WWindow *wwin);
void miniwindow_updatetitle(WWindow *wwin);
void miniwindow_removeIcon(WWindow *wwin);
void miniwindow_map(WWindow *wwin);
void miniwindow_unmap(WWindow *wwin);
void miniwindow_iconupdate(WWindow *wwin);
void miniwindow_icon_show(WWindow *wwin);

void miniwindow_Expose(WObjDescriptor *desc, XEvent *event);
void miniwindow_MouseDown(WObjDescriptor *desc, XEvent *event);

int miniwindow_get_xpos(WWindow *wwin);
int miniwindow_get_ypos(WWindow *wwin);

#endif /* WMMINIWINDOW_H */
