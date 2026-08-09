/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMDIALOG_KEYBINDS_H_
#define WMDIALOG_KEYBINDS_H_

#include "screen.h"

/*
 * Key Bindings Panel.
 *
 * Shows every assigned keybinding (built-in window-manager keybindings plus
 * root-menu SHORTCUTs) as a scrollable, alphabetically-sorted list of
 * action name + key combination. Modelled after the Info / Legal panels but
 * kept in its own file so dialog.c stays small.
 */
void panel_show_keybinds(virtual_screen *vscr);

#endif /* WMDIALOG_KEYBINDS_H_ */
