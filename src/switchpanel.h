/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _SWITCHPANEL_H_
#define _SWITCHPANEL_H_

typedef struct SwitchPanel WSwitchPanel;

WSwitchPanel *wInitSwitchPanel(virtual_screen *vscr, WWindow *curwin, Bool class_only);

void wSwitchPanelDestroy(WSwitchPanel *panel);

WWindow *wSwitchPanelSelectNext(WSwitchPanel *panel, int back, int ignore_minimized, Bool class_only);
WWindow *wSwitchPanelSelectFirst(WSwitchPanel *panel, int back);

WWindow *wSwitchPanelHandleEvent(WSwitchPanel *panel, XEvent *event);

Window wSwitchPanelGetWindow(WSwitchPanel *swpanel);

#endif /* _SWITCHPANEL_H_ */
