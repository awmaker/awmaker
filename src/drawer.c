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

/***** Local variables ****/
static WMPropList *dCommand = NULL;
static WMPropList *dPasteCommand = NULL;
#ifdef USE_DOCK_XDND			/* XXX was OFFIX */
static WMPropList *dDropCommand = NULL;
#endif
static WMPropList *dAutoLaunch, *dLock;
static WMPropList *dName, *dForced, *dBuggyApplication, *dYes, *dNo;
static WMPropList *dHost, *dDock;
static WMPropList *dAutoAttractIcons;
static WMPropList *dPosition, *dApplications, *dLowered, *dCollapsed;
static WMPropList *dAutoCollapse, *dAutoRaiseLower, *dOmnipresent;
static WMPropList *dDrawers = NULL;

static char *findUniqueName(virtual_screen *vscr, const char *instance_basename);
static void drawerAppendToChain(WDock *drawer);
static void iconDblClick(WObjDescriptor *desc, XEvent *event);
static void drawerConsolidateIcons(WDock *drawer);
static WMenu *drawer_make_options_menu(virtual_screen *vscr);
static void drawer_remove_icons_callback(WMenu *menu, WMenuEntry *entry);
static void make_keys(void);


static void make_keys(void)
{
	if (dCommand != NULL)
		return;

	dCommand = WMRetainPropList(WMCreatePLString("Command"));
	dPasteCommand = WMRetainPropList(WMCreatePLString("PasteCommand"));
#ifdef USE_DOCK_XDND
	dDropCommand = WMRetainPropList(WMCreatePLString("DropCommand"));
#endif
	dLock = WMRetainPropList(WMCreatePLString("Lock"));
	dAutoLaunch = WMRetainPropList(WMCreatePLString("AutoLaunch"));
	dName = WMRetainPropList(WMCreatePLString("Name"));
	dForced = WMRetainPropList(WMCreatePLString("Forced"));
	dBuggyApplication = WMRetainPropList(WMCreatePLString("BuggyApplication"));
	dYes = WMRetainPropList(WMCreatePLString("Yes"));
	dNo = WMRetainPropList(WMCreatePLString("No"));
	dHost = WMRetainPropList(WMCreatePLString("Host"));

	dPosition = WMCreatePLString("Position");
	dApplications = WMCreatePLString("Applications");
	dLowered = WMCreatePLString("Lowered");
	dCollapsed = WMCreatePLString("Collapsed");
	dAutoCollapse = WMCreatePLString("AutoCollapse");
	dAutoRaiseLower = WMCreatePLString("AutoRaiseLower");
	dAutoAttractIcons = WMCreatePLString("AutoAttractIcons");

	dOmnipresent = WMCreatePLString("Omnipresent");

	dDock = WMCreatePLString("Dock");
	dDrawers = WMCreatePLString("Drawers");
}
WDock *drawer_create(virtual_screen *vscr, const char *name)
{
	WDock *dock;
	WAppIcon *btn;

	make_keys();

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

void drawer_menu(WDock *dock, WAppIcon *aicon, XEvent *event)
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
		assert(maxDeleted > 0); // would mean while test is wrong
		n = 0;
		for (i = 0; i < drawer->max_icons; i++) {
			ai = drawer->icon_array[i];
			if (ai != NULL && abs(ai->xindex) > maxDeleted)
				aicons_to_shift[n++] = ai;
		}
		assert(n == maxRemaining - maxDeleted); // for the code review ;-)
		wSlideAppicons(aicons_to_shift, n, !drawer->on_right_side);
		// Efficient beancounting
		maxRemaining -= 1;
		sum -= n;
	}
}
