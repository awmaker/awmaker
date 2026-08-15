/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef XMODIFIER_H_
#define XMODIFIER_H_

#include <X11/Xlib.h>

/* Grok X modifier mappings for shortcuts (XEmacs-derived, see xmodifier.c).

 This is the shared header for src/ and WPrefs.app/. Both entry-point families
 are exposed:
  - wXModifier* (the src/ style): wXModifierInitialize populates the global
    mapping state from the given display; wXModifierFromKey returns the X11
    mask (1 << index); wXModifierToShortcutLabel turns a mask back into a
    shortcut prefix.
  - ModifierFromKey (the WPrefs style): lazy-initializes from the given display
    and returns the raw modifier index (0..7), as consumed by WPrefs' modifierNames[].
 */

void wXModifierInitialize(Display *display);
int  wXModifierFromKey(const char *key);
char *wXModifierToShortcutLabel(int mask);
int ModifierFromKey(Display *dpy, const char *key);

#endif /* XMODIFIER_H_ */
