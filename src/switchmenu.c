/*
 *  Window Maker window manager
 *
 *  Copyright (c) 1997      Shige Abe
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "WindowMaker.h"
#include "window.h"
#include "actions.h"
#include "client.h"
#include "misc.h"
#include "stacking.h"
#include "workspace.h"
#include "framewin.h"
#include "switchmenu.h"

#define IS_GNUSTEP_MENU(w) ((w)->wm_gnustep_attr && \
	((w)->wm_gnustep_attr->flags & GSWindowLevelAttr) && \
	((w)->wm_gnustep_attr->window_level == WMMainMenuWindowLevel || \
	 (w)->wm_gnustep_attr->window_level == WMSubmenuWindowLevel))

static int initialized = 0;
static void observer(void *self, WMNotification *notif);
static void wsobserver(void *self, WMNotification *notif);

/*
 * FocusWindow
 *
 *  - Needs to check if already in the right workspace before
 *    calling wChangeWorkspace?
 *
 *  Order:
 *    Switch to correct workspace
 *    Unshade if shaded
 *    If iconified then deiconify else focus/raise.
 */
static void focusWindow(WMenu * menu, WMenuEntry * entry)
{
	WWindow *wwin;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) menu;

	wwin = (WWindow *) entry->clientdata;
	wWindowSingleFocus(wwin);
}

void InitializeSwitchMenu(void)
{
	if (initialized)
		return;

	initialized = 1;

	WMAddNotificationObserver(observer, NULL, WMNManaged, NULL);
	WMAddNotificationObserver(observer, NULL, WMNUnmanaged, NULL);
	WMAddNotificationObserver(observer, NULL, WMNChangedWorkspace, NULL);
	WMAddNotificationObserver(observer, NULL, WMNChangedState, NULL);
	WMAddNotificationObserver(observer, NULL, WMNChangedFocus, NULL);
	WMAddNotificationObserver(observer, NULL, WMNChangedStacking, NULL);
	WMAddNotificationObserver(observer, NULL, WMNChangedName, NULL);

	WMAddNotificationObserver(wsobserver, NULL, WMNWorkspaceChanged, NULL);
	WMAddNotificationObserver(wsobserver, NULL, WMNWorkspaceNameChanged, NULL);
}

WMenu *switchmenu_create(virtual_screen *vscr)
{
	WWindow *wwin;
	WMenu *switch_menu;

	switch_menu = menu_create(vscr, _("Windows"));
	menu_map(switch_menu, vscr);
	wwin = vscr->window.focused;
	while (wwin) {
		switchmenu_additem(switch_menu, wwin);
		wwin = wwin->prev;
	}

	return switch_menu;
}

/* Open switch menu */
void OpenSwitchMenu(virtual_screen *vscr, int x, int y, int keyboard)
{
	WMenu *switchmenu = vscr->menu.switch_menu;

	/* Mapped, so unmap or raise */
	if (switchmenu->flags.mapped) {
		if (!switchmenu->flags.buttoned) {
			wMenuUnmap(switchmenu);
		} else {
			wRaiseFrame(switchmenu->frame->core);

			if (keyboard)
				wMenuMapAt(vscr, switchmenu, 0, 0, True);
		}
		return;
	}

	/* Not mapped, map it */
	if (keyboard &&
	    x == vscr->screen_ptr->scr_width / 2 &&
	    y == vscr->screen_ptr->scr_height / 2)
		y = y - switchmenu->frame->core->height / 2;

	wMenuMapAt(vscr, switchmenu, x - switchmenu->frame->core->width / 2, y, keyboard);
}

static int menuIndexForWindow(WMenu * menu, WWindow * wwin, int old_pos)
{
	int idx, move_down;

	if (menu->entry_no <= old_pos)
		return -1;

	for (idx = 0, move_down = 0; idx < menu->entry_no; idx++) {
		WWindow *tw = (WWindow *) menu->entries[idx]->clientdata;

		/* Is the window moving down in the menu?  If so, we'll need to
		 * adjust its new index by 1.
		 */
		if (tw == wwin)
			move_down = 1;

		if (IS_OMNIPRESENT(tw) || (tw != wwin &&
					   tw->frame->workspace >= wwin->frame->workspace))
			return idx - move_down;
	}

	return idx;
}

void switchmenu_additem(WMenu *menu, WWindow *wwin)
{
	virtual_screen *vscr;
	WMenuEntry *entry;
	char *t, title[MAX_MENU_TEXT_LENGTH + 6];
	int idx = -1, len = sizeof(title);

	if (!menu)
		return;

	vscr = menu->vscr;

	if (wwin->flags.internal_window ||
	    WFLAGP(wwin, skip_window_list) ||
	    IS_GNUSTEP_MENU(wwin))
		return;

	if (wwin->title)
		snprintf(title, len, "%s", wwin->title);
	else
		snprintf(title, len, "%s", DEF_WINDOW_TITLE);

	if (!IS_OMNIPRESENT(wwin))
		idx = menuIndexForWindow(menu, wwin, 0);

	t = ShrinkString(vscr->screen_ptr->menu_entry_font, title, MAX_WINDOWLIST_WIDTH);
	entry = wMenuInsertCallback(menu, idx, t, focusWindow, wwin);
	wfree(t);

	entry->flags.indicator = 1;
	entry->rtext = wmalloc(MAX_WORKSPACENAME_WIDTH + 8);
	if (IS_OMNIPRESENT(wwin))
		snprintf(entry->rtext, MAX_WORKSPACENAME_WIDTH, "[*]");
	else
		snprintf(entry->rtext, MAX_WORKSPACENAME_WIDTH, "[%s]",
			 vscr->workspace.array[wwin->frame->workspace]->name);

	if (wwin->flags.hidden) {
		entry->flags.indicator_type = MI_HIDDEN;
		entry->flags.indicator_on = 1;
	} else if (wwin->flags.miniaturized) {
		entry->flags.indicator_type = MI_MINIWINDOW;
		entry->flags.indicator_on = 1;
	} else if (wwin->flags.focused) {
		entry->flags.indicator_type = MI_DIAMOND;
		entry->flags.indicator_on = 1;
	} else if (wwin->flags.shaded) {
		entry->flags.indicator_type = MI_SHADED;
		entry->flags.indicator_on = 1;
	}
}

void switchmenu_delitem(WMenu *menu, WWindow *wwin)
{
	WMenuEntry *entry;
	int i;

	if (!menu)
		return;

	for (i = 0; i < menu->entry_no; i++) {
		entry = menu->entries[i];
		/* this is the entry that was changed */
		if (entry->clientdata == wwin) {
			wMenuRemoveItem(menu, i);
			break;
		}
	}
}

static void switchmenu_changeitem(virtual_screen *vscr, WWindow *wwin)
{
	WMenuEntry *entry;
	char title[MAX_MENU_TEXT_LENGTH + 6];
	int i;

	if (!vscr->menu.switch_menu)
		return;

	for (i = 0; i < vscr->menu.switch_menu->entry_no; i++) {
		entry = vscr->menu.switch_menu->entries[i];
		/* this is the entry that was changed */
		if (entry->clientdata == wwin) {
			if (entry->text)
				wfree(entry->text);

			if (wwin->title)
				snprintf(title, MAX_MENU_TEXT_LENGTH, "%s", wwin->title);
			else
				snprintf(title, MAX_MENU_TEXT_LENGTH, "%s", DEF_WINDOW_TITLE);

			entry->text = ShrinkString(vscr->screen_ptr->menu_entry_font, title, MAX_WINDOWLIST_WIDTH);

			break;
		}
	}
}

static void switchmenu_changeworkspaceitem(virtual_screen *vscr, WWindow *wwin)
{
	WMenuEntry *entry;
	int i;

	if (!vscr->menu.switch_menu)
		return;

	for (i = 0; i < vscr->menu.switch_menu->entry_no; i++) {
		entry = vscr->menu.switch_menu->entries[i];
		/* this is the entry that was changed */
		if (entry->clientdata == wwin) {
			if (entry->rtext) {
				char *t, *rt;
				int it, ion, idx = -1;

				if (IS_OMNIPRESENT(wwin))
					snprintf(entry->rtext, MAX_WORKSPACENAME_WIDTH, "[*]");
				else
					snprintf(entry->rtext, MAX_WORKSPACENAME_WIDTH, "[%s]",
						 vscr->workspace.array[wwin->frame->workspace]->name);

				rt = entry->rtext;
				entry->rtext = NULL;
				t = entry->text;
				entry->text = NULL;

				it = entry->flags.indicator_type;
				ion = entry->flags.indicator_on;

				if (!IS_OMNIPRESENT(wwin) && idx < 0)
					idx = menuIndexForWindow(vscr->menu.switch_menu, wwin, i);

				wMenuRemoveItem(vscr->menu.switch_menu, i);

				entry = wMenuInsertCallback(vscr->menu.switch_menu, idx, t, focusWindow, wwin);
				wfree(t);
				entry->rtext = rt;
				entry->flags.indicator = 1;
				entry->flags.indicator_type = it;
				entry->flags.indicator_on = ion;
			}
			break;
		}
	}
}

/* Update switch menu */
static void switchmenu_changestate(virtual_screen *vscr, WWindow *wwin)
{
	WMenuEntry *entry;
	int i;

	if (!vscr->menu.switch_menu)
		return;

	for (i = 0; i < vscr->menu.switch_menu->entry_no; i++) {
		entry = vscr->menu.switch_menu->entries[i];
		/* this is the entry that was changed */
		if (entry->clientdata == wwin) {
			if (wwin->flags.hidden) {
				entry->flags.indicator_type = MI_HIDDEN;
				entry->flags.indicator_on = 1;
			} else if (wwin->flags.miniaturized) {
				entry->flags.indicator_type = MI_MINIWINDOW;
				entry->flags.indicator_on = 1;
			} else if (wwin->flags.shaded && !wwin->flags.focused) {
				entry->flags.indicator_type = MI_SHADED;
				entry->flags.indicator_on = 1;
			} else {
				entry->flags.indicator_on = wwin->flags.focused;
				entry->flags.indicator_type = MI_DIAMOND;
			}
			break;
		}
	}
}

static void UpdateSwitchMenuWorkspace(virtual_screen *vscr, int workspace)
{
	WMenu *menu = vscr->menu.switch_menu;
	int i;
	WWindow *wwin;

	if (!menu)
		return;

	for (i = 0; i < menu->entry_no; i++) {
		wwin = (WWindow *) menu->entries[i]->clientdata;
		if (wwin->frame->workspace == workspace && !IS_OMNIPRESENT(wwin)) {
			if (IS_OMNIPRESENT(wwin))
				snprintf(menu->entries[i]->rtext, MAX_WORKSPACENAME_WIDTH, "[*]");
			else
				snprintf(menu->entries[i]->rtext, MAX_WORKSPACENAME_WIDTH, "[%s]",
					 vscr->workspace.array[wwin->frame->workspace]->name);

			menu->flags.realized = 0;
		}
	}

	if (!menu->flags.realized)
		wMenuRealize(menu);
}

static void observer(void *self, WMNotification * notif)
{
	WWindow *wwin = (WWindow *) WMGetNotificationObject(notif);
	const char *name = WMGetNotificationName(notif);
	void *data = WMGetNotificationClientData(notif);
	int tmp;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) self;

	if (!wwin)
		return;

	if (strcmp(name, WMNManaged) == 0) {
		switchmenu_additem(wwin->vscr->menu.switch_menu, wwin);
	} else if (strcmp(name, WMNUnmanaged) == 0) {
		switchmenu_delitem(wwin->vscr->menu.switch_menu, wwin);
	} else if (strcmp(name, WMNChangedWorkspace) == 0) {
		switchmenu_changeworkspaceitem(wwin->vscr, wwin);
	} else if (strcmp(name, WMNChangedFocus) == 0) {
		switchmenu_changestate(wwin->vscr, wwin);
	} else if (strcmp(name, WMNChangedName) == 0) {
		switchmenu_changeitem(wwin->vscr, wwin);
	} else if (strcmp(name, WMNChangedState) == 0) {
		if (strcmp((char *)data, "omnipresent") == 0)
			switchmenu_changeworkspaceitem(wwin->vscr, wwin);
		else
			switchmenu_changestate(wwin->vscr, wwin);
	}

	/* If menu is not mapped, exit */
	if (!wwin->vscr->menu.switch_menu->frame ||
	    !wwin->vscr->menu.switch_menu->frame->vscr)
		return;

	wMenuRealize(wwin->vscr->menu.switch_menu);

	tmp = wwin->vscr->menu.switch_menu->frame->top_width + 5;
	/* if menu got unreachable, bring it to a visible place */
	if (wwin->vscr->menu.switch_menu->frame_x < tmp - (int) wwin->vscr->menu.switch_menu->frame->core->width)
		wMenuMove(wwin->vscr->menu.switch_menu, tmp - (int) wwin->vscr->menu.switch_menu->frame->core->width,
			  wwin->vscr->menu.switch_menu->frame_y, False);

	wMenuPaint(wwin->vscr->menu.switch_menu);
}

static void wsobserver(void *self, WMNotification *notif)
{
	virtual_screen *vscr = (virtual_screen *) WMGetNotificationObject(notif);
	const char *name = WMGetNotificationName(notif);
	void *data = WMGetNotificationClientData(notif);

	/* Parameter not used, but tell the compiler that it is ok */
	(void) self;

	if (strcmp(name, WMNWorkspaceNameChanged) == 0)
		UpdateSwitchMenuWorkspace(vscr, (uintptr_t) data);
}
