/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
 * and individual contributors; see LICENSE for full attribution.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wconfig.h"

#include "WindowMaker.h"
#include "actions.h"
#include "session.h"
#include "shell.h"
#include "misc.h"
#include "shutdown.h"
#include "main.h"
#include "screen.h"

#include <WINGs/WUtil.h>

void shExec(virtual_screen *vscr, const char *cmdline)
{
	char *expanded;

	expanded = ExpandOptions(vscr, cmdline);
	if (!expanded)
		return;

	ExecuteShellCommand(vscr, expanded);
	wfree(expanded);
}

void shRestart(virtual_screen *vscr, const char *cmdline)
{
	(void)vscr;

	Shutdown(WSRestartPreparationMode);
	Restart((char *)cmdline, False);
	Restart(NULL, True);
}

void shRefresh(virtual_screen *vscr)
{
	wRefreshDesktop(vscr);
}

void shArrangeIcons(virtual_screen *vscr)
{
	wArrangeIcons(vscr, True);
}

void shShowAll(virtual_screen *vscr)
{
	wShowAllWindows(vscr);
}

void shHideOthers(virtual_screen *vscr)
{
	wHideOtherApplications(vscr->window.focused);
}

void shSaveSession(virtual_screen *vscr)
{
	if (!wPreferences.save_session_on_exit)
		wSessionSaveState(vscr);

	wScreenSaveState(vscr);
}

void shClearSession(virtual_screen *vscr)
{
	wSessionClearState();
	wScreenSaveState(vscr);
}
