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
	DM_DOCKPOSSUBMENU,
	DM_ADD_DRAWER,
	DM_LAUNCH,
	DM_BRING,
	DM_HIDE,
	DM_SETTINGS,
	DM_KILL
};

static void iconDblClick(WObjDescriptor *desc, XEvent *event);
static void dock_menu(WDock *dock, WAppIcon *aicon, XEvent *event);
static WMenu *makeDockPositionMenu(virtual_screen *vscr);
static void updateDockPositionMenu(WDock *dock, WMenu *pos_menu);
static void restore_dock_position(WDock *dock, WMPropList *state);
static int dock_set_attacheddocks_do(WDock *dock, WMPropList *apps);
static void dock_set_attacheddocks(WDock *dock, WMPropList *state);
static WAppIcon *restore_dock_icon_state(virtual_screen *vscr, WMPropList *info, int index);
static WMPropList *dock_save_state(WDock *dock);
static void save_application_list(WMPropList *state, WMPropList *list, virtual_screen *vscr);
static void setDockPositionKeepOnTopCallback(WMenu *menu, WMenuEntry *entry);
static void setDockPositionAutoRaiseLowerCallback(WMenu *menu, WMenuEntry *entry);
static void setDockPositionNormalCallback(WMenu *menu, WMenuEntry *entry);

WDock *dock_create(virtual_screen *vscr)
{
	WDock *dock;
	WAppIcon *btn;

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
		if (event->xbutton.state & wPreferences.modifier_mask)
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

void dock_enter_notify(WObjDescriptor *desc, XEvent *event)
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

	return;
}

void dock_leave_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	if (desc->parent_type != WCLASS_DOCK_ICON)
		return;

	dock_leave(btn->dock);
}

void dock_map(WDock *dock, WMPropList *state)
{
	WAppIcon *btn = dock->icon_array[0];
	WIcon *icon = btn->icon;
	WCoreWindow *wcore = icon->core;
	virtual_screen *vscr = dock->vscr;
	WScreen *scr = vscr->screen_ptr;

	/* Return if virtual screen is not mapped */
	if (!scr)
		return;

	wcore_map_toplevel(wcore, vscr, 0, 0,
					   wPreferences.icon_size, wPreferences.icon_size, 0,
					   scr->w_depth, scr->w_visual, scr->w_colormap,
					   scr->white_pixel);

	if (wPreferences.flags.clip_merged_in_dock)
		wcore->descriptor.handle_expose = clip_icon_expose;
	else
		wcore->descriptor.handle_expose = dock_icon_expose;

	map_icon_image(icon);

	WMAddNotificationObserver(icon_appearanceObserver, icon, WNIconAppearanceSettingsChanged, icon);
	WMAddNotificationObserver(icon_tileObserver, icon, WNIconTileSettingsChanged, icon);

#ifdef USE_DOCK_XDND
	wXDNDMakeAwareness(wcore->window);
#endif

	AddToStackList(vscr, wcore);

	wcore->descriptor.handle_mousedown = dock_icon_mouse_down;
	wcore->descriptor.handle_enternotify = dock_enter_notify;
	wcore->descriptor.handle_leavenotify = dock_leave_notify;
	btn->x_pos = scr->scr_width - ICON_SIZE - DOCK_EXTRA_SPACE;
	btn->y_pos = 0;

	dock->x_pos = btn->x_pos;
	dock->y_pos = btn->y_pos;
	XMapWindow(dpy, wcore->window);

	wRaiseFrame(vscr, wcore);

	if (!state)
		return;

	WMRetainPropList(state);

	/* restore position */
	restore_dock_position(dock, state);

	restore_state_lowered(dock, state);
	restore_state_collapsed(dock, state);
	(void) restore_state_autocollapsed(dock, state);
	restore_state_autoraise(dock, state);
	(void) restore_state_autoattracticons(dock, state);

	/* application list */
	dock_set_attacheddocks(dock, state);

	WMReleasePropList(state);
}

void dock_unmap(WDock *dock)
{
	WAppIcon *btn = dock->icon_array[0];

	dock_unset_attacheddocks(dock);
	XUnmapWindow(dpy, btn->icon->core->window);
	RemoveFromStackList(btn->icon->vscr, btn->icon->core);
	unmap_icon_image(btn->icon);
}

static void restore_dock_position(WDock *dock, WMPropList *state)
{
	WMPropList *value, *dPosition;
	virtual_screen *vscr = dock->vscr;
	WScreen *scr = vscr->screen_ptr;

	dPosition = WMCreatePLString("Position");
	value = WMGetFromPLDictionary(state, dPosition);
	if (value) {
		if (!WMIsPLString(value)) {
				wwarning(_("bad value in dock state info: Position"));
		} else {
			if (sscanf(WMGetFromPLString(value), "%i,%i", &dock->x_pos, &dock->y_pos) != 2)
				wwarning(_("bad value in dock state info: Position"));

			/* check position sanity */
			if (!onScreen(vscr, dock->x_pos, dock->y_pos)) {
				int x = dock->x_pos;
				wScreenKeepInside(vscr, &x, &dock->y_pos, ICON_SIZE, ICON_SIZE);
			}

			/* Is this needed any more? */
			if (dock->x_pos >= 0) {
				dock->x_pos = DOCK_EXTRA_SPACE;
				dock->on_right_side = 0;
			} else {
				dock->x_pos = scr->scr_width - DOCK_EXTRA_SPACE - ICON_SIZE;
				dock->on_right_side = 1;
			}
		}
	}
}

void dock_leave(WDock *dock)
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

	return;
}

void dock_icon_expose(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *aicon = desc->parent;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	wIconPaint(aicon->icon);
	wAppIconPaint(aicon);
}

static int dock_set_attacheddocks_do(WDock *dock, WMPropList *apps)
{
	virtual_screen *vscr = dock->vscr;
	int count, i;
	WMPropList *value;
	WAppIcon *aicon;

	count = WMGetPropListItemCount(apps);
	if (count == 0)
		return 1;

	/* dock->icon_count is set to 1 when dock is created.
	 * Since Clip is already restored, we want to keep it so for clip,
	 * but for dock we may change the default top tile, so we set it to 0.
	 */
	dock->icon_count = 0;

	for (i = 0; i < count; i++) {
		if (dock->icon_count >= dock->max_icons) {
			wwarning(_("there are too many icons stored in dock. Ignoring what doesn't fit"));
			break;
		}

		value = WMGetFromPLArray(apps, i);
		aicon = restore_dock_icon_state(vscr, value, dock->icon_count);
		dock->icon_array[dock->icon_count] = aicon;

		if (aicon) {
			aicon->dock = dock;
			aicon->x_pos = dock->x_pos + (aicon->xindex * ICON_SIZE);
			aicon->y_pos = dock->y_pos + (aicon->yindex * ICON_SIZE);
			dock->icon_count++;
		} else if (dock->icon_count == 0) {
			dock->icon_count++;
		}
	}

	return 0;
}

static void dock_set_attacheddocks(WDock *dock, WMPropList *state)
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

	if (dock_set_attacheddocks_do(dock, apps))
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

void dock_unset_attacheddocks(WDock *dock)
{
	set_attacheddocks_unmap(dock);
}

static WAppIcon *restore_dock_icon_state(virtual_screen *vscr, WMPropList *info, int index)
{
	WAppIcon *aicon;
	WMPropList *cmd, *value;
	WMPropList *dCommand, *dName, *dPasteCommand, *dLock, *dAutoLaunch;
	WMPropList *dPosition, *dForced, *dBuggyApplication, *dOmnipresent;
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
	aicon->icon->core->descriptor.handle_mousedown = dock_icon_mouse_down;
	aicon->icon->core->descriptor.handle_enternotify = dock_enter_notify;
	aicon->icon->core->descriptor.handle_leavenotify = dock_leave_notify;
	aicon->icon->core->descriptor.parent_type = WCLASS_DOCK_ICON;
	aicon->icon->core->descriptor.parent = aicon;

#ifdef USE_DOCK_XDND
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
	}

	aicon->xindex = 0;

	dOmnipresent = WMCreatePLString("Omnipresent");
	value = WMGetFromPLDictionary(info, dOmnipresent);
	aicon->omnipresent = getBooleanDockValue(value, dOmnipresent);

	aicon->running = 0;
	aicon->docked = 1;

	return aicon;
}

Bool dock_attach_icon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon)
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

				/* If the target is the dock, reject the icon. */
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
	icon->icon->core->descriptor.handle_mousedown = dock_icon_mouse_down;
	icon->icon->core->descriptor.handle_enternotify = dock_enter_notify;
	icon->icon->core->descriptor.handle_leavenotify = dock_leave_notify;
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

/*
 * returns the closest Dock slot index for the passed
 * coordinates.
 *
 * Returns False if icon can't be docked.
 *
 * Note: this function should NEVER alter ret_x or ret_y, unless it will
 * return True. -Dan
 */
/* Redocking == true means either icon->dock == dock (normal case)
 * or we are called from handleDockMove for a drawer */
Bool dock_snap_icon(WDock *dock, WAppIcon *icon, int req_x, int req_y, int *ret_x, int *ret_y, int redocking)
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

	/* We can return False right away if
	 * - we do not come from this dock (which is a WM_DOCK),
	 * - we are not right over it, and
	 * - we are not the main tile of a drawer.
	 * In the latter case, we are called from handleDockMove. */
	if (icon->dock != dock && ex_x != 0 &&
		!(icon->dock && icon->dock->type == WM_DRAWER && icon == icon->dock->icon_array[0]))
		return False;

	if (!redocking && ex_x != 0)
		return False;

	if (getDrawer(vscr, ex_y)) /* Return false so that the drawer gets it. */
		return False;

	aicon = NULL;
	for (i = 0; i < dock->max_icons; i++) {
		nicon = dock->icon_array[i];
		if (nicon && nicon->yindex == ex_y) {
			aicon = nicon;
			break;
		}
	}

	if (redocking) {
		int sig, done, closest;

		/* Possible cases when redocking:
		 *
		 * icon dragged out of range of any slot -> false
		 * icon dragged on a drawer -> false (to open the drawer)
		 * icon dragged to range of free slot
		 * icon dragged to range of same slot
		 * icon dragged to range of different icon
		 */
		if (abs(ex_x) > DOCK_DETTACH_THRESHOLD)
			return False;

		if (aicon == icon || !aicon) {
			*ret_x = 0;
			*ret_y = ex_y;
			return True;
		}

		/* start looking at the upper slot or lower? */
		if (ex_y * ICON_SIZE < (req_y + offset - dy))
			sig = 1;
		else
			sig = -1;

		done = 0;
		/* look for closest free slot */
		for (i = 0; i < (DOCK_DETTACH_THRESHOLD + 1) * 2 && !done; i++) {
			int j;

			done = 1;
			closest = sig * (i / 2) + ex_y;
			/* check if this slot is fully on the screen and not used */
			if (onScreen(vscr, dx, dy + closest * ICON_SIZE)) {
				for (j = 0; j < dock->max_icons; j++) {
					if (dock->icon_array[j]
						&& dock->icon_array[j]->yindex == closest) {
						/* slot is used by someone else */
						if (dock->icon_array[j] != icon)
							done = 0;
						break;
					}
				}
				/* slot is used by a drawer */
				done = done && !getDrawer(vscr, closest);
			} else {
				/* !onScreen */
				done = 0;
			}

			sig = -sig;
		}

		if (done &&
		    ((ex_y >= closest && ex_y - closest < DOCK_DETTACH_THRESHOLD + 1) ||
		     (ex_y < closest && closest - ex_y <= DOCK_DETTACH_THRESHOLD + 1))) {
			*ret_x = 0;
			*ret_y = closest;
			return True;
		}
	} else {	/* !redocking */
		/* if slot is free and the icon is close enough, return it */
		if (!aicon && ex_x == 0) {
			*ret_x = 0;
			*ret_y = ex_y;
			return True;
		}
	}

	return False;
}

void dock_autolaunch(int vscrno)
{
	/* auto-launch apps */
	if (!wPreferences.flags.nodock && w_global.vscreens[vscrno]->dock.dock) {
		w_global.vscreens[vscrno]->last_dock = w_global.vscreens[vscrno]->dock.dock;
		wDockDoAutoLaunch(w_global.vscreens[vscrno]->dock.dock, 0);
	}
}

void dockIconPaint(WAppIcon *btn)
{
	virtual_screen *vscr = btn->icon->vscr;

	if (btn == vscr->clip.icon) {
		wClipIconPaint(btn);
	} else if (wIsADrawer(btn)) {
		wDrawerIconPaint(btn);
	} else {
		wIconPaint(btn->icon);
		wAppIconPaint(btn);
		save_appicon(btn);
	}
}

void wDockSaveState(virtual_screen *vscr, WMPropList *old_state)
{
	WMPropList *dock_state, *keys, *dDock;

	dock_state = dock_save_state(vscr->dock.dock);

	/* Copy saved states of docks with different sizes. */
	if (old_state) {
		int i;
		WMPropList *tmp;

		keys = WMGetPLDictionaryKeys(old_state);
		for (i = 0; i < WMGetPropListItemCount(keys); i++) {
			tmp = WMGetFromPLArray(keys, i);
			if (strncasecmp(WMGetFromPLString(tmp), "applications", 12) == 0
			    && !WMGetFromPLDictionary(dock_state, tmp))
				WMPutInPLDictionary(dock_state, tmp, WMGetFromPLDictionary(old_state, tmp));
		}

		WMReleasePropList(keys);
	}

	dDock = WMCreatePLString("Dock");
	WMPutInPLDictionary(w_global.session_state, dDock, dock_state);
	WMReleasePropList(dock_state);
}

static WMPropList *dock_save_state(WDock *dock)
{
	virtual_screen *vscr = dock->vscr;
	int i;
	WMPropList *icon_info, *value;
	WMPropList *list, *dock_state, *dPosition, *dApplications, *dLowered;
	WMPropList *dAutoRaiseLower, *dYes, *dNo;
	char buffer[256];

	list = WMCreatePLArray(NULL);

	for (i = 0; i < dock->max_icons; i++) {
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

	/* Save with the same screen_id. See get_application_list() */
	save_application_list(dock_state, list, vscr);

	snprintf(buffer, sizeof(buffer), "%i,%i", (dock->on_right_side ? -ICON_SIZE : 0), dock->y_pos);
	value = WMCreatePLString(buffer);
	dPosition = WMCreatePLString("Position");
	WMPutInPLDictionary(dock_state, dPosition, value);
	WMReleasePropList(value);
	WMReleasePropList(list);

	dYes = WMRetainPropList(WMCreatePLString("Yes"));
	dNo = WMRetainPropList(WMCreatePLString("No"));
	value = (dock->lowered ? dYes : dNo);
	dLowered = WMCreatePLString("Lowered");
	WMPutInPLDictionary(dock_state, dLowered, value);

	value = (dock->auto_raise_lower ? dYes : dNo);
	dAutoRaiseLower = WMCreatePLString("AutoRaiseLower");
	WMPutInPLDictionary(dock_state, dAutoRaiseLower, value);

	return dock_state;
}

static void save_application_list(WMPropList *state, WMPropList *list, virtual_screen *vscr)
{
	WMPropList *key;

	key = get_applications_string(vscr);
	WMPutInPLDictionary(state, key, list);
	WMReleasePropList(key);
}
