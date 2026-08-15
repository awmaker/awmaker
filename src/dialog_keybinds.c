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
 * Key Bindings Panel.
 *
 * A scrollable panel listing every assigned keybinding:
 *   - built-in window-manager keybindings (wKeyBindings[WKBD_*]), and
 *   - root-menu SHORTCUTs declared in WMRootMenu (via the SHBinding list).
 *
 * The rows are kept sorted alphabetically by the key combination (with a
 * natural, number-aware comparison so F1, F2, ..., F10 and Mod1+, Mod2+, ...
 * appear in the expected order). Each row renders the action name on the left
 * and the key combination on the right.
 */

#include "awconfig.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WindowMaker.h"
#include "GNUstep.h"
#include "screen.h"
#include "window.h"
#include "framewin.h"
#include "actions.h"
#include "stacking.h"
#include "xinerama.h"
#include "misc.h"
#include "keybind.h"
#include "shbinding.h"

#include <keybinds_meta.h>

#include <WINGs/WINGs.h>

#define KEYBINDS_WIDTH 380
#define KEYBINDS_HEIGHT 380
#define MARGIN 10

typedef struct KeybindsPanel {
	virtual_screen *vscr;
	WWindow *wwin;
	WMWindow *win;
	WMList *list;
	WMFont *font;
	char **keys;		/* key combination label per list row (index-aligned) */
	int key_count;
} KeybindsPanel;

static KeybindsPanel *keybindsPanel = NULL;

/*
 * Human-readable label for a root-menu SHORTCUT (an SHBinding that is not one
 * of the window-manager keybindings). For EXEC/SHEXEC we show the command; the
 * "exec " prefix added at decode time is stripped for readability. The returned
 * string is borrowed (owned by the binding) and must be copied by the caller.
 */
static const char *shbinding_name(const SHBinding *b)
{
	if (b->type == RSM_EXEC && b->cmd) {
		const char *cmd = b->cmd;

		if (strncmp(cmd, "exec ", 5) == 0)
			cmd += 5;
		return cmd;
	}

	switch (b->type) {
	case RSM_RESTART:
		return _("Restart Window Maker");
	case RSM_EXIT:
		return _("Exit window manager");
	case RSM_SHUTDOWN:
		return _("Kill X session");
	case RSM_REFRESH:
		return _("Refresh screen");
	case RSM_ARRANGE_ICONS:
		return _("Arrange icons");
	case RSM_HIDE_OTHERS:
		return _("Hide other applications");
	case RSM_SHOW_ALL:
		return _("Show all windows");
	case RSM_SAVE_SESSION:
		return _("Save session");
	case RSM_CLEAR_SESSION:
		return _("Clear session");
	case RSM_INFO_PANEL:
		return _("Open info panel");
	case RSM_LEGAL_PANEL:
		return _("Open legal panel");
	default:
		return _("Key binding");
	}
}

/*
 * Natural (number-aware) string comparison: digit runs are compared by numeric
 * value so that "F1", "F2", ..., "F10" and "Mod1", "Mod2" sort as expected.
 */
static int natural_cmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
			while (*a == '0')
				a++;
			while (*b == '0')
				b++;
			{
				const char *pa = a, *pb = b;

				while (isdigit((unsigned char)*pa))
					pa++;
				while (isdigit((unsigned char)*pb))
					pb++;

				if (pa - a != pb - b)
					return (pa - a < pb - b) ? -1 : 1;
				while (a < pa) {
					if (*a != *b)
						return (*a < *b) ? -1 : 1;
					a++;
					b++;
				}
			}
		} else {
			if (*a != *b)
				return (*a < *b) ? -1 : 1;
			a++;
			b++;
		}
	}
	return (*a == 0 && *b == 0) ? 0 : (*a ? 1 : -1);
}

typedef struct Row {
	char *name;
	char *key;
} Row;

static int row_cmp(const void *pa, const void *pb)
{
	const Row *a = (const Row *)pa;
	const Row *b = (const Row *)pb;

	return natural_cmp(a->key, b->key);
}

static void drawRow(WMList *lPtr, int index, Drawable d, char *text,
		    int state, WMRect *rect)
{
	KeybindsPanel *panel = WMGetHangedData(lPtr);
	WScreen *scr = panel->vscr->screen_ptr;
	WMScreen *wmscr = WMWidgetScreen(panel->win);
	WMFont *font = panel->font;
	int x, y, width, height;
	WMColor *back;
	const char *key;
	size_t klen, tlen;

	x = rect->pos.x;
	y = rect->pos.y;
	width = rect->size.width;
	height = rect->size.height;

	back = (state & WLDSSelected) ? scr->white : scr->gray;
	XFillRectangle(dpy, d, WMColorGC(back), x, y, width, height);
	XDrawLine(dpy, d, WMColorGC(scr->white), x, y + height - 1, x + width, y + height - 1);

	tlen = strlen(text);
	WMDrawString(wmscr, d, scr->black, font, x + 6, y + 2, text, tlen);

	if (index >= 0 && index < panel->key_count && panel->keys[index]) {
		key = panel->keys[index];
		klen = strlen(key);
		WMDrawString(wmscr, d, scr->black, font,
			     x + width - WMWidthOfString(font, key, klen) - 6,
			     y + 2, key, klen);
	}
}

static void destroy_keybinds_panel(WCoreWindow *foo, void *data, XEvent *event)
{
	KeybindsPanel *panel = keybindsPanel;
	int i;

	(void)foo;
	(void)data;
	(void)event;

	if (!panel)
		return;
	keybindsPanel = NULL;

	WMUnmapWidget(panel->win);
	for (i = 0; i < panel->key_count; i++)
		wfree(panel->keys[i]);
	wfree(panel->keys);
	if (panel->font)
		WMReleaseFont(panel->font);
	WMDestroyWidget(panel->win);
	wUnmanageWindow(panel->wwin, False, False);
	wfree(panel);
}

static int collect_rows(Row *rows, int capacity)
{
	SHBinding *b;
	int count = 0;
	int i;

	/* built-in window-manager keybindings */
	for (i = 0; i < WKBD_LAST; i++) {
		char *key;

		if (wKeyBindings[i].keycode == 0)
			continue;
		if (!KeyBindingTitle(i))
			continue;

		key = GetShortcutKey(&wKeyBindings[i]);
		if (!key)
			continue;

		if (count >= capacity)
			break;
		rows[count].name = wstrdup(_(KeyBindingTitle(i)));
		rows[count].key = key;
		count++;
	}

	/* root-menu SHORTCUTs (skip the RSM_WKBD copies of the built-ins above) */
	for (b = shGetBindings(); b != NULL; b = b->next) {
		char key[128];

		if (b->type == RSM_WKBD)
			continue;
		if (b->keycode == 0)
			continue;

		shLabelFor(b, key, sizeof(key));
		if (key[0] == '\0')
			continue;

		if (count >= capacity)
			break;
		rows[count].name = wstrdup(shbinding_name(b));
		rows[count].key = wstrdup(key);
		count++;
	}

	return count;
}

static void create_keybinds_widgets(virtual_screen *vscr, KeybindsPanel *panel,
				    int win_width, int win_height)
{
	int wmScaleWidth, wmScaleHeight;
	Row *rows;
	int count, capacity, i;

	capacity = WKBD_LAST + 32;

	rows = wmalloc(sizeof(Row) * capacity);
	count = collect_rows(rows, capacity);

	/* natural sort by key combination */
	qsort(rows, count, sizeof(Row), row_cmp);

	panel->keys = wmalloc(sizeof(char *) * (count ? count : 1));
	panel->key_count = count;
	for (i = 0; i < count; i++)
		panel->keys[i] = rows[i].key;

	panel->win = WMCreateWindow(vscr->screen_ptr->wmscreen, "keybinds");
	WMGetScaleBaseFromSystemFont(vscr->screen_ptr->wmscreen, &wmScaleWidth, &wmScaleHeight);
	WMResizeWidget(panel->win, win_width, win_height);

	panel->list = WMCreateList(panel->win);
	WMResizeWidget(panel->list, WMScaleX(win_width - 2 * MARGIN), WMScaleY(win_height - 2 * MARGIN));
	WMMoveWidget(panel->list, WMScaleX(MARGIN), WMScaleY(MARGIN));
	WMSetListUserDrawProc(panel->list, drawRow);

	panel->font = WMSystemFontOfSize(vscr->screen_ptr->wmscreen, WMScaleY(11));
	if (panel->font)
		WMSetListUserDrawItemHeight(panel->list, WMFontHeight(panel->font) + 4);

	for (i = 0; i < count; i++)
		WMAddListItem(panel->list, rows[i].name);

	WMHangData(panel->list, panel);

	/* panel->keys[] now owns the key strings (transferred from rows[i].key);
	 * only free the names, which WMAddListItem copied internally. */
	for (i = 0; i < count; i++)
		wfree(rows[i].name);
	wfree(rows);

	WMRealizeWidget(panel->win);
	WMMapSubwidgets(panel->win);
}

void panel_show_keybinds(virtual_screen *vscr)
{
	KeybindsPanel *panel;
	Window parent;
	WWindow *wwin;
	WMPoint center;
	int wmScaleWidth, wmScaleHeight;
	int win_width, win_height;
	int wframeflags;

	if (keybindsPanel) {
		if (keybindsPanel->vscr->screen_ptr == vscr->screen_ptr) {
			wRaiseFrame(keybindsPanel->wwin->frame->vscr, keybindsPanel->wwin->frame->core);
			wSetFocusTo(vscr, keybindsPanel->wwin);
		}
		return;
	}

	WMGetScaleBaseFromSystemFont(vscr->screen_ptr->wmscreen, &wmScaleWidth, &wmScaleHeight);
	win_width = WMScaleX(KEYBINDS_WIDTH);
	win_height = WMScaleY(KEYBINDS_HEIGHT);

	panel = wmalloc(sizeof(KeybindsPanel));
	panel->vscr = vscr;

	create_keybinds_widgets(vscr, panel, win_width, win_height);

	WMRealizeWidget(panel->win);
	WMMapSubwidgets(panel->win);

	parent = XCreateSimpleWindow(dpy, vscr->screen_ptr->root_win,
				     0, 0, win_width, win_height, 0, 0, 0);
	XReparentWindow(dpy, WMWidgetXID(panel->win), parent, 0, 0);
	center = wGetPointToCenterRectInHead(vscr, wGetHeadForPointerLocation(vscr),
					     win_width, win_height);

	wframeflags = WFF_RIGHT_BUTTON | WFF_BORDER | WFF_TITLEBAR;

	wwin = wManageInternalWindow(vscr, parent, None, _("Key Bindings"),
				     center.x, center.y, win_width, win_height, wframeflags);

	WSETUFLAG(wwin, no_closable, 0);
	WSETUFLAG(wwin, no_close_button, 0);

	wwin->frame->on_click_right = destroy_keybinds_panel;

	panel->wwin = wwin;
	WMMapWidget(panel->win);
	wWindowMap(wwin);

	keybindsPanel = panel;
}
