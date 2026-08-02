/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
 * and individual contributors; see LICENSE for full attribution.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "WindowMaker.h"
#include "screen.h"
#include "defaults.h"

#include <WINGs/WUtil.h>

/*
 * Create the user configuration files (and the directory tree that holds
 * them) when they are missing. This lets awmaker start with a minimal but
 * valid configuration without requiring an external installer script
 * (wmaker.inst) or pre-existing files.
 *
 * The files written here mirror the ones the old installer would have
 * installed; values that are not explicitly present fall back to the defaults
 * compiled into the binary (see defaults.c), so a small file is enough.
 *
 * Returns True on success (or when everything already existed), False on error.
 */

/* Recursively create the directory `path' (including intermediates). */
static Bool wmkdir(const char *path)
{
	char *dir = wstrdup(path);
	char *p;
	struct stat st;

	for (p = dir + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(dir, 0777) == -1 && errno != EEXIST) {
				wfree(dir);
				return False;
			}
			*p = '/';
		}
	}
	if (mkdir(dir, 0777) == -1 && errno != EEXIST) {
		if (stat(dir, &st) == -1 || !S_ISDIR(st.st_mode)) {
			wfree(dir);
			return False;
		}
	}
	wfree(dir);
	return True;
}

/* Write `plist' to `path' only if the file does not already exist. */
static Bool write_file_if_missing(const char *path, WMPropList *plist)
{
	struct stat st;

	if (stat(path, &st) == 0)
		return True;

	return WMWritePropListToFile(plist, path);
}

/*
 * Minimal WMRootMenu: a root system menu with a terminal, the WPrefs
 * configurator, workspaces and a commands/session section.
 */
static Bool write_root_menu(const char *path)
{
	WMPropList *menu, *sub, *entry;
	Bool ok;

	menu = WMCreatePLArray(WMCreatePLString(_("Window Maker")), NULL);

	entry = WMCreatePLArray(WMCreatePLString(_("XTerm")),
				WMCreatePLString("EXEC"),
				WMCreatePLString("xterm -sb"), NULL);
	WMAddToPLArray(menu, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Configure Window Maker")),
				WMCreatePLString("EXEC"),
				WMCreatePLString("WPrefs"), NULL);
	WMAddToPLArray(menu, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Workspaces")),
				WMCreatePLString("WORKSPACE_MENU"), NULL);
	WMAddToPLArray(menu, entry);
	WMReleasePropList(entry);

	sub = WMCreatePLArray(WMCreatePLString(_("Commands")), NULL);

	entry = WMCreatePLArray(WMCreatePLString(_("Hide Others")),
				WMCreatePLString("HIDE_OTHERS"), NULL);
	WMAddToPLArray(sub, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Show All")),
				WMCreatePLString("SHOW_ALL"), NULL);
	WMAddToPLArray(sub, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Arrange Icons")),
				WMCreatePLString("ARRANGE_ICONS"), NULL);
	WMAddToPLArray(sub, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Refresh")),
				WMCreatePLString("REFRESH"), NULL);
	WMAddToPLArray(sub, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Save Session")),
				WMCreatePLString("SAVE_SESSION"), NULL);
	WMAddToPLArray(sub, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Clear Session")),
				WMCreatePLString("CLEAR_SESSION"), NULL);
	WMAddToPLArray(sub, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Restart")),
				WMCreatePLString("RESTART"), NULL);
	WMAddToPLArray(sub, entry);
	WMReleasePropList(entry);

	entry = WMCreatePLArray(WMCreatePLString(_("Exit...")),
				WMCreatePLString("EXIT"), NULL);
	WMAddToPLArray(sub, entry);
	WMReleasePropList(entry);

	WMAddToPLArray(menu, sub);
	WMReleasePropList(sub);

	ok = write_file_if_missing(path, menu);
	WMReleasePropList(menu);
	return ok;
}

/*
 * Minimal WMState: a dock with the WPrefs configurator and a terminal, a clip,
 * an (empty) drawer list and a default workspace.
 */
static Bool write_wm_state(const char *path)
{
	WMPropList *root, *dock, *apps, *app, *clip, *ws, *state, *arr;
	Bool ok;

	root = WMCreatePLDictionary(NULL, NULL);

	/* Dock */
	dock = WMCreatePLDictionary(NULL, NULL);
	apps = WMCreatePLArray(NULL, NULL);

	app = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(app, WMCreatePLString("Command"),
			    WMCreatePLString("WPrefs"));
	WMPutInPLDictionary(app, WMCreatePLString("Name"),
			    WMCreatePLString("Logo.WMDock"));
	WMPutInPLDictionary(app, WMCreatePLString("AutoLaunch"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(app, WMCreatePLString("Forced"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(app, WMCreatePLString("Position"),
			    WMCreatePLString("0,0"));
	WMPutInPLDictionary(app, WMCreatePLString("Lock"),
			    WMCreatePLString("Yes"));
	WMAddToPLArray(apps, app);
	WMReleasePropList(app);

	app = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(app, WMCreatePLString("Command"),
			    WMCreatePLString("xterm"));
	WMPutInPLDictionary(app, WMCreatePLString("Name"),
			    WMCreatePLString("xterm.XTerm"));
	WMPutInPLDictionary(app, WMCreatePLString("AutoLaunch"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(app, WMCreatePLString("Forced"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(app, WMCreatePLString("Position"),
			    WMCreatePLString("0,1"));
	WMAddToPLArray(apps, app);
	WMReleasePropList(app);

	WMPutInPLDictionary(dock, WMCreatePLString("Applications"), apps);
	WMReleasePropList(apps);
	WMPutInPLDictionary(dock, WMCreatePLString("Position"),
			    WMCreatePLString("-64,0"));
	WMPutInPLDictionary(dock, WMCreatePLString("Lowered"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(root, WMCreatePLString("Dock"), dock);
	WMReleasePropList(dock);

	/* Clip */
	clip = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(clip, WMCreatePLString("Command"),
			    WMCreatePLString("-"));
	WMPutInPLDictionary(clip, WMCreatePLString("Name"),
			    WMCreatePLString("Logo.WMClip"));
	WMPutInPLDictionary(clip, WMCreatePLString("AutoLaunch"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(clip, WMCreatePLString("StartHidden"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(clip, WMCreatePLString("StartMiniaturized"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(clip, WMCreatePLString("Forced"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(clip, WMCreatePLString("Position"),
			    WMCreatePLString("0,0"));
	WMPutInPLDictionary(clip, WMCreatePLString("DropCommand"),
			    WMCreatePLString("wmsetbg -u -t %d"));
	WMPutInPLDictionary(root, WMCreatePLString("Clip"), clip);
	WMReleasePropList(clip);

	/* Drawers (empty) */
	WMPutInPLDictionary(root, WMCreatePLString("Drawers"),
			    WMCreatePLArray(NULL, NULL));

	/* Default workspace, "Main" */
	ws = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(ws, WMCreatePLString("Name"),
			    WMCreatePLString("Main"));
	state = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(state, WMCreatePLString("Applications"),
			    WMCreatePLArray(NULL, NULL));
	WMPutInPLDictionary(state, WMCreatePLString("Lowered"),
			    WMCreatePLString("Yes"));
	WMPutInPLDictionary(state, WMCreatePLString("Collapsed"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(state, WMCreatePLString("AutoAttractIcons"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(state, WMCreatePLString("KeepAttracted"),
			    WMCreatePLString("No"));
	WMPutInPLDictionary(ws, WMCreatePLString("Clip"), state);
	WMReleasePropList(state);
	arr = WMCreatePLArray(ws, NULL);
	WMReleasePropList(ws);
	WMPutInPLDictionary(root, WMCreatePLString("Workspaces"), arr);
	WMReleasePropList(arr);

	ok = write_file_if_missing(path, root);
	WMReleasePropList(root);
	return ok;
}

/*
 * Minimal WMWindowAttributes: icons for the dock, panel, clip and drawer, plus
 * a default fallback icon.
 */
static Bool write_window_attributes(const char *path)
{
	WMPropList *root, *wmd;
	Bool ok;

	root = WMCreatePLDictionary(NULL, NULL);

	wmd = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(wmd, WMCreatePLString("Icon"),
			    WMCreatePLString("GNUstepGlow.tiff"));
	WMPutInPLDictionary(root, WMCreatePLString("Logo.WMDock"), wmd);
	WMReleasePropList(wmd);

	wmd = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(wmd, WMCreatePLString("Icon"),
			    WMCreatePLString("GNUstep.tiff"));
	WMPutInPLDictionary(root, WMCreatePLString("Logo.WMPanel"), wmd);
	WMReleasePropList(wmd);

	wmd = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(wmd, WMCreatePLString("Icon"),
			    WMCreatePLString("clip.tiff"));
	WMPutInPLDictionary(root, WMCreatePLString("Logo.WMClip"), wmd);
	WMReleasePropList(wmd);

	wmd = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(wmd, WMCreatePLString("Icon"),
			    WMCreatePLString("Drawer.tiff"));
	WMPutInPLDictionary(root, WMCreatePLString("WMDrawer"), wmd);
	WMReleasePropList(wmd);

	wmd = WMCreatePLDictionary(NULL, NULL);
	WMPutInPLDictionary(wmd, WMCreatePLString("Icon"),
			    WMCreatePLString("defaultAppIcon.tiff"));
	WMPutInPLDictionary(wmd, WMCreatePLString("SharedAppIcon"),
			    WMCreatePLString("Yes"));
	WMPutInPLDictionary(root, WMCreatePLString("*"), wmd);
	WMReleasePropList(wmd);

	ok = write_file_if_missing(path, root);
	WMReleasePropList(root);
	return ok;
}

/* Minimal WMGLOBAL domain. */
static Bool write_wm_global(const char *path)
{
	WMPropList *root;
	Bool ok;

	root = WMCreatePLDictionary(NULL, NULL);

	WMPutInPLDictionary(root, WMCreatePLString("SystemFont"),
			    WMCreatePLString("Sans"));
	WMPutInPLDictionary(root, WMCreatePLString("BoldSystemFont"),
			    WMCreatePLString("Sans:bold"));
	WMPutInPLDictionary(root, WMCreatePLString("DefaultFontSize"),
			    WMCreatePLString("11"));
	WMPutInPLDictionary(root, WMCreatePLString("AntialiasedText"),
			    WMCreatePLString("Yes"));
	WMPutInPLDictionary(root, WMCreatePLString("FloppyPath"),
			    WMCreatePLString("/media/floppy"));
	WMPutInPLDictionary(root, WMCreatePLString("DoubleClickTime"),
			    WMCreatePLString("250"));
	WMPutInPLDictionary(root, WMCreatePLString("MouseWheelUp"),
			    WMCreatePLString("Button4"));
	WMPutInPLDictionary(root, WMCreatePLString("MouseWheelDown"),
			    WMCreatePLString("Button5"));

	ok = write_file_if_missing(path, root);
	WMReleasePropList(root);
	return ok;
}

/*
 * Minimal WindowMaker defaults domain. Any key not listed here falls back to
 * the defaults compiled into the binary (see defaults.c), so this only needs
 * a small, user-relevant subset to produce a valid, editable starting point.
 */
static Bool write_window_maker(const char *path)
{
	WMPropList *root;
	Bool ok;

	root = WMCreatePLDictionary(NULL, NULL);

	WMPutInPLDictionary(root, WMCreatePLString("ColormapSize"),
			    WMCreatePLString("4"));
	WMPutInPLDictionary(root, WMCreatePLString("IconSize"),
			    WMCreatePLString("64"));
	WMPutInPLDictionary(root, WMCreatePLString("ModifierKey"),
			    WMCreatePLString("Mod1"));
	WMPutInPLDictionary(root, WMCreatePLString("FocusMode"),
			    WMCreatePLString("manual"));
	WMPutInPLDictionary(root, WMCreatePLString("DisableDock"),
			    WMCreatePLString("NO"));
	WMPutInPLDictionary(root, WMCreatePLString("DisableClip"),
			    WMCreatePLString("NO"));
	WMPutInPLDictionary(root, WMCreatePLString("DisableDrawers"),
			    WMCreatePLString("NO"));
	WMPutInPLDictionary(root, WMCreatePLString("WorkspaceBack"),
			    WMCreatePropListFromDescription("(solid, \"rgb:50/50/75\")"));
	WMPutInPLDictionary(root, WMCreatePLString("WindowTitleFont"),
			    WMCreatePLString("Sans:bold:pixelsize=12"));
	WMPutInPLDictionary(root, WMCreatePLString("MenuTitleFont"),
			    WMCreatePLString("Sans:bold:pixelsize=12"));
	WMPutInPLDictionary(root, WMCreatePLString("MenuTextFont"),
			    WMCreatePLString("Sans:pixelsize=12"));
	WMPutInPLDictionary(root, WMCreatePLString("IconTitleFont"),
			    WMCreatePLString("Sans:pixelsize=9"));
	WMPutInPLDictionary(root, WMCreatePLString("SaveSessionOnExit"),
			    WMCreatePLString("NO"));
	WMPutInPLDictionary(root, WMCreatePLString("RootMenuKey"),
			    WMCreatePLString("F12"));
	WMPutInPLDictionary(root, WMCreatePLString("WindowListKey"),
			    WMCreatePLString("F11"));

	ok = write_file_if_missing(path, root);
	WMReleasePropList(root);
	return ok;
}

/* Create a directory under the user's GNUstep root, freeing the temp path. */
static Bool wmkdir_free(char *path)
{
	Bool ok = wmkdir(path);
	wfree(path);
	return ok;
}

Bool wCreateDefaultConfig(void)
{
	const char *defs, *lib;
	char *p;
	Bool ok = True;

	if (!wusergnusteppath())
		return False;

	defs = wdefaultspathfordomain("");
	lib = wuserdatapath();

	if (!wmkdir_free(wstrdup(wusergnusteppath())))
		return False;
	if (!wmkdir_free(wstrdup(defs)))
		return False;
	if (!wmkdir_free(wstrdup(lib)))
		return False;

	p = wstrconcat(lib, "/Icons");
	ok = wmkdir_free(p) && ok;
	p = wstrconcat(lib, "/WindowMaker");
	ok = wmkdir_free(p) && ok;
	p = wstrconcat(lib, "/WindowMaker/Styles");
	ok = wmkdir_free(p) && ok;
	p = wstrconcat(lib, "/WindowMaker/Themes");
	ok = wmkdir_free(p) && ok;
	p = wstrconcat(lib, "/WindowMaker/Backgrounds");
	ok = wmkdir_free(p) && ok;
	p = wstrconcat(lib, "/WindowMaker/IconSets");
	ok = wmkdir_free(p) && ok;
	p = wstrconcat(lib, "/WindowMaker/Pixmaps");
	ok = wmkdir_free(p) && ok;
	p = wstrconcat(lib, "/WindowMaker/CachedPixmaps");
	ok = wmkdir_free(p) && ok;
	p = wstrconcat(lib, "/WindowMaker/WPrefs");
	ok = wmkdir_free(p) && ok;

	if (!ok)
		return False;

	p = wstrconcat(defs, "WMGLOBAL");
	ok = write_wm_global(p) && ok;
	wfree(p);

	p = wstrconcat(defs, "WMRootMenu");
	ok = write_root_menu(p) && ok;
	wfree(p);

	p = wstrconcat(defs, "WMState");
	ok = write_wm_state(p) && ok;
	wfree(p);

	p = wstrconcat(defs, "WMWindowAttributes");
	ok = write_window_attributes(p) && ok;
	wfree(p);

	p = wstrconcat(defs, "WindowMaker");
	ok = write_window_maker(p) && ok;
	wfree(p);

	return ok;
}
