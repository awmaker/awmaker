/* dock.c- Dock module for WindowMaker - Clip
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 2019 Rodolfo García Peñas (kix)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>

#include "WindowMaker.h"
#include "wcore.h"
#include "window.h"
#include "icon.h"
#include "appicon.h"
#include "actions.h"
#include "stacking.h"
#include "dock.h"
#include "dockedapp.h"
#include "dialog.h"
#include "shell.h"
#include "properties.h"
#include "menu.h"
#include "client.h"
#include "wdefaults.h"
#include "workspace.h"
#include "framewin.h"
#include "superfluous.h"
#include "xinerama.h"
#include "placement.h"
#include "misc.h"
#include "event.h"
#ifdef USE_DOCK_XDND
#include "xdnd.h"
#endif

enum
{
	CM_CLIPOPTSSUBMENU,
	CM_ONE,
	CM_SELECT,
	CM_SELECTALL,
	CM_KEEP_ICONS,
	CM_MOVE_ICONS,
	CM_REMOVE_ICONS,
	CM_ATTRACT,
	CM_LAUNCH,
	CM_BRING,
	CM_HIDE,
	CM_SETTINGS,
	CM_KILL
};

static void iconDblClick(WObjDescriptor *desc, XEvent *event);
static void clip_button2_menu(WObjDescriptor *desc, XEvent *event);
static void clip_button3_menu(WObjDescriptor *desc, XEvent *event);
static void updateWorkspaceMenu(WMenu *menu, WAppIcon *icon);
static void omnipresentCallback(WMenu *menu, WMenuEntry *entry);
static void renameCallback(WMenu *menu, WMenuEntry *entry);
static WMenu *makeWorkspaceMenu(virtual_screen *vscr);
static WMenu *clip_make_options_menu(virtual_screen *vscr);
static void switchWSCommand(WMenu *menu, WMenuEntry *entry);
static void clip_remove_icons_callback(WMenu *menu, WMenuEntry *entry);

WDock *clip_create(virtual_screen *vscr, WMPropList *state)
{
	WDock *dock;
	WAppIcon *btn;

	dock = dock_create_core(vscr);
	restore_clip_position(dock, state);

	btn = vscr->clip.icon;
	btn->dock = dock;

	dock->type = WM_CLIP;
	dock->on_right_side = 1;
	dock->icon_array[0] = btn;
	dock->menu = NULL;

	restore_state_lowered(dock, state);
	restore_state_collapsed(dock, state);
	(void) restore_state_autocollapsed(dock, state);
	restore_state_autoraise(dock, state);
	(void) restore_state_autoattracticons(dock, state);

	return dock;
}

void clip_icon_mouse_down(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *aicon = desc->parent;
	WDock *dock = aicon->dock;
	virtual_screen *vscr = aicon->icon->vscr;
	int sts;

	if (aicon->editing || WCHECK_STATE(WSTATE_MODAL))
		return;

	vscr->last_dock = dock;

	if (dock->menu && dock->menu->flags.mapped)
		wMenuUnmap(dock->menu);

	if (IsDoubleClick(vscr, event)) {
		/* double-click was not in the main clip icon */
		if (dock->type != WM_CLIP || aicon->xindex != 0 || aicon->yindex != 0
		    || getClipButton(event->xbutton.x, event->xbutton.y) == CLIP_IDLE) {
			iconDblClick(desc, event);
			return;
		}
	}

	switch (event->xbutton.button) {
	case Button1:
		if (event->xbutton.state & MOD_MASK)
			wDockLower(dock);
		else
			wDockRaise(dock);

		if ((event->xbutton.state & ShiftMask) && aicon != vscr->clip.icon && dock->type != WM_DOCK) {
			wIconSelect(aicon->icon);
			return;
		}

		if (aicon->yindex == 0 && aicon->xindex == 0) {
			if (getClipButton(event->xbutton.x, event->xbutton.y) != CLIP_IDLE)
				handleClipChangeWorkspace(vscr, event);
			else
				handleDockMove(dock, aicon, event);
		} else {
			Bool hasMoved = wHandleAppIconMove(aicon, event);
			if (wPreferences.single_click && !hasMoved)
				iconDblClick(desc, event);
		}
		break;
	case Button2:
		if (aicon == vscr->clip.icon) {
			clip_button2_menu(desc, event);
		} else if (event->xbutton.state & ShiftMask) {
			sts = wClipMakeIconOmnipresent(aicon, !aicon->omnipresent);
			if (sts == WO_FAILED || sts == WO_SUCCESS)
				wAppIconPaint(aicon);
		} else {
			WAppIcon *btn = desc->parent;

			if (!btn->launching && (!btn->running || (event->xbutton.state & ControlMask)))
				launchDockedApplication(btn, True);
		}
		break;
	case Button3:
		if (event->xbutton.send_event &&
		    XGrabPointer(dpy, aicon->icon->core->window, True, ButtonMotionMask
				 | ButtonReleaseMask | ButtonPressMask, GrabModeAsync,
				 GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
			wwarning("pointer grab failed for clip icon menu");
			return;
		}

		clip_button3_menu(desc, event);
		break;
	case Button4:
		wWorkspaceRelativeChange(vscr, 1);
		break;
	case Button5:
		wWorkspaceRelativeChange(vscr, -1);
	}
}

static void clip_button2_menu(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *aicon = desc->parent;
	virtual_screen *vscr = aicon->icon->vscr;
	WScreen *scr = vscr->screen_ptr;
	WMenu *wsMenu;
	int xpos;

	wsMenu = wWorkspaceMenuMake(vscr, False);
	wWorkspaceMenuUpdate(vscr, wsMenu);
	workspaces_set_menu_enabled_items(vscr, wsMenu);

	xpos = event->xbutton.x_root - wsMenu->frame->width / 2 - 1;
	if (xpos < 0)
		xpos = 0;
	else if (xpos + wsMenu->frame->width > scr->scr_width - 2)
		xpos = scr->scr_width - wsMenu->frame->width - 4;

	menu_map(wsMenu);
	wsMenu->x_pos = xpos;
	wsMenu->y_pos = event->xbutton.y_root + 2;
	wMenuMapAt(vscr, wsMenu, False);

	desc = &wsMenu->core->descriptor;
	/* allow drag select */
	event->xany.send_event = True;
	(*desc->handle_mousedown) (desc, event);

	wsMenu->flags.realized = 0;
	wMenuDestroy(wsMenu);
	wsMenu = NULL;
}

static void clip_button3_menu(WObjDescriptor *desc, XEvent *event)
{
	WMenu *opt_menu, *wks_menu;
	WObjDescriptor *desc2;
	WApplication *wapp = NULL;
	WAppIcon *aicon = desc->parent;
	WDock *clip = aicon->dock;
	virtual_screen *vscr = aicon->icon->vscr;
	WScreen *scr = vscr->screen_ptr;
	WMenuEntry *entry = NULL;
	int x_pos, n_selected, appIsRunning;

	/* Set some variables used in the menu */
	n_selected = numberOfSelectedIcons(clip);
	appIsRunning = aicon->running && aicon->icon && aicon->icon->owner;

	if (aicon->icon->owner)
		wapp = wApplicationOf(aicon->icon->owner->main_window);

	/* Create menus */
	clip->menu = menu_create(vscr, NULL);
	wks_menu = makeWorkspaceMenu(vscr);
	opt_menu = clip_make_options_menu(vscr);
	entry = wMenuAddCallback(clip->menu, _("Clip Options"), NULL, NULL);
	wMenuEntrySetCascade_create(clip->menu, entry, opt_menu);

	/*
	 * The same menu is used for the dock and its appicons. If the menu
	 * entry text is different between the two contexts, or if it can
	 * change depending on some state, free the duplicated string (from
	 * wMenuInsertCallback) and use gettext's string
	 */
	if (aicon == vscr->clip.icon)
		entry = wMenuAddCallback(clip->menu, _("Rename Workspace"), renameCallback, NULL);
	else
		if (n_selected > 0)
			entry = wMenuAddCallback(clip->menu, _("Toggle Omnipresent"), omnipresentCallback, NULL);
		else
			entry = wMenuAddCallback(clip->menu, _("Omnipresent"), omnipresentCallback, NULL);

	/* Selected */
	entry = wMenuAddCallback(clip->menu, _("Selected"), selectCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_on = 1;
	entry->flags.indicator_type = MI_CHECK;

	/* Select / Unslect All Icons */
	if (n_selected > 0)
		entry = wMenuAddCallback(clip->menu, _("Unselect All Icons"), selectIconsCallback, NULL);
	else
		entry = wMenuAddCallback(clip->menu, _("Select All Icons"), selectIconsCallback, NULL);

	/* Keep Icon / Keep Icons */
	if (n_selected > 1)
		entry = wMenuAddCallback(clip->menu, _("Keep Icons"), keepIconsCallback, NULL);
	else
		entry = wMenuAddCallback(clip->menu, _("Keep Icon"), keepIconsCallback, NULL);

	/* Move Icon or Icons To */
	if (n_selected > 1)
		entry = wMenuAddCallback(clip->menu, _("Move Icons To"), NULL, NULL);
	else
		entry = wMenuAddCallback(clip->menu, _("Move Icon To"), NULL, NULL);

	wMenuEntrySetCascade_create(clip->menu, entry, wks_menu);

	/* Remove Icon or Icons */
	if (n_selected > 1)
		entry = wMenuAddCallback(clip->menu, _("Remove Icons"), clip_remove_icons_callback, NULL);
	else
		entry = wMenuAddCallback(clip->menu, _("Remove Icon"), clip_remove_icons_callback, NULL);

	wMenuAddCallback(clip->menu, _("Attract Icons"), attractIconsCallback, NULL);
	wMenuAddCallback(clip->menu, _("Launch"), launchCallback, NULL);

	/* Unhide Here / Bring Here */
	if (wapp && wapp->flags.hidden)
		wMenuAddCallback(clip->menu, _("Unhide Here"), dockUnhideHereCallback, NULL);
	else
		wMenuAddCallback(clip->menu, _("Bring Here"), dockUnhideHereCallback, NULL);

	/* Hide / Unhide */
	if (wapp && wapp->flags.hidden)
		entry = wMenuAddCallback(clip->menu, _("Unhide"), dockHideCallback, NULL);
	else
		entry = wMenuAddCallback(clip->menu, _("Hide"), dockHideCallback, NULL);

	/* Settings */
	wMenuAddCallback(clip->menu, _("Settings..."), settingsCallback, NULL);

	/* Kill / Remove Drawer */
	if (wIsADrawer(aicon))
		entry = wMenuAddCallback(clip->menu, _("Remove drawer"), removeDrawerCallback, NULL);
	else
		entry = wMenuAddCallback(clip->menu, _("Kill"), dockKillCallback, NULL);

	/* clip/drawer options */
	dockUpdateOptionsMenu(clip, opt_menu);

	/* Rename Workspace */
	entry = clip->menu->entries[CM_ONE];
	if (aicon == vscr->clip.icon) {
		entry->clientdata = clip;
		entry->flags.indicator = 0;
	} else {
		entry->clientdata = aicon;
		if (n_selected > 0) {
			entry->flags.indicator = 0;
		} else {
			entry->flags.indicator = 1;
			entry->flags.indicator_on = aicon->omnipresent;
			entry->flags.indicator_type = MI_CHECK;
		}
	}

	/* select/unselect icon */
	entry = clip->menu->entries[CM_SELECT];
	entry->clientdata = aicon;
	entry->flags.indicator_on = aicon->icon->selected;
	menu_entry_set_enabled(clip->menu, CM_SELECT, aicon != vscr->clip.icon && !wIsADrawer(aicon));

	/* select/unselect all icons */
	entry = clip->menu->entries[CM_SELECTALL];
	entry->clientdata = aicon;
	menu_entry_set_enabled(clip->menu, CM_SELECTALL, clip->icon_count > 1);

	/* keep icon(s) */
	entry = clip->menu->entries[CM_KEEP_ICONS];
	entry->clientdata = aicon;
	menu_entry_set_enabled(clip->menu, CM_KEEP_ICONS, clip->icon_count > 1);

	/* this is the workspace submenu part */
	entry = clip->menu->entries[CM_MOVE_ICONS];
	updateWorkspaceMenu(wks_menu, aicon);
	menu_entry_set_enabled(clip->menu, CM_MOVE_ICONS, !aicon->omnipresent);

	/* remove icon(s) */
	entry = clip->menu->entries[CM_REMOVE_ICONS];
	entry->clientdata = aicon;
	menu_entry_set_enabled(clip->menu, CM_REMOVE_ICONS, clip->icon_count > 1);

	/* attract icon(s) */
	entry = clip->menu->entries[CM_ATTRACT];
	entry->clientdata = aicon;

	/* launch */
	entry = clip->menu->entries[CM_LAUNCH];
	entry->clientdata = aicon;
	menu_entry_set_enabled(clip->menu, CM_LAUNCH, aicon->command != NULL);

	/* unhide here */
	entry = clip->menu->entries[CM_BRING];
	entry->clientdata = aicon;
	menu_entry_set_enabled(clip->menu, CM_BRING, appIsRunning);

	/* hide */
	entry = clip->menu->entries[CM_HIDE];
	entry->clientdata = aicon;
	menu_entry_set_enabled(clip->menu, CM_HIDE, appIsRunning);

	/* settings */
	entry = clip->menu->entries[CM_SETTINGS];
	entry->clientdata = aicon;
	menu_entry_set_enabled(clip->menu, CM_SETTINGS, !aicon->editing && !wPreferences.flags.noupdates);

	/* kill or remove drawer */
	entry = clip->menu->entries[CM_KILL];
	entry->clientdata = aicon;
	if (wIsADrawer(aicon))
		menu_entry_set_enabled(clip->menu, CM_KILL, True);
	else
		menu_entry_set_enabled(clip->menu, CM_KILL, appIsRunning);

	menu_entry_set_enabled_paint(clip->menu, CM_SELECT);
	menu_entry_set_enabled_paint(clip->menu, CM_SELECTALL);
	menu_entry_set_enabled_paint(clip->menu, CM_KEEP_ICONS);
	menu_entry_set_enabled_paint(clip->menu, CM_MOVE_ICONS);
	menu_entry_set_enabled_paint(clip->menu, CM_REMOVE_ICONS);
	menu_entry_set_enabled_paint(clip->menu, CM_LAUNCH);
	menu_entry_set_enabled_paint(clip->menu, CM_BRING);
	menu_entry_set_enabled_paint(clip->menu, CM_HIDE);
	menu_entry_set_enabled_paint(clip->menu, CM_SETTINGS);
	menu_entry_set_enabled_paint(clip->menu, CM_KILL);

	x_pos = event->xbutton.x_root - clip->menu->frame->width / 2 - 1;
	if (x_pos < 0)
		x_pos = 0;
	else if (x_pos + clip->menu->frame->width > scr->scr_width - 2)
		x_pos = scr->scr_width - clip->menu->frame->width - 4;

	menu_map(clip->menu);
	menu_map(opt_menu);
	menu_map(wks_menu);
	clip->menu->flags.realized = 0;

	clip->menu->x_pos = x_pos;
	clip->menu->y_pos = event->xbutton.y_root + 2;
	wMenuMapAt(vscr, clip->menu, False);

	/* allow drag select */
	event->xany.send_event = True;
	desc2 = &clip->menu->core->descriptor;
	(*desc2->handle_mousedown) (desc2, event);

	opt_menu->flags.realized = 0;
	wks_menu->flags.realized = 0;
	clip->menu->flags.realized = 0;

	wMenuDestroy(clip->menu);

	wks_menu = NULL;
	opt_menu = NULL;
	clip->menu = NULL;
}
static void iconDblClick(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = desc->parent;
	WDock *dock = btn->dock;
	WApplication *wapp = NULL;
	int unhideHere = 0;

	if (btn->icon->owner && !(event->xbutton.state & ControlMask)) {
		wapp = wApplicationOf(btn->icon->owner->main_window);
		unhideHere = (event->xbutton.state & ShiftMask);

		/* go to the last workspace that the user worked on the app */
		if (wapp->last_workspace != dock->vscr->workspace.current && !unhideHere)
			wWorkspaceChange(dock->vscr, wapp->last_workspace);

		wUnhideApplication(wapp, event->xbutton.button == Button2, unhideHere);

		if (event->xbutton.state & MOD_MASK)
			wHideOtherApplications(btn->icon->owner);
	} else {
		if (event->xbutton.button == Button1) {
			if (event->xbutton.state & MOD_MASK) {
				/* raise/lower dock */
				toggleLowered(dock);
			} else if (btn == dock->vscr->clip.icon) {
				if (getClipButton(event->xbutton.x, event->xbutton.y) != CLIP_IDLE) {
					handleClipChangeWorkspace(dock->vscr, event);
				} else if (wPreferences.flags.clip_merged_in_dock) {
					/* Is actually the dock */
					if (btn->command) {
						if (!btn->launching && (!btn->running || (event->xbutton.state & ControlMask)))
							launchDockedApplication(btn, False);
					} else {
						panel_show(dock->vscr, PANEL_INFO);
					}
				} else {
					toggleCollapsed(dock);
				}
			} else if (btn->command) {
				if (!btn->launching && (!btn->running || (event->xbutton.state & ControlMask)))
					launchDockedApplication(btn, False);
			}
		}
	}
}

static void updateWorkspaceMenu(WMenu *menu, WAppIcon *icon)
{
	virtual_screen *vscr = menu->vscr;
	int i;

	if (!menu || !icon)
		return;

	for (i = 0; i < vscr->workspace.count; i++) {
		if (i < menu->entry_no) {
			if (strcmp(menu->entries[i]->text, vscr->workspace.array[i]->name) != 0) {
				wfree(menu->entries[i]->text);
				menu->entries[i]->text = wstrdup(vscr->workspace.array[i]->name);
				menu->flags.realized = 0;
			}

			menu->entries[i]->clientdata = (void *)icon;
		} else {
			wMenuAddCallback(menu, vscr->workspace.array[i]->name, switchWSCommand, (void *)icon);
			menu->flags.realized = 0;
		}

		if (i == vscr->workspace.current)
			menu_entry_set_enabled(menu, i, False);
		else
			menu_entry_set_enabled(menu, i, True);
	}

	for (i = 0; i < vscr->workspace.count; i++)
		menu_entry_set_enabled_paint(menu, i);

	menu->flags.realized = 0;
}
static WMenu *makeWorkspaceMenu(virtual_screen *vscr)
{
	WMenu *menu;

	menu = menu_create(vscr, NULL);

	wMenuAddCallback(menu, "", switchWSCommand, (void *) vscr->clip.icon);

	menu->flags.realized = 0;

	return menu;
}


static WMenu *clip_make_options_menu(virtual_screen *vscr)
{
	WMenu *menu;
	WMenuEntry *entry;

	menu = menu_create(vscr, NULL);

	entry = wMenuAddCallback(menu, _("Keep on Top"), toggleLoweredCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_on = 1;
	entry->flags.indicator_type = MI_CHECK;

	entry = wMenuAddCallback(menu, _("Collapsed"), toggleCollapsedCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_on = 1;
	entry->flags.indicator_type = MI_CHECK;

	entry = wMenuAddCallback(menu, _("Autocollapse"), toggleAutoCollapseCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_on = 1;
	entry->flags.indicator_type = MI_CHECK;

	entry = wMenuAddCallback(menu, _("Autoraise"), toggleAutoRaiseLowerCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_on = 1;
	entry->flags.indicator_type = MI_CHECK;

	entry = wMenuAddCallback(menu, _("Autoattract Icons"), toggleAutoAttractCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_on = 1;
	entry->flags.indicator_type = MI_CHECK;

	menu->flags.realized = 0;

	return menu;
}


static void renameCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock = entry->clientdata;
	char buffer[128];
	int wspace;
	char *name;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	/* Check if the screen exists */
	if (!dock->vscr->screen_ptr)
		return;

	assert(entry->clientdata != NULL);

	wspace = dock->vscr->workspace.current;
	name = wstrdup(dock->vscr->workspace.array[wspace]->name);

	snprintf(buffer, sizeof(buffer), _("Type the name for workspace %i:"), wspace + 1);
	if (wInputDialog(dock->vscr, _("Rename Workspace"), buffer, &name))
		wWorkspaceRename(dock->vscr, wspace, name);

	wfree(name);
}

static void switchWSCommand(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *btn, *icon = (WAppIcon *) entry->clientdata;
	virtual_screen *vscr = icon->icon->vscr;
	WDock *src, *dest;
	WMArray *selectedIcons;
	int x, y;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	if (entry->order == vscr->workspace.current)
		return;

	src = icon->dock;
	dest = vscr->workspace.array[entry->order]->clip;

	selectedIcons = getSelected(src);

	if (WMGetArrayItemCount(selectedIcons)) {
		WMArrayIterator iter;

		WM_ITERATE_ARRAY(selectedIcons, btn, iter) {
			if (wDockFindFreeSlot(dest, &x, &y)) {
				wDockMoveIconBetweenDocks(src, dest, btn, x, y);
				XUnmapWindow(dpy, btn->icon->core->window);
			}
		}
	} else if (icon != vscr->clip.icon) {
		if (wDockFindFreeSlot(dest, &x, &y)) {
			wDockMoveIconBetweenDocks(src, dest, icon, x, y);
			XUnmapWindow(dpy, icon->icon->core->window);
		}
	}

	WMFreeArray(selectedIcons);
}

static void clip_remove_icons_callback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *clickedIcon = (WAppIcon *) entry->clientdata;
	WDock *dock;
	WMArray *selectedIcons;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	assert(clickedIcon != NULL);
	dock = clickedIcon->dock;

	/* This is only for security, to avoid crash in PlaceIcon
	 * but it shouldn't happend, because the callback cannot
	 * be used without screen */
	if (!dock->vscr->screen_ptr)
		return;

	selectedIcons = getSelected(dock);
	if (WMGetArrayItemCount(selectedIcons)) {
		if (wMessageDialog(dock->vscr,
					_("Workspace Clip"),
					_("All selected icons will be removed!"),
					_("OK"), _("Cancel"), NULL) != WAPRDefault) {
			WMFreeArray(selectedIcons);
			return;
		}
	} else {
		if (clickedIcon->xindex == 0 && clickedIcon->yindex == 0) {
			WMFreeArray(selectedIcons);
			return;
		}

		WMAddToArray(selectedIcons, clickedIcon);
	}

	removeIcons(selectedIcons, dock);
}

static void omnipresentCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *clickedIcon = entry->clientdata;
	WAppIcon *aicon;
	WDock *dock;
	WMArray *selectedIcons;
	WMArrayIterator iter;
	int failed, sts;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	assert(entry->clientdata != NULL);

	dock = clickedIcon->dock;

	selectedIcons = getSelected(dock);

	if (!WMGetArrayItemCount(selectedIcons))
		WMAddToArray(selectedIcons, clickedIcon);

	failed = 0;
	WM_ITERATE_ARRAY(selectedIcons, aicon, iter) {
		sts = wClipMakeIconOmnipresent(aicon, !aicon->omnipresent);
		if (sts == WO_SUCCESS)
			wAppIconPaint(aicon);

		if (sts == WO_FAILED) {
			wAppIconPaint(aicon);
			failed++;
		} else if (aicon->icon->selected) {
			wIconSelect(aicon->icon);
		}
	}

	WMFreeArray(selectedIcons);

	/* If the screen is not painted, then we cannot show the dialog */
	if (dock->vscr->screen_ptr) {
		if (failed > 1)
			wMessageDialog(dock->vscr, _("Warning"),
				       _("Some icons cannot be made omnipresent. "
					 "Please make sure that no other icon is "
					 "docked in the same positions on the other "
					 "workspaces and the Clip is not full in "
					 "some workspace."), _("OK"), NULL, NULL);
		if (failed == 1)
			wMessageDialog(dock->vscr, _("Warning"),
				       _("Icon cannot be made omnipresent. "
					 "Please make sure that no other icon is "
					 "docked in the same position on the other "
					 "workspaces and the Clip is not full in "
					 "some workspace."), _("OK"), NULL, NULL);
	}
}
