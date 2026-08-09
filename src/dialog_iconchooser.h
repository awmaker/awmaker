/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMDIALOG_ICONCHOOSER_H_
#define WMDIALOG_ICONCHOOSER_H_

#include "screen.h"
#include "appicon.h"
#include "dockedapp.h"
#include "winspector.h"

/*
 * Icon Chooser Panel.
 *
 * Modal dialog that lets the user pick an icon file. Extracted from dialog.c
 * as an autonomous panel (own struct + private event loop), mirroring
 * dialog_info / dialog_legal. The IconPanel type itself lives privately in
 * dialog_iconchooser.c; only a pointer is handed out (via dockedapp.h /
 * winspector.h `iconchooserdlg` fields).
 */
Bool wIconChooserDialog(AppSettingsPanel *app_panel, InspectorPanel *ins_panel, WAppIcon *icon, char **file);

#endif /* WMDIALOG_ICONCHOOSER_H_ */
