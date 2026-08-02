/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMACTIONS_H_
#define WMACTIONS_H_

#include "window.h"

#define MAX_HORIZONTAL         (1 << 0)
#define MAX_VERTICAL           (1 << 1)
#define MAX_LEFTHALF           (1 << 2)
#define MAX_RIGHTHALF          (1 << 3)
#define MAX_TOPHALF            (1 << 4)
#define MAX_BOTTOMHALF         (1 << 5)
#define MAX_MAXIMUS            (1 << 6)
#define MAX_IGNORE_XINERAMA    (1 << 7)
#define MAX_KEYBOARD           (1 << 8)

#define SAVE_GEOMETRY_X        (1 << 0)
#define SAVE_GEOMETRY_Y        (1 << 1)
#define SAVE_GEOMETRY_WIDTH    (1 << 2)
#define SAVE_GEOMETRY_HEIGHT   (1 << 3)
#define SAVE_GEOMETRY_ALL      SAVE_GEOMETRY_X | SAVE_GEOMETRY_Y | SAVE_GEOMETRY_WIDTH | SAVE_GEOMETRY_HEIGHT

void wSetFocusTo(virtual_screen *vscr, WWindow *wwin);

int wMouseMoveWindow(WWindow *wwin, XEvent *ev);
int wKeyboardMoveResizeWindow(WWindow *wwin);

void wMouseResizeWindow(WWindow *wwin, XEvent *ev);

void wShadeWindow(WWindow *wwin);
void wUnshadeWindow(WWindow *wwin);

void wIconifyWindow(WWindow *wwin);
void wDeiconifyWindow(WWindow *wwin);

void wSelectWindows(virtual_screen *vscr, XEvent *ev);

void wSelectWindow(WWindow *wwin, Bool flag);
void wUnselectWindows(virtual_screen *vscr);

void wMaximizeWindow(WWindow *wwin, int directions, int head);
void wUnmaximizeWindow(WWindow *wwin);
void handleMaximize(WWindow *wwin, int directions);

void wHideAll(virtual_screen *vsrc);
void wHideOtherApplications(WWindow *wwin);
void wShowAllWindows(virtual_screen *vscr);

void wHideApplication(WApplication *wapp);
void wUnhideApplication(WApplication *wapp, Bool miniwindows,
                        Bool bringToCurrentWS);

void wRefreshDesktop(virtual_screen *vscr);

void wArrangeIcons(virtual_screen *vscr, Bool arrangeAll);

void wMakeWindowVisible(WWindow *wwin);

void wFullscreenWindow(WWindow *wwin);
void wUnfullscreenWindow(WWindow *wwin);

void animateResize(virtual_screen *vscr, int x, int y, int w, int h, int fx, int fy, int fw, int fh);
void update_saved_geometry(WWindow *wwin);

void movePionterToWindowCenter(WWindow *wwin);
void moveBetweenHeads(WWindow *wwin, int direction);
#endif

