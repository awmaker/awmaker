/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _XDND_H_
#define _XDND_H_

void wXDNDInitializeAtoms(void);
Bool wXDNDProcessSelection(XEvent *event);
Bool wXDNDProcessClientMessage(XClientMessageEvent *event);
void wXDNDMakeAwareness(Window window);

#define XDND_VERSION 3L

#define XDND_ENTER_SOURCE_WIN(e)	((e)->data.l[0])
#define XDND_ENTER_THREE_TYPES(e)	(((e)->data.l[1] & 0x1UL) == 0)
#define XDND_ENTER_TYPE(e, i)		((e)->data.l[2 + i])	/* i => (0, 1, 2) */

/* XdndPosition */
#define XDND_POSITION_SOURCE_WIN(e)     ((e)->data.l[0])

/* XdndStatus */
#define XDND_STATUS_TARGET_WIN(e)	((e)->xclient.data.l[0])
#define XDND_STATUS_WILL_ACCEPT_SET(e, b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x1UL) | (((b) == 0) ? 0 : 0x1UL)
#define XDND_STATUS_WANT_POSITION_SET(e, b) (e)->xclient.data.l[1] = ((e)->xclient.data.l[1] & ~0x2UL) | (((b) == 0) ? 0 : 0x2UL)
#define XDND_STATUS_RECT_SET(e, x, y, w, h)	{(e)->xclient.data.l[2] = ((x) << 16) | ((y) & 0xFFFFUL); (e)->xclient.data.l[3] = ((w) << 16) | ((h) & 0xFFFFUL); }
#define XDND_STATUS_ACTION(e)		((e)->xclient.data.l[4])

/* XdndDrop */
#define XDND_DROP_SOURCE_WIN(e)         ((e)->data.l[0])

/* XdndFinished */
#define XDND_FINISHED_TARGET_WIN(e)	((e)->xclient.data.l[0])

#endif
