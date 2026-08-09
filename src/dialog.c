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

#define ICONDLG_WIDTH 450
#define ICONDLG_HEIGHT 280

#define CRASHING_WIDTH 295
#define CRASHING_HEIGHT 345

static int pstrcmp(const char **str1, const char **str2);
static int strmatch(const void *str1, const void *str2);
static void ScanFiles(const char *dir, const char *prefix, unsigned acceptmask, unsigned declinemask, WMArray *result);
static void buttonCallback(void *self, void *clientData);
static void drawIconProc(WMList *lPtr, int index, Drawable d, char *text, int state, WMRect *rect);
static void handleHistoryKeyPress(XEvent *event, void *clientData);
static void handleKeyPress(XEvent *event, void *clientData);
static void keyPressHandler(XEvent *event, void *data);
static void listCallback(void *self, void *data);
static void listIconPaths(WMList *lPtr);
static void listPixmaps(virtual_screen *vscr, WMList *lPtr, const char *path);
static void okButtonCallback(void *self, void *clientData);
static void SaveHistory(WMArray *history, const char *filename);
static void setCrashAction(void *self, void *clientData);
static void setViewedImage(IconPanel *panel, const char *file);
static void toggleSaveSession(WMWidget *w, void *data);
static void create_dialog_iconchooser_widgets(IconPanel *panel, const int win_width, const int win_height, int wmScaleWidth, int wmScaleHeight);
static void destroy_dialog_iconchooser(IconPanel *panel, Window parent);
static char *HistoryFileName(const char *name);
static char *create_dialog_iconchooser_title(const char *instance, const char *class);
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
 *****************************************************************
 * Icon Selection Panel
 *****************************************************************
 */

static void listPixmaps(virtual_screen *vscr, WMList *lPtr, const char *path)
{
	struct dirent *dentry;
	DIR *dir;
	char pbuf[PATH_MAX + 16];
	char *apath;
	IconPanel *panel = WMGetHangedData(lPtr);

	panel->preview = False;
	apath = wexpandpath(path);
	dir = opendir(apath);
	if (!dir) {
		wfree(apath);
		snprintf(pbuf, sizeof(pbuf),
			 _("Could not open directory \"%s\":\n%s"),
			 path, strerror(errno));
		wMessageDialog(vscr, _("Error"), pbuf, _("OK"), NULL, NULL);
		return;
	}

	/* list contents in the column */
	while ((dentry = readdir(dir))) {
		struct stat statb;

		if (strcmp(dentry->d_name, ".") == 0 || strcmp(dentry->d_name, "..") == 0)
			continue;

		if (wstrlcpy(pbuf, apath, sizeof(pbuf)) >= sizeof(pbuf) ||
		    wstrlcat(pbuf, "/", sizeof(pbuf)) >= sizeof(pbuf) ||
		    wstrlcat(pbuf, dentry->d_name, sizeof(pbuf)) >= sizeof(pbuf)) {
			wwarning(_("full path for file \"%s\" in \"%s\" is longer than %d bytes, skipped"),
				 dentry->d_name, path, (int) sizeof(pbuf) - 1);
			continue;
		}

		if (stat(pbuf, &statb) < 0)
			continue;

		if (statb.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)
		    && statb.st_mode & (S_IFREG | S_IFLNK)) {
			WMAddListItem(lPtr, dentry->d_name);
		}
	}

	WMSortListItems(lPtr);
	closedir(dir);
	wfree(apath);
	panel->preview = True;
}

static void setViewedImage(IconPanel *panel, const char *file)
{
	WMPixmap *pixmap;
	RColor color;
	int iwidth, iheight;

	color.red = 0xae;
	color.green = 0xaa;
	color.blue = 0xae;
	color.alpha = 0;
	iwidth = WMWidgetWidth(panel->iconView);
	iheight = WMWidgetHeight(panel->iconView);
	pixmap = WMCreateScaledBlendedPixmapFromFile(WMWidgetScreen(panel->win), file, &color, iwidth, iheight);
	if (!pixmap) {
		WMSetButtonEnabled(panel->okButton, False);
		WMSetLabelText(panel->iconView, _("Could not load image file "));
		WMSetLabelImage(panel->iconView, NULL);
	} else {
		WMSetButtonEnabled(panel->okButton, True);
		WMSetLabelText(panel->iconView, NULL);
		WMSetLabelImage(panel->iconView, pixmap);
		WMReleasePixmap(pixmap);
	}
}

static void listCallback(void *self, void *data)
{
	WMList *lPtr = (WMList *) self;
	IconPanel *panel = (IconPanel *) data;
	char *path;

	if (lPtr == panel->dirList) {
		WMListItem *item = WMGetListSelectedItem(lPtr);
		if (item == NULL)
			return;

		path = item->text;
		WMSetTextFieldText(panel->fileField, path);
		WMSetLabelImage(panel->iconView, NULL);
		WMSetButtonEnabled(panel->okButton, False);
		WMClearList(panel->iconList);
		listPixmaps(panel->vscr, panel->iconList, path);
	} else {
		char *tmp, *iconFile;
		WMListItem *item = WMGetListSelectedItem(panel->dirList);
		if (item == NULL)
			return;

		path = item->text;

		item = WMGetListSelectedItem(panel->iconList);
		if (item == NULL)
			return;

		iconFile = item->text;
		tmp = wexpandpath(path);
		path = wmalloc(strlen(tmp) + strlen(iconFile) + 4);
		strcpy(path, tmp);
		strcat(path, "/");
		strcat(path, iconFile);
		wfree(tmp);
		WMSetTextFieldText(panel->fileField, path);
		setViewedImage(panel, path);
		wfree(path);
	}
}

static void listIconPaths(WMList *lPtr)
{
	char *paths, *path;

	paths = wstrdup(wPreferences.icon_path);
	path = strtok(paths, ":");
	do {
		char *tmp;

		tmp = wexpandpath(path);
		/* do not sort, because the order implies the order of
		 * directories searched */
		if (access(tmp, X_OK) == 0)
			WMAddListItem(lPtr, path);

		wfree(tmp);
	} while ((path = strtok(NULL, ":")) != NULL);

	wfree(paths);
}

static void drawIconProc(WMList *lPtr, int index, Drawable d, char *text, int state, WMRect *rect)
{
	IconPanel *panel = WMGetHangedData(lPtr);
	WScreen *scr = panel->vscr->screen_ptr;
	GC gc = scr->draw_gc;
	GC copygc = scr->copy_gc;
	char *file, *dirfile;
	WMPixmap *pixmap;
	WMColor *back;
	WMSize size;
	WMScreen *wmscr = WMWidgetScreen(panel->win);
	RColor color;
	int x, y, width, height, len;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) index;

	if (!panel->preview)
		return;

	x = rect->pos.x;
	y = rect->pos.y;
	width = rect->size.width;
	height = rect->size.height;
	back = (state & WLDSSelected) ? scr->white : scr->gray;
	dirfile = wexpandpath(WMGetListSelectedItem(panel->dirList)->text);
	len = strlen(dirfile) + strlen(text) + 4;
	file = wmalloc(len);
	snprintf(file, len, "%s/%s", dirfile, text);
	wfree(dirfile);

	color.red = WMRedComponentOfColor(back) >> 8;
	color.green = WMGreenComponentOfColor(back) >> 8;
	color.blue = WMBlueComponentOfColor(back) >> 8;
	color.alpha = WMGetColorAlpha(back) >> 8;

	pixmap = WMCreateScaledBlendedPixmapFromFile(wmscr, file, &color, width - 2, height - 2);
	wfree(file);

	if (!pixmap)
		return;

	XFillRectangle(dpy, d, WMColorGC(back), x, y, width, height);
	XSetClipMask(dpy, gc, None);
	XDrawLine(dpy, d, WMColorGC(scr->white), x, y + height - 1, x + width, y + height - 1);
	size = WMGetPixmapSize(pixmap);
	XSetClipMask(dpy, copygc, WMGetPixmapMaskXID(pixmap));
	XSetClipOrigin(dpy, copygc, x + (width - size.width) / 2, y + 2);
	XCopyArea(dpy, WMGetPixmapXID(pixmap), d, copygc, 0, 0,
		  size.width > 100 ? 100 : size.width, size.height > 64 ? 64 : size.height,
		  x + (width - size.width) / 2, y + 2);

	{
		int i, j;
		int fheight = WMFontHeight(panel->normalfont);
		int tlen = strlen(text);
		int twidth = WMWidthOfString(panel->normalfont, text, tlen);
		int ofx, ofy;

		ofx = x + (width - twidth) / 2;
		ofy = y + 64 - fheight;

		for (i = -1; i < 2; i++)
			for (j = -1; j < 2; j++)
				WMDrawString(wmscr, d, scr->white, panel->normalfont,
					     ofx + i, ofy + j, text, tlen);

		WMDrawString(wmscr, d, scr->black, panel->normalfont, ofx, ofy, text, tlen);
	}

	WMReleasePixmap(pixmap);
	/* I hope it is better to do not use cache / on my box it is fast nuff */
	XFlush(dpy);
}

static void buttonCallback(void *self, void *clientData)
{
	WMButton *bPtr = (WMButton *) self;
	IconPanel *panel = (IconPanel *) clientData;

	if (bPtr == panel->okButton) {
		panel->done = True;
		panel->result = True;
	} else if (bPtr == panel->cancelButton) {
		panel->done = True;
		panel->result = False;
	} else if (bPtr == panel->previewButton) {
	/**** Previewer ****/
		WMSetButtonEnabled(bPtr, False);
		WMSetListUserDrawItemHeight(panel->iconList, 68);
		WMSetListUserDrawProc(panel->iconList, drawIconProc);
		WMRedisplayWidget(panel->iconList);
		/* for draw proc to access screen/gc */
	/*** end preview ***/
	}
}

static void keyPressHandler(XEvent *event, void *data)
{
	IconPanel *panel = (IconPanel *) data;
	char buffer[32];
	KeySym ksym;
	int iidx;
	int didx;
	int item = 0;
	WMList *list = NULL;

	if (event->type == KeyRelease)
		return;

	buffer[0] = 0;
	XLookupString(&event->xkey, buffer, sizeof(buffer), &ksym, NULL);
	iidx = WMGetListSelectedItemRow(panel->iconList);
	didx = WMGetListSelectedItemRow(panel->dirList);

	switch (ksym) {
	case XK_Up:
		if (iidx > 0)
			item = iidx - 1;
		else
			item = iidx;
		list = panel->iconList;
		break;
	case XK_Down:
		if (iidx < WMGetListNumberOfRows(panel->iconList) - 1)
			item = iidx + 1;
		else
			item = iidx;
		list = panel->iconList;
		break;
	case XK_Home:
		item = 0;
		list = panel->iconList;
		break;
	case XK_End:
		item = WMGetListNumberOfRows(panel->iconList) - 1;
		list = panel->iconList;
		break;
	case XK_Next:
		if (didx < WMGetListNumberOfRows(panel->dirList) - 1)
			item = didx + 1;
		else
			item = didx;
		list = panel->dirList;
		break;
	case XK_Prior:
		if (didx > 0)
			item = didx - 1;
		else
			item = 0;
		list = panel->dirList;
		break;
	case XK_Return:
		WMPerformButtonClick(panel->okButton);
		break;
	case XK_Escape:
		WMPerformButtonClick(panel->cancelButton);
		break;
	}

	if (list) {
		WMSelectListItem(list, item);
		WMSetListPosition(list, item - 5);
		listCallback(list, panel);
	}
}

static void create_dialog_iconchooser_widgets(IconPanel *panel, const int win_width, const int win_height, int wmScaleWidth, int wmScaleHeight)
{
	WScreen *scr = panel->vscr->screen_ptr;
	WMFont *boldFont;
	WMColor *color;

	panel->win = WMCreateWindow(scr->wmscreen, "iconChooser");
	WMResizeWidget(panel->win, win_width, win_height);

	WMCreateEventHandler(WMWidgetView(panel->win), KeyPressMask | KeyReleaseMask, keyPressHandler, panel);

	boldFont = WMBoldSystemFontOfSize(scr->wmscreen, WMScaleY(12));
	panel->normalfont = WMSystemFontOfSize(WMWidgetScreen(panel->win), WMScaleY(12));

	panel->dirLabel = WMCreateLabel(panel->win);
	WMResizeWidget(panel->dirLabel, WMScaleX(200), WMScaleY(20));
	WMMoveWidget(panel->dirLabel, WMScaleX(10), WMScaleY(7));
	WMSetLabelText(panel->dirLabel, _("Directories"));
	WMSetLabelFont(panel->dirLabel, boldFont);
	WMSetLabelTextAlignment(panel->dirLabel, WACenter);

	WMSetLabelRelief(panel->dirLabel, WRSunken);

	panel->iconLabel = WMCreateLabel(panel->win);
	WMResizeWidget(panel->iconLabel, WMScaleX(140), WMScaleY(20));
	WMMoveWidget(panel->iconLabel, WMScaleX(215), WMScaleY(7));
	WMSetLabelText(panel->iconLabel, _("Icons"));
	WMSetLabelFont(panel->iconLabel, boldFont);
	WMSetLabelTextAlignment(panel->iconLabel, WACenter);

	WMReleaseFont(boldFont);

	color = WMWhiteColor(scr->wmscreen);
	WMSetLabelTextColor(panel->dirLabel, color);
	WMSetLabelTextColor(panel->iconLabel, color);
	WMReleaseColor(color);

	color = WMDarkGrayColor(scr->wmscreen);
	WMSetWidgetBackgroundColor(panel->iconLabel, color);
	WMSetWidgetBackgroundColor(panel->dirLabel, color);
	WMReleaseColor(color);

	WMSetLabelRelief(panel->iconLabel, WRSunken);

	panel->dirList = WMCreateList(panel->win);
	WMResizeWidget(panel->dirList, WMScaleX(200), WMScaleY(170));
	WMMoveWidget(panel->dirList, WMScaleX(10), WMScaleY(30));
	WMSetListAction(panel->dirList, listCallback, panel);

	panel->iconList = WMCreateList(panel->win);
	WMResizeWidget(panel->iconList, WMScaleX(140), WMScaleY(170));
	WMMoveWidget(panel->iconList, WMScaleX(215), WMScaleY(30));
	WMSetListAction(panel->iconList, listCallback, panel);

	WMHangData(panel->iconList, panel);

	panel->previewButton = WMCreateCommandButton(panel->win);
	WMResizeWidget(panel->previewButton, WMScaleX(75), WMScaleY(26));
	WMMoveWidget(panel->previewButton, WMScaleX(365), WMScaleY(130));
	WMSetButtonText(panel->previewButton, _("Preview"));
	WMSetButtonAction(panel->previewButton, buttonCallback, panel);

	panel->iconView = WMCreateLabel(panel->win);
	WMResizeWidget(panel->iconView, WMScaleX(75), WMScaleY(75));
	WMMoveWidget(panel->iconView, WMScaleX(365), WMScaleY(40));
	WMSetLabelImagePosition(panel->iconView, WIPOverlaps);
	WMSetLabelRelief(panel->iconView, WRSunken);
	WMSetLabelTextAlignment(panel->iconView, WACenter);

	panel->fileLabel = WMCreateLabel(panel->win);
	WMResizeWidget(panel->fileLabel, WMScaleX(80), WMScaleY(20));
	WMMoveWidget(panel->fileLabel, WMScaleX(10), WMScaleY(210));
	WMSetLabelText(panel->fileLabel, _("File Name:"));

	panel->fileField = WMCreateTextField(panel->win);
	WMSetViewNextResponder(WMWidgetView(panel->fileField), WMWidgetView(panel->win));
	WMResizeWidget(panel->fileField, WMScaleX(345), WMScaleY(20));
	WMMoveWidget(panel->fileField, WMScaleX(95), WMScaleY(210));
	WMSetTextFieldEditable(panel->fileField, False);

	panel->okButton = WMCreateCommandButton(panel->win);
	WMResizeWidget(panel->okButton, WMScaleX(80), WMScaleY(26));
	WMMoveWidget(panel->okButton, WMScaleX(360), WMScaleY(242));
	WMSetButtonText(panel->okButton, _("OK"));
	WMSetButtonEnabled(panel->okButton, False);
	WMSetButtonAction(panel->okButton, buttonCallback, panel);

	panel->cancelButton = WMCreateCommandButton(panel->win);
	WMResizeWidget(panel->cancelButton, WMScaleX(80), WMScaleY(26));
	WMMoveWidget(panel->cancelButton, WMScaleX(270), WMScaleY(242));
	WMSetButtonText(panel->cancelButton, _("Cancel"));
	WMSetButtonAction(panel->cancelButton, buttonCallback, panel);

	WMRealizeWidget(panel->win);
	WMMapSubwidgets(panel->win);
}

static char *create_dialog_iconchooser_title(const char *instance, const char *class)
{
	static const char *prefix = NULL;
	char *title;
	int len;

	prefix = _("Icon Chooser");
	len = strlen(prefix)
		+ 2					/* " ["            */
		+ (instance ? strlen(instance) : 1)	/* instance or "?" */
		+ 1					/* "."             */
		+ (class ? strlen(class) : 1)		/* class or "?"    */
		+ 1					/* "]"             */
		+ 1;					/* final NUL       */

	title = wmalloc(len);
	strcpy(title, prefix);

	if (instance || class) {
		strcat(title, " [");
		if (instance != NULL)
			strcat(title, instance);
		else
			strcat(title, "?");

		strcat(title, ".");
		if (class != NULL)
			strcat(title, class);
		else
			strcat(title, "?");

		strcat(title, "]");
	}

	return title;
}

static void destroy_dialog_iconchooser(IconPanel *panel, Window parent)
{
	WMReleaseFont(panel->normalfont);
	WMUnmapWidget(panel->win);
	WMDestroyWidget(panel->win);
	wUnmanageWindow(panel->wwin, False, False);
	wfree(panel);
	XDestroyWindow(dpy, parent);
}

Bool wIconChooserDialog(AppSettingsPanel *app_panel, InspectorPanel *ins_panel, WAppIcon *icon, char **file)
{
	virtual_screen *vscr;
	WScreen *scr;
	char *defaultPath, *wantedPath, *title;
	const char *instance, *class;
	int win_width, win_height, wmScaleWidth, wmScaleHeight;
	Window parent;
	IconPanel *panel;
	Bool result;
	WMPoint center;
	int wframeflags;

	panel = wmalloc(sizeof(IconPanel));
	if (app_panel) {
		/* Set values if parent is AppSettingsPanel */
		app_panel->iconchooserdlg = panel;
		instance = app_panel->editedIcon->wm_instance;
		class = app_panel->editedIcon->wm_class;
		vscr = app_panel->wwin->vscr;
	} else if (ins_panel) {
		/* Set values if parent is InspectorPanel */
		ins_panel->iconchooserdlg = panel;
		instance = ins_panel->inspected->wm_instance;
		class = ins_panel->inspected->wm_class;
		vscr = ins_panel->wwin->vscr;
	} else {
		/* Set values if parent is Icon */
		instance = icon->wm_instance;
		class = icon->wm_class;
		vscr = icon->icon->vscr;
	}

	scr = vscr->screen_ptr;
	panel->vscr = vscr;

	WMGetScaleBaseFromSystemFont(scr->wmscreen, &wmScaleWidth, &wmScaleHeight);
	win_width = WMScaleX(ICONDLG_WIDTH);
	win_height = WMScaleY(ICONDLG_HEIGHT);
	create_dialog_iconchooser_widgets(panel, win_width, win_height, wmScaleWidth, wmScaleHeight);
	parent = XCreateSimpleWindow(dpy, scr->root_win, 0, 0, win_width, win_height, 0, 0, 0);
	XReparentWindow(dpy, WMWidgetXID(panel->win), parent, 0, 0);
	title = create_dialog_iconchooser_title(instance, class);
	center = getCenter(vscr, win_width, win_height);

	wframeflags = WFF_BORDER | WFF_TITLEBAR;

	panel->wwin = wManageInternalWindow(vscr, parent, None, title, center.x, center.y, win_width, win_height, wframeflags);
	wfree(title);

	/* put icon paths in the list */
	listIconPaths(panel->dirList);

	WMMapWidget(panel->win);
	wWindowMap(panel->wwin);

	while (!panel->done) {
		XEvent event;

		WMNextEvent(dpy, &event);
		WMHandleEvent(&event);
	}

	if (!panel->result) {
		*file = NULL;
		destroy_dialog_iconchooser(panel, parent);
		return False;
	}

	/*
	 * Check if the file the user selected is not the one that
	 * would be loaded by default with the current search path
	 */
	*file = WMGetListSelectedItem(panel->iconList)->text;
	if (**file == 0) {
		wfree(*file);
		*file = NULL;
		destroy_dialog_iconchooser(panel, parent);
		return False;
	}

	defaultPath = FindImage(wPreferences.icon_path, *file);
	wantedPath = WMGetTextFieldText(panel->fileField);

	/* If the file is not the default, use full path */
	if (strcmp(wantedPath, defaultPath) != 0) {
		*file = wantedPath;
	} else {
		*file = wstrdup(*file);
		wfree(wantedPath);
	}

	wfree(defaultPath);
	result = panel->result;
	destroy_dialog_iconchooser(panel, parent);

	return result;
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
