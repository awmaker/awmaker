/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef WMAPPICON_H_
#define WMAPPICON_H_

#include <wraster.h>

#include "window.h"
#include "icon.h"
#include "application.h"

typedef struct WAppIcon {
	short xindex;
	short yindex;
	struct WAppIcon *next;
	struct WAppIcon *prev;
	WIcon *icon;
	int x_pos, y_pos;		/* absolute screen coordinate */
	char *command;			/* command used to launch app */
#ifdef USE_DOCK_XDND
	char *dnd_command;		/* command to use when something is */
					/* dropped on us */
#endif
	char *paste_command;		/* command to run when
					 * something is pasted */
	char *wm_class;
	char *wm_instance;
	pid_t pid;			 /* for apps launched from the dock */
	Window main_window;
	struct WDock *dock;		 /* In which dock is docked. */
	struct AppSettingsPanel *panel;  /* Settings Panel */
	unsigned int docked:1;
	unsigned int omnipresent:1;	 /* If omnipresent when
					  * docked in clip */
	unsigned int attracted:1;	 /* If it was attracted by the clip */
	unsigned int launching:1;
	unsigned int running:1;		 /* application is already running */
	unsigned int relaunching:1;	 /* launching 2nd instance */
	unsigned int forced_dock:1;
	unsigned int auto_launch:1;	 /* launch app on startup */
	unsigned int updated:1;
	unsigned int editing:1;		 /* editing docked icon */
	unsigned int drop_launch:1;	 /* launching from drop action */
	unsigned int paste_launch:1;	 /* launching from paste action */
	unsigned int destroyed:1;	 /* appicon was destroyed */
	unsigned int buggy_app:1;	 /* do not make dock rely on hints
					  * set by app */
	unsigned int lock:1;		 /* do not allow to be destroyed */
} WAppIcon;

Bool wHandleAppIconMove(WAppIcon *aicon, XEvent *event);

void wAppIconDestroy(WAppIcon *aicon);
void wAppIconPaint(WAppIcon *aicon);
void wAppIconMove(WAppIcon *aicon, int x, int y);
void create_appicon_for_application(WApplication *wapp, WWindow *wwin);
void removeAppIconFor(WApplication *wapp);
void save_appicon(WAppIcon *aicon);
void paint_app_icon(WApplication *wapp);
void unpaint_app_icon(WApplication *wapp);
void wApplicationExtractDirPackIcon(const char *path, const char *wm_instance,
				    const char *wm_class);
WAppIcon *dock_icon_create(virtual_screen *vscr, char *command, char *wm_class, char *wm_instance);
WAppIcon *create_appicon(virtual_screen *vscr, char *command, char *wm_class, char *wm_instance);

void appicon_map(WAppIcon *aicon);
void appicon_unmap(WAppIcon *aicon);

void appIconMouseDown(WObjDescriptor *desc, XEvent *event);
void add_to_appicon_list(WAppIcon *appicon);

void move_appicon_to_dock(virtual_screen *vscr, WAppIcon *icon, char *wm_class, char *wm_instance);

#endif
