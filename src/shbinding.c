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
#include "keytree.h"

#include <string.h>

#include <X11/XKBlib.h>

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

/*
 * Root-menu shortcut bindings (F5-J/K). 'shMenuBindings' is the canonical,
 * persistently-owned collection of root-menu shortcuts; 'shBindingList' is a
 * derived runtime list (owned clones) rebuilt by shRebuildList from
 * wKeyBindings + shMenuBindings. Keeping the two separate lets a root-menu
 * rebuild clear+recollect without touching the wKeyBindings-derived part and
 * without sharing/double-freeing nodes.
 */
static SHBinding *shMenuBindings;

void shAddBinding(SHBinding *b)
{
	b->next = shBindingList;
	shBindingList = b;
}

SHBinding *shGetBindings(void)
{
	return shBindingList;
}

void shAddMenuBinding(SHBinding *b)
{
	b->next = shMenuBindings;
	shMenuBindings = b;
}

void shClearMenuBindings(void)
{
	SHBinding *b, *tmp;

	for (b = shMenuBindings; b != NULL; b = tmp) {
		tmp = b->next;
		wfree(b->cmd);
		wfree(b->chain_modifiers);
		wfree(b->chain_keycodes);
		wfree(b);
	}
	shMenuBindings = NULL;

	/* drop them from the derived runtime list */
	shRebuildList();
}

static SHBinding *shCloneBinding(const SHBinding *src)
{
	SHBinding *b = wmalloc(sizeof(SHBinding));

	*b = *src;
	b->next = NULL;

	if (src->cmd)
		b->cmd = wstrdup(src->cmd);

	if (src->chain_length > 1) {
		int n = src->chain_length - 1;

		b->chain_modifiers = wmalloc(n * sizeof(unsigned int));
		b->chain_keycodes = wmalloc(n * sizeof(KeyCode));
		memcpy(b->chain_modifiers, src->chain_modifiers, n * sizeof(unsigned int));
		memcpy(b->chain_keycodes, src->chain_keycodes, n * sizeof(KeyCode));
	} else {
		b->chain_modifiers = NULL;
		b->chain_keycodes = NULL;
	}

	return b;
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
 * Rebuild the runtime SHBinding list from wKeyBindings (RSM_WKBD) + the
 * canonical root-menu shortcuts (shMenuBindings) (F5-G/F5-J). The single source
 * of truth for the key trie. Each entry is a clone so it is owned by the
 * runtime list; the canonical menu collection stays untouched.
 */
void shRebuildList(void)
{
	int i;
	SHBinding *mb;

	shFreeBindings();

	for (i = 0; i < WKBD_LAST; i++) {
		if (wKeyBindings[i].keycode == 0)
			continue;

		SHBinding *b = shCloneBinding(&(SHBinding){
			.modifier = wKeyBindings[i].modifier,
			.keycode = wKeyBindings[i].keycode,
			.chain_length = 1,
			.type = RSM_WKBD,
			.wkbd_idx = i,
		});
		shAddBinding(b);
	}

	for (mb = shMenuBindings; mb != NULL; mb = mb->next)
		shAddBinding(shCloneBinding(mb));
}

/*
 * Rebuild the key-chain trie from the SHBinding list (F5-H).
 *
 * Every binding in the list becomes one leaf in wKeyTreeRoot, keyed by its key
 * sequence (leader key + followers for a chain). Multiple bindings sharing a
 * sequence all attach to the same leaf (insertion order).
 *
 * Inert by itself: the executor (F5-I, handleKeyPress) has not been switched
 * over yet, so this commit changes no runtime behaviour.
 */
void wKeyTreeRebuild(void)
{
	SHBinding *b;
	unsigned int mods[10];
	KeyCode keys[10];

	wKeyTreeDestroy(wKeyTreeRoot);
	wKeyTreeRoot = NULL;

	for (b = shBindingList; b != NULL; b = b->next) {
		WKeyNode *leaf;
		int len, j;

		if (b->keycode == 0)
			continue;

		len = (b->chain_length > 1) ? b->chain_length : 1;
		mods[0] = b->modifier;
		keys[0] = b->keycode;

		for (j = 1; j < len; j++) {
			mods[j] = b->chain_modifiers[j - 1];
			keys[j] = b->chain_keycodes[j - 1];
		}

		if (len > 10)
			len = 10;

		leaf = wKeyTreeInsert(&wKeyTreeRoot, mods, keys, len);
		if (leaf)
			wKeyNodeAddBinding(leaf, b);
	}
}

/*
 * Render a binding's key sequence as a shortcut label ("Control+Alt+R", chain
 * keys joined with '+') into buf. Used by menus purely to paint the label (F5-M);
 * return the label length.
 */
unsigned int shLabelFor(const SHBinding *b, char *buf, unsigned int buflen)
{
	int i, len;

	buf[0] = '\0';

	len = (b->chain_length > 1) ? b->chain_length : 1;

	for (i = 0; i < len; i++) {
		unsigned int mod = (i == 0) ? b->modifier : b->chain_modifiers[i - 1];
		KeyCode key = (i == 0) ? b->keycode : b->chain_keycodes[i - 1];
		char part[128];

		if (i > 0)
			wstrlcat(buf, "+", buflen);

		if (GetCanonicalShortcutLabel(mod, XkbKeycodeToKeysym(dpy, key, 0, 0),
					      part, sizeof(part)))
			wstrlcat(buf, part, buflen);
		else
			wstrlcat(buf, "?", buflen);
	}

	return (unsigned int)strlen(buf);
}
