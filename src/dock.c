/* dock.c- Dock module for WindowMaker - Dock
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
#include "clip.h"
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
	DM_DOCKPOSSUBMENU,
	DM_ADD_DRAWER,
	DM_LAUNCH,
	DM_BRING,
	DM_HIDE,
	DM_SETTINGS,
	DM_KILL
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

static void iconDblClick(WObjDescriptor *desc, XEvent *event);
static void dock_menu(WDock *dock, WAppIcon *aicon, XEvent *event);
static WMenu *makeDockPositionMenu(virtual_screen *vscr);
static void updateDockPositionMenu(WDock *dock, WMenu *pos_menu);
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
static void setDockPositionKeepOnTopCallback(WMenu *menu, WMenuEntry *entry);
static void setDockPositionAutoRaiseLowerCallback(WMenu *menu, WMenuEntry *entry);
static void setDockPositionNormalCallback(WMenu *menu, WMenuEntry *entry);

WDock *dock_create(virtual_screen *vscr)
{
	WDock *dock;
	WAppIcon *btn;

	make_keys();
	dock = dock_create_core(vscr);

	/* Set basic variables */
	dock->type = WM_DOCK;
	dock->menu = NULL;

	btn = dock_icon_create(vscr, NULL, "WMDock", "Logo");

	btn->xindex = 0;
	btn->yindex = 0;
	btn->docked = 1;
	btn->dock = dock;
	dock->on_right_side = 1;
	dock->icon_array[0] = btn;

	btn->icon->core->descriptor.parent_type = WCLASS_DOCK_ICON;
	btn->icon->core->descriptor.parent = btn;

	if (wPreferences.flags.clip_merged_in_dock) {
		btn->icon->tile_type = TILE_CLIP;
		vscr->clip.icon = btn;
	} else {
		btn->icon->tile_type = TILE_NORMAL;
	}

	return dock;
}

void dock_icon_mouse_down(WObjDescriptor *desc, XEvent *event)
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
		if (event->xbutton.state & MOD_MASK)
			wDockLower(dock);
		else
			wDockRaise(dock);

		if (aicon->yindex == 0 && aicon->xindex == 0) {
			if (getClipButton(event->xbutton.x, event->xbutton.y) != CLIP_IDLE &&
			    wPreferences.flags.clip_merged_in_dock)
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

		dock_menu(dock, aicon, event);
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
			} else if (wIsADrawer(btn)) {
				toggleCollapsed(dock);
			} else if (btn->command) {
				if (!btn->launching && (!btn->running || (event->xbutton.state & ControlMask)))
					launchDockedApplication(btn, False);
			} else if (btn->xindex == 0 && btn->yindex == 0 && btn->dock->type == WM_DOCK) {
				panel_show(dock->vscr, PANEL_INFO);
			}
		}
	}
}

static void dock_menu(WDock *dock, WAppIcon *aicon, XEvent *event)
{
	virtual_screen *vscr = aicon->icon->vscr;
	WScreen *scr = vscr->screen_ptr;
	WMenu *pos_menu;
	WObjDescriptor *desc;
	WMenuEntry *entry = NULL;
	WApplication *wapp = NULL;
	int appIsRunning, x_pos;

	/* Get info about the application */
	if (aicon->icon->owner)
		wapp = wApplicationOf(aicon->icon->owner->main_window);

	appIsRunning = aicon->running && aicon->icon && aicon->icon->owner;

	/* Create the menu */
	dock->menu = menu_create(vscr, NULL);

	/* Dock position menu */
	entry = wMenuAddCallback(dock->menu, _("Dock position"), NULL, NULL);
	pos_menu = makeDockPositionMenu(vscr);
	wMenuEntrySetCascade_create(dock->menu, entry, pos_menu);

	/* Add drawer menu */
	if (!wPreferences.flags.nodrawer)
		wMenuAddCallback(dock->menu, _("Add a drawer"), addADrawerCallback, NULL);

	wMenuAddCallback(dock->menu, _("Launch"), launchCallback, NULL);

	/* Unhide Here / Bring Here */
	if (wapp && wapp->flags.hidden)
		wMenuAddCallback(dock->menu, _("Unhide Here"), dockUnhideHereCallback, NULL);
	else
		wMenuAddCallback(dock->menu, _("Bring Here"), dockUnhideHereCallback, NULL);

	/* Hide / Unhide */
	if (wapp && wapp->flags.hidden)
		entry = wMenuAddCallback(dock->menu, _("Unhide"), dockHideCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Hide"), dockHideCallback, NULL);

	/* Settings */
	wMenuAddCallback(dock->menu, _("Settings..."), settingsCallback, NULL);

	/* Remove drawer / kill */
	if (wIsADrawer(aicon))
		entry = wMenuAddCallback(dock->menu, _("Remove drawer"), removeDrawerCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Kill"), dockKillCallback, NULL);

	if (!wPreferences.flags.nodrawer) {
		/* add a drawer */
		entry = dock->menu->entries[DM_ADD_DRAWER];
		entry->clientdata = aicon;
		menu_entry_set_enabled(dock->menu, DM_ADD_DRAWER, True);

		/* launch */
		entry = dock->menu->entries[DM_LAUNCH];
		entry->clientdata = aicon;
		menu_entry_set_enabled(dock->menu, DM_LAUNCH, aicon->command != NULL);

		/* unhide here */
		entry = dock->menu->entries[DM_BRING];
		entry->clientdata = aicon;
		menu_entry_set_enabled(dock->menu, DM_BRING, appIsRunning);

		/* hide */
		entry = dock->menu->entries[DM_HIDE];
		entry->clientdata = aicon;
		menu_entry_set_enabled(dock->menu, DM_HIDE, appIsRunning);

		/* settings */
		entry = dock->menu->entries[DM_SETTINGS];
		entry->clientdata = aicon;
		menu_entry_set_enabled(dock->menu, DM_SETTINGS, !aicon->editing && !wPreferences.flags.noupdates);

		/* kill or remove drawer */
		entry = dock->menu->entries[DM_KILL];
		entry->clientdata = aicon;
		if (wIsADrawer(aicon))
			menu_entry_set_enabled(dock->menu, DM_KILL, True);
		else
			menu_entry_set_enabled(dock->menu, DM_KILL, appIsRunning);

		menu_entry_set_enabled_paint(dock->menu, DM_ADD_DRAWER);
		menu_entry_set_enabled_paint(dock->menu, DM_LAUNCH);
		menu_entry_set_enabled_paint(dock->menu, DM_BRING);
		menu_entry_set_enabled_paint(dock->menu, DM_HIDE);
		menu_entry_set_enabled_paint(dock->menu, DM_SETTINGS);
		menu_entry_set_enabled_paint(dock->menu, DM_KILL);
	}

	/* Dock position menu */
	updateDockPositionMenu(dock, pos_menu);

	x_pos = dock->on_right_side ? scr->scr_width - dock->menu->frame->width - 3 : 0;

	/* Positions and mapping */
	menu_map(dock->menu);
	menu_map(pos_menu);

	dock->menu->x_pos = x_pos;
	dock->menu->y_pos = event->xbutton.y_root + 2;
	wMenuMapAt(vscr, dock->menu, False);

	/* allow drag select */
	event->xany.send_event = True;
	desc = &dock->menu->core->descriptor;
	(*desc->handle_mousedown) (desc, event);

	/* Destroy the menu */
	pos_menu->flags.realized = 0;
	dock->menu->flags.realized = 0;
	wMenuDestroy(dock->menu);
	pos_menu = NULL;
	dock->menu = NULL;
}

static WMenu *makeDockPositionMenu(virtual_screen *vscr)
{
	/* When calling this, the dock is being created, so scr->dock is still not set
	 * Therefore the callbacks' clientdata and the indicators can't be set,
	 * they will be updated when the dock menu is opened. */
	WMenu *menu;
	WMenuEntry *entry;

	menu = menu_create(vscr, NULL);

	entry = wMenuAddCallback(menu, _("Normal"), setDockPositionNormalCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_type = MI_DIAMOND;

	entry = wMenuAddCallback(menu, _("Auto raise & lower"), setDockPositionAutoRaiseLowerCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_type = MI_DIAMOND;

	entry = wMenuAddCallback(menu, _("Keep on Top"), setDockPositionKeepOnTopCallback, NULL);
	entry->flags.indicator = 1;
	entry->flags.indicator_type = MI_DIAMOND;

	menu->flags.realized = 0;

	return menu;
}

static void updateDockPositionMenu(WDock *dock, WMenu *pos_menu)
{
	WMenuEntry *entry;
	int index = 0;

	if (!pos_menu || !dock)
		return;

	/* Normal level */
	entry = pos_menu->entries[index++];
	entry->flags.indicator_on = (dock->lowered && !dock->auto_raise_lower);
	entry->clientdata = dock;

	/* Auto-raise/lower */
	entry = pos_menu->entries[index++];
	entry->flags.indicator_on = dock->auto_raise_lower;
	entry->clientdata = dock;

	/* Keep on top */
	entry = pos_menu->entries[index++];
	entry->flags.indicator_on = !dock->lowered;
	entry->clientdata = dock;

	dock->menu->flags.realized = 0;
}



static void setDockPositionNormalCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock = (WDock *) entry->clientdata;
	WDrawerChain *dc;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	/* Already set, nothing to do */
	if (entry->flags.indicator_on)
		return;

	/* Do we come from auto raise lower or keep on top? */
	if (dock->auto_raise_lower) {
		dock->auto_raise_lower = 0;
		/* Only for aesthetic purposes, can be removed
		 * when Autoraise status is no longer exposed
		 * in drawer option menu */
		for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
			dc->adrawer->auto_raise_lower = 0;
	} else {
		/* Will take care of setting lowered = 0 in drawers */
		toggleLowered(dock);
	}

	entry->flags.indicator_on = 1;
}
static void setDockPositionAutoRaiseLowerCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock = (WDock *) entry->clientdata;
	WDrawerChain *dc;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	/* Already set, nothing to do */
	if (entry->flags.indicator_on)
		return;

	/* Do we come from normal or keep on top? */
	if (!dock->lowered)
		toggleLowered(dock);

	dock->auto_raise_lower = 1;

	/* Only for aesthetic purposes, can be removed
	 * when Autoraise status is no longer exposed
	 * in drawer option menu */
	for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
		dc->adrawer->auto_raise_lower = 1;

	entry->flags.indicator_on = 1;
}

static void setDockPositionKeepOnTopCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock = (WDock *) entry->clientdata;
	WDrawerChain *dc;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	/* Already set, nothing to do */
	if (entry->flags.indicator_on)
		return;

	dock->auto_raise_lower = 0;

	/* Only for aesthetic purposes, can be removed
	 * when Autoraise status is no longer exposed
	 * in drawer option menu */
	for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
		dc->adrawer->auto_raise_lower = 0;

	toggleLowered(dock);
	entry->flags.indicator_on = 1;
}
