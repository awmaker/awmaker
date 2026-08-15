/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
 * and individual contributors; see LICENSE for full attribution.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "awconfig.h"

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
 * valid configuration without requiring an external installer script or
 * pre-existing files.
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
			if (mkdir(dir, 0777) == -1) {
				if (errno != EEXIST || stat(dir, &st) == -1 || !S_ISDIR(st.st_mode)) {
					wfree(dir);
					return False;
				}
			}
			*p = '/';
		}
	}
	if (mkdir(dir, 0777) == -1) {
		if (errno != EEXIST || stat(dir, &st) == -1 || !S_ISDIR(st.st_mode)) {
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
 * Write a plain-text file (e.g. a shell script) only if it does not already
 * exist, making it executable. Returns True on success or when the file was
 * already present.
 */
static Bool write_script_if_missing(const char *path, const char *content)
{
	FILE *fp;
	struct stat st;

	if (stat(path, &st) == 0)
		return True;

	fp = fopen(path, "w");
	if (!fp)
		return False;
	if (fputs(content, fp) == EOF) {
		fclose(fp);
		return False;
	}
	if (fclose(fp) != 0)
		return False;

	return chmod(path, 0755) == 0;
}

/* Minimal, empty autostart script users can edit. */
static Bool write_autostart(const char *path)
{
	return write_script_if_missing(path,
		"#!/bin/sh\n"
		"#\n"
		"# Place applications to be executed when WindowMaker is started here.\n"
		"# This should only be used for non-X applications or applications that\n"
		"# do not support session management. Other applications should be\n"
		"# restarted by the WindowMaker session restoring mechanism.\n"
		"#\n"
		"# WindowMaker will wait until this script finishes, so put a ``&'' at\n"
		"# the end of command lines that could block.\n"
		"#\n");
}

/* Minimal, empty exit script users can edit. */
static Bool write_exitscript(const char *path)
{
	return write_script_if_missing(path,
		"#!/bin/sh\n"
		"#\n"
		"# Place commands to be executed when WindowMaker is exited here.\n"
		"#\n"
		"# WindowMaker will wait until this script finishes, so put a ``&'' at\n"
		"# the end of command lines that could block.\n"
		"#\n");
}

/*
 * Helpers to build the root menu plist. They mirror the reference text menus
 * (WindowMaker/menu + appearance.menu + background.menu) translated into the
 * WMRootMenu plist format that awmaker reads (rootmenu.c configureMenu).
 */
static WMPropList *mk_entry(const char *title, const char *cmd, const char *params)
{
	WMPropList *e;

	if (params)
		e = WMCreatePLArray(WMCreatePLString(title),
				    WMCreatePLString(cmd),
				    WMCreatePLString(params), NULL);
	else
		e = WMCreatePLArray(WMCreatePLString(title),
				    WMCreatePLString(cmd), NULL);
	return e;
}

static WMPropList *mk_menu(const char *title)
{
	return WMCreatePLArray(WMCreatePLString(title), NULL);
}

/* Append `item' to `menu' then release the caller's reference. */
static void menu_add(WMPropList *menu, WMPropList *item)
{
	WMAddToPLArray(menu, item);
	WMReleasePropList(item);
}

/*
 * WMRootMenu (plist). Replicates the reference application/background theme
 * menus: an Info panel, launchers, workspaces, applications, utils, selection,
 * commands, an Appearance section (background solids/gradients/images plus
 * styles/themes/icon-sets via OPEN_MENU of the user data dirs) and a session
 * section.
 */
static Bool write_root_menu(const char *path)
{
	WMPropList *menu, *sub;
	const char *usr = wusergnusteppath();
	Bool ok;

	menu = mk_menu("Window Maker");

	/* Info */
	sub = mk_menu("Info");
	menu_add(sub, mk_entry("Info Panel", "INFO_PANEL", NULL));
	menu_add(sub, mk_entry("Legal", "LEGAL_PANEL", NULL));
	menu_add(sub, mk_entry("Key Bindings", "KEYBINDS_PANEL", NULL));
	menu_add(sub, mk_entry("System Console", "EXEC", "xconsole"));
	menu_add(sub, mk_entry("System Load", "SHEXEC", "xosview || xload"));
	menu_add(sub, mk_entry("Process List", "EXEC", "xterm -e top"));
	menu_add(sub, mk_entry("Manual Browser", "EXEC", "xman"));
	menu_add(menu, sub);

	menu_add(menu, mk_entry("Run...", "SHEXEC", "%a(Run,Type command to run:)"));
	menu_add(menu, mk_entry("XTerm", "EXEC", "xterm -sb"));
	menu_add(menu, mk_entry("Mozilla Firefox", "EXEC", "firefox"));
	menu_add(menu, mk_entry("Workspaces", "WORKSPACE_MENU", NULL));

	/* Applications */
	sub = mk_menu("Applications");
	menu_add(sub, mk_entry("Gimp", "SHEXEC", "gimp >/dev/null"));
	menu_add(sub, mk_entry("Ghostview", "EXEC", "ghostview %a(GhostView,Enter file to view)"));
	menu_add(sub, mk_entry("Xpdf", "EXEC", "xpdf %a(Xpdf,Enter PDF to view)"));
	menu_add(sub, mk_entry("Abiword", "EXEC", "abiword"));
	menu_add(sub, mk_entry("Dia", "EXEC", "dia"));

	{
		WMPropList *oo = mk_menu("OpenOffice.org");
		menu_add(oo, mk_entry("OpenOffice.org", "EXEC", "ooffice"));
		menu_add(oo, mk_entry("Writer", "EXEC", "oowriter"));
		menu_add(oo, mk_entry("Spreadsheet", "EXEC", "oocalc"));
		menu_add(oo, mk_entry("Draw", "EXEC", "oodraw"));
		menu_add(oo, mk_entry("Impress", "EXEC", "ooimpress"));
		menu_add(sub, oo);
	}

	{
		WMPropList *ed = mk_menu("Editors");
		menu_add(ed, mk_entry("XEmacs", "EXEC", "xemacs"));
		menu_add(ed, mk_entry("Emacs", "EXEC", "emacs"));
		menu_add(ed, mk_entry("XJed", "EXEC", "xjed"));
		menu_add(ed, mk_entry("VI", "EXEC", "xterm -e vi"));
		menu_add(ed, mk_entry("GVIM", "EXEC", "gvim"));
		menu_add(ed, mk_entry("NEdit", "EXEC", "nedit"));
		menu_add(ed, mk_entry("Xedit", "EXEC", "xedit"));
		menu_add(sub, ed);
	}

	{
		WMPropList *mm = mk_menu("Multimedia");
		WMPropList *xm = mk_menu("XMMS");
		menu_add(xm, mk_entry("XMMS", "EXEC", "xmms"));
		menu_add(xm, mk_entry("XMMS play/pause", "EXEC", "xmms -t"));
		menu_add(xm, mk_entry("XMMS stop", "EXEC", "xmms -s"));
		menu_add(mm, xm);
		menu_add(mm, mk_entry("Xine video player", "EXEC", "xine"));
		menu_add(mm, mk_entry("MPlayer", "EXEC", "mplayer"));
		menu_add(sub, mm);
	}
	menu_add(menu, sub);

	/* Utils */
	sub = mk_menu("Utils");
	menu_add(sub, mk_entry("Calculator", "EXEC", "xcalc"));
	menu_add(sub, mk_entry("Window Properties", "SHEXEC",
			       "xprop | xmessage -center -title xprop -file -"));
	menu_add(sub, mk_entry("Font Chooser", "EXEC", "xfontsel"));
	menu_add(sub, mk_entry("Magnify", "EXEC", "wmagnify"));
	menu_add(sub, mk_entry("Colormap", "EXEC", "xcmap"));
	menu_add(sub, mk_entry("Kill X Application", "EXEC", "xkill"));
	menu_add(menu, sub);

	/* Selection */
	sub = mk_menu("Selection");
	menu_add(sub, mk_entry("Copy", "SHEXEC", "echo %s | wxcopy"));
	menu_add(sub, mk_entry("Mail To", "EXEC", "xterm -name mail -T Pine -e pine %s"));
	menu_add(sub, mk_entry("Navigate", "EXEC", "netscape %s"));
	menu_add(sub, mk_entry("Search in Manual", "SHEXEC",
			       "man %s | xless || echo \"no manual page for %s\""));
	menu_add(menu, sub);

	/* Commands */
	sub = mk_menu("Commands");
	menu_add(sub, mk_entry("Hide Others", "HIDE_OTHERS", NULL));
	menu_add(sub, mk_entry("Show All", "SHOW_ALL", NULL));
	menu_add(sub, mk_entry("Arrange Icons", "ARRANGE_ICONS", NULL));
	menu_add(sub, mk_entry("Refresh", "REFRESH", NULL));
	menu_add(sub, mk_entry("Lock", "EXEC", "xlock -allowroot -usefirst"));
	menu_add(menu, sub);

	/* Appearance */
	sub = mk_menu("Appearance");

	{
		WMPropList *bg = mk_menu("Background");

		WMPropList *solid = mk_menu("Solid");
		menu_add(solid, mk_entry("Black", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, black)'"));
		menu_add(solid, mk_entry("Blue", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, \"#505075\")'"));
		menu_add(solid, mk_entry("Indigo", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, \"#243e6c\")'"));
		menu_add(solid, mk_entry("Bluemarine", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, \"#224477\")'"));
		menu_add(solid, mk_entry("Deep Blue", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, \"#180090\")'"));
		menu_add(solid, mk_entry("Purple", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, \"#554466\")'"));
		menu_add(solid, mk_entry("Wheat", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, wheat4)'"));
		menu_add(solid, mk_entry("Dark Gray", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, \"#333340\")'"));
		menu_add(solid, mk_entry("Wine", "EXEC", "wdwrite WindowMaker WorkspaceBack '(solid, \"#400020\")'"));
		menu_add(bg, solid);

		WMPropList *grad = mk_menu("Gradient");
		menu_add(grad, mk_entry("Sunset", "EXEC", "wdwrite WindowMaker WorkspaceBack '(mvgradient, deepskyblue4, black, deepskyblue4, tomato4)'"));
		menu_add(grad, mk_entry("Sky", "EXEC", "wdwrite WindowMaker WorkspaceBack '(vgradient, blue4, white)'"));
		menu_add(grad, mk_entry("Blue Shades", "EXEC", "wdwrite WindowMaker WorkspaceBack '(vgradient, \"#7080a5\", \"#101020\")'"));
		menu_add(grad, mk_entry("Indigo Shades", "EXEC", "wdwrite WindowMaker WorkspaceBack '(vgradient, \"#746ebc\", \"#242e4c\")'"));
		menu_add(grad, mk_entry("Purple Shades", "EXEC", "wdwrite WindowMaker WorkspaceBack '(vgradient, \"#654c66\", \"#151426\")'"));
		menu_add(grad, mk_entry("Wheat Shades", "EXEC", "wdwrite WindowMaker WorkspaceBack '(vgradient, \"#a09060\", \"#302010\")'"));
		menu_add(grad, mk_entry("Grey Shades", "EXEC", "wdwrite WindowMaker WorkspaceBack '(vgradient, \"#636380\", \"#131318\")'"));
		menu_add(grad, mk_entry("Wine Shades", "EXEC", "wdwrite WindowMaker WorkspaceBack '(vgradient, \"#600040\", \"#180010\")'"));
		menu_add(bg, grad);

		/* OPEN_MENU of the user background dir with a command per scaling mode. */
		WMPropList *imgs = mk_menu("Images");
		char bg_buf[1024];
		snprintf(bg_buf, sizeof(bg_buf), "-noext %s/Library/WindowMaker/Backgrounds WITH wmsetbg -u -t", usr);
		menu_add(imgs, mk_entry("Tiled", "OPEN_MENU", bg_buf));
		snprintf(bg_buf, sizeof(bg_buf), "-noext %s/Library/WindowMaker/Backgrounds WITH wmsetbg -u -s", usr);
		menu_add(imgs, mk_entry("Scaled", "OPEN_MENU", bg_buf));
		snprintf(bg_buf, sizeof(bg_buf), "-noext %s/Library/WindowMaker/Backgrounds WITH wmsetbg -u -e", usr);
		menu_add(imgs, mk_entry("Centered", "OPEN_MENU", bg_buf));
		snprintf(bg_buf, sizeof(bg_buf), "-noext %s/Library/WindowMaker/Backgrounds WITH wmsetbg -u -a", usr);
		menu_add(imgs, mk_entry("Maximized", "OPEN_MENU", bg_buf));
		snprintf(bg_buf, sizeof(bg_buf), "-noext %s/Library/WindowMaker/Backgrounds WITH wmsetbg -u -f", usr);
		menu_add(imgs, mk_entry("Filled", "OPEN_MENU", bg_buf));
		menu_add(bg, imgs);

		menu_add(sub, bg);
	}

	/* themes + icon sets via OPEN_MENU of the user data dirs */
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), "-noext %s/Library/WindowMaker/Styles WITH setstyle", usr);
		menu_add(sub, mk_entry("Styles", "OPEN_MENU", buf));
		snprintf(buf, sizeof(buf), "-noext %s/Library/WindowMaker/Themes WITH setstyle", usr);
		menu_add(sub, mk_entry("Themes", "OPEN_MENU", buf));
		snprintf(buf, sizeof(buf), "-noext %s/Library/WindowMaker/IconSets WITH seticons", usr);
		menu_add(sub, mk_entry("Icon Sets", "OPEN_MENU", buf));
	}

	{
		char buf[1024];
		snprintf(buf, sizeof(buf), "geticonset %s/Library/WindowMaker/IconSets/%%a(IconSet name)", usr);
		menu_add(sub, mk_entry("Save IconSet", "EXEC", buf));
		snprintf(buf, sizeof(buf), "getstyle -p %s/Library/WindowMaker/Themes/%%a(Theme name)", usr);
		menu_add(sub, mk_entry("Save Theme", "EXEC", buf));
	}

	menu_add(sub, mk_entry("Preferences Utility", "EXEC", "WPrefs"));
	menu_add(menu, sub);

	/* Session */
	sub = mk_menu("Session");
	menu_add(sub, mk_entry("Save Session", "SAVE_SESSION", NULL));
	menu_add(sub, mk_entry("Clear Session", "CLEAR_SESSION", NULL));
	menu_add(sub, mk_entry("Restart Window Maker", "RESTART", NULL));
	menu_add(sub, mk_entry("Start BlackBox", "RESTART", "blackbox"));
	menu_add(sub, mk_entry("Start IceWM", "RESTART", "icewm"));
	menu_add(sub, mk_entry("Exit", "EXIT", NULL));
	menu_add(menu, sub);

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
	WMPutInPLDictionary(root, WMCreatePLString("KeychainTimeoutDelay"),
			    WMCreatePLString("1000"));
	WMPutInPLDictionary(root, WMCreatePLString("KeychainCancelKey"),
			    WMCreatePLString("Escape"));

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
	char *defs;
	const char *lib;
	char *p;
	Bool ok = True;

	if (!wusergnusteppath())
		return False;

	defs = wdefaultspathfordomain("");
	lib = wuserdatapath();

	if (!wmkdir_free(wstrdup(wusergnusteppath()))) {
		wfree(defs);
		return False;
	}
	if (!wmkdir_free(wstrdup(defs))) {
		wfree(defs);
		return False;
	}
	if (!wmkdir_free(wstrdup(lib))) {
		wfree(defs);
		return False;
	}

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

	if (!ok) {
		wfree(defs);
		return False;
	}

	p = wstrconcat(lib, "/WindowMaker/autostart");
	ok = write_autostart(p) && ok;
	wfree(p);

	p = wstrconcat(lib, "/WindowMaker/exitscript");
	ok = write_exitscript(p) && ok;
	wfree(p);

	if (!ok) {
		wfree(defs);
		return False;
	}

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

	wfree(defs);
	return ok;
}
