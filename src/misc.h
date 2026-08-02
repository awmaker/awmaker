/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef WMMISC_H_
#define WMMISC_H_

#include "defaults.h"
#include "keybind.h"
#include "appicon.h"

Bool wFetchName(Display *dpy, Window win, char **winname);
Bool UpdateDomainFile(WDDomain *domain);

void move_window(Window win, int from_x, int from_y, int to_x, int to_y);
void slide_windows(Window wins[], int n, int from_x, int from_y, int to_x, int to_y);
void ParseWindowName(WMPropList *value, char **winstance, char **wclass, const char *where);

static inline void slide_window(Window win, int from_x, int from_y, int to_x, int to_y)
{
	slide_windows(&win, 1, from_x, from_y, to_x, to_y);
}

/* Helper is a 'wmsetbg' subprocess with sets the background for the current workspace */
Bool start_bg_helper(virtual_screen *vscr);
void SendHelperMessage(virtual_screen *vscr, char type, int workspace, const char *msg);

char *ShrinkString(WMFont *font, const char *string, int width);
char *FindImage(const char *paths, const char *file);
char *ExpandOptions(virtual_screen *vscr, const char *cmdline);
char *GetShortcutString(const char *text);
char *GetShortcutKey(WShortKey key);
char *EscapeWM_CLASS(const char *name, const char *class);
char *StrConcatDot(const char *a, const char *b);
char *GetCommandForWindow(Window win);

int create_minipixmap_for_window(virtual_screen *vscr, Window win, Pixmap *tmp);
int create_minipixmap_for_wwindow(virtual_screen *vscr, WWindow *wwin, Pixmap *pixmap);
#endif
