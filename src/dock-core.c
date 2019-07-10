/* dock.c- built-in Dock module for WindowMaker
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *  Copyright (c) 1998-2003 Dan Pascu
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

#ifndef PATH_MAX
#define PATH_MAX DEFAULT_PATH_MAX
#endif

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
	OM_KEEP_ON_TOP,
	OM_COLLAPSED,
	OM_AUTO_COLLAPSED,
	OM_AUTORAISE,
	OM_AUTOATTRACT
};

static pid_t execCommand(WAppIcon *btn, const char *command, WSavedState *state);
static void trackDeadProcess(pid_t pid, unsigned int status, WDock *dock);
static void moveDock(WDock *dock, int new_x, int new_y);

void toggleLoweredCallback(WMenu *menu, WMenuEntry *entry)
{
	toggleLowered(entry->clientdata);
	entry->flags.indicator_on = !(((WDock *) entry->clientdata)->lowered);
	wMenuPaint(menu);
}

static int matchWindow(const void *item, const void *cdata)
{
	return (((WFakeGroupLeader *) item)->leader == (Window) cdata);
}

void dockKillCallback(WMenu *menu, WMenuEntry *entry)
{
	virtual_screen *vscr = menu->vscr;
	WScreen *scr = vscr->screen_ptr;
	WAppIcon *icon;
	WFakeGroupLeader *fPtr;
	char *buffer, *shortname, **argv;
	int argc;

	if (!WCHECK_STATE(WSTATE_NORMAL))
		return;

	icon = (WAppIcon *) entry->clientdata;
	icon->editing = 1;
	WCHANGE_STATE(WSTATE_MODAL);

	/* strip away dir names */
	shortname = basename(icon->command);
	/* separate out command options */
	wtokensplit(shortname, &argv, &argc);

	buffer = wstrconcat(argv[0],
			    _(" will be forcibly closed.\n"
			      "Any unsaved changes will be lost.\n" "Please confirm."));

	if (icon->icon && icon->icon->owner) {
		fPtr = icon->icon->owner->fake_group;
	} else {
		/* is this really necessary? can we kill a non-running dock icon? */
		Window win = icon->main_window;
		int index;

		index = WMFindInArray(scr->fakeGroupLeaders, matchWindow, (void *)win);
		if (index != WANotFound)
			fPtr = WMGetFromArray(scr->fakeGroupLeaders, index);
		else
			fPtr = NULL;
	}

	if (wPreferences.dont_confirm_kill
	    || wMessageDialog(menu->vscr, _("Kill Application"),
			      buffer, _("Yes"), _("No"), NULL) == WAPRDefault) {
		if (fPtr != NULL) {
			WWindow *wwin, *twin;

			wwin = vscr->window.focused;
			while (wwin) {
				twin = wwin->prev;
				if (wwin->fake_group == fPtr)
					wClientKill(wwin);

				wwin = twin;
			}
		} else if (icon->icon && icon->icon->owner) {
			wClientKill(icon->icon->owner);
		}
	}

	wfree(buffer);
	wtokenfree(argv, argc);

	icon->editing = 0;

	WCHANGE_STATE(WSTATE_NORMAL);
}

/* TODO: replace this function with a member of the dock struct */
int numberOfSelectedIcons(WDock *dock)
{
	WAppIcon *aicon;
	int i, n;

	n = 0;
	for (i = 1; i < dock->max_icons; i++) {
		aicon = dock->icon_array[i];
		if (aicon && aicon->icon->selected)
			n++;
	}

	return n;
}

WMArray *getSelected(WDock *dock)
{
	WMArray *ret = WMCreateArray(8);
	WAppIcon *btn;
	int i;

	for (i = 1; i < dock->max_icons; i++) {
		btn = dock->icon_array[i];
		if (btn && btn->icon->selected)
			WMAddToArray(ret, btn);
	}

	return ret;
}

void removeIcons(WMArray *icons, WDock *dock)
{
	WAppIcon *aicon;
	WCoord *coord;
	int keepit;
	WMArrayIterator it;

	WM_ITERATE_ARRAY(icons, aicon, it) {
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

	WMFreeArray(icons);

	if (wPreferences.auto_arrange_icons)
		wArrangeIcons(dock->vscr, True);
}


void keepIconsCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *clickedIcon = (WAppIcon *) entry->clientdata;
	WDock *dock;
	WAppIcon *aicon;
	WMArray *selectedIcons;
	WMArrayIterator it;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	dock = clickedIcon->dock;
	selectedIcons = getSelected(dock);
	if (!WMGetArrayItemCount(selectedIcons) &&
	    clickedIcon != dock->vscr->clip.icon) {
		char *command = NULL;

		if (!clickedIcon->command && !clickedIcon->editing) {
			clickedIcon->editing = 1;
			if (wInputDialog(dock->vscr, _("Keep Icon"),
					 _("Type the command used to launch the application"), &command)) {
				if (command && (command[0] == 0 || (command[0] == '-' && command[1] == 0))) {
					wfree(command);
					command = NULL;
				}
				clickedIcon->command = command;
				clickedIcon->editing = 0;
			} else {
				clickedIcon->editing = 0;
				if (command)
					wfree(command);
				WMFreeArray(selectedIcons);
				return;
			}
		}

		WMAddToArray(selectedIcons, clickedIcon);
	}

	WM_ITERATE_ARRAY(selectedIcons, aicon, it) {
		if (aicon->icon->selected)
			wIconSelect(aicon->icon);

		if (aicon->attracted && aicon->command) {
			aicon->attracted = 0;
			if (aicon->icon->shadowed) {
				aicon->icon->shadowed = 0;

				/*
				 * Update icon pixmap, RImage doesn't change,
				 * so call wIconUpdate is not needed
				 */
				update_icon_pixmap(aicon->icon);

				/* Paint it */
				wAppIconPaint(aicon);
			}
		}
		save_appicon(aicon);
	}
	WMFreeArray(selectedIcons);
}

void toggleAutoAttractCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock = (WDock *) entry->clientdata;
	virtual_screen *vscr = dock->vscr;
	int i;

	dock->attract_icons = !dock->attract_icons;
	entry->flags.indicator_on = dock->attract_icons;
	wMenuPaint(menu);

	if (dock->attract_icons) {
		if (dock->type == WM_DRAWER) {
			/* The newly auto-attracting dock is a drawer: disable any clip and
			 * previously attracting drawer */
			if (!wPreferences.flags.noclip) {
				for (i = 0; i < vscr->workspace.count; i++)
					vscr->workspace.array[i]->clip->attract_icons = False;
					/* dock menu will be updated later, when opened */
			}

			if (vscr->drawer.attracting_drawer != NULL)
				vscr->drawer.attracting_drawer->attract_icons = False;

			vscr->drawer.attracting_drawer = dock;
		} else {
			/* The newly auto-attracting dock is a clip: disable
			 * previously attracting drawer, if applicable */
			if (vscr->drawer.attracting_drawer != NULL) {
				vscr->drawer.attracting_drawer->attract_icons = False;
				/* again, its menu will be updated, later. */
				vscr->drawer.attracting_drawer = NULL;
			}
		}
	}
}

void selectCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *icon = (WAppIcon *) entry->clientdata;
	wIconSelect(icon->icon);
	wMenuPaint(menu);
}

void attractIconsCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *clickedIcon = (WAppIcon *) entry->clientdata;
	WDock *clip; /* clip... is a WM_CLIP or a WM_DRAWER */
	WAppIcon *aicon;
	int x, y, x_pos, y_pos;
	Bool update_icon = False;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	clip = clickedIcon->dock;
	aicon = w_global.app_icon_list;

	while (aicon) {
		if (!aicon->docked && wDockFindFreeSlot(clip, &x, &y)) {
			x_pos = clip->x_pos + x * ICON_SIZE;
			y_pos = clip->y_pos + y * ICON_SIZE;
			if (aicon->x_pos != x_pos || aicon->y_pos != y_pos)
				move_window(aicon->icon->core->window, aicon->x_pos, aicon->y_pos, x_pos, y_pos);

			aicon->attracted = 1;
			if (!aicon->icon->shadowed) {
				aicon->icon->shadowed = 1;
				update_icon = True;
			}
			wDockAttachIcon(clip, aicon, x, y, update_icon);
			if (clip->collapsed || !clip->mapped)
				XUnmapWindow(dpy, aicon->icon->core->window);
		}
		aicon = aicon->next;
	}
}

void selectIconsCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *clickedIcon = (WAppIcon *) entry->clientdata;
	WDock *dock;
	WMArray *selectedIcons;
	WMArrayIterator iter;
	WAppIcon *btn;
	int i;

	dock = clickedIcon->dock;
	selectedIcons = getSelected(dock);

	if (!WMGetArrayItemCount(selectedIcons)) {
		for (i = 1; i < dock->max_icons; i++) {
			btn = dock->icon_array[i];
			if (btn && !btn->icon->selected)
				wIconSelect(btn->icon);
		}
	} else {
		WM_ITERATE_ARRAY(selectedIcons, btn, iter) {
			wIconSelect(btn->icon);
		}
	}
	WMFreeArray(selectedIcons);

	wMenuPaint(menu);
}

void toggleCollapsedCallback(WMenu *menu, WMenuEntry *entry)
{
	toggleCollapsed(entry->clientdata);
	entry->flags.indicator_on = ((WDock *) entry->clientdata)->collapsed;
	wMenuPaint(menu);
}

void toggleAutoCollapseCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock;

	dock = (WDock *) entry->clientdata;
	dock->auto_collapse = !dock->auto_collapse;
	entry->flags.indicator_on = ((WDock *) entry->clientdata)->auto_collapse;

	wMenuPaint(menu);
}

void toggleAutoRaiseLower(WDock *dock)
{
	WDrawerChain *dc;

	dock->auto_raise_lower = !dock->auto_raise_lower;
	if (dock->type == WM_DOCK)
		for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
			toggleAutoRaiseLower(dc->adrawer);
}

void toggleAutoRaiseLowerCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	dock = (WDock *) entry->clientdata;
	toggleAutoRaiseLower(dock);
	entry->flags.indicator_on = ((WDock *) entry->clientdata)->auto_raise_lower;
	wMenuPaint(menu);
}

void launchCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *btn = (WAppIcon *) entry->clientdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	launchDockedApplication(btn, False);
}

void settingsCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *btn = (WAppIcon *) entry->clientdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	if (btn->editing)
		return;
	ShowDockAppSettingsPanel(btn);
}

void dockHideCallback(WMenu *menu, WMenuEntry *entry)
{
	WApplication *wapp;
	WAppIcon *btn = (WAppIcon *) entry->clientdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	wapp = wApplicationOf(btn->icon->owner->main_window);

	if (wapp->flags.hidden) {
		wWorkspaceChange(btn->icon->vscr, wapp->last_workspace);
		wUnhideApplication(wapp, False, False);
	} else {
		wHideApplication(wapp);
	}
}

void dockUnhideHereCallback(WMenu *menu, WMenuEntry *entry)
{
	WApplication *wapp;
	WAppIcon *btn = (WAppIcon *) entry->clientdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	wapp = wApplicationOf(btn->icon->owner->main_window);

	wUnhideApplication(wapp, False, True);
}

void launchDockedApplication(WAppIcon *btn, Bool withSelection)
{
	virtual_screen *vscr = btn->icon->vscr;

	if (btn->launching)
		return;

	if ((withSelection || btn->command == NULL) &&
	    (!withSelection || btn->paste_command == NULL))
		return;

	if (!btn->forced_dock) {
		btn->relaunching = btn->running;
		btn->running = 1;
	}

	if (btn->wm_instance || btn->wm_class) {
		WWindowAttributes attr;
		memset(&attr, 0, sizeof(WWindowAttributes));
		wDefaultFillAttributes(btn->wm_instance, btn->wm_class, &attr, NULL, True);

		if (!attr.no_appicon && !btn->buggy_app)
			btn->launching = 1;
		else
			btn->running = 0;
	}

	btn->drop_launch = 0;
	btn->paste_launch = withSelection;
	vscr->last_dock = btn->dock;
	btn->pid = execCommand(btn, (withSelection ? btn->paste_command : btn->command), NULL);
	if (btn->pid > 0) {
		if (btn->buggy_app) {
			/* give feedback that the app was launched */
			btn->launching = 1;
			dockIconPaint(btn);
			btn->launching = 0;
			WMAddTimerHandler(200, (WMCallback *) dockIconPaint, btn);
		} else {
			dockIconPaint(btn);
		}
	} else {
		wwarning(_("could not launch application %s"), btn->command);
		btn->launching = 0;
		if (!btn->relaunching)
			btn->running = 0;
	}
}

void dockUpdateOptionsMenu(WDock *dock, WMenu *menu)
{
	WMenuEntry *entry;

	if (!menu || !dock)
		return;

	/* keep on top */
	entry = menu->entries[OM_KEEP_ON_TOP];
	entry->flags.indicator_on = !dock->lowered;
	entry->clientdata = dock;
	menu_entry_set_enabled(menu, OM_KEEP_ON_TOP, dock->type == WM_CLIP);

	/* collapsed */
	entry = menu->entries[OM_COLLAPSED];
	entry->flags.indicator_on = dock->collapsed;
	entry->clientdata = dock;

	/* auto-collapse */
	entry = menu->entries[OM_AUTO_COLLAPSED];
	entry->flags.indicator_on = dock->auto_collapse;
	entry->clientdata = dock;

	/* auto-raise/lower */
	entry = menu->entries[OM_AUTORAISE];
	entry->flags.indicator_on = dock->auto_raise_lower;
	entry->clientdata = dock;
	menu_entry_set_enabled(menu, OM_AUTORAISE, dock->lowered && (dock->type == WM_CLIP));

	/* attract icons */
	entry = menu->entries[OM_AUTOATTRACT];
	entry->flags.indicator_on = dock->attract_icons;
	entry->clientdata = dock;

	menu_entry_set_enabled_paint(menu, OM_KEEP_ON_TOP);
	menu_entry_set_enabled_paint(menu, OM_AUTORAISE);
	menu->flags.realized = 0;
}


WDock *dock_create_core(virtual_screen *vscr)
{
	WDock *dock;

	dock = wmalloc(sizeof(WDock));
	dock->max_icons = DOCK_MAX_ICONS;
	dock->icon_array = wmalloc(sizeof(WAppIcon *) * dock->max_icons);

	/* Set basic variables */
	dock->vscr = vscr;
	dock->icon_count = 1;
	dock->collapsed = 0;
	dock->auto_collapse = 0;
	dock->auto_collapse_magic = NULL;
	dock->auto_raise_lower = 0;
	dock->auto_lower_magic = NULL;
	dock->auto_raise_magic = NULL;
	dock->attract_icons = 0;
	dock->lowered = 1;

	return dock;
}

WMPropList *make_icon_state(WAppIcon *btn)
{
	virtual_screen *vscr = btn->icon->vscr;
	WMPropList *node = NULL;
	WMPropList *dCommand, *dName, *dAutoLaunch, *dLock, *dForced, *dYes, *dNo;
	WMPropList *dBuggyApplication, *dPosition, *dOmnipresent, *dPasteCommand;
	WMPropList *command, *name, *autolaunch, *lock, *forced;
	WMPropList *position, *buggy, *omnipresent;
	char *tmp;
	char buffer[64];

	if (!btn)
		return NULL;

	if (!btn->command)
		command = WMCreatePLString("-");
	else
		command = WMCreatePLString(btn->command);

	dYes = WMRetainPropList(WMCreatePLString("Yes"));
	dNo = WMRetainPropList(WMCreatePLString("No"));
	autolaunch = btn->auto_launch ? dYes : dNo;
	lock = btn->lock ? dYes : dNo;

	tmp = EscapeWM_CLASS(btn->wm_instance, btn->wm_class);
	name = WMCreatePLString(tmp);
	wfree(tmp);

	forced = btn->forced_dock ? dYes : dNo;
	buggy = btn->buggy_app ? dYes : dNo;
	if (!wPreferences.flags.clip_merged_in_dock && btn == vscr->clip.icon)
		snprintf(buffer, sizeof(buffer), "%i,%i", btn->x_pos, btn->y_pos);
	else
		snprintf(buffer, sizeof(buffer), "%hi,%hi", btn->xindex, btn->yindex);

	position = WMCreatePLString(buffer);
	dCommand = WMRetainPropList(WMCreatePLString("Command"));
	dName = WMRetainPropList(WMCreatePLString("Name"));
	dAutoLaunch = WMRetainPropList(WMCreatePLString("AutoLaunch"));
	dLock = WMRetainPropList(WMCreatePLString("Lock"));
	dForced = WMRetainPropList(WMCreatePLString("Forced"));
	dBuggyApplication = WMRetainPropList(WMCreatePLString("BuggyApplication"));
	dPosition = WMCreatePLString("Position");
	node = WMCreatePLDictionary(dCommand, command,
				    dName, name,
				    dAutoLaunch, autolaunch,
				    dLock, lock,
				    dForced, forced, dBuggyApplication, buggy, dPosition, position, NULL);
	WMReleasePropList(command);
	WMReleasePropList(name);
	WMReleasePropList(position);

	dOmnipresent = WMCreatePLString("Omnipresent");
	omnipresent = btn->omnipresent ? dYes : dNo;
	if (btn->dock != vscr->dock.dock && (btn->xindex != 0 || btn->yindex != 0))
		WMPutInPLDictionary(node, dOmnipresent, omnipresent);

#ifdef USE_DOCK_XDND
	WMPropList *dDropCommand;
	dDropCommand = WMRetainPropList(WMCreatePLString("DropCommand"));
	if (btn->dnd_command) {
		command = WMCreatePLString(btn->dnd_command);
		WMPutInPLDictionary(node, dDropCommand, command);
		WMReleasePropList(command);
	}
#endif

	if (btn->paste_command) {
		dPasteCommand = WMRetainPropList(WMCreatePLString("PasteCommand"));
		command = WMCreatePLString(btn->paste_command);
		WMPutInPLDictionary(node, dPasteCommand, command);
		WMReleasePropList(command);
	}

	return node;
}

Bool getBooleanDockValue(WMPropList *value, WMPropList *key)
{
	if (value) {
		if (WMIsPLString(value)) {
			if (strcasecmp(WMGetFromPLString(value), "YES") == 0)
				return True;
		} else {
			wwarning(_("bad value in docked icon state info %s"), WMGetFromPLString(key));
		}
	}
	return False;
}

/* restore lowered/raised state */
void restore_state_lowered(WDock *dock, WMPropList *state)
{
	WMPropList *value, *dLowered;

	dock->lowered = 0;

	if (!state)
		return;

	dLowered = WMCreatePLString("Lowered");
	value = WMGetFromPLDictionary(state, dLowered);
	if (value) {
		if (!WMIsPLString(value)) {
			wwarning(_("bad value in dock/drawer state info: Lowered"));
		} else {
			if (strcasecmp(WMGetFromPLString(value), "YES") == 0)
				dock->lowered = 1;
		}
	}
}

/* restore collapsed state */
void restore_state_collapsed(WDock *dock, WMPropList *state)
{
	WMPropList *value, *dCollapsed;

	dock->collapsed = 0;

	if (!state)
		return;

	dCollapsed = WMCreatePLString("Collapsed");
	value = WMGetFromPLDictionary(state, dCollapsed);
	if (value) {
		if (!WMIsPLString(value)) {
			wwarning(_("bad value in dock/drawer state info: Collapsed"));
		} else {
			if (strcasecmp(WMGetFromPLString(value), "YES") == 0)
				dock->collapsed = 1;
		}
	}
}

/* restore auto-collapsed state */
int restore_state_autocollapsed(WDock *dock, WMPropList *state)
{
	WMPropList *value, *dAutoCollapse;
	int ret = 0;

	if (!state)
		return 0;

	dAutoCollapse = WMCreatePLString("AutoCollapse");
	value = WMGetFromPLDictionary(state, dAutoCollapse);
	if (!value)
		return 0;

	if (!WMIsPLString(value)) {
			wwarning(_("bad value in dock/drawer state info: AutoCollapse"));
	} else {
		if (strcasecmp(WMGetFromPLString(value), "YES") == 0) {
			dock->auto_collapse = 1;
			dock->collapsed = 1;
			ret = 1;
		}
	}

	return ret;
}

/* restore auto-raise/lower state */
void restore_state_autoraise(WDock *dock, WMPropList *state)
{
	WMPropList *value, *dAutoRaiseLower;

	if (!state)
		return;

	dAutoRaiseLower = WMCreatePLString("AutoRaiseLower");
	value = WMGetFromPLDictionary(state, dAutoRaiseLower);
	if (!value)
		return;

	if (!WMIsPLString(value)) {
			wwarning(_("bad value in dock/drawer state info: AutoRaiseLower"));
	} else {
		if (strcasecmp(WMGetFromPLString(value), "YES") == 0)
			dock->auto_raise_lower = 1;
	}
}

/* restore attract icons state */
int restore_state_autoattracticons(WDock *dock, WMPropList *state)
{
	WMPropList *value, *dAutoAttractIcons;
	int ret = 0;

	dock->attract_icons = 0;

	if (!state)
		return 0;

	dAutoAttractIcons = WMCreatePLString("AutoAttractIcons");
	value = WMGetFromPLDictionary(state, dAutoAttractIcons);
	if (!value)
		return 0;

	if (!WMIsPLString(value)) {
			wwarning(_("bad value in dock/drawer state info: AutoAttractIcons"));
	} else {
		if (strcasecmp(WMGetFromPLString(value), "YES") == 0) {
			dock->attract_icons = 1;
			ret = 1;
		}
	}

	return ret;
}

WMPropList *get_applications_string(virtual_screen *vscr)
{
	WMPropList *key;
	char buffer[64];

	snprintf(buffer, sizeof(buffer), "Applications_%d", vscr->id);
	key = WMCreatePLString(buffer);

	return key;
}

WMPropList *get_application_list(WMPropList *dock_state, virtual_screen *vscr)
{
	WMPropList *key, *apps, *dApplications;

	/*
	 * When saving, it saves the dock state in
	 * Applications and Applications_nnn
	 *
	 * When loading, it will first try Applications_nnn.
	 * If it does not exist, use Applications as default.
	 */
	key = get_applications_string(vscr);
	apps = WMGetFromPLDictionary(dock_state, key);
	WMReleasePropList(key);

	if (!apps) {
		dApplications = WMCreatePLString("Applications");
		apps = WMGetFromPLDictionary(dock_state, dApplications);
	}

	return apps;
}

void set_attacheddocks_map(WDock *dock)
{
	WAppIcon *aicon;
	int i = 0;

	if (!dock)
		return;

	if (dock->type != WM_DOCK)
		i = 1;

	for (; i < dock->max_icons; i++) {
		aicon = dock->icon_array[i];
		if (aicon) {
			appicon_map(aicon);

			if (dock->lowered)
				ChangeStackingLevel(aicon->icon->vscr, aicon->icon->core, WMNormalLevel);
			else
				ChangeStackingLevel(aicon->icon->vscr, aicon->icon->core, WMDockLevel);

			wCoreConfigure(aicon->icon->core, aicon->x_pos, aicon->y_pos, aicon->icon->width, aicon->icon->height);
			if (!dock->collapsed)
				XMapWindow(dpy, aicon->icon->core->window);

			wRaiseFrame(aicon->icon->vscr, aicon->icon->core);
		}
	}
}

void set_attacheddocks_unmap(WDock *dock)
{
	WAppIcon *aicon;
	int i = 0;

	if (!dock)
		return;

	if (dock->type != WM_DOCK)
		i = 1;

	for (; i < dock->max_icons; i++) {
		aicon = dock->icon_array[i];
		if (aicon)
			appicon_unmap(aicon);
	}
}


void wDockLaunchWithState(WAppIcon *btn, WSavedState *state)
{
	if (btn && btn->command && !btn->running && !btn->launching) {
		btn->drop_launch = 0;
		btn->paste_launch = 0;

		btn->pid = execCommand(btn, btn->command, state);

		if (btn->pid > 0) {
			if (!btn->forced_dock && !btn->buggy_app) {
				btn->launching = 1;
				dockIconPaint(btn);
			}
		}
	} else {
		wfree(state);
	}
}

void dockedapps_autolaunch(int vscrno)
{
	dock_autolaunch(vscrno);
	clip_autolaunch(vscrno);
	drawers_autolaunch(vscrno);
}

void wDockDoAutoLaunch(WDock *dock, int workspace)
{
	WAppIcon *btn;
	WSavedState *state;
	int i;

	for (i = 0; i < dock->max_icons; i++) {
		btn = dock->icon_array[i];
		if (!btn || !btn->auto_launch)
			continue;

		state = wmalloc(sizeof(WSavedState));
		state->workspace = workspace;
		/* TODO: this is klugy and is very difficult to understand
		 * what's going on. Try to clean up */
		wDockLaunchWithState(btn, state);
	}
}

#ifdef USE_DOCK_XDND			/* was OFFIX */
static WDock *findDock(virtual_screen *vscr, XEvent *event, int *icon_pos)
{
	WDock *dock;
	int i;

	dock = vscr->dock.dock;
	if (dock != NULL) {
		for (i = 0; i < dock->max_icons; i++) {
			if (dock->icon_array[i] &&
			    dock->icon_array[i]->icon->core->window == event->xclient.window) {
				*icon_pos = i;
				return dock;
			}
		}
	}

	dock = vscr->workspace.array[vscr->workspace.current]->clip;
	if (dock != NULL) {
		for (i = 0; i < dock->max_icons; i++) {
			if (dock->icon_array[i] &&
			    dock->icon_array[i]->icon->core->window == event->xclient.window) {
				*icon_pos = i;
				return dock;
			}
		}
	}

	*icon_pos = -1;
	return NULL;
}

int wDockReceiveDNDDrop(virtual_screen *vscr, XEvent *event)
{
	WDock *dock;
	WAppIcon *btn;
	WWindowAttributes attr;
	int icon_pos;

	dock = findDock(vscr, event, &icon_pos);
	if (!dock)
		return False;

	/*
	 * Return True if the drop was on an application icon window.
	 * In this case, let the ClientMessage handler redirect the
	 * message to the app.
	 */
	if (dock->icon_array[icon_pos]->icon->icon_win != None)
		return True;

	if (dock->icon_array[icon_pos]->dnd_command == NULL)
		return False;

	vscr->screen_ptr->flags.dnd_data_convertion_status = 0;
	btn = dock->icon_array[icon_pos];
	if (!btn->forced_dock) {
		btn->relaunching = btn->running;
		btn->running = 1;
	}

	if (btn->wm_instance || btn->wm_class) {
		memset(&attr, 0, sizeof(WWindowAttributes));
		wDefaultFillAttributes(btn->wm_instance, btn->wm_class, &attr, NULL, True);

		if (!attr.no_appicon)
			btn->launching = 1;
		else
			btn->running = 0;
	}

	btn->paste_launch = 0;
	btn->drop_launch = 1;
	vscr->last_dock = dock;
	btn->pid = execCommand(btn, btn->dnd_command, NULL);
	if (btn->pid > 0) {
		dockIconPaint(btn);
	} else {
		btn->launching = 0;
		if (!btn->relaunching)
			btn->running = 0;
	}

	return False;
}
#endif	/* USE_DOCK_XDND */

Bool wDockAttachIcon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon)
{
	switch (dock->type) {
	case WM_DOCK:
		return dock_attach_icon(dock, icon, x, y, update_icon);
		break;
	case WM_CLIP:
		return clip_attach_icon(dock, icon, x, y, update_icon);
		break;
	case WM_DRAWER:
		return drawer_attach_icon(dock, icon, x, y, update_icon);
	}

	/* Avoid compiler warning */
	return True;
}

void wDockReattachIcon(WDock *dock, WAppIcon *icon, int x, int y)
{
	int index;

	for (index = 1; index < dock->max_icons; index++) {
		if (dock->icon_array[index] == icon)
			break;
	}

	icon->yindex = y;
	icon->xindex = x;
	icon->x_pos = dock->x_pos + x * ICON_SIZE;
	icon->y_pos = dock->y_pos + y * ICON_SIZE;
}

Bool wDockMoveIconBetweenDocks(WDock *src, WDock *dest, WAppIcon *icon, int x, int y)
{
	WWindow *wwin;
	char *command = NULL;
	int index, sts;
	Bool update_icon = False;

	if (src == dest)
		return True;	/* No move needed, we're already there */

	if (dest == NULL)
		return False;

	/*
	 * For the moment we can't do this if we move icons in Clip from one
	 * workspace to other, because if we move two or more icons without
	 * command, the dialog box will not be able to tell us to which of the
	 * moved icons it applies. -Dan
	 */
	if ((dest->type == WM_DOCK /*|| dest->keep_attracted */ ) && icon->command == NULL) {
		/* If icon->owner exists, it means the application is running */
		if (icon->icon->owner) {
			wwin = icon->icon->owner;
			command = GetCommandForWindow(wwin->client_win);
		}

		if (command) {
			icon->command = command;
		} else {
			icon->editing = 1;
			/* icon->forced_dock = 1; */
			if (wInputDialog(src->vscr, _("Dock Icon"),
					 _("Type the command used to launch the application"), &command)) {

				if (command && (command[0] == 0 || (command[0] == '-' && command[1] == 0))) {
					wfree(command);
					command = NULL;
				}

				icon->command = command;
			} else {
				icon->editing = 0;
				if (command)
					wfree(command);

				return False;
			}

			icon->editing = 0;
		}
	}

	if (dest->type == WM_DOCK || dest->type == WM_DRAWER) {
		sts = wClipMakeIconOmnipresent(icon, False);
		if (sts == WO_FAILED || sts == WO_SUCCESS)
			wAppIconPaint(icon);
	}

	for (index = 1; index < src->max_icons; index++)
		if (src->icon_array[index] == icon)
			break;

	src->icon_array[index] = NULL;
	src->icon_count--;

	for (index = 1; index < dest->max_icons; index++)
		if (dest->icon_array[index] == NULL)
			break;

	dest->icon_array[index] = icon;
	icon->dock = dest;

	/* deselect the icon */
	if (icon->icon->selected)
		wIconSelect(icon->icon);

	/* New type is like the destination type */
	switch (dest->type) {
	case WM_DOCK:
		icon->icon->core->descriptor.handle_mousedown = dock_icon_mouse_down;
		icon->icon->core->descriptor.handle_enternotify = dock_enter_notify;
		icon->icon->core->descriptor.handle_leavenotify = dock_leave_notify;

		/* set it to be kept when moving to dock.
		 * Unless the icon does not have a command set
		 */
		if (icon->command) {
			icon->attracted = 0;
			if (icon->icon->shadowed) {
				icon->icon->shadowed = 0;
				update_icon = True;
			}
		}

		if (src->auto_collapse || src->auto_raise_lower)
			dock_leave(src);

		break;
	case WM_CLIP:
		icon->icon->core->descriptor.handle_mousedown = clip_icon_mouse_down;
		icon->icon->core->descriptor.handle_enternotify = clip_enter_notify;
		icon->icon->core->descriptor.handle_leavenotify = clip_leave_notify;

		if (src->auto_collapse || src->auto_raise_lower)
			clip_leave(src);

		break;
	case WM_DRAWER:
		icon->icon->core->descriptor.handle_mousedown = drawer_icon_mouse_down;
		icon->icon->core->descriptor.handle_enternotify = drawer_enter_notify;
		icon->icon->core->descriptor.handle_leavenotify = drawer_leave_notify;

		/* set it to be kept when moving to dock.
		 * Unless the icon does not have a command set
		 */
		if (icon->command) {
			icon->attracted = 0;
			if (icon->icon->shadowed) {
				icon->icon->shadowed = 0;
				update_icon = True;
			}

			save_appicon(icon);
		}

		if (src->auto_collapse || src->auto_raise_lower)
			drawer_leave(src);
	}

	icon->yindex = y;
	icon->xindex = x;

	icon->x_pos = dest->x_pos + x * ICON_SIZE;
	icon->y_pos = dest->y_pos + y * ICON_SIZE;

	dest->icon_count++;

	MoveInStackListUnder(icon->icon->vscr, dest->icon_array[index - 1]->icon->core, icon->icon->core);

	/*
	 * Update icon pixmap, RImage doesn't change,
	 * so call wIconUpdate is not needed
	 */
	if (update_icon)
		update_icon_pixmap(icon->icon);

	/* Paint it */
	wAppIconPaint(icon);

	return True;
}

void wDockDetach(WDock *dock, WAppIcon *icon)
{
	int index, sts;
	Bool update_icon = False;

	/* make the settings panel be closed */
	if (icon->panel)
		DestroyDockAppSettingsPanel(icon->panel);

	/* This must be called before icon->dock is set to NULL.
	 * Don't move it. -Dan
	 */
	sts = wClipMakeIconOmnipresent(icon, False);
	if (sts == WO_FAILED || sts == WO_SUCCESS)
		wAppIconPaint(icon);

	icon->docked = 0;
	icon->dock = NULL;
	icon->attracted = 0;
	icon->auto_launch = 0;
	if (icon->icon->shadowed) {
		icon->icon->shadowed = 0;
		update_icon = True;
	}

	/* deselect the icon */
	if (icon->icon->selected)
		wIconSelect(icon->icon);

	if (icon->command) {
		wfree(icon->command);
		icon->command = NULL;
	}
#ifdef USE_DOCK_XDND			/* was OFFIX */
	if (icon->dnd_command) {
		wfree(icon->dnd_command);
		icon->dnd_command = NULL;
	}
#endif
	if (icon->paste_command) {
		wfree(icon->paste_command);
		icon->paste_command = NULL;
	}

	for (index = 1; index < dock->max_icons; index++)
		if (dock->icon_array[index] == icon)
			break;

	dock->icon_array[index] = NULL;
	icon->yindex = -1;
	icon->xindex = -1;

	dock->icon_count--;

	/* Remove the Cached Icon */
	remove_cache_icon(icon->icon->file_name);

	/* if the dock is not attached to an application or
	 * the application did not set the appropriate hints yet,
	 * destroy the icon */
	if (!icon->running || !wApplicationOf(icon->main_window)) {
		wAppIconDestroy(icon);
	} else {
		icon->icon->core->descriptor.handle_mousedown = appIconMouseDown;
		icon->icon->core->descriptor.handle_enternotify = NULL;
		icon->icon->core->descriptor.handle_leavenotify = NULL;
		icon->icon->core->descriptor.parent_type = WCLASS_APPICON;
		icon->icon->core->descriptor.parent = icon;

		ChangeStackingLevel(icon->icon->vscr, icon->icon->core, NORMAL_ICON_LEVEL);

		/*
		 * Update icon pixmap, RImage doesn't change,
		 * so call wIconUpdate is not needed
		 */
		if (update_icon)
			update_icon_pixmap(icon->icon);

		/* Paint it */
		wAppIconPaint(icon);

		if (wPreferences.auto_arrange_icons)
			wArrangeIcons(dock->vscr, True);
	}

	if (dock->auto_collapse || dock->auto_raise_lower) {
		switch (dock->type) {
		case WM_DOCK:
			dock_leave(dock);
			break;
		case WM_CLIP:
			clip_leave(dock);
			break;
		case WM_DRAWER:
			drawer_leave(dock);
		}
	}
}

Bool wDockSnapIcon(WDock *dock, WAppIcon *icon, int req_x, int req_y,
		   int *ret_x, int *ret_y, int redocking)
{
	switch (dock->type) {
	case WM_DOCK:
		return dock_snap_icon(dock, icon, req_x, req_y, ret_x,
				      ret_y, redocking);
		break;
	case WM_CLIP:
		return clip_snap_icon(dock, icon, req_x, req_y, ret_x,
				       ret_y, redocking);
		break;
	case WM_DRAWER:
		return drawer_snap_icon(dock, icon, req_x, req_y, ret_x,
					ret_y, redocking);
	}

	/* Avoid compiler warning, and defalt */
	return False;
}

int onScreen(virtual_screen *vscr, int x, int y)
{
	WMRect rect;
	int flags;

	rect.pos.x = x;
	rect.pos.y = y;
	rect.size.width = rect.size.height = ICON_SIZE;

	wGetRectPlacementInfo(vscr, rect, &flags);

	return !(flags & (XFLAG_DEAD | XFLAG_PARTIAL));
}

/*
 * returns true if it can find a free slot in the dock,
 * in which case it changes x_pos and y_pos accordingly.
 * Else returns false.
 */
Bool wDockFindFreeSlot(WDock *dock, int *x_pos, int *y_pos)
{
	virtual_screen *vscr = dock->vscr;
	WScreen *scr = vscr->screen_ptr;
	WAppIcon *btn;
	WAppIconChain *chain;
	unsigned char *slot_map;
	int mwidth, r, x, y, corner;
	int i, done = False;
	int ex = scr->scr_width, ey = scr->scr_height;
	int extra_count = 0;

	if (dock->type == WM_DRAWER) {
		if (dock->icon_count >= dock->max_icons) /* drawer is full */
			return False;

		*x_pos = dock->icon_count * (dock->on_right_side ? -1 : 1);
		*y_pos = 0;

		return True;
	}

	if (dock->type == WM_CLIP && dock != vscr->workspace.array[vscr->workspace.current]->clip)
		extra_count = vscr->global_icon_count;

	/* if the dock is full */
	if (dock->icon_count + extra_count >= dock->max_icons)
		return False;

	if (!wPreferences.flags.nodock && vscr->dock.dock && vscr->dock.dock->on_right_side)
		ex -= ICON_SIZE + DOCK_EXTRA_SPACE;

	if (ex < dock->x_pos)
		ex = dock->x_pos;
#define C_NONE 0
#define C_NW 1
#define C_NE 2
#define C_SW 3
#define C_SE 4

	/* check if clip is in a corner */
	if (dock->type == WM_CLIP) {
		if (dock->x_pos < 1 && dock->y_pos < 1)
			corner = C_NE;
		else if (dock->x_pos < 1 && dock->y_pos >= (ey - ICON_SIZE))
			corner = C_SE;
		else if (dock->x_pos >= (ex - ICON_SIZE) && dock->y_pos >= (ey - ICON_SIZE))
			corner = C_SW;
		else if (dock->x_pos >= (ex - ICON_SIZE) && dock->y_pos < 1)
			corner = C_NW;
		else
			corner = C_NONE;
	} else {
		corner = C_NONE;
	}

	/* If the clip is in the corner, use only slots that are in the border
	 * of the screen */
	if (corner != C_NONE) {
		char *hmap, *vmap;
		int hcount, vcount;

		hcount = WMIN(dock->max_icons, vscr->screen_ptr->scr_width / ICON_SIZE);
		vcount = WMIN(dock->max_icons, vscr->screen_ptr->scr_height / ICON_SIZE);
		hmap = wmalloc(hcount + 1);
		vmap = wmalloc(vcount + 1);

		/* mark used positions */
		switch (corner) {
		case C_NE:
			for (i = 0; i < dock->max_icons; i++) {
				btn = dock->icon_array[i];
				if (!btn)
					continue;

				if (btn->xindex == 0 && btn->yindex > 0 && btn->yindex < vcount)
					vmap[btn->yindex] = 1;
				else if (btn->yindex == 0 && btn->xindex > 0 && btn->xindex < hcount)
					hmap[btn->xindex] = 1;
			}

			for (chain = vscr->clip.global_icons; chain != NULL; chain = chain->next) {
				btn = chain->aicon;
				if (btn->xindex == 0 && btn->yindex > 0 && btn->yindex < vcount)
					vmap[btn->yindex] = 1;
				else if (btn->yindex == 0 && btn->xindex > 0 && btn->xindex < hcount)
					hmap[btn->xindex] = 1;
			}

			break;
		case C_NW:
			for (i = 0; i < dock->max_icons; i++) {
				btn = dock->icon_array[i];
				if (!btn)
					continue;

				if (btn->xindex == 0 && btn->yindex > 0 && btn->yindex < vcount)
					vmap[btn->yindex] = 1;
				else if (btn->yindex == 0 && btn->xindex < 0 && btn->xindex > -hcount)
					hmap[-btn->xindex] = 1;
			}

			for (chain = vscr->clip.global_icons; chain != NULL; chain = chain->next) {
				btn = chain->aicon;
				if (btn->xindex == 0 && btn->yindex > 0 && btn->yindex < vcount)
					vmap[btn->yindex] = 1;
				else if (btn->yindex == 0 && btn->xindex < 0 && btn->xindex > -hcount)
					hmap[-btn->xindex] = 1;
			}

			break;
		case C_SE:
			for (i = 0; i < dock->max_icons; i++) {
				btn = dock->icon_array[i];
				if (!btn)
					continue;

				if (btn->xindex == 0 && btn->yindex < 0 && btn->yindex > -vcount)
					vmap[-btn->yindex] = 1;
				else if (btn->yindex == 0 && btn->xindex > 0 && btn->xindex < hcount)
					hmap[btn->xindex] = 1;
			}

			for (chain = vscr->clip.global_icons; chain != NULL; chain = chain->next) {
				btn = chain->aicon;
				if (btn->xindex == 0 && btn->yindex < 0 && btn->yindex > -vcount)
					vmap[-btn->yindex] = 1;
				else if (btn->yindex == 0 && btn->xindex > 0 && btn->xindex < hcount)
					hmap[btn->xindex] = 1;
			}

			break;
		case C_SW:
		default:
			for (i = 0; i < dock->max_icons; i++) {
				btn = dock->icon_array[i];
				if (!btn)
					continue;

				if (btn->xindex == 0 && btn->yindex < 0 && btn->yindex > -vcount)
					vmap[-btn->yindex] = 1;
				else if (btn->yindex == 0 && btn->xindex < 0 && btn->xindex > -hcount)
					hmap[-btn->xindex] = 1;

			}

			for (chain = vscr->clip.global_icons; chain != NULL; chain = chain->next) {
				btn = chain->aicon;
				if (btn->xindex == 0 && btn->yindex < 0 && btn->yindex > -vcount)
					vmap[-btn->yindex] = 1;
				else if (btn->yindex == 0 && btn->xindex < 0 && btn->xindex > -hcount)
					hmap[-btn->xindex] = 1;
			}
		}

		x = 0;
		y = 0;
		done = 0;
		/* search a vacant slot */
		for (i = 1; i < WMAX(vcount, hcount); i++) {
			if (i < vcount && vmap[i] == 0) {
				/* found a slot */
				x = 0;
				y = i;
				done = 1;
				break;
			} else if (i < hcount && hmap[i] == 0) {
				/* found a slot */
				x = i;
				y = 0;
				done = 1;
				break;
			}
		}
		wfree(vmap);
		wfree(hmap);
		/* If found a slot, translate and return */
		if (done) {
			if (corner == C_NW || corner == C_NE)
				*y_pos = y;
			else
				*y_pos = -y;

			if (corner == C_NE || corner == C_SE)
				*x_pos = x;
			else
				*x_pos = -x;

			return True;
		}
		/* else, try to find a slot somewhere else */
	}

	/* a map of mwidth x mwidth would be enough if we allowed icons to be
	 * placed outside of screen */
	mwidth = (int)ceil(sqrt(dock->max_icons));

	/* In the worst case (the clip is in the corner of the screen),
	 * the amount of icons that fit in the clip is smaller.
	 * Double the map to get a safe value.
	 */
	mwidth += mwidth;

	r = (mwidth - 1) / 2;

	slot_map = wmalloc(mwidth * mwidth);

#define XY2OFS(x,y) (WMAX(abs(x),abs(y)) > r) ? 0 : (((y)+r)*(mwidth)+(x)+r)

	/* mark used slots in the map. If the slot falls outside the map
	 * (for example, when all icons are placed in line), ignore them. */
	for (i = 0; i < dock->max_icons; i++) {
		btn = dock->icon_array[i];
		if (btn)
			slot_map[XY2OFS(btn->xindex, btn->yindex)] = 1;
	}

	for (chain = vscr->clip.global_icons; chain != NULL; chain = chain->next)
		slot_map[XY2OFS(chain->aicon->xindex, chain->aicon->yindex)] = 1;

	/* Find closest slot from the center that is free by scanning the
	 * map from the center to outward in circular passes.
	 * This will not result in a neat layout, but will be optimal
	 * in the sense that there will not be holes left.
	 */
	done = 0;
	for (i = 1; i <= r && !done; i++) {
		int tx, ty;

		/* top and bottom parts of the ring */
		for (x = -i; x <= i && !done; x++) {
			tx = dock->x_pos + x * ICON_SIZE;
			y = -i;
			ty = dock->y_pos + y * ICON_SIZE;
			if (slot_map[XY2OFS(x, y)] == 0 && onScreen(vscr, tx, ty)) {
				*x_pos = x;
				*y_pos = y;
				done = 1;
				break;
			}

			y = i;
			ty = dock->y_pos + y * ICON_SIZE;
			if (slot_map[XY2OFS(x, y)] == 0 && onScreen(vscr, tx, ty)) {
				*x_pos = x;
				*y_pos = y;
				done = 1;
				break;
			}
		}

		/* left and right parts of the ring */
		for (y = -i + 1; y <= i - 1; y++) {
			ty = dock->y_pos + y * ICON_SIZE;
			x = -i;
			tx = dock->x_pos + x * ICON_SIZE;
			if (slot_map[XY2OFS(x, y)] == 0 && onScreen(vscr, tx, ty)) {
				*x_pos = x;
				*y_pos = y;
				done = 1;
				break;
			}

			x = i;
			tx = dock->x_pos + x * ICON_SIZE;
			if (slot_map[XY2OFS(x, y)] == 0 && onScreen(vscr, tx, ty)) {
				*x_pos = x;
				*y_pos = y;
				done = 1;
				break;
			}
		}
	}

	wfree(slot_map);

#undef XY2OFS

	return done;
}

static void moveDock(WDock *dock, int new_x, int new_y)
{
	WAppIcon *btn;
	WDrawerChain *dc;
	int i;

	if (dock->type == WM_DOCK)
		for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
			moveDock(dc->adrawer, new_x, dc->adrawer->y_pos - dock->y_pos + new_y);

	dock->x_pos = new_x;
	dock->y_pos = new_y;

	for (i = 0; i < dock->max_icons; i++) {
		btn = dock->icon_array[i];
		if (btn) {
			btn->x_pos = new_x + btn->xindex * ICON_SIZE;
			btn->y_pos = new_y + btn->yindex * ICON_SIZE;
			XMoveWindow(dpy, btn->icon->core->window, btn->x_pos, btn->y_pos);
		}
	}
}

static void swapDock(WDock *dock)
{
	virtual_screen *vscr = dock->vscr;
	WScreen *scr = vscr->screen_ptr;
	WAppIcon *btn;
	int x, i;

	if (dock->on_right_side)
		x = dock->x_pos = scr->scr_width - ICON_SIZE - DOCK_EXTRA_SPACE;
	else
		x = dock->x_pos = DOCK_EXTRA_SPACE;

	swapDrawers(vscr, x);

	for (i = 0; i < dock->max_icons; i++) {
		btn = dock->icon_array[i];
		if (btn) {
			btn->x_pos = x;
			XMoveWindow(dpy, btn->icon->core->window, btn->x_pos, btn->y_pos);
		}
	}

	wScreenUpdateUsableArea(vscr);
}

static pid_t execCommand(WAppIcon *btn, const char *command, WSavedState *state)
{
	virtual_screen *vscr = btn->icon->vscr;
	WScreen *scr = vscr->screen_ptr;
	pid_t pid;
	char **argv;
	int argc;
	char *cmdline;

	cmdline = ExpandOptions(vscr, command);

	if (scr->flags.dnd_data_convertion_status || !cmdline) {
		if (cmdline)
			wfree(cmdline);

		if (state)
			wfree(state);

		return 0;
	}

	wtokensplit(cmdline, &argv, &argc);

	if (!argc) {
		if (cmdline)
			wfree(cmdline);

		if (state)
			wfree(state);

		return 0;
	}

	pid = execute_command2(vscr, argv, argc);
	wtokenfree(argv, argc);

	if (pid > 0) {
		if (!state) {
			state = wmalloc(sizeof(WSavedState));
			state->hidden = -1;
			state->miniaturized = -1;
			state->shaded = -1;
			if (btn->dock == vscr->dock.dock || btn->dock->type == WM_DRAWER || btn->omnipresent)
				state->workspace = -1;
			else
				state->workspace = vscr->workspace.current;
		}

		wWindowAddSavedState(btn->wm_instance, btn->wm_class, cmdline, pid, state);
		wAddDeathHandler(pid, (WDeathHandler *) trackDeadProcess, btn->dock);
	} else if (state) {
		wfree(state);
	}

	wfree(cmdline);
	return pid;
}

void wDockHideIcons(WDock *dock)
{
	int i;

	if (dock == NULL)
		return;

	for (i = 1; i < dock->max_icons; i++) {
		if (dock->icon_array[i])
			XUnmapWindow(dpy, dock->icon_array[i]->icon->core->window);
	}
	dock->mapped = 0;

	dockIconPaint(dock->icon_array[0]);
}

void wDockShowIcons(WDock *dock)
{
	int i;
	WAppIcon *btn;

	if (dock == NULL)
		return;

	btn = dock->icon_array[0];
	moveDock(dock, btn->x_pos, btn->y_pos);

	/* Deleting any change in stacking level, this function is now only about
	   mapping icons */

	if (!dock->collapsed) {
		for (i = 1; i < dock->max_icons; i++) {
			if (dock->icon_array[i])
				XMapWindow(dpy, dock->icon_array[i]->icon->core->window);
		}
	}
	dock->mapped = 1;

	dockIconPaint(btn);
}

void wDockLower(WDock *dock)
{
	int i;
	WDrawerChain *dc;

	if (dock->type == WM_DOCK)
		for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
			wDockLower(dc->adrawer);

	for (i = 0; i < dock->max_icons; i++)
		if (dock->icon_array[i])
			wLowerFrame(dock->icon_array[i]->icon->vscr, dock->icon_array[i]->icon->core);
}

void wDockRaise(WDock *dock)
{
	int i;
	WDrawerChain *dc;

	for (i = dock->max_icons - 1; i >= 0; i--)
		if (dock->icon_array[i])
			wRaiseFrame(dock->icon_array[i]->icon->vscr, dock->icon_array[i]->icon->core);

	if (dock->type == WM_DOCK)
		for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
			wDockRaise(dc->adrawer);
}

void wDockRaiseLower(WDock *dock)
{
	if (!dock->icon_array[0]->icon->core->stacking->above
	    || (dock->icon_array[0]->icon->core->stacking->window_level
		!= dock->icon_array[0]->icon->core->stacking->above->stacking->window_level))
		wDockLower(dock);
	else
		wDockRaise(dock);
}

void wDockFinishLaunch(WAppIcon *icon)
{
	icon->launching = 0;
	icon->relaunching = 0;
	dockIconPaint(icon);
}

WAppIcon *wDockFindIconForWindow(WDock *dock, Window window)
{
	WAppIcon *icon;
	int i;

	for (i = 0; i < dock->max_icons; i++) {
		icon = dock->icon_array[i];
		if (icon && icon->main_window == window)
			return icon;
	}
	return NULL;
}

static int find_win_in_dock(WDock *dock, Window window, char *wm_class,
			    char *wm_instance, char *command, Bool firstPass)
{
	WAppIcon *icon;
	int i;

	for (i = 0; i < dock->max_icons; i++) {
		icon = dock->icon_array[i];
		if (!icon)
			continue;

		/* app is already attached to icon */
		if (icon->main_window == window)
			return 1;

		if (!icon->wm_instance && !icon->wm_class)
			continue;

		if (!icon->launching && icon->running)
			continue;

		if (icon->wm_instance && wm_instance && strcmp(icon->wm_instance, wm_instance) != 0)
			continue;

		if (icon->wm_class && wm_class && strcmp(icon->wm_class, wm_class) != 0)
			continue;

		if (firstPass && command && strcmp(icon->command, command) != 0)
			continue;

		if (!icon->relaunching) {
			WApplication *wapp;

			/* Possibly an application that was docked with dockit,
			 * but the user did not update WMState to indicate that
			 * it was docked by force */
			wapp = wApplicationOf(window);
			if (!wapp) {
				icon->forced_dock = 1;
				icon->running = 0;
			}

			if (!icon->forced_dock)
				icon->main_window = window;
		}

		if (!wPreferences.no_animations && !icon->launching &&
		    !w_global.startup.phase1 && !dock->collapsed)
			move_appicon_to_dock(dock->vscr, icon, wm_class, wm_instance);

		wDockFinishLaunch(icon);
		return 1;
	}

	return 0;
}

void wDockTrackWindowLaunch(WDock *dock, Window window)
{
	char *wm_class, *wm_instance, *command = NULL;
	Bool found = False;

	if (!PropGetWMClass(window, &wm_class, &wm_instance)) {
		wfree(wm_class);
		wfree(wm_instance);
		return;
	}

	command = GetCommandForWindow(window);

	found = find_win_in_dock(dock, window, wm_class, wm_instance, command, True);
	if (!found)
		find_win_in_dock(dock, window, wm_class, wm_instance, command, False);

	if (command)
		wfree(command);

	if (wm_class)
		wfree(wm_class);

	if (wm_instance)
		wfree(wm_instance);
}

static void trackDeadProcess(pid_t pid, unsigned int status, WDock *client_data)
{
	WDock *dock = (WDock *) client_data;
	WAppIcon *icon;
	int i;

	for (i = 0; i < dock->max_icons; i++) {
		icon = dock->icon_array[i];
		if (!icon)
			continue;

		if (icon->launching && icon->pid == pid) {
			if (!icon->relaunching) {
				icon->running = 0;
				icon->main_window = None;
			}

			wDockFinishLaunch(icon);
			icon->pid = 0;
			if (status == 111) {
				char msg[PATH_MAX];
				char *cmd;

#ifdef USE_DOCK_XDND
				if (icon->drop_launch)
					cmd = icon->dnd_command;
				else
#endif
				if (icon->paste_launch)
					cmd = icon->paste_command;
				else
					cmd = icon->command;

				snprintf(msg, sizeof(msg), _("Could not execute command \"%s\""), cmd);

				wMessageDialog(dock->vscr, _("Error"), msg, _("OK"), NULL, NULL);
			}
			break;
		}
	}
}

/* This function is called when the dock switches state between
 * "normal" (including auto-raise/lower) and "keep on top". It is
 * therefore clearly distinct from wDockLower/Raise, which are called
 * each time a not-kept-on-top dock is lowered/raised. */
void toggleLowered(WDock *dock)
{
	WAppIcon *tmp;
	WDrawerChain *dc;
	int newlevel, i;

	if (!dock->lowered) {
		newlevel = WMNormalLevel;
		dock->lowered = 1;
	} else {
		newlevel = WMDockLevel;
		dock->lowered = 0;
	}

	for (i = 0; i < dock->max_icons; i++) {
		tmp = dock->icon_array[i];
		if (!tmp)
			continue;

		ChangeStackingLevel(tmp->icon->vscr, tmp->icon->core, newlevel);

		/* When the dock is no longer "on top", explicitly lower it as well.
		 * It saves some CPU cycles (probably) to do it ourselves here
		 * rather than calling wDockLower at the end of toggleLowered */
		if (dock->lowered)
			wLowerFrame(tmp->icon->vscr, tmp->icon->core);
	}

	if (dock->type == WM_DOCK) {
		for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
			toggleLowered(dc->adrawer);

		wScreenUpdateUsableArea(dock->vscr);
	}
}

void toggleCollapsed(WDock *dock)
{
	if (dock->collapsed) {
		dock->collapsed = 0;
		wDockShowIcons(dock);
	} else {
		dock->collapsed = 1;
		wDockHideIcons(dock);
	}
}

/******************************************************************/

void handleDockMove(WDock *dock, WAppIcon *aicon, XEvent *event)
{
	virtual_screen *vscr = dock->vscr;
	WScreen *scr = vscr->screen_ptr;
	int ofs_x = event->xbutton.x, ofs_y = event->xbutton.y;
	WIcon *icon = aicon->icon;
	WAppIcon *tmpaicon;
	WDrawerChain *dc;
	int x = aicon->x_pos, y = aicon->y_pos;;
	int shad_x = x, shad_y = y;
	XEvent ev;
	int grabbed = 0, done, previously_on_right, now_on_right, previous_x_pos, i;
	Pixmap ghost = None;
	int superfluous = wPreferences.superfluous;	/* we catch it to avoid problems */

	if (XGrabPointer(dpy, aicon->icon->core->window, True, ButtonMotionMask
			 | ButtonReleaseMask | ButtonPressMask, GrabModeAsync,
			 GrabModeAsync, None, None, CurrentTime) != GrabSuccess)
		wwarning("pointer grab failed for dock move");

	if (dock->type == WM_DRAWER) {
		Window wins[2];
		wins[0] = icon->core->window;
		wins[1] = scr->dock_shadow;
		XRestackWindows(dpy, wins, 2);
		XMoveResizeWindow(dpy, scr->dock_shadow, aicon->x_pos, aicon->y_pos,
				ICON_SIZE, ICON_SIZE);

		if (superfluous) {
			if (icon->pixmap!=None)
				ghost = MakeGhostIcon(vscr, icon->pixmap);
			else
				ghost = MakeGhostIcon(vscr, icon->core->window);

			XSetWindowBackgroundPixmap(dpy, scr->dock_shadow, ghost);
			XClearWindow(dpy, scr->dock_shadow);
		}

		XMapWindow(dpy, scr->dock_shadow);
	}

	previously_on_right = now_on_right = dock->on_right_side;
	previous_x_pos = dock->x_pos;
	done = 0;
	while (!done) {
		WMMaskEvent(dpy, PointerMotionMask | ButtonReleaseMask | ButtonPressMask
			    | ButtonMotionMask | ExposureMask | EnterWindowMask, &ev);
		switch (ev.type) {
		case Expose:
			WMHandleEvent(&ev);
			break;

                case EnterNotify:
                        /* It means the cursor moved so fast that it entered
                         * something else (if moving slowly, it would have
                         * stayed in the dock that is being moved. Ignore such
                         * "spurious" EnterNotifiy's */
                        break;

		case MotionNotify:
			if (!grabbed) {
				if (abs(ofs_x - ev.xmotion.x) >= MOVE_THRESHOLD
				    || abs(ofs_y - ev.xmotion.y) >= MOVE_THRESHOLD) {
					XChangeActivePointerGrab(dpy, ButtonMotionMask
								 | ButtonReleaseMask | ButtonPressMask,
								 wPreferences.cursor[WCUR_MOVE], CurrentTime);
					grabbed = 1;
				}
				break;
			}

			switch (dock->type) {
			case WM_CLIP:
				x = ev.xmotion.x_root - ofs_x;
				y = ev.xmotion.y_root - ofs_y;
				wScreenKeepInside(vscr, &x, &y, ICON_SIZE, ICON_SIZE);
				moveDock(dock, x, y);
				break;
			case WM_DOCK:
				x = ev.xmotion.x_root - ofs_x;
				y = ev.xmotion.y_root - ofs_y;
				if (previously_on_right)
					now_on_right = (ev.xmotion.x_root >= previous_x_pos - ICON_SIZE);
				else
					now_on_right = (ev.xmotion.x_root > previous_x_pos + ICON_SIZE * 2);

				if (now_on_right != dock->on_right_side) {
					dock->on_right_side = now_on_right;
					swapDock(dock);
					wArrangeIcons(vscr, False);
				}

				/* Also perform the vertical move */
				wScreenKeepInside(vscr, &x, &y, ICON_SIZE, ICON_SIZE);
				moveDock(dock, dock->x_pos, y);
				if (wPreferences.flags.wrap_appicons_in_dock) {
					for (i = 0; i < dock->max_icons; i++) {
						int new_y, new_index, j, ok;
						tmpaicon = dock->icon_array[i];
						if (tmpaicon == NULL)
							continue;

						if (onScreen(vscr, tmpaicon->x_pos, tmpaicon->y_pos))
							continue;

						new_y = (tmpaicon->y_pos + ICON_SIZE * dock->max_icons) % (ICON_SIZE * dock->max_icons);
						new_index = (new_y - dock->y_pos) / ICON_SIZE;
						if (!onScreen(vscr, tmpaicon->x_pos, new_y))
							continue;

						ok = 1;
						for (j = 0; j < dock->max_icons; j++) {
							if (dock->icon_array[j] != NULL &&
							    dock->icon_array[j]->yindex == new_index) {
								ok = 0;
								break;
							}
						}

						if (!ok || getDrawer(vscr, new_index) != NULL)
							continue;

						wDockReattachIcon(dock, tmpaicon, tmpaicon->xindex, new_index);
					}

					for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next) {
						int new_y, new_index, j, ok;
						tmpaicon = dc->adrawer->icon_array[0];
						if (onScreen(vscr, tmpaicon->x_pos, tmpaicon->y_pos))
							continue;

						new_y = (tmpaicon->y_pos + ICON_SIZE * dock->max_icons) % (ICON_SIZE * dock->max_icons);
						new_index = (new_y - dock->y_pos) / ICON_SIZE;
						if (!onScreen(vscr, tmpaicon->x_pos, new_y))
							continue;

						ok = 1;
						for (j = 0; j < dock->max_icons; j++) {
							if (dock->icon_array[j] != NULL &&
							    dock->icon_array[j]->yindex == new_index) {
								ok = 0;
								break;
							}
						}

						if (!ok || getDrawer(vscr, new_index) != NULL)
							continue;

						moveDock(dc->adrawer, tmpaicon->x_pos, new_y);
					}
				}
				break;
			case WM_DRAWER:
			{
				WDock *real_dock = vscr->dock.dock;
				Bool snapped;
				int ix, iy;
				x = ev.xmotion.x_root - ofs_x;
				y = ev.xmotion.y_root - ofs_y;
				snapped = wDockSnapIcon(real_dock, aicon, x, y, &ix, &iy, True);
				if (snapped) {
					shad_x = real_dock->x_pos + ix * wPreferences.icon_size;
					shad_y = real_dock->y_pos + iy * wPreferences.icon_size;
					XMoveWindow(dpy, scr->dock_shadow, shad_x, shad_y);
				}
				moveDock(dock, x, y);
				break;
			}
			}
			break;

		case ButtonPress:
			break;

		case ButtonRelease:
			if (ev.xbutton.button != event->xbutton.button)
				break;

			XUngrabPointer(dpy, CurrentTime);
			if (dock->type == WM_DRAWER) {
				Window wins[dock->icon_count];
				int offset_index;

				/*
				 * When the dock is on the Right side, the index of the icons are negative to
				 * reflect the fact that they are placed on the other side of the dock; we use
				 * an offset here so we can have an always positive index for the storage in
				 * the 'wins' array.
				 */
				if (dock->on_right_side)
					offset_index = dock->icon_count - 1;
				else
					offset_index = 0;

				for (i = 0; i < dock->max_icons; i++) {
					tmpaicon = dock->icon_array[i];
					if (tmpaicon == NULL)
						continue;

					wins[tmpaicon->xindex + offset_index] = tmpaicon->icon->core->window;
				}

				slide_windows(wins, dock->icon_count,
					(dock->on_right_side ? x - (dock->icon_count - 1) * ICON_SIZE : x),
					y,
					(dock->on_right_side ? shad_x - (dock->icon_count - 1) * ICON_SIZE : shad_x),
					shad_y);

				XUnmapWindow(dpy, scr->dock_shadow);
				moveDock(dock, shad_x, shad_y);
				XResizeWindow(dpy, scr->dock_shadow, ICON_SIZE, ICON_SIZE);
			}

			if (dock->type == WM_CLIP) {
				for (i = 0; i < vscr->workspace.count; i++) {
					if ((vscr->workspace.array[i]) && (vscr->workspace.array[i]->clip)) {
						vscr->workspace.array[i]->clip->x_pos = x;
						vscr->workspace.array[i]->clip->y_pos = y;
					}
				}
			}

			done = 1;
			break;
		}
	}

	if (superfluous) {
		if (ghost != None)
			XFreePixmap(dpy, ghost);

		XSetWindowBackground(dpy, scr->dock_shadow, scr->white_pixel);
	}
}




void wSlideAppicons(WAppIcon **appicons, int n, int to_the_left)
{
	int i;
	int leftmost = -1, min_index = 9999, from_x = -1; // leftmost and from_x initialized to avoid warning
	Window wins[n];
	WAppIcon *aicon;

	if (n < 1)
		return;

	for (i = 0; i < n; i++) {
		aicon = appicons[i];
		aicon->xindex += (to_the_left ? -1 : +1);
		if (aicon->xindex < min_index) {
			min_index = aicon->xindex;
			leftmost = i;
			from_x = aicon->x_pos;
		}
		aicon->x_pos += (to_the_left ? -ICON_SIZE : +ICON_SIZE);
	}

	for (i = 0; i < n; i++) {
		aicon = appicons[i];
		wins[aicon->xindex - min_index] = aicon->icon->core->window;
	}

	aicon = appicons[leftmost];
	slide_windows(wins, n, from_x, aicon->y_pos, aicon->x_pos, aicon->y_pos);
}


