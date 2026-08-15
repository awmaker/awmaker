/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SHORTCUT_PARSE_H_
#define SHORTCUT_PARSE_H_

#include <X11/Xlib.h>

/* Parse a single "Mod1+Mod2+Key" token (from a root menu shortcut or a
 * user-menu hotkey) into its modifier bitmask and keycode.
 *
 * The token is split on '+' and each modifier token is fed to
 * wXModifierFromKey(); the trailing token must resolve to a keycode via
 * XStringToKeysym/XKeysymToKeycode. On any failure it issues a wwarning
 * naming `file` and returns False, leaving *modifier/*keycode unchanged.
 *
 * Shared by src/rootmenu.c and src/usermenu.c.
 * Display is passed in (rather than the global `dpy`) so the shared file
 * stays decoupled from the WM's global — matching awmcommon/xmodifier.c.
 */
Bool parseShortcutToken(Display *dpy, const char *file, const char *token,
			unsigned int *modifier, KeyCode *keycode);

#endif /* SHORTCUT_PARSE_H_ */
