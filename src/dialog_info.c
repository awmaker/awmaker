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
 * Info Panel.
 *
 * Shows the about/version/visual/capabilities information (the "Info"
 * window that the WPrefs/root-menu Info entry opens). Extracted from
 * dialog.c as an autonomous panel -- own struct + own singleton -- so
 * dialog.c stays small. Only one instance exists per screen; if it is
 * already up it is raised and focused instead of recreated.
 */

#include "awconfig.h"

#include <X11/Xlib.h>

#include <stdio.h>
#include <string.h>

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
#include <malloc.h>
#endif

#include "WindowMaker.h"
#include "GNUstep.h"
#include "screen.h"
#include "window.h"
#include "framewin.h"
#include "actions.h"
#include "stacking.h"
#include "xinerama.h"
#include "misc.h"
#include "dialog_info.h"

#define COPYRIGHT_TEXT \
	"Copyright \xc2\xa9 1997-2006 Alfredo K. Kojima\n"\
	"Copyright \xc2\xa9 1998-2006 Dan Pascu\n"\
	"Copyright \xc2\xa9 2013-2014 Window Maker Developers Team\n" \
	"Copyright \xc2\xa9 2015-2020 Rodolfo García Peñas (kix)"

#define INFOPANEL_WIDTH 402
#define INFOPANEL_HEIGHT 290
#define MARGIN 10

typedef struct InfoPanel {
	virtual_screen *vscr;
	WWindow *wwin;
	WMWindow *win;
	WMFrame *frame;

	WMLabel *lbl_logo;
	WMLabel *lbl_name1;
	WMFrame *frm_line;
	WMLabel *lbl_name2;
	WMLabel *lbl_version;
	WMLabel *lbl_info;
	WMLabel *lbl_copyr;
} InfoPanel;

static InfoPanel *infoPanel = NULL;

static void destroy_info_panel(WCoreWindow *foo, void *data, XEvent *event)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) foo;
	(void) data;
	(void) event;

	WMUnmapWidget(infoPanel->win);
	WMDestroyWidget(infoPanel->win);
	wUnmanageWindow(infoPanel->wwin, False, False);
	wfree(infoPanel);
	infoPanel = NULL;
}

static void create_info_widgets(virtual_screen *vscr, InfoPanel *panel, int win_width, int win_height, int wmScaleWidth, int wmScaleHeight)
{
	WMPixmap *logo;
	WMFont *font;
	char *strbuf = NULL;
	const char *separator;
	char buffer[256];
#ifdef USE_XINERAMA
	char heads[128];
#endif
#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
	struct mallinfo ma = mallinfo();
#endif
	char **strl;
	int i, width = 50, sepHeight;
	char *visuals[] = {
		"StaticGray",
		"GrayScale",
		"StaticColor",
		"PseudoColor",
		"TrueColor",
		"DirectColor"
	};

	panel->win = WMCreateWindow(vscr->screen_ptr->wmscreen, "info");
	WMGetScaleBaseFromSystemFont(vscr->screen_ptr->wmscreen, &wmScaleWidth, &wmScaleHeight);
	WMResizeWidget(panel->win, win_width, win_height);

	panel->frame = WMCreateFrame(panel->win);
	WMResizeWidget(panel->frame, win_width - (2 * WMScaleX(MARGIN)), win_height - (2 * WMScaleY(MARGIN)));
	WMMoveWidget(panel->frame, WMScaleX(MARGIN), WMScaleY(MARGIN));
	WMSetFrameTitle(panel->frame, NULL);

	logo = WMCreateApplicationIconBlendedPixmap(vscr->screen_ptr->wmscreen, (RColor *) NULL);
	if (!logo)
		logo = WMRetainPixmap(WMGetApplicationIconPixmap(vscr->screen_ptr->wmscreen));

	if (logo) {
		panel->lbl_logo = WMCreateLabel(panel->frame);
		WMResizeWidget(panel->lbl_logo, WMScaleX(64), WMScaleY(64));
		WMMoveWidget(panel->lbl_logo, WMScaleX(30), WMScaleY(20));
		WMSetLabelImagePosition(panel->lbl_logo, WIPImageOnly);
		WMSetLabelImage(panel->lbl_logo, logo);
		WMReleasePixmap(logo);
	}

	sepHeight = WMScaleY(3);
	panel->lbl_name1 = WMCreateLabel(panel->frame);

	WMResizeWidget(panel->lbl_name1, WMScaleX(240), WMScaleY(30) + WMScaleY(2));
	WMMoveWidget(panel->lbl_name1, WMScaleX(100), WMScaleY(30) - WMScaleY(2) - sepHeight);

	snprintf(buffer, sizeof(buffer),
		"Lucida Sans,Comic Sans MS,URW Gothic L,Trebuchet MS:italic:pixelsize=%d:antialias=true",
		WMScaleY(24));
	font = WMCreateFont(vscr->screen_ptr->wmscreen, buffer);
	strbuf = "AW Maker";
	if (font) {
		width = WMWidthOfString(font, strbuf, strlen(strbuf));
		WMSetLabelFont(panel->lbl_name1, font);
		WMReleaseFont(font);
	}

	WMSetLabelTextAlignment(panel->lbl_name1, WACenter);
	WMSetLabelText(panel->lbl_name1, strbuf);

	panel->frm_line = WMCreateFrame(panel->frame);
	WMResizeWidget(panel->frm_line, width, sepHeight);
	WMMoveWidget(panel->frm_line, WMScaleX(100) + (WMScaleX(240) - width) / 2, WMScaleY(60) - sepHeight);
	WMSetFrameRelief(panel->frm_line, WRSimple);
	WMSetWidgetBackgroundColor(panel->frm_line, vscr->screen_ptr->black);

	panel->lbl_name2 = WMCreateLabel(panel->frame);
	WMResizeWidget(panel->lbl_name2, WMScaleX(240), WMScaleY(24));
	WMMoveWidget(panel->lbl_name2, WMScaleX(100), WMScaleY(60));
	snprintf(buffer, sizeof(buffer), "URW Gothic L,Nimbus Sans L:pixelsize=%d:antialias=true", WMScaleY(16));
	font = WMCreateFont(vscr->screen_ptr->wmscreen, buffer);
	if (font) {
		WMSetLabelFont(panel->lbl_name2, font);
		WMReleaseFont(font);
		font = NULL;
	}

	WMSetLabelTextAlignment(panel->lbl_name2, WACenter);
	WMSetLabelText(panel->lbl_name2, _("Abstracting Window Maker"));

	snprintf(buffer, sizeof(buffer), _("Version %s"), VERSION);
	panel->lbl_version = WMCreateLabel(panel->frame);
	WMResizeWidget(panel->lbl_version, WMScaleX(310), WMScaleY(16));
	WMMoveWidget(panel->lbl_version, WMScaleX(30), WMScaleY(95));
	WMSetLabelTextAlignment(panel->lbl_version, WARight);
	WMSetLabelText(panel->lbl_version, buffer);
	WMSetLabelWraps(panel->lbl_version, False);

	panel->lbl_copyr = WMCreateLabel(panel->frame);
	WMResizeWidget(panel->lbl_copyr, WMScaleX(360), WMScaleY(60));
	WMMoveWidget(panel->lbl_copyr, WMScaleX(15), WMScaleY(190));
	WMSetLabelTextAlignment(panel->lbl_copyr, WALeft);
	WMSetLabelText(panel->lbl_copyr, COPYRIGHT_TEXT);
	font = WMSystemFontOfSize(vscr->screen_ptr->wmscreen, WMScaleY(11));
	if (font) {
		WMSetLabelFont(panel->lbl_copyr, font);
		WMReleaseFont(font);
		font = NULL;
	}

	strbuf = NULL;
	snprintf(buffer, sizeof(buffer), _("Using visual 0x%x: %s %ibpp "),
		 (unsigned) vscr->screen_ptr->w_visual->visualid, visuals[vscr->screen_ptr->w_visual->class], vscr->screen_ptr->w_depth);

	strbuf = wstrappend(strbuf, buffer);

	switch (vscr->screen_ptr->w_depth) {
	case 15:
		strbuf = wstrappend(strbuf, _("(32 thousand colors)\n"));
		break;
	case 16:
		strbuf = wstrappend(strbuf, _("(64 thousand colors)\n"));
		break;
	case 24:
	case 32:
		strbuf = wstrappend(strbuf, _("(16 million colors)\n"));
		break;
	default:
		snprintf(buffer, sizeof(buffer), _("(%d colors)\n"), 1 << vscr->screen_ptr->w_depth);
		strbuf = wstrappend(strbuf, buffer);
		break;
	}

#if defined(HAVE_MALLOC_H) && defined(HAVE_MALLINFO)
	snprintf(buffer, sizeof(buffer),
#ifdef DEBUG
		_("Total memory allocated: %i kB (in use: %i kB, %d free chunks).\n"),
#else
		_("Total memory allocated: %i kB (in use: %i kB).\n"),
#endif
		(ma.arena + ma.hblkhd) / 1024,
		(ma.uordblks + ma.hblkhd) / 1024
#ifdef DEBUG
		/*
		 * This information is representative of the memory
		 * fragmentation. In ideal case it should be 1, but
		 * that is never possible
		 */
		, ma.ordblks
#endif
		);

	strbuf = wstrappend(strbuf, buffer);
#endif

	strbuf = wstrappend(strbuf, _("Image formats: "));
	strl = RSupportedFileFormats();
	separator = NULL;
	for (i = 0; strl[i] != NULL; i++) {
		if (separator != NULL)
			strbuf = wstrappend(strbuf, separator);
		strbuf = wstrappend(strbuf, strl[i]);
		separator = ", ";
	}

	strbuf = wstrappend(strbuf, _("\nAdditional support for: "));
	strbuf = wstrappend(strbuf, "WMSPEC");

#ifdef USE_MWM_HINTS
	strbuf = wstrappend(strbuf, ", MWM");
#endif

#ifdef USE_DOCK_XDND
	strbuf = wstrappend(strbuf, ", XDnD");
#endif

#ifdef USE_MAGICK
	strbuf = wstrappend(strbuf, ", ImageMagick");
#endif

#ifdef USE_XINERAMA
	strbuf = wstrappend(strbuf, _("\n"));
#ifdef SOLARIS_XINERAMA
	strbuf = wstrappend(strbuf, _("Solaris "));
#endif
	strbuf = wstrappend(strbuf, _("Xinerama: "));

	snprintf(heads, sizeof(heads) - 1, _("%d head(s) found."), vscr->screen_ptr->xine_info.count);
	strbuf = wstrappend(strbuf, heads);
#endif

#ifdef USE_RANDR
	strbuf = wstrappend(strbuf, _("\n"));
	strbuf = wstrappend(strbuf, "RandR: ");
	if (w_global.xext.randr.supported)
		strbuf = wstrappend(strbuf, _("supported"));
	else
		strbuf = wstrappend(strbuf, _("unsupported"));

	strbuf = wstrappend(strbuf, ".");
#endif

	panel->lbl_info = WMCreateLabel(panel->frame);
	WMResizeWidget(panel->lbl_info, WMScaleX(350), WMScaleY(80));
	WMMoveWidget(panel->lbl_info, WMScaleX(15), WMScaleY(115));
	WMSetLabelText(panel->lbl_info, strbuf);
	font = WMSystemFontOfSize(vscr->screen_ptr->wmscreen, WMScaleY(11));
	if (font) {
		WMSetLabelFont(panel->lbl_info, font);
		WMReleaseFont(font);
		font = NULL;
	}

	wfree(strbuf);
}

void panel_show_info(virtual_screen *vscr)
{
	InfoPanel *panel = NULL;
	Window parent;
	WWindow *wwin;
	WMPoint center;
	int wmScaleWidth, wmScaleHeight;
	int win_width, win_height;
	char title[256];
	int wframeflags;

	WMGetScaleBaseFromSystemFont(vscr->screen_ptr->wmscreen, &wmScaleWidth, &wmScaleHeight);

	win_width = WMScaleX(INFOPANEL_WIDTH);
	win_height = WMScaleY(INFOPANEL_HEIGHT);
	sprintf(title, "Info");

	if (infoPanel) {
		if (infoPanel->vscr->screen_ptr == vscr->screen_ptr) {
			wRaiseFrame(infoPanel->wwin->frame->vscr, infoPanel->wwin->frame->core);
			wSetFocusTo(vscr, infoPanel->wwin);
		}

		return;
	}

	panel = wmalloc(sizeof(InfoPanel));
	panel->vscr = vscr;
	create_info_widgets(vscr, panel, win_width, win_height, wmScaleWidth, wmScaleHeight);
	infoPanel = panel;

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

	wwin->frame->on_click_right = destroy_info_panel;

	panel->wwin = wwin;
	WMMapWidget(panel->win);
	wWindowMap(wwin);
}
