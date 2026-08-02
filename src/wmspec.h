/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */



#ifndef _WMSPEC_H_
#define _WMSPEC_H_

#include "screen.h"
#include "window.h"
#include <X11/Xlib.h>

void wNETWMInitStuff(virtual_screen *vscr);
void wNETWMCleanup(WScreen *scr);
void wNETWMUpdateWorkarea(virtual_screen *vscr);
Bool wNETWMGetUsableArea(virtual_screen *vscr, int head, WArea *area);
void wNETWMCheckInitialClientState(WWindow *wwin);
void wNETWMCheckInitialFrameState(WWindow *wwin);
Bool wNETWMProcessClientMessage(XClientMessageEvent *event);
void wNETWMCheckClientHints(WWindow *wwin, int *layer, int *workspace);
void wNETWMCheckClientHintChange(WWindow *wwin, XPropertyEvent *event);
void wNETWMUpdateActions(WWindow *wwin, Bool del);
void wNETWMUpdateDesktop(virtual_screen *vscr);
void wNETWMPositionSplash(WWindow *wwin, int *x, int *y, int width, int height);
int wNETWMGetPidForWindow(Window window);
int wNETWMGetCurrentDesktopFromHint(WScreen *scr);
char *wNETWMGetWindowName(Window window);
void wNETFrameExtents(WWindow *wwin);
void wNETCleanupFrameExtents(WWindow *wwin);
RImage *get_window_image_from_x11(Window window);
#endif
