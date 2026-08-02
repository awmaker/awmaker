/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _WM_SHELL_H_
#define _WM_SHELL_H_

#include "wconfig.h"

void ExecuteShellCommand(virtual_screen *vscr, const char *command);
int execute_command(virtual_screen *vscr, char **argv, int argc);
int execute_command2(virtual_screen *vscr, char **argv, int argc);

#endif
