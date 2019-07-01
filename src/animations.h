/*
 *  AWindow Maker window manager
 *
 *  Copyright (c) 2019 Rodolfo García Peñas (kix)
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

#ifndef WMANIMATIONS_H
#define WMANIMATIONS_H

#include "window.h"

#define UNSHADE   0
#define SHADE     1

void animation_shade(WWindow *wwin, Bool what);
void animation_catchevents(void);
void animateResize(virtual_screen *vscr, int x, int y, int w, int h, int fx, int fy, int fw, int fh);

#endif /* WMANIMATIONS_H */
