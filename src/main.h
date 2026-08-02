/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMMAIN_H_
#define WMMAIN_H_

#include "config.h"

#ifdef HAVE_STDNORETURN
#include <stdnoreturn.h>
#endif

noreturn void Exit(int status);
void Restart(char *manager, Bool abortOnFailure);
void SetupEnvironment(virtual_screen *vscr);
noreturn void wAbort(Bool dumpCore);
void ExecExitScript(void);
int getWVisualID(int screen);

#endif
