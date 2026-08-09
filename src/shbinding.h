/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
 * and individual contributors; see LICENSE for full attribution.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMSHBINDING_H
#define WMSHBINDING_H

#include "screen.h"

/*
 * Shortcut-action logic functions (F5, §8F5).
 *
 * These are the menu-independent handlers a root-menu shortcut can run. They
 * take a virtual_screen (or the data the action needs) instead of a WMenu /
 * WMenuEntry, so a binding can be executed without any menu being materialized.
 * The root-menu callbacks call these directly (as thin wrappers).
 */

void shExec(virtual_screen *vscr, const char *cmdline);
void shRestart(virtual_screen *vscr, const char *cmdline);
void shExit(virtual_screen *vscr, Bool quick);
void shShutdown(virtual_screen *vscr, Bool quick);
void shRefresh(virtual_screen *vscr);
void shArrangeIcons(virtual_screen *vscr);
void shShowAll(virtual_screen *vscr);
void shHideOthers(virtual_screen *vscr);
void shSaveSession(virtual_screen *vscr);
void shClearSession(virtual_screen *vscr);

#endif /* WMSHBINDING_H */
