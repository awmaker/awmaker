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

/**** Local variables ****/
#define CLIP_REWIND       1
#define CLIP_IDLE         0
#define CLIP_FORWARD      2

#define MOD_MASK wPreferences.modifier_mask
#define ICON_SIZE wPreferences.icon_size

enum
{
	OM_KEEP_ON_TOP,
	OM_COLLAPSED,
	OM_AUTO_COLLAPSED,
	OM_AUTORAISE,
	OM_AUTOATTRACT
};

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

static void dockIconPaint(WAppIcon *btn);

static void dock_icon_mouse_down(WObjDescriptor *desc, XEvent *event);
static void clip_icon_mouse_down(WObjDescriptor *desc, XEvent *event);
static void drawer_icon_mouse_down(WObjDescriptor *desc, XEvent *event);

static pid_t execCommand(WAppIcon *btn, const char *command, WSavedState *state);

static void trackDeadProcess(pid_t pid, unsigned int status, WDock *dock);

static int getClipButton(int px, int py);

static void toggleLowered(WDock *dock);

static void toggleCollapsed(WDock *dock);

static void dock_icon_expose(WObjDescriptor *desc, XEvent *event);
static void clip_icon_expose(WObjDescriptor *desc, XEvent *event);
static void drawer_icon_expose(WObjDescriptor *desc, XEvent *event);

static void dock_leave(WDock *dock);
static void clip_leave(WDock *dock);
static void drawer_leave(WDock *dock);

static void handleClipChangeWorkspace(virtual_screen *vscr, XEvent *event);

static void dock_enter_notify(WObjDescriptor *desc, XEvent *event);
static void clip_enter_notify(WObjDescriptor *desc, XEvent *event);
static void drawer_enter_notify(WObjDescriptor *desc, XEvent *event);

static void dock_leave_notify(WObjDescriptor *desc, XEvent *event);
static void clip_leave_notify(WObjDescriptor *desc, XEvent *event);
static void drawer_leave_notify(WObjDescriptor *desc, XEvent *event);

static void clip_autocollapse(void *cdata);
static void drawer_autocollapse(void *cdata);

static void clipAutoExpand(void *cdata);
static void launchDockedApplication(WAppIcon *btn, Bool withSelection);

static void clipAutoLower(void *cdata);
static void clipAutoRaise(void *cdata);

static void drawerIconExpose(WObjDescriptor *desc, XEvent *event);
static void removeDrawerCallback(WMenu *menu, WMenuEntry *entry);
static void drawerAppendToChain(WDock *drawer);
static char *findUniqueName(virtual_screen *vscr, const char *instance_basename);
static void addADrawerCallback(WMenu *menu, WMenuEntry *entry);
static void swapDrawers(virtual_screen *vscr, int new_x);
static WDock *getDrawer(virtual_screen *vscr, int y_index);
static int indexOfHole(WDock *drawer, WAppIcon *moving_aicon, int redocking);
static void drawerConsolidateIcons(WDock *drawer);
static void drawerRestoreState_map(WDock *drawer);

static int onScreen(virtual_screen *scr, int x, int y);

static void moveDock(WDock *dock, int new_x, int new_y);

static void save_application_list(WMPropList *state, WMPropList *list, virtual_screen *vscr);

static void restore_dock_position(WDock *dock, WMPropList *state);
static void restore_clip_position(WDock *dock, WMPropList *state);
static void restore_drawer_position(WDock *drawer, WMPropList *state);
static void restore_clip_position_map(WDock *dock);
static void restore_state_lowered(WDock *dock, WMPropList *state);
static void restore_state_collapsed(WDock *dock, WMPropList *state);
static void restore_state_autoraise(WDock *dock, WMPropList *state);
static void dock_set_attacheddocks(WDock *dock, WMPropList *state);
static int dock_set_attacheddocks_do(WDock *dock, WMPropList *apps);
static int drawer_set_attacheddocks_do(WDock *dock, WMPropList *apps);
static void clip_set_attacheddocks(WDock *dock, WMPropList *state);
static void dock_unset_attacheddocks(WDock *dock);
static int restore_state_autocollapsed(WDock *dock, WMPropList *state);
static int restore_state_autoattracticons(WDock *dock, WMPropList *state);

static void wDockDoAutoLaunch(WDock *dock, int workspace);

static void dock_autolaunch(int vscrno);
static void clip_autolaunch(int vscrno);
static void drawers_autolaunch(int vscrno);

static void clip_button3_menu(WObjDescriptor *desc, XEvent *event);
static void clip_button2_menu(WObjDescriptor *desc, XEvent *event);

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

static void toggleLoweredCallback(WMenu *menu, WMenuEntry *entry)
{
	assert(entry->clientdata != NULL);

	toggleLowered(entry->clientdata);

	entry->flags.indicator_on = !(((WDock *) entry->clientdata)->lowered);

	wMenuPaint(menu);
}

static int matchWindow(const void *item, const void *cdata)
{
	return (((WFakeGroupLeader *) item)->leader == (Window) cdata);
}

static void killCallback(WMenu *menu, WMenuEntry *entry)
{
	virtual_screen *vscr = menu->vscr;
	WScreen *scr = vscr->screen_ptr;
	WAppIcon *icon;
	WFakeGroupLeader *fPtr;
	char *buffer, *shortname, **argv;
	int argc;

	if (!WCHECK_STATE(WSTATE_NORMAL))
		return;

	assert(entry->clientdata != NULL);

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
static int numberOfSelectedIcons(WDock *dock)
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

static WMArray *getSelected(WDock *dock)
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

static void removeIcons(WMArray *icons, WDock *dock)
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

static void keepIconsCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *clickedIcon = (WAppIcon *) entry->clientdata;
	WDock *dock;
	WAppIcon *aicon;
	WMArray *selectedIcons;
	WMArrayIterator it;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	assert(clickedIcon != NULL);
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

static void toggleAutoAttractCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock = (WDock *) entry->clientdata;
	virtual_screen *vscr = dock->vscr;
	int i;

	assert(entry->clientdata != NULL);

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

static void selectCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *icon = (WAppIcon *) entry->clientdata;

	assert(icon != NULL);

	wIconSelect(icon->icon);

	wMenuPaint(menu);
}

static void attractIconsCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *clickedIcon = (WAppIcon *) entry->clientdata;
	WDock *clip; /* clip... is a WM_CLIP or a WM_DRAWER */
	WAppIcon *aicon;
	int x, y, x_pos, y_pos;
	Bool update_icon = False;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	assert(entry->clientdata != NULL);
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

static void selectIconsCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *clickedIcon = (WAppIcon *) entry->clientdata;
	WDock *dock;
	WMArray *selectedIcons;
	WMArrayIterator iter;
	WAppIcon *btn;
	int i;

	assert(clickedIcon != NULL);
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

static void toggleCollapsedCallback(WMenu *menu, WMenuEntry *entry)
{
	assert(entry->clientdata != NULL);

	toggleCollapsed(entry->clientdata);

	entry->flags.indicator_on = ((WDock *) entry->clientdata)->collapsed;

	wMenuPaint(menu);
}

static void toggleAutoCollapseCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock;
	assert(entry->clientdata != NULL);

	dock = (WDock *) entry->clientdata;

	dock->auto_collapse = !dock->auto_collapse;

	entry->flags.indicator_on = ((WDock *) entry->clientdata)->auto_collapse;

	wMenuPaint(menu);
}

static void toggleAutoRaiseLower(WDock *dock)
{
	WDrawerChain *dc;

	dock->auto_raise_lower = !dock->auto_raise_lower;
	if (dock->type == WM_DOCK)
		for (dc = dock->vscr->drawer.drawers; dc != NULL; dc = dc->next)
			toggleAutoRaiseLower(dc->adrawer);
}

static void toggleAutoRaiseLowerCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	assert(entry->clientdata != NULL);

	dock = (WDock *) entry->clientdata;

	toggleAutoRaiseLower(dock);

	entry->flags.indicator_on = ((WDock *) entry->clientdata)->auto_raise_lower;

	wMenuPaint(menu);
}

static void launchCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *btn = (WAppIcon *) entry->clientdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	launchDockedApplication(btn, False);
}

static void settingsCallback(WMenu *menu, WMenuEntry *entry)
{
	WAppIcon *btn = (WAppIcon *) entry->clientdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	if (btn->editing)
		return;
	ShowDockAppSettingsPanel(btn);
}

static void hideCallback(WMenu *menu, WMenuEntry *entry)
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

static void unhideHereCallback(WMenu *menu, WMenuEntry *entry)
{
	WApplication *wapp;
	WAppIcon *btn = (WAppIcon *) entry->clientdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	wapp = wApplicationOf(btn->icon->owner->main_window);

	wUnhideApplication(wapp, False, True);
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

static void launchDockedApplication(WAppIcon *btn, Bool withSelection)
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

static void updateOptionsMenu(WDock *dock, WMenu *menu)
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

static WDock *dock_create_core(virtual_screen *vscr)
{
	WDock *dock;

	make_keys();

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

	wcore_map_toplevel(wcore, vscr, 0, 0, icon->width, icon->height, 0,
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

void drawer_map(WDock *dock, virtual_screen *vscr)
{
	WAppIcon *btn = dock->icon_array[0];
	WIcon *icon = btn->icon;
	WCoreWindow *wcore = icon->core;
	WScreen *scr = vscr->screen_ptr;

	dock->x_pos = scr->scr_width - ICON_SIZE - DOCK_EXTRA_SPACE;

	btn->x_pos = dock->x_pos;
	btn->y_pos = dock->y_pos;

	wcore_map_toplevel(wcore, vscr, btn->x_pos, btn->y_pos,
			   icon->width, icon->height, 0,
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

void drawer_unmap(WDock *dock)
{
	WAppIcon *btn = dock->icon_array[0];

	XUnmapWindow(dpy, btn->icon->core->window);
	RemoveFromStackList(btn->icon->vscr, btn->icon->core);
	unmap_icon_image(btn->icon);
	wcore_unmap(btn->icon->core);
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

static void dockIconPaint(WAppIcon *btn)
{
	virtual_screen *vscr = btn->icon->vscr;

	if (btn == vscr->clip.icon) {
		wClipIconPaint(btn);
	} else if (wIsADrawer(btn)) {
		wDrawerIconPaint(btn);
	} else {
		wAppIconPaint(btn);
		save_appicon(btn);
	}
}

static WMPropList *make_icon_state(WAppIcon *btn)
{
	virtual_screen *vscr = btn->icon->vscr;
	WMPropList *node = NULL;
	WMPropList *command, *autolaunch, *lock, *name, *forced;
	WMPropList *position, *buggy, *omnipresent;
	char *tmp;
	char buffer[64];

	if (btn) {
		if (!btn->command)
			command = WMCreatePLString("-");
		else
			command = WMCreatePLString(btn->command);

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

		node = WMCreatePLDictionary(dCommand, command,
					    dName, name,
					    dAutoLaunch, autolaunch,
					    dLock, lock,
					    dForced, forced, dBuggyApplication, buggy, dPosition, position, NULL);
		WMReleasePropList(command);
		WMReleasePropList(name);
		WMReleasePropList(position);

		omnipresent = btn->omnipresent ? dYes : dNo;
		if (btn->dock != vscr->dock.dock && (btn->xindex != 0 || btn->yindex != 0))
			WMPutInPLDictionary(node, dOmnipresent, omnipresent);

#ifdef USE_DOCK_XDND			/* was OFFIX */
		if (btn->dnd_command) {
			command = WMCreatePLString(btn->dnd_command);
			WMPutInPLDictionary(node, dDropCommand, command);
			WMReleasePropList(command);
		}
#endif

		if (btn->paste_command) {
			command = WMCreatePLString(btn->paste_command);
			WMPutInPLDictionary(node, dPasteCommand, command);
			WMReleasePropList(command);
		}
	}

	return node;
}

static WMPropList *dock_save_state(WDock *dock)
{
	virtual_screen *vscr = dock->vscr;
	int i;
	WMPropList *icon_info;
	WMPropList *list = NULL, *dock_state = NULL;
	WMPropList *value;
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

	dock_state = WMCreatePLDictionary(dApplications, list, NULL);

	/* Save with the same screen_id. See get_application_list() */
	save_application_list(dock_state, list, vscr);

	snprintf(buffer, sizeof(buffer), "%i,%i", (dock->on_right_side ? -ICON_SIZE : 0), dock->y_pos);
	value = WMCreatePLString(buffer);
	WMPutInPLDictionary(dock_state, dPosition, value);
	WMReleasePropList(value);
	WMReleasePropList(list);

	value = (dock->lowered ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dLowered, value);

	value = (dock->auto_raise_lower ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dAutoRaiseLower, value);

	return dock_state;
}

static WMPropList *clip_save_state(WDock *dock)
{
	virtual_screen *vscr = dock->vscr;
	int i;
	WMPropList *icon_info;
	WMPropList *list = NULL, *dock_state = NULL;
	WMPropList *value;
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

	dock_state = WMCreatePLDictionary(dApplications, list, NULL);
	WMReleasePropList(list);

	value = (dock->collapsed ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dCollapsed, value);

	value = (dock->auto_collapse ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dAutoCollapse, value);

	value = (dock->attract_icons ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dAutoAttractIcons, value);

	value = (dock->lowered ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dLowered, value);

	value = (dock->auto_raise_lower ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dAutoRaiseLower, value);

	/* TODO: Check why in the last workspace, clip is at x=0, y=0
	 * Save the Clip position using the Clip in workspace 1
	 */
	snprintf(buffer, sizeof(buffer), "%i,%i",
		 vscr->workspace.array[0]->clip->x_pos,
		 vscr->workspace.array[0]->clip->y_pos);
	value = WMCreatePLString(buffer);
	WMPutInPLDictionary(dock_state, dPosition, value);
	WMReleasePropList(value);

	return dock_state;
}

static WMPropList *drawer_save_state(WDock *dock)
{
	int i;
	WMPropList *icon_info;
	WMPropList *list = NULL, *dock_state = NULL;
	WMPropList *value;

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

	dock_state = WMCreatePLDictionary(dApplications, list, NULL);
	WMReleasePropList(list);

	value = (dock->collapsed ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dCollapsed, value);

	value = (dock->auto_collapse ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dAutoCollapse, value);

	value = (dock->attract_icons ? dYes : dNo);
	WMPutInPLDictionary(dock_state, dAutoAttractIcons, value);

	return dock_state;
}

void wDockSaveState(virtual_screen *vscr, WMPropList *old_state)
{
	WMPropList *dock_state;
	WMPropList *keys;

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

	WMPutInPLDictionary(w_global.session_state, dDock, dock_state);
	WMReleasePropList(dock_state);
}

WMPropList *wClipSaveWorkspaceState(virtual_screen *vscr, int workspace)
{
	return clip_save_state(vscr->workspace.array[workspace]->clip);
}

static Bool getBooleanDockValue(WMPropList *value, WMPropList *key)
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

static WAppIcon *restore_dock_icon_state(virtual_screen *vscr, WMPropList *info, int index)
{
	WAppIcon *aicon;
	WMPropList *cmd, *value;
	char *wclass, *winstance, *command;

	cmd = WMGetFromPLDictionary(info, dCommand);
	if (!cmd || !WMIsPLString(cmd))
		return NULL;

	/* parse window name */
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

#ifdef USE_DOCK_XDND			/* was OFFIX */
	cmd = WMGetFromPLDictionary(info, dDropCommand);
	if (cmd)
		aicon->dnd_command = wstrdup(WMGetFromPLString(cmd));
#endif

	cmd = WMGetFromPLDictionary(info, dPasteCommand);
	if (cmd)
		aicon->paste_command = wstrdup(WMGetFromPLString(cmd));

	/* check auto launch */
	value = WMGetFromPLDictionary(info, dAutoLaunch);

	aicon->auto_launch = getBooleanDockValue(value, dAutoLaunch);

	/* check lock */
	value = WMGetFromPLDictionary(info, dLock);

	aicon->lock = getBooleanDockValue(value, dLock);

	/* check if it wasn't normally docked */
	value = WMGetFromPLDictionary(info, dForced);

	aicon->forced_dock = getBooleanDockValue(value, dForced);

	/* check if we can rely on the stuff in the app */
	value = WMGetFromPLDictionary(info, dBuggyApplication);

	aicon->buggy_app = getBooleanDockValue(value, dBuggyApplication);

	/* get position in the dock */
	value = WMGetFromPLDictionary(info, dPosition);
	if (value && WMIsPLString(value)) {
		if (sscanf(WMGetFromPLString(value), "%hi,%hi", &aicon->xindex, &aicon->yindex) != 2)
			wwarning(_("bad value in docked icon state info %s"), WMGetFromPLString(dPosition));
	} else {
		aicon->yindex = index;
	}

	aicon->xindex = 0;

	/* check if icon is omnipresent */
	value = WMGetFromPLDictionary(info, dOmnipresent);

	aicon->omnipresent = getBooleanDockValue(value, dOmnipresent);

	aicon->running = 0;
	aicon->docked = 1;

	return aicon;
}

static WAppIcon *restore_clip_icon_state(virtual_screen *vscr, WMPropList *info, int index)
{
	WAppIcon *aicon;
	WMPropList *cmd, *value;
	char *wclass, *winstance, *command;

	cmd = WMGetFromPLDictionary(info, dCommand);
	if (!cmd || !WMIsPLString(cmd))
		return NULL;

	/* parse window name */
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
	cmd = WMGetFromPLDictionary(info, dDropCommand);
	if (cmd)
		aicon->dnd_command = wstrdup(WMGetFromPLString(cmd));
#endif

	cmd = WMGetFromPLDictionary(info, dPasteCommand);
	if (cmd)
		aicon->paste_command = wstrdup(WMGetFromPLString(cmd));

	/* check auto launch */
	value = WMGetFromPLDictionary(info, dAutoLaunch);

	aicon->auto_launch = getBooleanDockValue(value, dAutoLaunch);

	/* check lock */
	value = WMGetFromPLDictionary(info, dLock);

	aicon->lock = getBooleanDockValue(value, dLock);

	/* check if it wasn't normally docked */
	value = WMGetFromPLDictionary(info, dForced);

	aicon->forced_dock = getBooleanDockValue(value, dForced);

	/* check if we can rely on the stuff in the app */
	value = WMGetFromPLDictionary(info, dBuggyApplication);

	aicon->buggy_app = getBooleanDockValue(value, dBuggyApplication);

	/* get position in the dock */
	value = WMGetFromPLDictionary(info, dPosition);
	if (value && WMIsPLString(value)) {
		if (sscanf(WMGetFromPLString(value), "%hi,%hi", &aicon->xindex, &aicon->yindex) != 2)
			wwarning(_("bad value in docked icon state info %s"), WMGetFromPLString(dPosition));
	} else {
		aicon->yindex = index;
		aicon->xindex = 0;
	}

	/* check if icon is omnipresent */
	value = WMGetFromPLDictionary(info, dOmnipresent);

	aicon->omnipresent = getBooleanDockValue(value, dOmnipresent);

	aicon->running = 0;
	aicon->docked = 1;

	return aicon;
}

static WAppIcon *restore_drawer_icon_state(virtual_screen *vscr, WMPropList *info, int index)
{
	WAppIcon *aicon;
	WMPropList *cmd, *value;
	char *wclass, *winstance, *command;

	cmd = WMGetFromPLDictionary(info, dCommand);
	if (!cmd || !WMIsPLString(cmd))
		return NULL;

	/* parse window name */
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

#ifdef USE_DOCK_XDND			/* was OFFIX */
	cmd = WMGetFromPLDictionary(info, dDropCommand);
	if (cmd)
		aicon->dnd_command = wstrdup(WMGetFromPLString(cmd));
#endif

	cmd = WMGetFromPLDictionary(info, dPasteCommand);
	if (cmd)
		aicon->paste_command = wstrdup(WMGetFromPLString(cmd));

	/* check auto launch */
	value = WMGetFromPLDictionary(info, dAutoLaunch);

	aicon->auto_launch = getBooleanDockValue(value, dAutoLaunch);

	/* check lock */
	value = WMGetFromPLDictionary(info, dLock);

	aicon->lock = getBooleanDockValue(value, dLock);

	/* check if it wasn't normally docked */
	value = WMGetFromPLDictionary(info, dForced);

	aicon->forced_dock = getBooleanDockValue(value, dForced);

	/* check if we can rely on the stuff in the app */
	value = WMGetFromPLDictionary(info, dBuggyApplication);

	aicon->buggy_app = getBooleanDockValue(value, dBuggyApplication);

	/* get position in the dock */
	value = WMGetFromPLDictionary(info, dPosition);
	if (value && WMIsPLString(value)) {
		if (sscanf(WMGetFromPLString(value), "%hi,%hi", &aicon->xindex, &aicon->yindex) != 2)
			wwarning(_("bad value in docked icon state info %s"), WMGetFromPLString(dPosition));
	} else {
		aicon->yindex = index;
		aicon->xindex = 0;
	}

	/* check if icon is omnipresent */
	value = WMGetFromPLDictionary(info, dOmnipresent);

	aicon->omnipresent = getBooleanDockValue(value, dOmnipresent);

	aicon->running = 0;
	aicon->docked = 1;

	return aicon;
}

#define COMPLAIN(key) wwarning(_("bad value in dock/drawer state info:%s"), key)

/* restore lowered/raised state */
static void restore_state_lowered(WDock *dock, WMPropList *state)
{
	WMPropList *value;

	dock->lowered = 0;

	if (!state)
		return;

	value = WMGetFromPLDictionary(state, dLowered);
	if (value) {
		if (!WMIsPLString(value)) {
			COMPLAIN("Lowered");
		} else {
			if (strcasecmp(WMGetFromPLString(value), "YES") == 0)
				dock->lowered = 1;
		}
	}
}

/* restore collapsed state */
static void restore_state_collapsed(WDock *dock, WMPropList *state)
{
	WMPropList *value;

	dock->collapsed = 0;

	if (!state)
		return;

	value = WMGetFromPLDictionary(state, dCollapsed);
	if (value) {
		if (!WMIsPLString(value)) {
			COMPLAIN("Collapsed");
		} else {
			if (strcasecmp(WMGetFromPLString(value), "YES") == 0)
				dock->collapsed = 1;
		}
	}
}

/* restore auto-collapsed state */
static int restore_state_autocollapsed(WDock *dock, WMPropList *state)
{
	WMPropList *value;
	int ret = 0;

	if (!state)
		return 0;

	value = WMGetFromPLDictionary(state, dAutoCollapse);
	if (!value)
		return 0;

	if (!WMIsPLString(value)) {
		COMPLAIN("AutoCollapse");
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
static void restore_state_autoraise(WDock *dock, WMPropList *state)
{
	WMPropList *value;

	if (!state)
		return;

	value = WMGetFromPLDictionary(state, dAutoRaiseLower);
	if (!value)
		return;

	if (!WMIsPLString(value)) {
		COMPLAIN("AutoRaiseLower");
	} else {
		if (strcasecmp(WMGetFromPLString(value), "YES") == 0)
			dock->auto_raise_lower = 1;
	}
}

/* restore attract icons state */
static int restore_state_autoattracticons(WDock *dock, WMPropList *state)
{
	WMPropList *value;
	int ret = 0;

	dock->attract_icons = 0;

	if (!state)
		return 0;

	value = WMGetFromPLDictionary(state, dAutoAttractIcons);
	if (!value)
		return 0;

	if (!WMIsPLString(value)) {
		COMPLAIN("AutoAttractIcons");
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

static WMPropList *get_application_list(WMPropList *dock_state, virtual_screen *vscr)
{
	WMPropList *key, *apps;

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

	if (!apps)
		apps = WMGetFromPLDictionary(dock_state, dApplications);

	return apps;
}

static void save_application_list(WMPropList *state, WMPropList *list, virtual_screen *vscr)
{
	WMPropList *key;

	key = get_applications_string(vscr);
	WMPutInPLDictionary(state, key, list);
	WMReleasePropList(key);
}

static void set_attacheddocks_map(WDock *dock)
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

static void set_attacheddocks_unmap(WDock *dock)
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

static void dock_unset_attacheddocks(WDock *dock)
{
	set_attacheddocks_unmap(dock);
}

static void restore_dock_position(WDock *dock, WMPropList *state)
{
	WMPropList *value;
	virtual_screen *vscr = dock->vscr;
	WScreen *scr = vscr->screen_ptr;

	value = WMGetFromPLDictionary(state, dPosition);
	if (value) {
		if (!WMIsPLString(value)) {
			COMPLAIN("Position");
		} else {
			if (sscanf(WMGetFromPLString(value), "%i,%i", &dock->x_pos, &dock->y_pos) != 2)
				COMPLAIN("Position");

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

static void restore_clip_position(WDock *dock, WMPropList *state)
{
	virtual_screen *vscr = dock->vscr;
	WMPropList *value;

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

void restore_drawer_position(WDock *drawer, WMPropList *state)
{
	virtual_screen *vscr = drawer->vscr;
	WMPropList *value;
	int y_index;

	value = WMGetFromPLDictionary(state, dPosition);
	if (!value || !WMIsPLString(value)) {
		COMPLAIN("Position");
	} else {
		if (sscanf(WMGetFromPLString(value), "%i,%i", &drawer->x_pos, &drawer->y_pos) != 2)
			COMPLAIN("Position");

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

static void dock_autolaunch(int vscrno)
{
	/* auto-launch apps */
	if (!wPreferences.flags.nodock && w_global.vscreens[vscrno]->dock.dock) {
		w_global.vscreens[vscrno]->last_dock = w_global.vscreens[vscrno]->dock.dock;
		wDockDoAutoLaunch(w_global.vscreens[vscrno]->dock.dock, 0);
	}
}

static void clip_autolaunch(int vscrno)
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

static void drawers_autolaunch(int vscrno)
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

void dockedapps_autolaunch(int vscrno)
{
	dock_autolaunch(vscrno);
	clip_autolaunch(vscrno);
	drawers_autolaunch(vscrno);
}

static void wDockDoAutoLaunch(WDock *dock, int workspace)
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

	assert(index < dock->max_icons);

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

	assert(index < dock->max_icons);

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

	assert(index < dock->max_icons);

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
	assert(index < dock->max_icons);

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

	assert(index < src->max_icons);

	src->icon_array[index] = NULL;
	src->icon_count--;

	for (index = 1; index < dest->max_icons; index++)
		if (dest->icon_array[index] == NULL)
			break;

	assert(index < dest->max_icons);

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

	assert(index < dock->max_icons);
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

	assert(j == abs(ex_x - index_of_hole));

	wSlideAppicons(aicons_to_shift, j, (index_of_hole < ex_x));

	*ret_x = ex_x;
	*ret_y = ex_y;

	return True;
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

static int onScreen(virtual_screen *vscr, int x, int y)
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
static void toggleLowered(WDock *dock)
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

static void toggleCollapsed(WDock *dock)
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
static void iconDblClick(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = desc->parent;
	WDock *dock = btn->dock;
	WApplication *wapp = NULL;
	int unhideHere = 0;

	if (btn->icon->owner && !(event->xbutton.state & ControlMask)) {
		wapp = wApplicationOf(btn->icon->owner->main_window);

		assert(wapp != NULL);

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

static void handleDockMove(WDock *dock, WAppIcon *aicon, XEvent *event)
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


static int getClipButton(int px, int py)
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

static void handleClipChangeWorkspace(virtual_screen *vscr, XEvent *event)
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
		wMenuAddCallback(dock->menu, _("Unhide Here"), unhideHereCallback, NULL);
	else
		wMenuAddCallback(dock->menu, _("Bring Here"), unhideHereCallback, NULL);

	/* Hide / Unhide */
	if (wapp && wapp->flags.hidden)
		entry = wMenuAddCallback(dock->menu, _("Unhide"), hideCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Hide"), hideCallback, NULL);

	/* Settings */
	wMenuAddCallback(dock->menu, _("Settings..."), settingsCallback, NULL);

	/* Remove drawer / kill */
	if (wIsADrawer(aicon))
		entry = wMenuAddCallback(dock->menu, _("Remove drawer"), removeDrawerCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Kill"), killCallback, NULL);

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
		wMenuAddCallback(dock->menu, _("Unhide Here"), unhideHereCallback, NULL);
	else
		wMenuAddCallback(dock->menu, _("Bring Here"), unhideHereCallback, NULL);

	/* Hide / Unhide Icons */
	if (wapp && wapp->flags.hidden)
		entry = wMenuAddCallback(dock->menu, _("Unhide"), hideCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Hide"), hideCallback, NULL);

	wMenuAddCallback(dock->menu, _("Settings..."), settingsCallback, NULL);

	/* Remove drawer / Kill */
	if (wIsADrawer(aicon))
		entry = wMenuAddCallback(dock->menu, _("Remove drawer"), removeDrawerCallback, NULL);
	else
		entry = wMenuAddCallback(dock->menu, _("Kill"), killCallback, NULL);

	updateOptionsMenu(dock, opt_menu);

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

static void dock_icon_mouse_down(WObjDescriptor *desc, XEvent *event)
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

static void clip_icon_mouse_down(WObjDescriptor *desc, XEvent *event)
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

static void drawer_icon_mouse_down(WObjDescriptor *desc, XEvent *event)
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

static void dock_enter_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;
	WDock *dock, *tmp;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	assert(event->type == EnterNotify);

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

static void clip_enter_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;
	WDock *dock, *tmp;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	assert(event->type == EnterNotify);

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

static void drawer_enter_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;
	WDock *dock, *tmp;
	virtual_screen *vscr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	assert(event->type == EnterNotify);

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

static void dock_leave(WDock *dock)
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

static void clip_leave(WDock *dock)
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

static void drawer_leave(WDock *dock)
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

static void dock_leave_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	assert(event->type == LeaveNotify);

	if (desc->parent_type != WCLASS_DOCK_ICON)
		return;

	dock_leave(btn->dock);
}

static void clip_leave_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	assert(event->type == LeaveNotify);

	if (desc->parent_type != WCLASS_DOCK_ICON)
		return;

	clip_leave(btn->dock);
}

static void drawer_leave_notify(WObjDescriptor *desc, XEvent *event)
{
	WAppIcon *btn = (WAppIcon *) desc->parent;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	assert(event->type == LeaveNotify);

	if (desc->parent_type != WCLASS_DOCK_ICON)
		return;

	drawer_leave(btn->dock);
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

static void drawer_autocollapse(void *cdata)
{
	WDock *dock = (WDock *) cdata;

	if (dock->auto_collapse) {
		dock->collapsed = 1;
		wDockHideIcons(dock);
	}

	dock->auto_collapse_magic = NULL;
}

static void clipAutoExpand(void *cdata)
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

static void clipAutoLower(void *cdata)
{
	WDock *dock = (WDock *) cdata;

	if (dock->auto_raise_lower)
		wDockLower(dock);

	dock->auto_lower_magic = NULL;
}

static void clipAutoRaise(void *cdata)
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

static void dock_icon_expose(WObjDescriptor *desc, XEvent *event)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	wAppIconPaint(desc->parent);
}

static void clip_icon_expose(WObjDescriptor *desc, XEvent *event)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	wClipIconPaint(desc->parent);
}

static void drawer_icon_expose(WObjDescriptor *desc, XEvent *event)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	wAppIconPaint(desc->parent);
}

static void drawerIconExpose(WObjDescriptor *desc, XEvent *event)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) event;

	wDrawerIconPaint((WAppIcon *) desc->parent);
}


static int addADrawer(virtual_screen *vscr)
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


static void addADrawerCallback(WMenu *menu, WMenuEntry *entry)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	assert(entry->clientdata!=NULL);
	addADrawer(((WAppIcon *) entry->clientdata)->dock->vscr);
}


static void drawerDestroy(WDock *drawer)
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


static void removeDrawerCallback(WMenu *menu, WMenuEntry *entry)
{
	WDock *dock = ((WAppIcon*)entry->clientdata)->dock;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	assert(dock != NULL);

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
			wDrawerIconPaint(ai);
		}
		XMoveWindow(dpy, ai->icon->core->window, ai->x_pos, ai->y_pos);
	}
}


static void swapDrawers(virtual_screen *vscr, int new_x)
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


static WDock *getDrawer(virtual_screen *vscr, int y_index)
{
	WDrawerChain *dc;

	for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next)
		if (dc->adrawer->y_pos - vscr->dock.dock->y_pos == y_index * ICON_SIZE)
			return dc->adrawer;

	return NULL;
}



/* Find the "hole" a moving appicon created when snapped into the
 * drawer. redocking is a boolean. If the moving appicon comes from the
 * drawer, drawer->icon_count is correct. If not, redocking is then false and
 * there are now drawer->icon_count plus one appicons in the drawer. */
static int indexOfHole(WDock *drawer, WAppIcon *moving_aicon, int redocking)
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

static WDock *drawerRestoreState(virtual_screen *vscr, WMPropList *drawer_state)
{
	WDock *drawer;
	WMPropList *apps, *value, *dock_state;

	if (!drawer_state)
		return NULL;

	make_keys();

	WMRetainPropList(drawer_state);

	/* Get the instance name, and create a drawer */
	value = WMGetFromPLDictionary(drawer_state, dName);
	drawer = drawer_create(vscr, WMGetFromPLString(value));

	/* restore DnD command and paste command */
#ifdef USE_DOCK_XDND
	value = WMGetFromPLDictionary(drawer_state, dDropCommand);
	if (value && WMIsPLString(value))
		drawer->icon_array[0]->dnd_command = wstrdup(WMGetFromPLString(value));
#endif /* USE_DOCK_XDND */

	value = WMGetFromPLDictionary(drawer_state, dPasteCommand);
	if (value && WMIsPLString(value))
		drawer->icon_array[0]->paste_command = wstrdup(WMGetFromPLString(value));

	/* restore position */
	restore_drawer_position(drawer, drawer_state);

	/* restore dock properties (applist and others) */
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
	WAppIcon *ai;
	char buffer[64];

	ai = drawer->icon_array[0];
	/* Store its name */
	pstr = WMCreatePLString(ai->wm_instance);
	drawer_state = WMCreatePLDictionary(dName, pstr, NULL); /* we need this final NULL */
	WMReleasePropList(pstr);

	/* Store its position */
	snprintf(buffer, sizeof(buffer), "%i,%i", ai->x_pos, ai->y_pos);
	pstr = WMCreatePLString(buffer);
	WMPutInPLDictionary(drawer_state, dPosition, pstr);
	WMReleasePropList(pstr);

#ifdef USE_DOCK_XDND
	/* Store its DnD command */
	if (ai->dnd_command) {
		pstr = WMCreatePLString(ai->dnd_command);
		WMPutInPLDictionary(drawer_state, dDropCommand, pstr);
		WMReleasePropList(pstr);
	}
#endif /* USE_DOCK_XDND */

	/* Store its paste command */
	if (ai->paste_command) {
		pstr = WMCreatePLString(ai->paste_command);
		WMPutInPLDictionary(drawer_state, dPasteCommand, pstr);
		WMReleasePropList(pstr);
	}

	/* Store applications list and other properties */
	pstr = drawer_save_state(drawer);
	WMPutInPLDictionary(drawer_state, dDock, pstr);
	WMReleasePropList(pstr);

	return drawer_state;
}


void wDrawersSaveState(virtual_screen *vscr)
{
	WMPropList *all_drawers, *drawer_state;
	int i;
	WDrawerChain *dc;

	make_keys();

	all_drawers = WMCreatePLArray(NULL);
	for (i=0, dc = vscr->drawer.drawers;
	     i < vscr->drawer.drawer_count;
	     i++, dc = dc->next) {
		drawer_state = drawerSaveState(dc->adrawer);
		WMAddToPLArray(all_drawers, drawer_state);
		WMReleasePropList(drawer_state);
	}

	WMPutInPLDictionary(w_global.session_state, dDrawers, all_drawers);
	WMReleasePropList(all_drawers);
}

void wDrawers_unmap(int vscrno)
{
	WDrawerChain *dc;

	for (dc = w_global.vscreens[vscrno]->drawer.drawers; dc; dc = dc->next)
		drawerRestoreState_unmap(dc->adrawer);
}

void wDrawersRestoreState(virtual_screen *vscr)
{
	WMPropList *all_drawers, *drawer_state;
	int i;

	make_keys();

	if (w_global.session_state == NULL)
		return;

	all_drawers = WMGetFromPLDictionary(w_global.session_state, dDrawers);
	if (!all_drawers)
		return;

	for (i = 0; i < WMGetPropListItemCount(all_drawers); i++) {
		drawer_state = WMGetFromPLArray(all_drawers, i);
		drawerRestoreState(vscr, drawer_state);
	}
}

void wDrawersRestoreState_map(virtual_screen *vscr)
{
	WDrawerChain *dc;

	for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next)
		drawerRestoreState_map(dc->adrawer);
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
		wMenuAddCallback(clip->menu, _("Unhide Here"), unhideHereCallback, NULL);
	else
		wMenuAddCallback(clip->menu, _("Bring Here"), unhideHereCallback, NULL);

	/* Hide / Unhide */
	if (wapp && wapp->flags.hidden)
		entry = wMenuAddCallback(clip->menu, _("Unhide"), hideCallback, NULL);
	else
		entry = wMenuAddCallback(clip->menu, _("Hide"), hideCallback, NULL);

	/* Settings */
	wMenuAddCallback(clip->menu, _("Settings..."), settingsCallback, NULL);

	/* Kill / Remove Drawer */
	if (wIsADrawer(aicon))
		entry = wMenuAddCallback(clip->menu, _("Remove drawer"), removeDrawerCallback, NULL);
	else
		entry = wMenuAddCallback(clip->menu, _("Kill"), killCallback, NULL);

	/* clip/drawer options */
	updateOptionsMenu(clip, opt_menu);

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
