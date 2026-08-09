/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wconfig.h"
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#ifdef __FreeBSD__
#include <sys/signal.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include "WindowMaker.h"
#include "screen.h"
#include "window.h"
#include "dialog.h"
#include "dialog_crash.h"
#include "main.h"


static int showCrashDialog(int sig)
{
	int crashAction;

	dpy = XOpenDisplay(NULL);
	if (dpy) {
/* XXX TODO make sure that window states are saved and restored via netwm */

		XGrabServer(dpy);
		crashAction = wShowCrashingDialogPanel(sig);
		XCloseDisplay(dpy);
		dpy = NULL;
	} else {
		werror(_("cannot open connection for crashing dialog panel. Aborting."));
		crashAction = WMAbort;
	}

	if (crashAction == WMStartAlternate) {
		int i;

		wmessage(_("trying to start alternate window manager..."));

		for (i = 0; i < WMGetArrayItemCount(wPreferences.fallbackWMs); i++) {
			Restart(WMGetFromArray(wPreferences.fallbackWMs, i), False);
		}

		wfatal(_("failed to start alternate window manager. Aborting."));

		return 0;
	} else if (crashAction == WMAbort)
		return 0;
	else
		return 1;
}

int MonitorLoop(int argc, char **argv)
{
	pid_t pid, exited;
	char **child_argv = wmalloc(sizeof(char *) * (argc + 2));
	int i, status;
	time_t last_start;
	Bool error = False;

	for (i = 0; i < argc; i++)
		child_argv[i] = argv[i];
	child_argv[i++] = "--for-real";
	child_argv[i] = NULL;

	for (;;) {
		last_start = time(NULL);

		/* Start Window Maker */
		pid = fork();
		if (pid == 0) {
			execvp(child_argv[0], child_argv);
			werror(_("Error respawning Window Maker"));
			exit(1);
		} else if (pid < 0) {
			werror(_("Error respawning Window Maker"));
			exit(1);
		}

		do {
			exited = waitpid(-1, &status, 0);
			if (exited < 0) {
				werror(_("Error during monitoring of Window Maker process."));
				error = True;
				break;
			}
		} while (exited != pid);

		if (error)
			break;

		child_argv[argc] = "--for-real-";

		/* Check if the wmaker process exited due to a crash */
		if (WIFSIGNALED(status) &&
		    (WTERMSIG(status) == SIGSEGV ||
		     WTERMSIG(status) == SIGBUS ||
		     WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGABRT || WTERMSIG(status) == SIGFPE)) {
			/* If so, we check when was the last restart.
			 * If it was less than 3s ago, it's a bad sign, so we show
			 * the crash panel and ask the user what to do */
			if (time(NULL) - last_start < 3) {
				if (showCrashDialog(WTERMSIG(status)) == 0) {
					wfree(child_argv);
					return 1;
				}
			}
			wwarning(_("Window Maker exited due to a crash (signal %i) and will be restarted."),
				 WTERMSIG(status));
		} else
			break;
	}
	wfree(child_argv);
	return 0;
}
