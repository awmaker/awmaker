/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _WM_INPUT_H_
#define _WM_INPUT_H_

#include "awconfig.h"

/* Keyboard definitions */
extern unsigned int _NumLockMask;
extern unsigned int _ScrollLockMask;

/* Keyboard functions */
void wHackedGrabButton(Display *dpy, unsigned int button, unsigned int modifiers,
		       Window grab_window, Bool owner_events,
		       unsigned int event_mask, int pointer_mode,
		       int keyboard_mode, Window confine_to, Cursor cursor);

#ifdef NUMLOCK_HACK
void wHackedGrabKey(Display *dpy, int keycode, unsigned int modifiers,
		    Window grab_window, Bool owner_events, int pointer_mode,
		    int keyboard_mode);
#endif

void getOffendingModifiers(Display *dpy);

#endif
