/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMSESSION_H_
#define WMSESSION_H_

void wSessionSaveState(virtual_screen *vscr);
void wSessionClearState(void);
void wSessionRestoreState(virtual_screen *vscr);
void wSessionRestoreLastWorkspace(virtual_screen *vscr);
#endif
