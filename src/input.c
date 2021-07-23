/* input.c
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

#include <X11/Xutil.h>
#include "input.h"

unsigned int _NumLockMask;
unsigned int _ScrollLockMask;

/* Keyboard functions */
#ifdef NUMLOCK_HACK
void
wHackedGrabKey(Display *dpy, int keycode, unsigned int modifiers,
	       Window grab_window, Bool owner_events, int pointer_mode, int keyboard_mode)
{
	if (modifiers == AnyModifier)
		return;

	/* grab all combinations of the modifier with CapsLock, NumLock and
	 * ScrollLock. How much memory/CPU does such a monstrosity consume
	 * in the server?
	 */
	if (_NumLockMask)
		XGrabKey(dpy, keycode, modifiers | _NumLockMask,
			 grab_window, owner_events, pointer_mode, keyboard_mode);
	if (_ScrollLockMask)
		XGrabKey(dpy, keycode, modifiers | _ScrollLockMask,
			 grab_window, owner_events, pointer_mode, keyboard_mode);
	if (_NumLockMask && _ScrollLockMask)
		XGrabKey(dpy, keycode, modifiers | _NumLockMask | _ScrollLockMask,
			 grab_window, owner_events, pointer_mode, keyboard_mode);
	if (_NumLockMask)
		XGrabKey(dpy, keycode, modifiers | _NumLockMask | LockMask,
			 grab_window, owner_events, pointer_mode, keyboard_mode);
	if (_ScrollLockMask)
		XGrabKey(dpy, keycode, modifiers | _ScrollLockMask | LockMask,
			 grab_window, owner_events, pointer_mode, keyboard_mode);
	if (_NumLockMask && _ScrollLockMask)
		XGrabKey(dpy, keycode, modifiers | _NumLockMask | _ScrollLockMask | LockMask,
			 grab_window, owner_events, pointer_mode, keyboard_mode);
	/* phew, I guess that's all, right? */
}
#endif			  /* NUMLOCK_HACK */

void
wHackedGrabButton(Display *dpy, unsigned int button, unsigned int modifiers,
		  Window grab_window, Bool owner_events,
		  unsigned int event_mask, int pointer_mode, int keyboard_mode, Window confine_to, Cursor cursor)
{
	XGrabButton(dpy, button, modifiers, grab_window, owner_events,
		    event_mask, pointer_mode, keyboard_mode, confine_to, cursor);

	if (modifiers == AnyModifier)
		return;

	XGrabButton(dpy, button, modifiers | LockMask, grab_window, owner_events,
		    event_mask, pointer_mode, keyboard_mode, confine_to, cursor);

#ifdef NUMLOCK_HACK
	/* same as above, but for mouse buttons */
	if (_NumLockMask)
		XGrabButton(dpy, button, modifiers | _NumLockMask,
			    grab_window, owner_events, event_mask, pointer_mode,
			    keyboard_mode, confine_to, cursor);
	if (_ScrollLockMask)
		XGrabButton(dpy, button, modifiers | _ScrollLockMask,
			    grab_window, owner_events, event_mask, pointer_mode,
			    keyboard_mode, confine_to, cursor);
	if (_NumLockMask && _ScrollLockMask)
		XGrabButton(dpy, button, modifiers | _ScrollLockMask | _NumLockMask,
			    grab_window, owner_events, event_mask, pointer_mode,
			    keyboard_mode, confine_to, cursor);
	if (_NumLockMask)
		XGrabButton(dpy, button, modifiers | _NumLockMask | LockMask,
			    grab_window, owner_events, event_mask, pointer_mode,
			    keyboard_mode, confine_to, cursor);
	if (_ScrollLockMask)
		XGrabButton(dpy, button, modifiers | _ScrollLockMask | LockMask,
			    grab_window, owner_events, event_mask, pointer_mode,
			    keyboard_mode, confine_to, cursor);
	if (_NumLockMask && _ScrollLockMask)
		XGrabButton(dpy, button, modifiers | _ScrollLockMask | _NumLockMask | LockMask,
			    grab_window, owner_events, event_mask, pointer_mode,
			    keyboard_mode, confine_to, cursor);
#endif			  /* NUMLOCK_HACK */
}

void getOffendingModifiers(Display *dpy)
{
	int i;
	XModifierKeymap *modmap;
	KeyCode nlock, slock;
	static int mask_table[8] = {
		ShiftMask, LockMask, ControlMask, Mod1Mask,
		Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
	};

	nlock = XKeysymToKeycode(dpy, XK_Num_Lock);
	slock = XKeysymToKeycode(dpy, XK_Scroll_Lock);

	/*
	 * Find out the masks for the NumLock and ScrollLock modifiers,
	 * so that we can bind the grabs for when they are enabled too.
	 */
	modmap = XGetModifierMapping(dpy);

	if (modmap != NULL && modmap->max_keypermod > 0) {
		for (i = 0; i < 8 * modmap->max_keypermod; i++) {
			if (modmap->modifiermap[i] == nlock && nlock != 0)
				_NumLockMask = mask_table[i / modmap->max_keypermod];
			else if (modmap->modifiermap[i] == slock && slock != 0)
				_ScrollLockMask = mask_table[i / modmap->max_keypermod];
		}
	}

	if (modmap)
		XFreeModifiermap(modmap);
}
