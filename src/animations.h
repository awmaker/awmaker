/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMANIMATIONS_H
#define WMANIMATIONS_H

#include "window.h"

#define UNSHADE   0
#define SHADE     1

void animation_shade(WWindow *wwin, Bool what);
void animation_catchevents(void);
void animateResize(virtual_screen *vscr, int x, int y, int w, int h, int fx, int fy, int fw, int fh);
void animation_maximize(WWindow *wwin);
void animation_minimize(WWindow *wwin);
void animation_hide(WWindow *wwin, int icon_x, int icon_y);
void animation_unhide(WWindow *wwin, int icon_x, int icon_y);
void animation_slide_window(Window win, int icon_x, int icon_y, int x, int y);
int animation_iconify_window(WWindow *wwin);
int animation_deiconify_window(WWindow *wwin);

#endif /* WMANIMATIONS_H */
