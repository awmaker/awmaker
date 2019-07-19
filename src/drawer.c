/* dock.c- Dock module for WindowMaker - Drawer
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
	RM_DRAWEROPTSSUBMENU,
	RM_SELECT,
	RM_SELECTALL,
	RM_KEEP_ICONS,
	RM_REMOVE_ICONS,
	RM_ATTRACT,
	RM_LAUNCH,
	RM_BRING,
	RM_HIDE,
	RM_SETTINGS,
	RM_KILL
};

static void drawerRemoveFromChain(WDock *drawer);
static WAppIcon *restore_drawer_icon_state(virtual_screen *vscr, WMPropList *info, int index);
static void drawer_map(WDock *dock, virtual_screen *vscr);
static void drawer_unmap(WDock *dock);
static void drawer_menu(WDock *dock, WAppIcon *aicon, XEvent *event);

static char *findUniqueName(virtual_screen *vscr, const char *instance_basename);
static void drawer_icon_expose(WObjDescriptor *desc, XEvent *event);
static void drawerAppendToChain(WDock *drawer);
static void iconDblClick(WObjDescriptor *desc, XEvent *event);
static void drawerConsolidateIcons(WDock *drawer);
static WMPropList *drawer_save_state(WDock *dock);
static void drawerIconExpose(WObjDescriptor *desc, XEvent *event);
static WMenu *drawer_make_options_menu(virtual_screen *vscr);
static void drawer_remove_icons_callback(WMenu *menu, WMenuEntry *entry);
static void drawerRestoreState_map(WDock *drawer);
static int drawer_set_attacheddocks_do(WDock *dock, WMPropList *apps);
static void drawer_autocollapse(void *cdata);
static void restore_drawer_position(WDock *drawer, WMPropList *state);

WDock *drawer_create(virtual_screen *vscr, const char *name)
{
	WDock *dock;
	WAppIcon *btn;

	dock = dock_create_core(vscr);

	/* Set basic variables */
	dock->type = WM_DRAWER;
	dock->auto_collapse = 1;

	if (!name)
		name = findUniqueName(vscr, "Drawer");

	btn = dock_icon_create(vscr, NULL, "WMDrawer", (char *) name);

	/* Create appicon's icon */
	btn->xindex = 0;
	btn->yindex = 0;
	btn->docked = 1;
	btn->dock = dock;
	dock->on_right_side = vscr->dock.dock->on_right_side;
	dock->icon_array[0] = btn;

	btn->icon->core->descriptor.parent_type = WCLASS_DOCK_ICON;
	btn->icon->core->descriptor.parent = btn;
	btn->icon->tile_type = TILE_DRAWER;
	dock->menu = NULL;
	drawerAppendToChain(dock);

	return dock;
}

/* Don't free the returned string. Duplicate it. */
static char *findUniqueName(virtual_screen *vscr, const char *instance_basename)
{
	static char buffer[128];
	WDrawerChain *dc;
	int i;
	Bool already_in_use = True;

#define UNIQUE_NAME_WATCHDOG 128
	for (i = 0; already_in_use && i < UNIQUE_NAME_WATCHDOG; i++) {
		snprintf(buffer, sizeof buffer, "%s%d", instance_basename, i);

		already_in_use = False;

		for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next) {
			if (!strncmp(dc->adrawer->icon_array[0]->wm_instance, buffer,
					sizeof buffer)) {
				already_in_use = True;
				break;
			}
		}
	}

	if (i == UNIQUE_NAME_WATCHDOG)
		wwarning("Couldn't find a unique name for drawer in %d attempts.", i);
#undef UNIQUE_NAME_WATCHDOG

	return buffer;
}

static void drawerAppendToChain(WDock *drawer)
{
	virtual_screen *vscr = drawer->vscr;
	WDrawerChain **where_to_add;

	where_to_add = &vscr->drawer.drawers;
	while ((*where_to_add) != NULL)
		where_to_add = &(*where_to_add)->next;

	*where_to_add = wmalloc(sizeof(WDrawerChain));
	(*where_to_add)->adrawer = drawer;
	(*where_to_add)->next = NULL;
	vscr->drawer.drawer_count++;
}

void drawer_icon_mouse_down(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *aicon = desc->parent;
	WDock *dock = aicon->dock;
	virtual_screen *vscr = aicon->icon->vscr;
	WAppIcon *btn;

	if (aicon->editing || WCHECK_STATE(WSTATE_MODAL))
		return;

	vscr->last_dock = dock;

	if (dock->menu && dock->menu->flags.mapped)
		wMenuUnmap(dock->menu);

	if (IsDoubleClick(vscr, event)) {
		/* double-click was not in the main clip icon */
		iconDblClick(desc, event);
		return;
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
			handleDockMove(dock, aicon, event);
		} else {
			Bool hasMoved = wHandleAppIconMove(aicon, event);
			if (wPreferences.single_click && !hasMoved)
				iconDblClick(desc, event);
		}
		break;
	case Button2:
		btn = desc->parent;

		if (!btn->launching && (!btn->running || (event->xbutton.state & ControlMask)))
			launchDockedApplication(btn, True);

		break;
	case Button3:
		if (event->xbutton.send_event &&
		    XGrabPointer(dpy, aicon->icon->core->window, True, ButtonMotionMask
				 | ButtonReleaseMask | ButtonPressMask, GrabModeAsync,
				 GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
			wwarning("pointer grab failed for dockicon menu");
			return;
		}

		drawer_menu(dock, aicon, event);
		break;
	case Button4:
		break;
	case Button5:
		break;
	}
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
			} else if (wIsADrawer(btn)) {
				toggleCollapsed(dock);
			} else if (btn->command) {
				if (!btn->launching && (!btn->running || (event->xbutton.state & ControlMask)))
					launchDockedApplication(btn, False);
			}
		}
	}
}

static void drawer_menu(WDock *dock, WAppIcon *aicon, XEvent *event)
{
	virtual_screen *vscr = aicon->icon->vscr;
	WScreen *scr = vscr->screen_ptr;
	WMenu *opt_menu;
	WMenuEntry *entry;
	WObjDescriptor *desc;
	int n_selected, appIsRunning, x_pos;
	WApplication *wapp = NULL;

	/* Set some variables used in the menu */
	n_selected = numberOfSelectedIcons(dock);
	appIsRunning = aicon->running && aicon->icon && aicon->icon->owner;

	if (aicon->icon->owner)
		wapp = wApplicationOf(aicon->icon->owner->main_window);

	/* create dock menu */
	dock->menu = menu_create(vscr, NULL);

	/* Drawer options */
	entry = wMenuAddCallback(dock->menu, _("Drawer options"), NULL, NULL);
	opt_menu = drawer_make_options_menu(vscr);
	wMenuEntrySetCascade_create(dock->menu, entry, opt_menu);

	entry = wMenuAddCallback(dock->menu, _("Selected"), selectCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_on = 1;
	entry->flags.indicator_type = MI_CHECK;

	/* Select All Icons / Unselect All Icons */
	if (n_selected > 0)
		entry = wMenuAddCallback(dock->menu, _("Unselect All Icons"), selectIconsCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Select All Icons"), selectIconsCallback, NULL);

	/* Keep Icons / Keep Icon */
	if (n_selected > 1)
		entry = wMenuAddCallback(dock->menu, _("Keep Icons"), keepIconsCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Keep Icon"), keepIconsCallback, NULL);

	/* Remove Icons / Remove Icon */
	if (n_selected > 1)
		entry = wMenuAddCallback(dock->menu, _("Remove Icons"), drawer_remove_icons_callback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Remove Icon"), drawer_remove_icons_callback, NULL);

	wMenuAddCallback(dock->menu, _("Attract Icons"), attractIconsCallback, NULL);
	wMenuAddCallback(dock->menu, _("Launch"), launchCallback, NULL);

	/* Unhide Here / Bring Here */
	if (wapp && wapp->flags.hidden)
		wMenuAddCallback(dock->menu, _("Unhide Here"), dockUnhideHereCallback, NULL);
	else
		wMenuAddCallback(dock->menu, _("Bring Here"), dockUnhideHereCallback, NULL);

	/* Hide / Unhide Icons */
	if (wapp && wapp->flags.hidden)
		entry = wMenuAddCallback(dock->menu, _("Unhide"), dockHideCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Hide"), dockHideCallback, NULL);

	wMenuAddCallback(dock->menu, _("Settings..."), settingsCallback, NULL);

	/* Remove drawer / Kill */
	if (wIsADrawer(aicon))
		entry = wMenuAddCallback(dock->menu, _("Remove drawer"), removeDrawerCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Kill"), dockKillCallback, NULL);

	dockUpdateOptionsMenu(dock, opt_menu);

	/* select/unselect icon */
	entry = dock->menu->entries[RM_SELECT];
	entry->clientdata = aicon;
	entry->flags.indicator_on = aicon->icon->selected;
	menu_entry_set_enabled(dock->menu, RM_SELECT, aicon != vscr->clip.icon && !wIsADrawer(aicon));

	/* select/unselect all icons */
	entry = dock->menu->entries[RM_SELECTALL];
	entry->clientdata = aicon;
	menu_entry_set_enabled(dock->menu, RM_SELECTALL, dock->icon_count > 1);

	/* keep icon(s) */
	entry = dock->menu->entries[RM_KEEP_ICONS];
	entry->clientdata = aicon;
	menu_entry_set_enabled(dock->menu, RM_KEEP_ICONS, dock->icon_count > 1);

	/* remove icon(s) */
	entry = dock->menu->entries[RM_REMOVE_ICONS];
	entry->clientdata = aicon;
	menu_entry_set_enabled(dock->menu, RM_REMOVE_ICONS, dock->icon_count > 1);

	/* attract icon(s) */
	entry = dock->menu->entries[RM_ATTRACT];
	entry->clientdata = aicon;

	/* launch */
	entry = dock->menu->entries[RM_LAUNCH];
	entry->clientdata = aicon;
	menu_entry_set_enabled(dock->menu, RM_LAUNCH, aicon->command != NULL);

	/* unhide here */
	entry = dock->menu->entries[RM_BRING];
	entry->clientdata = aicon;
	menu_entry_set_enabled(dock->menu, RM_BRING, appIsRunning);

	/* hide */
	entry = dock->menu->entries[RM_HIDE];
	entry->clientdata = aicon;
	menu_entry_set_enabled(dock->menu, RM_HIDE, appIsRunning);

	/* settings */
	entry = dock->menu->entries[RM_SETTINGS];
	entry->clientdata = aicon;
	menu_entry_set_enabled(dock->menu, RM_SETTINGS, !aicon->editing && !wPreferences.flags.noupdates);

	/* kill or remove drawer */
	entry = dock->menu->entries[RM_KILL];
	entry->clientdata = aicon;
	if (wIsADrawer(aicon))
		menu_entry_set_enabled(dock->menu, RM_KILL, True);
	else
		menu_entry_set_enabled(dock->menu, RM_KILL, appIsRunning);

	menu_entry_set_enabled_paint(dock->menu, RM_SELECT);
	menu_entry_set_enabled_paint(dock->menu, RM_SELECTALL);
	menu_entry_set_enabled_paint(dock->menu, RM_KEEP_ICONS);
	menu_entry_set_enabled_paint(dock->menu, RM_REMOVE_ICONS);
	menu_entry_set_enabled_paint(dock->menu, RM_LAUNCH);
	menu_entry_set_enabled_paint(dock->menu, RM_BRING);
	menu_entry_set_enabled_paint(dock->menu, RM_HIDE);
	menu_entry_set_enabled_paint(dock->menu, RM_SETTINGS);
	menu_entry_set_enabled_paint(dock->menu, RM_KILL);

	x_pos = event->xbutton.x_root - dock->menu->frame->width / 2 - 1;
	if (x_pos < 0)
		x_pos = 0;
	else if (x_pos + dock->menu->frame->width > scr->scr_width - 2)
		x_pos = scr->scr_width - dock->menu->frame->width - 4;

	menu_map(dock->menu);
	menu_map(opt_menu);
	dock->menu->flags.realized = 0;

	dock->menu->x_pos = x_pos;
	dock->menu->y_pos = event->xbutton.y_root + 2;
	wMenuMapAt(vscr, dock->menu, False);

	/* allow drag select */
	event->xany.send_event = True;
	desc = &dock->menu->core->descriptor;
	(*desc->handle_mousedown) (desc, event);

	/* Destroy the menu */
	opt_menu->flags.realized = 0;
	dock->menu->flags.realized = 0;
	wMenuDestroy(dock->menu);
	opt_menu = NULL;
	dock->menu = NULL;
}

static WMenu *drawer_make_options_menu(virtual_screen *vscr)
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

static void drawer_remove_icons_callback(WMenu *menu, WMenuEntry *entry)
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
					_("Drawer"),
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
	drawerConsolidateIcons(dock);
}

static void drawerConsolidateIcons(WDock *drawer)
{
	WAppIcon *aicons_to_shift[drawer->icon_count];
	int maxRemaining = 0;
	int sum = 0;
	int i;
	for (i = 0; i < drawer->max_icons; i++) {
		WAppIcon *ai = drawer->icon_array[i];
		if (ai == NULL)
			continue;
		sum += abs(ai->xindex);
		if (abs(ai->xindex) > maxRemaining)
			maxRemaining = abs(ai->xindex);
	}
	while (sum != maxRemaining * (maxRemaining + 1) / 2) { // while there is a hole
		WAppIcon *ai;
		int n;
		// Look up for the hole at max position
		int maxDeleted;
		for (maxDeleted = maxRemaining - 1; maxDeleted > 0; maxDeleted--) {
			Bool foundAppIconThere = False;
			for (i = 0; i < drawer->max_icons; i++) {
				WAppIcon *ai = drawer->icon_array[i];
				if (ai == NULL)
					continue;
				if (abs(ai->xindex) == maxDeleted) {
					foundAppIconThere = True;
					break;
				}
			}
			if (!foundAppIconThere)
				break;
		}

		n = 0;
		for (i = 0; i < drawer->max_icons; i++) {
			ai = drawer->icon_array[i];
			if (ai != NULL && abs(ai->xindex) > maxDeleted)
				aicons_to_shift[n++] = ai;
		}

		wSlideAppicons(aicons_to_shift, n, !drawer->on_right_side);
		// Efficient beancounting
		maxRemaining -= 1;
		sum -= n;
	}
}

void wDrawersRestoreState(virtual_screen *vscr)
{
	WMPropList *all_drawers, *drawer_state, *dDrawers;
	int i;

	if (w_global.session_state == NULL)
		return;

	dDrawers = WMCreatePLString("Drawers");
	all_drawers = WMGetFromPLDictionary(w_global.session_state, dDrawers);
	if (!all_drawers)
		return;

	for (i = 0; i < WMGetPropListItemCount(all_drawers); i++) {
		drawer_state = WMGetFromPLArray(all_drawers, i);
		drawerRestoreState(vscr, drawer_state);
	}
}

void addADrawerCallback(WMenu *menu, WMenuEntry *entry)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	addADrawer(((WAppIcon *) entry->clientdata)->dock->vscr);
}

void drawerDestroy(WDock *drawer)
{
	virtual_screen *vscr;
	int i;
	WAppIcon *aicon = NULL;
	WMArray *icons;

	if (drawer == NULL)
		return;

	vscr = drawer->vscr;

	/* Note regarding menus: we can't delete any dock/clip/drawer menu, because
	 * that would (attempt to) wfree some memory in gettext library (see menu
	 * entries that have several "versions", such like "Hide" and "Unhide"). */
	wDefaultPurgeInfo(drawer->icon_array[0]->wm_instance,
			drawer->icon_array[0]->wm_class);

	if (drawer->icon_count == 2) {
		/* Drawer contains a single appicon: dock it where the drawer was */
		for (i = 1; i < drawer->max_icons; i++) {
			aicon = drawer->icon_array[i];
			if (aicon != NULL)
				break;
		}

		wDockMoveIconBetweenDocks(drawer, vscr->dock.dock, aicon,
					0, (drawer->y_pos - vscr->dock.dock->y_pos) / ICON_SIZE);
		XMoveWindow(dpy, aicon->icon->core->window, drawer->x_pos, drawer->y_pos);
		XMapWindow(dpy, aicon->icon->core->window);
	} else if (drawer->icon_count > 2) {
		icons = WMCreateArray(drawer->icon_count - 1);
		for (i = 1; i < drawer->max_icons; i++) {
			aicon = drawer->icon_array[i];
			if (aicon == NULL)
				continue;

			WMAddToArray(icons, aicon);
		}

		removeIcons(icons, drawer);
	}

	if (drawer->auto_collapse_magic) {
		WMDeleteTimerHandler(drawer->auto_collapse_magic);
		drawer->auto_collapse_magic = NULL;
	}

	if (drawer->auto_lower_magic) {
		WMDeleteTimerHandler(drawer->auto_lower_magic);
		drawer->auto_lower_magic = NULL;
	}

	wAppIconDestroy(drawer->icon_array[0]);
	wfree(drawer->icon_array);
	drawer->icon_array = NULL;

	drawerRemoveFromChain(drawer);
	if (vscr->last_dock == drawer)
		vscr->last_dock = NULL;

	if (vscr->drawer.attracting_drawer == drawer)
		vscr->drawer.attracting_drawer = NULL;

	wfree(drawer);
}

void wDrawersRestoreState_map(virtual_screen *vscr)
{
	WDrawerChain *dc;

	for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next)
		drawerRestoreState_map(dc->adrawer);
}

int addADrawer(virtual_screen *vscr)
{
	int i, y, sig, found_y;
	WDock *drawer, *dock = vscr->dock.dock;
	WDrawerChain *dc;
	char can_be_here[2 * dock->max_icons - 1];

	if (dock->icon_count + vscr->drawer.drawer_count >= dock->max_icons)
		return -1;

	for (y = -dock->max_icons + 1; y < dock->max_icons; y++)
		can_be_here[y + dock->max_icons - 1] = True;

	for (i = 0; i < dock->max_icons; i++)
		if (dock->icon_array[i] != NULL)
			can_be_here[dock->icon_array[i]->yindex + dock->max_icons - 1] = False;

	for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next) {
		y = (int) ((dc->adrawer->y_pos - dock->y_pos) / ICON_SIZE);
		can_be_here[y + dock->max_icons - 1] = False;
	}

	found_y = False;
	for (sig = 1; !found_y && sig > -2; sig -= 2) { /* 1, then -1 */
		for (y = sig; sig * y < dock->max_icons; y += sig) {
			if (can_be_here[y + dock->max_icons - 1] &&
				onScreen(vscr, dock->x_pos, dock->y_pos + y * ICON_SIZE)) {
				found_y = True;
				break;
			}
		}
	}

	if (!found_y)
		/* This can happen even when dock->icon_count + scr->drawer_count
		 * < dock->max_icons when the dock is not aligned on an
		 * ICON_SIZE multiple, as some space is lost above and under it */
		return -1;

	drawer = drawer_create(vscr, NULL);
	drawer->auto_raise_lower = vscr->dock.dock->auto_raise_lower;
	drawer->x_pos = dock->x_pos;
	drawer->y_pos = dock->y_pos + ICON_SIZE * y;
	drawer->icon_array[0]->xindex = 0;
	drawer->icon_array[0]->yindex = 0;
	drawer->icon_array[0]->x_pos = drawer->x_pos;
	drawer->icon_array[0]->y_pos = drawer->y_pos;

	drawerRestoreState_map(drawer);

	return 0;
}

WDock *drawerRestoreState(virtual_screen *vscr, WMPropList *drawer_state)
{
	WDock *drawer;
	WMPropList *apps, *value, *dock_state;
	WMPropList *dDock, *dPasteCommand, *dName, *dApplications;

	if (!drawer_state)
		return NULL;

	WMRetainPropList(drawer_state);

	/* Get the instance name, and create a drawer */
	dName = WMRetainPropList(WMCreatePLString("Name"));
	value = WMGetFromPLDictionary(drawer_state, dName);
	drawer = drawer_create(vscr, WMGetFromPLString(value));

	/* restore DnD command and paste command */
#ifdef USE_DOCK_XDND
	WMPropList *dDropCommand = NULL;
	dDropCommand = WMRetainPropList(WMCreatePLString("DropCommand"));
	value = WMGetFromPLDictionary(drawer_state, dDropCommand);
	if (value && WMIsPLString(value))
		drawer->icon_array[0]->dnd_command = wstrdup(WMGetFromPLString(value));
#endif /* USE_DOCK_XDND */

	dPasteCommand = WMRetainPropList(WMCreatePLString("PasteCommand"));
	value = WMGetFromPLDictionary(drawer_state, dPasteCommand);
	if (value && WMIsPLString(value))
		drawer->icon_array[0]->paste_command = wstrdup(WMGetFromPLString(value));

	/* restore position */
	restore_drawer_position(drawer, drawer_state);

	/* restore dock properties (applist and others) */
	dDock = WMCreatePLString("Dock");
	dock_state = WMGetFromPLDictionary(drawer_state, dDock);

	/* restore collapsed state */
	restore_state_collapsed(drawer, dock_state);

	/* restore auto-collapsed state */
	if (!restore_state_autocollapsed(drawer, dock_state))
		drawer->auto_collapse = 0; /* because drawer_create() sets it
					    * Probably we can change it
					    * But I am not sure yet (kix) */

	/* restore auto-raise/lower state: same as scr->dock, no matter what */
	drawer->auto_raise_lower = vscr->dock.dock->auto_raise_lower;

	/* restore attract icons state */
	if (restore_state_autoattracticons(drawer, dock_state))
		vscr->drawer.attracting_drawer = drawer;

	/* application list */
	dApplications = WMCreatePLString("Applications");
	apps = WMGetFromPLDictionary(dock_state, dApplications);
	if (apps)
		drawer_set_attacheddocks_do(drawer, apps);

	WMReleasePropList(drawer_state);

	return drawer;
}

static void drawerRestoreState_unmap(WDock *drawer)
{
	set_attacheddocks_unmap(drawer);
	drawer_unmap(drawer);
}

static void drawerRestoreState_map(WDock *drawer)
{
	virtual_screen *vscr = drawer->vscr;

	drawer_map(drawer, vscr);

	/* restore lowered/raised state: same as scr->dock, no matter what */
	drawer->lowered = vscr->dock.dock->lowered;
	if (!drawer->lowered)
		ChangeStackingLevel(drawer->icon_array[0]->icon->vscr, drawer->icon_array[0]->icon->core, WMDockLevel);
	else
		ChangeStackingLevel(drawer->icon_array[0]->icon->vscr, drawer->icon_array[0]->icon->core, WMNormalLevel);

	wRaiseFrame(drawer->icon_array[0]->icon->vscr, drawer->icon_array[0]->icon->core);

	set_attacheddocks_map(drawer);
}

/* Same kind of comment than for previous function: this function is
 * very similar to make_icon_state, but has substential differences as
 * well. */
static WMPropList *drawerSaveState(WDock *drawer)
{
	WMPropList *pstr, *drawer_state;
	WMPropList *dDock, *dPasteCommand, *dName, *dPosition;
	WAppIcon *ai;
	char buffer[64];

	ai = drawer->icon_array[0];
	/* Store its name */
	pstr = WMCreatePLString(ai->wm_instance);
	dName = WMRetainPropList(WMCreatePLString("Name"));
	drawer_state = WMCreatePLDictionary(dName, pstr, NULL); /* we need this final NULL */
	WMReleasePropList(pstr);

	/* Store its position */
	snprintf(buffer, sizeof(buffer), "%i,%i", ai->x_pos, ai->y_pos);
	pstr = WMCreatePLString(buffer);
	dPosition = WMCreatePLString("Position");
	WMPutInPLDictionary(drawer_state, dPosition, pstr);
	WMReleasePropList(pstr);

#ifdef USE_DOCK_XDND
	WMPropList *dDropCommand = NULL;
	/* Store its DnD command */
	if (ai->dnd_command) {
		pstr = WMCreatePLString(ai->dnd_command);
		dDropCommand = WMRetainPropList(WMCreatePLString("DropCommand"));
		WMPutInPLDictionary(drawer_state, dDropCommand, pstr);
		WMReleasePropList(pstr);
	}
#endif

	/* Store its paste command */
	if (ai->paste_command) {
		pstr = WMCreatePLString(ai->paste_command);
		dPasteCommand = WMRetainPropList(WMCreatePLString("PasteCommand"));
		WMPutInPLDictionary(drawer_state, dPasteCommand, pstr);
		WMReleasePropList(pstr);
	}

	/* Store applications list and other properties */
	pstr = drawer_save_state(drawer);
	dDock = WMCreatePLString("Dock");
	WMPutInPLDictionary(drawer_state, dDock, pstr);
	WMReleasePropList(pstr);

	return drawer_state;
}


void wDrawersSaveState(virtual_screen *vscr)
{
	WMPropList *all_drawers, *drawer_state, *dDrawers;
	int i;
	WDrawerChain *dc;

	all_drawers = WMCreatePLArray(NULL);
	for (i=0, dc = vscr->drawer.drawers;
	     i < vscr->drawer.drawer_count;
	     i++, dc = dc->next) {
		drawer_state = drawerSaveState(dc->adrawer);
		WMAddToPLArray(all_drawers, drawer_state);
		WMReleasePropList(drawer_state);
	}

	dDrawers = WMCreatePLString("Drawers");
	WMPutInPLDictionary(w_global.session_state, dDrawers, all_drawers);
	WMReleasePropList(all_drawers);
}

void wDrawers_unmap(int vscrno)
{
	WDrawerChain *dc;

	for (dc = w_global.vscreens[vscrno]->drawer.drawers; dc; dc = dc->next)
		drawerRestoreState_unmap(dc->adrawer);
}

static void drawerRemoveFromChain(WDock *drawer)
{
	virtual_screen *vscr = drawer->vscr;
	WDrawerChain *next, **to_remove;

	to_remove = &vscr->drawer.drawers;
	while (True) {
		if (*to_remove == NULL) {
			wwarning("The drawer to be removed can not be found.");
			return;
		}

		if ((*to_remove)->adrawer == drawer)
			break;

		to_remove = &(*to_remove)->next;
	}

	next = (*to_remove)->next;
	wfree(*to_remove);
	*to_remove = next;
	vscr->drawer.drawer_count--;
}



static int drawer_set_attacheddocks_do(WDock *dock, WMPropList *apps)
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
		aicon = restore_drawer_icon_state(vscr, value, dock->icon_count);
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

static WAppIcon *restore_drawer_icon_state(virtual_screen *vscr, WMPropList *info, int index)
{
	WAppIcon *aicon;
	WMPropList *cmd, *value;
	WMPropList *dCommand, *dPasteCommand, *dLock, *dOmnipresent;
	WMPropList *dName, *dForced, *dBuggyApplication, *dPosition, *dAutoLaunch;
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

	aicon->icon->core->descriptor.handle_expose = drawer_icon_expose;
	aicon->icon->core->descriptor.handle_mousedown = drawer_icon_mouse_down;
	aicon->icon->core->descriptor.handle_enternotify = drawer_enter_notify;
	aicon->icon->core->descriptor.handle_leavenotify = drawer_leave_notify;
	aicon->icon->core->descriptor.parent_type = WCLASS_DOCK_ICON;
	aicon->icon->core->descriptor.parent = aicon;

#ifdef USE_DOCK_XDND
	WMPropList *dDropCommand = NULL;
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

static WMPropList *drawer_save_state(WDock *dock)
{
	int i;
	WMPropList *icon_info;
	WMPropList *list = NULL, *dock_state = NULL;
	WMPropList *value, *dYes, *dNo;
	WMPropList *dApplications, *dCollapsed, *dAutoAttractIcons, *dAutoCollapse;

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
	dCollapsed = WMCreatePLString("Collapsed");
	value = (dock->collapsed ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dCollapsed, value);

	value = (dock->auto_collapse ? dYes : dNo);
	dAutoCollapse = WMCreatePLString("AutoCollapse");
	WMPutInPLDictionary(dock_state, dAutoCollapse, value);

	value = (dock->attract_icons ? dYes : dNo);
	dAutoAttractIcons = WMCreatePLString("AutoAttractIcons");
	WMPutInPLDictionary(dock_state, dAutoAttractIcons, value);

	return dock_state;
}

static void drawer_icon_expose(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *aicon = desc->parent;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	wIconPaint(aicon->icon);
	wAppIconPaint(aicon);
}

static void drawer_unmap(WDock *dock)
{
	WAppIcon *btn = dock->icon_array[0];

	XUnmapWindow(dpy, btn->icon->core->window);
	RemoveFromStackList(btn->icon->vscr, btn->icon->core);
	unmap_icon_image(btn->icon);
	wcore_unmap(btn->icon->core);
}

static void drawer_map(WDock *dock, virtual_screen *vscr)
{
	WAppIcon *btn = dock->icon_array[0];
	WIcon *icon = btn->icon;
	WCoreWindow *wcore = icon->core;
	WScreen *scr = vscr->screen_ptr;

	dock->x_pos = scr->scr_width - ICON_SIZE - DOCK_EXTRA_SPACE;

	btn->x_pos = dock->x_pos;
	btn->y_pos = dock->y_pos;

	wcore_map_toplevel(wcore, vscr, btn->x_pos, btn->y_pos,
					   wPreferences.icon_size, wPreferences.icon_size, 0,
					   scr->w_depth, scr->w_visual,
					   scr->w_colormap, scr->white_pixel);

	map_icon_image(icon);

	WMAddNotificationObserver(icon_appearanceObserver, icon, WNIconAppearanceSettingsChanged, icon);
	WMAddNotificationObserver(icon_tileObserver, icon, WNIconTileSettingsChanged, icon);

#ifdef USE_DOCK_XDND
	wXDNDMakeAwareness(wcore->window);
#endif

	AddToStackList(vscr, wcore);

	wcore->descriptor.handle_expose = drawerIconExpose;
	wcore->descriptor.handle_mousedown = drawer_icon_mouse_down;
	wcore->descriptor.handle_enternotify = drawer_enter_notify;
	wcore->descriptor.handle_leavenotify = drawer_leave_notify;

	XMapWindow(dpy, wcore->window);
	wRaiseFrame(vscr, wcore);
}

Bool drawer_attach_icon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon)
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

				return False;
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
	icon->icon->core->descriptor.handle_mousedown = drawer_icon_mouse_down;
	icon->icon->core->descriptor.handle_enternotify = drawer_enter_notify;
	icon->icon->core->descriptor.handle_leavenotify = drawer_leave_notify;
	icon->icon->core->descriptor.parent_type = WCLASS_DOCK_ICON;
	icon->icon->core->descriptor.parent = icon;

	MoveInStackListUnder(icon->icon->vscr, dock->icon_array[index - 1]->icon->core, icon->icon->core);
	wAppIconMove(icon, icon->x_pos, icon->y_pos);

	/*
	 * Update icon pixmap, RImage doesn't change,
	 * so call wIconUpdate is not needed
	 */
	if (lupdate_icon) {
		update_icon_pixmap(icon->icon);
		wIconPaint(icon->icon); /* dup */
	}

	/* Paint it */
	wIconPaint(icon->icon);
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

static void drawerIconExpose(WObjDescriptor *desc, XEvent *event)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	wDrawerIconPaint((WAppIcon *) desc->parent);
}



void removeDrawerCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock = ((WAppIcon*)entry->clientdata)->dock;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	if (dock->icon_count > 2) {
		if (wMessageDialog(dock->vscr, _("Drawer"),
					_("All icons in this drawer will be detached!"),
					_("OK"), _("Cancel"), NULL) != WAPRDefault)
			return;
	}

	drawerDestroy(dock);
}


void wDrawerIconPaint(WAppIcon *dicon)
{
	Window win = dicon->icon->core->window;
	virtual_screen *vscr = dicon->icon->vscr;
	WScreen *scr = vscr->screen_ptr;
	XPoint p[4];
	GC gc = scr->draw_gc;
	WMColor *color;

	wIconPaint(dicon->icon);

	if (!dicon->dock->collapsed)
		color = scr->clip_title_color[CLIP_NORMAL];
	else
		color = scr->clip_title_color[CLIP_COLLAPSED];

	XSetForeground(dpy, gc, WMColorPixel(color));

	if (dicon->dock->on_right_side) {
		p[0].x = p[3].x = 10;
		p[0].y = p[3].y = ICON_SIZE / 2 - 5;
		p[1].x = 10;
		p[1].y = ICON_SIZE / 2 + 5;
		p[2].x = 5;
		p[2].y = ICON_SIZE / 2;
	} else {
		p[0].x = p[3].x = ICON_SIZE - 1 - 10;
		p[0].y = p[3].y = ICON_SIZE / 2 - 5;
		p[1].x = ICON_SIZE - 1 - 10;
		p[1].y = ICON_SIZE / 2 + 5;
		p[2].x = ICON_SIZE - 1 - 5;
		p[2].y = ICON_SIZE / 2;
	}

	XFillPolygon(dpy, win, gc, p,3,Convex,CoordModeOrigin);
	XDrawLines(dpy, win, gc, p,4,CoordModeOrigin);
}


RImage *wDrawerMakeTile(virtual_screen *vscr, RImage *normalTile)
{
	RImage *tile = RCloneImage(normalTile);
	RColor dark;
	RColor light;

	dark.alpha = 0;
	dark.red = dark.green = dark.blue = 60;

	light.alpha = 0;
	light.red = light.green = light.blue = 80;

	/* arrow bevel */
	if (!vscr->dock.dock || vscr->dock.dock->on_right_side) {
		ROperateLine(tile, RSubtractOperation, 11, ICON_SIZE / 2 - 7,
			4, ICON_SIZE / 2, &dark);       /* / */
		ROperateLine(tile, RSubtractOperation, 11, ICON_SIZE / 2 + 7,
			4, ICON_SIZE / 2, &dark);       /* \ */
		ROperateLine(tile, RAddOperation,      11, ICON_SIZE / 2 - 7,
			11, ICON_SIZE / 2 + 7, &light); /* | */
	} else {
		ROperateLine(tile, RSubtractOperation, ICON_SIZE-1 - 11, ICON_SIZE / 2 - 7,
			ICON_SIZE-1 - 4, ICON_SIZE / 2, &dark);      /* \ */
		ROperateLine(tile, RAddOperation,      ICON_SIZE-1 - 11, ICON_SIZE / 2 + 7,
			ICON_SIZE-1 - 4, ICON_SIZE / 2, &light);     /* / */
		ROperateLine(tile, RSubtractOperation, ICON_SIZE-1 - 11, ICON_SIZE / 2 - 7,
			ICON_SIZE-1 - 11, ICON_SIZE / 2 + 7, &dark); /* | */
	}
	return tile;
}


static void swapDrawer(WDock *drawer, int new_x)
{
	int i;

	drawer->on_right_side = !drawer->on_right_side;
	drawer->x_pos = new_x;

	for (i = 0; i < drawer->max_icons; i++) {
		WAppIcon *ai;
		ai = drawer->icon_array[i];
		if (ai == NULL)
			continue;

		ai->xindex *= -1; /* so A B C becomes C B A */
		ai->x_pos = new_x + ai->xindex * ICON_SIZE;

		/* Update drawer's tile */
		if (i == 0) {
			wIconUpdate(ai->icon);
			wIconPaint(ai->icon);
			wDrawerIconPaint(ai);
		}

		XMoveWindow(dpy, ai->icon->core->window, ai->x_pos, ai->y_pos);
	}
}


void swapDrawers(virtual_screen *vscr, int new_x)
{
	WDrawerChain *dc;

	if (w_global.tile.drawer)
		RReleaseImage(w_global.tile.drawer);

	w_global.tile.drawer = wDrawerMakeTile(vscr, w_global.tile.icon);

	for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next)
		swapDrawer(dc->adrawer, new_x);
}


int wIsADrawer(WAppIcon *aicon)
{
	return aicon && aicon->dock &&
		aicon->dock->type == WM_DRAWER && aicon->dock->icon_array[0] == aicon;
}


WDock *getDrawer(virtual_screen *vscr, int y_index)
{
	WDrawerChain *dc;

	for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next)
		if (dc->adrawer->y_pos - vscr->dock.dock->y_pos == y_index * ICON_SIZE)
			return dc->adrawer;

	return NULL;
}

static void restore_drawer_position(WDock *drawer, WMPropList *state)
{
	virtual_screen *vscr = drawer->vscr;
	WMPropList *value, *dPosition;
	int y_index;

	dPosition = WMCreatePLString("Position");
	value = WMGetFromPLDictionary(state, dPosition);
	if (!value || !WMIsPLString(value)) {
		wwarning(_("bad value in drawer state info: Position"));
	} else {
		if (sscanf(WMGetFromPLString(value), "%i,%i", &drawer->x_pos, &drawer->y_pos) != 2)
			wwarning(_("bad value in drawer state info: Position"));

		/* check position sanity */
		if (drawer->x_pos != vscr->dock.dock->x_pos)
			drawer->x_pos = vscr->dock.dock->x_pos;

		y_index = (drawer->y_pos - vscr->dock.dock->y_pos) / ICON_SIZE;

		/* Here we should do something more intelligent, since it
		 * can happen even if the user hasn't hand-edited his
		 * G/D/State file (but uses a lower resolution). */
		if (y_index >= vscr->dock.dock->max_icons)
			y_index = vscr->dock.dock->max_icons - 1;

		drawer->y_pos = vscr->dock.dock->y_pos + y_index * ICON_SIZE;
	}
}

Bool drawer_snap_icon(WDock *dock, WAppIcon *icon, int req_x, int req_y, int *ret_x, int *ret_y, int redocking)
{
	virtual_screen *vscr = dock->vscr;
	int dx, dy;
	int ex_x, ex_y;
	int i, offset = ICON_SIZE / 2;
	WAppIcon *aicon = NULL;

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

	WAppIcon *aicons_to_shift[ dock->icon_count ];
	int index_of_hole, j;

	if (ex_y != 0 ||
		abs(ex_x) - dock->icon_count > DOCK_DETTACH_THRESHOLD ||
		(ex_x < 0 && !dock->on_right_side) ||
		(ex_x > 0 &&  dock->on_right_side)) {
		return False;
	}

	if (ex_x == 0)
		ex_x = (dock->on_right_side ? -1 : 1);

	/* "Reduce" ex_x but keep its sign */
	if (redocking) {
		if (abs(ex_x) > dock->icon_count - 1) /* minus 1: do not take icon_array[0] into account */
			ex_x = ex_x * (dock->icon_count - 1) / abs(ex_x); /* don't use *= ! */
	} else {
		if (abs(ex_x) > dock->icon_count)
			ex_x = ex_x * dock->icon_count / abs(ex_x);
	}

	index_of_hole = indexOfHole(dock, icon, redocking);

	/* Find the appicons between where icon was (index_of_hole) and where
	 * it wants to be (ex_x) and slide them. */
	j = 0;
	for (i = 1; i < dock->max_icons; i++) {
		aicon = dock->icon_array[i];
		if ((aicon != NULL) && aicon != icon &&
			((ex_x <= aicon->xindex && aicon->xindex < index_of_hole) ||
				(index_of_hole < aicon->xindex && aicon->xindex <= ex_x)))
			aicons_to_shift[j++] = aicon;
	}

	wSlideAppicons(aicons_to_shift, j, (index_of_hole < ex_x));

	*ret_x = ex_x;
	*ret_y = ex_y;

	return True;
}

void drawer_enter_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;
	WDock *dock, *tmp;
	virtual_screen *vscr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	if (desc->parent_type != WCLASS_DOCK_ICON)
		return;

	vscr = btn->icon->vscr;
	dock = btn->dock;

	if (dock == NULL)
		return;

	/* The auto raise/lower code */
	tmp = vscr->dock.dock;
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

void drawer_leave(WDock *dock)
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

	tmp = dock->vscr->dock.dock;
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
		dock->auto_collapse_magic = WMAddTimerHandler(wPreferences.clip_auto_collapse_delay, drawer_autocollapse, (void *)dock);
}

static void drawer_autocollapse(void *cdata)
{
	WDock *dock = (WDock *) cdata;

	if (dock->auto_collapse) {
		dock->collapsed = 1;
		wDockHideIcons(dock);
	}

	dock->auto_collapse_magic = NULL;
}

void drawer_leave_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	if (desc->parent_type != WCLASS_DOCK_ICON)
		return;

	drawer_leave(btn->dock);
}

void drawers_autolaunch(int vscrno)
{
	/* auto-launch apps in drawers */
	if (!wPreferences.flags.nodrawer) {
		WDrawerChain *dc;
		for (dc = w_global.vscreens[vscrno]->drawer.drawers; dc; dc = dc->next) {
			w_global.vscreens[vscrno]->last_dock = dc->adrawer;
			wDockDoAutoLaunch(dc->adrawer, 0);
		}
	}
}

void wDrawerFillTheGap(WDock *drawer, WAppIcon *aicon, Bool redocking)
{
	int i, j;
	int index_of_hole = indexOfHole(drawer, aicon, redocking);
	WAppIcon *aicons_to_shift[drawer->icon_count];

	j = 0;
	for (i = 0; i < drawer->max_icons; i++) {
		WAppIcon *ai = drawer->icon_array[i];
		if (ai && ai != aicon &&
			abs(ai->xindex) > abs(index_of_hole))
			aicons_to_shift[j++] = ai;
	}
	if (j != drawer->icon_count - abs(index_of_hole) - (redocking ? 1 : 0))
		wwarning("Removing aicon at index %d from %s: j=%d but should be %d",
			index_of_hole, drawer->icon_array[0]->wm_instance,
			j, drawer->icon_count - abs(index_of_hole) - (redocking ? 1 : 0));
	wSlideAppicons(aicons_to_shift, j, !drawer->on_right_side);
}

/* Find the "hole" a moving appicon created when snapped into the
 * drawer. redocking is a boolean. If the moving appicon comes from the
 * drawer, drawer->icon_count is correct. If not, redocking is then false and
 * there are now drawer->icon_count plus one appicons in the drawer. */
int indexOfHole(WDock *drawer, WAppIcon *moving_aicon, int redocking)
{
	int index_of_hole, i;

	/* Classic interview question...
	 *
	 * We have n-1 (n = drawer->icon_count-1 or drawer->icon_count, see
	 * redocking) appicons, whose xindex are unique in [1..n]. One is missing:
	 * that's where the ghost of the moving appicon is, that's what the
	 * function should return.
	 *
	 * We compute 1+2+...+n (this sum is equal to n*(n+1)/2), we subtract to
	 * this sum the xindex of each of the n-1 appicons, and we get the correct
	 * index! */

	if (redocking) {
		index_of_hole = (drawer->icon_count - 1) * drawer->icon_count / 2;
	} else {
		index_of_hole = drawer->icon_count * (drawer->icon_count + 1) / 2;
	}
	index_of_hole *= (drawer->on_right_side ? -1 : 1);

	for (i = 1; i < drawer->max_icons; i++) {
		if (drawer->icon_array[i] && drawer->icon_array[i] != moving_aicon)
			index_of_hole -= drawer->icon_array[i]->xindex;
	}
	/* wmessage(" Index of the moving appicon is %d (%sredocking)", index_of_hole, (redocking ? "" : "not ")); */
	if (abs(index_of_hole) > abs(drawer->icon_count) - (redocking ? 1 : 0))
		wwarning(" index_of_hole is too large ! (%d greater than %d)",
			index_of_hole, abs(drawer->icon_count) - (redocking ? 1 : 0));
	if (index_of_hole == 0)
		wwarning(" index_of_hole == 0 (%sredocking, icon_count == %d)", (redocking ? "" : "not "), drawer->icon_count);

	return index_of_hole;
}
