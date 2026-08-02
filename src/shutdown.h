/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SHUTDOWN_H
#define SHUTDOWN_H

/* shutdown modes */
typedef enum {
    WSExitMode,
    WSLogoutMode,
    WSKillMode,
    WSRestartPreparationMode
} WShutdownMode;

void Shutdown(WShutdownMode mode);
void RestoreDesktop(virtual_screen *vscr);

#endif  /* SHUTDOWN_H */
