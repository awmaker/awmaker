/* session.c - session state handling and R6 style session management
 *
 *  Copyright (c) 1998-2003 Dan Pascu
 *  Copyright (c) 1998-2003 Alfredo Kojima
 *
 *  Window Maker window manager
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

/*
 *
 * If defined(XSMP_ENABLED) and session manager is running then
 * 	do normal stuff
 * else
 * 	do pre-R6 session management stuff (save window state and relaunch)
 *
 * When doing a checkpoint:
 *
 * = Without XSMP
 * Open "Stop"/status Dialog
 * Send SAVE_YOURSELF to clients and wait for reply
 * Save restart info
 * Save state of clients
 *
 * = With XSMP
 * Send checkpoint request to sm
 *
 * When exiting:
 * -------------
 *
 * = Without XSMP
 *
 * Open "Exit Now"/status Dialog
 * Send SAVE_YOURSELF to clients and wait for reply
 * Save restart info
 * Save state of clients
 * Send DELETE to all clients
 * When no more clients are left or user hit "Exit Now", exit
 *
 * = With XSMP
 *
 * Send Shutdown request to session manager
 * if SaveYourself message received, save state of clients
 * if the Die message is received, exit.
 */

#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>

#include "WindowMaker.h"
#include "screen.h"
#include "window.h"
#include "client.h"
#include "session.h"
#include "framewin.h"
#include "workspace.h"
#include "shell.h"
#include "properties.h"
#include "application.h"
#include "appicon.h"
#include "dock-core.h"
#include "dock.h"
#include "misc.h"

#include <WINGs/WUtil.h>


static WMPropList *sApplications = NULL;
static WMPropList *sCommand;
static WMPropList *sName;
static WMPropList *sHost;
static WMPropList *sWorkspace;
static WMPropList *sShaded;
static WMPropList *sMiniaturized;
static WMPropList *sHidden;
static WMPropList *sGeometry;
static WMPropList *sShortcutMask;

static WMPropList *sDock;
static WMPropList *sYes, *sNo;

static void make_keys(void)
{
	if (sApplications != NULL)
		return;

	sApplications = WMCreatePLString("Applications");
	sCommand = WMCreatePLString("Command");
	sName = WMCreatePLString("Name");
	sHost = WMCreatePLString("Host");
	sWorkspace = WMCreatePLString("Workspace");
	sShaded = WMCreatePLString("Shaded");
	sMiniaturized = WMCreatePLString("Miniaturized");
	sHidden = WMCreatePLString("Hidden");
	sGeometry = WMCreatePLString("Geometry");
	sDock = WMCreatePLString("Dock");
	sShortcutMask = WMCreatePLString("ShortcutMask");

	sYes = WMCreatePLString("Yes");
	sNo = WMCreatePLString("No");
}

static int getBool(WMPropList * value)
{
	char *val;

	if (!WMIsPLString(value))
		return 0;

	val = WMGetFromPLString(value);
	if (val == NULL)
		return 0;

	if ((val[1] == '\0' && (val[0] == 'y' || val[0] == 'Y'))
	    || strcasecmp(val, "YES") == 0) {

		return 1;
	} else if ((val[1] == '\0' && (val[0] == 'n' || val[0] == 'N'))
		   || strcasecmp(val, "NO") == 0) {
		return 0;
	} else {
		int i;
		if (sscanf(val, "%i", &i) == 1) {
			return (i != 0);
		} else {
			wwarning(_("can't convert \"%s\" to boolean"), val);
			return 0;
		}
	}
}

static unsigned getInt(WMPropList * value)
{
	char *val;
	unsigned n;

	if (!WMIsPLString(value))
		return 0;
	val = WMGetFromPLString(value);
	if (!val)
		return 0;
	if (sscanf(val, "%u", &n) != 1)
		return 0;

	return n;
}

static WMPropList *makeWindowState(WWindow *wwin, WApplication *wapp)
{
	virtual_screen *vscr = wwin->vscr;
	Window win;
	int i;
	unsigned mask;
	char *class, *instance, *command = NULL, buffer[512];
	WMPropList *win_state, *cmd, *name, *workspace;
	WMPropList *shaded, *miniaturized, *hidden, *geometry;
	WMPropList *dock, *shortcut;

	if (wwin->orig_main_window != None && wwin->orig_main_window != wwin->client_win)
		win = wwin->orig_main_window;
	else
		win = wwin->client_win;

	command = GetCommandForWindow(win);
	if (!command)
		return NULL;

	if (PropGetWMClass(win, &class, &instance)) {
		if (class && instance)
			snprintf(buffer, sizeof(buffer), "%s.%s", instance, class);
		else if (instance)
			snprintf(buffer, sizeof(buffer), "%s", instance);
		else if (class)
			snprintf(buffer, sizeof(buffer), ".%s", class);
		else
			snprintf(buffer, sizeof(buffer), ".");

		name = WMCreatePLString(buffer);
		cmd = WMCreatePLString(command);

		workspace = WMCreatePLString(wwin->frame->vscr->workspace.array[wwin->frame->workspace]->name);
		shaded = wwin->flags.shaded ? sYes : sNo;
		miniaturized = wwin->flags.miniaturized ? sYes : sNo;
		hidden = wwin->flags.hidden ? sYes : sNo;
		snprintf(buffer, sizeof(buffer), "%ix%i+%i+%i",
			 wwin->width, wwin->height, wwin->frame_x, wwin->frame_y);

		geometry = WMCreatePLString(buffer);
		for (mask = 0, i = 0; i < MAX_WINDOW_SHORTCUTS; i++) {
			if (w_global.shortcut.windows[i] != NULL &&
			    WMGetFirstInArray(w_global.shortcut.windows[i], wwin) != WANotFound)
				mask |= 1 << i;
		}

		snprintf(buffer, sizeof(buffer), "%u", mask);
		shortcut = WMCreatePLString(buffer);

		win_state = WMCreatePLDictionary(sName, name,
						 sCommand, cmd,
						 sWorkspace, workspace,
						 sShaded, shaded,
						 sMiniaturized, miniaturized,
						 sHidden, hidden,
						 sShortcutMask, shortcut, sGeometry, geometry, NULL);

		WMReleasePropList(name);
		WMReleasePropList(cmd);
		WMReleasePropList(workspace);
		WMReleasePropList(geometry);
		WMReleasePropList(shortcut);
		if (wapp && wapp->app_icon && wapp->app_icon->dock) {
			int i;
			char *name = NULL;
			if (wapp->app_icon->dock == vscr->dock.dock)
				name = "Dock";

			/* Try the clips */
			if (name == NULL) {
				for (i = 0; i < wwin->frame->vscr->workspace.count; i++)
					if (wwin->frame->vscr->workspace.array[i]->clip == wapp->app_icon->dock)
						break;

				if (i < wwin->frame->vscr->workspace.count)
					name = wwin->frame->vscr->workspace.array[i]->name;
			}

			/* Try the drawers */
			if (name == NULL) {
				WDrawerChain *dc;
				for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next)
					if (dc->adrawer == wapp->app_icon->dock)
						break;

				name = dc->adrawer->icon_array[0]->wm_instance;
			}

			dock = WMCreatePLString(name);
			WMPutInPLDictionary(win_state, sDock, dock);
			WMReleasePropList(dock);
		}
	} else {
		win_state = NULL;
	}

	if (instance)
		wfree(instance);
	if (class)
		wfree(class);
	if (command)
		wfree(command);

	return win_state;
}

void wSessionSaveState(virtual_screen *vscr)
{
	WWindow *wwin = vscr->window.focused;
	WMPropList *win_info, *wks;
	WMPropList *list = NULL;
	WMArray *wapp_list = NULL;

	make_keys();

	if (!w_global.session_state) {
		w_global.session_state = WMCreatePLDictionary(NULL, NULL);
		if (!w_global.session_state)
			return;
	}

	list = WMCreatePLArray(NULL);

	wapp_list = WMCreateArray(16);

	while (wwin) {
		WApplication *wapp = wApplicationOf(wwin->main_window);
		Window appId = wwin->orig_main_window;

		if ((wwin->transient_for == None ||
		     wwin->transient_for == wwin->vscr->screen_ptr->root_win) &&
		    (WMGetFirstInArray(wapp_list, (void *)appId) == WANotFound ||
		     WFLAGP(wwin, shared_appicon)) &&
		    !WFLAGP(wwin, dont_save_session)) {
			/* A entry for this application was not yet saved. Save one. */
			win_info = makeWindowState(wwin, wapp);
			if (win_info != NULL) {
				WMAddToPLArray(list, win_info);
				WMReleasePropList(win_info);
				/* If we were successful in saving the info for this window
				 * add the application the window belongs to, to the
				 * application list, so no multiple entries for the same
				 * application are saved.
				 */
				WMAddToArray(wapp_list, (void *)appId);
			}
		}
		wwin = wwin->prev;
	}

	WMRemoveFromPLDictionary(w_global.session_state, sApplications);
	WMPutInPLDictionary(w_global.session_state, sApplications, list);
	WMReleasePropList(list);

	wks = WMCreatePLString(vscr->workspace.array[vscr->workspace.current]->name);
	WMPutInPLDictionary(w_global.session_state, sWorkspace, wks);
	WMReleasePropList(wks);

	WMFreeArray(wapp_list);
}

void wSessionClearState(void)
{
	make_keys();

	if (!w_global.session_state)
		return;

	WMRemoveFromPLDictionary(w_global.session_state, sApplications);
	WMRemoveFromPLDictionary(w_global.session_state, sWorkspace);
}

static pid_t execCommand(virtual_screen *vscr, char *command)
{
	pid_t pid;
	char **argv;
	int argc;

	wtokensplit(command, &argv, &argc);

	if (!argc)
		return 0;

	pid = execute_command2(vscr, argv, argc);

	while (argc > 0)
		wfree(argv[--argc]);

	wfree(argv);
	return pid;
}

static WSavedState *getWindowState(virtual_screen *vscr, WMPropList *win_state)
{
	WSavedState *state = wmalloc(sizeof(WSavedState));
	WMPropList *value;
	char *tmp;
	unsigned mask;
	int i;

	state->workspace = -1;
	value = WMGetFromPLDictionary(win_state, sWorkspace);
	if (value && WMIsPLString(value)) {
		tmp = WMGetFromPLString(value);
		if (sscanf(tmp, "%i", &state->workspace) != 1) {
			state->workspace = -1;
			for (i = 0; i < vscr->workspace.count; i++) {
				if (strcmp(vscr->workspace.array[i]->name, tmp) == 0) {
					state->workspace = i;
					break;
				}
			}
		} else {
			state->workspace--;
		}
	}

	value = WMGetFromPLDictionary(win_state, sShaded);
	if (value != NULL)
		state->shaded = getBool(value);

	value = WMGetFromPLDictionary(win_state, sMiniaturized);
	if (value != NULL)
		state->miniaturized = getBool(value);

	value = WMGetFromPLDictionary(win_state, sHidden);
	if (value != NULL)
		state->hidden = getBool(value);

	value = WMGetFromPLDictionary(win_state, sShortcutMask);
	if (value != NULL) {
		mask = getInt(value);
		state->window_shortcuts = mask;
	}

	value = WMGetFromPLDictionary(win_state, sGeometry);
	if (value && WMIsPLString(value)) {
		if (!(sscanf(WMGetFromPLString(value), "%ix%i+%i+%i",
			     &state->w, &state->h, &state->x, &state->y) == 4 && (state->w > 0 && state->h > 0))) {
			state->w = 0;
			state->h = 0;
		}
	}

	return state;
}

static inline int is_same(const char *x, const char *y)
{
	if ((x == NULL) && (y == NULL))
		return 1;

	if ((x == NULL) || (y == NULL))
		return 0;

	if (strcmp(x, y) == 0)
		return 1;
	else
		return 0;
}

void wSessionRestoreState(virtual_screen *vscr)
{
	WSavedState *state;
	char *instance, *class, *command;
	WMPropList *win_info, *apps, *cmd, *value;
	pid_t pid;
	int i, count;
	WDock *dock;
	WAppIcon *btn = NULL;
	int j, n, found;
	char *tmp;

	make_keys();

	if (!w_global.session_state)
		return;

	WMPLSetCaseSensitive(True);

	apps = WMGetFromPLDictionary(w_global.session_state, sApplications);
	if (!apps)
		return;

	count = WMGetPropListItemCount(apps);
	if (count == 0)
		return;

	for (i = 0; i < count; i++) {
		win_info = WMGetFromPLArray(apps, i);

		cmd = WMGetFromPLDictionary(win_info, sCommand);
		if (!cmd || !WMIsPLString(cmd) || !(command = WMGetFromPLString(cmd)))
			continue;

		value = WMGetFromPLDictionary(win_info, sName);
		if (!value)
			continue;

		ParseWindowName(value, &instance, &class, "session");
		if (!instance && !class)
			continue;

		state = getWindowState(vscr, win_info);

		dock = NULL;
		value = WMGetFromPLDictionary(win_info, sDock);
		if (value && WMIsPLString(value) && (tmp = WMGetFromPLString(value)) != NULL) {
			if (sscanf(tmp, "%i", &n) != 1) {
				if (!strcasecmp(tmp, "DOCK"))
					dock = vscr->dock.dock;

				/* Try the clips */
				if (dock == NULL) {
					for (j = 0; j < vscr->workspace.count; j++) {
						if (strcmp(vscr->workspace.array[j]->name, tmp) == 0) {
							dock = vscr->workspace.array[j]->clip;
							break;
						}
					}
				}

				/* Try the drawers */
				if (dock == NULL) {
					WDrawerChain *dc;
					for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next) {
						if (strcmp(dc->adrawer->icon_array[0]->wm_instance, tmp) == 0) {
							dock = dc->adrawer;
							break;
						}
					}
				}
			} else {
				if (n == 0)
					dock = vscr->dock.dock;
				else if (n > 0 && n <= vscr->workspace.count)
					dock = vscr->workspace.array[n - 1]->clip;
			}
		}

		found = 0;
		if (dock != NULL) {
			for (j = 0; j < dock->max_icons; j++) {
				btn = dock->icon_array[j];
				if (btn && is_same(instance, btn->wm_instance) &&
				    is_same(class, btn->wm_class) &&
				    is_same(command, btn->command) &&
				    !btn->launching) {
					found = 1;
					break;
				}
			}
		}

		if (found) {
			wDockLaunchWithState(btn, state);
		} else if ((pid = execCommand(vscr, command)) > 0) {
			wWindowAddSavedState(instance, class, command, pid, state);
		} else {
			wfree(state);
		}

		if (instance)
			wfree(instance);

		if (class)
			wfree(class);
	}

	/* clean up */
	WMPLSetCaseSensitive(False);
}

void wSessionRestoreLastWorkspace(virtual_screen *vscr)
{
	WMPropList *wks;
	int w;
	char *value;

	make_keys();

	if (!w_global.session_state)
		return;

	WMPLSetCaseSensitive(True);

	wks = WMGetFromPLDictionary(w_global.session_state, sWorkspace);
	if (!wks || !WMIsPLString(wks))
		return;

	value = WMGetFromPLString(wks);

	if (!value)
		return;

	/* clean up */
	WMPLSetCaseSensitive(False);

	/* Get the workspace number for the workspace name */
	w = wGetWorkspaceNumber(vscr, value);

	if (w != vscr->workspace.current && w < vscr->workspace.count)
		wWorkspaceChange(vscr, w);
}
