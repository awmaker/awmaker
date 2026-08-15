/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Icon Chooser Panel.
 *
 * Modal dialog for picking an icon file. Extracted from dialog.c as an
 * autonomous panel (own struct + private event loop) so dialog.c stays small.
 * The IconPanel type is private to this file; callers only receive a pointer
 * (stored in dockedapp.h/winspector.h `iconchooserdlg`).
 */

#include "awconfig.h"

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

#ifndef PATH_MAX
#define PATH_MAX DEFAULT_PATH_MAX
#endif

#include "WindowMaker.h"
#include "GNUstep.h"
#include "screen.h"
#include "window.h"
#include "wdefaults.h"
#include "misc.h"
#include "stacking.h"
#include "framewin.h"
#include "actions.h"
#include "xinerama.h"
#include "dialog.h"
#include "appicon.h"
#include "dockedapp.h"
#include "winspector.h"
#include "dialog_iconchooser.h"

#define ICONDLG_WIDTH 450
#define ICONDLG_HEIGHT 280

typedef struct IconPanel {
	virtual_screen *vscr;
	WWindow *wwin;
	WMWindow *win;

	WMLabel *dirLabel;
	WMLabel *iconLabel;

	WMList *dirList;
	WMList *iconList;
	WMFont *normalfont;

	WMButton *previewButton;

	WMLabel *iconView;

	WMLabel *fileLabel;
	WMTextField *fileField;

	WMButton *okButton;
	WMButton *cancelButton;

	short done;
	short result;
	short preview;
} IconPanel;

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
	center = wGetPointToCenterRectInHead(vscr, wGetHeadForPointerLocation(vscr), win_width, win_height);

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
