/* input.h
 *
 *  Copyright (c) 2017 Rodolfo García Peñas <kix@kix.es>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _WM_INPUT_H_
#define _WM_INPUT_H_

#include "wconfig.h"

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
