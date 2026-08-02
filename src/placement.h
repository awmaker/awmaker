/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLACEMENT_H
#define PLACEMENT_H

WCoord *PlaceIcon(virtual_screen *vscr, int head);

/* Computes the intersecting length of two line sections */
int calcIntersectionLength(int p1, int l1, int p2, int l2);

/* Computes the intersecting area of two rectangles */
int calcIntersectionArea(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2);

void PlaceWindow(WWindow *wwin, int *x_ret, int *y_ret, unsigned width, unsigned height);

void InteractivePlaceWindow(WWindow *wwin, int *x_ret, int *y_ret, unsigned width, unsigned height);

#endif  /* PLACEMENT_H */
