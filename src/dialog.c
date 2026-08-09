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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <signal.h>
#ifdef __FreeBSD__
#include <sys/signal.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX DEFAULT_PATH_MAX
#endif

#include "WindowMaker.h"
#include "GNUstep.h"
#include "screen.h"
#include "window.h"
#include "wdefaults.h"
#include "dialog.h"
#include "misc.h"
#include "stacking.h"
#include "framewin.h"
#include "window.h"
#include "actions.h"
#include "xinerama.h"

#define CRASHING_WIDTH 295
#define CRASHING_HEIGHT 345

static int pstrcmp(const char **str1, const char **str2);
static int strmatch(const void *str1, const void *str2);
static void ScanFiles(const char *dir, const char *prefix, unsigned acceptmask, unsigned declinemask, WMArray *result);
static void handleHistoryKeyPress(XEvent *event, void *clientData);
static void handleKeyPress(XEvent *event, void *clientData);
static void okButtonCallback(void *self, void *clientData);
static void SaveHistory(WMArray *history, const char *filename);
static void setCrashAction(void *self, void *clientData);
static void toggleSaveSession(WMWidget *w, void *data);
static char *HistoryFileName(const char *name);
static WMArray *GenerateVariants(const char *complete);
static WMArray *LoadHistory(const char *filename, int max);
static WMPixmap *getWindowMakerIconImage(WMScreen *scr);
static WMPoint getCenter(virtual_screen *vscr, int width, int height);

static int alert_panel(WMAlertPanel *panel, virtual_screen *vscr, const char *title);

static WMPoint getCenter(virtual_screen *vscr, int width, int height)
{
	return wGetPointToCenterRectInHead(vscr, wGetHeadForPointerLocation(vscr), width, height);
}

static int alert_panel(WMAlertPanel *panel, virtual_screen *vscr, const char *title)
{
	WScreen *scr = vscr->screen_ptr;
	Window parent;
	WWindow *wwin;
	const int win_width = WMWidgetWidth(panel->win);
	const int win_height = WMWidgetHeight(panel->win);
	int result, wframeflags;
	WMPoint center;

	parent = XCreateSimpleWindow(dpy, scr->root_win, 0, 0, win_width, win_height, 0, 0, 0);
	XReparentWindow(dpy, WMWidgetXID(panel->win), parent, 0, 0);
	center = getCenter(vscr, win_width, win_height);

	wframeflags = WFF_BORDER | WFF_TITLEBAR;

	wwin = wManageInternalWindow(vscr, parent, None, title, center.x, center.y, win_width, win_height, wframeflags);

	wwin->client_leader = WMWidgetXID(panel->win);
	WMMapWidget(panel->win);
	wWindowMap(wwin);
	WMRunModalLoop(WMWidgetScreen(panel->win), WMWidgetView(panel->win));
	result = panel->result;
	WMUnmapWidget(panel->win);
	wUnmanageWindow(wwin, False, False);
	WMDestroyAlertPanel(panel);
	XDestroyWindow(dpy, parent);

	return result;
}

int wMessageDialog(virtual_screen *vscr, const char *title, const char *message, const char *defBtn, const char *altBtn, const char *othBtn)
{
	WMAlertPanel *panel;

	panel = WMCreateScaledAlertPanel(vscr->screen_ptr->wmscreen, NULL, title, message, defBtn, altBtn, othBtn);
	return alert_panel(panel, vscr, title);
}

static void toggleSaveSession(WMWidget *w, void *data)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) data;

	wPreferences.save_session_on_exit = WMGetButtonSelected((WMButton *) w);
}

int wExitDialog(virtual_screen *vscr, const char *title, const char *message, const char *defBtn, const char *altBtn, const char *othBtn)
{
	WMAlertPanel *panel;
	WMButton *saveSessionBtn;
	int pwidth;

	panel = WMCreateScaledAlertPanel(vscr->screen_ptr->wmscreen, NULL, title, message, defBtn, altBtn, othBtn);
	pwidth = WMWidgetWidth(panel->win);

	/* add save session button */
	saveSessionBtn = WMCreateSwitchButton(panel->hbox);
	WMSetButtonAction(saveSessionBtn, toggleSaveSession, NULL);
	WMAddBoxSubview(panel->hbox, WMWidgetView(saveSessionBtn), False, True, pwidth / 2, 0, 0);
	WMSetButtonText(saveSessionBtn, _("Save workspace state"));
	WMSetButtonSelected(saveSessionBtn, wPreferences.save_session_on_exit);
	WMRealizeWidget(saveSessionBtn);
	WMMapWidget(saveSessionBtn);

	/* Alert panel show */
	return alert_panel(panel, vscr, title);
}

static char *HistoryFileName(const char *name)
{
	char *filename = NULL;

	filename = getenv("XDG_STATE_HOME");
	if (filename)
		filename = wstrappend(wexpandpath(filename), "/" PACKAGE_TARNAME "/History");
	else
		filename = wstrconcat(wusergnusteppath(), "/.AppInfo/" PACKAGE_TARNAME "/History");

	if (name && strlen(name)) {
		filename = wstrappend(filename, ".");
		filename = wstrappend(filename, name);
	}

	return filename;
}

static int strmatch(const void *str1, const void *str2)
{
	return !strcmp((const char *)str1, (const char *)str2);
}

static WMArray *LoadHistory(const char *filename, int max)
{
	WMPropList *plhistory, *plitem;
	WMArray *history;
	int i, num;
	char *str;

	history = WMCreateArrayWithDestructor(1, wfree);
	WMAddToArray(history, wstrdup(""));
	plhistory = WMReadPropListFromFile(filename);
	if (!plhistory)
		return history;

	if (plhistory) {
		if (WMIsPLArray(plhistory)) {
			num = WMGetPropListItemCount(plhistory);

			for (i = 0; i < num; ++i) {
				plitem = WMGetFromPLArray(plhistory, i);
				if (WMIsPLString(plitem)) {
					str = WMGetFromPLString(plitem);
					if (WMFindInArray(history, strmatch, str) == WANotFound) {
						/*
						 * The string here is duplicated because it will be freed
						 * automatically when the array is deleted. This is not really
						 * great because it is already an allocated string,
						 * unfortunately we cannot re-use it because it will be freed
						 * when we discard the PL (and we don't want to waste the PL's
						 * memory either)
						 */
						WMAddToArray(history, wstrdup(str));
						if (--max <= 0)
							break;
					}
				}
			}
		}
		WMReleasePropList(plhistory);
	}

	return history;
}

static void SaveHistory(WMArray *history, const char *filename)
{
	int i;
	WMPropList *plhistory;

	plhistory = WMCreatePLArray(NULL);
	for (i = 0; i < WMGetArrayItemCount(history); ++i)
		WMAddToPLArray(plhistory, WMCreatePLString(WMGetFromArray(history, i)));

	WMWritePropListToFile(plhistory, filename);
	WMReleasePropList(plhistory);
}

static int pstrcmp(const char **str1, const char **str2)
{
	return strcmp(*str1, *str2);
}

static void ScanFiles(const char *dir, const char *prefix,
		      unsigned acceptmask, unsigned declinemask, WMArray *result)
{
	int prefixlen;
	DIR *d;
	struct dirent *de;
	struct stat sb;
	char *fullfilename, *suffix;

	prefixlen = strlen(prefix);
	d = opendir(dir);
	if (!d)
		return;

	while ((de = readdir(d)) != NULL) {
		if (strlen(de->d_name) > prefixlen &&
		    !strncmp(prefix, de->d_name, prefixlen) &&
		    strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..")) {
			fullfilename = wstrconcat((char *)dir, "/");
			fullfilename = wstrappend(fullfilename, de->d_name);

			if (stat(fullfilename, &sb) == 0 &&
			    (sb.st_mode & acceptmask) &&
			    !(sb.st_mode & declinemask) &&
			    WMFindInArray(result, (WMMatchDataProc *) strmatch,
					  de->d_name + prefixlen) == WANotFound) {
				suffix = wstrdup(de->d_name + prefixlen);
				if (sb.st_mode & S_IFDIR)
					suffix = wstrappend(suffix, "/");

				WMAddToArray(result, suffix);
			}
			wfree(fullfilename);
		}
	}

	closedir(d);
}

static WMArray *GenerateVariants(const char *complete)
{
	Bool firstWord = True;
	WMArray *variants = NULL;
	char *pos = NULL, *path = NULL, *tmp = NULL, *dir = NULL, *prefix = NULL;

	variants = WMCreateArrayWithDestructor(0, wfree);

	while (*complete == ' ')
		++complete;

	pos = strrchr(complete, ' ');
	if (pos != NULL) {
		complete = pos + 1;
		firstWord = False;
	}

	pos = strrchr(complete, '/');
	if (pos != NULL) {
		tmp = wstrndup((char *)complete, pos - complete + 1);
		if (*tmp == '~' && *(tmp + 1) == '/' && getenv("HOME")) {
			dir = wstrdup(getenv("HOME"));
			dir = wstrappend(dir, tmp + 1);
			wfree(tmp);
		} else {
			dir = tmp;
		}

		prefix = wstrdup(pos + 1);
		ScanFiles(dir, prefix, (unsigned)-1, 0, variants);
		wfree(dir);
		wfree(prefix);
	} else if (*complete == '~') {
		WMAddToArray(variants, wstrdup("/"));
	} else if (firstWord) {
		path = getenv("PATH");
		while (path) {
			pos = strchr(path, ':');
			if (pos) {
				tmp = wstrndup(path, pos - path);
				path = pos + 1;
			} else if (*path != '\0') {
				tmp = wstrdup(path);
				path = NULL;
			} else
				break;
			ScanFiles(tmp, complete, S_IXOTH | S_IXGRP | S_IXUSR, S_IFDIR, variants);
			wfree(tmp);
		}
	}

	WMSortArray(variants, (WMCompareDataProc *) pstrcmp);

	return variants;
}

static void handleHistoryKeyPress(XEvent *event, void *clientData)
{
	char *text;
	unsigned pos;
	WMInputPanelWithHistory *p = (WMInputPanelWithHistory *) clientData;
	KeySym ksym;

	ksym = XLookupKeysym(&event->xkey, 0);
	switch (ksym) {
	case XK_Up:
		if (p->histpos < WMGetArrayItemCount(p->history) - 1) {
			if (p->histpos == 0)
				wfree(WMReplaceInArray(p->history, 0, WMGetTextFieldText(p->panel->text)));
			p->histpos++;
			WMSetTextFieldText(p->panel->text, WMGetFromArray(p->history, p->histpos));
		}
		break;
	case XK_Down:
		if (p->histpos > 0) {
			p->histpos--;
			WMSetTextFieldText(p->panel->text, WMGetFromArray(p->history, p->histpos));
		}
		break;
	case XK_Tab:
		if (!p->variants) {
			text = WMGetTextFieldText(p->panel->text);
			pos = WMGetTextFieldCursorPosition(p->panel->text);
			p->prefix = wstrndup(text, pos);
			p->suffix = wstrdup(text + pos);
			wfree(text);
			p->variants = GenerateVariants(p->prefix);
			p->varpos = 0;
			if (!p->variants) {
				wfree(p->prefix);
				wfree(p->suffix);
				p->prefix = NULL;
				p->suffix = NULL;
			}
		}
		if (p->variants && p->prefix && p->suffix) {
			p->varpos++;
			if (p->varpos > WMGetArrayItemCount(p->variants))
				p->varpos = 0;
			if (p->varpos > 0)
				text = wstrconcat(p->prefix, WMGetFromArray(p->variants, p->varpos - 1));
			else
				text = wstrdup(p->prefix);
			pos = strlen(text);
			text = wstrappend(text, p->suffix);
			WMSetTextFieldText(p->panel->text, text);
			WMSetTextFieldCursorPosition(p->panel->text, pos);
			wfree(text);
		}
		break;
	}

	if (ksym != XK_Tab) {
		if (p->prefix) {
			wfree(p->prefix);
			p->prefix = NULL;
		}
		if (p->suffix) {
			wfree(p->suffix);
			p->suffix = NULL;
		}
		if (p->variants) {
			WMFreeArray(p->variants);
			p->variants = NULL;
		}
	}
}

static char *create_input_panel(virtual_screen *vscr, WMInputPanel *panel)
{
	WScreen *scr = vscr->screen_ptr;
	WWindow *wwin;
	const int win_width = WMWidgetWidth(panel->win);
	const int win_height = WMWidgetHeight(panel->win);
	char *result = NULL;
	Window parent;
	WMPoint center;
	int wframeflags;

	parent = XCreateSimpleWindow(dpy, scr->root_win, 0, 0, win_width, win_height, 0, 0, 0);
	XSelectInput(dpy, parent, KeyPressMask | KeyReleaseMask);
	XReparentWindow(dpy, WMWidgetXID(panel->win), parent, 0, 0);
	center = getCenter(vscr, win_width, win_height);

	wframeflags = WFF_BORDER | WFF_TITLEBAR;

	wwin = wManageInternalWindow(vscr, parent, None, NULL, center.x, center.y, win_width, win_height, wframeflags);
	wwin->client_leader = WMWidgetXID(panel->win);

	WSETUFLAG(wwin, no_closable, 0);
	WSETUFLAG(wwin, no_close_button, 0);

	WMMapWidget(panel->win);
	wWindowMap(wwin);
	WMRunModalLoop(WMWidgetScreen(panel->win), WMWidgetView(panel->win));

	if (panel->result == WAPRDefault)
		result = WMGetTextFieldText(panel->text);

	wUnmanageWindow(wwin, False, False);
	WMDestroyInputPanel(panel);
	XDestroyWindow(dpy, parent);

	return result;
}

int wAdvancedInputDialog(virtual_screen *vscr, const char *title,
			 const char *message, const char *name, char **text)
{
	WScreen *scr = vscr->screen_ptr;
	char *result;
	WMInputPanelWithHistory *p;
	char *filename;

	filename = HistoryFileName(name);
	p = wmalloc(sizeof(WMInputPanelWithHistory));
	p->panel = WMCreateScaledInputPanel(scr->wmscreen, NULL, title, message, *text, _("OK"), _("Cancel"));
	p->history = LoadHistory(filename, wPreferences.history_lines);
	p->histpos = 0;
	p->prefix = NULL;
	p->suffix = NULL;
	p->rest = NULL;
	p->variants = NULL;
	p->varpos = 0;

	WMCreateEventHandler(WMWidgetView(p->panel->text), KeyPressMask, handleHistoryKeyPress, p);

	result = create_input_panel(vscr, p->panel);
	if (result) {
		wfree(WMReplaceInArray(p->history, 0, wstrdup(result)));
		SaveHistory(p->history, filename);
	}

	WMFreeArray(p->history);
	wfree(p);
	wfree(filename);
	if (!result)
		return False;

	if (*text)
		wfree(*text);

	*text = result;

	return True;
}

int wInputDialog(virtual_screen *vscr, const char *title, const char *message, char **text)
{
	WScreen *scr = vscr->screen_ptr;
	WMInputPanel *panel;
	char *result;

	panel = WMCreateScaledInputPanel(scr->wmscreen, NULL, title, message, *text, _("OK"), _("Cancel"));
	result = create_input_panel(vscr, panel);

	if (!result)
		return False;

	if (*text)
		wfree(*text);

	*text = result;

	return True;
}

/*
 ***********************************************************************
 * Crashing Dialog Panel
 ***********************************************************************
 */

static void handleKeyPress(XEvent *event, void *clientData)
{
	CrashPanel *panel = (CrashPanel *) clientData;

	if (event->xkey.keycode == panel->retKey)
		WMPerformButtonClick(panel->okB);
}

static void okButtonCallback(void *self, void *clientData)
{
	CrashPanel *panel = (CrashPanel *) clientData;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) self;

	panel->done = True;
}

static void setCrashAction(void *self, void *clientData)
{
	WMPopUpButton *pop = (WMPopUpButton *) self;
	CrashPanel *panel = (CrashPanel *) clientData;

	panel->action = WMGetPopUpButtonSelectedItem(pop);
}

/* Make this read the logo from a compiled in pixmap -Dan */
static WMPixmap *getWindowMakerIconImage(WMScreen *scr)
{
	WMPixmap *pix = NULL;
	char *path = NULL;

	/* Get the Logo icon, without the default icon */
	path = get_icon_filename("Logo", "WMPanel", NULL, False);
	if (path) {
		RColor gray;

		gray.red = 0xae;
		gray.green = 0xaa;
		gray.blue = 0xae;
		gray.alpha = 0;

		pix = WMCreateBlendedPixmapFromFile(scr, path, &gray);
		wfree(path);
	}

	return pix;
}

int wShowCrashingDialogPanel(int whatSig)
{
	CrashPanel *panel;
	WMScreen *scr;
	WMFont *font;
	WMPixmap *logo;
	int screen_no, scr_width, scr_height, action;
	char buf[256];

	screen_no = DefaultScreen(dpy);
	scr_width = WidthOfScreen(ScreenOfDisplay(dpy, screen_no));
	scr_height = HeightOfScreen(ScreenOfDisplay(dpy, screen_no));
	scr = WMCreateScreen(dpy, screen_no);
	if (!scr) {
		werror(_("cannot open connection for crashing dialog panel. Aborting."));
		return WMAbort;
	}

	panel = wmalloc(sizeof(CrashPanel));

	panel->retKey = XKeysymToKeycode(dpy, XK_Return);
	panel->win = WMCreateWindow(scr, "crashingDialog");
	WMResizeWidget(panel->win, CRASHING_WIDTH, CRASHING_HEIGHT);
	WMMoveWidget(panel->win, (scr_width - CRASHING_WIDTH) / 2, (scr_height - CRASHING_HEIGHT) / 2);

	logo = getWindowMakerIconImage(scr);
	if (logo) {
		panel->iconL = WMCreateLabel(panel->win);
		WMResizeWidget(panel->iconL, 64, 64);
		WMMoveWidget(panel->iconL, 10, 10);
		WMSetLabelImagePosition(panel->iconL, WIPImageOnly);
		WMSetLabelImage(panel->iconL, logo);
	}

	panel->nameL = WMCreateLabel(panel->win);
	WMResizeWidget(panel->nameL, 200, 30);
	WMMoveWidget(panel->nameL, 80, 25);
	WMSetLabelTextAlignment(panel->nameL, WALeft);
	font = WMBoldSystemFontOfSize(scr, 24);
	WMSetLabelFont(panel->nameL, font);
	WMReleaseFont(font);
	WMSetLabelText(panel->nameL, _("Fatal error"));

	panel->sepF = WMCreateFrame(panel->win);
	WMResizeWidget(panel->sepF, CRASHING_WIDTH + 4, 2);
	WMMoveWidget(panel->sepF, -2, 80);

	panel->noteL = WMCreateLabel(panel->win);
	WMResizeWidget(panel->noteL, CRASHING_WIDTH - 20, 40);
	WMMoveWidget(panel->noteL, 10, 90);
	WMSetLabelTextAlignment(panel->noteL, WAJustified);
	snprintf(buf, sizeof(buf), _("Window Maker received signal %i."), whatSig);
	WMSetLabelText(panel->noteL, buf);

	panel->note2L = WMCreateLabel(panel->win);
	WMResizeWidget(panel->note2L, CRASHING_WIDTH - 20, 100);
	WMMoveWidget(panel->note2L, 10, 130);
	WMSetLabelTextAlignment(panel->note2L, WALeft);
	snprintf(buf, sizeof(buf), /* Comment for the PO file: the %s is an email address */
		 _(" This fatal error occurred probably due to a bug."
		   " Please fill the included BUGFORM and report it to %s."),
		 PACKAGE_BUGREPORT);
	WMSetLabelText(panel->note2L, buf);
	WMSetLabelWraps(panel->note2L, True);

	panel->whatF = WMCreateFrame(panel->win);
	WMResizeWidget(panel->whatF, CRASHING_WIDTH - 20, 50);
	WMMoveWidget(panel->whatF, 10, 240);
	WMSetFrameTitle(panel->whatF, _("What do you want to do now?"));

	panel->whatP = WMCreatePopUpButton(panel->whatF);
	WMResizeWidget(panel->whatP, CRASHING_WIDTH - 20 - 70, 20);
	WMMoveWidget(panel->whatP, 35, 20);
	WMSetPopUpButtonPullsDown(panel->whatP, False);
	WMSetPopUpButtonText(panel->whatP, _("Select action"));
	WMAddPopUpButtonItem(panel->whatP, _("Abort and leave a core file"));
	WMAddPopUpButtonItem(panel->whatP, _("Restart Window Maker"));
	WMAddPopUpButtonItem(panel->whatP, _("Start alternate window manager"));
	WMSetPopUpButtonAction(panel->whatP, setCrashAction, panel);
	WMSetPopUpButtonSelectedItem(panel->whatP, WMRestart);
	panel->action = WMRestart;

	WMMapSubwidgets(panel->whatF);

	panel->okB = WMCreateCommandButton(panel->win);
	WMResizeWidget(panel->okB, 80, 26);
	WMMoveWidget(panel->okB, 205, 309);
	WMSetButtonText(panel->okB, _("OK"));
	WMSetButtonImage(panel->okB, WMGetSystemPixmap(scr, WSIReturnArrow));
	WMSetButtonAltImage(panel->okB, WMGetSystemPixmap(scr, WSIHighlightedReturnArrow));
	WMSetButtonImagePosition(panel->okB, WIPRight);
	WMSetButtonAction(panel->okB, okButtonCallback, panel);

	panel->done = 0;

	WMCreateEventHandler(WMWidgetView(panel->win), KeyPressMask, handleKeyPress, panel);

	WMRealizeWidget(panel->win);
	WMMapSubwidgets(panel->win);

	WMMapWidget(panel->win);

	XSetInputFocus(dpy, WMWidgetXID(panel->win), RevertToParent, CurrentTime);

	while (!panel->done) {
		XEvent event;

		WMNextEvent(dpy, &event);
		WMHandleEvent(&event);
	}

	action = panel->action;

	WMUnmapWidget(panel->win);
	WMDestroyWidget(panel->win);
	wfree(panel);

	return action;
}
