/* winspector.h - window attribute inspector header file
 *
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

#ifndef WINSPECTOR_H_
#define WINSPECTOR_H_

typedef struct InspectorPanel InspectorPanel;

#include "config.h"
#include "window.h"
#include "dialog.h"

struct InspectorPanel {
	struct InspectorPanel *nextPtr;

	WWindow *frame;
	WWindow *inspected;	/* the window that's being inspected */
	WMWindow *wwin;
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
	WMButton *moreChk[11];
#else
	WMButton *moreChk[12];
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
