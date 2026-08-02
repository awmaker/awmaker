/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef WMPROPERTIES_H_
#define WMPROPERTIES_H_

#include "GNUstep.h"

unsigned char* PropGetCheckProperty(Window window, Atom hint, Atom type,
                                    int format, int count, int *retCount);

int PropGetWindowState(Window window);

int PropGetNormalHints(Window window, XSizeHints *size_hints, int *pre_iccm);
void PropGetProtocols(Window window, WProtocols *prots);
int PropGetWMClass(Window window, char **wm_class, char **wm_instance);
int PropGetGNUstepWMAttr(Window window, GNUstepWMAttributes **attr);

void PropSetWMakerProtocols(Window root);
void PropCleanUp(Window root);
void PropSetIconTileHint(virtual_screen *vscr, RImage *image);

Window PropGetClientLeader(Window window);

#endif
