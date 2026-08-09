/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMWORKSPACE_H_
#define WMWORKSPACE_H_



typedef struct WWorkspace {
	char *name;
	struct WDock *clip;
	RImage *map;
} WWorkspace;

void workspace_create(virtual_screen *vscr);
void workspace_map(virtual_screen *vscr, WWorkspace *wspace, int wksno, WMPropList *parr);

int wGetWorkspaceNumber(virtual_screen *vscr, const char *value);
Bool wWorkspaceDelete(virtual_screen *vscr, int workspace);
void wWorkspaceChange(virtual_screen *vscr, int workspace);
void wWorkspaceForceChange(virtual_screen *vscr, int workspace);
WMenu *wWorkspaceMenuMake(virtual_screen *vscr, const char *title);
void wWorkspaceMenuUpdate(virtual_screen *vscr, WMenu *menu);
void wWorkspaceMenuUpdate_map(virtual_screen *vscr);
void wWorkspaceMenuEdit(virtual_screen *vscr);
void OpenWorkspaceMenu(virtual_screen *vscr, int x, int y, int keyboard);
void wWorkspaceSaveState(virtual_screen *vscr, WMPropList *old_state);
void wWorkspaceRename(virtual_screen *vscr, int workspace, const char *name);
void wWorkspaceRelativeChange(virtual_screen *vscr, int amount);

void workspaces_restore(virtual_screen *vscr);
void workspaces_restore_map(virtual_screen *vscr);
void workspaces_set_menu_enabled_items(virtual_screen *vscr, WMenu *menu);
#endif
