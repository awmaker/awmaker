/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMDIALOG_INFO_H_
#define WMDIALOG_INFO_H_

#include "screen.h"

/*
 * Info Panel.
 *
 * Shows the about/version/visual/capabilities information. Extracted from
 * dialog.c as an autonomous panel (own struct + singleton) so dialog.c stays
 * small, mirroring dialog_keybinds / the planned dialog_legal.
 */
void panel_show_info(virtual_screen *vscr);

#endif /* WMDIALOG_INFO_H_ */
