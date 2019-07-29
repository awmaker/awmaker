/* dock.h- built-in Dock module for WindowMaker
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *  Copyright (c) 1998-2003 Dan Pascu
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

#ifndef WMDOCKCORE_H_
#define WMDOCKCORE_H_

#include "appicon.h"

typedef struct WDock {
    virtual_screen *vscr;	/* pointer to the virtual_screen for the dock */
    int x_pos, y_pos;		/* position of the first icon */

    WAppIcon **icon_array;	/* array of docked icons */
    int max_icons;

    int icon_count;

#define WM_DOCK        0
#define WM_CLIP        1
#define WM_DRAWER      2
    int type;

    WMagicNumber auto_expand_magic;
    WMagicNumber auto_collapse_magic;
    WMagicNumber auto_raise_magic;
    WMagicNumber auto_lower_magic;
    unsigned int auto_collapse:1;      /* if clip auto-collapse itself */
    unsigned int auto_raise_lower:1;   /* if clip should raise/lower when
                                        * entered/leaved */
    unsigned int on_right_side:1;
    unsigned int collapsed:1;
    unsigned int mapped:1;
    unsigned int lowered:1;
    unsigned int attract_icons:1;      /* If clip should attract app-icons */

    unsigned int lclip_button_pushed:1;
    unsigned int rclip_button_pushed:1;

    struct WMenu *menu;

    struct WDDomain *defaults;
} WDock;



void wDockHideIcons(WDock *dock);
void wDockShowIcons(WDock *dock);
void wDockLower(WDock *dock);
void wDockRaise(WDock *dock);
void wDockRaiseLower(WDock *dock);
void wDockSaveState(virtual_screen *vscr, WMPropList *old_state);

Bool wDockAttachIcon(WDock *dock, WAppIcon *icon, int x, int y, Bool update_icon);
Bool wDockSnapIcon(WDock *dock, WAppIcon *icon, int req_x, int req_y,
                   int *ret_x, int *ret_y, int redocking);
Bool wDockFindFreeSlot(WDock *dock, int *req_x, int *req_y);
void wDockDetach(WDock *dock, WAppIcon *icon);
Bool wDockMoveIconBetweenDocks(WDock *src, WDock *dest, WAppIcon *icon, int x, int y);
void wDockReattachIcon(WDock *dock, WAppIcon *icon, int x, int y);

void wSlideAppicons(WAppIcon **appicons, int n, int to_the_left);

void wDockFinishLaunch(WAppIcon *icon);
void wDockTrackWindowLaunch(WDock *dock, Window window);
WAppIcon *wDockFindIconForWindow(WDock *dock, Window window);
void wDockLaunchWithState(WAppIcon *btn, WSavedState *state);

void dockedapps_autolaunch(int vscrno);

#ifdef USE_DOCK_XDND
int wDockReceiveDNDDrop(virtual_screen *vscr, XEvent *event);
#endif

#define WO_FAILED          0
#define WO_NOT_APPLICABLE  1
#define WO_SUCCESS         2

typedef enum
{
	P_NORMAL = 0,
	P_AUTO_RAISE_LOWER,
	P_KEEP_ON_TOP,
} dockPosition;

void restore_state_lowered(WDock *dock, WMPropList *state);
void restore_state_collapsed(WDock *dock, WMPropList *state);
void restore_state_autoraise(WDock *dock, WMPropList *state);
int restore_state_autocollapsed(WDock *dock, WMPropList *state);
int restore_state_autoattracticons(WDock *dock, WMPropList *state);

void handleDockMove(WDock *dock, WAppIcon *aicon, XEvent *event);
void launchDockedApplication(WAppIcon *btn, Bool withSelection);
void selectCallback(WMenu *menu, WMenuEntry *entry);
void selectIconsCallback(WMenu *menu, WMenuEntry *entry);
void keepIconsCallback(WMenu *menu, WMenuEntry *entry);
void attractIconsCallback(WMenu *menu, WMenuEntry *entry);
void launchCallback(WMenu *menu, WMenuEntry *entry);
void dockHideHereCallback(WMenu *menu, WMenuEntry *entry);
void dockUnhideHereCallback(WMenu *menu, WMenuEntry *entry);
void toggleLoweredCallback(WMenu *menu, WMenuEntry *entry);
void toggleCollapsedCallback(WMenu *menu, WMenuEntry *entry);
void toggleAutoCollapseCallback(WMenu *menu, WMenuEntry *entry);
void settingsCallback(WMenu *menu, WMenuEntry *entry);
void toggleAutoRaiseLowerCallback(WMenu *menu, WMenuEntry *entry);
void toggleAutoAttractCallback(WMenu *menu, WMenuEntry *entry);
void dockHideCallback(WMenu *menu, WMenuEntry *entry);
void dockKillCallback(WMenu *menu, WMenuEntry *entry);
void dockUpdateOptionsMenu(WDock *dock, WMenu *menu);

void removeIcons(WMArray *icons, WDock *dock);
void toggleLowered(WDock *dock);
void toggleCollapsed(WDock *dock);
int numberOfSelectedIcons(WDock *dock);
WMArray *getSelected(WDock *dock);

WDock *dock_create_core(virtual_screen *vscr);
void set_attacheddocks_map(WDock *dock);
void set_attacheddocks_unmap(WDock *dock);
Bool getBooleanDockValue(WMPropList *value, WMPropList *key);
WMPropList *make_icon_state(WAppIcon *btn);
void wDockDoAutoLaunch(WDock *dock, int workspace);
WMPropList *get_application_list(WMPropList *dock_state, virtual_screen *vscr);
WMPropList *get_applications_string(virtual_screen *vscr);

int onScreen(virtual_screen *vscr, int x, int y);
void dock_unset_attacheddocks(WDock *dock);

#define CLIP_REWIND       1
#define CLIP_IDLE         0
#define CLIP_FORWARD      2

#define ICON_SIZE wPreferences.icon_size

#endif
