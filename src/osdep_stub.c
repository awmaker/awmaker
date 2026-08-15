/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <sys/utsname.h>

#include <WINGs/WUtil.h>

#include "awconfig.h"
#include "osdep.h"

Bool GetCommandForPid(int pid, char ***argv, int *argc)
{
	static int notified = 0;

	if (!notified) {
		struct utsname un;

		/* The comment below is placed in the PO file by xgettext to help translator */
		if (uname(&un) != -1) {
			/*
			 *  1st %s is a function name
			 *  2nd %s is an email address
			 *  3rd %s is the name of the operating system
			 */
			wwarning(_("%s is not implemented on this platform; "
			           "tell %s you are running %s release %s version %s"),
			         __FUNCTION__, PACKAGE_BUGREPORT,
				un.sysname, un.release, un.version);
			notified = 1;
		}

	}

	*argv = NULL;
	*argc = 0;

	return False;
}
