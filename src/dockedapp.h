/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMDOCKEDAPP_H_
#define WMDOCKEDAPP_H_

typedef struct AppSettingsPanel AppSettingsPanel;

#include "dialog.h"

struct AppSettingsPanel {
	WMWindow *win;
	WAppIcon *editedIcon;
	IconPanel *iconchooserdlg;

	WWindow *wwin;

	WMLabel *iconLabel;
	WMLabel *nameLabel;

	WMFrame *commandFrame;
	WMTextField *commandField;

	WMFrame *dndCommandFrame;
	WMTextField *dndCommandField;
	WMLabel *dndCommandLabel;

	WMFrame *pasteCommandFrame;
	WMTextField *pasteCommandField;
	WMLabel *pasteCommandLabel;

	WMFrame *iconFrame;
	WMTextField *iconField;
	WMButton *browseBtn;

	WMButton *autoLaunchBtn;
	WMButton *lockBtn;

	WMButton *okBtn;
	WMButton *cancelBtn;

	Window parent;

	/* kluge */
	unsigned int destroyed:1;
	unsigned int choosingIcon:1;
};

void DestroyDockAppSettingsPanel(AppSettingsPanel *panel);
void ShowDockAppSettingsPanel(WAppIcon *aicon);

#endif
