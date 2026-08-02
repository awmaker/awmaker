/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMEVENT_H
#define WMEVENT_H

#include "config.h"

#ifdef HAVE_STDNORETURN
#include <stdnoreturn.h>
#endif

typedef void (WDeathHandler)(pid_t pid, unsigned int status, void *cdata);

noreturn void EventLoop(void);
void DispatchEvent(XEvent *event);
void ProcessPendingEvents(void);
WMagicNumber wAddDeathHandler(pid_t pid, WDeathHandler *callback, void *cdata);
Bool IsDoubleClick(virtual_screen *vscr, XEvent *event);

/* called from the signal handler */
void NotifyDeadProcess(pid_t pid, unsigned char status);

#endif /* WMEVENT_H */
