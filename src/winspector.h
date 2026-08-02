/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WINSPECTOR_H_
#define WINSPECTOR_H_

typedef struct InspectorPanel InspectorPanel;

#include "config.h"
#include "window.h"
#include "dialog.h"

struct InspectorPanel {
	struct InspectorPanel *nextPtr;

	WWindow *wwin;
	WWindow *inspected;	/* the window that's being inspected */
	WMWindow *win;
	Window parent;
	char *title;		/* InspectorPanel title */
	IconPanel *iconchooserdlg;

	/* common stuff */
	WMButton *revertBtn;
	WMButton *applyBtn;
	WMButton *saveBtn;
	WMPopUpButton *pagePopUp;

	/* first page. general stuff */
	WMFrame *specFrm;
	WMButton *instRb;
	WMButton *clsRb;
	WMButton *bothRb;
	WMButton *defaultRb;
	WMButton *selWinB;
	WMLabel *specLbl;

	/* second page. attributes */
	WMFrame *attrFrm;
	WMButton *attrChk[11];

	/* 3rd page. more attributes */
	WMFrame *moreFrm;
#ifndef XKB_BUTTON_HINT
	WMButton *moreChk[12];
#else
	WMButton *moreChk[13];
#endif

	/* 4th page. icon and workspace */
	WMFrame *iconFrm;
	WMLabel *iconLbl;
	WMLabel *fileLbl;
	WMTextField *fileText;
	WMButton *alwChk;
	WMButton *browseIconBtn;
	WMFrame *wsFrm;
	WMPopUpButton *wsP;

	/* 5th page. application wide attributes */
	WMFrame *appFrm;
	WMButton *appChk[3];

	unsigned int done:1;
	unsigned int destroyed:1;
	unsigned int choosingIcon:1;
};

void winspector_destroy(struct InspectorPanel *panel);
void wShowInspectorForWindow(WWindow *wwin);
void wHideInspectorForWindow(WWindow *wwin);
void wUnhideInspectorForWindow(WWindow *wwin);
void wCloseInspectorForWindow(WWindow *wwin);
void wDestroyInspectorPanels(void);
WWindow *wGetWindowOfInspectorForWindow(WWindow *wwin);
#endif
