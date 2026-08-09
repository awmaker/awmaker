/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef WMDIALOG_H_
#define WMDIALOG_H_

#include "screen.h"

enum {
	WMAbort = 0,
	WMRestart,
	WMStartAlternate
};

typedef struct WMInputPanelWithHistory {
	WMInputPanel *panel;
	WMArray *history;
	int histpos;
	char *prefix;
	char *suffix;
	char *rest;
	WMArray *variants;
	int varpos;
} WMInputPanelWithHistory;

int wMessageDialog(virtual_screen *vscr, const char *title, const char *message,
		   const char *defBtn, const char *altBtn, const char *othBtn);
int wAdvancedInputDialog(virtual_screen *vscr, const char *title, const char *message, const char *name, char **text);
int wInputDialog(virtual_screen *vscr, const char *title, const char *message, char **text);

int wExitDialog(virtual_screen *vscr, const char *title, const char *message, const char *defBtn,
		const char *altBtn, const char *othBtn);

#endif
