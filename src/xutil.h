/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _WM_XUTIL_H_
#define _WM_XUTIL_H_

void FormatXError(Display *dpy, XErrorEvent *error, char *buffer, int size);


void RequestSelection(Display *dpy, Window requestor, Time timestamp);


char *GetSelection(Display *dpy, Window requestor);


#endif
