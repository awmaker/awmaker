/* workspace.c- Workspace management
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
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
#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef USE_XSHAPE
#include <X11/extensions/shape.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "WindowMaker.h"
#include "framewin.h"
#include "window.h"
#include "icon.h"
#include "misc.h"
#include "menu.h"
#include "application.h"
#include "dock.h"
#include "actions.h"
#include "workspace.h"
#include "appicon.h"
#include "wmspec.h"
#include "xinerama.h"
#include "event.h"
#include "wsmap.h"
#include "dialog.h"

#define MC_DESTROY_LAST 1
#define MC_LAST_USED    2
/* index of the first workspace menu entry */
#define MC_WORKSPACE1   3

#define WORKSPACE_NAME_DISPLAY_PADDING 32

int set_clip_omnipresent(virtual_screen *vscr, int wksno);

void menu_workspace_addwks(virtual_screen *vscr, WMenu *menu);
void menu_workspace_delwks(virtual_screen *vscr, WMenu *menu);
void menu_workspace_shortcut_labels(virtual_screen *vscr, WMenu *menu);

static WMPropList *dWorkspaces = NULL;
static WMPropList *dClip, *dName;

static void make_keys(void)
{
	if (dWorkspaces != NULL)
		return;

	dWorkspaces = WMCreatePLString("Workspaces");
	dName = WMCreatePLString("Name");
	dClip = WMCreatePLString("Clip");
}

static void set_workspace_name(virtual_screen *vscr, WWorkspace *wspace, char *name)
{
	static const char *new_name = NULL;
	static size_t name_length;

	wspace->name = NULL;

	if (name) {
		wspace->name = wstrdup(name);
	} else {
		new_name = _("Workspace %i");
		name_length = strlen(new_name) + 8;
		wspace->name = wmalloc(name_length);
		snprintf(wspace->name, name_length, new_name, vscr->workspace.count);
	}
}

static void update_workspace_list(virtual_screen *vscr, WWorkspace *wspace)
{
	WWorkspace **list;
	int i;

	list = wmalloc(sizeof(WWorkspace *) * vscr->workspace.count);

	for (i = 0; i < vscr->workspace.count - 1; i++)
		list[i] = vscr->workspace.array[i];

	list[i] = wspace;
	if (vscr->workspace.array)
		wfree(vscr->workspace.array);

	vscr->workspace.array = list;
}

static void set_clip_in_workspace(virtual_screen *vscr, WWorkspace *wspace, WMPropList *wks_state)
{
	WMPropList *clip_state = NULL;
	wspace->clip = NULL;

	if (wPreferences.flags.noclip)
		return;

	if (wks_state)
		clip_state = WMGetFromPLDictionary(wks_state, dClip);

	wspace->clip = clip_create(vscr, clip_state);
}

static void set_clip_in_workspace_map(virtual_screen *vscr, WWorkspace *wspace, int wksno, WMPropList *wks_state)
{
	WMPropList *clip_state, *tmp_state;

	make_keys();

	if (wksno < 0)
		tmp_state = w_global.session_state;
	else
		tmp_state = wks_state;

	if (wPreferences.flags.noclip)
		return;

	clip_state = WMGetFromPLDictionary(tmp_state, dClip);
	clip_map(wspace->clip, clip_state);

	if (wksno >= 0) {
		if (wksno > 0)
			wDockHideIcons(vscr->workspace.array[wksno]->clip);

		vscr->workspace.array[0]->clip->icon_count += set_clip_omnipresent(vscr, wksno);
	}
}

static void workspace_create_core(virtual_screen *vscr, WMPropList *wks_state, char *wksname)
{
	WWorkspace *wspace;

	if (vscr->workspace.count >= MAX_WORKSPACES)
		return;

	make_keys();

	/* Create a new one */
	wspace = wmalloc(sizeof(WWorkspace));
	vscr->workspace.count++;

	/* Set the workspace name */
	set_workspace_name(vscr, wspace, wksname);
	update_workspace_list(vscr, wspace);

	set_clip_in_workspace(vscr, wspace, wks_state);

	menu_workspace_addwks(vscr, vscr->workspace.menu);
	menu_workspace_shortcut_labels(vscr, vscr->workspace.menu);
}

void workspace_create(virtual_screen *vscr)
{
	workspace_create_core(vscr, NULL, NULL);
}

static void workspace_create_with_state(virtual_screen *vscr, int wksno, WMPropList *parr)
{
	WMPropList *pstr, *wks_state = NULL;
	char *wksname = NULL;

	make_keys();

	wks_state = WMGetFromPLArray(parr, wksno);
	if (WMIsPLDictionary(wks_state))
		pstr = WMGetFromPLDictionary(wks_state, dName);
	else
		pstr = wks_state;

	wksname = WMGetFromPLString(pstr);

	workspace_create_core(vscr, wks_state, wksname);
}

void workspace_map(virtual_screen *vscr, WWorkspace *wspace, int wksno, WMPropList *parr)
{
	WMPropList *wks_state = NULL;

	make_keys();

	if (parr != NULL)
		wks_state = WMGetFromPLArray(parr, wksno);

	if ((!wPreferences.flags.noclip) && (!vscr->clip.mapped))
		clip_icon_map(vscr);

	set_clip_in_workspace_map(vscr, wspace, wksno, wks_state);
	wWorkspaceMenuUpdate_map(vscr);

	wNETWMUpdateDesktop(vscr);
	WMPostNotificationName(WMNWorkspaceCreated, vscr, (void *)(uintptr_t) (vscr->workspace.count - 1));
	XFlush(dpy);
}

static void update_submenu(WMenu *menu)
{
	virtual_screen *vscr;
	int i;

	if (!menu)
		return;

	vscr = menu->vscr;
	i = menu->entry_no;
	while (i > vscr->workspace.count)
		wMenuRemoveItem(menu, --i);
}


Bool wWorkspaceDelete(virtual_screen *vscr, int workspace)
{
	WWindow *tmp;
	WWorkspace **list;
	int i, j;
	char buf[256];

	if (workspace <= 0)
		return False;

	/* verify if workspace is in use by some window */
	tmp = vscr->window.focused;
	while (tmp) {
		if (!IS_OMNIPRESENT(tmp) && tmp->frame->workspace == workspace) {
			snprintf(buf, sizeof(buf), _("Workspace \"%s\" in use; cannot delete"),
			 vscr->workspace.array[workspace]->name);
			wMessageDialog(vscr, _("Error"), buf, _("OK"), NULL, NULL);

			return False;
		}

		tmp = tmp->prev;
	}

	if (!wPreferences.flags.noclip) {
		clip_destroy(vscr->workspace.array[workspace]->clip);
		vscr->workspace.array[workspace]->clip = NULL;
	}

	list = wmalloc(sizeof(WWorkspace *) * (vscr->workspace.count - 1));
	j = 0;
	for (i = 0; i < vscr->workspace.count; i++) {
		if (i != workspace) {
			list[j++] = vscr->workspace.array[i];
		} else {
			if (vscr->workspace.array[i]->name)
				wfree(vscr->workspace.array[i]->name);

			if (vscr->workspace.array[i]->map)
				RReleaseImage(vscr->workspace.array[i]->map);

			wfree(vscr->workspace.array[i]);
		}
	}

	wfree(vscr->workspace.array);
	vscr->workspace.array = list;

	vscr->workspace.count--;

	menu_workspace_delwks(vscr, vscr->workspace.menu);
	menu_workspace_shortcut_labels(vscr, vscr->workspace.menu);
	wWorkspaceMenuUpdate_map(vscr);

	update_submenu(vscr->workspace.submenu);

	wNETWMUpdateDesktop(vscr);
	WMPostNotificationName(WMNWorkspaceDestroyed, vscr, (void *)(uintptr_t) (vscr->workspace.count - 1));

	if (vscr->workspace.current >= vscr->workspace.count)
		wWorkspaceChange(vscr, vscr->workspace.count - 1);

	if (vscr->workspace.last_used >= vscr->workspace.count)
		vscr->workspace.last_used = 0;

	return True;
}

typedef struct WorkspaceNameData {
	int count;
	RImage *back;
	RImage *text;
	time_t timeout;
} WorkspaceNameData;

static void hideWorkspaceName(void *data)
{
	WScreen *scr = (WScreen *) data;

	if (!scr->workspace_name_data || scr->workspace_name_data->count == 0
	    || time(NULL) > scr->workspace_name_data->timeout) {
		XUnmapWindow(dpy, scr->workspace_name);

		if (scr->workspace_name_data) {
			RReleaseImage(scr->workspace_name_data->back);
			RReleaseImage(scr->workspace_name_data->text);
			wfree(scr->workspace_name_data);

			scr->workspace_name_data = NULL;
		}
		scr->workspace_name_timer = NULL;
	} else {
		RImage *img = RCloneImage(scr->workspace_name_data->back);
		Pixmap pix;

		scr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_FADE_DELAY, hideWorkspaceName, scr);

		RCombineImagesWithOpaqueness(img, scr->workspace_name_data->text,
					     scr->workspace_name_data->count * 255 / 10);

		RConvertImage(scr->rcontext, img, &pix);

		RReleaseImage(img);

		XSetWindowBackgroundPixmap(dpy, scr->workspace_name, pix);
		XClearWindow(dpy, scr->workspace_name);
		XFreePixmap(dpy, pix);
		XFlush(dpy);

		scr->workspace_name_data->count--;
	}
}

static void showWorkspaceName(virtual_screen *vscr, int workspace)
{
	WorkspaceNameData *data;
	RXImage *ximg;
	Pixmap text, mask;
	int w, h;
	int px, py;
	char *name = vscr->workspace.array[workspace]->name;
	int len = strlen(name);
	int x, y;
#ifdef USE_XINERAMA
	int head;
	WMRect rect;
	int xx, yy;
#endif

	if (wPreferences.workspace_name_display_position == WD_NONE || vscr->workspace.count < 2)
		return;

	if (vscr->screen_ptr->workspace_name_timer) {
		WMDeleteTimerHandler(vscr->screen_ptr->workspace_name_timer);
		XUnmapWindow(dpy, vscr->screen_ptr->workspace_name);
		XFlush(dpy);
	}

	vscr->screen_ptr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_DELAY, hideWorkspaceName, vscr->screen_ptr);

	if (vscr->screen_ptr->workspace_name_data) {
		RReleaseImage(vscr->screen_ptr->workspace_name_data->back);
		RReleaseImage(vscr->screen_ptr->workspace_name_data->text);
		wfree(vscr->screen_ptr->workspace_name_data);
	}

	data = wmalloc(sizeof(WorkspaceNameData));
	data->back = NULL;

	w = WMWidthOfString(vscr->workspace.font_for_name, name, len);
	h = WMFontHeight(vscr->workspace.font_for_name);

#ifdef USE_XINERAMA
	head = wGetHeadForPointerLocation(vscr);
	rect = wGetRectForHead(vscr->screen_ptr, head);
	if (vscr->screen_ptr->xine_info.count) {
		xx = rect.pos.x + (vscr->screen_ptr->xine_info.screens[head].size.width - (w + 4)) / 2;
		yy = rect.pos.y + (vscr->screen_ptr->xine_info.screens[head].size.height - (h + 4)) / 2;
	} else {
		xx = (vscr->screen_ptr->scr_width - (w + 4)) / 2;
		yy = (vscr->screen_ptr->scr_height - (h + 4)) / 2;
	}
#endif

	switch (wPreferences.workspace_name_display_position) {
	case WD_TOP:
#ifdef USE_XINERAMA
		px = xx;
#else
		px = (vscr->screen_ptr->scr_width - (w + 4)) / 2;
#endif
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_BOTTOM:
#ifdef USE_XINERAMA
		px = xx;
#else
		px = (vscr->screen_ptr->scr_width - (w + 4)) / 2;
#endif
		py = vscr->screen_ptr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_TOPLEFT:
		px = WORKSPACE_NAME_DISPLAY_PADDING;
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_TOPRIGHT:
		px = vscr->screen_ptr->scr_width - (w + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_BOTTOMLEFT:
		px = WORKSPACE_NAME_DISPLAY_PADDING;
		py = vscr->screen_ptr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_BOTTOMRIGHT:
		px = vscr->screen_ptr->scr_width - (w + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		py = vscr->screen_ptr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_CENTER:
	default:
#ifdef USE_XINERAMA
		px = xx;
		py = yy;
#else
		px = (vscr->screen_ptr->scr_width - (w + 4)) / 2;
		py = (vscr->screen_ptr->scr_height - (h + 4)) / 2;
#endif
		break;
	}

	XResizeWindow(dpy, vscr->screen_ptr->workspace_name, w + 4, h + 4);
	XMoveWindow(dpy, vscr->screen_ptr->workspace_name, px, py);

	text = XCreatePixmap(dpy, vscr->screen_ptr->w_win, w + 4, h + 4, vscr->screen_ptr->w_depth);
	mask = XCreatePixmap(dpy, vscr->screen_ptr->w_win, w + 4, h + 4, 1);

	XFillRectangle(dpy, text, WMColorGC(vscr->screen_ptr->black), 0, 0, w + 4, h + 4);

	for (x = 0; x <= 4; x++)
		for (y = 0; y <= 4; y++)
			WMDrawString(vscr->screen_ptr->wmscreen, text, vscr->screen_ptr->white, vscr->workspace.font_for_name, x, y, name, len);

	XSetForeground(dpy, vscr->screen_ptr->mono_gc, 1);
	XSetBackground(dpy, vscr->screen_ptr->mono_gc, 0);
	XCopyPlane(dpy, text, mask, vscr->screen_ptr->mono_gc, 0, 0, w + 4, h + 4, 0, 0, 1 << (vscr->screen_ptr->w_depth - 1));
	XSetBackground(dpy, vscr->screen_ptr->mono_gc, 1);
	XFillRectangle(dpy, text, WMColorGC(vscr->screen_ptr->black), 0, 0, w + 4, h + 4);
	WMDrawString(vscr->screen_ptr->wmscreen, text, vscr->screen_ptr->white, vscr->workspace.font_for_name, 2, 2, name, len);

#ifdef USE_XSHAPE
	if (w_global.xext.shape.supported)
		XShapeCombineMask(dpy, vscr->screen_ptr->workspace_name, ShapeBounding, 0, 0, mask, ShapeSet);
#endif
	XSetWindowBackgroundPixmap(dpy, vscr->screen_ptr->workspace_name, text);
	XClearWindow(dpy, vscr->screen_ptr->workspace_name);

	data->text = RCreateImageFromDrawable(vscr->screen_ptr->rcontext, text, None);

	XFreePixmap(dpy, text);
	XFreePixmap(dpy, mask);

	if (!data->text) {
		XMapRaised(dpy, vscr->screen_ptr->workspace_name);
		XFlush(dpy);

		goto erro;
	}

	ximg = RGetXImage(vscr->screen_ptr->rcontext, vscr->screen_ptr->root_win, px, py, data->text->width, data->text->height);
	if (!ximg)
		goto erro;

	XMapRaised(dpy, vscr->screen_ptr->workspace_name);
	XFlush(dpy);

	data->back = RCreateImageFromXImage(vscr->screen_ptr->rcontext, ximg->image, NULL);
	RDestroyXImage(vscr->screen_ptr->rcontext, ximg);

	if (!data->back)
		goto erro;

	data->count = 10;

	/* set a timeout for the effect */
	data->timeout = time(NULL) + 2 + (WORKSPACE_NAME_DELAY + WORKSPACE_NAME_FADE_DELAY * data->count) / 1000;

	vscr->screen_ptr->workspace_name_data = data;

	return;

 erro:
	if (vscr->screen_ptr->workspace_name_timer)
		WMDeleteTimerHandler(vscr->screen_ptr->workspace_name_timer);

	if (data->text)
		RReleaseImage(data->text);

	if (data->back)
		RReleaseImage(data->back);

	wfree(data);

	vscr->screen_ptr->workspace_name_data = NULL;

	vscr->screen_ptr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_DELAY +
						      10 * WORKSPACE_NAME_FADE_DELAY, hideWorkspaceName, vscr->screen_ptr);
}

void wWorkspaceChange(virtual_screen *vscr, int workspace)
{
	if (w_global.startup.phase1 || w_global.startup.phase2 || vscr->screen_ptr->flags.ignore_focus_events)
		return;

	if (workspace != vscr->workspace.current)
		wWorkspaceForceChange(vscr, workspace);
}

void wWorkspaceRelativeChange(virtual_screen *vscr, int amount)
{
	int w;

	/* While the deiconify animation is going on the window is
	 * still "flying" to its final position and we don't want to
	 * change workspace before the animation finishes, otherwise
	 * the window will land in the new workspace */
	if (vscr->workspace.ignore_change)
		return;

	w = vscr->workspace.current + amount;

	if (amount < 0) {
		if (w >= 0)
			wWorkspaceChange(vscr, w);
		else if (wPreferences.ws_cycle)
			wWorkspaceChange(vscr, vscr->workspace.count + w);
	} else if (amount > 0) {
		if (w < vscr->workspace.count)
			wWorkspaceChange(vscr, w);
		else if (wPreferences.ws_advance)
			wWorkspaceChange(vscr, WMIN(w, MAX_WORKSPACES - 1));
		else if (wPreferences.ws_cycle)
			wWorkspaceChange(vscr, w % vscr->workspace.count);
	}
}

void wWorkspaceForceChange(virtual_screen *vscr, int workspace)
{
	WWindow *tmp, *foc = NULL, *foc2 = NULL;
	int count, s1, s2;

	if (workspace >= MAX_WORKSPACES || workspace < 0)
		return;

	if (wPreferences.enable_workspace_pager &&
	    !vscr->workspace.process_map_event)
		wWorkspaceMapUpdate(vscr);

	SendHelperMessage(vscr, 'C', workspace + 1, NULL);

	if (workspace > vscr->workspace.count - 1) {
		count = workspace - vscr->workspace.count + 1;
		while (count > 0) {
			s1 = vscr->workspace.count;
			workspace_create(vscr);
			s2 = vscr->workspace.count;
			if (s2 > s1)
				workspace_map(vscr, vscr->workspace.array[vscr->workspace.count - 1], -1, NULL);

			count--;
		}
	}

	wClipUpdateForWorkspaceChange(vscr, workspace);

	vscr->workspace.last_used = vscr->workspace.current;
	vscr->workspace.current = workspace;

	wWorkspaceMenuUpdate(vscr, vscr->workspace.menu);
	wWorkspaceMenuUpdate_map(vscr);

	tmp = vscr->window.focused;
	if (tmp != NULL) {
		WWindow **toUnmap;
		int toUnmapSize, toUnmapCount;

		if ((IS_OMNIPRESENT(tmp) && (tmp->flags.mapped || tmp->flags.shaded) &&
		     !WFLAGP(tmp, no_focusable)) || tmp->flags.changing_workspace)
			foc = tmp;

		toUnmapSize = 16;
		toUnmapCount = 0;
		toUnmap = wmalloc(toUnmapSize * sizeof(WWindow *));

		/* foc2 = tmp; will fix annoyance with gnome panel
		 * but will create annoyance for every other application
		 */
		while (tmp) {
			if (tmp->frame->workspace != workspace && !tmp->flags.selected) {
				/* unmap windows not on this workspace */
				if ((tmp->flags.mapped || tmp->flags.shaded) &&
				    !IS_OMNIPRESENT(tmp) && !tmp->flags.changing_workspace) {
					if (toUnmapCount == toUnmapSize) {
						toUnmapSize *= 2;
						toUnmap = wrealloc(toUnmap, toUnmapSize * sizeof(WWindow *));
					}

					toUnmap[toUnmapCount++] = tmp;
				}
				/* also unmap miniwindows not on this workspace */
				if (!wPreferences.sticky_icons && tmp->flags.miniaturized &&
				    tmp->icon && !IS_OMNIPRESENT(tmp)) {
					XUnmapWindow(dpy, tmp->icon->core->window);
					tmp->icon->mapped = 0;
				}

				/* update current workspace of omnipresent windows */
				if (IS_OMNIPRESENT(tmp)) {
					WApplication *wapp = wApplicationOf(tmp->main_window);

					tmp->frame->workspace = workspace;

					if (wapp)
						wapp->last_workspace = workspace;

					if (!foc2 && (tmp->flags.mapped || tmp->flags.shaded))
						foc2 = tmp;
				}
			} else {
				/* change selected windows' workspace */
				if (tmp->flags.selected) {
					wWindowChangeWorkspace(tmp, workspace);
					if (!tmp->flags.miniaturized && !foc)
						foc = tmp;

				} else {
					if (!tmp->flags.hidden) {
						if (!(tmp->flags.mapped || tmp->flags.miniaturized)) {
							/* remap windows that are on this workspace */
							wWindowMap(tmp);
							if (!foc && !WFLAGP(tmp, no_focusable))
								foc = tmp;
						}
						/* Also map miniwindow if not omnipresent */
						if (!wPreferences.sticky_icons &&
						    tmp->flags.miniaturized && !IS_OMNIPRESENT(tmp) && tmp->icon) {
							tmp->icon->mapped = 1;
							XMapWindow(dpy, tmp->icon->core->window);
						}
					}
				}
			}
			tmp = tmp->prev;
		}

		while (toUnmapCount > 0)
			wWindowUnmap(toUnmap[--toUnmapCount]);

		wfree(toUnmap);

		/* Gobble up events unleashed by our mapping & unmapping.
		 * These may trigger various grab-initiated focus &
		 * crossing events. However, we don't care about them,
		 * and ignore their focus implications altogether to avoid
		 * flicker.
		 */
		vscr->screen_ptr->flags.ignore_focus_events = 1;
		ProcessPendingEvents();
		vscr->screen_ptr->flags.ignore_focus_events = 0;

		if (!foc)
			foc = foc2;

		/*
		 * Check that the window we want to focus still exists, because the application owning it
		 * could decide to unmap/destroy it in response to unmap any of its other window following
		 * the workspace change, this happening during our 'ProcessPendingEvents' loop.
		 */
		if (foc != NULL) {
			WWindow *parse;
			Bool found;

			found = False;
			for (parse = vscr->window.focused; parse != NULL; parse = parse->prev) {
				if (parse == foc) {
					found = True;
					break;
				}
			}
			if (!found)
				foc = NULL;
		}

		if (vscr->window.focused->flags.mapped && !foc)
			foc = vscr->window.focused;

		if (wPreferences.focus_mode == WKF_CLICK) {
			wSetFocusTo(vscr, foc);
		} else {
			unsigned int mask;
			int foo;
			Window bar, win;
			WWindow *tmp;

			tmp = NULL;
			if (XQueryPointer(dpy, vscr->screen_ptr->root_win, &bar, &win, &foo, &foo, &foo, &foo, &mask))
				tmp = wWindowFor(win);

			/* If there's a window under the pointer, focus it.
			 * (we ate all other focus events above, so it's
			 * certainly not focused). Otherwise focus last
			 * focused, or the root (depending on sloppiness)
			 */
			if (!tmp && wPreferences.focus_mode == WKF_SLOPPY)
				wSetFocusTo(vscr, foc);
			else
				wSetFocusTo(vscr, tmp);
		}
	}

	/* We need to always arrange icons when changing workspace, even if
	 * no autoarrange icons, because else the icons in different workspaces
	 * can be superposed.
	 * This can be avoided if appicons are also workspace specific.
	 */
	if (!wPreferences.sticky_icons)
		wArrangeIcons(vscr, False);

	if (vscr->dock.dock)
		wAppIconPaint(vscr->dock.dock->icon_array[0]);

	if (!wPreferences.flags.noclip && (vscr->workspace.array[workspace]->clip->auto_collapse ||
					   vscr->workspace.array[workspace]->clip->auto_raise_lower)) {
		/* to handle enter notify. This will also */
		XUnmapWindow(dpy, vscr->clip.icon->icon->core->window);
		XMapWindow(dpy, vscr->clip.icon->icon->core->window);
	} else if (vscr->clip.mapped) {
		wClipIconPaint(vscr->clip.icon);
	}

	wScreenUpdateUsableArea(vscr);
	wNETWMUpdateDesktop(vscr);
	showWorkspaceName(vscr, workspace);

	WMPostNotificationName(WMNWorkspaceChanged, vscr, (void *)(uintptr_t) workspace);
}

static void switchWSCommand(WMenu *menu, WMenuEntry *entry)
{
	wWorkspaceChange(menu->vscr, (long)entry->clientdata);
}

static void lastWSCommand(WMenu *menu, WMenuEntry *entry)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;

	wWorkspaceChange(menu->vscr, menu->vscr->workspace.last_used);
}

static void deleteWSCommand(WMenu *menu, WMenuEntry *entry)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;

	wWorkspaceDelete(menu->vscr, menu->vscr->workspace.count - 1);
}

static void newWSCommand(WMenu *menu, WMenuEntry *foo)
{
	int s1, s2;
	virtual_screen *vscr = menu->vscr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) foo;

	s1 = vscr->workspace.count;
	workspace_create(vscr);
	s2 = vscr->workspace.count;

	/* autochange workspace */
	if (s2 > s1) {
		workspace_map(vscr,
			      vscr->workspace.array[vscr->workspace.count - 1],
			      -1, NULL);
		wWorkspaceChange(vscr, s2 - 1);
	}
}

void wWorkspaceRename(virtual_screen *vscr, int workspace, const char *name)
{
	char buf[MAX_WORKSPACENAME_WIDTH + 1];
	char *tmp;

	if (workspace >= vscr->workspace.count)
		return;

	/* trim white spaces */
	tmp = wtrimspace(name);

	if (strlen(tmp) == 0)
		snprintf(buf, sizeof(buf), _("Workspace %i"), workspace + 1);
	else
		strncpy(buf, tmp, MAX_WORKSPACENAME_WIDTH);

	buf[MAX_WORKSPACENAME_WIDTH] = 0;
	wfree(tmp);

	/* update workspace */
	wfree(vscr->workspace.array[workspace]->name);
	vscr->workspace.array[workspace]->name = wstrdup(buf);

	if (vscr->workspace.menu) {
		if (strcmp(vscr->workspace.menu->entries[workspace + MC_WORKSPACE1]->text, buf) != 0) {
			wfree(vscr->workspace.menu->entries[workspace + MC_WORKSPACE1]->text);
			vscr->workspace.menu->entries[workspace + MC_WORKSPACE1]->text = wstrdup(buf);
			wMenuRealize(vscr->workspace.menu);
		}
	}

	if (vscr->clip.icon)
		wClipIconPaint(vscr->clip.icon);

	WMPostNotificationName(WMNWorkspaceNameChanged, vscr, (void *)(uintptr_t) workspace);
}

/* callback for when menu entry is edited */
static void onMenuEntryEdited(WMenu *menu, WMenuEntry *entry)
{
	char *tmp;

	tmp = entry->text;
	wWorkspaceRename(menu->vscr, (long)entry->clientdata, tmp);
}

WMenu *wWorkspaceMenuMake(virtual_screen *vscr, Bool titled)
{
	WMenu *wsmenu;
	WMenuEntry *entry;

	if (titled)
		wsmenu = menu_create(vscr, _("Workspaces"));
	else
		wsmenu = menu_create(vscr, NULL);

	/* callback to be called when an entry is edited */
	wsmenu->on_edit = onMenuEntryEdited;

	wMenuAddCallback(wsmenu, _("New"), newWSCommand, NULL);
	wMenuAddCallback(wsmenu, _("Destroy Last"), deleteWSCommand, NULL);
	entry = wMenuAddCallback(wsmenu, _("Last Used"), lastWSCommand, NULL);
	entry->rtext = GetShortcutKey(wKeyBindings[WKBD_LASTWORKSPACE]);

	return wsmenu;
}

void menu_workspace_addwks(virtual_screen *vscr, WMenu *menu)
{
	char title[MAX_WORKSPACENAME_WIDTH + 1];
	WMenuEntry *entry;
	int i;
	long ws;

	if (!menu)
		return;

	/* new workspace(s) added */
	i = vscr->workspace.count - (menu->entry_no - MC_WORKSPACE1);
	ws = menu->entry_no - MC_WORKSPACE1;
	while (i > 0) {
		wstrlcpy(title, vscr->workspace.array[ws]->name, MAX_WORKSPACENAME_WIDTH);

		entry = wMenuAddCallback(menu, title, switchWSCommand, (void *)ws);
		entry->flags.indicator = 1;
		entry->flags.editable = 1;

		i--;
		ws++;
	}
}

void menu_workspace_delwks(virtual_screen *vscr, WMenu *menu)
{
	int i;

	if (!menu)
		return;

	for (i = menu->entry_no - 1; i >= vscr->workspace.count + MC_WORKSPACE1; i--)
		wMenuRemoveItem(menu, i);
}

void menu_workspace_shortcut_labels(virtual_screen *vscr, WMenu *menu)
{
	int i;

	if (!menu)
		return;

	for (i = 0; i < vscr->workspace.count; i++) {
		/* workspace shortcut labels */
		if (i / 10 == vscr->workspace.current / 10)
			menu->entries[i + MC_WORKSPACE1]->rtext = GetShortcutKey(wKeyBindings[WKBD_WORKSPACE1 + (i % 10)]);
		else
			menu->entries[i + MC_WORKSPACE1]->rtext = NULL;

		menu->entries[i + MC_WORKSPACE1]->flags.indicator_on = 0;
	}

	menu->entries[vscr->workspace.current + MC_WORKSPACE1]->flags.indicator_on = 1;
}

void workspaces_set_menu_enabled_items(virtual_screen *vscr, WMenu *menu)
{
	/* don't let user destroy current workspace */
	if (vscr->workspace.current == vscr->workspace.count - 1)
		menu_entry_set_enabled(menu, MC_DESTROY_LAST, False);
	else
		menu_entry_set_enabled(menu, MC_DESTROY_LAST, True);

	/* back to last workspace */
	if (vscr->workspace.count && vscr->workspace.last_used != vscr->workspace.current)
		menu_entry_set_enabled(menu, MC_LAST_USED, True);
	else
		menu_entry_set_enabled(menu, MC_LAST_USED, False);

	menu_entry_set_enabled_paint(menu, MC_DESTROY_LAST);
	menu_entry_set_enabled_paint(menu, MC_LAST_USED);
}

void wWorkspaceMenuUpdate_map(virtual_screen *vscr)
{
	WMenu *menu = vscr->workspace.menu;
	int tmp;

	if (!menu)
		return;

	wMenuRealize(menu);
	workspaces_set_menu_enabled_items(vscr, menu);

	tmp = menu->frame->top_width + 5;
	/* if menu got unreachable, bring it to a visible place */
	if (menu->frame_x < tmp - (int) menu->frame->width)
		wMenuMove(menu, tmp - (int) menu->frame->width, menu->frame_y, False);

	wMenuPaint(menu);
}

void wWorkspaceMenuUpdate(virtual_screen *vscr, WMenu *menu)
{
	if (!menu)
		return;

	if (menu->entry_no < vscr->workspace.count + MC_WORKSPACE1)
		menu_workspace_addwks(vscr, menu);
	else if (menu->entry_no > vscr->workspace.count + MC_WORKSPACE1)
		menu_workspace_delwks(vscr, menu);

	menu_workspace_shortcut_labels(vscr, menu);
}

void wWorkspaceSaveState(virtual_screen *vscr, WMPropList *old_state)
{
	WMPropList *parr, *pstr, *wks_state, *old_wks_state, *foo, *bar;
	int i;

	make_keys();

	old_wks_state = WMGetFromPLDictionary(old_state, dWorkspaces);
	parr = WMCreatePLArray(NULL);
	for (i = 0; i < vscr->workspace.count; i++) {
		pstr = WMCreatePLString(vscr->workspace.array[i]->name);
		wks_state = WMCreatePLDictionary(dName, pstr, NULL);
		WMReleasePropList(pstr);
		if (!wPreferences.flags.noclip) {
			pstr = wClipSaveWorkspaceState(vscr, i);
			WMPutInPLDictionary(wks_state, dClip, pstr);
			WMReleasePropList(pstr);
		} else if (old_wks_state != NULL) {
			foo = WMGetFromPLArray(old_wks_state, i);
			if (foo != NULL) {
				bar = WMGetFromPLDictionary(foo, dClip);
				if (bar != NULL)
					WMPutInPLDictionary(wks_state, dClip, bar);
			}
		}

		WMAddToPLArray(parr, wks_state);
		WMReleasePropList(wks_state);
	}

	WMPutInPLDictionary(w_global.session_state, dWorkspaces, parr);
	WMReleasePropList(parr);
}

int set_clip_omnipresent(virtual_screen *vscr, int wksno)
{
	int sts, j, added_omnipresent_icons = 0;

	for (j = 0; j < vscr->workspace.array[wksno]->clip->max_icons; j++) {
		WAppIcon *aicon = vscr->workspace.array[wksno]->clip->icon_array[j];
		int k;

		if (!aicon || !aicon->omnipresent)
			continue;

		aicon->omnipresent = 0;

		sts = wClipMakeIconOmnipresent(aicon, True);

		/* kix TODO Remove it
		 * We don't need paint yet, we wil paint all icons later
		if (sts == WO_FAILED || sts == WO_SUCCESS)
			wAppIconPaint(aicon);
		 */

		if (sts != WO_SUCCESS)
			continue;

		if (wksno == 0)
			continue;

		/* Move this appicon from workspace i to workspace 0 */
		vscr->workspace.array[wksno]->clip->icon_array[j] = NULL;
		vscr->workspace.array[wksno]->clip->icon_count--;
		added_omnipresent_icons++;

		/* If there are too many omnipresent appicons, we are in trouble */
		assert(vscr->workspace.array[0]->clip->icon_count + added_omnipresent_icons
		       <= vscr->workspace.array[0]->clip->max_icons);

		/* Find first free spot on workspace 0 */
		for (k = 0; k < vscr->workspace.array[0]->clip->max_icons; k++)
			if (vscr->workspace.array[0]->clip->icon_array[k] == NULL)
				break;

		vscr->workspace.array[0]->clip->icon_array[k] = aicon;
		aicon->dock = vscr->workspace.array[0]->clip;
	}

	return added_omnipresent_icons;
}

void workspaces_restore(virtual_screen *vscr)
{
	WMPropList *parr;
	int wksno;

	make_keys();

	if (w_global.session_state == NULL)
		return;

	parr = WMGetFromPLDictionary(w_global.session_state, dWorkspaces);
	if (!parr)
		return;

	for (wksno = 0; wksno < WMIN(WMGetPropListItemCount(parr), MAX_WORKSPACES); wksno++)
		workspace_create_with_state(vscr, wksno, parr);
}

void workspaces_restore_map(virtual_screen *vscr)
{
	WMPropList *parr;
	int wksno;

	make_keys();

	if (w_global.session_state == NULL)
		return;

	parr = WMGetFromPLDictionary(w_global.session_state, dWorkspaces);
	if (!parr)
		return;

	for (wksno = 0; wksno < vscr->workspace.count; wksno++)
		workspace_map(vscr, vscr->workspace.array[wksno], wksno, parr);
}

/* Returns the workspace number for a given workspace name */
int wGetWorkspaceNumber(virtual_screen *vscr, const char *value)
{
        int w, i;

	if (sscanf(value, "%i", &w) != 1) {
		w = -1;
		for (i = 0; i < vscr->workspace.count; i++) {
			if (strcmp(vscr->workspace.array[i]->name, value) == 0) {
				w = i;
				break;
			}
		}
	} else {
		w--;
	}

	return w;
}
