/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GEOMVIEW_H
#define GEOMVIEW_H

typedef struct W_GeometryView WGeometryView;


WGeometryView *WCreateGeometryView(WMScreen *scr);

void WSetGeometryViewShownPosition(WGeometryView *gview, int x, int y);

void WSetGeometryViewShownSize(WGeometryView *gview,
                               unsigned width, unsigned height);

#endif  /* GEOMVIEW_H */
