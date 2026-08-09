/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMDIALOG_CRASH_H_
#define WMDIALOG_CRASH_H_

/*
 * Crashing Dialog Panel.
 *
 * Modal panel shown on a fatal signal, letting the user choose whether to
 * abort (+core), restart Window Maker, or start an alternate WM. Extracted
 * from dialog.c as an autonomous panel (own struct + private event loop),
 * mirroring dialog_info / dialog_legal / dialog_iconchooser. Returns one of
 * WMAbort / WMRestart / WMStartAlternate (defined in dialog.h).
 */
int wShowCrashingDialogPanel(int whatSig);

#endif /* WMDIALOG_CRASH_H_ */
