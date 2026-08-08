/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#define MAX_RTEXT_LENGTH (MAX_WORKSPACENAME_WIDTH + MAX_SHORTCUT_LENGTH + 16)

static int initialized = 0;
static void switchmenu_setup_switchmenu_notif(void);
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

static void switchmenu_setup_switchmenu_notif(void)
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
	switchmenu_setup_switchmenu_notif();
	wwin = vscr->window.focused;
	while (wwin) {
		switchmenu_additem(switch_menu, wwin);
		wwin = wwin->prev;
	}

	return switch_menu;
}

void switchmenu_destroy(virtual_screen *vscr)
{
	if (!vscr->menu.switch_menu)
		return;

	WMRemoveNotificationObserver(vscr->menu.switch_menu);
	wMenuDestroy(vscr->menu.switch_menu);
	vscr->menu.switch_menu = NULL;
	vscr->menu.flags.added_window_menu = 0;
}

/* Open switch menu */
void OpenSwitchMenu(virtual_screen *vscr, int x, int y, int keyboard)
{
	if (!vscr->menu.switch_menu) {
		vscr->menu.switch_menu = switchmenu_create(vscr);
		menu_map(vscr->menu.switch_menu);
	}

	/* Mapped, so unmap or raise */
	if (vscr->menu.switch_menu->flags.mapped) {
		if (!vscr->menu.switch_menu->flags.buttoned) {
			switchmenu_destroy(vscr);
		} else {
			wRaiseFrame(vscr->menu.switch_menu->frame->vscr,
				    vscr->menu.switch_menu->frame->core);

			if (keyboard) {
				vscr->menu.switch_menu->x_pos = 0;
				vscr->menu.switch_menu->y_pos = 0;
				wMenuMapAt(vscr, vscr->menu.switch_menu, True);
			}
		}
		return;
	}

	/* Not mapped, map it */
	if (keyboard &&
	    x == vscr->screen_ptr->scr_width / 2 &&
	    y == vscr->screen_ptr->scr_height / 2)
		y = y - vscr->menu.switch_menu->frame->height / 2;

	vscr->menu.switch_menu->x_pos = x - vscr->menu.switch_menu->frame->width / 2;
	vscr->menu.switch_menu->y_pos = y;
	wMenuMapAt(vscr, vscr->menu.switch_menu, keyboard);
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

static void fillRtext(char *buf, size_t bufsz, WWindow *wwin, virtual_screen *vscr)
{
	char *mlbl = wwin->mark_key_label ? GetShortcutString(wwin->mark_key_label) : NULL;

	if (IS_OMNIPRESENT(wwin)) {
		if (mlbl)
			snprintf(buf, bufsz, "[%s] [*]", mlbl);
		else
			snprintf(buf, bufsz, "[*]");
	} else {
		if (mlbl)
			snprintf(buf, bufsz, "[%s] [%s]", mlbl,
				 vscr->workspace.array[wwin->frame->workspace]->name);
		else
			snprintf(buf, bufsz, "[%s]",
				 vscr->workspace.array[wwin->frame->workspace]->name);
	}
	wfree(mlbl);
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
	entry->rtext = wmalloc(MAX_RTEXT_LENGTH);
	fillRtext(entry->rtext, MAX_RTEXT_LENGTH, wwin, vscr);

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

static void switchmenu_changeitem(WMenu *menu, WWindow *wwin)
{
	virtual_screen *vscr;
	WMenuEntry *entry;
	char title[MAX_MENU_TEXT_LENGTH + 6];
	int i;

	if (!menu)
		return;

	vscr = menu->vscr;

	for (i = 0; i < menu->entry_no; i++) {
		entry = menu->entries[i];
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

static void switchmenu_changeentry_workspaceitem(WMenu *menu, WWindow *wwin,
						 WMenuEntry *entry, int i)
{
	virtual_screen *vscr;
	char *t, *rt;
	int it, ion, idx = -1;

	if (!entry->rtext)
		return;

	vscr = menu->vscr;

	fillRtext(entry->rtext, MAX_RTEXT_LENGTH, wwin, vscr);

	rt = entry->rtext;
	entry->rtext = NULL;
	t = entry->text;
	entry->text = NULL;

	it = entry->flags.indicator_type;
	ion = entry->flags.indicator_on;

	if (!IS_OMNIPRESENT(wwin) && idx < 0)
		idx = menuIndexForWindow(menu, wwin, i);

	wMenuRemoveItem(menu, i);

	entry = wMenuInsertCallback(menu, idx, t, focusWindow, wwin);
	wfree(t);
	entry->rtext = rt;
	entry->flags.indicator = 1;
	entry->flags.indicator_type = it;
	entry->flags.indicator_on = ion;
}

static void switchmenu_changeworkspaceitem(WMenu *menu, WWindow *wwin)
{
	WMenuEntry *entry;
	int i;

	if (!menu)
		return;

	for (i = 0; i < menu->entry_no; i++) {
		entry = menu->entries[i];
		/* this is the entry that was changed */
		if (entry->clientdata == wwin) {
			switchmenu_changeentry_workspaceitem(menu, wwin, entry, i);
			break;
		}
	}
}

/* Update switch menu */
static void switchmenu_changestate(WMenu *menu, WWindow *wwin)
{
	WMenuEntry *entry;
	int i;

	if (!menu)
		return;

	for (i = 0; i < menu->entry_no; i++) {
		entry = menu->entries[i];
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

static void update_menu_workspacerename(WMenu *menu, int workspace)
{
	virtual_screen *vscr;
	WWindow *wwin;
	int i;

	if (!menu)
		return;

	vscr = menu->vscr;

	for (i = 0; i < menu->entry_no; i++) {
		wwin = (WWindow *) menu->entries[i]->clientdata;
		if (wwin->frame->workspace == workspace && !IS_OMNIPRESENT(wwin)) {
			fillRtext(menu->entries[i]->rtext, MAX_RTEXT_LENGTH, wwin, vscr);

			menu->flags.realized = 0;
		}
	}

	if (!menu->flags.realized)
		wMenuRealize(menu);
}

void switchmenu_handle_notification(WMenu *menu, const char *name, int workspace)
{
	if (strcmp(name, WMNWorkspaceNameChanged) == 0)
		update_menu_workspacerename(menu, workspace);
}

void switchmenu_handle_notification_wwin(WMenu *menu, WWindow *wwin, const char *name, char *data)
{
	if (strcmp(name, WMNManaged) == 0) {
		switchmenu_additem(menu, wwin);
	} else if (strcmp(name, WMNUnmanaged) == 0) {
		switchmenu_delitem(menu, wwin);
	} else if (strcmp(name, WMNChangedWorkspace) == 0) {
		switchmenu_changeworkspaceitem(menu, wwin);
	} else if (strcmp(name, WMNChangedFocus) == 0) {
		switchmenu_changestate(menu, wwin);
	} else if (strcmp(name, WMNChangedName) == 0) {
		switchmenu_changeitem(menu, wwin);
	} else if (strcmp(name, WMNChangedState) == 0) {
		if (strcmp(data, "omnipresent") == 0)
			switchmenu_changeworkspaceitem(menu, wwin);
		else if (strcmp(data, "mark") == 0)
			switchmenu_changeworkspaceitem(menu, wwin);
		else
			switchmenu_changestate(menu, wwin);
	}
}

static void observer(void *self, WMNotification *notif)
{
	WWindow *wwin = (WWindow *) WMGetNotificationObject(notif);
	const char *name = WMGetNotificationName(notif);
	void *data = WMGetNotificationClientData(notif);

	/* Parameter not used, but tell the compiler that it is ok */
	(void) self;

	if (!wwin)
		return;

	switchmenu_handle_notification_wwin(wwin->vscr->menu.switch_menu,
					    wwin, name, (char *) data);

	/* If menu is not mapped, exit */
	if (!wwin->vscr->menu.switch_menu ||
	    !wwin->vscr->menu.switch_menu->frame ||
	    !wwin->vscr->menu.switch_menu->frame->vscr)
		return;

	menu_move_visible(wwin->vscr->menu.switch_menu);
}

static void wsobserver(void *self, WMNotification *notif)
{
	virtual_screen *vscr = (virtual_screen *) WMGetNotificationObject(notif);
	const char *name = WMGetNotificationName(notif);
	void *data = WMGetNotificationClientData(notif);
	int workspace = (uintptr_t) data;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) self;

	switchmenu_handle_notification(vscr->menu.switch_menu, name, workspace);
}
