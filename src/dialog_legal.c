/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Legal Panel.
 *
 * Shows the GPL notice (the "Legal" window opened from the root-menu Info
 * entry). Extracted from dialog.c as an autonomous panel -- own struct + own
 * singleton -- so dialog.c stays small. Only one instance exists per screen;
 * if it is already up it is raised and focused instead of recreated.
 */

#include "awconfig.h"

#include <X11/Xlib.h>

#include <stdio.h>

#include "WindowMaker.h"
#include "GNUstep.h"
#include "screen.h"
#include "window.h"
#include "framewin.h"
#include "actions.h"
#include "stacking.h"
#include "xinerama.h"
#include "misc.h"
#include "dialog_legal.h"

#define LEGAL_TEXT \
	"    Window Maker is free software; you can redistribute it and/or "\
	"modify it under the terms of the GNU General Public License as "\
	"published by the Free Software Foundation; either version 2 of the "\
	"License, or (at your option) any later version.\n\n"\
	"    Window Maker is distributed in the hope that it will be useful, "\
	"but WITHOUT ANY WARRANTY; without even the implied warranty "\
	"of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. "\
	"See the GNU General Public License for more details.\n\n"\
	"    You should have received a copy of the GNU General Public "\
	"License along with this program; if not, write to the Free Software "\
	"Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA"\
	"02110-1301 USA."

#define LEGALPANEL_WIDTH 420
#define LEGALPANEL_HEIGHT 250
#define MARGIN 10

typedef struct LegalPanel {
	virtual_screen *vscr;
	WWindow *wwin;
	WMWindow *win;
	WMFrame *frame;

	WMLabel *lbl_license;
} LegalPanel;

static LegalPanel *legalPanel = NULL;

static void destroy_legal_panel(WCoreWindow *foo, void *data, XEvent *event)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) foo;
	(void) data;
	(void) event;

	WMUnmapWidget(legalPanel->win);
	WMDestroyWidget(legalPanel->win);
	wUnmanageWindow(legalPanel->wwin, False, False);
	wfree(legalPanel);
	legalPanel = NULL;
}

static void create_legal_widgets(virtual_screen *vscr, LegalPanel *panel, int win_width, int win_height, int wmScaleWidth, int wmScaleHeight)
{
	panel->win = WMCreateWindow(vscr->screen_ptr->wmscreen, "legal");
	WMResizeWidget(panel->win, win_width, win_height);

	panel->frame = WMCreateFrame(panel->win);
	WMResizeWidget(panel->frame, win_width - (2 * WMScaleX(MARGIN)), win_height - (2 * WMScaleY(MARGIN)));
	WMMoveWidget(panel->frame, WMScaleX(MARGIN), WMScaleY(MARGIN));
	WMSetFrameTitle(panel->frame, NULL);

	panel->lbl_license = WMCreateLabel(panel->frame);
	WMSetLabelWraps(panel->lbl_license, True);
	WMResizeWidget(panel->lbl_license, win_width - (4 * WMScaleX(10)), win_height - (4 * WMScaleY(10)));
	WMMoveWidget(panel->lbl_license, WMScaleX(8), WMScaleY(8));
	WMSetLabelTextAlignment(panel->lbl_license, WALeft);
	WMSetLabelText(panel->lbl_license, LEGAL_TEXT);
}

void panel_show_legal(virtual_screen *vscr)
{
	LegalPanel *panel = NULL;
	Window parent;
	WWindow *wwin;
	WMPoint center;
	int wmScaleWidth, wmScaleHeight;
	int win_width, win_height;
	char title[256];
	int wframeflags;

	WMGetScaleBaseFromSystemFont(vscr->screen_ptr->wmscreen, &wmScaleWidth, &wmScaleHeight);

	win_width = WMScaleX(LEGALPANEL_WIDTH);
	win_height = WMScaleY(LEGALPANEL_HEIGHT);
	sprintf(title, "Legal");

	if (legalPanel) {
		if (legalPanel->vscr->screen_ptr == vscr->screen_ptr) {
			wRaiseFrame(legalPanel->wwin->frame->vscr, legalPanel->wwin->frame->core);
			wSetFocusTo(vscr, legalPanel->wwin);
		}

		return;
	}

	panel = wmalloc(sizeof(LegalPanel));
	panel->vscr = vscr;
	create_legal_widgets(vscr, panel, win_width, win_height, wmScaleWidth, wmScaleHeight);
	legalPanel = panel;

	WMRealizeWidget(panel->win);
	WMMapSubwidgets(panel->win);
	WMMapSubwidgets(panel->frame);

	parent = XCreateSimpleWindow(dpy, vscr->screen_ptr->root_win, 0, 0, win_width, win_height, 0, 0, 0);
	XReparentWindow(dpy, WMWidgetXID(panel->win), parent, 0, 0);
	center = wGetPointToCenterRectInHead(vscr, wGetHeadForPointerLocation(vscr), win_width, win_height);

	wframeflags = WFF_RIGHT_BUTTON | WFF_BORDER | WFF_TITLEBAR;

	wwin = wManageInternalWindow(vscr, parent, None, title, center.x, center.y, win_width, win_height, wframeflags);

	WSETUFLAG(wwin, no_closable, 0);
	WSETUFLAG(wwin, no_close_button, 0);

	wwin->frame->on_click_right = destroy_legal_panel;

	panel->wwin = wwin;
	WMMapWidget(panel->win);
	wWindowMap(wwin);
}
