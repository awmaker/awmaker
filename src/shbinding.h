/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
 * and individual contributors; see LICENSE for full attribution.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMSHBINDING_H
#define WMSHBINDING_H

#include "screen.h"

/*
 * Shortcut-action logic functions (F5, §8F5).
 *
 * These are the menu-independent handlers a root-menu shortcut can run. They
 * take a virtual_screen (or the data the action needs) instead of a WMenu /
 * WMenuEntry, so a binding can be executed without any menu being materialized.
 * The root-menu callbacks call these directly (as thin wrappers).
 */

void shExec(virtual_screen *vscr, const char *cmdline);
void shRestart(virtual_screen *vscr, const char *cmdline);
void shExit(virtual_screen *vscr, Bool quick);
void shShutdown(virtual_screen *vscr, Bool quick);
void shRefresh(virtual_screen *vscr);
void shArrangeIcons(virtual_screen *vscr);
void shShowAll(virtual_screen *vscr);
void shHideOthers(virtual_screen *vscr);
void shSaveSession(virtual_screen *vscr);
void shClearSession(virtual_screen *vscr);
void shInfoPanel(virtual_screen *vscr);
void shLegalPanel(virtual_screen *vscr);
void shKeybindsPanel(virtual_screen *vscr);

/*
 * SHBinding — persistent, menu-independent keybinding/action list (F5, §8F5).
 *
 * Unlike the deleted shortcutList, an SHBinding stores only data (action type +
 * parameters), never a pointer into a materialized menu, so it cannot dangle
 * when the root menu is rebuilt (the F5 SIGSEGV root cause). Execution goes
 * through shRunAction, which dispatches to the sh* logic functions above.
 */

typedef enum {
	RSM_EXEC,           /* run cmd via shExec (cmd) */
	RSM_RESTART,        /* restart WM via shRestart (cmd, optional) */
	RSM_EXIT,           /* exit via shExit (quick) */
	RSM_SHUTDOWN,       /* shutdown via shShutdown (quick) */
	RSM_REFRESH,        /* shRefresh */
	RSM_ARRANGE_ICONS,  /* shArrangeIcons */
	RSM_HIDE_OTHERS,    /* shHideOthers */
	RSM_SHOW_ALL,       /* shShowAll */
	RSM_SAVE_SESSION,   /* shSaveSession */
	RSM_CLEAR_SESSION,  /* shClearSession */
	RSM_INFO_PANEL,     /* shInfoPanel */
	RSM_LEGAL_PANEL,    /* shLegalPanel */
	RSM_KEYBINDS_PANEL, /* shKeybindsPanel */
	RSM_WKBD            /* a window-keybinding action (wkbd_idx) */
} SHActionType;

typedef struct SHBinding {
	unsigned int modifier;
	KeyCode keycode;
	int chain_length;               /* 1 if not a chain */
	unsigned int *chain_modifiers;
	KeyCode *chain_keycodes;
	SHActionType type;
	char *cmd;                      /* RSM_EXEC/RESTART: params (owned) */
	Bool quick;                     /* RSM_EXIT/SHUTDOWN */
	int wkbd_idx;                   /* RSM_WKBD: WKBD_* index */
	struct SHBinding *next;
} SHBinding;

void shAddBinding(SHBinding *b);
void shRebuildList(void);
void shRunAction(SHBinding *b, virtual_screen *vscr);
unsigned int shLabelFor(const SHBinding *b, char *buf, unsigned int buflen);

/* Runtime binding list accessor (F5-L): the derived list rebuilt by
 * shRebuildList. Read-only iteration by callers (e.g. wRootMenuBindShortcuts /
 * wRootMenuPerformShortcut) for keygrabs / fallback dispatch. */
SHBinding *shGetBindings(void);

/*
 * Root-menu shortcut bindings (F5-J/K). They are collected in their own list
 * (shAddMenuBinding), cleared on root-menu destruction (shClearMenuBindings),
 * and merged into the runtime list by shRebuildList. Keeping them separate from
 * the per-menu menu/entry pointers lets the key trie execute them without any
 * menu object, and lets a root-menu rebuild recollect cleanly (no duplicates).
 */
void shAddMenuBinding(SHBinding *b);
void shClearMenuBindings(void);

/*
 * Rebuild the key-chain trie from the SHBinding list (F5-H). Populates
 * wKeyTreeRoot with one leaf per binding, keyed by its (chain of) key(s).
 * Called on config change (wKeyTreeRebuild) — data-only, never reentrant with
 * menu open/close, so no use-after-free (the F5 SIGSEGV root cause).
 */
void wKeyTreeRebuild(void);

#endif /* WMSHBINDING_H */
