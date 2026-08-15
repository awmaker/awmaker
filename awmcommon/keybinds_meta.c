/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <awconfig.h>

#include <string.h>

#include <keybinds.h>
#include <keybinds_meta.h>

const KeyBindingMeta keybinds_meta[] = {
	{"RootMenuKey", WKBD_ROOTMENU, N_("Open applications menu")},
	{"WindowMenuKey", WKBD_WINDOWMENU, N_("Open window commands menu")},
	{"WindowListKey", WKBD_WINDOWLIST, N_("Open window list menu")},
	{"WorkspaceMenuKey", WKBD_WORKSPACEMENU, N_("Open workspace menu")},
	{"MiniaturizeKey", WKBD_MINIATURIZE, N_("Miniaturize active window")},
	{"MinimizeAllKey", WKBD_MINIMIZEALL, N_("Miniaturize all windows")},
	{"HideKey", WKBD_HIDE, N_("Hide active application")},
	{"HideOthersKey", WKBD_HIDE_OTHERS, N_("Hide other applications")},
	{"MaximizeKey", WKBD_MAXIMIZE, N_("Maximize active window")},
	{"VMaximizeKey", WKBD_VMAXIMIZE, N_("Maximize active window vertically")},
	{"HMaximizeKey", WKBD_HMAXIMIZE, N_("Maximize active window horizontally")},
	{"CentralKey", WKBD_CENTRAL, N_("Maximize active window central")},
	{"LHMaximizeKey", WKBD_LHMAXIMIZE, N_("Maximize active window left half")},
	{"RHMaximizeKey", WKBD_RHMAXIMIZE, N_("Maximize active window right half")},
	{"THMaximizeKey", WKBD_THMAXIMIZE, N_("Maximize active window top half")},
	{"BHMaximizeKey", WKBD_BHMAXIMIZE, N_("Maximize active window bottom half")},
	{"LTCMaximizeKey", WKBD_LTCMAXIMIZE, N_("Maximize active window left top corner")},
	{"RTCMaximizeKey", WKBD_RTCMAXIMIZE, N_("Maximize active window right top corner")},
	{"LBCMaximizeKey", WKBD_LBCMAXIMIZE, N_("Maximize active window left bottom corner")},
	{"RBCMaximizeKey", WKBD_RBCMAXIMIZE, N_("Maximize active window right bottom corner")},
	{"MaximusKey", WKBD_MAXIMUS, N_("Tiled maximization")},
	{"SelectKey", WKBD_SELECT, N_("Select active window")},
	{"KeepOnTopKey", WKBD_KEEP_ON_TOP, N_("Toggle window on top status")},
	{"KeepAtBottomKey", WKBD_KEEP_AT_BOTTOM, N_("Toggle window at bottom status")},
	{"OmnipresentKey", WKBD_OMNIPRESENT, N_("Toggle window omnipresent status")},
	{"RaiseKey", WKBD_RAISE, N_("Raise active window")},
	{"LowerKey", WKBD_LOWER, N_("Lower active window")},
	{"RaiseLowerKey", WKBD_RAISELOWER, N_("Raise/Lower window under mouse pointer")},
	{"MoveResizeKey", WKBD_MOVERESIZE, N_("Move/Resize active window")},
	{"ShadeKey", WKBD_SHADE, N_("Shade active window")},
	{"WorkspaceMapKey", WKBD_WORKSPACEMAP, N_("Open workspace pager")},
	{"FocusNextKey", WKBD_FOCUSNEXT, N_("Focus next window")},
	{"FocusPrevKey", WKBD_FOCUSPREV, N_("Focus previous window")},
	{"FocusWindowLeftKey", WKBD_FOCUSLEFT, N_("Focus window to the left")},
	{"FocusWindowRightKey", WKBD_FOCUSRIGHT, N_("Focus window to the right")},
	{"FocusWindowUpKey", WKBD_FOCUSUP, N_("Focus window above")},
	{"FocusWindowDownKey", WKBD_FOCUSDOWN, N_("Focus window below")},
	{"GroupNextKey", WKBD_GROUPNEXT, N_("Focus next group window")},
	{"GroupPrevKey", WKBD_GROUPPREV, N_("Focus previous group window")},
	{"MarkSetKey", WKBD_MARK_SET, N_("Mark window: set mark")},
	{"MarkUnsetKey", WKBD_MARK_UNSET, N_("Mark window: unset mark")},
	{"MarkBringKey", WKBD_MARK_BRING, N_("Mark window: bring marked window here")},
	{"MarkJumpKey", WKBD_MARK_JUMP, N_("Mark window: jump to marked window")},
	{"MarkSwapKey", WKBD_MARK_SWAP, N_("Mark window: swap with marked window")},
	{"CloseKey", WKBD_CLOSE, N_("Close active window")},
	{"DockRaiseLowerKey", WKBD_DOCKRAISELOWER, N_("Raise/Lower Dock")},
	{"ClipRaiseLowerKey", WKBD_CLIPRAISELOWER, N_("Raise/Lower Clip")},
	{"Workspace1Key", WKBD_WORKSPACE1, N_("Switch to workspace 1")},
	{"Workspace2Key", WKBD_WORKSPACE2, N_("Switch to workspace 2")},
	{"Workspace3Key", WKBD_WORKSPACE3, N_("Switch to workspace 3")},
	{"Workspace4Key", WKBD_WORKSPACE4, N_("Switch to workspace 4")},
	{"Workspace5Key", WKBD_WORKSPACE5, N_("Switch to workspace 5")},
	{"Workspace6Key", WKBD_WORKSPACE6, N_("Switch to workspace 6")},
	{"Workspace7Key", WKBD_WORKSPACE7, N_("Switch to workspace 7")},
	{"Workspace8Key", WKBD_WORKSPACE8, N_("Switch to workspace 8")},
	{"Workspace9Key", WKBD_WORKSPACE9, N_("Switch to workspace 9")},
	{"Workspace10Key", WKBD_WORKSPACE10, N_("Switch to workspace 10")},
	{"NextWorkspaceKey", WKBD_NEXTWORKSPACE, N_("Switch to next workspace")},
	{"PrevWorkspaceKey", WKBD_PREVWORKSPACE, N_("Switch to previous workspace")},
	{"LastWorkspaceKey", WKBD_LASTWORKSPACE, N_("Switch to last used workspace")},
	{"NextWorkspaceLayerKey", WKBD_NEXTWSLAYER, N_("Switch to next ten workspaces")},
	{"PrevWorkspaceLayerKey", WKBD_PREVWSLAYER, N_("Switch to previous ten workspaces")},
	{"MoveToWorkspace1Key", WKBD_MOVE_WORKSPACE1, N_("Move window to workspace 1")},
	{"MoveToWorkspace2Key", WKBD_MOVE_WORKSPACE2, N_("Move window to workspace 2")},
	{"MoveToWorkspace3Key", WKBD_MOVE_WORKSPACE3, N_("Move window to workspace 3")},
	{"MoveToWorkspace4Key", WKBD_MOVE_WORKSPACE4, N_("Move window to workspace 4")},
	{"MoveToWorkspace5Key", WKBD_MOVE_WORKSPACE5, N_("Move window to workspace 5")},
	{"MoveToWorkspace6Key", WKBD_MOVE_WORKSPACE6, N_("Move window to workspace 6")},
	{"MoveToWorkspace7Key", WKBD_MOVE_WORKSPACE7, N_("Move window to workspace 7")},
	{"MoveToWorkspace8Key", WKBD_MOVE_WORKSPACE8, N_("Move window to workspace 8")},
	{"MoveToWorkspace9Key", WKBD_MOVE_WORKSPACE9, N_("Move window to workspace 9")},
	{"MoveToWorkspace10Key", WKBD_MOVE_WORKSPACE10, N_("Move window to workspace 10")},
	{"MoveToNextWorkspaceKey", WKBD_MOVE_NEXTWORKSPACE, N_("Move window to next workspace")},
	{"MoveToPrevWorkspaceKey", WKBD_MOVE_PREVWORKSPACE, N_("Move window to previous workspace")},
	{"MoveToLastWorkspaceKey", WKBD_MOVE_LASTWORKSPACE, N_("Move window to last used workspace")},
	{"MoveToNextWorkspaceLayerKey", WKBD_MOVE_NEXTWSLAYER, N_("Move window to next ten workspaces")},
	{"MoveToPrevWorkspaceLayerKey", WKBD_MOVE_PREVWSLAYER, N_("Move window to previous ten workspaces")},
	{"WindowShortcut1Key", WKBD_WINDOW1, N_("Shortcut for window 1")},
	{"WindowShortcut2Key", WKBD_WINDOW2, N_("Shortcut for window 2")},
	{"WindowShortcut3Key", WKBD_WINDOW3, N_("Shortcut for window 3")},
	{"WindowShortcut4Key", WKBD_WINDOW4, N_("Shortcut for window 4")},
	{"WindowShortcut5Key", WKBD_WINDOW5, N_("Shortcut for window 5")},
	{"WindowShortcut6Key", WKBD_WINDOW6, N_("Shortcut for window 6")},
	{"WindowShortcut7Key", WKBD_WINDOW7, N_("Shortcut for window 7")},
	{"WindowShortcut8Key", WKBD_WINDOW8, N_("Shortcut for window 8")},
	{"WindowShortcut9Key", WKBD_WINDOW9, N_("Shortcut for window 9")},
	{"WindowShortcut10Key", WKBD_WINDOW10, N_("Shortcut for window 10")},
	{"MoveTo12to6Head", WKBD_MOVE_12_TO_6_HEAD, N_("Move to right/bottom/left/top head")},
	{"MoveTo6to12Head", WKBD_MOVE_6_TO_12_HEAD, N_("Move to left/top/right/bottom head")},
	{"WindowRelaunchKey", WKBD_RELAUNCH, N_("Launch new instance of application")},
	{"ScreenSwitchKey", WKBD_SWITCH_SCREEN, N_("Switch to Next Screen/Monitor")},
	{"RunKey", WKBD_RUN, N_("Run application")},
#ifdef KEEP_XKB_LOCK_STATUS
	{"ToggleKbdModeKey", WKBD_TOGGLE, N_("Toggle keyboard language")},
#endif
};

const int nb_keybindings = (int)(sizeof(keybinds_meta) / sizeof(keybinds_meta[0]));

const char *KeyBindingTitle(int wkbd)
{
	int i;

	for (i = 0; i < nb_keybindings; i++) {
		if (keybinds_meta[i].wkbd == wkbd)
			return keybinds_meta[i].title;
	}
	return NULL;
}

const char *KeyBindingForKey(int wkbd)
{
	int i;

	for (i = 0; i < nb_keybindings; i++) {
		if (keybinds_meta[i].wkbd == wkbd)
			return keybinds_meta[i].key;
	}
	return NULL;
}

int WkbdForKey(const char *key)
{
	int i;

	if (!key)
		return -1;
	for (i = 0; i < nb_keybindings; i++) {
		if (strcmp(keybinds_meta[i].key, key) == 0)
			return keybinds_meta[i].wkbd;
	}
	return -1;
}
