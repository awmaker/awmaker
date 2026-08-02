/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMSUPERFLUOUS_H
#define WMSUPERFLUOUS_H

#include "window.h"

void DoKaboom(virtual_screen *vscr, Window win, int x, int y);
Pixmap MakeGhostIcon(virtual_screen *vscr, Drawable drawable);
void DoWindowBirth(WWindow *wwin);

#endif
