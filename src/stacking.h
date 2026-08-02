/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMSTACKING_H_
#define WMSTACKING_H_
void wRaiseFrame(virtual_screen *vscr, WCoreWindow *frame);
void wLowerFrame(virtual_screen *vscr, WCoreWindow *frame);
void wRaiseLowerFrame(virtual_screen *vscr, WCoreWindow *frame);
void AddToStackList(virtual_screen *vscr, WCoreWindow *frame);
void MoveInStackListUnder(virtual_screen *vscr, WCoreWindow *prev, WCoreWindow *frame);
void RemoveFromStackList(virtual_screen *vscr, WCoreWindow *frame);
void ChangeStackingLevel(virtual_screen *vscr, WCoreWindow *frame, int new_level);
void RemakeStackList(virtual_screen *vscr);
void CommitStacking(virtual_screen *vscr);
void CommitStackingForFrame(virtual_screen *vscr, WCoreWindow *frame);
void CommitStackingForWindow(virtual_screen *vscr, WCoreWindow *frame);
#endif
