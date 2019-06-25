/*
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
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
