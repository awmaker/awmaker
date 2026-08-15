/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMKEYBIND_H
#define WMKEYBIND_H

#include <keybinds.h>

/* ---[ Global Variables ]------------------------------------------------ */

struct SHBinding;	/* shbinding.h (CUN-1: built-ins are SHBinding, RSM_WKBD) */
/*
 * NOTE: the key-chain struct WShortKey used to live here; it is now local to
 * usermenu.c (CUN-2), where it is only the {modifier,keycode} pair used to
 * synthesize user-menu keystrokes for the client — not a WM keybinding.
 */
/* `wKeyBindings` is declared in shbinding.h — include it to use the built-ins. */

/* ---[ Functions ]------------------------------------------------------- */

void wKeyboardInitialize(void);

#endif /* WMKEYBIND_H */
