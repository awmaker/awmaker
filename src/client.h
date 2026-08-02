/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMCLIENT_H_
#define WMCLIENT_H_

void wClientSetState(WWindow *wwin, int state, Window icon_win);

void wClientRestore(WWindow *wwin);
void wClientConfigure(WWindow *wwin, XConfigureRequestEvent *xcre);
void wClientGetGravityOffsets(WWindow *wwin, int *ofs_x, int *ofs_y);
void wClientSendProtocol(WWindow *wwin, Atom protocol, Time time);
void wClientKill(WWindow *wwin);
void wClientCheckProperty(WWindow *wwin, XPropertyEvent *event);


void wClientGetNormalHints(WWindow *wwin, XWindowAttributes *wattribs,
                           Bool geometry, int *x, int *y, unsigned *width,
                           unsigned *height);
void GetColormapWindows(WWindow *wwin);

#endif
