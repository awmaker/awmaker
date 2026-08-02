/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _XMODIFIER_H_INCLUDED
#define _XMODIFIER_H_INCLUDED

void wXModifierInitialize(void);
int  wXModifierFromKey(const char *key);
char *wXModifierToShortcutLabel(int mask);

#endif /* _XMODIFIER_H_INCLUDED */
