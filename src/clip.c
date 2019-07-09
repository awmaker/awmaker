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
#include "dock-core.h"
#include "dock.h"
#include "clip.h"
#include "drawer.h"
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
static void clip_autocollapse(void *cdata);
static void clip_set_attacheddocks(WDock *dock, WMPropList *state);
static void restore_clip_position(WDock *dock, WMPropList *state);
static void restore_clip_position_map(WDock *dock);
static void omnipresentCallback(WMenu *menu, WMenuEntry *entry);
static void renameCallback(WMenu *menu, WMenuEntry *entry);
static WMenu *makeWorkspaceMenu(virtual_screen *vscr);
static WMenu *clip_make_options_menu(virtual_screen *vscr);
static void switchWSCommand(WMenu *menu, WMenuEntry *entry);
static void clip_remove_icons_callback(WMenu *menu, WMenuEntry *entry);
static int clip_set_attacheddocks_do(WDock *dock, WMPropList *apps);

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
		if (event->xbutton.state & wPreferences.modifier_mask)
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

		if (event->xbutton.state & wPreferences.modifier_mask)
			wHideOtherApplications(btn->icon->owner);
	} else {
		if (event->xbutton.button == Button1) {
			if (event->xbutton.state & wPreferences.modifier_mask) {
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

static void paintClipButtons(WAppIcon *clipIcon, Bool lpushed, Bool rpushed)
{
	Window win = clipIcon->icon->core->window;
	WScreen *scr = clipIcon->icon->vscr->screen_ptr;
	XPoint p[4];
	int pt = CLIP_BUTTON_SIZE * ICON_SIZE / 64;
	int tp = ICON_SIZE - pt;
	int as = pt - 15;	/* 15 = 5+5+5 */
	GC gc = scr->draw_gc;	/* maybe use WMColorGC() instead here? */
	WMColor *color;

	color = scr->clip_title_color[CLIP_NORMAL];

	XSetForeground(dpy, gc, WMColorPixel(color));

	if (rpushed) {
		p[0].x = tp + 1;
		p[0].y = 1;
		p[1].x = ICON_SIZE - 2;
		p[1].y = 1;
		p[2].x = ICON_SIZE - 2;
		p[2].y = pt - 1;
	} else if (lpushed) {
		p[0].x = 1;
		p[0].y = tp;
		p[1].x = pt;
		p[1].y = ICON_SIZE - 2;
		p[2].x = 1;
		p[2].y = ICON_SIZE - 2;
	}
	if (lpushed || rpushed) {
		XSetForeground(dpy, scr->draw_gc, scr->white_pixel);
		XFillPolygon(dpy, win, scr->draw_gc, p, 3, Convex, CoordModeOrigin);
		XSetForeground(dpy, scr->draw_gc, scr->black_pixel);
	}

	/* top right arrow */
	p[0].x = p[3].x = ICON_SIZE - 5 - as;
	p[0].y = p[3].y = 5;
	p[1].x = ICON_SIZE - 6;
	p[1].y = 5;
	p[2].x = ICON_SIZE - 6;
	p[2].y = 4 + as;
	if (rpushed) {
		XFillPolygon(dpy, win, scr->draw_gc, p, 3, Convex, CoordModeOrigin);
		XDrawLines(dpy, win, scr->draw_gc, p, 4, CoordModeOrigin);
	} else {
		XFillPolygon(dpy, win, gc, p, 3, Convex, CoordModeOrigin);
		XDrawLines(dpy, win, gc, p, 4, CoordModeOrigin);
	}

	/* bottom left arrow */
	p[0].x = p[3].x = 5;
	p[0].y = p[3].y = ICON_SIZE - 5 - as;
	p[1].x = 5;
	p[1].y = ICON_SIZE - 6;
	p[2].x = 4 + as;
	p[2].y = ICON_SIZE - 6;
	if (lpushed) {
		XFillPolygon(dpy, win, scr->draw_gc, p, 3, Convex, CoordModeOrigin);
		XDrawLines(dpy, win, scr->draw_gc, p, 4, CoordModeOrigin);
	} else {
		XFillPolygon(dpy, win, gc, p, 3, Convex, CoordModeOrigin);
		XDrawLines(dpy, win, gc, p, 4, CoordModeOrigin);
	}
}

RImage *wClipMakeTile(RImage *normalTile)
{
	RImage *tile = RCloneImage(normalTile);
	RColor black;
	RColor dark;
	RColor light;
	int pt, tp;
	int as;

	pt = CLIP_BUTTON_SIZE * wPreferences.icon_size / 64;
	tp = wPreferences.icon_size - 1 - pt;
	as = pt - 15;

	black.alpha = 255;
	black.red = black.green = black.blue = 0;

	dark.alpha = 0;
	dark.red = dark.green = dark.blue = 60;

	light.alpha = 0;
	light.red = light.green = light.blue = 80;

	/* top right */
	ROperateLine(tile, RSubtractOperation, tp, 0, wPreferences.icon_size - 2, pt - 1, &dark);
	RDrawLine(tile, tp - 1, 0, wPreferences.icon_size - 1, pt + 1, &black);
	ROperateLine(tile, RAddOperation, tp, 2, wPreferences.icon_size - 3, pt, &light);

	/* arrow bevel */
	ROperateLine(tile, RSubtractOperation, ICON_SIZE - 7 - as, 4, ICON_SIZE - 5, 4, &dark);
	ROperateLine(tile, RSubtractOperation, ICON_SIZE - 6 - as, 5, ICON_SIZE - 5, 6 + as, &dark);
	ROperateLine(tile, RAddOperation, ICON_SIZE - 5, 4, ICON_SIZE - 5, 6 + as, &light);

	/* bottom left */
	ROperateLine(tile, RAddOperation, 2, tp + 2, pt - 2, wPreferences.icon_size - 3, &dark);
	RDrawLine(tile, 0, tp - 1, pt + 1, wPreferences.icon_size - 1, &black);
	ROperateLine(tile, RSubtractOperation, 0, tp - 2, pt + 1, wPreferences.icon_size - 2, &light);

	/* arrow bevel */
	ROperateLine(tile, RSubtractOperation, 4, ICON_SIZE - 7 - as, 4, ICON_SIZE - 5, &dark);
	ROperateLine(tile, RSubtractOperation, 5, ICON_SIZE - 6 - as, 6 + as, ICON_SIZE - 5, &dark);
	ROperateLine(tile, RAddOperation, 4, ICON_SIZE - 5, 6 + as, ICON_SIZE - 5, &light);

	return tile;
}

void wClipIconPaint(WAppIcon *aicon)
{
	virtual_screen *vscr = aicon->icon->vscr;
	WScreen *scr = vscr->screen_ptr;
	WWorkspace *workspace = vscr->workspace.array[vscr->workspace.current];
	WMColor *color;
	Window win = aicon->icon->core->window;
	int length, nlength;
	char *ws_name, ws_number[10];
	int ty, tx;

	wIconPaint(aicon->icon);

	length = strlen(workspace->name);
	ws_name = wmalloc(length + 1);
	snprintf(ws_name, length + 1, "%s", workspace->name);

	if (snprintf(ws_number, sizeof(ws_number), "%d", vscr->workspace.current + 1) == sizeof(ws_number))
		snprintf(ws_number, sizeof(ws_number), "-");

	nlength = strlen(ws_number);

	if (wPreferences.flags.noclip || !workspace->clip->collapsed)
		color = scr->clip_title_color[CLIP_NORMAL];
	else
		color = scr->clip_title_color[CLIP_COLLAPSED];

	ty = ICON_SIZE - WMFontHeight(scr->clip_title_font) - 3;

	tx = CLIP_BUTTON_SIZE * ICON_SIZE / 64;

	if(wPreferences.show_clip_title)
		WMDrawString(scr->wmscreen, win, color, scr->clip_title_font, tx, ty, ws_name, length);

	tx = (ICON_SIZE / 2 - WMWidthOfString(scr->clip_title_font, ws_number, nlength)) / 2;

	WMDrawString(scr->wmscreen, win, color, scr->clip_title_font, tx, 2, ws_number, nlength);

	wfree(ws_name);

	if (aicon->launching)
		XFillRectangle(dpy, aicon->icon->core->window, scr->stipple_gc,
			       0, 0, wPreferences.icon_size, wPreferences.icon_size);

	paintClipButtons(aicon, aicon->dock->lclip_button_pushed, aicon->dock->rclip_button_pushed);
}

/* Create appicon's icon */
WAppIcon *clip_icon_create(virtual_screen *vscr)
{
	WAppIcon *btn;

	btn = dock_icon_create(vscr, NULL, "WMClip", "Logo");

	btn->icon->tile_type = TILE_CLIP;

	btn->xindex = 0;
	btn->yindex = 0;
	btn->x_pos = 0;
	btn->y_pos = 0;
	btn->docked = 1;

	return btn;
}

void clip_icon_map(virtual_screen *vscr)
{
	WIcon *icon = vscr->clip.icon->icon;
	WCoreWindow *wcore = icon->core;
	WScreen *scr = vscr->screen_ptr;

	wcore_map_toplevel(wcore, vscr, 0, 0, icon->width, icon->height, 0,
			   scr->w_depth, scr->w_visual,
			   scr->w_colormap, scr->white_pixel);

	map_icon_image(icon);

	WMAddNotificationObserver(icon_appearanceObserver, icon, WNIconAppearanceSettingsChanged, icon);
	WMAddNotificationObserver(icon_tileObserver, icon, WNIconTileSettingsChanged, icon);

#ifdef USE_DOCK_XDND
	wXDNDMakeAwareness(wcore->window);
#endif

	AddToStackList(vscr, wcore);

	wcore->descriptor.handle_expose = clip_icon_expose;
	wcore->descriptor.handle_mousedown = clip_icon_mouse_down;
	wcore->descriptor.handle_enternotify = clip_enter_notify;
	wcore->descriptor.handle_leavenotify = clip_leave_notify;
	wcore->descriptor.parent_type = WCLASS_DOCK_ICON;
	wcore->descriptor.parent = vscr->clip.icon;
	vscr->clip.mapped = 1;

	XMapWindow(dpy, wcore->window);
}

void clip_icon_unmap(virtual_screen *vscr)
{
	vscr->clip.mapped = 0;

	XUnmapWindow(dpy, vscr->clip.icon->icon->core->window);
	RemoveFromStackList(vscr, vscr->clip.icon->icon->core);
	unmap_icon_image(vscr->clip.icon->icon);
	wcore_unmap(vscr->clip.icon->icon->core);
}

void clip_map(WDock *dock, WMPropList *state)
{
	virtual_screen *vscr = dock->vscr;
	WAppIcon *btn = vscr->clip.icon;

	wRaiseFrame(btn->icon->vscr, btn->icon->core);
	XMoveWindow(dpy, btn->icon->core->window, btn->x_pos, btn->y_pos);

	if (!state)
		return;

	WMRetainPropList(state);

	/* restore position */
	restore_clip_position_map(dock);

	/* application list */
	clip_set_attacheddocks(dock, state);

	WMReleasePropList(state);
}

void clip_unmap(WDock *dock)
{
	dock_unset_attacheddocks(dock);
}

void clip_destroy(WDock *dock)
{
	int i, keepit;
	WAppIcon *aicon;
	WCoord *coord;

	for (i = 1; i < dock->max_icons; i++) {
		aicon = dock->icon_array[i];
		if (aicon) {
			keepit = aicon->running && wApplicationOf(aicon->main_window);
			wDockDetach(dock, aicon);
			if (keepit) {
				coord = PlaceIcon(dock->vscr, wGetHeadForWindow(aicon->icon->owner));
				aicon->x_pos = coord->x;
				aicon->y_pos = coord->y;
				wfree(coord);
				XMoveWindow(dpy, aicon->icon->core->window, aicon->x_pos, aicon->y_pos);

				if (!dock->mapped || dock->collapsed)
					XMapWindow(dpy, aicon->icon->core->window);
			}
		}
	}

	if (wPreferences.auto_arrange_icons)
		wArrangeIcons(dock->vscr, True);

	wfree(dock->icon_array);

	if (dock->vscr->last_dock == dock)
		dock->vscr->last_dock = NULL;

	wfree(dock);
}

static WMPropList *clip_save_state(WDock *dock)
{
	virtual_screen *vscr = dock->vscr;
	int i;
	WMPropList *icon_info;
	WMPropList *list = NULL, *dock_state = NULL;
	WMPropList *value, *dYes, *dNo, *dPosition, *dApplications;
	WMPropList *dLowered, *dCollapsed, *dAutoCollapse, *dAutoRaiseLower;
	WMPropList *dAutoAttractIcons;
	char buffer[256];

	list = WMCreatePLArray(NULL);

	for (i = 1; i < dock->max_icons; i++) {
		WAppIcon *btn = dock->icon_array[i];

		if (!btn || btn->attracted)
			continue;

		icon_info = make_icon_state(dock->icon_array[i]);
		if (icon_info != NULL) {
			WMAddToPLArray(list, icon_info);
			WMReleasePropList(icon_info);
		}
	}

	dApplications = WMCreatePLString("Applications");
	dock_state = WMCreatePLDictionary(dApplications, list, NULL);
	WMReleasePropList(list);

	dYes = WMRetainPropList(WMCreatePLString("Yes"));
	dNo = WMRetainPropList(WMCreatePLString("No"));
	value = (dock->collapsed ? dYes : dNo);
	dCollapsed = WMCreatePLString("Collapsed");
	WMPutInPLDictionary(dock_state, dCollapsed, value);

	value = (dock->auto_collapse ? dYes : dNo);
	dAutoCollapse = WMCreatePLString("AutoCollapse");
	WMPutInPLDictionary(dock_state, dAutoCollapse, value);

	value = (dock->attract_icons ? dYes : dNo);
	dAutoAttractIcons = WMCreatePLString("AutoAttractIcons");
	WMPutInPLDictionary(dock_state, dAutoAttractIcons, value);

	value = (dock->lowered ? dYes : dNo);
	dLowered = WMCreatePLString("Lowered");
	WMPutInPLDictionary(dock_state, dLowered, value);

	value = (dock->auto_raise_lower ? dYes : dNo);
	dAutoRaiseLower = WMCreatePLString("AutoRaiseLower");
	WMPutInPLDictionary(dock_state, dAutoRaiseLower, value);

	/* TODO: Check why in the last workspace, clip is at x=0, y=0
	 * Save the Clip position using the Clip in workspace 1
	 */
	snprintf(buffer, sizeof(buffer), "%i,%i",
		 vscr->workspace.array[0]->clip->x_pos,
		 vscr->workspace.array[0]->clip->y_pos);
	value = WMCreatePLString(buffer);
	dPosition = WMCreatePLString("Position");
	WMPutInPLDictionary(dock_state, dPosition, value);
	WMReleasePropList(value);

	return dock_state;
}


WMPropList *wClipSaveWorkspaceState(virtual_screen *vscr, int workspace)
{
	return clip_save_state(vscr->workspace.array[workspace]->clip);
}

static WAppIcon *restore_clip_icon_state(virtual_screen *vscr, WMPropList *info, int index)
{
	WAppIcon *aicon;
	WMPropList *cmd, *value, *dCommand, *dPasteCommand, *dLock, *dAutoLaunch;
	WMPropList *dName, *dForced, *dBuggyApplication, *dPosition, *dOmnipresent;
	char *wclass, *winstance, *command;

	dCommand = WMRetainPropList(WMCreatePLString("Command"));
	cmd = WMGetFromPLDictionary(info, dCommand);
	if (!cmd || !WMIsPLString(cmd))
		return NULL;

	/* parse window name */
	dName = WMRetainPropList(WMCreatePLString("Name"));
	value = WMGetFromPLDictionary(info, dName);
	if (!value)
		return NULL;

	ParseWindowName(value, &winstance, &wclass, "dock");

	if (!winstance && !wclass)
		return NULL;

	/* get commands */
	command = wstrdup(WMGetFromPLString(cmd));
	if (strcmp(command, "-") == 0) {
		wfree(command);

		if (wclass)
			wfree(wclass);

		if (winstance)
			wfree(winstance);

		return NULL;
	}

	/* Create appicon's icon */
	aicon = create_appicon(vscr, command, wclass, winstance);

	if (wclass)
		wfree(wclass);

	if (winstance)
		wfree(winstance);

	wfree(command);

	aicon->icon->core->descriptor.handle_expose = dock_icon_expose;
	aicon->icon->core->descriptor.handle_mousedown = clip_icon_mouse_down;
	aicon->icon->core->descriptor.handle_enternotify = clip_enter_notify;
	aicon->icon->core->descriptor.handle_leavenotify = clip_leave_notify;
	aicon->icon->core->descriptor.parent_type = WCLASS_DOCK_ICON;
	aicon->icon->core->descriptor.parent = aicon;

#ifdef USE_DOCK_XDND			/* was OFFIX */
	WMPropList *dDropCommand;
	dDropCommand = WMRetainPropList(WMCreatePLString("DropCommand"));
	cmd = WMGetFromPLDictionary(info, dDropCommand);
	if (cmd)
		aicon->dnd_command = wstrdup(WMGetFromPLString(cmd));
#endif

	dPasteCommand = WMRetainPropList(WMCreatePLString("PasteCommand"));
	cmd = WMGetFromPLDictionary(info, dPasteCommand);
	if (cmd)
		aicon->paste_command = wstrdup(WMGetFromPLString(cmd));

	/* check auto launch */
	dAutoLaunch = WMRetainPropList(WMCreatePLString("AutoLaunch"));
	value = WMGetFromPLDictionary(info, dAutoLaunch);
	aicon->auto_launch = getBooleanDockValue(value, dAutoLaunch);

	/* check lock */
	dLock = WMRetainPropList(WMCreatePLString("Lock"));
	value = WMGetFromPLDictionary(info, dLock);
	aicon->lock = getBooleanDockValue(value, dLock);

	/* check if it wasn't normally docked */
	dForced = WMRetainPropList(WMCreatePLString("Forced"));
	value = WMGetFromPLDictionary(info, dForced);
	aicon->forced_dock = getBooleanDockValue(value, dForced);

	/* check if we can rely on the stuff in the app */
	dBuggyApplication = WMRetainPropList(WMCreatePLString("BuggyApplication"));
	value = WMGetFromPLDictionary(info, dBuggyApplication);
	aicon->buggy_app = getBooleanDockValue(value, dBuggyApplication);

	/* get position in the dock */
	dPosition = WMCreatePLString("Position");
	value = WMGetFromPLDictionary(info, dPosition);
	if (value && WMIsPLString(value)) {
		if (sscanf(WMGetFromPLString(value), "%hi,%hi", &aicon->xindex, &aicon->yindex) != 2)
			wwarning(_("bad value in docked icon state info %s"), WMGetFromPLString(dPosition));
	} else {
		aicon->yindex = index;
		aicon->xindex = 0;
	}

	/* check if icon is omnipresent */
	dOmnipresent = WMCreatePLString("Omnipresent");
	value = WMGetFromPLDictionary(info, dOmnipresent);

	aicon->omnipresent = getBooleanDockValue(value, dOmnipresent);
	aicon->running = 0;
	aicon->docked = 1;

	return aicon;
}

static void clip_set_attacheddocks(WDock *dock, WMPropList *state)
{
	virtual_screen *vscr = dock->vscr;
	char screen_id[64];
	WMPropList *apps;
	WAppIcon *old_top;

	old_top = dock->icon_array[0];

	snprintf(screen_id, sizeof(screen_id), "%ix%i", vscr->screen_ptr->scr_width, vscr->screen_ptr->scr_height);
	apps = get_application_list(state, vscr);
	if (!apps)
		return;

	if (clip_set_attacheddocks_do(dock, apps))
		return;

	set_attacheddocks_map(dock);

	/* if the first icon is not defined, use the default */
	if (dock->icon_array[0] == NULL) {
		/* update default icon */
		old_top->x_pos = dock->x_pos;
		old_top->y_pos = dock->y_pos;
		if (dock->lowered)
			ChangeStackingLevel(old_top->icon->vscr, old_top->icon->core, WMNormalLevel);
		else
			ChangeStackingLevel(old_top->icon->vscr, old_top->icon->core, WMDockLevel);

		dock->icon_array[0] = old_top;
		XMoveWindow(dpy, old_top->icon->core->window, dock->x_pos, dock->y_pos);
		/* we don't need to increment dock->icon_count here because it was
		 * incremented in the loop above.
		 */
	} else if (old_top != dock->icon_array[0]) {
		if (old_top == vscr->clip.icon) /* TODO dande: understand the logic */
			vscr->clip.icon = dock->icon_array[0];

		wAppIconDestroy(old_top);
	}
}

void clip_autolaunch(int vscrno)
{
	int i;

	/* auto-launch apps in clip */
	if (!wPreferences.flags.noclip) {
		for (i = 0; i < w_global.vscreens[vscrno]->workspace.count; i++) {
			if (w_global.vscreens[vscrno]->workspace.array[i]->clip) {
				w_global.vscreens[vscrno]->last_dock = w_global.vscreens[vscrno]->workspace.array[i]->clip;
				wDockDoAutoLaunch(w_global.vscreens[vscrno]->workspace.array[i]->clip, i);
			}
		}
	}
}

void restore_clip_position(WDock *dock, WMPropList *state)
{
	virtual_screen *vscr = dock->vscr;
	WMPropList *value, *dPosition;

	if (!state) {
		/* If no state is a new workspace+clip,
		 * copy from clip at workspace 0
		 */
		if (vscr->workspace.array[0]->clip) {
			dock->x_pos = vscr->workspace.array[0]->clip->x_pos;
			dock->y_pos = vscr->workspace.array[0]->clip->y_pos;
			vscr->clip.icon->x_pos = dock->x_pos;
			vscr->clip.icon->y_pos = dock->y_pos;
		}

		return;
	}

	dPosition = WMCreatePLString("Position");
	value = WMGetFromPLDictionary(state, dPosition);
	if (!value)
		return;

	if (!WMIsPLString(value)) {
		wwarning(_("Bad value in clip state info: Position"));
		return;
	}

	if (sscanf(WMGetFromPLString(value), "%i,%i", &dock->x_pos, &dock->y_pos) != 2)
		wwarning(_("Bad value in clip state info: Position"));

	/* Copy the dock coords in the appicon coords */
	vscr->clip.icon->x_pos = dock->x_pos;
	vscr->clip.icon->y_pos = dock->y_pos;
}

static void restore_clip_position_map(WDock *dock)
{
	/* check position sanity */
	if (!onScreen(dock->vscr, dock->x_pos, dock->y_pos)) {
		int x = dock->x_pos;
		wScreenKeepInside(dock->vscr, &x, &dock->y_pos, ICON_SIZE, ICON_SIZE);
	}

	/* Is this needed any more? */
	if (dock->x_pos < 0) {
		dock->x_pos = 0;
	} else if (dock->x_pos > dock->vscr->screen_ptr->scr_width - ICON_SIZE) {
		dock->x_pos = dock->vscr->screen_ptr->scr_width - ICON_SIZE;
	}

	/* Copy the dock coords in the appicon coords */
	dock->vscr->clip.icon->x_pos = dock->x_pos;
	dock->vscr->clip.icon->y_pos = dock->y_pos;
}

static int clip_set_attacheddocks_do(WDock *dock, WMPropList *apps)
{
	virtual_screen *vscr = dock->vscr;
	int count, i;
	WMPropList *value;
	WAppIcon *aicon;

	count = WMGetPropListItemCount(apps);
	if (count == 0)
		return 1;

	for (i = 0; i < count; i++) {
		if (dock->icon_count >= dock->max_icons) {
			wwarning(_("there are too many icons stored in dock. Ignoring what doesn't fit"));
			break;
		}

		value = WMGetFromPLArray(apps, i);
		aicon = restore_clip_icon_state(vscr, value, dock->icon_count);
		dock->icon_array[dock->icon_count] = aicon;

		if (aicon) {
			aicon->dock = dock;
			aicon->x_pos = dock->x_pos + (aicon->xindex * ICON_SIZE);
			aicon->y_pos = dock->y_pos + (aicon->yindex * ICON_SIZE);
			dock->icon_count++;
		}
	}

	return 0;
}

Bool clip_attach_icon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon)
{
	WWindow *wwin;
	Bool lupdate_icon = False;
	char *command = NULL;
	int index;

	icon->editing = 0;

	if (update_icon)
		lupdate_icon = True;

	if (icon->command == NULL) {
		/* If icon->owner exists, it means the application is running */
		if (icon->icon->owner) {
			wwin = icon->icon->owner;
			command = GetCommandForWindow(wwin->client_win);
		}

		if (command) {
			icon->command = command;
		} else {
			if (!icon->attracted) {
				icon->editing = 1;
				if (wInputDialog(dock->vscr, _("Dock Icon"),
						 _("Type the command used to launch the application"), &command)) {

					if (command && (command[0] == 0 || (command[0] == '-' && command[1] == 0))) {
						wfree(command);
						command = NULL;
					}

					icon->command = command;
					icon->editing = 0;
				} else {
					icon->editing = 0;
					if (command)
						wfree(command);
					/* If the target is the clip, make it an attracted icon */
					icon->attracted = 1;
					if (!icon->icon->shadowed) {
						icon->icon->shadowed = 1;
						lupdate_icon = True;
					}
				}
			}
		}
	}

	for (index = 1; index < dock->max_icons; index++)
		if (dock->icon_array[index] == NULL)
			break;

	dock->icon_array[index] = icon;
	icon->yindex = y;
	icon->xindex = x;

	icon->omnipresent = 0;

	icon->x_pos = dock->x_pos + x * ICON_SIZE;
	icon->y_pos = dock->y_pos + y * ICON_SIZE;

	dock->icon_count++;

	icon->running = 1;
	icon->launching = 0;
	icon->docked = 1;
	icon->dock = dock;
	icon->icon->core->descriptor.handle_mousedown = clip_icon_mouse_down;
	icon->icon->core->descriptor.handle_enternotify = clip_enter_notify;
	icon->icon->core->descriptor.handle_leavenotify = clip_leave_notify;
	icon->icon->core->descriptor.parent_type = WCLASS_DOCK_ICON;
	icon->icon->core->descriptor.parent = icon;

	MoveInStackListUnder(icon->icon->vscr, dock->icon_array[index - 1]->icon->core, icon->icon->core);
	wAppIconMove(icon, icon->x_pos, icon->y_pos);

	/*
	 * Update icon pixmap, RImage doesn't change,
	 * so call wIconUpdate is not needed
	 */
	if (lupdate_icon)
		update_icon_pixmap(icon->icon);

	/* Paint it */
	wAppIconPaint(icon);

	/* Save it */
	save_appicon(icon);

	if (wPreferences.auto_arrange_icons)
		wArrangeIcons(dock->vscr, True);

#ifdef USE_DOCK_XDND			/* was OFFIX */
	if (icon->command && !icon->dnd_command) {
		int len = strlen(icon->command) + 8;
		icon->dnd_command = wmalloc(len);
		snprintf(icon->dnd_command, len, "%s %%d", icon->command);
	}
#endif

	if (icon->command && !icon->paste_command) {
		int len = strlen(icon->command) + 8;
		icon->paste_command = wmalloc(len);
		snprintf(icon->paste_command, len, "%s %%s", icon->command);
	}

	return True;
}

Bool clip_snap_icon(WDock *dock, WAppIcon *icon, int req_x, int req_y, int *ret_x, int *ret_y, int redocking)
{
	virtual_screen *vscr = dock->vscr;
	int dx, dy;
	int ex_x, ex_y;
	int i, offset = ICON_SIZE / 2;
	WAppIcon *aicon = NULL;
	WAppIcon *nicon = NULL;

	if (wPreferences.flags.noupdates)
		return False;

	dx = dock->x_pos;
	dy = dock->y_pos;

	/* if the dock is full */
	if (!redocking && (dock->icon_count >= dock->max_icons))
		return False;

	/* exact position */
	if (req_y < dy)
		ex_y = (req_y - offset - dy) / ICON_SIZE;
	else
		ex_y = (req_y + offset - dy) / ICON_SIZE;

	if (req_x < dx)
		ex_x = (req_x - offset - dx) / ICON_SIZE;
	else
		ex_x = (req_x + offset - dx) / ICON_SIZE;

	/* check if the icon is outside the screen boundaries */
	if (!onScreen(vscr, dx + ex_x * ICON_SIZE, dy + ex_y * ICON_SIZE))
		return False;

	int start, stop, k, neighbours = 0;

	start = icon->omnipresent ? 0 : vscr->workspace.current;
	stop = icon->omnipresent ? vscr->workspace.count : start + 1;

	aicon = NULL;
	for (k = start; k < stop; k++) {
		WDock *tmp = vscr->workspace.array[k]->clip;
		if (!tmp)
			continue;

		for (i = 0; i < tmp->max_icons; i++) {
			nicon = tmp->icon_array[i];
			if (nicon && nicon->xindex == ex_x && nicon->yindex == ex_y) {
				aicon = nicon;
				break;
			}
		}

		if (aicon)
			break;
	}

	for (k = start; k < stop; k++) {
		WDock *tmp = vscr->workspace.array[k]->clip;
		if (!tmp)
			continue;

		for (i = 0; i < tmp->max_icons; i++) {
			nicon = tmp->icon_array[i];
			if (nicon && nicon != icon &&	/* Icon can't be it's own neighbour */
			    (abs(nicon->xindex - ex_x) <= CLIP_ATTACH_VICINITY &&
			     abs(nicon->yindex - ex_y) <= CLIP_ATTACH_VICINITY)) {
				neighbours = 1;
				break;
			}
		}

		if (neighbours)
			break;
	}

	if (neighbours && (aicon == NULL || (redocking && aicon == icon))) {
		*ret_x = ex_x;
		*ret_y = ex_y;
		return True;
	}

	return False;
}

int getClipButton(int px, int py)
{
	int pt = (CLIP_BUTTON_SIZE + 2) * ICON_SIZE / 64;

	if (px < 0 || py < 0 || px >= ICON_SIZE || py >= ICON_SIZE)
		return CLIP_IDLE;

	if (py <= pt - ((int)ICON_SIZE - 1 - px))
		return CLIP_FORWARD;
	else if (px <= pt - ((int)ICON_SIZE - 1 - py))
		return CLIP_REWIND;

	return CLIP_IDLE;
}

void handleClipChangeWorkspace(virtual_screen *vscr, XEvent *event)
{
	XEvent ev;
	int done, direction, new_ws;
	int new_dir;
	WDock *clip = vscr->clip.icon->dock;

	direction = getClipButton(event->xbutton.x, event->xbutton.y);

	clip->lclip_button_pushed = direction == CLIP_REWIND;
	clip->rclip_button_pushed = direction == CLIP_FORWARD;

	wClipIconPaint(vscr->clip.icon);
	done = 0;
	while (!done) {
		WMMaskEvent(dpy, ExposureMask | ButtonMotionMask | ButtonReleaseMask | ButtonPressMask, &ev);
		switch (ev.type) {
		case Expose:
			WMHandleEvent(&ev);
			break;

		case MotionNotify:
			new_dir = getClipButton(ev.xmotion.x, ev.xmotion.y);
			if (new_dir != direction) {
				direction = new_dir;
				clip->lclip_button_pushed = direction == CLIP_REWIND;
				clip->rclip_button_pushed = direction == CLIP_FORWARD;
				wClipIconPaint(vscr->clip.icon);
			}
			break;

		case ButtonPress:
			break;

		case ButtonRelease:
			if (ev.xbutton.button == event->xbutton.button)
				done = 1;
		}
	}

	clip->lclip_button_pushed = 0;
	clip->rclip_button_pushed = 0;

	new_ws = wPreferences.ws_advance || (event->xbutton.state & ControlMask);

	if (direction == CLIP_FORWARD) {
		if (vscr->workspace.current < vscr->workspace.count - 1)
			wWorkspaceChange(vscr, vscr->workspace.current + 1);
		else if (new_ws && vscr->workspace.current < MAX_WORKSPACES - 1)
			wWorkspaceChange(vscr, vscr->workspace.current + 1);
		else if (wPreferences.ws_cycle)
			wWorkspaceChange(vscr, 0);
	} else if (direction == CLIP_REWIND) {
		if (vscr->workspace.current > 0)
			wWorkspaceChange(vscr, vscr->workspace.current - 1);
		else if (vscr->workspace.current == 0 && wPreferences.ws_cycle)
			wWorkspaceChange(vscr, vscr->workspace.count - 1);
	}

	wClipIconPaint(vscr->clip.icon);
}

void clip_enter_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;
	WDock *dock, *tmp;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	if (desc->parent_type != WCLASS_DOCK_ICON)
		return;

	dock = btn->dock;
	if (dock == NULL)
		return;

	/* The auto raise/lower code */
	tmp = dock;
	if (tmp->auto_lower_magic) {
		WMDeleteTimerHandler(tmp->auto_lower_magic);
		tmp->auto_lower_magic = NULL;
	}

	if (tmp->auto_raise_lower && !tmp->auto_raise_magic)
		tmp->auto_raise_magic = WMAddTimerHandler(wPreferences.clip_auto_raise_delay, clipAutoRaise, (void *) tmp);

	/* The auto expand/collapse code */
	if (dock->auto_collapse_magic) {
		WMDeleteTimerHandler(dock->auto_collapse_magic);
		dock->auto_collapse_magic = NULL;
	}

	if (dock->auto_collapse && !dock->auto_expand_magic)
		dock->auto_expand_magic = WMAddTimerHandler(wPreferences.clip_auto_expand_delay, clipAutoExpand, (void *)dock);
}

void clip_icon_expose(WObjDescriptor *desc, XEvent *event)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	wClipIconPaint(desc->parent);
}


void clip_leave_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	if (desc->parent_type != WCLASS_DOCK_ICON)
		return;

	clip_leave(btn->dock);
}

static void clip_autocollapse(void *cdata)
{
	WDock *dock = (WDock *) cdata;

	if (dock->auto_collapse) {
		dock->collapsed = 1;
		wDockHideIcons(dock);
	}

	dock->auto_collapse_magic = NULL;
}

void clipAutoExpand(void *cdata)
{
	WDock *dock = (WDock *) cdata;

	if (dock->type != WM_CLIP && dock->type != WM_DRAWER)
		return;

	if (dock->auto_collapse) {
		dock->collapsed = 0;
		wDockShowIcons(dock);
	}
	dock->auto_expand_magic = NULL;
}

void clipAutoLower(void *cdata)
{
	WDock *dock = (WDock *) cdata;

	if (dock->auto_raise_lower)
		wDockLower(dock);

	dock->auto_lower_magic = NULL;
}

void clipAutoRaise(void *cdata)
{
	WDock *dock = (WDock *) cdata;

	if (dock->auto_raise_lower)
		wDockRaise(dock);

	dock->auto_raise_magic = NULL;
}

static Bool iconCanBeOmnipresent(WAppIcon *aicon)
{
	virtual_screen *vscr = aicon->icon->vscr;
	WDock *clip;
	WAppIcon *btn;
	int i, j;

	for (i = 0; i < vscr->workspace.count; i++) {
		clip = vscr->workspace.array[i]->clip;

		if (clip == aicon->dock)
			continue;

		if (clip->icon_count + vscr->global_icon_count >= clip->max_icons)
			return False;	/* Clip is full in some workspace */

		for (j = 0; j < clip->max_icons; j++) {
			btn = clip->icon_array[j];
			if (btn && btn->xindex == aicon->xindex && btn->yindex == aicon->yindex)
				return False;
		}
	}

	return True;
}

int wClipMakeIconOmnipresent(WAppIcon *aicon, int omnipresent)
{
	virtual_screen *vscr = aicon->icon->vscr;
	WAppIconChain *new_entry, *tmp, *tmp1;
	int status = WO_SUCCESS;

	if ((vscr->dock.dock && aicon->dock == vscr->dock.dock) || aicon == vscr->clip.icon)
		return WO_NOT_APPLICABLE;

	if (aicon->omnipresent == omnipresent)
		return WO_SUCCESS;

	if (omnipresent) {
		if (iconCanBeOmnipresent(aicon)) {
			aicon->omnipresent = 1;
			new_entry = wmalloc(sizeof(WAppIconChain));
			new_entry->aicon = aicon;
			new_entry->next = vscr->clip.global_icons;
			vscr->clip.global_icons = new_entry;
			vscr->global_icon_count++;
		} else {
			aicon->omnipresent = 0;
			status = WO_FAILED;
		}
	} else {
		aicon->omnipresent = 0;
		if (aicon == vscr->clip.global_icons->aicon) {
			tmp = vscr->clip.global_icons->next;
			wfree(vscr->clip.global_icons);
			vscr->clip.global_icons = tmp;
			vscr->global_icon_count--;
		} else {
			tmp = vscr->clip.global_icons;
			while (tmp->next) {
				if (tmp->next->aicon == aicon) {
					tmp1 = tmp->next->next;
					wfree(tmp->next);
					tmp->next = tmp1;
					vscr->global_icon_count--;
					break;
				}
				tmp = tmp->next;
			}
		}
	}

	return status;
}

void clip_leave(WDock *dock)
{
	XEvent event;
	WObjDescriptor *desc = NULL;
	WDock *tmp;

	if (dock == NULL)
		return;

	if (XCheckTypedEvent(dpy, EnterNotify, &event) != False) {
		if (XFindContext(dpy, event.xcrossing.window, w_global.context.client_win,
				 (XPointer *) & desc) != XCNOENT
		    && desc && desc->parent_type == WCLASS_DOCK_ICON
		    && ((WAppIcon *) desc->parent)->dock == dock) {
			/* We haven't left the dock/clip/drawer yet */
			XPutBackEvent(dpy, &event);
			return;
		}

		XPutBackEvent(dpy, &event);
	} else {
		/* We entered a withdrawn window, so we're still in Clip */
		return;
	}

	tmp = dock;
	if (tmp->auto_raise_magic) {
		WMDeleteTimerHandler(tmp->auto_raise_magic);
		tmp->auto_raise_magic = NULL;
	}

	if (tmp->auto_raise_lower && !tmp->auto_lower_magic)
		tmp->auto_lower_magic = WMAddTimerHandler(wPreferences.clip_auto_lower_delay, clipAutoLower, (void *)tmp);

	if (dock->auto_expand_magic) {
		WMDeleteTimerHandler(dock->auto_expand_magic);
		dock->auto_expand_magic = NULL;
	}

	if (dock->auto_collapse && !dock->auto_collapse_magic)
		dock->auto_collapse_magic = WMAddTimerHandler(wPreferences.clip_auto_collapse_delay, clip_autocollapse, (void *)dock);
}

void wClipUpdateForWorkspaceChange(virtual_screen *vscr, int workspace)
{
	WDock *old_clip;
	WAppIconChain *chain;

	if (wPreferences.flags.noclip)
		return;

	vscr->clip.icon->dock = vscr->workspace.array[workspace]->clip;
	if (vscr->workspace.current != workspace) {
		old_clip = vscr->workspace.array[vscr->workspace.current]->clip;
		chain = vscr->clip.global_icons;

		while (chain) {
			wDockMoveIconBetweenDocks(chain->aicon->dock,
					     vscr->workspace.array[workspace]->clip,
					     chain->aicon, chain->aicon->xindex, chain->aicon->yindex);

			if (vscr->workspace.array[workspace]->clip->collapsed)
				XUnmapWindow(dpy, chain->aicon->icon->core->window);

			chain = chain->next;
		}

		wDockHideIcons(old_clip);
		if (old_clip->auto_raise_lower) {
			if (old_clip->auto_raise_magic) {
				WMDeleteTimerHandler(old_clip->auto_raise_magic);
				old_clip->auto_raise_magic = NULL;
			}

			wDockLower(old_clip);
		}

		if (old_clip->auto_collapse) {
			if (old_clip->auto_expand_magic) {
				WMDeleteTimerHandler(old_clip->auto_expand_magic);
				old_clip->auto_expand_magic = NULL;
			}

			old_clip->collapsed = 1;
		}

		wDockShowIcons(vscr->workspace.array[workspace]->clip);
	}
}
