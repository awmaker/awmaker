/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMDIALOG_LEGAL_H_
#define WMDIALOG_LEGAL_H_

#include "screen.h"

/*
 * Legal Panel.
 *
 * Shows the GPL notice. Extracted from dialog.c as an autonomous panel (own
 * struct + singleton), mirroring dialog_keybinds / dialog_info.
 */
void panel_show_legal(virtual_screen *vscr);

#endif /* WMDIALOG_LEGAL_H_ */
