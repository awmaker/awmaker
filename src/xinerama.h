/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _WMXINERAMA_H_
#define _WMXINERAMA_H_

#include "screen.h"
#include "window.h"
#include <WINGs/WINGs.h>

enum {
	DIRECTION_LEFT,
	DIRECTION_RIGHT,
	DIRECTION_UP,
	DIRECTION_DOWN
};


void wInitXinerama(WScreen *scr);

#define wXineramaHeads(scr) ((scr)->xine_info.count ? (scr)->xine_info.count : 1)

#define XFLAG_NONE	0x00
#define XFLAG_DEAD	0x01
#define XFLAG_MULTIPLE	0x02
#define XFLAG_PARTIAL	0x04

int wGetRectPlacementInfo(virtual_screen *vscr, WMRect rect, int *flags);
int wGetHeadForRect(virtual_screen *vscr, WMRect rect);
int wGetHeadForWindow(WWindow *wwin);
int wGetHeadForPoint(virtual_screen *vscr, WMPoint point);
int wGetHeadForPointerLocation(virtual_screen *vscr);
int wGetHeadRelativeToCurrentHead(virtual_screen *vscr, int current_head, int direction);

WMRect wGetRectForHead(WScreen *scr, int head);
void wGetRectUnion(const WMRect *rect1, const WMRect *rect2, WMRect *dest);
WArea wGetUsableAreaForHead(virtual_screen *vscr, int head, WArea *totalAreaPtr, Bool noicons);
WMPoint wGetPointToCenterRectInHead(virtual_screen *vscr, int head, int width, int height);

Bool wWindowTouchesHead(WWindow *wwin, int head);

#endif
