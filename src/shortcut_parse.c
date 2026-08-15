/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <awconfig.h>

#include <string.h>

#include <X11/Xlib.h>

#include <WINGs/WUtil.h>

#include "shortcut_parse.h"
#include "xmodifier.h"

#define MAX_SHORTCUT_LENGTH 64

/* Parse a single "Mod1+Mod2+Key" token into a modifier bitmask and keycode.
 * Shared by src/rootmenu.c and src/usermenu.c.
 * Returns True on success. */
Bool parseShortcutToken(Display *dpy, const char *file, const char *token,
			unsigned int *modifier, KeyCode *keycode)
{
	char buf[MAX_SHORTCUT_LENGTH];
	char *b;
	char *k;
	KeySym ksym;

	wstrlcpy(buf, token, MAX_SHORTCUT_LENGTH);
	b = buf;

	/* get modifiers */
	while ((k = strchr(b, '+')) != NULL) {
		int mod;

		*k = 0;
		mod = wXModifierFromKey(b);
		if (mod < 0) {
			wwarning(_("%s: invalid key modifier \"%s\""), file, b);
			return False;
		}
		*modifier |= mod;

		b = k + 1;
	}

	/* get key */
	ksym = XStringToKeysym(b);

	if (ksym == NoSymbol) {
		wwarning(_("%s: invalid kbd shortcut specification \"%s\""), file, token);
		return False;
	}

	*keycode = XKeysymToKeycode(dpy, ksym);
	if (*keycode == 0) {
		wwarning(_("%s: invalid key in shortcut \"%s\""), file, token);
		return False;
	}

	return True;
}
