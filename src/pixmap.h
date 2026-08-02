/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef WMPIXMAP_H_
#define WMPIXMAP_H_

#include "screen.h"

typedef struct WPixmap {
    Pixmap image;		       /* icon image */
    Pixmap mask;		       /* icon mask */
    int width, height;		       /* size of pixmap */
    int depth;			       /* depth of pixmap */
    unsigned int shared:1;	       /* if pixmaps should be kept
                                        * when structure is freed */
    unsigned int client_owned:1;
    unsigned int client_owned_mask:1;
} WPixmap;

WPixmap *wPixmapCreate(Pixmap image, Pixmap mask);
WPixmap *wPixmapCreateFromXPMData(WScreen *scr, char **data);
WPixmap *wPixmapCreateFromXBMData(WScreen *scr, char *data, char *mask,
                                  int width, int height, unsigned long fg,
                                  unsigned long bg);
void wPixmapDestroy(WPixmap *pix);
void destroy_pixmap(Pixmap pix);
#endif
