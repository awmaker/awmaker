/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMRESOURCES_H_
#define WMRESOURCES_H_

#include "screen.h"

int wGetColorForColormap(WScreen *scr, Colormap colormap, const char *color_name, XColor *color);
int wGetColor(WScreen *scr, const char *color_name, XColor *color);
void wFreeColor(WScreen *scr, unsigned long pixel);

#endif
