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

#include "wconfig.h"

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
 * Human-readable name for each built-in keybinding (WKBD_*). Designated
 * initializers so the table stays robust to enum reordering; unlisted indices
 * fall back to NULL and are skipped.
 */
static const char *wkbd_name[WKBD_LAST] = {
	[WKBD_ROOTMENU] = N_("Open applications menu"),
	[WKBD_WINDOWMENU] = N_("Open window commands menu"),
	[WKBD_WINDOWLIST] = N_("Open window list menu"),
	[WKBD_WORKSPACEMENU] = N_("Open workspace menu"),
	[WKBD_MINIATURIZE] = N_("Miniaturize active window"),
	[WKBD_MINIMIZEALL] = N_("Miniaturize all windows"),
	[WKBD_HIDE] = N_("Hide active application"),
	[WKBD_HIDE_OTHERS] = N_("Hide other applications"),
	[WKBD_MAXIMIZE] = N_("Maximize active window"),
	[WKBD_VMAXIMIZE] = N_("Maximize active window vertically"),
	[WKBD_HMAXIMIZE] = N_("Maximize active window horizontally"),
	[WKBD_CENTRAL] = N_("Maximize active window central"),
	[WKBD_LHMAXIMIZE] = N_("Maximize active window left half"),
	[WKBD_RHMAXIMIZE] = N_("Maximize active window right half"),
	[WKBD_THMAXIMIZE] = N_("Maximize active window top half"),
	[WKBD_BHMAXIMIZE] = N_("Maximize active window bottom half"),
	[WKBD_LTCMAXIMIZE] = N_("Maximize active window left top corner"),
	[WKBD_RTCMAXIMIZE] = N_("Maximize active window right top corner"),
	[WKBD_LBCMAXIMIZE] = N_("Maximize active window left bottom corner"),
	[WKBD_RBCMAXIMIZE] = N_("Maximize active window right bottom corner"),
	[WKBD_MAXIMUS] = N_("Tiled maximization"),
	[WKBD_SELECT] = N_("Select active window"),
	[WKBD_KEEP_ON_TOP] = N_("Toggle window on top status"),
	[WKBD_KEEP_AT_BOTTOM] = N_("Toggle window at bottom status"),
	[WKBD_OMNIPRESENT] = N_("Toggle window omnipresent status"),
	[WKBD_RAISE] = N_("Raise active window"),
	[WKBD_LOWER] = N_("Lower active window"),
	[WKBD_RAISELOWER] = N_("Raise/Lower window under mouse pointer"),
	[WKBD_MOVERESIZE] = N_("Move/Resize active window"),
	[WKBD_SHADE] = N_("Shade active window"),
	[WKBD_WORKSPACEMAP] = N_("Open workspace pager"),
	[WKBD_FOCUSNEXT] = N_("Focus next window"),
	[WKBD_FOCUSPREV] = N_("Focus previous window"),
	[WKBD_FOCUSLEFT] = N_("Focus window to the left"),
	[WKBD_FOCUSRIGHT] = N_("Focus window to the right"),
	[WKBD_FOCUSUP] = N_("Focus window above"),
	[WKBD_FOCUSDOWN] = N_("Focus window below"),
	[WKBD_GROUPNEXT] = N_("Focus next group window"),
	[WKBD_GROUPPREV] = N_("Focus previous group window"),
	[WKBD_MARK_SET] = N_("Mark window: set mark"),
	[WKBD_MARK_UNSET] = N_("Mark window: unset mark"),
	[WKBD_MARK_BRING] = N_("Mark window: bring marked window here"),
	[WKBD_MARK_JUMP] = N_("Mark window: jump to marked window"),
	[WKBD_MARK_SWAP] = N_("Mark window: swap with marked window"),
	[WKBD_CLOSE] = N_("Close active window"),
	[WKBD_DOCKRAISELOWER] = N_("Raise/Lower Dock"),
	[WKBD_CLIPRAISELOWER] = N_("Raise/Lower Clip"),
	[WKBD_WORKSPACE1] = N_("Switch to workspace 1"),
	[WKBD_WORKSPACE2] = N_("Switch to workspace 2"),
	[WKBD_WORKSPACE3] = N_("Switch to workspace 3"),
	[WKBD_WORKSPACE4] = N_("Switch to workspace 4"),
	[WKBD_WORKSPACE5] = N_("Switch to workspace 5"),
	[WKBD_WORKSPACE6] = N_("Switch to workspace 6"),
	[WKBD_WORKSPACE7] = N_("Switch to workspace 7"),
	[WKBD_WORKSPACE8] = N_("Switch to workspace 8"),
	[WKBD_WORKSPACE9] = N_("Switch to workspace 9"),
	[WKBD_WORKSPACE10] = N_("Switch to workspace 10"),
	[WKBD_NEXTWORKSPACE] = N_("Switch to next workspace"),
	[WKBD_PREVWORKSPACE] = N_("Switch to previous workspace"),
	[WKBD_LASTWORKSPACE] = N_("Switch to last used workspace"),
	[WKBD_NEXTWSLAYER] = N_("Switch to next ten workspaces"),
	[WKBD_PREVWSLAYER] = N_("Switch to previous ten workspaces"),
	[WKBD_MOVE_WORKSPACE1] = N_("Move window to workspace 1"),
	[WKBD_MOVE_WORKSPACE2] = N_("Move window to workspace 2"),
	[WKBD_MOVE_WORKSPACE3] = N_("Move window to workspace 3"),
	[WKBD_MOVE_WORKSPACE4] = N_("Move window to workspace 4"),
	[WKBD_MOVE_WORKSPACE5] = N_("Move window to workspace 5"),
	[WKBD_MOVE_WORKSPACE6] = N_("Move window to workspace 6"),
	[WKBD_MOVE_WORKSPACE7] = N_("Move window to workspace 7"),
	[WKBD_MOVE_WORKSPACE8] = N_("Move window to workspace 8"),
	[WKBD_MOVE_WORKSPACE9] = N_("Move window to workspace 9"),
	[WKBD_MOVE_WORKSPACE10] = N_("Move window to workspace 10"),
	[WKBD_MOVE_NEXTWORKSPACE] = N_("Move window to next workspace"),
	[WKBD_MOVE_PREVWORKSPACE] = N_("Move window to previous workspace"),
	[WKBD_MOVE_LASTWORKSPACE] = N_("Move window to last used workspace"),
	[WKBD_MOVE_NEXTWSLAYER] = N_("Move window to next ten workspaces"),
	[WKBD_MOVE_PREVWSLAYER] = N_("Move window to previous ten workspaces"),
	[WKBD_WINDOW1] = N_("Shortcut for window 1"),
	[WKBD_WINDOW2] = N_("Shortcut for window 2"),
	[WKBD_WINDOW3] = N_("Shortcut for window 3"),
	[WKBD_WINDOW4] = N_("Shortcut for window 4"),
	[WKBD_WINDOW5] = N_("Shortcut for window 5"),
	[WKBD_WINDOW6] = N_("Shortcut for window 6"),
	[WKBD_WINDOW7] = N_("Shortcut for window 7"),
	[WKBD_WINDOW8] = N_("Shortcut for window 8"),
	[WKBD_WINDOW9] = N_("Shortcut for window 9"),
	[WKBD_WINDOW10] = N_("Shortcut for window 10"),
	[WKBD_MOVE_12_TO_6_HEAD] = N_("Move to right/bottom/left/top head"),
	[WKBD_MOVE_6_TO_12_HEAD] = N_("Move to left/top/right/bottom head"),
	[WKBD_RELAUNCH] = N_("Launch new instance of application"),
	[WKBD_SWITCH_SCREEN] = N_("Switch to Next Screen/Monitor"),
	[WKBD_RUN] = N_("Run application"),
#ifdef KEEP_XKB_LOCK_STATUS
	[WKBD_TOGGLE] = N_("Toggle keyboard language"),
#endif
};

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
		if (!wkbd_name[i])
			continue;

		key = GetShortcutKey(wKeyBindings[i]);
		if (!key)
			continue;

		if (count >= capacity)
			break;
		rows[count].name = wstrdup(_(wkbd_name[i]));
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
