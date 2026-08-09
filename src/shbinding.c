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
#include "dialog.h"
#include "main.h"
#include "screen.h"
#include "shbinding.h"
#include "keybind.h"

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

void shExit(virtual_screen *vscr, Bool quick)
{
	static int inside = 0;
	int result;

	/* prevent reentrant calls */
	if (inside)
		return;

	inside = 1;

#define R_CANCEL 0
#define R_EXIT   1

	result = R_CANCEL;

	if (quick) {
		result = R_EXIT;
	} else {
		int r, oldSaveSessionFlag;

		oldSaveSessionFlag = wPreferences.save_session_on_exit;
		r = wExitDialog(vscr, _("Exit"),
				_("Exit window manager?"), _("Exit"), _("Cancel"), NULL);

		if (r == WAPRDefault) {
			result = R_EXIT;
		} else if (r == WAPRAlternate) {
			/* Don't modify the "save session on exit" flag if the
			 * user canceled the operation. */
			wPreferences.save_session_on_exit = oldSaveSessionFlag;
		}
	}

	if (result == R_EXIT)
		Shutdown(WSExitMode);

#undef R_EXIT
#undef R_CANCEL
	inside = 0;
}

void shShutdown(virtual_screen *vscr, Bool quick)
{
	static int inside = 0;
	int result;

	/* prevent reentrant calls */
	if (inside)
		return;

	inside = 1;

#define R_CANCEL 0
#define R_CLOSE 1
#define R_KILL 2

	result = R_CANCEL;
	if (quick) {
		result = R_CLOSE;
	} else {
		int r, oldSaveSessionFlag;

		oldSaveSessionFlag = wPreferences.save_session_on_exit;

		r = wExitDialog(vscr,
				_("Kill X session"),
				_("Kill Window System session?\n"
				  "(all applications will be closed)"), _("Kill"), _("Cancel"), NULL);
		if (r == WAPRDefault) {
			result = R_KILL;
		} else if (r == WAPRAlternate) {
			/* Don't modify the "save session on exit" flag if the
			 * user canceled the operation. */
			wPreferences.save_session_on_exit = oldSaveSessionFlag;
		}
	}

	if (result != R_CANCEL)
		Shutdown(WSKillMode);

#undef R_CLOSE
#undef R_CANCEL
#undef R_KILL
	inside = 0;
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

void shInfoPanel(virtual_screen *vscr)
{
	panel_show(vscr, PANEL_INFO);
}

void shLegalPanel(virtual_screen *vscr)
{
	panel_show(vscr, PANEL_LEGAL);
}

/*
 * Persistent, session-lifetime binding list (F5, §8F5.2): the source of truth
 * fed by every wKeyBindings[WKBD_*] (as RSM_WKBD) and by the root-menu shortcuts
 * (decoded at parse time). Menus never feed or own it; they only paint labels.
 */
static SHBinding *shBindingList;

void shAddBinding(SHBinding *b)
{
	b->next = shBindingList;
	shBindingList = b;
}

/*
 * Dispatch a binding to its action. RSM_WKBD is a window keybinding: its actual
 * execution flows through handleKeyPress' normal switch / the key trie (F5-H/I),
 * so nothing runs here; the root-menu RSM_* actions run the Phase-1 logic
 * functions directly (no duplicated callbacks).
 */
void shRunAction(SHBinding *b, virtual_screen *vscr)
{
	switch (b->type) {
	case RSM_EXEC:
		shExec(vscr, b->cmd);
		break;
	case RSM_RESTART:
		shRestart(vscr, b->cmd);
		break;
	case RSM_EXIT:
		shExit(vscr, b->quick);
		break;
	case RSM_SHUTDOWN:
		shShutdown(vscr, b->quick);
		break;
	case RSM_REFRESH:
		shRefresh(vscr);
		break;
	case RSM_ARRANGE_ICONS:
		shArrangeIcons(vscr);
		break;
	case RSM_HIDE_OTHERS:
		shHideOthers(vscr);
		break;
	case RSM_SHOW_ALL:
		shShowAll(vscr);
		break;
	case RSM_SAVE_SESSION:
		shSaveSession(vscr);
		break;
	case RSM_CLEAR_SESSION:
		shClearSession(vscr);
		break;
	case RSM_INFO_PANEL:
		shInfoPanel(vscr);
		break;
	case RSM_LEGAL_PANEL:
		shLegalPanel(vscr);
		break;
	case RSM_WKBD:
		break;
	}
}

static void shFreeBindings(void)
{
	SHBinding *b, *tmp;

	for (b = shBindingList; b != NULL; b = tmp) {
		tmp = b->next;
		wfree(b->chain_modifiers);
		wfree(b->chain_keycodes);
		wfree(b->cmd);
		wfree(b);
	}
	shBindingList = NULL;
}

/*
 * Register every window keybinding (wKeyBindings[0..WKBD_LAST-1]) as an
 * RSM_WKBD binding — the single source of truth. Chains (F5-I) and root-menu
 * shortcuts (F5-J) will add further bindings through shAddBinding.
 */
void shRebuildList(void)
{
	int i;

	shFreeBindings();

	for (i = 0; i < WKBD_LAST; i++) {
		SHBinding *b;

		if (wKeyBindings[i].keycode == 0)
			continue;

		b = wmalloc(sizeof(SHBinding));
		b->modifier = wKeyBindings[i].modifier;
		b->keycode = wKeyBindings[i].keycode;
		b->chain_length = 1;
		b->type = RSM_WKBD;
		b->wkbd_idx = i;
		shAddBinding(b);
	}
}
