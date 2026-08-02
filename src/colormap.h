/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMCOLORMAP_H
#define WMCOLORMAP_H

void wColormapInstallForWindow(virtual_screen *vscr, WWindow *wwin);
void wColormapAllowClientInstallation(virtual_screen *vscr, Bool starting);

#endif /* WMCOLORMAP_H */
