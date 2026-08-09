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
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "WindowMaker.h"
#include "actions.h"
#include "menu.h"
#include "misc.h"
#include "main.h"
#include "shell.h"
#include "dialog.h"
#include "keybind.h"
#include "stacking.h"
#include "workspace.h"
#include "defaults.h"
#include "framewin.h"
#include "session.h"
#include "shutdown.h"
#include "xmodifier.h"
#include "rootmenu.h"
#include "switchmenu.h"
#include "screen.h"
#include "input.h"
#include "shbinding.h"

#include <WINGs/WUtil.h>

#define MAX_SHORTCUT_LENGTH 64

static WMenu *readMenuPipe(virtual_screen *vscr, char **file_name);
static WMenu *readPLMenuPipe(virtual_screen *vscr, char **file_name);
static WMenu *readMenuFile(virtual_screen *vscr, const char *file_name);
static WMenu *readMenuDirectory(virtual_screen *vscr, const char *title, char **file_name, const char *command);
static WMenu *configureMenu(virtual_screen *vscr, WMPropList *definition);
static void menu_parser_register_macros(WMenuParser parser);
static void observer(void *self, WMNotification *notif);
static void wsobserver(void *self, WMNotification *notif);
static void rootmenu_setup_switchmenu_notif(void);
static void rootmenu_map(virtual_screen *vscr, int keyboard);

static int initialized = 0;

/*
 * Syntax:
 * # main menu
 * "Menu Name" MENU
 * 	"Title" EXEC command_to_exec -params
 * 	"Submenu" MENU
 * 		"Title" EXEC command_to_exec -params
 * 	"Submenu" END
 * 	"Workspaces" WORKSPACE_MENU
 * 	"Title" built_in_command
 * 	"Quit" EXIT
 * 	"Quick Quit" EXIT QUICK
 * "Menu Name" END
 *
 * Commands may be preceded by SHORTCUT key
 *
 * Built-in commands:
 *
 * INFO_PANEL - shows the Info Panel
 * LEGAL_PANEL - shows the Legal info panel
 * SHUTDOWN [QUICK] - closes the X server [without confirmation]
 * REFRESH - forces the desktop to be repainted
 * EXIT [QUICK] - exit the window manager [without confirmation]
 * EXEC <program> - execute an external program
 * SHEXEC <command> - execute a shell command
 * WORKSPACE_MENU - places the workspace submenu
 * ARRANGE_ICONS
 * RESTART [<window manager>] - restarts the window manager
 * SHOW_ALL - unhide all windows on workspace
 * HIDE_OTHERS - hides all windows excep the focused one
 * OPEN_MENU file - read menu data from file which must be a valid menu file.
 * OPEN_MENU /some/dir [/some/other/dir ...] [WITH command -options]
 *              - read menu data from directory(ies) and
 * 		  eventually precede each with a command.
 * OPEN_MENU | command
 *              - opens command and uses its stdout to construct and insert
 *                the resulting menu in current position. The output of
 *                command must be a valid menu description.
 *                The space between '|' and command is optional.
 *                || will do the same, but will not cache the contents.
 * OPEN_PLMENU | command
 *		- opens command and uses its stdout which must be in proplist
 *		  fromat to construct and insert the resulting menu in current
 *		  position.
 *		  The space between '|' and command is optional.
 *		  || will do the same, but will not cache the contents.
 * SAVE_SESSION - saves the current state of the desktop, which include
 *		  all running applications, all their hints (geometry,
 *		  position on screen, workspace they live on, the dock
 *		  or clip from where they were launched, and
 *		  if minimized, shaded or hidden. Also saves the current
 *		  workspace the user is on. All will be restored on every
 *		  start of windowmaker until another SAVE_SESSION or
 *		  CLEAR_SESSION is used. If SaveSessionOnExit = Yes; in
 *		  WindowMaker domain file, then saving is automatically
 *		  done on every windowmaker exit, overwriting any
 *		  SAVE_SESSION or CLEAR_SESSION (see below). Also save
 *		  dock state now.
 * CLEAR_SESSION - clears any previous saved session. This will not have
 *		  any effect if SaveSessionOnExit is True.
 *
 */

#define M_QUICK		1

/* menu commands */

static void execCommand(WMenu *menu, WMenuEntry *entry)
{
	shExec(menu->vscr, entry->clientdata);
}

static void exitCommand(WMenu *menu, WMenuEntry *entry)
{
	shExit(menu->vscr, (entry->clientdata == (void *)M_QUICK));
}

static void shutdownCommand(WMenu *menu, WMenuEntry *entry)
{
	shShutdown(menu->vscr, (entry->clientdata == (void *)M_QUICK));
}

static void restartCommand(WMenu *menu, WMenuEntry *entry)
{
	shRestart(menu->vscr, entry->clientdata);
}

static void refreshCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shRefresh(menu->vscr);
}

static void arrangeIconsCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shArrangeIcons(menu->vscr);
}

static void showAllCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shShowAll(menu->vscr);
}

static void hideOthersCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shHideOthers(menu->vscr);
}

static void saveSessionCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shSaveSession(menu->vscr);
}

static void clearSessionCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shClearSession(menu->vscr);
}

static void infoPanelCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shInfoPanel(menu->vscr);
}

static void legalPanelCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shLegalPanel(menu->vscr);
}

static void keybindsPanelCommand(WMenu *menu, WMenuEntry *entry)
{
	(void) entry;

	shKeybindsPanel(menu->vscr);
}

/********************************************************************/

static char *getLocalizedMenuFile(const char *menu)
{
	char *buffer, *ptr, *locale;
	int len;

	if (!w_global.locale)
		return NULL;

	len = strlen(menu) + strlen(w_global.locale) + 8;
	buffer = wmalloc(len);

	/* try menu.locale_name */
	snprintf(buffer, len, "%s.%s", menu, w_global.locale);
	if (access(buffer, F_OK) == 0)
		return buffer;

	/* position of locale in our buffer */
	locale = buffer + strlen(menu) + 1;

	/* check if it is in the form aa_bb.encoding and check for aa_bb */
	ptr = strchr(locale, '.');
	if (ptr) {
		*ptr = 0;
		if (access(buffer, F_OK) == 0)
			return buffer;
	}

	/* now check for aa */
	ptr = strchr(locale, '_');
	if (ptr) {
		*ptr = 0;
		if (access(buffer, F_OK) == 0)
			return buffer;
	}

	wfree(buffer);

	return NULL;
}

Bool wRootMenuPerformShortcut(XEvent *event)
{
	virtual_screen *vscr = wScreenForRootWindow(event->xkey.root);
	SHBinding *b;
	int modifiers;
	int done = 0;

	/* ignore CapsLock */
	modifiers = event->xkey.state & w_global.shortcut.modifiers_mask;

	for (b = shGetBindings(); b != NULL; b = b->next) {
		if (b->type == RSM_WKBD || b->keycode == 0 || b->chain_length != 1)
			continue;

		if (b->keycode == event->xkey.keycode && b->modifier == modifiers) {
			shRunAction(b, vscr);
			done = True;
		}
	}

	return done;
}

void wRootMenuBindShortcuts(Window window)
{
	SHBinding *b;

	for (b = shGetBindings(); b != NULL; b = b->next) {
		if (b->type == RSM_WKBD || b->keycode == 0 || b->chain_length != 1)
			continue;

		if (b->modifier != AnyModifier) {
			XGrabKey(dpy, b->keycode, b->modifier | LockMask,
				 window, True, GrabModeAsync, GrabModeAsync);
#ifdef NUMLOCK_HACK
			wHackedGrabKey(dpy, b->keycode, b->modifier, window, True, GrabModeAsync, GrabModeAsync);
#endif
		}
		XGrabKey(dpy, b->keycode, b->modifier, window, True, GrabModeAsync, GrabModeAsync);
	}
}

void rebindKeygrabs(virtual_screen *vscr)
{
	WWindow *wwin;

	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);

		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}
}

/*
 * Map a root-menu command/params to an SHActionType + payload for an SHBinding
 * (F5-J). Mirrors the branches of addMenuEntry so a shortcut executes the same
 * logic the menu entry would.
 */
static Bool decodeShortcutAction(const char *command, const char *params,
				 SHActionType *type, char **cmd, Bool *quick)
{
	*cmd = NULL;
	*quick = False;

	if (strcmp(command, "EXEC") == 0) {
		*type = RSM_EXEC;
		*cmd = wstrconcat("exec ", params);
	} else if (strcmp(command, "SHEXEC") == 0) {
		*type = RSM_EXEC;
		*cmd = wstrdup(params);
	} else if (strcmp(command, "EXIT") == 0) {
		*type = RSM_EXIT;
		*quick = (params && strcmp(params, "QUICK") == 0);
	} else if (strcmp(command, "SHUTDOWN") == 0) {
		*type = RSM_SHUTDOWN;
		*quick = (params && strcmp(params, "QUICK") == 0);
	} else if (strcmp(command, "REFRESH") == 0) {
		*type = RSM_REFRESH;
	} else if (strcmp(command, "ARRANGE_ICONS") == 0) {
		*type = RSM_ARRANGE_ICONS;
	} else if (strcmp(command, "HIDE_OTHERS") == 0) {
		*type = RSM_HIDE_OTHERS;
	} else if (strcmp(command, "SHOW_ALL") == 0) {
		*type = RSM_SHOW_ALL;
	} else if (strcmp(command, "RESTART") == 0) {
		*type = RSM_RESTART;
		*cmd = wstrdup(params ? params : "");
	} else if (strcmp(command, "SAVE_SESSION") == 0) {
		*type = RSM_SAVE_SESSION;
	} else if (strcmp(command, "CLEAR_SESSION") == 0) {
		*type = RSM_CLEAR_SESSION;
	} else if (strcmp(command, "INFO_PANEL") == 0) {
		*type = RSM_INFO_PANEL;
	} else if (strcmp(command, "LEGAL_PANEL") == 0) {
		*type = RSM_LEGAL_PANEL;
	} else if (strcmp(command, "KEYBINDS_PANEL") == 0) {
		*type = RSM_KEYBINDS_PANEL;
	} else {
		return False;
	}

	return True;
}

static Bool addShortcut(const char *file, const char *shortcutDefinition, WMenu *menu,
			WMenuEntry *entry, const char *command, const char *params)
{
	KeySym ksym;
	char *k;
	char buf[MAX_SHORTCUT_LENGTH], *b;
	SHActionType type;
	char *scmd = NULL;
	Bool squick = False;
	unsigned int modifier = 0;
	KeyCode keycode = 0;

	wstrlcpy(buf, shortcutDefinition, MAX_SHORTCUT_LENGTH);
	b = (char *)buf;

	/* get modifiers */
	while ((k = strchr(b, '+')) != NULL) {
		int mod;

		*k = 0;
		mod = wXModifierFromKey(b);
		if (mod < 0) {
			wwarning(_("%s: invalid key modifier \"%s\""), file, b);
			return False;
		}
		modifier |= mod;

		b = k + 1;
	}

	/* get key */
	ksym = XStringToKeysym(b);

	if (ksym == NoSymbol) {
		wwarning(_("%s:invalid kbd shortcut specification \"%s\" for entry %s"),
			 file, shortcutDefinition, entry->text);
		return False;
	}

	keycode = XKeysymToKeycode(dpy, ksym);
	if (keycode == 0) {
		wwarning(_("%s:invalid key in shortcut \"%s\" for entry %s"), file,
			 shortcutDefinition, entry->text);
		return False;
	}

	menu->vscr->menu.flags.root_menu_changed_shortcuts = 1;

	/* Decode into a menu-independent SHBinding so the key trie (F5-H/I) can
	 * execute the shortcut without any menu pointer. */
	if (decodeShortcutAction(command, params, &type, &scmd, &squick)) {
		SHBinding *sb = wmalloc(sizeof(SHBinding));

		sb->modifier = modifier;
		sb->keycode = keycode;
		sb->chain_length = 1;
		sb->type = type;
		sb->cmd = scmd;
		sb->quick = squick;
		shAddMenuBinding(sb);

		/* Paint the menu's shortcut label from the binding list (F5-M),
		 * not from the raw file string. */
		if (entry) {
			char label[128];

			shLabelFor(sb, label, sizeof(label));
			entry->rtext = wstrdup(label);
		}
	} else {
		wfree(scmd);
	}

	return True;
}

static char *next_token(char *line, char **next)
{
	char *tmp, c;
	char *ret;

	*next = NULL;
	while (*line == ' ' || *line == '\t')
		line++;

	tmp = line;

	if (*tmp == '"') {
		tmp++;
		line++;
		while (*tmp != 0 && *tmp != '"')
			tmp++;
		if (*tmp != '"') {
			wwarning(_("%s: unmatched '\"' in menu file"), line);
			return NULL;
		}
	} else {
		do {
			if (*tmp == '\\')
				tmp++;

			if (*tmp != 0)
				tmp++;

		} while (*tmp != 0 && *tmp != ' ' && *tmp != '\t');
	}

	c = *tmp;
	*tmp = 0;
	ret = wstrdup(line);
	*tmp = c;

	if (c == 0)
		return ret;
	else
		tmp++;

	/* skip blanks */
	while (*tmp == ' ' || *tmp == '\t')
		tmp++;

	if (*tmp != 0)
		*next = tmp;

	return ret;
}

static void separateCommand(char *line, char ***file, char **command)
{
	char *token, *tmp = line;
	WMArray *array = WMCreateArray(4);
	int count, i;

	*file = NULL;
	*command = NULL;
	do {
		token = next_token(tmp, &tmp);
		if (token) {
			if (strcmp(token, "WITH") == 0) {
				if (tmp != NULL && *tmp != 0)
					*command = wstrdup(tmp);
				else
					wwarning(_("%s: missing command"), line);
				wfree(token);
				break;
			}
			WMAddToArray(array, token);
		}
	} while (token != NULL && tmp != NULL);

	count = WMGetArrayItemCount(array);
	if (count > 0) {
		*file = wmalloc(sizeof(char *) * (count + 1));
		(*file)[count] = NULL;
		for (i = 0; i < count; i++) {
			(*file)[i] = WMGetFromArray(array, i);
		}
	}
	WMFreeArray(array);
}

static WMenu *constructPLMenu(virtual_screen *vscr, const char *path)
{
	WMPropList *pl = NULL;
	WMenu *menu = NULL;

	if (!path)
		return NULL;

	pl = WMReadPropListFromFile(path);
	if (!pl)
		return NULL;

	menu = configureMenu(vscr, pl);

	WMReleasePropList(pl);

	if (!menu)
		return NULL;

	return menu;
}



static void constructMenu(WMenu *menu, WMenuEntry *entry)
{
	WMenu *submenu;
	struct stat stat_buf;
	char **path, *cmd, *lpath = NULL;
	int i, first = -1;
	time_t last = 0;

	separateCommand((char *)entry->clientdata, &path, &cmd);
	if (path == NULL || *path == NULL || **path == 0) {
		wwarning(_("invalid OPEN_MENU specification: %s"), (char *)entry->clientdata);
		if (path) {
			for (i = 0; path[i] != NULL; i++)
				wfree(path[i]);
			wfree(path);
		}

		if (cmd)
			wfree(cmd);

		return;
	}

	if (path[0][0] == '|') {
		/* pipe menu */
		if (!menu->cascades[entry->cascade] || menu->cascades[entry->cascade]->timestamp == 0) {
			/* parse pipe */
			submenu = readMenuPipe(menu->vscr, path);

			if (submenu != NULL) {
				if (path[0][1] == '|')
					submenu->timestamp = 0;
				else
					submenu->timestamp = 1;	/* there's no automatic reloading */
			}
		} else {
			submenu = NULL;
		}
	} else {
		/* try interpreting path as a proplist file */
		submenu = constructPLMenu(menu->vscr, path[0]);

		/* if unsuccessful, try it as an old-style file */
		if (!submenu) {
			i = 0;
			while (path[i] != NULL) {
				char *tmp;

				if (strcmp(path[i], "-noext") == 0) {
					i++;
					continue;
				}

				tmp = wexpandpath(path[i]);

				if (strstr(tmp, "#usergnusteppath#") == tmp) {
					char *old_tmp = tmp;

					tmp = wstrconcat(wusergnusteppath(),
							  tmp + 17);
					wfree(old_tmp);
				}

				wfree(path[i]);
				lpath = getLocalizedMenuFile(tmp);
				if (lpath) {
					wfree(tmp);
					path[i] = lpath;
					lpath = NULL;
				} else {
					path[i] = tmp;
				}

				if (stat(path[i], &stat_buf) == 0) {
					if (last < stat_buf.st_mtime)
						last = stat_buf.st_mtime;
					if (first < 0)
						first = i;
				} else {
					werror(_("%s:could not stat menu"), path[i]);
				}

				i++;
			}

			if (first < 0) {
				werror(_("%s:could not stat menu:%s"), "OPEN_MENU", (char *)entry->clientdata);
				i = 0;
				while (path[i] != NULL)
					wfree(path[i++]);

				wfree(path);
				if (cmd)
					wfree(cmd);

				return;
			}

			stat(path[first], &stat_buf);
			if (!menu->cascades[entry->cascade] ||
			    menu->cascades[entry->cascade]->timestamp < last) {
				if (S_ISDIR(stat_buf.st_mode)) {
					/* menu directory */
					submenu = readMenuDirectory(menu->vscr, entry->text, path, cmd);
					if (submenu)
						submenu->timestamp = last;
				} else if (S_ISREG(stat_buf.st_mode)) {
					/* menu file */

					if (cmd || path[1])
						wwarning(_("too many parameters in OPEN_MENU: %s"),
								(char *)entry->clientdata);

					submenu = readMenuFile(menu->vscr, path[first]);
					if (submenu)
						submenu->timestamp = stat_buf.st_mtime;
				} else {
					submenu = NULL;
				}
			} else {
				submenu = NULL;
			}
		}
	}

	if (submenu) {
		wMenuEntryRemoveCascade(menu, entry);
		wMenuEntrySetCascade_create(menu, entry, submenu);
	}

	i = 0;
	while (path[i] != NULL)
		wfree(path[i++]);

	wfree(path);
	if (cmd)
		wfree(cmd);
}

static void constructPLMenuFromPipe(WMenu * menu, WMenuEntry * entry)
{
	WMenu *submenu = NULL;
	char **path;
	char *cmd;
	int i;

	separateCommand((char *)entry->clientdata, &path, &cmd);
	if (path == NULL || *path == NULL || **path == 0) {
		wwarning(_("invalid OPEN_PLMENU specification: %s"),
		    (char *)entry->clientdata);
		if (path) {
			for (i = 0; path[i] != NULL; i++)
				wfree(path[i]);
			wfree(path);
		}
		if (cmd)
			wfree(cmd);
		return;
	}

	if (path[0][0] == '|') {
		/* pipe menu */

		if (!menu->cascades[entry->cascade]
		|| menu->cascades[entry->cascade]->timestamp == 0) {
			/* parse pipe */
			submenu = readPLMenuPipe(menu->vscr, path);

			if (submenu != NULL) {
				if (path[0][1] == '|')
					submenu->timestamp = 0;
				else
					submenu->timestamp = 1;	/* there's no automatic reloading */
			}
		}
	}

	if (submenu) {
		wMenuEntryRemoveCascade(menu, entry);
		wMenuEntrySetCascade_create(menu, entry, submenu);
	}

	i = 0;
	while (path[i] != NULL)
		wfree(path[i++]);

	wfree(path);
	if (cmd)
		wfree(cmd);

}
static void cleanupWorkspaceMenu(WMenu *menu)
{
	if (menu->vscr->workspace.menu == menu)
		menu->vscr->workspace.menu = NULL;
}

static WMenuEntry *addWorkspaceMenu(virtual_screen *vscr, WMenu *menu, const char *title)
{
	WMenuEntry *entry;

	if (vscr->menu.flags.added_workspace_menu) {
		wwarning(_
			 ("There are more than one WORKSPACE_MENU commands in the applications menu. Only one is allowed."));
		return NULL;
	}

	vscr->menu.flags.added_workspace_menu = 1;

	vscr->workspace.menu = wWorkspaceMenuMake(vscr, _("Workspaces"));
	menu_map(vscr->workspace.menu);
	vscr->workspace.menu->on_destroy = cleanupWorkspaceMenu;

	entry = wMenuAddCallback(menu, title, NULL, NULL);
	wMenuEntrySetCascade_create(menu, entry, vscr->workspace.menu);
	wWorkspaceMenuUpdate(vscr, vscr->workspace.menu);
	wWorkspaceMenuUpdate_map(vscr);

	return entry;
}

static void cleanupWindowsMenu(WMenu *menu)
{
	if (menu->vscr->menu.root_switch == menu)
		menu->vscr->menu.root_switch = NULL;
}

static WMenuEntry *addWindowsMenu(virtual_screen *vscr, WMenu *menu, const char *title)
{
	WWindow *wwin;
	WMenuEntry *entry;

	if (vscr->menu.flags.added_window_menu) {
		wwarning(_
			 ("There are more than one WINDOWS_MENU commands in the applications menu. Only one is allowed."));
		return NULL;
	}

	vscr->menu.flags.added_window_menu = 1;

	vscr->menu.root_switch = menu_create(vscr, _("Window List"));
	menu_map(vscr->menu.root_switch);
	vscr->menu.root_switch->on_destroy = cleanupWindowsMenu;
	wwin = vscr->window.focused;
	while (wwin) {
		switchmenu_additem(vscr->menu.root_switch, wwin);
		menu_move_visible(vscr->menu.root_switch);
		wwin = wwin->prev;
	}

	entry = wMenuAddCallback(menu, title, NULL, NULL);
	wMenuEntrySetCascade_create(menu, entry, vscr->menu.root_switch);

	return entry;
}

static WMenuEntry *addMenuEntry(WMenu *menu, const char *title, const char *shortcut, const char *command,
				const char *params, const char *file_name)
{
	virtual_screen *vscr;
	WMenuEntry *entry = NULL;
	Bool shortcutOk = False;

	if (!menu)
		return NULL;

	vscr = menu->vscr;
	if (strcmp(command, "OPEN_MENU") == 0) {
		if (!params) {
			wwarning(_("%s:missing parameter for menu command \"%s\""), file_name, command);
		} else {
			WMenu *dummy;
			char *path;

			path = wfindfile(DEF_CONFIG_PATHS, params);
			if (!path)
				path = wstrdup(params);

			dummy = menu_create(vscr, title);
			menu_map(dummy);
			entry = wMenuAddCallback(menu, title, constructMenu, path);
			entry->free_cdata = wfree;
			wMenuEntrySetCascade_create(menu, entry, dummy);
		}
	} else if (strcmp(command, "OPEN_PLMENU") == 0) {
		if (!params) {
			wwarning(_("%s:missing parameter for menu command \"%s\""), file_name, command);
		} else {
			WMenu *dummy;
			char *path;

			path = wfindfile(DEF_CONFIG_PATHS, params);
			if (!path)
				path = wstrdup(params);

			dummy = menu_create(vscr, title);
			menu_map(dummy);
			entry = wMenuAddCallback(menu, title, constructPLMenuFromPipe, path);
			entry->free_cdata = wfree;
			wMenuEntrySetCascade_create(menu, entry, dummy);
		}
	} else if (strcmp(command, "EXEC") == 0) {
		if (!params)
			wwarning(_("%s:missing parameter for menu command \"%s\""), file_name, command);
		else {
			entry = wMenuAddCallback(menu, title, execCommand, wstrconcat("exec ", params));
			entry->free_cdata = wfree;
			shortcutOk = True;
		}
	} else if (strcmp(command, "SHEXEC") == 0) {
		if (!params)
			wwarning(_("%s:missing parameter for menu command \"%s\""), file_name, command);
		else {
			entry = wMenuAddCallback(menu, title, execCommand, wstrdup(params));
			entry->free_cdata = wfree;
			shortcutOk = True;
		}
	} else if (strcmp(command, "EXIT") == 0) {

		if (params && strcmp(params, "QUICK") == 0)
			entry = wMenuAddCallback(menu, title, exitCommand, (void *)M_QUICK);
		else
			entry = wMenuAddCallback(menu, title, exitCommand, NULL);

		shortcutOk = True;
	} else if (strcmp(command, "SHUTDOWN") == 0) {

		if (params && strcmp(params, "QUICK") == 0)
			entry = wMenuAddCallback(menu, title, shutdownCommand, (void *)M_QUICK);
		else
			entry = wMenuAddCallback(menu, title, shutdownCommand, NULL);

		shortcutOk = True;
	} else if (strcmp(command, "REFRESH") == 0) {
		entry = wMenuAddCallback(menu, title, refreshCommand, NULL);

		shortcutOk = True;
	} else if (strcmp(command, "WORKSPACE_MENU") == 0) {
		entry = addWorkspaceMenu(vscr, menu, title);
	} else if (strcmp(command, "WINDOWS_MENU") == 0) {
		entry = addWindowsMenu(vscr, menu, title);
	} else if (strcmp(command, "ARRANGE_ICONS") == 0) {
		entry = wMenuAddCallback(menu, title, arrangeIconsCommand, NULL);

		shortcutOk = True;
	} else if (strcmp(command, "HIDE_OTHERS") == 0) {
		entry = wMenuAddCallback(menu, title, hideOthersCommand, NULL);

		shortcutOk = True;
	} else if (strcmp(command, "SHOW_ALL") == 0) {
		entry = wMenuAddCallback(menu, title, showAllCommand, NULL);

		shortcutOk = True;
	} else if (strcmp(command, "RESTART") == 0) {
		entry = wMenuAddCallback(menu, title, restartCommand, params ? wstrdup(params) : NULL);
		entry->free_cdata = wfree;
		shortcutOk = True;
	} else if (strcmp(command, "SAVE_SESSION") == 0) {
		entry = wMenuAddCallback(menu, title, saveSessionCommand, NULL);

		shortcutOk = True;
	} else if (strcmp(command, "CLEAR_SESSION") == 0) {
		entry = wMenuAddCallback(menu, title, clearSessionCommand, NULL);
		shortcutOk = True;
	} else if (strcmp(command, "INFO_PANEL") == 0) {
		entry = wMenuAddCallback(menu, title, infoPanelCommand, NULL);
		shortcutOk = True;
	} else if (strcmp(command, "LEGAL_PANEL") == 0) {
		entry = wMenuAddCallback(menu, title, legalPanelCommand, NULL);
		shortcutOk = True;
	} else if (strcmp(command, "KEYBINDS_PANEL") == 0) {
		entry = wMenuAddCallback(menu, title, keybindsPanelCommand, NULL);
		shortcutOk = True;
	} else {
		wwarning(_("%s:unknown command \"%s\" in menu config."), file_name, command);

		return NULL;
	}

	if (shortcut && entry) {
		if (!shortcutOk) {
			wwarning(_("%s:can't add shortcut for entry \"%s\""), file_name, title);
		} else {
			addShortcut(file_name, shortcut, menu, entry, command, params);
		}
	}

	return entry;
}

/*******************   Menu Configuration From File   *******************/

static void freeline(char *title, char *command, char *parameter, char *shortcut)
{
	wfree(title);
	wfree(command);
	wfree(parameter);
	wfree(shortcut);
}

static WMenu *parseCascade(virtual_screen *vscr, WMenu *menu, WMenuParser parser)
{
	char *command, *params, *shortcut, *title;

	while (WMenuParserGetLine(parser, &title, &command, &params, &shortcut)) {
		if (command == NULL || !command[0]) {
			WMenuParserError(parser, _("missing command in menu config") );
			freeline(title, command, params, shortcut);
			return NULL;
		}

		if (strcasecmp(command, "MENU") == 0) {
			WMenu *cascade;

			/* start submenu */
			cascade = menu_create(vscr, M_(title));
			menu_map(cascade);
			if (!parseCascade(vscr, cascade, parser))
				wMenuDestroy(cascade);
			else
				wMenuEntrySetCascade_create(menu, wMenuAddCallback(menu, M_(title), NULL, NULL), cascade);

		} else if (strcasecmp(command, "END") == 0) {
			/* end of menu */
			freeline(title, command, params, shortcut);
			return menu;
		} else {
			/* normal items */
			addMenuEntry(menu, M_(title), shortcut, command, params, WMenuParserGetFilename(parser));
		}

		freeline(title, command, params, shortcut);
	}

	WMenuParserError(parser, _("syntax error in menu file: END declaration missing") );

	return NULL;
}

static WMenu *readMenu(virtual_screen *vscr, const char *flat_file, FILE *file)
{
	WMenu *menu = NULL;
	WMenuParser parser;
	char *title, *command, *params, *shortcut;

	parser = WMenuParserCreate(flat_file, file, DEF_CONFIG_PATHS);
	menu_parser_register_macros(parser);

	while (WMenuParserGetLine(parser, &title, &command, &params, &shortcut)) {

		if (command == NULL || !command[0]) {
			WMenuParserError(parser, _("missing command in menu config") );
			freeline(title, command, params, shortcut);
			break;
		}

		if (strcasecmp(command, "MENU") == 0) {
			menu = menu_create(vscr, M_(title));
			menu_map(menu);
					if (!parseCascade(vscr, menu, parser)) {
				wMenuDestroy(menu);
				menu = NULL;
			}

			freeline(title, command, params, shortcut);
			break;
		} else {
			WMenuParserError(parser, _("invalid menu, no menu title given") );
			freeline(title, command, params, shortcut);
			break;
		}

		freeline(title, command, params, shortcut);
	}

	WMenuParserDelete(parser);
	return menu;
}

static WMenu *readMenuFile(virtual_screen *vscr, const char *file_name)
{
	WMenu *menu = NULL;
	FILE *file = NULL;

	file = fopen(file_name, "rb");
	if (!file) {
		werror(_("could not open menu file \"%s\": %s"), file_name, strerror(errno));
		return NULL;
	}

	menu = readMenu(vscr, file_name, file);
	fclose(file);

	return menu;
}

static inline int generate_command_from_list(char *buffer, size_t buffer_size, char **command_elements)
{
	char *rd;
	int wr_idx;
	int i;

	wr_idx = 0;
	for (i = 0; command_elements[i] != NULL; i++) {

		if (i > 0)
			if (wr_idx < buffer_size - 1)
				buffer[wr_idx++] = ' ';

		for (rd = command_elements[i]; *rd != '\0'; rd++) {
			if (wr_idx < buffer_size - 1)
				buffer[wr_idx++] = *rd;
			else
				return 1;
		}
	}
	buffer[wr_idx] = '\0';
	return 0;
}

/************    Menu Configuration From Pipe      *************/
static WMenu *readPLMenuPipe(virtual_screen *vscr, char **file_name)
{
	WMPropList *plist = NULL;
	WMenu *menu = NULL;
	char *filename;
	char flat_file[MAXLINE];

	if (generate_command_from_list(flat_file, sizeof(flat_file), file_name)) {
		werror(_("could not open menu file \"%s\": %s"),
		       file_name[0], _("pipe command for PropertyList is too long"));
		return NULL;
	}

	filename = flat_file + (flat_file[1] == '|' ? 2 : 1);
	plist = WMReadPropListFromPipe(filename);
	if (!plist)
		return NULL;

	menu = configureMenu(vscr, plist);

	WMReleasePropList(plist);

	if (!menu)
		return NULL;

	return menu;
}

static WMenu *readMenuPipe(virtual_screen *vscr, char **file_name)
{
	WMenu *menu = NULL;
	FILE *file = NULL;
	char *filename;
	char flat_file[MAXLINE];

	if (generate_command_from_list(flat_file, sizeof(flat_file), file_name)) {
		werror(_("could not open menu file \"%s\": %s"),
		       file_name[0], _("pipe command is too long"));
		return NULL;
	}

	filename = flat_file + (flat_file[1] == '|' ? 2 : 1);

	/*
	 * In case of memory problem, 'popen' will not set the errno, so we initialise it
	 * to be able to display a meaningful message. For other problems, 'popen' will
	 * properly set errno, so we'll still get a good message
	 */
	errno = ENOMEM;
	file = popen(filename, "r");
	if (!file) {
		werror(_("could not open menu file \"%s\": %s"), filename, strerror(errno));
		return NULL;
	}

	menu = readMenu(vscr, flat_file, file);
	pclose(file);

	return menu;
}

typedef struct {
	char *name;
	int index;
} dir_data;

static int myCompare(const void *d1, const void *d2)
{
	dir_data *p1 = *(dir_data **) d1;
	dir_data *p2 = *(dir_data **) d2;

	return strcmp(p1->name, p2->name);
}

/***** Preset some macro for file parser *****/
static void menu_parser_register_macros(WMenuParser parser)
{
	Visual *visual;
	char buf[32];

	// Used to return CPP verion, now returns wmaker's version
	WMenuParserRegisterSimpleMacro(parser, "__VERSION__", VERSION);

	// All macros below were historically defined by WindowMaker
	visual = DefaultVisual(dpy, DefaultScreen(dpy));
	snprintf(buf, sizeof(buf), "%d", visual->class);
	WMenuParserRegisterSimpleMacro(parser, "VISUAL", buf);

	snprintf(buf, sizeof(buf), "%d", DefaultDepth(dpy, DefaultScreen(dpy)) );
	WMenuParserRegisterSimpleMacro(parser, "DEPTH", buf);

	snprintf(buf, sizeof(buf), "%d", WidthOfScreen(DefaultScreenOfDisplay(dpy)) );
	WMenuParserRegisterSimpleMacro(parser, "SCR_WIDTH", buf);

	snprintf(buf, sizeof(buf), "%d", HeightOfScreen(DefaultScreenOfDisplay(dpy)) );
	WMenuParserRegisterSimpleMacro(parser, "SCR_HEIGHT", buf);

	WMenuParserRegisterSimpleMacro(parser, "DISPLAY", XDisplayName(DisplayString(dpy)) );

	WMenuParserRegisterSimpleMacro(parser, "WM_VERSION", "\"" VERSION "\"");
}

/************  Menu Configuration From Directory   *************/

static Bool isFilePackage(const char *file)
{
	int l;

	/* check if the extension indicates this file is a
	 * file package. For now, only recognize .themed */

	l = strlen(file);

	if (l > 7 && strcmp(&(file[l - 7]), ".themed") == 0)
		return True;

	return False;
}

static WMenu *readMenuDirectory(virtual_screen *vscr, const char *title, char **path, const char *command)
{
	DIR *dir;
	struct dirent *dentry;
	struct stat stat_buf;
	WMenu *menu = NULL;
	char *buffer;
	WMArray *dirs = NULL, *files = NULL;
	WMArrayIterator iter;
	int length, i, have_space = 0;
	dir_data *data;
	int stripExtension = 0;

	dirs = WMCreateArray(16);
	files = WMCreateArray(16);

	i = 0;
	while (path[i] != NULL) {
		if (strcmp(path[i], "-noext") == 0) {
			stripExtension = 1;
			i++;
			continue;
		}

		dir = opendir(path[i]);
		if (!dir) {
			i++;
			continue;
		}

		while ((dentry = readdir(dir))) {

			if (strcmp(dentry->d_name, ".") == 0 || strcmp(dentry->d_name, "..") == 0)
				continue;

			if (dentry->d_name[0] == '.')
				continue;

			buffer = malloc(strlen(path[i]) + strlen(dentry->d_name) + 4);
			if (!buffer) {
				werror(_("out of memory while constructing directory menu %s"), path[i]);
				break;
			}

			strcpy(buffer, path[i]);
			strcat(buffer, "/");
			strcat(buffer, dentry->d_name);

			if (stat(buffer, &stat_buf) != 0) {
				werror(_("%s:could not stat file \"%s\" in menu directory"),
					  path[i], dentry->d_name);
			} else {
				Bool isFilePack = False;

				data = NULL;
				if (S_ISDIR(stat_buf.st_mode)
				    && !(isFilePack = isFilePackage(dentry->d_name))) {

					/* access always returns success for user root */
					if (access(buffer, X_OK) == 0) {
						/* Directory is accesible. Add to directory list */

						data = (dir_data *) wmalloc(sizeof(dir_data));
						data->name = wstrdup(dentry->d_name);
						data->index = i;

						WMAddToArray(dirs, data);
					}
				} else if (S_ISREG(stat_buf.st_mode) || isFilePack) {
					/* Hack because access always returns X_OK success for user root */
#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)
					if ((command != NULL && access(buffer, R_OK) == 0) ||
					    (command == NULL && access(buffer, X_OK) == 0 &&
					     (stat_buf.st_mode & S_IXANY))) {

						data = (dir_data *) wmalloc(sizeof(dir_data));
						data->name = wstrdup(dentry->d_name);
						data->index = i;

						WMAddToArray(files, data);
					}
				}
			}
			wfree(buffer);
		}

		closedir(dir);
		i++;
	}

	if (!WMGetArrayItemCount(dirs) && !WMGetArrayItemCount(files)) {
		WMFreeArray(dirs);
		WMFreeArray(files);
		return NULL;
	}

	WMSortArray(dirs, myCompare);
	WMSortArray(files, myCompare);

	menu = menu_create(vscr, M_(title));
	menu_map(menu);

	WM_ITERATE_ARRAY(dirs, data, iter) {
		/* New directory. Use same OPEN_MENU command that was used
		 * for the current directory. */
		length = strlen(path[data->index]) + strlen(data->name) + 6;
		if (stripExtension)
			length += 7;
		if (command)
			length += strlen(command) + 6;
		buffer = malloc(length);
		if (!buffer) {
			werror(_("out of memory while constructing directory menu %s"), path[data->index]);
			break;
		}

		buffer[0] = '\0';
		if (stripExtension)
			strcat(buffer, "-noext ");

		have_space = strchr(path[data->index], ' ') != NULL || strchr(data->name, ' ') != NULL;

		if (have_space)
			strcat(buffer, "\"");
		strcat(buffer, path[data->index]);

		strcat(buffer, "/");
		strcat(buffer, data->name);
		if (have_space)
			strcat(buffer, "\"");
		if (command) {
			strcat(buffer, " WITH ");
			strcat(buffer, command);
		}

		addMenuEntry(menu, M_(data->name), NULL, "OPEN_MENU", buffer, path[data->index]);

		wfree(buffer);
		wfree(data->name);
		wfree(data);
	}

	WM_ITERATE_ARRAY(files, data, iter) {
		/* executable: add as entry */
		length = strlen(path[data->index]) + strlen(data->name) + 6;
		if (command)
			length += strlen(command);

		buffer = malloc(length);
		if (!buffer) {
			werror(_("out of memory while constructing directory menu %s"), path[data->index]);
			break;
		}

		have_space = strchr(path[data->index], ' ') != NULL || strchr(data->name, ' ') != NULL;
		if (command != NULL) {
			strcpy(buffer, command);
			strcat(buffer, " ");
			if (have_space)
				strcat(buffer, "\"");
			strcat(buffer, path[data->index]);
		} else {
			if (have_space) {
				buffer[0] = '"';
				buffer[1] = 0;
				strcat(buffer, path[data->index]);
			} else {
				strcpy(buffer, path[data->index]);
			}
		}
		strcat(buffer, "/");
		strcat(buffer, data->name);
		if (have_space)
			strcat(buffer, "\"");

		if (stripExtension) {
			char *ptr = strrchr(data->name, '.');
			if (ptr && ptr != data->name)
				*ptr = 0;
		}
		addMenuEntry(menu, M_(data->name), NULL, "SHEXEC", buffer, path[data->index]);

		wfree(buffer);
		wfree(data->name);
		wfree(data);
	}

	WMFreeArray(files);
	WMFreeArray(dirs);

	return menu;
}

/************  Menu Configuration From WMRootMenu   *************/

static WMenu *makeDefaultMenu(virtual_screen *vscr)
{
	WMenu *menu = NULL;

	wMessageDialog(vscr, _("Error"),
		       _("The applications menu could not be loaded. "
			 "Look at the console output for a detailed "
			 "description of the errors."), _("OK"), NULL, NULL);

	menu = menu_create(vscr, _("Commands"));

	wMenuAddCallback(menu, M_("XTerm"), execCommand, "xterm");
	wMenuAddCallback(menu, M_("rxvt"), execCommand, "rxvt");
	wMenuAddCallback(menu, _("Restart"), restartCommand, NULL);
	wMenuAddCallback(menu, _("Exit..."), exitCommand, NULL);

	menu_map(menu);

	return menu;
}

static WMenu *configure_plstring_menu(virtual_screen *vscr, WMPropList *definition)
{
	WMenu *menu = NULL;
	struct stat stat_buf;
	char *tmp, *path = NULL;
	Bool menu_is_default = False;

	/* menu definition is a string. Probably a path, so parse the file */
	tmp = wexpandpath(WMGetFromPLString(definition));
	path = getLocalizedMenuFile(tmp);
	if (!path)
		path = wfindfile(DEF_CONFIG_PATHS, tmp);

	if (!path) {
		path = wfindfile(DEF_CONFIG_PATHS, DEF_MENU_FILE);
		menu_is_default = True;
	}

	if (!path) {
		werror(_("could not find menu file \"%s\" referenced in WMRootMenu"), tmp);
		wfree(tmp);
		return NULL;
	}

	if (stat(path, &stat_buf) < 0) {
		werror(_("could not access menu \"%s\" referenced in WMRootMenu"), path);
		wfree(path);
		wfree(tmp);
		return NULL;
	}

	if (!vscr->menu.root_menu ||
	    stat_buf.st_mtime > vscr->menu.root_menu->timestamp ||
	    /* if the pointer in WMRootMenu has changed */
	    w_global.domain.root_menu->timestamp > vscr->menu.root_menu->timestamp) {
		WMPropList *menu_from_file = NULL;

		if (menu_is_default) {
			wwarning(_
				 ("using default menu file \"%s\" as the menu referenced in WMRootMenu could not be found "),
				 path);
		}

		menu_from_file = WMReadPropListFromFile(path);
		if (menu_from_file == NULL) { /* old style menu */
			menu = readMenuFile(vscr, path);
		} else {
			menu = configureMenu(vscr, menu_from_file);
			WMReleasePropList(menu_from_file);
		}

		if (menu)
			menu->timestamp = WMAX(stat_buf.st_mtime, w_global.domain.root_menu->timestamp);
	} else {
		menu = NULL;
	}

	wfree(path);
	wfree(tmp);

	return menu;
}

static void configure_menu_entries(virtual_screen *vscr, WMPropList *definition, WMenu *menu, int count)
{
	WMPropList *elem;
	WMPropList *title, *command, *params;
	char *tmp;
	int i;

	for (i = 1; i < count; i++) {
		elem = WMGetFromPLArray(definition, i);
		if (!WMIsPLArray(elem) || WMGetPropListItemCount(elem) < 2) {
			tmp = WMGetPropListDescription(elem, False);
			wwarning(_("%s:format error in root menu configuration \"%s\""), "WMRootMenu", tmp);
			wfree(tmp);
			continue;
		}

		if (WMIsPLArray(WMGetFromPLArray(elem, 1))) {
			WMenu *submenu;
			WMenuEntry *mentry;

			/* submenu */
			submenu = configureMenu(vscr, elem);
			if (submenu) {
				mentry = wMenuAddCallback(menu, submenu->title, NULL, NULL);
				wMenuEntrySetCascade_create(menu, mentry, submenu);
			}
		} else {
			int idx = 0;
			WMPropList *shortcut;
			/* normal entry */

			title = WMGetFromPLArray(elem, idx++);
			shortcut = WMGetFromPLArray(elem, idx++);
			if (strcmp(WMGetFromPLString(shortcut), "SHORTCUT") == 0) {
				shortcut = WMGetFromPLArray(elem, idx++);
				command = WMGetFromPLArray(elem, idx++);
			} else {
				command = shortcut;
				shortcut = NULL;
			}

			params = WMGetFromPLArray(elem, idx++);
			if (!title || !command) {
				tmp = WMGetPropListDescription(elem, False);
				wwarning(_("%s:format error in root menu configuration \"%s\""), "WMRootMenu", tmp);
				wfree(tmp);
				continue;
			}

			addMenuEntry(menu, M_(WMGetFromPLString(title)),
				     shortcut ? WMGetFromPLString(shortcut) : NULL,
				     WMGetFromPLString(command),
				     params ? WMGetFromPLString(params) : NULL, "WMRootMenu");
		}
	}
}

/*
 *----------------------------------------------------------------------
 * configureMenu--
 * 	Reads root menu configuration from defaults database.
 *
 *----------------------------------------------------------------------
 */
static WMenu *configureMenu(virtual_screen *vscr, WMPropList *definition)
{
	WMenu *menu = NULL;
	WMPropList *elem;
	int count;
	char *tmp, *mtitle;

	if (WMIsPLString(definition))
		return configure_plstring_menu(vscr, definition);

	count = WMGetPropListItemCount(definition);
	if (count == 0)
		return NULL;

	elem = WMGetFromPLArray(definition, 0);
	if (!WMIsPLString(elem)) {
		tmp = WMGetPropListDescription(elem, False);
		wwarning(_("%s:format error in root menu configuration \"%s\""), "WMRootMenu", tmp);
		wfree(tmp);
		return NULL;
	}

	mtitle = WMGetFromPLString(elem);
	menu = menu_create(vscr, M_(mtitle));
	menu_map(menu);

	configure_menu_entries(vscr, definition, menu, count);
	wMenuRealize(menu);

	return menu;
}

WMenu *create_rootmenu(virtual_screen *vscr)
{
	WMenu *menu = NULL;
	WMPropList *definition;

	vscr->menu.flags.root_menu_changed_shortcuts = 0;
	vscr->menu.flags.added_workspace_menu = 0;
	vscr->menu.flags.added_window_menu = 0;

	rootmenu_setup_switchmenu_notif();

	definition = w_global.domain.root_menu->dictionary;
	if (!definition || !WMIsPLArray(definition)) {
		/* No usable root-menu definition (file missing or malformed): fall
		 * back to the built-in minimal menu instead of returning NULL, which
		 * would make restore_rootmenu() dereference a NULL menu and crash. */
		menu = makeDefaultMenu(vscr);
		return menu;
	}

	menu = configureMenu(vscr, definition);
	if (!menu)
		menu = makeDefaultMenu(vscr);

	return menu;
}

void rootmenu_destroy(virtual_screen *vscr)
{
	if (!vscr->menu.root_menu)
		return;

	WMRemoveNotificationObserver(vscr->menu.root_menu);
	wMenuDestroy(vscr->menu.root_menu);
	vscr->menu.root_menu = NULL;
	vscr->menu.flags.root_menu_changed_shortcuts = 0;
	vscr->menu.flags.added_workspace_menu = 0;
	vscr->menu.flags.added_window_menu = 0;

	/* Drop the menu's shortcut bindings and rebuild the trie (F5-K): only
	 * when the menu is actually (re)built, never reentrantly on open/close. */
	shClearMenuBindings();
	wKeyTreeRebuild();
}

/*
 *----------------------------------------------------------------------
 * OpenRootMenu--
 * 	Opens the root menu, parsing the menu configuration from the
 * defaults database.
 *	If the menu is already mapped and is not sticked to the
 * root window, it will be unmapped.
 *
 * Side effects:
 * 	The menu may be remade.
 *
 * Notes:
 * Construction of OPEN_MENU entries are delayed to the moment the
 * user map's them.
 *----------------------------------------------------------------------
 */
void OpenRootMenu(virtual_screen *vscr, int x, int y, int keyboard)
{
	WMenu *rootmenu = NULL;

	if (vscr->menu.root_menu && vscr->menu.root_menu->flags.mapped) {
		rootmenu = vscr->menu.root_menu;
		if (!rootmenu->flags.buttoned) {
			rootmenu_destroy(vscr);
		} else {
			wRaiseFrame(vscr, rootmenu->frame->core);

			if (keyboard) {
				rootmenu->x_pos = 0;
				rootmenu->y_pos = 0;
				wMenuMapAt(vscr, rootmenu, True);
			}
		}
		return;
	}

	/* Rebuild the root menu when it was never created or the WMRootMenu
	 * definition changed on disk since it was last built, so that editing
	 * the menu takes effect without restarting the WM. */
	if (!vscr->menu.root_menu ||
	    w_global.domain.root_menu->timestamp > vscr->menu.root_menu->timestamp) {
		if (vscr->menu.root_menu)
			rootmenu_destroy(vscr);
		vscr->menu.root_menu = create_rootmenu(vscr);
		if (vscr->menu.root_menu)
			vscr->menu.root_menu->timestamp = w_global.domain.root_menu->timestamp;

		/* The menu was (re)built: merge its freshly-parsed shortcuts into
		 * the runtime list and rebuild the key trie (F5-K). This happens on
		 * config change / first build, not on every open/close. */
		shRebuildList();
		wKeyTreeRebuild();
	}

	rootmenu = vscr->menu.root_menu;
	if (!rootmenu)
		return;

	vscr->menu.root_menu->x_pos = x;
	vscr->menu.root_menu->y_pos = y;
	rootmenu_map(vscr, keyboard);

	if (vscr->menu.flags.root_menu_changed_shortcuts)
		rebindKeygrabs(vscr);
}

static void rootmenu_map(virtual_screen *vscr, int keyboard)
{
	int x, y, newx, newy;

	if (!vscr->menu.root_menu)
		return;

	x = vscr->menu.root_menu->x_pos;
	y = vscr->menu.root_menu->y_pos;

	if (keyboard && x == 0 && y == 0) {
		newx = newy = 0;
	} else if (keyboard &&
		   x == vscr->screen_ptr->scr_width / 2 &&
		   y == vscr->screen_ptr->scr_height / 2) {
		newx = x - vscr->menu.root_menu->frame->width / 2;
		newy = y - vscr->menu.root_menu->frame->height / 2;
	} else {
		newx = x - vscr->menu.root_menu->frame->width / 2;
		newy = y;
	}

	vscr->menu.root_menu->x_pos = newx;
	vscr->menu.root_menu->y_pos = newy;
	wMenuMapAt(vscr, vscr->menu.root_menu, keyboard);
}

static void rootmenu_setup_switchmenu_notif(void)
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

static void observer(void *self, WMNotification *notif)
{
	WWindow *wwin = (WWindow *) WMGetNotificationObject(notif);
	const char *name = WMGetNotificationName(notif);
	void *data = WMGetNotificationClientData(notif);

	/* Parameter not used, but tell the compiler that it is ok */
	(void) self;

	if (!wwin)
		return;

	/* TODO: kix: I am not sure about this, here en rootmenu */
	switchmenu_handle_notification_wwin(wwin->vscr->menu.root_switch,
					    wwin, name, (char *) data);
}

static void wsobserver(void *self, WMNotification *notif)
{
	virtual_screen *vscr = (virtual_screen *) WMGetNotificationObject(notif);
	const char *name = WMGetNotificationName(notif);
	void *data = WMGetNotificationClientData(notif);
	int workspace = (uintptr_t) data;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) self;

	switchmenu_handle_notification(vscr->menu.root_switch, name, workspace);
}
