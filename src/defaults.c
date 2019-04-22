/* defaults.c - manage configuration through defaults db
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *  Copyright (c) 1998-2003 Dan Pascu
 *  Copyright (c) 2014 Window Maker Team
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <signal.h>

#ifndef PATH_MAX
#define PATH_MAX DEFAULT_PATH_MAX
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <wraster.h>

#include "WindowMaker.h"
#include "framewin.h"
#include "window.h"
#include "texture.h"
#include "screen.h"
#include "resources.h"
#include "defaults.h"
#include "keybind.h"
#include "xmodifier.h"
#include "icon.h"
#include "shell.h"
#include "actions.h"
#include "dock.h"
#include "workspace.h"
#include "properties.h"
#include "misc.h"
#include "winmenu.h"

typedef struct _WDefaultEntry  WDefaultEntry;
typedef int (WDECallbackConvert) (virtual_screen *vscr, WDefaultEntry *entry, WMPropList *plvalue, void *addr, void **tdata);
typedef int (WDECallbackUpdate) (virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data);

struct _WDefaultEntry {
	const char *key;
	const char *default_value;
	void *extra_data;
	void *addr;
	WDECallbackConvert *convert;
	WDECallbackUpdate *update;
	WMPropList *plkey;
	WMPropList *plvalue;	/* default value */
};

/* used to map strings to integers */
typedef struct {
	const char *string;
	short value;
	char is_alias;
} WOptionEnumeration;

/* type converters */
static WDECallbackConvert getBool;
static WDECallbackConvert getInt;
static WDECallbackConvert getCoord;
static WDECallbackConvert getPathList;
static WDECallbackConvert getEnum;
static WDECallbackConvert getTexture;
static WDECallbackConvert getWSBackground;
static WDECallbackConvert getWSSpecificBackground;
static WDECallbackConvert getFont;
static WDECallbackConvert getColor;
static WDECallbackConvert getKeybind;
static WDECallbackConvert getModMask;
static WDECallbackConvert getPropList;
static WDECallbackConvert getCursor;

/* value setting functions */
static WDECallbackUpdate setJustify;
static WDECallbackUpdate setClearance;
static WDECallbackUpdate setIfDockPresent;
static WDECallbackUpdate setClipMergedInDock;
static WDECallbackUpdate setWrapAppiconsInDock;
static WDECallbackUpdate setStickyIcons;
static WDECallbackUpdate setWidgetColor;
static WDECallbackUpdate setIconTile;
static WDECallbackUpdate setWinTitleFont;
static WDECallbackUpdate setMenuTitleFont;
static WDECallbackUpdate setMenuTextFont;
static WDECallbackUpdate setIconTitleFont;
static WDECallbackUpdate setIconTitleColor;
static WDECallbackUpdate setIconTitleBack;
static WDECallbackUpdate setFrameBorderWidth;
static WDECallbackUpdate setFrameBorderColor;
static WDECallbackUpdate setFrameFocusedBorderColor;
static WDECallbackUpdate setFrameSelectedBorderColor;
static WDECallbackUpdate setLargeDisplayFont;
static WDECallbackUpdate setWTitleColorFocused;
static WDECallbackUpdate setWTitleColorOwner;
static WDECallbackUpdate setWTitleColorUnfocused;
static WDECallbackUpdate setFTitleBack;
static WDECallbackUpdate setPTitleBack;
static WDECallbackUpdate setUTitleBack;
static WDECallbackUpdate setResizebarBack;
static WDECallbackUpdate setWorkspaceBack;
static WDECallbackUpdate setWorkspaceSpecificBack;
static WDECallbackUpdate setMenuTitleColor;
static WDECallbackUpdate setMenuTextColor;
static WDECallbackUpdate setMenuDisabledColor;
static WDECallbackUpdate setMenuTitleBack;
static WDECallbackUpdate setMenuTextBack;
static WDECallbackUpdate setHightlight;
static WDECallbackUpdate setHightlightText;
static WDECallbackUpdate setKeyGrab;
static WDECallbackUpdate setDoubleClick;
static WDECallbackUpdate setIconPosition;
static WDECallbackUpdate setWorkspaceMapBackground;
static WDECallbackUpdate setClipTitleFont;
static WDECallbackUpdate setClipTitleColor;
static WDECallbackUpdate setClipTitleColorCollapsed;
static WDECallbackUpdate setMenuStyle;
static WDECallbackUpdate setSwPOptions;
static WDECallbackUpdate updateUsableArea;
static WDECallbackUpdate setModifierKeyLabels;
static WDECallbackUpdate setCursor;

/*
 * Tables to convert strings to enumeration values.
 * Values stored are char
 */

/* WARNING: sum of length of all value strings must not exceed
 * this value */
#define TOTAL_VALUES_LENGTH	80

#define REFRESH_WINDOW_TEXTURES	(1<<0)
#define REFRESH_MENU_TEXTURE	(1<<1)
#define REFRESH_MENU_FONT	(1<<2)
#define REFRESH_MENU_COLOR	(1<<3)
#define REFRESH_MENU_TITLE_TEXTURE	(1<<4)
#define REFRESH_MENU_TITLE_FONT	(1<<5)
#define REFRESH_MENU_TITLE_COLOR	(1<<6)
#define REFRESH_WINDOW_TITLE_COLOR (1<<7)
#define REFRESH_WINDOW_FONT	(1<<8)
#define REFRESH_ICON_TILE	(1<<9)
#define REFRESH_ICON_FONT	(1<<10)

#define REFRESH_BUTTON_IMAGES   (1<<11)

#define REFRESH_ICON_TITLE_COLOR (1<<12)
#define REFRESH_ICON_TITLE_BACK (1<<13)

#define REFRESH_WORKSPACE_MENU	(1<<14)
#define REFRESH_USABLE_AREA	(1<<15)
#define REFRESH_ARRANGE_ICONS	(1<<16)
#define REFRESH_STICKY_ICONS	(1<<17)

#define REFRESH_FRAME_BORDER REFRESH_MENU_FONT|REFRESH_WINDOW_FONT

#define NUM2STRING_(x) #x
#define NUM2STRING(x) NUM2STRING_(x)

static WOptionEnumeration seFocusModes[] = {
	{"Manual", WKF_CLICK, 0}, {"ClickToFocus", WKF_CLICK, 1},
	{"Sloppy", WKF_SLOPPY, 0}, {"SemiAuto", WKF_SLOPPY, 1}, {"Auto", WKF_SLOPPY, 1},
	{NULL, 0, 0}
};

static WOptionEnumeration seTitlebarModes[] = {
	{"new", TS_NEW, 0}, {"old", TS_OLD, 0},
	{"next", TS_NEXT, 0}, {NULL, 0, 0}
};

static WOptionEnumeration seColormapModes[] = {
	{"Manual", WCM_CLICK, 0}, {"ClickToFocus", WCM_CLICK, 1},
	{"Auto", WCM_POINTER, 0}, {"FocusFollowMouse", WCM_POINTER, 1},
	{NULL, 0, 0}
};

static WOptionEnumeration sePlacements[] = {
	{"Auto", WPM_AUTO, 0},
	{"Smart", WPM_SMART, 0},
	{"Cascade", WPM_CASCADE, 0},
	{"Random", WPM_RANDOM, 0},
	{"Manual", WPM_MANUAL, 0},
	{"Center", WPM_CENTER, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seGeomDisplays[] = {
	{"None", WDIS_NONE, 0},
	{"Center", WDIS_CENTER, 0},
	{"Corner", WDIS_TOPLEFT, 0},
	{"Floating", WDIS_FRAME_CENTER, 0},
	{"Line", WDIS_NEW, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seSpeeds[] = {
	{"UltraFast", SPEED_ULTRAFAST, 0},
	{"Fast", SPEED_FAST, 0},
	{"Medium", SPEED_MEDIUM, 0},
	{"Slow", SPEED_SLOW, 0},
	{"UltraSlow", SPEED_ULTRASLOW, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seMouseButtonActions[] = {
	{"None", WA_NONE, 0},
	{"SelectWindows", WA_SELECT_WINDOWS, 0},
	{"OpenApplicationsMenu", WA_OPEN_APPMENU, 0},
	{"OpenWindowListMenu", WA_OPEN_WINLISTMENU, 0},
	{"MoveToPrevWorkspace", WA_MOVE_PREVWORKSPACE, 0},
	{"MoveToNextWorkspace", WA_MOVE_NEXTWORKSPACE, 0},
	{"MoveToPrevWindow", WA_MOVE_PREVWINDOW, 0},
	{"MoveToNextWindow", WA_MOVE_NEXTWINDOW, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seMouseWheelActions[] = {
	{"None", WA_NONE, 0},
	{"SwitchWorkspaces", WA_SWITCH_WORKSPACES, 0},
	{"SwitchWindows", WA_SWITCH_WINDOWS, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seIconificationStyles[] = {
	{"Zoom", WIS_ZOOM, 0},
	{"Twist", WIS_TWIST, 0},
	{"Flip", WIS_FLIP, 0},
	{"None", WIS_NONE, 0},
	{"random", WIS_RANDOM, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seJustifications[] = {
	{"Left", WTJ_LEFT, 0},
	{"Center", WTJ_CENTER, 0},
	{"Right", WTJ_RIGHT, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seIconPositions[] = {
	{"blv", IY_BOTTOM | IY_LEFT | IY_VERT, 0},
	{"blh", IY_BOTTOM | IY_LEFT | IY_HORIZ, 0},
	{"brv", IY_BOTTOM | IY_RIGHT | IY_VERT, 0},
	{"brh", IY_BOTTOM | IY_RIGHT | IY_HORIZ, 0},
	{"tlv", IY_TOP | IY_LEFT | IY_VERT, 0},
	{"tlh", IY_TOP | IY_LEFT | IY_HORIZ, 0},
	{"trv", IY_TOP | IY_RIGHT | IY_VERT, 0},
	{"trh", IY_TOP | IY_RIGHT | IY_HORIZ, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seMenuStyles[] = {
	{"normal", MS_NORMAL, 0},
	{"singletexture", MS_SINGLE_TEXTURE, 0},
	{"flat", MS_FLAT, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seDisplayPositions[] = {
	{"none", WD_NONE, 0},
	{"center", WD_CENTER, 0},
	{"top", WD_TOP, 0},
	{"bottom", WD_BOTTOM, 0},
	{"topleft", WD_TOPLEFT, 0},
	{"topright", WD_TOPRIGHT, 0},
	{"bottomleft", WD_BOTTOMLEFT, 0},
	{"bottomright", WD_BOTTOMRIGHT, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seWorkspaceBorder[] = {
	{"None", WB_NONE, 0},
	{"LeftRight", WB_LEFTRIGHT, 0},
	{"TopBottom", WB_TOPBOTTOM, 0},
	{"AllDirections", WB_ALLDIRS, 0},
	{NULL, 0, 0}
};

static WOptionEnumeration seDragMaximizedWindow[] = {
	{"Move", DRAGMAX_MOVE, 0},
	{"RestoreGeometry", DRAGMAX_RESTORE, 0},
	{"Unmaximize", DRAGMAX_UNMAXIMIZE, 0},
	{"NoMove", DRAGMAX_NOMOVE, 0},
	{NULL, 0, 0}
};

/*
 * ALL entries in the tables below NEED to have a default value
 * defined, and this value needs to be correct.
 *
 * Also add the default key/value pair to WindowMaker/Defaults/WindowMaker.in
 */

/* these options will only affect the window manager on startup
 *
 * static defaults can't access the screen data, because it is
 * created after these defaults are read
 */
WDefaultEntry staticOptionList[] = {

	{"ColormapSize", "4", NULL,
	    &wPreferences.cmap_size, getInt, NULL, NULL, NULL},
	{"DisableDithering", "NO", NULL,
	    &wPreferences.no_dithering, getBool, NULL, NULL, NULL},
	{"IconSize", "64", NULL,
	    &wPreferences.icon_size, getInt, NULL, NULL, NULL},
	{"ModifierKey", "Mod1", NULL,
	    &wPreferences.modifier_mask, getModMask, NULL, NULL, NULL},
	{"FocusMode", "manual", seFocusModes,				/* have a problem when switching from */
	    &wPreferences.focus_mode, getEnum, NULL, NULL, NULL},	/* manual to sloppy without restart */
	{"NewStyle", "new", seTitlebarModes,
	    &wPreferences.new_style, getEnum, NULL, NULL, NULL},
	{"DisableDock", "NO", (void *)WM_DOCK,
	    NULL, getBool, setIfDockPresent, NULL, NULL},
	{"DisableClip", "NO", (void *)WM_CLIP,
	    NULL, getBool, setIfDockPresent, NULL, NULL},
	{"DisableDrawers", "NO", (void *)WM_DRAWER,
	    NULL, getBool, setIfDockPresent, NULL, NULL},
	{"ClipMergedInDock", "NO", NULL,
	    NULL, getBool, setClipMergedInDock, NULL, NULL},
	{"DisableMiniwindows", "NO", NULL,
	    &wPreferences.disable_miniwindows, getBool, NULL, NULL, NULL},
	{"EnableWorkspacePager", "NO", NULL,
	    &wPreferences.enable_workspace_pager, getBool, NULL, NULL, NULL}
};

WDefaultEntry noscreenOptionList[] = {
	{"PixmapPath", DEF_PIXMAP_PATHS, NULL,
	    &wPreferences.pixmap_path, getPathList, NULL, NULL, NULL},
	{"IconPath", DEF_ICON_PATHS, NULL,
	    &wPreferences.icon_path, getPathList, NULL, NULL, NULL},
	{"IconificationStyle", "Zoom", seIconificationStyles,
	    &wPreferences.iconification_style, getEnum, NULL, NULL, NULL},
	{"DisableWSMouseActions", "NO", NULL,
	    &wPreferences.disable_root_mouse, getBool, NULL, NULL, NULL},
	{"MouseLeftButtonAction", "SelectWindows", seMouseButtonActions,
	    &wPreferences.mouse_button1, getEnum, NULL, NULL, NULL},
	{"MouseMiddleButtonAction", "OpenWindowListMenu", seMouseButtonActions,
	    &wPreferences.mouse_button2, getEnum, NULL, NULL, NULL},
	{"MouseRightButtonAction", "OpenApplicationsMenu", seMouseButtonActions,
	    &wPreferences.mouse_button3, getEnum, NULL, NULL, NULL},
	{"MouseBackwardButtonAction", "None", seMouseButtonActions,
	    &wPreferences.mouse_button8, getEnum, NULL, NULL, NULL},
	{"MouseForwardButtonAction", "None", seMouseButtonActions,
	    &wPreferences.mouse_button9, getEnum, NULL, NULL, NULL},
	{"MouseWheelAction", "None", seMouseWheelActions,
	    &wPreferences.mouse_wheel_scroll, getEnum, NULL, NULL, NULL},
	{"MouseWheelTiltAction", "None", seMouseWheelActions,
	    &wPreferences.mouse_wheel_tilt, getEnum, NULL, NULL, NULL},
	{"ColormapMode", "auto", seColormapModes,
	    &wPreferences.colormap_mode, getEnum, NULL, NULL, NULL},
	{"AutoFocus", "YES", NULL,
	    &wPreferences.auto_focus, getBool, NULL, NULL, NULL},
	{"RaiseDelay", "0", NULL,
	    &wPreferences.raise_delay, getInt, NULL, NULL, NULL},
	{"CirculateRaise", "NO", NULL,
	    &wPreferences.circ_raise, getBool, NULL, NULL, NULL},
	{"Superfluous", "YES", NULL,
	    &wPreferences.superfluous, getBool, NULL, NULL, NULL},
	{"AdvanceToNewWorkspace", "NO", NULL,
	    &wPreferences.ws_advance, getBool, NULL, NULL, NULL},
	{"CycleWorkspaces", "NO", NULL,
	    &wPreferences.ws_cycle, getBool, NULL, NULL, NULL},
	{"WorkspaceNameDisplayPosition", "center", seDisplayPositions,
	    &wPreferences.workspace_name_display_position, getEnum, NULL, NULL, NULL},
	{"SaveSessionOnExit", "NO", NULL,
	    &wPreferences.save_session_on_exit, getBool, NULL, NULL, NULL},
	{"WrapMenus", "NO", NULL,
	    &wPreferences.wrap_menus, getBool, NULL, NULL, NULL},
	{"ScrollableMenus", "YES", NULL,
	    &wPreferences.scrollable_menus, getBool, NULL, NULL, NULL},
	{"MenuScrollSpeed", "fast", seSpeeds,
	    &wPreferences.menu_scroll_speed, getEnum, NULL, NULL, NULL},
	{"IconSlideSpeed", "fast", seSpeeds,
	    &wPreferences.icon_slide_speed, getEnum, NULL, NULL, NULL},
	{"ShadeSpeed", "fast", seSpeeds,
	    &wPreferences.shade_speed, getEnum, NULL, NULL, NULL},
	{"BounceAppIconsWhenUrgent", "YES", NULL,
	    &wPreferences.bounce_appicons_when_urgent, getBool, NULL, NULL, NULL},
	{"RaiseAppIconsWhenBouncing", "NO", NULL,
	    &wPreferences.raise_appicons_when_bouncing, getBool, NULL, NULL, NULL},
	{"DoNotMakeAppIconsBounce", "NO", NULL,
	    &wPreferences.do_not_make_appicons_bounce, getBool, NULL, NULL, NULL},
	{"DoubleClickTime", "250", (void *) &wPreferences.dblclick_time,
	    &wPreferences.dblclick_time, getInt, setDoubleClick, NULL, NULL},
	{"ClipAutoraiseDelay", "600", NULL,
	     &wPreferences.clip_auto_raise_delay, getInt, NULL, NULL, NULL},
	{"ClipAutolowerDelay", "1000", NULL,
	    &wPreferences.clip_auto_lower_delay, getInt, NULL, NULL, NULL},
	{"ClipAutoexpandDelay", "600", NULL,
	    &wPreferences.clip_auto_expand_delay, getInt, NULL, NULL, NULL},
	{"ClipAutocollapseDelay", "1000", NULL,
	    &wPreferences.clip_auto_collapse_delay, getInt, NULL, NULL, NULL},
	{"WrapAppiconsInDock", "YES", NULL,
	    NULL, getBool, setWrapAppiconsInDock, NULL, NULL},
	{"AlignSubmenus", "NO", NULL,
	    &wPreferences.align_menus, getBool, NULL, NULL, NULL},
	{"ViKeyMenus", "NO", NULL,
	    &wPreferences.vi_key_menus, getBool, NULL, NULL, NULL},
	{"OpenTransientOnOwnerWorkspace", "NO", NULL,
	    &wPreferences.open_transients_with_parent, getBool, NULL, NULL, NULL},
	{"WindowPlacement", "auto", sePlacements,
	    &wPreferences.window_placement, getEnum, NULL, NULL, NULL},
	{"IgnoreFocusClick", "NO", NULL,
	    &wPreferences.ignore_focus_click, getBool, NULL, NULL, NULL},
	{"UseSaveUnders", "NO", NULL,
	    &wPreferences.use_saveunders, getBool, NULL, NULL, NULL},
	{"OpaqueMove", "YES", NULL,
	    &wPreferences.opaque_move, getBool, NULL, NULL, NULL},
	{"OpaqueResize", "NO", NULL,
	    &wPreferences.opaque_resize, getBool, NULL, NULL, NULL},
	{"OpaqueMoveResizeKeyboard", "NO", NULL,
	    &wPreferences.opaque_move_resize_keyboard, getBool, NULL, NULL, NULL},
	{"DisableAnimations", "NO", NULL,
	    &wPreferences.no_animations, getBool, NULL, NULL, NULL},
	{"DontLinkWorkspaces", "YES", NULL,
	    &wPreferences.no_autowrap, getBool, NULL, NULL, NULL},
	{"WindowSnapping", "NO", NULL,
	    &wPreferences.window_snapping, getBool, NULL, NULL, NULL},
	{"SnapEdgeDetect", "1", NULL,
	    &wPreferences.snap_edge_detect, getInt, NULL, NULL, NULL},
	{"SnapCornerDetect", "10", NULL,
	    &wPreferences.snap_corner_detect, getInt, NULL, NULL, NULL},
	{"SnapToTopMaximizesFullscreen", "NO", NULL,
	    &wPreferences.snap_to_top_maximizes_fullscreen, getBool, NULL, NULL, NULL},
	{"DragMaximizedWindow", "Move", seDragMaximizedWindow,
	    &wPreferences.drag_maximized_window, getEnum, NULL, NULL, NULL},
	{"MoveHalfMaximizedWindowsBetweenScreens", "NO", NULL,
	    &wPreferences.move_half_max_between_heads, getBool, NULL, NULL, NULL},
	{"AlternativeHalfMaximized", "NO", NULL,
	    &wPreferences.alt_half_maximize, getBool, NULL, NULL, NULL},
	{"PointerWithHalfMaxWindows", "NO", NULL,
	    &wPreferences.pointer_with_half_max_windows, getBool, NULL, NULL, NULL},
	{"HighlightActiveApp", "YES", NULL,
	    &wPreferences.highlight_active_app, getBool, NULL, NULL, NULL},
	{"AutoArrangeIcons", "NO", NULL,
	    &wPreferences.auto_arrange_icons, getBool, NULL, NULL, NULL},
	{"WindowPlaceOrigin", "(64, 0)", NULL,
	    &wPreferences.window_place_origin, getCoord, NULL, NULL, NULL},
	{"ResizeDisplay", "center", seGeomDisplays,
	    &wPreferences.size_display, getEnum, NULL, NULL, NULL},
	{"MoveDisplay", "floating", seGeomDisplays,
	    &wPreferences.move_display, getEnum, NULL, NULL, NULL},
	{"DontConfirmKill", "NO", NULL,
	    &wPreferences.dont_confirm_kill, getBool, NULL, NULL, NULL},
	{"WindowTitleBalloons", "YES", NULL,
	    &wPreferences.window_balloon, getBool, NULL, NULL, NULL},
	{"MiniwindowTitleBalloons", "NO", NULL,
	    &wPreferences.miniwin_title_balloon, getBool, NULL, NULL, NULL},
	{"MiniwindowPreviewBalloons", "NO", NULL,
	    &wPreferences.miniwin_preview_balloon, getBool, NULL, NULL, NULL},
	{"AppIconBalloons", "NO", NULL,
	    &wPreferences.appicon_balloon, getBool, NULL, NULL, NULL},
	{"HelpBalloons", "NO", NULL,
	    &wPreferences.help_balloon, getBool, NULL, NULL, NULL},
	{"EdgeResistance", "30", NULL,
	    &wPreferences.edge_resistance, getInt, NULL, NULL, NULL},
	{"ResizeIncrement", "0", NULL,
	    &wPreferences.resize_increment, getInt, NULL, NULL, NULL},
	{"Attraction", "NO", NULL,
	    &wPreferences.attract, getBool, NULL, NULL, NULL},
	{"DisableBlinking", "NO", NULL,
	    &wPreferences.dont_blink, getBool, NULL, NULL, NULL},
	{"SingleClickLaunch",	"NO",	NULL,
	    &wPreferences.single_click, getBool, NULL, NULL, NULL},
	{"StrictWindozeCycle",	"YES",	NULL,
	    &wPreferences.strict_windoze_cycle, getBool, NULL, NULL, NULL},
	{"SwitchPanelOnlyOpen",	"NO",	NULL,
	    &wPreferences.panel_only_open, getBool, NULL, NULL, NULL},
	{"MiniPreviewSize", "128", NULL,
	    &wPreferences.minipreview_size, getInt, NULL, NULL, NULL},
	{"IgnoreGtkHints", "NO", NULL,
	    &wPreferences.ignore_gtk_decoration_hints, getBool, NULL, NULL, NULL},

	/* style options */
	{"SmoothWorkspaceBack", "NO", NULL,
	    NULL, getBool, NULL, NULL, NULL},
	{"TitleJustify", "center", seJustifications,
	    &wPreferences.title_justification, getEnum, setJustify, NULL, NULL},
	{"WindowTitleExtendSpace", DEF_WINDOW_TITLE_EXTEND_SPACE, NULL,
	    &wPreferences.window_title_clearance, getInt, setClearance, NULL, NULL},
	{"WindowTitleMinHeight", "0", NULL,
	    &wPreferences.window_title_min_height, getInt, setClearance, NULL, NULL},
	{"WindowTitleMaxHeight", NUM2STRING(INT_MAX), NULL,
	    &wPreferences.window_title_max_height, getInt, setClearance, NULL, NULL},
	{"MenuTitleExtendSpace", DEF_MENU_TITLE_EXTEND_SPACE, NULL,
	    &wPreferences.menu_title_clearance, getInt, setClearance, NULL, NULL},
	{"MenuTitleMinHeight", "0", NULL,
	    &wPreferences.menu_title_min_height, getInt, setClearance, NULL, NULL},
	{"MenuTitleMaxHeight", NUM2STRING(INT_MAX), NULL,
	    &wPreferences.menu_title_max_height, getInt, setClearance, NULL, NULL},
	{"MenuTextExtendSpace", DEF_MENU_TEXT_EXTEND_SPACE, NULL,
	    &wPreferences.menu_text_clearance, getInt, setClearance, NULL, NULL},
	{"ShowClipTitle", "YES", NULL,
	    &wPreferences.show_clip_title, getBool, NULL, NULL, NULL},
	{"DialogHistoryLines", "500", NULL,
	    &wPreferences.history_lines, getInt, NULL, NULL, NULL},
	{"CycleActiveHeadOnly", "NO", NULL,
	    &wPreferences.cycle_active_head_only, getBool, NULL, NULL, NULL},
	{"CycleIgnoreMinimized", "NO", NULL,
	    &wPreferences.cycle_ignore_minimized, getBool, NULL, NULL, NULL}
};

WDefaultEntry optionList[] = {

	/* dynamic options */

	{"NoWindowOverDock", "NO", NULL,
	    &wPreferences.no_window_over_dock, getBool, updateUsableArea, NULL, NULL}, /* - */
	{"NoWindowOverIcons", "NO", NULL,
	    &wPreferences.no_window_over_icons, getBool, updateUsableArea, NULL, NULL}, /* - */
	{"IconPosition", "blh", seIconPositions,
	    &wPreferences.icon_yard, getEnum, setIconPosition, NULL, NULL}, /* - */
	{"WorkspaceBorder", "None", seWorkspaceBorder,
	    &wPreferences.workspace_border_position, getEnum, updateUsableArea, NULL, NULL}, /* - */
	{"WorkspaceBorderSize", "0", NULL,
	    &wPreferences.workspace_border_size, getInt, updateUsableArea, NULL, NULL}, /* - */
	{"StickyIcons", "NO", NULL,
	    &wPreferences.sticky_icons, getBool, setStickyIcons, NULL, NULL}, /* - */

	/* style options */

	{"MenuStyle", "normal", seMenuStyles,
	    &wPreferences.menu_style, getEnum, setMenuStyle, NULL, NULL}, /* - */
	{"WidgetColor", "(solid, gray)", NULL,
	    &wPreferences.texture.widgetcolor, getTexture, setWidgetColor, NULL, NULL},
	{"WorkspaceSpecificBack", "()", NULL,
	    NULL, getWSSpecificBackground, setWorkspaceSpecificBack, NULL, NULL},
	/* WorkspaceBack must come after WorkspaceSpecificBack or
	 * WorkspaceBack won't know WorkspaceSpecificBack was also
	 * specified and 2 copies of wmsetbg will be launched */
	{"WorkspaceBack", "(solid, \"rgb:50/50/75\")", NULL,
	    NULL, getWSBackground, setWorkspaceBack, NULL, NULL},
	{"IconBack", "(dgradient, \"rgb:a6/a6/b6\", \"rgb:51/55/61\")", NULL,
	    &wPreferences.texture.iconback, getTexture, setIconTile, NULL, NULL},
	{"WindowTitleFont", DEF_TITLE_FONT, NULL,
	    &wPreferences.font.wintitle, getFont, setWinTitleFont, NULL, NULL}, /* - */
	{"MenuTitleFont", DEF_MENU_TITLE_FONT, NULL,
	    &wPreferences.font.menutitle, getFont, setMenuTitleFont, NULL, NULL}, /* - */
	{"MenuTextFont", DEF_MENU_ENTRY_FONT, NULL,
	    &wPreferences.font.menutext, getFont, setMenuTextFont, NULL, NULL}, /* - */
	{"IconTitleFont", DEF_ICON_TITLE_FONT, NULL,
	    &wPreferences.font.icontitle, getFont, setIconTitleFont, NULL, NULL}, /* - */
	{"ClipTitleFont", DEF_CLIP_TITLE_FONT, NULL,
	    &wPreferences.font.cliptitle, getFont, setClipTitleFont, NULL, NULL}, /* - */
	{"LargeDisplayFont", DEF_WORKSPACE_NAME_FONT, NULL,
	    &wPreferences.font.largedisplay, getFont, setLargeDisplayFont, NULL, NULL}, /* - */
	{"HighlightColor", "white", NULL,
	    &wPreferences.color.highlight, getColor, setHightlight, NULL, NULL},
	{"HighlightTextColor", "black", NULL,
	    &wPreferences.color.highlighttext, getColor, setHightlightText, NULL, NULL},
	{"ClipTitleColor", "black", NULL,
	    &wPreferences.color.cliptitle, getColor, setClipTitleColor, NULL, NULL},
	{"CClipTitleColor", "\"rgb:61/61/61\"", NULL,
	    &wPreferences.color.cliptitlecollapsed, getColor, setClipTitleColorCollapsed, NULL, NULL},
	{"FTitleColor", "white", NULL,
	    &wPreferences.color.titlefocused, getColor, setWTitleColorFocused, NULL, NULL},
	{"PTitleColor", "white", NULL,
	    &wPreferences.color.titleowner, getColor, setWTitleColorOwner, NULL, NULL},
	{"UTitleColor", "black", NULL,
	    &wPreferences.color.titleunfocused, getColor, setWTitleColorUnfocused, NULL, NULL},
	{"FTitleBack", "(solid, black)", NULL,
	    &wPreferences.texture.titlebackfocused, getTexture, setFTitleBack, NULL, NULL},
	{"PTitleBack", "(solid, gray40)", NULL,
	    &wPreferences.texture.titlebackowner, getTexture, setPTitleBack, NULL, NULL},
	{"UTitleBack", "(solid, \"rgb:aa/aa/aa\")", NULL,
	    &wPreferences.texture.titlebackunfocused, getTexture, setUTitleBack, NULL, NULL},
	{"ResizebarBack", "(solid, \"rgb:aa/aa/aa\")", NULL,
	    &wPreferences.texture.resizebarback, getTexture, setResizebarBack, NULL, NULL},
	{"MenuTitleColor", "white", NULL,
	    &wPreferences.color.menutitle, getColor, setMenuTitleColor, NULL, NULL},
	{"MenuTextColor", "black", NULL,
	    &wPreferences.color.menutext, getColor, setMenuTextColor, NULL, NULL},
	{"MenuDisabledColor", "gray50", NULL,
	    &wPreferences.color.menudisabled, getColor, setMenuDisabledColor, NULL, NULL},
	{"MenuTitleBack", "(solid, black)", NULL,
	    &wPreferences.texture.menutitleback, getTexture, setMenuTitleBack, NULL, NULL},
	{"MenuTextBack", "(solid, \"rgb:aa/aa/aa\")", NULL,
	    &wPreferences.texture.menutextback, getTexture, setMenuTextBack, NULL, NULL},
	{"IconTitleColor", "white", NULL,
	    &wPreferences.color.icontitle, getColor, setIconTitleColor, NULL, NULL},
	{"IconTitleBack", "black", NULL,
	    &wPreferences.color.icontitleback, getColor, setIconTitleBack, NULL, NULL},
	{"SwitchPanelImages", "(swtile.png, swback.png, 30, 40)", NULL,
	    NULL, getPropList, setSwPOptions, NULL, NULL},
	{"ModifierKeyLabels", "(\"Shift+\", \"Control+\", \"Mod1+\", \"Mod2+\", \"Mod3+\", \"Mod4+\", \"Mod5+\")", NULL,
	    NULL, getPropList, setModifierKeyLabels, NULL, NULL},
	{"FrameBorderWidth", "1", NULL,
	    NULL, getInt, setFrameBorderWidth, NULL, NULL}, /* - */
	{"FrameBorderColor", "black", NULL,
	    &wPreferences.color.frameborder, getColor, setFrameBorderColor, NULL, NULL},
	{"FrameFocusedBorderColor", "black", NULL,
	    &wPreferences.color.frameborderfocused, getColor, setFrameFocusedBorderColor, NULL, NULL},
	{"FrameSelectedBorderColor", "white", NULL,
	    &wPreferences.color.frameborderselected, getColor, setFrameSelectedBorderColor, NULL, NULL},
	{"WorkspaceMapBack", "(solid, black)", NULL,
	    &wPreferences.texture.workspacemapback, getTexture, setWorkspaceMapBackground, NULL, NULL},

	/* keybindings */

	{"RootMenuKey", "F12", (void *)WKBD_ROOTMENU,
	    &wPreferences.key.rootmenu, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowListKey", "F11", (void *)WKBD_WINDOWLIST,
	    &wPreferences.key.windowlist, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowMenuKey", "Control+Escape", (void *)WKBD_WINDOWMENU,
	    &wPreferences.key.windowmenu, getKeybind, setKeyGrab, NULL, NULL},
	{"DockRaiseLowerKey", "None", (void *)WKBD_DOCKRAISELOWER,
	    &wPreferences.key.dockraiselower, getKeybind, setKeyGrab, NULL, NULL},
	{"ClipRaiseLowerKey", "None", (void *)WKBD_CLIPRAISELOWER,
	    &wPreferences.key.clipraiselower, getKeybind, setKeyGrab, NULL, NULL},
	{"MiniaturizeKey", "Mod1+M", (void *)WKBD_MINIATURIZE,
	    &wPreferences.key.miniaturize, getKeybind, setKeyGrab, NULL, NULL},
	{"MinimizeAllKey", "None", (void *)WKBD_MINIMIZEALL,
	    &wPreferences.key.minimizeall, getKeybind, setKeyGrab, NULL, NULL},
	{"HideKey", "Mod1+H", (void *)WKBD_HIDE,
	    &wPreferences.key.hide, getKeybind, setKeyGrab, NULL, NULL},
	{"HideOthersKey", "None", (void *)WKBD_HIDE_OTHERS,
	    &wPreferences.key.hideothers, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveResizeKey", "None", (void *)WKBD_MOVERESIZE,
	    &wPreferences.key.moveresize, getKeybind, setKeyGrab, NULL, NULL},
	{"CloseKey", "None", (void *)WKBD_CLOSE,
	    &wPreferences.key.close, getKeybind, setKeyGrab, NULL, NULL},
	{"MaximizeKey", "None", (void *)WKBD_MAXIMIZE,
	    &wPreferences.key.maximize, getKeybind, setKeyGrab, NULL, NULL},
	{"VMaximizeKey", "None", (void *)WKBD_VMAXIMIZE,
	    &wPreferences.key.maximizev, getKeybind, setKeyGrab, NULL, NULL},
	{"HMaximizeKey", "None", (void *)WKBD_HMAXIMIZE,
	    &wPreferences.key.maximizeh, getKeybind, setKeyGrab, NULL, NULL},
	{"LHMaximizeKey", "None", (void *)WKBD_LHMAXIMIZE,
	    &wPreferences.key.maximizelh, getKeybind, setKeyGrab, NULL, NULL},
	{"RHMaximizeKey", "None", (void *)WKBD_RHMAXIMIZE,
	    &wPreferences.key.maximizerh, getKeybind, setKeyGrab, NULL, NULL},
	{"THMaximizeKey", "None", (void *)WKBD_THMAXIMIZE,
	    &wPreferences.key.maximizeth, getKeybind, setKeyGrab, NULL, NULL},
	{"BHMaximizeKey", "None", (void *)WKBD_BHMAXIMIZE,
	    &wPreferences.key.maximizebh, getKeybind, setKeyGrab, NULL, NULL},
	{"LTCMaximizeKey", "None", (void *)WKBD_LTCMAXIMIZE,
	    &wPreferences.key.maximizeltc, getKeybind, setKeyGrab, NULL, NULL},
	{"RTCMaximizeKey", "None", (void *)WKBD_RTCMAXIMIZE,
	    &wPreferences.key.maximizertc, getKeybind, setKeyGrab, NULL, NULL},
	{"LBCMaximizeKey", "None", (void *)WKBD_LBCMAXIMIZE,
	    &wPreferences.key.maximizelbc, getKeybind, setKeyGrab, NULL, NULL},
	{"RBCMaximizeKey", "None", (void *)WKBD_RBCMAXIMIZE,
	    &wPreferences.key.maximizerbc, getKeybind, setKeyGrab, NULL, NULL},
	{"MaximusKey", "None", (void *)WKBD_MAXIMUS,
	    &wPreferences.key.maximus, getKeybind, setKeyGrab, NULL, NULL},
	{"KeepOnTopKey", "None", (void *)WKBD_KEEP_ON_TOP,
	    &wPreferences.key.keepontop, getKeybind, setKeyGrab, NULL, NULL},
	{"KeepAtBottomKey", "None", (void *)WKBD_KEEP_AT_BOTTOM,
	    &wPreferences.key.keepatbottom, getKeybind, setKeyGrab, NULL, NULL},
	{"OmnipresentKey", "None", (void *)WKBD_OMNIPRESENT,
	    &wPreferences.key.omnipresent, getKeybind, setKeyGrab, NULL, NULL},
	{"RaiseKey", "Mod1+Up", (void *)WKBD_RAISE,
	    &wPreferences.key.raise, getKeybind, setKeyGrab, NULL, NULL},
	{"LowerKey", "Mod1+Down", (void *)WKBD_LOWER,
	    &wPreferences.key.lower, getKeybind, setKeyGrab, NULL, NULL},
	{"RaiseLowerKey", "None", (void *)WKBD_RAISELOWER,
	    &wPreferences.key.raiselower, getKeybind, setKeyGrab, NULL, NULL},
	{"ShadeKey", "None", (void *)WKBD_SHADE,
	    &wPreferences.key.shade, getKeybind, setKeyGrab, NULL, NULL},
	{"SelectKey", "None", (void *)WKBD_SELECT,
	    &wPreferences.key.select, getKeybind, setKeyGrab, NULL, NULL},
	{"WorkspaceMapKey", "None", (void *)WKBD_WORKSPACEMAP,
	    &wPreferences.key.workspacemap, getKeybind, setKeyGrab, NULL, NULL},
	{"FocusNextKey", "Mod1+Tab", (void *)WKBD_FOCUSNEXT,
	    &wPreferences.key.focusnext, getKeybind, setKeyGrab, NULL, NULL},
	{"FocusPrevKey", "Mod1+Shift+Tab", (void *)WKBD_FOCUSPREV,
	    &wPreferences.key.focusprev, getKeybind, setKeyGrab, NULL, NULL},
	{"GroupNextKey", "None", (void *)WKBD_GROUPNEXT,
	    &wPreferences.key.groupnext, getKeybind, setKeyGrab, NULL, NULL},
	{"GroupPrevKey", "None", (void *)WKBD_GROUPPREV,
	    &wPreferences.key.groupprev, getKeybind, setKeyGrab, NULL, NULL},
	{"NextWorkspaceKey", "Mod1+Control+Right", (void *)WKBD_NEXTWORKSPACE,
	    &wPreferences.key.workspacenext, getKeybind, setKeyGrab, NULL, NULL},
	{"PrevWorkspaceKey", "Mod1+Control+Left", (void *)WKBD_PREVWORKSPACE,
	    &wPreferences.key.workspaceprev, getKeybind, setKeyGrab, NULL, NULL},
	{"LastWorkspaceKey", "None", (void *)WKBD_LASTWORKSPACE,
	    &wPreferences.key.workspacelast, getKeybind, setKeyGrab, NULL, NULL},
	{"NextWorkspaceLayerKey", "None", (void *)WKBD_NEXTWSLAYER,
	    &wPreferences.key.workspacelayernext, getKeybind, setKeyGrab, NULL, NULL},
	{"PrevWorkspaceLayerKey", "None", (void *)WKBD_PREVWSLAYER,
	    &wPreferences.key.workspacelayerprev, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace1Key", "Mod1+1", (void *)WKBD_WORKSPACE1,
	    &wPreferences.key.workspace1, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace2Key", "Mod1+2", (void *)WKBD_WORKSPACE2,
	    &wPreferences.key.workspace2, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace3Key", "Mod1+3", (void *)WKBD_WORKSPACE3,
	    &wPreferences.key.workspace3, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace4Key", "Mod1+4", (void *)WKBD_WORKSPACE4,
	    &wPreferences.key.workspace4, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace5Key", "Mod1+5", (void *)WKBD_WORKSPACE5,
	    &wPreferences.key.workspace5, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace6Key", "Mod1+6", (void *)WKBD_WORKSPACE6,
	    &wPreferences.key.workspace6, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace7Key", "Mod1+7", (void *)WKBD_WORKSPACE7,
	    &wPreferences.key.workspace7, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace8Key", "Mod1+8", (void *)WKBD_WORKSPACE8,
	    &wPreferences.key.workspace8, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace9Key", "Mod1+9", (void *)WKBD_WORKSPACE9,
	    &wPreferences.key.workspace9, getKeybind, setKeyGrab, NULL, NULL},
	{"Workspace10Key", "Mod1+0", (void *)WKBD_WORKSPACE10,
	    &wPreferences.key.workspace10, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace1Key", "None", (void *)WKBD_MOVE_WORKSPACE1,
	    &wPreferences.key.movetoworkspace1, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace2Key", "None", (void *)WKBD_MOVE_WORKSPACE2,
	    &wPreferences.key.movetoworkspace2, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace3Key", "None", (void *)WKBD_MOVE_WORKSPACE3,
	    &wPreferences.key.movetoworkspace3, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace4Key", "None", (void *)WKBD_MOVE_WORKSPACE4,
	    &wPreferences.key.movetoworkspace4, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace5Key", "None", (void *)WKBD_MOVE_WORKSPACE5,
	    &wPreferences.key.movetoworkspace5, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace6Key", "None", (void *)WKBD_MOVE_WORKSPACE6,
	    &wPreferences.key.movetoworkspace6, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace7Key", "None", (void *)WKBD_MOVE_WORKSPACE7,
	    &wPreferences.key.movetoworkspace7, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace8Key", "None", (void *)WKBD_MOVE_WORKSPACE8,
	    &wPreferences.key.movetoworkspace8, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace9Key", "None", (void *)WKBD_MOVE_WORKSPACE9,
	    &wPreferences.key.movetoworkspace9, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToWorkspace10Key", "None", (void *)WKBD_MOVE_WORKSPACE10,
	    &wPreferences.key.movetoworkspace10, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToNextWorkspaceKey", "None", (void *)WKBD_MOVE_NEXTWORKSPACE,
	    &wPreferences.key.movetonextworkspace, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToPrevWorkspaceKey", "None", (void *)WKBD_MOVE_PREVWORKSPACE,
	    &wPreferences.key.movetoprevworkspace, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToLastWorkspaceKey", "None", (void *)WKBD_MOVE_LASTWORKSPACE,
	    &wPreferences.key.movetolastworkspace, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToNextWorkspaceLayerKey", "None", (void *)WKBD_MOVE_NEXTWSLAYER,
	    &wPreferences.key.movetonextworkspace, getKeybind, setKeyGrab, NULL, NULL},
	{"MoveToPrevWorkspaceLayerKey", "None", (void *)WKBD_MOVE_PREVWSLAYER,
	    &wPreferences.key.movetoprevworkspace, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut1Key", "None", (void *)WKBD_WINDOW1,
	    &wPreferences.key.windowshortcut1, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut2Key", "None", (void *)WKBD_WINDOW2,
	    &wPreferences.key.windowshortcut2, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut3Key", "None", (void *)WKBD_WINDOW3,
	    &wPreferences.key.windowshortcut3, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut4Key", "None", (void *)WKBD_WINDOW4,
	    &wPreferences.key.windowshortcut4, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut5Key", "None", (void *)WKBD_WINDOW5,
	    &wPreferences.key.windowshortcut5, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut6Key", "None", (void *)WKBD_WINDOW6,
	    &wPreferences.key.windowshortcut6, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut7Key", "None", (void *)WKBD_WINDOW7,
	    &wPreferences.key.windowshortcut7, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut8Key", "None", (void *)WKBD_WINDOW8,
	    &wPreferences.key.windowshortcut8, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut9Key", "None", (void *)WKBD_WINDOW9,
	    &wPreferences.key.windowshortcut9, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowShortcut10Key", "None", (void *)WKBD_WINDOW10,
	    &wPreferences.key.windowshortcut10, getKeybind, setKeyGrab, NULL, NULL},
	{"WindowRelaunchKey", "None", (void *)WKBD_RELAUNCH,
	    &wPreferences.key.windowrelaunch, getKeybind, setKeyGrab, NULL, NULL},
	{"ScreenSwitchKey", "None", (void *)WKBD_SWITCH_SCREEN,
	    &wPreferences.key.screenswitch, getKeybind, setKeyGrab, NULL, NULL},
	{"RunKey", "None", (void *)WKBD_RUN,
	    &wPreferences.key.run, getKeybind, setKeyGrab, NULL, NULL},

#ifdef KEEP_XKB_LOCK_STATUS
	{"ToggleKbdModeKey", "None", (void *)WKBD_TOGGLE,
	    &wPreferences.key.togglekbdmode, getKeybind, setKeyGrab, NULL, NULL},
	{"KbdModeLock", "NO", NULL,
	    &wPreferences.modelock, getBool, NULL, NULL, NULL}, /* - */
#endif				/* KEEP_XKB_LOCK_STATUS */

	{"NormalCursor", "(builtin, left_ptr)", (void *)WCUR_ROOT,
	    &wPreferences.cursors.root, getCursor, setCursor, NULL, NULL},
	{"ArrowCursor", "(builtin, top_left_arrow)", (void *)WCUR_ARROW,
	    &wPreferences.cursors.arrow, getCursor, setCursor, NULL, NULL},
	{"MoveCursor", "(builtin, fleur)", (void *)WCUR_MOVE,
	    &wPreferences.cursors.move, getCursor, setCursor, NULL, NULL},
	{"ResizeCursor", "(builtin, sizing)", (void *)WCUR_RESIZE,
	    &wPreferences.cursors.resize, getCursor, setCursor, NULL, NULL},
	{"TopLeftResizeCursor", "(builtin, top_left_corner)", (void *)WCUR_TOPLEFTRESIZE,
	    &wPreferences.cursors.resizetopleft, getCursor, setCursor, NULL, NULL},
	{"TopRightResizeCursor", "(builtin, top_right_corner)", (void *)WCUR_TOPRIGHTRESIZE,
	    &wPreferences.cursors.resizetopright, getCursor, setCursor, NULL, NULL},
	{"BottomLeftResizeCursor", "(builtin, bottom_left_corner)", (void *)WCUR_BOTTOMLEFTRESIZE,
	    &wPreferences.cursors.resizebottomleft, getCursor, setCursor, NULL, NULL},
	{"BottomRightResizeCursor", "(builtin, bottom_right_corner)", (void *)WCUR_BOTTOMRIGHTRESIZE,
	    &wPreferences.cursors.resizebottomright, getCursor, setCursor, NULL, NULL},
	{"VerticalResizeCursor", "(builtin, sb_v_double_arrow)", (void *)WCUR_VERTICALRESIZE,
	    &wPreferences.cursors.resizevertical, getCursor, setCursor, NULL, NULL},
	{"HorizontalResizeCursor", "(builtin, sb_h_double_arrow)", (void *)WCUR_HORIZONRESIZE,
	    &wPreferences.cursors.resizehorizontal, getCursor, setCursor, NULL, NULL},
	{"WaitCursor", "(builtin, watch)", (void *)WCUR_WAIT,
	    &wPreferences.cursors.wait, getCursor, setCursor, NULL, NULL},
	{"QuestionCursor", "(builtin, question_arrow)", (void *)WCUR_QUESTION,
	    &wPreferences.cursors.question, getCursor, setCursor, NULL, NULL},
	{"TextCursor", "(builtin, xterm)", (void *)WCUR_TEXT,
	    &wPreferences.cursors.text, getCursor, setCursor, NULL, NULL},
	{"SelectCursor", "(builtin, cross)", (void *)WCUR_SELECT,
	    &wPreferences.cursors.select, getCursor, setCursor, NULL, NULL}
};

static void init_defaults(void);
static void read_defaults_noscreen(WMPropList *new_dict);
static void wReadStaticDefaults(WMPropList *dict);
static void wDefaultsMergeGlobalMenus(WDDomain *menuDomain);
static void wDefaultUpdateIcons(virtual_screen *vscr);
static WDDomain *wDefaultsInitDomain(const char *domain, Bool requireDictionary);

void startup_set_defaults_virtual(void)
{
	/* Initialize the defaults variables */
	init_defaults();

	/* initialize defaults stuff */
	w_global.domain.wmaker = wDefaultsInitDomain("WindowMaker", True);
	if (!w_global.domain.wmaker->dictionary)
		wwarning(_("could not read domain \"%s\" from defaults database"), "WindowMaker");

	/* read defaults that don't change until a restart and are
	 * screen independent */
	wReadStaticDefaults(w_global.domain.wmaker ? w_global.domain.wmaker->dictionary : NULL);

	/* check sanity of some values */
	if (wPreferences.icon_size < 16) {
		wwarning(_("icon size is configured to %i, but it's too small. Using 16 instead"),
			 wPreferences.icon_size);

		wPreferences.icon_size = 16;
	}

	/* init other domains */
	w_global.domain.root_menu = wDefaultsInitDomain("WMRootMenu", False);
	if (!w_global.domain.root_menu->dictionary)
		wwarning(_("could not read domain \"%s\" from defaults database"), "WMRootMenu");

	wDefaultsMergeGlobalMenus(w_global.domain.root_menu);

	w_global.domain.window_attr = wDefaultsInitDomain("WMWindowAttributes", True);
	if (!w_global.domain.window_attr->dictionary)
		wwarning(_("could not read domain \"%s\" from defaults database"), "WMWindowAttributes");

	read_defaults_noscreen(w_global.domain.wmaker->dictionary);
}

/* This function sets the default values for all lists */
static void init_defaults(void)
{
	unsigned int i;
	WDefaultEntry *entry;

	WMPLSetCaseSensitive(False);

	/* Set the default values for the option list */
	for (i = 0; i < wlengthof(optionList); i++) {
		entry = &optionList[i];

		entry->plkey = WMCreatePLString(entry->key);
		if (entry->default_value)
			entry->plvalue = WMCreatePropListFromDescription(entry->default_value);
		else
			entry->plvalue = NULL;
	}

	/* Set the default values for the noscren option list */
	for (i = 0; i < wlengthof(noscreenOptionList); i++) {
		entry = &noscreenOptionList[i];

		entry->plkey = WMCreatePLString(entry->key);
		if (entry->default_value)
			entry->plvalue = WMCreatePropListFromDescription(entry->default_value);
		else
			entry->plvalue = NULL;
	}

	/* Set the default values for the static option list */
	for (i = 0; i < wlengthof(staticOptionList); i++) {
		entry = &staticOptionList[i];

		entry->plkey = WMCreatePLString(entry->key);
		if (entry->default_value)
			entry->plvalue = WMCreatePropListFromDescription(entry->default_value);
		else
			entry->plvalue = NULL;
	}
}

static WMPropList *readGlobalDomain(const char *domainName, Bool requireDictionary)
{
	WMPropList *globalDict = NULL;
	char path[PATH_MAX];
	struct stat stbuf;

	snprintf(path, sizeof(path), "%s/%s", DEFSDATADIR, domainName);
	if (stat(path, &stbuf) >= 0) {
		globalDict = WMReadPropListFromFile(path);
		if (globalDict && requireDictionary && !WMIsPLDictionary(globalDict)) {
			wwarning(_("Domain %s (%s) of global defaults database is corrupted!"), domainName, path);
			WMReleasePropList(globalDict);
			globalDict = NULL;
		} else if (!globalDict) {
			wwarning(_("could not load domain %s from global defaults database"), domainName);
		}
	}

	return globalDict;
}

#if defined(GLOBAL_PREAMBLE_MENU_FILE) || defined(GLOBAL_EPILOGUE_MENU_FILE)
static void prependMenu(WMPropList *destarr, WMPropList *array)
{
	WMPropList *item;
	int i;

	for (i = 0; i < WMGetPropListItemCount(array); i++) {
		item = WMGetFromPLArray(array, i);
		if (item)
			WMInsertInPLArray(destarr, i + 1, item);
	}
}

static void appendMenu(WMPropList *destarr, WMPropList *array)
{
	WMPropList *item;
	int i;

	for (i = 0; i < WMGetPropListItemCount(array); i++) {
		item = WMGetFromPLArray(array, i);
		if (item)
			WMAddToPLArray(destarr, item);
	}
}
#endif

static void wDefaultsMergeGlobalMenus(WDDomain *menuDomain)
{
	WMPropList *menu = menuDomain->dictionary;
	WMPropList *submenu;

	if (!menu || !WMIsPLArray(menu))
		return;

#ifdef GLOBAL_PREAMBLE_MENU_FILE
	submenu = WMReadPropListFromFile(DEFSDATADIR "/" GLOBAL_PREAMBLE_MENU_FILE);

	if (submenu && !WMIsPLArray(submenu)) {
		wwarning(_("invalid global menu file %s"), GLOBAL_PREAMBLE_MENU_FILE);
		WMReleasePropList(submenu);
		submenu = NULL;
	}
	if (submenu) {
		prependMenu(menu, submenu);
		WMReleasePropList(submenu);
	}
#endif

#ifdef GLOBAL_EPILOGUE_MENU_FILE
	submenu = WMReadPropListFromFile(DEFSDATADIR "/" GLOBAL_EPILOGUE_MENU_FILE);

	if (submenu && !WMIsPLArray(submenu)) {
		wwarning(_("invalid global menu file %s"), GLOBAL_EPILOGUE_MENU_FILE);
		WMReleasePropList(submenu);
		submenu = NULL;
	}
	if (submenu) {
		appendMenu(menu, submenu);
		WMReleasePropList(submenu);
	}
#endif

	menuDomain->dictionary = menu;
}

static WDDomain *wDefaultsInitDomain(const char *domain, Bool requireDictionary)
{
	WDDomain *db;
	struct stat stbuf;
	WMPropList *shared_dict = NULL;

	db = wmalloc(sizeof(WDDomain));
	db->domain_name = domain;
	db->path = wdefaultspathfordomain(domain);

	if (stat(db->path, &stbuf) >= 0) {
		db->dictionary = WMReadPropListFromFile(db->path);
		if (db->dictionary) {
			if (requireDictionary && !WMIsPLDictionary(db->dictionary)) {
				WMReleasePropList(db->dictionary);
				db->dictionary = NULL;
				wwarning(_("Domain %s (%s) of defaults database is corrupted!"), domain, db->path);
			}
			db->timestamp = stbuf.st_mtime;
		} else {
			wwarning(_("could not load domain %s from user defaults database"), domain);
		}
	}

	/* global system dictionary */
	shared_dict = readGlobalDomain(domain, requireDictionary);

	if (shared_dict && db->dictionary && WMIsPLDictionary(shared_dict) &&
	    WMIsPLDictionary(db->dictionary)) {
		WMMergePLDictionaries(shared_dict, db->dictionary, True);
		WMReleasePropList(db->dictionary);
		db->dictionary = shared_dict;
		if (stbuf.st_mtime > db->timestamp)
			db->timestamp = stbuf.st_mtime;
	} else if (!db->dictionary) {
		db->dictionary = shared_dict;
		if (stbuf.st_mtime > db->timestamp)
			db->timestamp = stbuf.st_mtime;
	}

	return db;
}

void wDefaultsCheckDomains(void *arg)
{
	virtual_screen *vscr;
	struct stat stbuf;
	WMPropList *shared_dict = NULL;
	WMPropList *dict;
	int i;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) arg;

	if (stat(w_global.domain.wmaker->path, &stbuf) >= 0 && w_global.domain.wmaker->timestamp < stbuf.st_mtime) {
		w_global.domain.wmaker->timestamp = stbuf.st_mtime;

		/* Global dictionary */
		shared_dict = readGlobalDomain("WindowMaker", True);

		/* User dictionary */
		dict = WMReadPropListFromFile(w_global.domain.wmaker->path);

		if (dict) {
			if (!WMIsPLDictionary(dict)) {
				WMReleasePropList(dict);
				dict = NULL;
				wwarning(_("Domain %s (%s) of defaults database is corrupted!"),
					 "WindowMaker", w_global.domain.wmaker->path);
			} else {
				if (shared_dict) {
					WMMergePLDictionaries(shared_dict, dict, True);
					WMReleasePropList(dict);
					dict = shared_dict;
					shared_dict = NULL;
				}

				for (i = 0; i < w_global.screen_count; i++) {
					vscr = wScreenWithNumber(i);
					if (vscr->screen_ptr)
						wReadDefaults(vscr, dict);
				}

				if (w_global.domain.wmaker->dictionary)
					WMReleasePropList(w_global.domain.wmaker->dictionary);

				w_global.domain.wmaker->dictionary = dict;
			}
		} else {
			wwarning(_("could not load domain %s from user defaults database"), "WindowMaker");
		}

		if (shared_dict)
			WMReleasePropList(shared_dict);

	}

	if (stat(w_global.domain.window_attr->path, &stbuf) >= 0 && w_global.domain.window_attr->timestamp < stbuf.st_mtime) {
		/* global dictionary */
		shared_dict = readGlobalDomain("WMWindowAttributes", True);
		/* user dictionary */
		dict = WMReadPropListFromFile(w_global.domain.window_attr->path);
		if (dict) {
			if (!WMIsPLDictionary(dict)) {
				WMReleasePropList(dict);
				dict = NULL;
				wwarning(_("Domain %s (%s) of defaults database is corrupted!"),
					 "WMWindowAttributes", w_global.domain.window_attr->path);
			} else {
				if (shared_dict) {
					WMMergePLDictionaries(shared_dict, dict, True);
					WMReleasePropList(dict);
					dict = shared_dict;
					shared_dict = NULL;
				}

				if (w_global.domain.window_attr->dictionary)
					WMReleasePropList(w_global.domain.window_attr->dictionary);

				w_global.domain.window_attr->dictionary = dict;
				for (i = 0; i < w_global.screen_count; i++) {
					vscr = wScreenWithNumber(i);
					if (vscr->screen_ptr) {
						wDefaultUpdateIcons(vscr);

						/* Update the panel image if changed */
						/* Don't worry. If the image is the same these
						 * functions will have no performance impact. */
						create_logo_image(vscr);
					}
				}
			}
		} else {
			wwarning(_("could not load domain %s from user defaults database"), "WMWindowAttributes");
		}

		w_global.domain.window_attr->timestamp = stbuf.st_mtime;
		if (shared_dict)
			WMReleasePropList(shared_dict);
	}

	if (stat(w_global.domain.root_menu->path, &stbuf) >= 0 && w_global.domain.root_menu->timestamp < stbuf.st_mtime) {
		dict = WMReadPropListFromFile(w_global.domain.root_menu->path);
		if (dict) {
			if (!WMIsPLArray(dict) && !WMIsPLString(dict)) {
				WMReleasePropList(dict);
				dict = NULL;
				wwarning(_("Domain %s (%s) of defaults database is corrupted!"),
					 "WMRootMenu", w_global.domain.root_menu->path);
			} else {
				if (w_global.domain.root_menu->dictionary)
					WMReleasePropList(w_global.domain.root_menu->dictionary);

				w_global.domain.root_menu->dictionary = dict;
				wDefaultsMergeGlobalMenus(w_global.domain.root_menu);
			}
		} else {
			wwarning(_("could not load domain %s from user defaults database"), "WMRootMenu");
		}
		w_global.domain.root_menu->timestamp = stbuf.st_mtime;
	}
#ifndef HAVE_INOTIFY
	if (!arg)
		WMAddTimerHandler(DEFAULTS_CHECK_INTERVAL, wDefaultsCheckDomains, arg);
#endif
}

/* This function read the static list values
 * All these values uses only the WPreferences and
 * the callbacks updates the WPreferences.
 * X11 Calls are not used in this list
 */
static void wReadStaticDefaults(WMPropList *dict)
{
	WMPropList *plvalue;
	WDefaultEntry *entry;
	unsigned int i;
	void *tdata;

	for (i = 0; i < wlengthof(staticOptionList); i++) {
		entry = &staticOptionList[i];

		if (dict)
			plvalue = WMGetFromPLDictionary(dict, entry->plkey);
		else
			plvalue = NULL;

		/* no default in the DB. Use builtin default */
		if (!plvalue)
			plvalue = entry->plvalue;

		if (plvalue) {
			/* convert data */
			(*entry->convert) (NULL, entry, plvalue, entry->addr, &tdata);
			if (entry->update)
				(*entry->update) (NULL, entry, tdata, entry->extra_data);
		}
	}
}

static void read_defaults_noscreen(WMPropList *new_dict)
{
	unsigned int i;
	WMPropList *plvalue, *old_value, *old_dict = NULL;
	WDefaultEntry *entry;
	void *tdata;

	if (w_global.domain.wmaker->dictionary != new_dict)
		old_dict = w_global.domain.wmaker->dictionary;

	for (i = 0; i < wlengthof(noscreenOptionList); i++) {
		entry = &noscreenOptionList[i];

		if (new_dict)
			plvalue = WMGetFromPLDictionary(new_dict, entry->plkey);
		else
			plvalue = NULL;

		if (!old_dict)
			old_value = NULL;
		else
			old_value = WMGetFromPLDictionary(old_dict, entry->plkey);

		if (!plvalue && !old_value) {
			/* no default in  the DB. Use builtin default */
			plvalue = entry->plvalue;
			if (plvalue && new_dict)
				WMPutInPLDictionary(new_dict, entry->plkey, plvalue);

		} else if (!plvalue) {
			/* value was deleted from DB. Keep current value */
			continue;
		} else if (!old_value) {
			/* set value for the 1st time */
		} else if (!WMIsPropListEqualTo(plvalue, old_value)) {
			/* value has changed */
		} else {
			/* Value was not changed since last time.
			 * We must continue, except if WorkspaceSpecificBack
			 * was updated previously
			 */
		}

		/* convert data */
		if (plvalue) {
			/* convert data */
			if ((*entry->convert) (NULL, entry, plvalue, entry->addr, &tdata)) {
				if (entry->update)
					(*entry->update) (NULL, entry, tdata, entry->extra_data);
			}
		}
	}
}

static unsigned int read_defaults_step1(virtual_screen *vscr, WMPropList *new_dict)
{
	unsigned int i, needs_refresh = 0;
	int update_workspace_back = 0;
	WMPropList *plvalue, *old_value, *old_dict = NULL;
	WDefaultEntry *entry;
	void *tdata;

	if (w_global.domain.wmaker->dictionary != new_dict)
		old_dict = w_global.domain.wmaker->dictionary;

	for (i = 0; i < wlengthof(optionList); i++) {
		entry = &optionList[i];

		if (new_dict)
			plvalue = WMGetFromPLDictionary(new_dict, entry->plkey);
		else
			plvalue = NULL;

		if (!old_dict)
			old_value = NULL;
		else
			old_value = WMGetFromPLDictionary(old_dict, entry->plkey);

		if (!plvalue && !old_value) {
			/* no default in  the DB. Use builtin default */
			plvalue = entry->plvalue;
			if (plvalue && new_dict)
				WMPutInPLDictionary(new_dict, entry->plkey, plvalue);

		} else if (!plvalue) {
			/* value was deleted from DB. Keep current value */
			continue;
		} else if (!old_value) {
			/* set value for the 1st time */
		} else if (!WMIsPropListEqualTo(plvalue, old_value)) {
			/* value has changed */
		} else {
			/* Value was not changed since last time.
			 * We must continue, except if WorkspaceSpecificBack
			 * was updated previously
			 */
			if (!(strcmp(entry->key, "WorkspaceBack") == 0 &&
			    update_workspace_back &&
			    vscr->screen_ptr->flags.backimage_helper_launched))
				continue;
		}

		if (plvalue) {
			/* convert data */
			if ((*entry->convert) (vscr, entry, plvalue, entry->addr, &tdata)) {
				/*
				 * If the WorkspaceSpecificBack data has been changed
				 * so that the helper will be launched now, we must be
				 * sure to send the default background texture config
				 * to the helper.
				 */
				if (strcmp(entry->key, "WorkspaceSpecificBack") == 0 &&
				    !vscr->screen_ptr->flags.backimage_helper_launched)
					update_workspace_back = 1;

				if (entry->update)
					needs_refresh |= (*entry->update) (vscr, entry, tdata, entry->extra_data);
			}
		}
	}

	return needs_refresh;
}

static void refresh_defaults(virtual_screen *vscr, unsigned int needs_refresh)
{
	int foo = 0;

	if (needs_refresh & REFRESH_MENU_TITLE_TEXTURE)
		foo |= WTextureSettings;
	if (needs_refresh & REFRESH_MENU_TITLE_FONT)
		foo |= WFontSettings;
	if (needs_refresh & REFRESH_MENU_TITLE_COLOR)
		foo |= WColorSettings;
	if (foo)
		WMPostNotificationName(WNMenuTitleAppearanceSettingsChanged, NULL,
				       (void *)(uintptr_t) foo);

	foo = 0;
	if (needs_refresh & REFRESH_MENU_TEXTURE)
		foo |= WTextureSettings;
	if (needs_refresh & REFRESH_MENU_FONT)
		foo |= WFontSettings;
	if (needs_refresh & REFRESH_MENU_COLOR)
		foo |= WColorSettings;
	if (foo)
		WMPostNotificationName(WNMenuAppearanceSettingsChanged, NULL, (void *)(uintptr_t) foo);

	foo = 0;
	if (needs_refresh & REFRESH_WINDOW_FONT)
		foo |= WFontSettings;
	if (needs_refresh & REFRESH_WINDOW_TEXTURES)
		foo |= WTextureSettings;
	if (needs_refresh & REFRESH_WINDOW_TITLE_COLOR)
		foo |= WColorSettings;
	if (foo)
		WMPostNotificationName(WNWindowAppearanceSettingsChanged, NULL, (void *)(uintptr_t) foo);

	if (!(needs_refresh & REFRESH_ICON_TILE)) {
		foo = 0;
		if (needs_refresh & REFRESH_ICON_FONT)
			foo |= WFontSettings;
		if (needs_refresh & REFRESH_ICON_TITLE_COLOR)
			foo |= WTextureSettings;
		if (needs_refresh & REFRESH_ICON_TITLE_BACK)
			foo |= WTextureSettings;
		if (foo)
			WMPostNotificationName(WNIconAppearanceSettingsChanged, NULL,
					       (void *)(uintptr_t) foo);
	}

	if (needs_refresh & REFRESH_ICON_TILE)
		WMPostNotificationName(WNIconTileSettingsChanged, NULL, NULL);

	if (needs_refresh & REFRESH_WORKSPACE_MENU) {
		if (vscr->workspace.menu) {
			wWorkspaceMenuUpdate(vscr, vscr->workspace.menu);
			wWorkspaceMenuUpdate_map(vscr);
		}
		if (vscr->workspace.submenu)
			vscr->workspace.submenu->flags.realized = 0;
	}

	if (needs_refresh & REFRESH_ARRANGE_ICONS) {
		wScreenUpdateUsableArea(vscr);
		wArrangeIcons(vscr, True);
	}

	/* Do not refresh if we already did it with the REFRESH_ARRANGE_ICONS */
	if (needs_refresh & REFRESH_USABLE_AREA && !(needs_refresh & REFRESH_ARRANGE_ICONS))
		wScreenUpdateUsableArea(vscr);

	if (needs_refresh & REFRESH_STICKY_ICONS && vscr->workspace.array) {
		wWorkspaceForceChange(vscr, vscr->workspace.current);
		wArrangeIcons(vscr, False);
	}
}

void wReadDefaults(virtual_screen *vscr, WMPropList *new_dict)
{
	unsigned int needs_refresh;

	needs_refresh = read_defaults_step1(vscr, new_dict);

	if (needs_refresh != 0 && !w_global.startup.phase1)
		refresh_defaults(vscr, needs_refresh);
}

static void wDefaultUpdateIcons(virtual_screen *vscr)
{
	WAppIcon *aicon = w_global.app_icon_list;
	WDrawerChain *dc;
	WWindow *wwin = vscr->window.focused;

	while (aicon) {
		/* Get the application icon, default included */
		wIconChangeImageFile(aicon->icon, NULL);
		wAppIconPaint(aicon);
		aicon = aicon->next;
	}

	if (!wPreferences.flags.noclip || wPreferences.flags.clip_merged_in_dock)
		wClipIconPaint(vscr->clip.icon);

	for (dc = vscr->drawer.drawers; dc != NULL; dc = dc->next)
		wDrawerIconPaint(dc->adrawer->icon_array[0]);

	while (wwin) {
		if (wwin->icon && wwin->flags.miniaturized)
			wIconChangeImageFile(wwin->icon, NULL);

		wwin = wwin->prev;
	}
}

/* --------------------------- Local ----------------------- */

#define GET_STRING_OR_DEFAULT(x, var) if (!WMIsPLString(value)) { \
    wwarning(_("Wrong option format for key \"%s\". Should be %s."), \
    entry->key, x); \
    wwarning(_("using default \"%s\" instead"), entry->default_value); \
    var = entry->default_value;\
    } else var = WMGetFromPLString(value)\


static int string2index(WMPropList *key, WMPropList *val, const char *def, WOptionEnumeration *values)
{
	char *str;
	WOptionEnumeration *v;
	char buffer[TOTAL_VALUES_LENGTH];

	if (WMIsPLString(val) && (str = WMGetFromPLString(val))) {
		for (v = values; v->string != NULL; v++) {
			if (strcasecmp(v->string, str) == 0)
				return v->value;
		}
	}

	buffer[0] = 0;
	for (v = values; v->string != NULL; v++) {
		if (!v->is_alias) {
			if (buffer[0] != 0)
				strcat(buffer, ", ");
			snprintf(buffer+strlen(buffer),
				sizeof(buffer)-strlen(buffer)-1, "\"%s\"", v->string);
		}
	}
	wwarning(_("wrong option value for key \"%s\"; got \"%s\", should be one of %s."),
		WMGetFromPLString(key),
		WMIsPLString(val) ? WMGetFromPLString(val) : "(unknown)",
		buffer);

	if (def) {
		return string2index(key, val, NULL, values);
	}

	return -1;
}

/*
 * value - is the value in the defaults DB
 * addr - is the address to store the data
 * ret - is the address to store a pointer to a temporary buffer. ret
 * 	must not be freed and is used by the set functions
 */
static int getBool(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	static char data;
	const char *val;
	int second_pass = 0;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	GET_STRING_OR_DEFAULT("Boolean", val);

 again:
	if ((val[1] == '\0' && (val[0] == 'y' || val[0] == 'Y'))
	    || strcasecmp(val, "YES") == 0) {

		data = 1;
	} else if ((val[1] == '\0' && (val[0] == 'n' || val[0] == 'N'))
		   || strcasecmp(val, "NO") == 0) {
		data = 0;
	} else {
		int i;
		if (sscanf(val, "%i", &i) == 1) {
			if (i != 0)
				data = 1;
			else
				data = 0;
		} else {
			wwarning(_("can't convert \"%s\" to boolean for key \"%s\""), val, entry->key);
			if (second_pass == 0) {
				val = WMGetFromPLString(entry->plvalue);
				second_pass = 1;
				wwarning(_("using default \"%s\" instead"), val);
				goto again;
			}
			return False;
		}
	}

	if (ret)
		*ret = &data;
	if (addr)
		*(char *)addr = data;

	return True;
}

static int getInt(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	static int data;
	const char *val;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	GET_STRING_OR_DEFAULT("Integer", val);

	if (sscanf(val, "%i", &data) != 1) {
		wwarning(_("can't convert \"%s\" to integer for key \"%s\""), val, entry->key);
		val = WMGetFromPLString(entry->plvalue);
		wwarning(_("using default \"%s\" instead"), val);
		if (sscanf(val, "%i", &data) != 1) {
			return False;
		}
	}

	if (ret)
		*ret = &data;
	if (addr)
		*(int *)addr = data;

	return True;
}

static int getCoord(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	static WCoord data;
	char *val_x, *val_y;
	int nelem, changed = 0;
	WMPropList *elem_x, *elem_y;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

 again:
	if (!WMIsPLArray(value)) {
		wwarning(_("Wrong option format for key \"%s\". Should be %s."), entry->key, "Coordinate");
		if (changed == 0) {
			value = entry->plvalue;
			changed = 1;
			wwarning(_("using default \"%s\" instead"), entry->default_value);
			goto again;
		}
		return False;
	}

	nelem = WMGetPropListItemCount(value);
	if (nelem != 2) {
		wwarning(_("Incorrect number of elements in array for key \"%s\"."), entry->key);
		if (changed == 0) {
			value = entry->plvalue;
			changed = 1;
			wwarning(_("using default \"%s\" instead"), entry->default_value);
			goto again;
		}
		return False;
	}

	elem_x = WMGetFromPLArray(value, 0);
	elem_y = WMGetFromPLArray(value, 1);

	if (!elem_x || !elem_y || !WMIsPLString(elem_x) || !WMIsPLString(elem_y)) {
		wwarning(_("Wrong value for key \"%s\". Should be Coordinate."), entry->key);
		if (changed == 0) {
			value = entry->plvalue;
			changed = 1;
			wwarning(_("using default \"%s\" instead"), entry->default_value);
			goto again;
		}
		return False;
	}

	val_x = WMGetFromPLString(elem_x);
	val_y = WMGetFromPLString(elem_y);

	if (sscanf(val_x, "%i", &data.x) != 1 || sscanf(val_y, "%i", &data.y) != 1) {
		wwarning(_("can't convert array to integers for \"%s\"."), entry->key);
		if (changed == 0) {
			value = entry->plvalue;
			changed = 1;
			wwarning(_("using default \"%s\" instead"), entry->default_value);
			goto again;
		}
		return False;
	}

	if (ret)
		*ret = &data;

	if (addr)
		*(WCoord *) addr = data;

	return True;
}

static int getPropList(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) addr;

	WMRetainPropList(value);

	*ret = value;

	return True;
}

static int getPathList(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	static char *data;
	int i, count, len;
	char *ptr;
	WMPropList *d;
	int changed = 0;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) ret;

 again:
	if (!WMIsPLArray(value)) {
		wwarning(_("Wrong option format for key \"%s\". Should be %s."), entry->key, "an array of paths");
		if (changed == 0) {
			value = entry->plvalue;
			changed = 1;
			wwarning(_("using default \"%s\" instead"), entry->default_value);
			goto again;
		}
		return False;
	}

	i = 0;
	count = WMGetPropListItemCount(value);
	if (count < 1) {
		if (changed == 0) {
			value = entry->plvalue;
			changed = 1;
			wwarning(_("using default \"%s\" instead"), entry->default_value);
			goto again;
		}
		return False;
	}

	len = 0;
	for (i = 0; i < count; i++) {
		d = WMGetFromPLArray(value, i);
		if (!d || !WMIsPLString(d)) {
			count = i;
			break;
		}
		len += strlen(WMGetFromPLString(d)) + 1;
	}

	ptr = data = wmalloc(len + 1);

	for (i = 0; i < count; i++) {
		d = WMGetFromPLArray(value, i);
		if (!d || !WMIsPLString(d)) {
			break;
		}
		strcpy(ptr, WMGetFromPLString(d));
		ptr += strlen(WMGetFromPLString(d));
		*ptr = ':';
		ptr++;
	}
	ptr--;
	*(ptr--) = 0;

	if (*(char **)addr != NULL) {
		wfree(*(char **)addr);
	}
	*(char **)addr = data;

	return True;
}

static int getEnum(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	static signed char data;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	data = string2index(entry->plkey, value, entry->default_value, (WOptionEnumeration *) entry->extra_data);
	if (data < 0)
		return False;

	if (ret)
		*ret = &data;

	if (addr)
		*(signed char *)addr = data;

	return True;
}

/*
 * (solid <color>)
 * (hgradient <color> <color>)
 * (vgradient <color> <color>)
 * (dgradient <color> <color>)
 * (mhgradient <color> <color> ...)
 * (mvgradient <color> <color> ...)
 * (mdgradient <color> <color> ...)
 * (igradient <color1> <color1> <thickness1> <color2> <color2> <thickness2>)
 * (tpixmap <file> <color>)
 * (spixmap <file> <color>)
 * (cpixmap <file> <color>)
 * (thgradient <file> <opaqueness> <color> <color>)
 * (tvgradient <file> <opaqueness> <color> <color>)
 * (tdgradient <file> <opaqueness> <color> <color>)
 * (function <lib> <function> ...)
 */

static WTexture *parse_texture(virtual_screen *vscr, WMPropList *pl)
{
	WMPropList *elem;
	char *val;
	int nelem;
	WTexture *texture = NULL;

	nelem = WMGetPropListItemCount(pl);
	if (nelem < 1)
		return NULL;

	elem = WMGetFromPLArray(pl, 0);
	if (!elem || !WMIsPLString(elem))
		return NULL;
	val = WMGetFromPLString(elem);

	if (strcasecmp(val, "solid") == 0) {
		XColor color;

		if (nelem != 2)
			return NULL;

		/* get color */

		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return NULL;
		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &color)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		texture = (WTexture *) wTextureMakeSolid(vscr, &color);
	} else if (strcasecmp(val, "dgradient") == 0
		   || strcasecmp(val, "vgradient") == 0 || strcasecmp(val, "hgradient") == 0) {
		RColor color1, color2;
		XColor xcolor;
		int type;

		if (nelem != 3) {
			wwarning(_("bad number of arguments in gradient specification"));
			return NULL;
		}

		if (val[0] == 'd' || val[0] == 'D')
			type = WTEX_DGRADIENT;
		else if (val[0] == 'h' || val[0] == 'H')
			type = WTEX_HGRADIENT;
		else
			type = WTEX_VGRADIENT;

		/* get from color */
		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return NULL;
		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		color1.alpha = 255;
		color1.red = xcolor.red >> 8;
		color1.green = xcolor.green >> 8;
		color1.blue = xcolor.blue >> 8;

		/* get to color */
		elem = WMGetFromPLArray(pl, 2);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		color2.alpha = 255;
		color2.red = xcolor.red >> 8;
		color2.green = xcolor.green >> 8;
		color2.blue = xcolor.blue >> 8;

		texture = (WTexture *) wTextureMakeGradient(vscr, type, &color1, &color2);
	} else if (strcasecmp(val, "igradient") == 0) {
		RColor colors1[2], colors2[2];
		int th1, th2;
		XColor xcolor;
		int i;

		if (nelem != 7) {
			wwarning(_("bad number of arguments in gradient specification"));
			return NULL;
		}

		/* get from color */
		for (i = 0; i < 2; i++) {
			elem = WMGetFromPLArray(pl, 1 + i);
			if (!elem || !WMIsPLString(elem))
				return NULL;
			val = WMGetFromPLString(elem);

			if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
				wwarning(_("\"%s\" is not a valid color name"), val);
				return NULL;
			}

			colors1[i].alpha = 255;
			colors1[i].red = xcolor.red >> 8;
			colors1[i].green = xcolor.green >> 8;
			colors1[i].blue = xcolor.blue >> 8;
		}

		elem = WMGetFromPLArray(pl, 3);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);
		th1 = atoi(val);

		/* get from color */
		for (i = 0; i < 2; i++) {
			elem = WMGetFromPLArray(pl, 4 + i);
			if (!elem || !WMIsPLString(elem))
				return NULL;

			val = WMGetFromPLString(elem);

			if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
				wwarning(_("\"%s\" is not a valid color name"), val);
				return NULL;
			}

			colors2[i].alpha = 255;
			colors2[i].red = xcolor.red >> 8;
			colors2[i].green = xcolor.green >> 8;
			colors2[i].blue = xcolor.blue >> 8;
		}

		elem = WMGetFromPLArray(pl, 6);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);
		th2 = atoi(val);
		texture = (WTexture *) wTextureMakeIGradient(vscr, th1, colors1, th2, colors2);
	} else if (strcasecmp(val, "mhgradient") == 0
		   || strcasecmp(val, "mvgradient") == 0 || strcasecmp(val, "mdgradient") == 0) {
		XColor color;
		RColor **colors;
		int i, count;
		int type;

		if (nelem < 3) {
			wwarning(_("too few arguments in multicolor gradient specification"));
			return NULL;
		}

		if (val[1] == 'h' || val[1] == 'H')
			type = WTEX_MHGRADIENT;
		else if (val[1] == 'v' || val[1] == 'V')
			type = WTEX_MVGRADIENT;
		else
			type = WTEX_MDGRADIENT;

		count = nelem - 1;

		colors = wmalloc(sizeof(RColor *) * (count + 1));

		for (i = 0; i < count; i++) {
			elem = WMGetFromPLArray(pl, i + 1);
			if (!elem || !WMIsPLString(elem)) {
				for (--i; i >= 0; --i)
					wfree(colors[i]);

				wfree(colors);
				return NULL;
			}

			val = WMGetFromPLString(elem);

			if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &color)) {
				wwarning(_("\"%s\" is not a valid color name"), val);
				for (--i; i >= 0; --i)
					wfree(colors[i]);

				wfree(colors);
				return NULL;
			} else {
				colors[i] = wmalloc(sizeof(RColor));
				colors[i]->red = color.red >> 8;
				colors[i]->green = color.green >> 8;
				colors[i]->blue = color.blue >> 8;
			}
		}

		colors[i] = NULL;
		texture = (WTexture *) wTextureMakeMGradient(vscr, type, colors);
	} else if (strcasecmp(val, "spixmap") == 0 ||
		   strcasecmp(val, "cpixmap") == 0 || strcasecmp(val, "tpixmap") == 0) {
		XColor color;
		int type;

		if (nelem != 3)
			return NULL;

		if (val[0] == 's' || val[0] == 'S')
			type = WTP_SCALE;
		else if (val[0] == 'c' || val[0] == 'C')
			type = WTP_CENTER;
		else
			type = WTP_TILE;

		/* get color */
		elem = WMGetFromPLArray(pl, 2);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &color)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		/* file name */
		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		texture = (WTexture *) wTextureMakePixmap(vscr, type, val, &color);
	} else if (strcasecmp(val, "thgradient") == 0
		   || strcasecmp(val, "tvgradient") == 0 || strcasecmp(val, "tdgradient") == 0) {
		RColor color1, color2;
		XColor xcolor;
		int opacity;
		int style;

		if (val[1] == 'h' || val[1] == 'H')
			style = WTEX_THGRADIENT;
		else if (val[1] == 'v' || val[1] == 'V')
			style = WTEX_TVGRADIENT;
		else
			style = WTEX_TDGRADIENT;

		if (nelem != 5) {
			wwarning(_("bad number of arguments in textured gradient specification"));
			return NULL;
		}

		/* get from color */
		elem = WMGetFromPLArray(pl, 3);
		if (!elem || !WMIsPLString(elem))
			return NULL;
		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		color1.alpha = 255;
		color1.red = xcolor.red >> 8;
		color1.green = xcolor.green >> 8;
		color1.blue = xcolor.blue >> 8;

		/* get to color */
		elem = WMGetFromPLArray(pl, 4);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		color2.alpha = 255;
		color2.red = xcolor.red >> 8;
		color2.green = xcolor.green >> 8;
		color2.blue = xcolor.blue >> 8;

		/* get opacity */
		elem = WMGetFromPLArray(pl, 2);
		if (!elem || !WMIsPLString(elem))
			opacity = 128;
		else
			val = WMGetFromPLString(elem);

		if (!val || (opacity = atoi(val)) < 0 || opacity > 255) {
			wwarning(_("bad opacity value for tgradient texture \"%s\". Should be [0..255]"), val);
			opacity = 128;
		}

		/* get file name */
		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		texture = (WTexture *) wTextureMakeTGradient(vscr, style, &color1, &color2, val, opacity);
	} else if (strcasecmp(val, "function") == 0) {
		/* Leave this in to handle the unlikely case of
		 * someone actually having function textures configured */
		wwarning("function texture support has been removed");
		return NULL;
	} else {
		wwarning(_("invalid texture type %s"), val);
		return NULL;
	}

	return texture;
}

static int getTexture(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	defstructpl *defstruct;
	WMPropList *name, *defname;
	int len;
	char *key;


	(void) vscr;

	key = NULL;
	len = sizeof(char *) * (strlen(entry->key));
	key = wmalloc(len + sizeof(char *));
	snprintf(key, len, "%s", entry->key);

	defstruct = NULL;
	defname = name = NULL;

	name = WMDeepCopyPropList(value);
	defname = WMDeepCopyPropList(entry->plvalue);

	defstruct = wmalloc(sizeof(struct defstruct));

	defstruct->key = key;
	defstruct->value = name;
	defstruct->defvalue = defname;

	/* TODO: We need free the previous memory, if used */
	if (addr)
		*(defstructpl **)addr = defstruct;

	if (ret)
		*ret = &defstruct;

	return True;
}

static int getWSBackground(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	WMPropList *elem;
	int changed = 0;
	char *val;
	int nelem;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) addr;

 again:
	if (!WMIsPLArray(value)) {
		wwarning(_("Wrong option format for key \"%s\". Should be %s."),
			 "WorkspaceBack", "Texture or None");
		if (changed == 0) {
			value = entry->plvalue;
			changed = 1;
			wwarning(_("using default \"%s\" instead"), entry->default_value);
			goto again;
		}
		return False;
	}

	/* only do basic error checking and verify for None texture */

	nelem = WMGetPropListItemCount(value);
	if (nelem > 0) {
		elem = WMGetFromPLArray(value, 0);
		if (!elem || !WMIsPLString(elem)) {
			wwarning(_("Wrong type for workspace background. Should be a texture type."));
			if (changed == 0) {
				value = entry->plvalue;
				changed = 1;
				wwarning(_("using default \"%s\" instead"), entry->default_value);
				goto again;
			}
			return False;
		}
		val = WMGetFromPLString(elem);

		if (strcasecmp(val, "None") == 0)
			return True;
	}
	*ret = WMRetainPropList(value);

	return True;
}

static int
getWSSpecificBackground(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	WMPropList *elem;
	int nelem;
	int changed = 0;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) addr;

 again:
	if (!WMIsPLArray(value)) {
		wwarning(_("Wrong option format for key \"%s\". Should be %s."),
			 "WorkspaceSpecificBack", "an array of textures");
		if (changed == 0) {
			value = entry->plvalue;
			changed = 1;
			wwarning(_("using default \"%s\" instead"), entry->default_value);
			goto again;
		}
		return False;
	}

	/* only do basic error checking and verify for None texture */

	nelem = WMGetPropListItemCount(value);
	if (nelem > 0) {
		while (nelem--) {
			elem = WMGetFromPLArray(value, nelem);
			if (!elem || !WMIsPLArray(elem)) {
				wwarning(_("Wrong type for background of workspace %i. Should be a texture."),
					 nelem);
			}
		}
	}

	*ret = WMRetainPropList(value);

#ifdef notworking
	/*
	 * Kluge to force wmsetbg helper to set the default background.
	 * If the WorkspaceSpecificBack is changed once wmaker has started,
	 * the WorkspaceBack won't be sent to the helper, unless the user
	 * changes it's value too. So, we must force this by removing the
	 * value from the defaults DB.
	 */
	if (!scr->flags.backimage_helper_launched && !scr->flags.startup) {
		WMPropList *key = WMCreatePLString("WorkspaceBack");

		WMRemoveFromPLDictionary(w_global.domain.wmaker->dictionary, key);

		WMReleasePropList(key);
	}
#endif
	return True;
}

static int getFont(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	const char *val;
	char *fontname;
	int len;

	(void) vscr;
	(void) addr;

	GET_STRING_OR_DEFAULT("Font", val);
	len = sizeof(char *) * (strlen(val));

	fontname = wmalloc(len + sizeof(char *));
	snprintf(fontname, len, "%s", val);

	if (addr)
		*(char **)addr = fontname;

	if (ret)
		*ret = &fontname;

	return True;
}

static int getColor(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	const char *val;
	int len, def_len;
	defstruct *color;
	char *colorname, *def_colorname;

	(void) vscr;
	(void) addr;

	/* Value */
	GET_STRING_OR_DEFAULT("Color", val);
	len = sizeof(char *) * (strlen(val));
	colorname = wmalloc(len + sizeof(char *));
	snprintf(colorname, len, "%s", val);

	/* Save the default value */
	val = WMGetFromPLString(entry->plvalue);
	def_len = sizeof(char *) * (strlen(val));
	def_colorname = wmalloc(def_len + sizeof(char *));
	snprintf(def_colorname, def_len, "%s", val);

	color = wmalloc(sizeof(struct defstruct));

	color->value = colorname;
	color->defvalue = def_colorname;

	/* TODO: We need free the previous memory, if used */
	if (addr)
		*(defstruct **)addr = color;

	if (ret)
		*ret = &color;

	return True;
}

static int getKeybind(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	const char *val;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) addr;

	GET_STRING_OR_DEFAULT("Key spec", val);

	if (addr)
		wstrlcpy(addr, val, MAX_SHORTCUT_LENGTH);

	if (ret)
		*ret = addr;

	return True;
}

static int getModMask(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	static int mask;
	const char *str;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	GET_STRING_OR_DEFAULT("Modifier Key", str);

	if (!str)
		return False;

	mask = wXModifierFromKey(str);
	if (mask < 0) {
		wwarning(_("%s: modifier key %s is not valid"), entry->key, str);
		mask = 0;
		return False;
	}

	if (addr)
		*(int *)addr = mask;

	if (ret)
		*ret = &mask;

	return True;
}

# include <X11/cursorfont.h>
typedef struct {
	const char *name;
	int id;
} WCursorLookup;

#define CURSOR_ID_NONE	(XC_num_glyphs)

static const WCursorLookup cursor_table[] = {
	{"X_cursor", XC_X_cursor},
	{"arrow", XC_arrow},
	{"based_arrow_down", XC_based_arrow_down},
	{"based_arrow_up", XC_based_arrow_up},
	{"boat", XC_boat},
	{"bogosity", XC_bogosity},
	{"bottom_left_corner", XC_bottom_left_corner},
	{"bottom_right_corner", XC_bottom_right_corner},
	{"bottom_side", XC_bottom_side},
	{"bottom_tee", XC_bottom_tee},
	{"box_spiral", XC_box_spiral},
	{"center_ptr", XC_center_ptr},
	{"circle", XC_circle},
	{"clock", XC_clock},
	{"coffee_mug", XC_coffee_mug},
	{"cross", XC_cross},
	{"cross_reverse", XC_cross_reverse},
	{"crosshair", XC_crosshair},
	{"diamond_cross", XC_diamond_cross},
	{"dot", XC_dot},
	{"dotbox", XC_dotbox},
	{"double_arrow", XC_double_arrow},
	{"draft_large", XC_draft_large},
	{"draft_small", XC_draft_small},
	{"draped_box", XC_draped_box},
	{"exchange", XC_exchange},
	{"fleur", XC_fleur},
	{"gobbler", XC_gobbler},
	{"gumby", XC_gumby},
	{"hand1", XC_hand1},
	{"hand2", XC_hand2},
	{"heart", XC_heart},
	{"icon", XC_icon},
	{"iron_cross", XC_iron_cross},
	{"left_ptr", XC_left_ptr},
	{"left_side", XC_left_side},
	{"left_tee", XC_left_tee},
	{"leftbutton", XC_leftbutton},
	{"ll_angle", XC_ll_angle},
	{"lr_angle", XC_lr_angle},
	{"man", XC_man},
	{"middlebutton", XC_middlebutton},
	{"mouse", XC_mouse},
	{"pencil", XC_pencil},
	{"pirate", XC_pirate},
	{"plus", XC_plus},
	{"question_arrow", XC_question_arrow},
	{"right_ptr", XC_right_ptr},
	{"right_side", XC_right_side},
	{"right_tee", XC_right_tee},
	{"rightbutton", XC_rightbutton},
	{"rtl_logo", XC_rtl_logo},
	{"sailboat", XC_sailboat},
	{"sb_down_arrow", XC_sb_down_arrow},
	{"sb_h_double_arrow", XC_sb_h_double_arrow},
	{"sb_left_arrow", XC_sb_left_arrow},
	{"sb_right_arrow", XC_sb_right_arrow},
	{"sb_up_arrow", XC_sb_up_arrow},
	{"sb_v_double_arrow", XC_sb_v_double_arrow},
	{"shuttle", XC_shuttle},
	{"sizing", XC_sizing},
	{"spider", XC_spider},
	{"spraycan", XC_spraycan},
	{"star", XC_star},
	{"target", XC_target},
	{"tcross", XC_tcross},
	{"top_left_arrow", XC_top_left_arrow},
	{"top_left_corner", XC_top_left_corner},
	{"top_right_corner", XC_top_right_corner},
	{"top_side", XC_top_side},
	{"top_tee", XC_top_tee},
	{"trek", XC_trek},
	{"ul_angle", XC_ul_angle},
	{"umbrella", XC_umbrella},
	{"ur_angle", XC_ur_angle},
	{"watch", XC_watch},
	{"xterm", XC_xterm},
	{NULL, CURSOR_ID_NONE}
};

static void check_bitmap_status(int status, const char *filename, Pixmap bitmap)
{
	switch (status) {
	case BitmapOpenFailed:
		wwarning(_("failed to open bitmap file \"%s\""), filename);
		break;
	case BitmapFileInvalid:
		wwarning(_("\"%s\" is not a valid bitmap file"), filename);
		break;
	case BitmapNoMemory:
		wwarning(_("out of memory reading bitmap file \"%s\""), filename);
		break;
	case BitmapSuccess:
		XFreePixmap(dpy, bitmap);
		break;
	}
}

/*
 * (none)
 * (builtin, <cursor_name>)
 * (bitmap, <cursor_bitmap>, <cursor_mask>)
 */
static int parse_cursor(virtual_screen *vscr, WMPropList *pl, Cursor *cursor)
{
	WMPropList *elem;
	char *val;
	int nelem;
	int status = 0;

	nelem = WMGetPropListItemCount(pl);
	if (nelem < 1)
		return (status);

	elem = WMGetFromPLArray(pl, 0);
	if (!elem || !WMIsPLString(elem))
		return (status);

	val = WMGetFromPLString(elem);

	if (strcasecmp(val, "none") == 0) {
		status = 1;
		*cursor = None;
	} else if (strcasecmp(val, "builtin") == 0) {
		int i;
		int cursor_id = CURSOR_ID_NONE;

		if (nelem != 2) {
			wwarning(_("bad number of arguments in cursor specification"));
			return (status);
		}
		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return (status);

		val = WMGetFromPLString(elem);

		for (i = 0; cursor_table[i].name != NULL; i++) {
			if (strcasecmp(val, cursor_table[i].name) == 0) {
				cursor_id = cursor_table[i].id;
				break;
			}
		}

		if (CURSOR_ID_NONE == cursor_id) {
			wwarning(_("unknown builtin cursor name \"%s\""), val);
		} else {
			*cursor = XCreateFontCursor(dpy, cursor_id);
			status = 1;
		}
	} else if (strcasecmp(val, "bitmap") == 0) {
		char *bitmap_name;
		char *mask_name;
		int bitmap_status;
		int mask_status;
		Pixmap bitmap;
		Pixmap mask;
		unsigned int w, h;
		int x, y;
		XColor fg, bg;

		if (nelem != 3) {
			wwarning(_("bad number of arguments in cursor specification"));
			return (status);
		}
		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return (status);

		val = WMGetFromPLString(elem);
		bitmap_name = FindImage(wPreferences.pixmap_path, val);
		if (!bitmap_name) {
			wwarning(_("could not find cursor bitmap file \"%s\""), val);
			return (status);
		}

		elem = WMGetFromPLArray(pl, 2);
		if (!elem || !WMIsPLString(elem)) {
			wfree(bitmap_name);
			return (status);
		}

		val = WMGetFromPLString(elem);
		mask_name = FindImage(wPreferences.pixmap_path, val);
		if (!mask_name) {
			wfree(bitmap_name);
			wwarning(_("could not find cursor bitmap file \"%s\""), val);
			return (status);
		}

		mask_status = XReadBitmapFile(dpy, vscr->screen_ptr->w_win, mask_name, &w, &h, &mask, &x, &y);
		bitmap_status = XReadBitmapFile(dpy, vscr->screen_ptr->w_win, bitmap_name, &w, &h, &bitmap, &x, &y);
		if ((BitmapSuccess == bitmap_status) && (BitmapSuccess == mask_status)) {
			fg.pixel = vscr->screen_ptr->black_pixel;
			bg.pixel = vscr->screen_ptr->white_pixel;
			XQueryColor(dpy, vscr->screen_ptr->w_colormap, &fg);
			XQueryColor(dpy, vscr->screen_ptr->w_colormap, &bg);
			*cursor = XCreatePixmapCursor(dpy, bitmap, mask, &fg, &bg, x, y);
			status = 1;
		}

		check_bitmap_status(bitmap_status, bitmap_name, bitmap);
		check_bitmap_status(mask_status, mask_name, mask);
		wfree(bitmap_name);
		wfree(mask_name);
	}

	return (status);
}

static int getCursor(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *value, void *addr, void **ret)
{
	defstructpl *defstruct;
	WMPropList *cursorname, *defcursorname;

	(void) vscr;

	defstruct = NULL;
	defcursorname = cursorname = NULL;

	if (WMIsPLArray(value)) {
		cursorname = WMDeepCopyPropList(value);
	} else {
		wwarning(_("Wrong option format for key \"%s\". Should be %s."),
			 entry->key, "cursor specification");
		wwarning(_("using default \"%s\" instead"), entry->default_value);
		if (WMIsPLArray(entry->plvalue))
			cursorname = WMDeepCopyPropList(entry->plvalue);
		else
			/* This should not happend */
			return (False);
	}

	if (WMIsPLArray(entry->plvalue))
		defcursorname = WMDeepCopyPropList(entry->plvalue);
	else
		/* If no default, use the provided... it should never happends */
		defcursorname = WMDeepCopyPropList(cursorname);

	defstruct = wmalloc(sizeof(struct defstruct));

	defstruct->value = cursorname;
	defstruct->defvalue = defcursorname;

	/* TODO: We need free the previous memory, if used */
	if (addr)
		*(defstructpl **)addr = defstruct;

	if (ret)
		*ret = &defstruct;

	return True;
}

#undef CURSOR_ID_NONE

/* ---------------- value setting functions --------------- */
static int setJustify(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) tdata;
	(void) extra_data;

	return REFRESH_WINDOW_TITLE_COLOR;
}

static int setClearance(virtual_screen *vscr, WDefaultEntry *entry, void *bar, void *foo)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) bar;
	(void) foo;

	return REFRESH_WINDOW_FONT | REFRESH_BUTTON_IMAGES | REFRESH_MENU_TITLE_FONT | REFRESH_MENU_FONT;
}

static int setIfDockPresent(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	char *flag = tdata;
	long which = (long) extra_data;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;

	switch (which) {
	case WM_DOCK:
		wPreferences.flags.nodock = wPreferences.flags.nodock || *flag;
		/* Drawers require the dock */
		wPreferences.flags.nodrawer = wPreferences.flags.nodrawer || wPreferences.flags.nodock;
		break;
	case WM_CLIP:
		wPreferences.flags.noclip = wPreferences.flags.noclip || *flag;
		break;
	case WM_DRAWER:
		wPreferences.flags.nodrawer = wPreferences.flags.nodrawer || *flag;
		break;
	default:
		break;
	}

	return 0;
}

static int setClipMergedInDock(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	char *flag = tdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) foo;

	wPreferences.flags.clip_merged_in_dock = *flag;
	wPreferences.flags.noclip = wPreferences.flags.noclip || *flag;
	return 0;
}

static int setWrapAppiconsInDock(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	char *flag = tdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) foo;

	wPreferences.flags.wrap_appicons_in_dock = *flag;
	return 0;
}

static int setStickyIcons(virtual_screen *vscr, WDefaultEntry *entry, void *bar, void *foo)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) bar;
	(void) foo;

	return REFRESH_STICKY_ICONS;
}

WTexture *get_texture_from_defstruct(virtual_screen *vscr, defstructpl *ds)
{
	char *key;
	static WTexture *texture;
	WMPropList *value, *defvalue;
	int changed = 0;

	key = ds->key;
	value = ds->value;
	defvalue = ds->defvalue;

 again:
	if (!WMIsPLArray(value)) {
		wwarning(_("Wrong option format for key \"%s\". Should be %s."), key, "Texture");
		if (changed == 0) {
			value = ds->defvalue;
			changed = 1;
			wwarning(_("using default value instead"));
			goto again;
		}
		return NULL;
	}

	if (strcmp(key, "WidgetColor") == 0 && !changed) {
		WMPropList *pl;

		pl = WMGetFromPLArray(value, 0);
		if (!pl || !WMIsPLString(pl) || !WMGetFromPLString(pl)
		    || strcasecmp(WMGetFromPLString(pl), "solid") != 0) {
			wwarning(_("Wrong option format for key \"%s\". Should be %s."),
				 key, "Solid Texture");

			value = defvalue;
			changed = 1;
			wwarning(_("using default value instead"));
			goto again;
		}
	}

	texture = parse_texture(vscr, value);

	if (!texture) {
		wwarning(_("Error in texture specification for key \"%s\""), key);
		if (changed == 0) {
			value = defvalue;
			changed = 1;
			wwarning(_("using default value instead"));
			goto again;
		}
		return NULL;
	}

	return texture;
}

static int setIconTile(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	Pixmap pixmap;
	RImage *img;
	WTexture **texture = NULL;
	int reset = 0;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.iconback);

	img = wTextureRenderImage(*texture, wPreferences.icon_size,
				  wPreferences.icon_size, ((*texture)->any.type & WREL_BORDER_MASK)
				  ? WREL_ICON : WREL_FLAT);
	if (!img) {
		wwarning(_("could not render texture for icon background"));
		if (!entry->addr)
			wTextureDestroy(vscr, *texture);

		return 0;
	}

	RConvertImage(vscr->screen_ptr->rcontext, img, &pixmap);

	if (w_global.tile.icon) {
		reset = 1;
		RReleaseImage(w_global.tile.icon);
		XFreePixmap(dpy, vscr->screen_ptr->icon_tile_pixmap);
	}

	w_global.tile.icon = img;

	/* put the icon in the noticeboard hint */
	PropSetIconTileHint(vscr, img);

	if (!wPreferences.flags.noclip || wPreferences.flags.clip_merged_in_dock) {
		if (w_global.tile.clip)
			RReleaseImage(w_global.tile.clip);

		w_global.tile.clip = wClipMakeTile(img);
	}

	if (!wPreferences.flags.nodrawer) {
		if (w_global.tile.drawer)
			RReleaseImage(w_global.tile.drawer);

		w_global.tile.drawer= wDrawerMakeTile(vscr, img);
	}

	vscr->screen_ptr->icon_tile_pixmap = pixmap;

	if (vscr->screen_ptr->def_icon_rimage) {
		RReleaseImage(vscr->screen_ptr->def_icon_rimage);
		vscr->screen_ptr->def_icon_rimage = NULL;
	}

	if (vscr->screen_ptr->icon_back_texture)
		wTextureDestroy(vscr, (WTexture *) vscr->screen_ptr->icon_back_texture);

	vscr->screen_ptr->icon_back_texture = wTextureMakeSolid(vscr, &((*texture)->any.color));

	/* Free the texture as nobody else will use it, nor refer to it.  */
	if (!entry->addr)
		wTextureDestroy(vscr, *texture);

	return (reset ? REFRESH_ICON_TILE : 0);
}

static int setWinTitleFont(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WMFont *font = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;
	int fixedlen = sizeof(char *) * 5;

	/* We must have the font loaded, but... */
	if (!wPreferences.font.wintitle) {
		wPreferences.font.wintitle = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.wintitle, fixedlen, "fixed");
	}

	font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.wintitle);
	if (!font) {
		if (wPreferences.font.wintitle)
			free(wPreferences.font.wintitle);

		wPreferences.font.wintitle = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.wintitle, fixedlen, "fixed");
		font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.wintitle);
	}

	if (!font) {
		wfatal(_("could not load any usable font!!!"));
		exit(1);
	}

	if (vscr->screen_ptr->title_font)
		WMReleaseFont(vscr->screen_ptr->title_font);

	vscr->screen_ptr->title_font = font;

	return REFRESH_WINDOW_FONT | REFRESH_BUTTON_IMAGES;
}

static int setMenuTitleFont(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WMFont *font = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;
	int fixedlen = sizeof(char *) * 5;

	/* We must have the font loaded, but... */
	if (!wPreferences.font.menutitle) {
		wPreferences.font.menutitle = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.menutitle, fixedlen, "fixed");
	}

	font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.menutitle);
	if (!font) {
		if (wPreferences.font.menutitle)
			free(wPreferences.font.menutitle);

		wPreferences.font.menutitle = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.menutitle, fixedlen, "fixed");
		font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.menutitle);
	}

	if (!font) {
		wfatal(_("could not load any usable font!!!"));
		exit(1);
	}

	if (vscr->screen_ptr->menu_title_font)
		WMReleaseFont(vscr->screen_ptr->menu_title_font);

	vscr->screen_ptr->menu_title_font = font;

	return REFRESH_MENU_TITLE_FONT;
}

static int setMenuTextFont(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WMFont *font = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;
	int fixedlen = sizeof(char *) * 5;

	/* We must have the font loaded, but... */
	if (!wPreferences.font.menutext) {
		wPreferences.font.menutext = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.menutext, fixedlen, "fixed");
	}

	font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.menutext);
	if (!font) {
		if (wPreferences.font.menutext)
			free(wPreferences.font.menutext);

		wPreferences.font.menutext = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.menutext, fixedlen, "fixed");
		font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.menutext);
	}

	if (!font) {
		wfatal(_("could not load any usable font!!!"));
		exit(1);
	}

	if (vscr->screen_ptr->menu_entry_font)
		WMReleaseFont(vscr->screen_ptr->menu_entry_font);

	vscr->screen_ptr->menu_entry_font = font;

	return REFRESH_MENU_FONT;
}

static int setIconTitleFont(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WMFont *font = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;
	int fixedlen = sizeof(char *) * 5;

	/* We must have the font loaded, but... */
	if (!wPreferences.font.icontitle) {
		wPreferences.font.icontitle = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.icontitle, fixedlen, "fixed");
	}

	font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.icontitle);
	if (!font) {
		if (wPreferences.font.icontitle)
			free(wPreferences.font.icontitle);

		wPreferences.font.icontitle = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.icontitle, fixedlen, "fixed");
		font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.icontitle);
	}

	if (!font) {
		wfatal(_("could not load any usable font!!!"));
		exit(1);
	}

	if (vscr->screen_ptr->icon_title_font)
		WMReleaseFont(vscr->screen_ptr->icon_title_font);

	vscr->screen_ptr->icon_title_font = font;

	return REFRESH_ICON_FONT;
}

static int setClipTitleFont(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WMFont *font = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;
	int fixedlen = sizeof(char *) * 5;

	/* We must have the font loaded, but... */
	if (!wPreferences.font.cliptitle) {
		wPreferences.font.cliptitle = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.cliptitle, fixedlen, "fixed");
	}

	font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.cliptitle);
	if (!font) {
		if (wPreferences.font.cliptitle)
			free(wPreferences.font.cliptitle);

		wPreferences.font.cliptitle = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.cliptitle, fixedlen, "fixed");
		font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.cliptitle);
	}

	if (!font) {
		wfatal(_("could not load any usable font!!!"));
		exit(1);
	}

	if (vscr->screen_ptr->clip_title_font)
		WMReleaseFont(vscr->screen_ptr->clip_title_font);

	vscr->screen_ptr->clip_title_font = font;

	return REFRESH_ICON_FONT;
}

static int setLargeDisplayFont(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WMFont *font = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;
	int fixedlen = sizeof(char *) * 5;

	/* We must have the font loaded, but... */
	if (!wPreferences.font.largedisplay) {
		wPreferences.font.largedisplay = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.largedisplay, fixedlen, "fixed");
	}

	font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.largedisplay);
	if (!font) {
		if (wPreferences.font.largedisplay)
			free(wPreferences.font.largedisplay);

		wPreferences.font.largedisplay = wmalloc(fixedlen + sizeof(char *));
		snprintf(wPreferences.font.largedisplay, fixedlen, "fixed");
		font = WMCreateFont(vscr->screen_ptr->wmscreen, wPreferences.font.largedisplay);
	}

	if (!font) {
		wfatal(_("could not load any usable font!!!"));
		exit(1);
	}

	if (vscr->workspace.font_for_name)
		WMReleaseFont(vscr->workspace.font_for_name);

	vscr->workspace.font_for_name = font;

	return 0;
}

static int setHightlight(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.highlight->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.highlight->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.highlight->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->select_color)
		WMReleaseColor(vscr->screen_ptr->select_color);

	vscr->screen_ptr->select_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_MENU_COLOR;
}

static int setHightlightText(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.highlighttext->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.highlighttext->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.highlighttext->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->select_text_color)
		WMReleaseColor(vscr->screen_ptr->select_text_color);

	vscr->screen_ptr->select_text_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_MENU_COLOR;
}

static int setClipTitleColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.cliptitle->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.cliptitle->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.cliptitle->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->clip_title_color[CLIP_NORMAL])
		WMReleaseColor(vscr->screen_ptr->clip_title_color[CLIP_NORMAL]);

	vscr->screen_ptr->clip_title_color[CLIP_NORMAL] = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);
	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_ICON_TITLE_COLOR;
}

static int setClipTitleColorCollapsed(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.cliptitlecollapsed->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.cliptitlecollapsed->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.cliptitlecollapsed->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->clip_title_color[CLIP_COLLAPSED])
		WMReleaseColor(vscr->screen_ptr->clip_title_color[CLIP_COLLAPSED]);

	vscr->screen_ptr->clip_title_color[CLIP_COLLAPSED] = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);
	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_ICON_TITLE_COLOR;
}

static int setWTitleColorFocused(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.titlefocused->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.titlefocused->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.titlefocused->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->window_title_color[WS_FOCUSED])
		WMReleaseColor(vscr->screen_ptr->window_title_color[WS_FOCUSED]);

	vscr->screen_ptr->window_title_color[WS_FOCUSED] =
	    WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_WINDOW_TITLE_COLOR;
}

static int setWTitleColorOwner(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.titleowner->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.titleowner->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.titleowner->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->window_title_color[WS_PFOCUSED])
		WMReleaseColor(vscr->screen_ptr->window_title_color[WS_PFOCUSED]);

	vscr->screen_ptr->window_title_color[WS_PFOCUSED] =
	    WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_WINDOW_TITLE_COLOR;
}

static int setWTitleColorUnfocused(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.titleunfocused->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.titleunfocused->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.titleunfocused->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->window_title_color[WS_UNFOCUSED])
		WMReleaseColor(vscr->screen_ptr->window_title_color[WS_UNFOCUSED]);

	vscr->screen_ptr->window_title_color[WS_UNFOCUSED] =
	    WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_WINDOW_TITLE_COLOR;
}

static int setMenuTitleColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.menutitle->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.menutitle->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.menutitle->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->menu_title_color[0])
		WMReleaseColor(vscr->screen_ptr->menu_title_color[0]);

	vscr->screen_ptr->menu_title_color[0] = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_MENU_TITLE_COLOR;
}

static int setMenuTextColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.menutext->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.menutext->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.menutext->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->mtext_color)
		WMReleaseColor(vscr->screen_ptr->mtext_color);

	vscr->screen_ptr->mtext_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	if (WMColorPixel(vscr->screen_ptr->dtext_color) == WMColorPixel(vscr->screen_ptr->mtext_color))
		WMSetColorAlpha(vscr->screen_ptr->dtext_color, 0x7fff);
	else
		WMSetColorAlpha(vscr->screen_ptr->dtext_color, 0xffff);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_MENU_COLOR;
}

static int setMenuDisabledColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.menudisabled->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.menudisabled->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.menudisabled->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->dtext_color)
		WMReleaseColor(vscr->screen_ptr->dtext_color);

	vscr->screen_ptr->dtext_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	if (WMColorPixel(vscr->screen_ptr->dtext_color) == WMColorPixel(vscr->screen_ptr->mtext_color))
		WMSetColorAlpha(vscr->screen_ptr->dtext_color, 0x7fff);
	else
		WMSetColorAlpha(vscr->screen_ptr->dtext_color, 0xffff);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_MENU_COLOR;
}

static int setIconTitleColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.icontitle->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.icontitle->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.icontitle->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->icon_title_color)
		WMReleaseColor(vscr->screen_ptr->icon_title_color);

	vscr->screen_ptr->icon_title_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_ICON_TITLE_COLOR;
}

static int setIconTitleBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.icontitleback->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.icontitleback->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.icontitleback->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->icon_title_texture)
		wTextureDestroy(vscr, (WTexture *) vscr->screen_ptr->icon_title_texture);

	vscr->screen_ptr->icon_title_texture = wTextureMakeSolid(vscr, color);

	return REFRESH_ICON_TITLE_BACK;
}

static int setFrameBorderWidth(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	int *value = tdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) extra_data;

	vscr->frame.border_width = *value;

	return REFRESH_FRAME_BORDER;
}

static int setFrameBorderColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborder->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.frameborder->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborder->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->frame_border_color)
		WMReleaseColor(vscr->screen_ptr->frame_border_color);

	vscr->screen_ptr->frame_border_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_FRAME_BORDER;
}

static int setFrameFocusedBorderColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborderfocused->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.frameborderfocused->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborderfocused->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->frame_focused_border_color)
		WMReleaseColor(vscr->screen_ptr->frame_focused_border_color);

	vscr->screen_ptr->frame_focused_border_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_FRAME_BORDER;
}

static int setFrameSelectedBorderColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	XColor clr, *color = NULL;
	color = &clr;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;
	(void) extra_data;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborderselected->value, color)) {
		wwarning(_("could not get color for key \"%s\""), entry->key);
		wwarning(_("using default \"%s\" instead"), wPreferences.color.frameborderselected->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborderselected->defvalue, color)) {
			wwarning(_("could not get color for key \"%s\""), entry->key);
			return False;
		}
	}

	if (vscr->screen_ptr->frame_selected_border_color)
		WMReleaseColor(vscr->screen_ptr->frame_selected_border_color);

	vscr->screen_ptr->frame_selected_border_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_FRAME_BORDER;
}

static int setWorkspaceSpecificBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *bar)
{
	WMPropList *value = tdata;
	WMPropList *val;
	char *str;
	int i;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) bar;

	if (vscr->screen_ptr->flags.backimage_helper_launched) {
		if (WMGetPropListItemCount(value) == 0) {
			SendHelperMessage(vscr, 'C', 0, NULL);
			SendHelperMessage(vscr, 'K', 0, NULL);

			WMReleasePropList(value);
			return 0;
		}
	} else {
		if (WMGetPropListItemCount(value) == 0)
			return 0;

		if (!start_bg_helper(vscr)) {
			WMReleasePropList(value);
			return 0;
		}

		SendHelperMessage(vscr, 'P', -1, wPreferences.pixmap_path);
	}

	for (i = 0; i < WMGetPropListItemCount(value); i++) {
		val = WMGetFromPLArray(value, i);
		if (val && WMIsPLArray(val) && WMGetPropListItemCount(val) > 0) {
			str = WMGetPropListDescription(val, False);
			SendHelperMessage(vscr, 'S', i + 1, str);
			wfree(str);
		} else {
			SendHelperMessage(vscr, 'U', i + 1, NULL);
		}
	}

	sleep(1);

	WMReleasePropList(value);
	return 0;
}

static int setWorkspaceBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *bar)
{
	WMPropList *value = tdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) bar;

	if (vscr->screen_ptr->flags.backimage_helper_launched) {
		char *str;

		if (WMGetPropListItemCount(value) == 0) {
			SendHelperMessage(vscr, 'U', 0, NULL);
		} else {
			/* set the default workspace background to this one */
			str = WMGetPropListDescription(value, False);
			if (str) {
				SendHelperMessage(vscr, 'S', 0, str);
				wfree(str);
				SendHelperMessage(vscr, 'C', vscr->workspace.current + 1, NULL);
			} else {
				SendHelperMessage(vscr, 'U', 0, NULL);
			}
		}
	} else if (WMGetPropListItemCount(value) > 0) {
		char *text;
		char *dither;
		int len;

		text = WMGetPropListDescription(value, False);
		len = strlen(text) + 40;
		dither = wPreferences.no_dithering ? "-m" : "-d";
		if (!strchr(text, '\'') && !strchr(text, '\\')) {
			char command[len];

			if (wPreferences.smooth_workspace_back)
				snprintf(command, len, "wmsetbg %s -S -p '%s' &", dither, text);
			else
				snprintf(command, len, "wmsetbg %s -p '%s' &", dither, text);

			ExecuteShellCommand(vscr, command);
		} else {
			wwarning(_("Invalid arguments for background \"%s\""), text);
		}

		wfree(text);
	}

	WMReleasePropList(value);

	return 0;
}

static int setWidgetColor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WTexture **texture = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.widgetcolor);

	if (vscr->screen_ptr->widget_texture)
		wTextureDestroy(vscr, (WTexture *) vscr->screen_ptr->widget_texture);

	vscr->screen_ptr->widget_texture = *(WTexSolid **) texture;

	return 0;
}

static int setFTitleBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WTexture **texture = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.titlebackfocused);

	if (vscr->screen_ptr->window_title_texture[WS_FOCUSED])
		wTextureDestroy(vscr, vscr->screen_ptr->window_title_texture[WS_FOCUSED]);

	vscr->screen_ptr->window_title_texture[WS_FOCUSED] = *texture;

	return REFRESH_WINDOW_TEXTURES;
}

static int setPTitleBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WTexture **texture = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.titlebackowner);

	if (vscr->screen_ptr->window_title_texture[WS_PFOCUSED])
		wTextureDestroy(vscr, vscr->screen_ptr->window_title_texture[WS_PFOCUSED]);

	vscr->screen_ptr->window_title_texture[WS_PFOCUSED] = *texture;

	return REFRESH_WINDOW_TEXTURES;
}

static int setUTitleBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WTexture **texture = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.titlebackunfocused);

	if (vscr->screen_ptr->window_title_texture[WS_UNFOCUSED])
		wTextureDestroy(vscr, vscr->screen_ptr->window_title_texture[WS_UNFOCUSED]);

	vscr->screen_ptr->window_title_texture[WS_UNFOCUSED] = *texture;

	return REFRESH_WINDOW_TEXTURES;
}

static int setResizebarBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WTexture **texture = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.resizebarback);

	if (vscr->screen_ptr->resizebar_texture[0])
		wTextureDestroy(vscr, vscr->screen_ptr->resizebar_texture[0]);

	vscr->screen_ptr->resizebar_texture[0] = *texture;

	return REFRESH_WINDOW_TEXTURES;
}

static int setMenuTitleBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WTexture **texture = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.menutitleback);

	if (vscr->screen_ptr->menu_title_texture[0])
		wTextureDestroy(vscr, vscr->screen_ptr->menu_title_texture[0]);

	vscr->screen_ptr->menu_title_texture[0] = *texture;

	return REFRESH_MENU_TITLE_TEXTURE;
}

static int setMenuTextBack(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WTexture **texture = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.menutextback);

	if (vscr->screen_ptr->menu_item_texture) {
		wTextureDestroy(vscr, vscr->screen_ptr->menu_item_texture);
		wTextureDestroy(vscr, (WTexture *) vscr->screen_ptr->menu_item_auxtexture);
	}

	vscr->screen_ptr->menu_item_texture = *texture;
	vscr->screen_ptr->menu_item_auxtexture = wTextureMakeSolid(vscr, &vscr->screen_ptr->menu_item_texture->any.color);

	return REFRESH_MENU_TEXTURE;
}

static int setKeyGrab(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	WShortKey shortcut;
	WWindow *wwin;
	char buf[MAX_SHORTCUT_LENGTH];
	long widx = (long) extra_data;
	KeySym ksym;
	char *k, *b, *value;
	int mod;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;

	/* TODO: make this as list/array... and remove this ugly if-else block */
	if (widx == WKBD_ROOTMENU) { /* "RootMenuKey" */
		value = wPreferences.key.rootmenu;
	} else if (widx == WKBD_WINDOWLIST) { /* "WindowListKey" */
		value = wPreferences.key.windowlist;
	} else if (widx == WKBD_WINDOWMENU) { /* "WindowMenuKey" */
		value = wPreferences.key.windowmenu;
	} else if (widx == WKBD_DOCKRAISELOWER) { /* "DockRaiseLowerKey" */
		value = wPreferences.key.dockraiselower;
	} else if (widx == WKBD_CLIPRAISELOWER) { /* "ClipRaiseLowerKey" */
		value = wPreferences.key.clipraiselower;
	} else if (widx == WKBD_MINIATURIZE) { /* "MiniaturizeKey" */
		value = wPreferences.key.miniaturize;
	} else if (widx == WKBD_MINIMIZEALL) { /* "MinimizeAllKey" */
		value = wPreferences.key.minimizeall;
	} else if (widx == WKBD_HIDE) { /* "HideKey" */
		value = wPreferences.key.hide;
	} else if (widx == WKBD_HIDE_OTHERS) { /* "HideOthersKey" */
		value = wPreferences.key.hideothers;
	} else if (widx == WKBD_MOVERESIZE) { /* "MoveResizeKey" */
		value = wPreferences.key.moveresize;
	} else if (widx == WKBD_CLOSE) { /* "CloseKey" */
		value = wPreferences.key.close;
	} else if (widx == WKBD_MAXIMIZE) { /* "MaximizeKey" */
		value = wPreferences.key.maximize;
	} else if (widx == WKBD_VMAXIMIZE) { /* "VMaximizeKey" */
		value = wPreferences.key.maximizev;
	} else if (widx == WKBD_HMAXIMIZE) { /* "HMaximizeKey" */
		value = wPreferences.key.maximizeh;
	} else if (widx == WKBD_LHMAXIMIZE) { /* "LHMaximizeKey" */
		value = wPreferences.key.maximizelh;
	} else if (widx == WKBD_RHMAXIMIZE) { /* "RHMaximizeKey" */
		value = wPreferences.key.maximizerh;
	} else if (widx == WKBD_THMAXIMIZE) { /* "THMaximizeKey" */
		value = wPreferences.key.maximizeth;
	} else if (widx == WKBD_BHMAXIMIZE) { /* "BHMaximizeKey" */
		value = wPreferences.key.maximizebh;
	} else if (widx == WKBD_LTCMAXIMIZE) { /* "LTCMaximizeKey" */
		value = wPreferences.key.maximizeltc;
	} else if (widx == WKBD_RTCMAXIMIZE) { /* "RTCMaximizeKey" */
		value = wPreferences.key.maximizertc;
	} else if (widx == WKBD_LBCMAXIMIZE) { /* "LBCMaximizeKey" */
		value = wPreferences.key.maximizelbc;
	} else if (widx == WKBD_RBCMAXIMIZE) { /* "RBCMaximizeKey" */
		value = wPreferences.key.maximizerbc;
	} else if (widx == WKBD_MAXIMUS) { /* "MaximusKey" */
		value = wPreferences.key.maximus;
	} else if (widx == WKBD_KEEP_ON_TOP) { /* "KeepOnTopKey" */
		value = wPreferences.key.keepontop;
	} else if (widx == WKBD_KEEP_AT_BOTTOM) { /* "KeepAtBottomKey" */
		value = wPreferences.key.keepatbottom;
	} else if (widx == WKBD_OMNIPRESENT) { /* "OmnipresentKey" */
		value = wPreferences.key.omnipresent;
	} else if (widx == WKBD_RAISE) { /* "RaiseKey" */
		value = wPreferences.key.raise;
	} else if (widx == WKBD_LOWER) { /* "LowerKey" */
		value = wPreferences.key.lower;
	} else if (widx == WKBD_RAISELOWER) { /* "RaiseLowerKey" */
		value = wPreferences.key.raiselower;
	} else if (widx == WKBD_SHADE) { /* "ShadeKey" */
		value = wPreferences.key.shade;
	} else if (widx == WKBD_SELECT) { /* "SelectKey" */
		value = wPreferences.key.select;
	} else if (widx == WKBD_WORKSPACEMAP) { /* "WorkspaceMapKey" */
		value = wPreferences.key.workspacemap;
	} else if (widx == WKBD_FOCUSNEXT) { /* "FocusNextKey" */
		value = wPreferences.key.focusnext;
	} else if (widx == WKBD_FOCUSPREV) { /* "FocusPrevKey" */
		value = wPreferences.key.focusprev;
	} else if (widx == WKBD_GROUPNEXT) { /* "GroupNextKey" */
		value = wPreferences.key.groupnext;
	} else if (widx == WKBD_GROUPPREV) { /* "GroupPrevKey" */
		value = wPreferences.key.groupprev;
	} else if (widx == WKBD_NEXTWORKSPACE) { /* "NextWorkspaceKey" */
		value = wPreferences.key.workspacenext;
	} else if (widx == WKBD_PREVWORKSPACE) { /* "PrevWorkspaceKey" */
		value = wPreferences.key.workspaceprev;
	} else if (widx == WKBD_LASTWORKSPACE) { /* "LastWorkspaceKey" */
		value = wPreferences.key.workspacelast;
	} else if (widx == WKBD_NEXTWSLAYER) { /* "NextWorkspaceLayerKey" */
		value = wPreferences.key.workspacelayernext;
	} else if (widx == WKBD_PREVWSLAYER) { /* "PrevWorkspaceLayerKey" */
		value = wPreferences.key.workspacelayerprev;
	} else if (widx == WKBD_WORKSPACE1) { /* "Workspace1Key" */
		value = wPreferences.key.workspace1;
	} else if (widx == WKBD_WORKSPACE2) { /* "Workspace2Key" */
		value = wPreferences.key.workspace2;
	} else if (widx == WKBD_WORKSPACE3) { /* "Workspace3Key" */
		value = wPreferences.key.workspace3;
	} else if (widx == WKBD_WORKSPACE4) { /* "Workspace4Key" */
		value = wPreferences.key.workspace4;
	} else if (widx == WKBD_WORKSPACE5) { /* "Workspace5Key" */
		value = wPreferences.key.workspace5;
	} else if (widx == WKBD_WORKSPACE6) { /* "Workspace6Key" */
		value = wPreferences.key.workspace6;
	} else if (widx == WKBD_WORKSPACE7) { /* "Workspace7Key" */
		value = wPreferences.key.workspace7;
	} else if (widx == WKBD_WORKSPACE8) { /* "Workspace8Key" */
		value = wPreferences.key.workspace8;
	} else if (widx == WKBD_WORKSPACE9) { /* "Workspace9Key" */
		value = wPreferences.key.workspace9;
	} else if (widx == WKBD_WORKSPACE10) { /* "Workspace10Key" */
		value = wPreferences.key.workspace10;
	} else if (widx == WKBD_MOVE_WORKSPACE1) { /* "MoveToWorkspace1Key" */
		value = wPreferences.key.movetoworkspace1;
	} else if (widx == WKBD_MOVE_WORKSPACE2) { /* "MoveToWorkspace2Key" */
		value = wPreferences.key.movetoworkspace2;
	} else if (widx == WKBD_MOVE_WORKSPACE3) { /* "MoveToWorkspace3Key" */
		value = wPreferences.key.movetoworkspace3;
	} else if (widx == WKBD_MOVE_WORKSPACE4) { /* "MoveToWorkspace4Key" */
		value = wPreferences.key.movetoworkspace4;
	} else if (widx == WKBD_MOVE_WORKSPACE5) { /* "MoveToWorkspace5Key" */
		value = wPreferences.key.movetoworkspace5;
	} else if (widx == WKBD_MOVE_WORKSPACE6) { /* "MoveToWorkspace6Key" */
		value = wPreferences.key.movetoworkspace6;
	} else if (widx == WKBD_MOVE_WORKSPACE7) { /* "MoveToWorkspace7Key" */
		value = wPreferences.key.movetoworkspace7;
	} else if (widx == WKBD_MOVE_WORKSPACE8) { /* "MoveToWorkspace8Key" */
		value = wPreferences.key.movetoworkspace8;
	} else if (widx == WKBD_MOVE_WORKSPACE9) { /* "MoveToWorkspace9Key" */
		value = wPreferences.key.movetoworkspace9;
	} else if (widx == WKBD_MOVE_WORKSPACE10) { /* "MoveToWorkspace10Key" */
		value = wPreferences.key.movetoworkspace10;
	} else if (widx == WKBD_MOVE_NEXTWORKSPACE) { /* "MoveToNextWorkspaceKey" */
		value = wPreferences.key.movetonextworkspace;
	} else if (widx == WKBD_MOVE_PREVWORKSPACE) { /* "MoveToPrevWorkspaceKey" */
		value = wPreferences.key.movetoprevworkspace;
	} else if (widx == WKBD_MOVE_LASTWORKSPACE) { /* "MoveToLastWorkspaceKey" */
		value = wPreferences.key.movetolastworkspace;
	} else if (widx == WKBD_MOVE_NEXTWSLAYER) { /* "MoveToNextWorkspaceLayerKey" */
		value = wPreferences.key.movetonextworkspace;
	} else if (widx == WKBD_MOVE_PREVWSLAYER) { /* "MoveToPrevWorkspaceLayerKey" */
		value = wPreferences.key.movetoprevworkspace;
	} else if (widx == WKBD_WINDOW1) { /* "WindowShortcut1Key" */
		value = wPreferences.key.windowshortcut1;
	} else if (widx == WKBD_WINDOW2) { /* "WindowShortcut2Key" */
		value = wPreferences.key.windowshortcut2;
	} else if (widx == WKBD_WINDOW3) { /* "WindowShortcut3Key" */
		value = wPreferences.key.windowshortcut3;
	} else if (widx == WKBD_WINDOW4) { /* "WindowShortcut4Key" */
		value = wPreferences.key.windowshortcut4;
	} else if (widx == WKBD_WINDOW5) { /* "WindowShortcut5Key" */
		value = wPreferences.key.windowshortcut5;
	} else if (widx == WKBD_WINDOW6) { /* "WindowShortcut6Key" */
		value = wPreferences.key.windowshortcut6;
	} else if (widx == WKBD_WINDOW7) { /* "WindowShortcut7Key" */
		value = wPreferences.key.windowshortcut7;
	} else if (widx == WKBD_WINDOW8) { /* "WindowShortcut8Key" */
		value = wPreferences.key.windowshortcut8;
	} else if (widx == WKBD_WINDOW9) { /* "WindowShortcut9Key" */
		value = wPreferences.key.windowshortcut9;
	} else if (widx == WKBD_WINDOW10) { /* "WindowShortcut10Key" */
		value = wPreferences.key.windowshortcut10;
	} else if (widx == WKBD_RELAUNCH) { /* "WindowRelaunchKey" */
		value = wPreferences.key.windowrelaunch;
	} else if (widx == WKBD_SWITCH_SCREEN) { /* "ScreenSwitchKey" */
		value = wPreferences.key.screenswitch;
	} else if (widx == WKBD_RUN) { /* "RunKey" */
		value = wPreferences.key.run;
#ifdef KEEP_XKB_LOCK_STATUS
	} else if (widx == WKBD_TOGGLE) { /* "ToggleKbdModeKey" */
		value = wPreferences.key.togglekbdmode;
#endif
	}

	wstrlcpy(buf, value, MAX_SHORTCUT_LENGTH);

		int error = 0;

	if ((strlen(value) == 0) || (strcasecmp(value, "NONE") == 0)) {
		shortcut.keycode = 0;
		shortcut.modifier = 0;
	} else {

		b = buf;

		/* get modifiers */
		shortcut.modifier = 0;
		while ((!error) && ((k = strchr(b, '+')) != NULL)) {
			*k = 0;
			mod = wXModifierFromKey(b);
			if (mod < 0) {
				wwarning(_("%s: invalid key modifier \"%s\""), entry->key, b);
				error = 1;
			}
			shortcut.modifier |= mod;

			b = k + 1;
		}

		if (!error) {
			/* get key */
			ksym = XStringToKeysym(b);

			if (ksym == NoSymbol) {
				wwarning(_("%s:invalid kbd shortcut specification \"%s\""), entry->key, value);
				error = 1;
			}

			if (!error) {
				shortcut.keycode = XKeysymToKeycode(dpy, ksym);
				if (shortcut.keycode == 0) {
					wwarning(_("%s:invalid key in shortcut \"%s\""), entry->key, value);
					error = 1;
				}
			}
		}
	}

	/* If no error, assign the keybind
	 * TODO: else... what?
	 * Now assign shortcut with modifier=0 key=0
	 */
	wKeyBindings[widx] = shortcut;

	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);

		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	/* do we need to update window menus? */
	if (widx >= WKBD_WORKSPACE1 && widx <= WKBD_WORKSPACE10)
		return REFRESH_WORKSPACE_MENU;
	if (widx == WKBD_LASTWORKSPACE)
		return REFRESH_WORKSPACE_MENU;
	if (widx >= WKBD_MOVE_WORKSPACE1 && widx <= WKBD_MOVE_WORKSPACE10)
		return REFRESH_WORKSPACE_MENU;

	return 0;
}

static int setIconPosition(virtual_screen *vscr, WDefaultEntry *entry, void *bar, void *foo)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) bar;
	(void) foo;

	return REFRESH_ARRANGE_ICONS;
}

static int updateUsableArea(virtual_screen *vscr, WDefaultEntry *entry, void *bar, void *foo)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) bar;
	(void) foo;

	return REFRESH_USABLE_AREA;

}

static int setWorkspaceMapBackground(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WTexture **texture = NULL;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) foo;
	(void) tdata;

	*texture = get_texture_from_defstruct(vscr, wPreferences.texture.workspacemapback);

	if (wPreferences.wsmbackTexture)
		wTextureDestroy(vscr, wPreferences.wsmbackTexture);

	wPreferences.wsmbackTexture = *texture;

	return REFRESH_WINDOW_TEXTURES;
}


static int setMenuStyle(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;
	(void) entry;
	(void) tdata;
	(void) foo;

	return REFRESH_MENU_TEXTURE;
}

static RImage *chopOffImage(RImage *image, int x, int y, int w, int h)
{
	RImage *img = RCreateImage(w, h, image->format == RRGBAFormat);

	RCopyArea(img, image, x, y, w, h, 0, 0);

	return img;
}

static int setSwPOptions(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WMPropList *array = tdata;
	char *path;
	RImage *bgimage;
	int cwidth, cheight;

	(void) foo;

	if (!WMIsPLArray(array) || WMGetPropListItemCount(array) == 0) {
		if (wPreferences.swtileImage)
			RReleaseImage(wPreferences.swtileImage);
		wPreferences.swtileImage = NULL;

		WMReleasePropList(array);
		return 0;
	}

	switch (WMGetPropListItemCount(array)) {
	case 4:
		if (!WMIsPLString(WMGetFromPLArray(array, 1))) {
			wwarning(_("Invalid arguments for option \"%s\""), entry->key);
			break;
		} else
			path = FindImage(wPreferences.pixmap_path, WMGetFromPLString(WMGetFromPLArray(array, 1)));

		if (!path) {
			wwarning(_("Could not find image \"%s\" for option \"%s\""),
				 WMGetFromPLString(WMGetFromPLArray(array, 1)), entry->key);
		} else {
			bgimage = RLoadImage(vscr->screen_ptr->rcontext, path, 0);
			if (!bgimage) {
				wwarning(_("Could not load image \"%s\" for option \"%s\""), path, entry->key);
				wfree(path);
			} else {
				wfree(path);

				cwidth = atoi(WMGetFromPLString(WMGetFromPLArray(array, 2)));
				cheight = atoi(WMGetFromPLString(WMGetFromPLArray(array, 3)));

				if (cwidth <= 0 || cheight <= 0 ||
				    cwidth >= bgimage->width - 2 || cheight >= bgimage->height - 2)
					wwarning(_("Invalid split sizes for switch panel back image."));
				else {
					int i;
					int swidth, theight;
					for (i = 0; i < 9; i++) {
						if (wPreferences.swbackImage[i])
							RReleaseImage(wPreferences.swbackImage[i]);

						wPreferences.swbackImage[i] = NULL;
					}
					swidth = (bgimage->width - cwidth) / 2;
					theight = (bgimage->height - cheight) / 2;

					wPreferences.swbackImage[0] = chopOffImage(bgimage, 0, 0, swidth, theight);
					wPreferences.swbackImage[1] = chopOffImage(bgimage, swidth, 0, cwidth, theight);
					wPreferences.swbackImage[2] = chopOffImage(bgimage, swidth + cwidth, 0,
									     swidth, theight);

					wPreferences.swbackImage[3] = chopOffImage(bgimage, 0, theight, swidth, cheight);
					wPreferences.swbackImage[4] = chopOffImage(bgimage, swidth, theight,
									     cwidth, cheight);
					wPreferences.swbackImage[5] = chopOffImage(bgimage, swidth + cwidth, theight,
									     swidth, cheight);

					wPreferences.swbackImage[6] = chopOffImage(bgimage, 0, theight + cheight,
									     swidth, theight);
					wPreferences.swbackImage[7] = chopOffImage(bgimage, swidth, theight + cheight,
									     cwidth, theight);
					wPreferences.swbackImage[8] =
					    chopOffImage(bgimage, swidth + cwidth, theight + cheight, swidth,
							 theight);

					// check if anything failed
					for (i = 0; i < 9; i++) {
						if (!wPreferences.swbackImage[i]) {
							for (; i >= 0; --i) {
								RReleaseImage(wPreferences.swbackImage[i]);
								wPreferences.swbackImage[i] = NULL;
							}
							break;
						}
					}
				}
				RReleaseImage(bgimage);
			}
		}
		/* Falls through */
	case 1:
		if (!WMIsPLString(WMGetFromPLArray(array, 0))) {
			wwarning(_("Invalid arguments for option \"%s\""), entry->key);
			break;
		} else {
			path = FindImage(wPreferences.pixmap_path, WMGetFromPLString(WMGetFromPLArray(array, 0)));
		}

		if (!path) {
			wwarning(_("Could not find image \"%s\" for option \"%s\""),
				 WMGetFromPLString(WMGetFromPLArray(array, 0)), entry->key);
		} else {
			if (wPreferences.swtileImage)
				RReleaseImage(wPreferences.swtileImage);

			wPreferences.swtileImage = RLoadImage(vscr->screen_ptr->rcontext, path, 0);
			if (!wPreferences.swtileImage)
				wwarning(_("Could not load image \"%s\" for option \"%s\""), path, entry->key);

			wfree(path);
		}
		break;

	default:
		wwarning(_("Invalid number of arguments for option \"%s\""), entry->key);
		break;
	}

	WMReleasePropList(array);

	return 0;
}

static int setModifierKeyLabels(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	WMPropList *array = tdata;
	int i;

	(void) foo;

	if (!WMIsPLArray(array) || WMGetPropListItemCount(array) != 7) {
		wwarning(_("Value for option \"%s\" must be an array of 7 strings"), entry->key);
		WMReleasePropList(array);
		return 0;
	}

	DestroyWindowMenu(vscr);

	for (i = 0; i < 7; i++) {
		if (wPreferences.modifier_labels[i]) {
			wfree(wPreferences.modifier_labels[i]);
			wPreferences.modifier_labels[i] = NULL;
		}

		if (WMIsPLString(WMGetFromPLArray(array, i))) {
			wPreferences.modifier_labels[i] = wstrdup(WMGetFromPLString(WMGetFromPLArray(array, i)));
		} else {
			wwarning(_("Invalid argument for option \"%s\" item %d"), entry->key, i);
			wPreferences.modifier_labels[i] = NULL;
		}
	}

	WMReleasePropList(array);

	return 0;
}

static int setDoubleClick(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *foo)
{
	int *value = tdata;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) vscr;

	if (*value <= 0)
		*(int *)foo = 1;

	W_setconf_doubleClickDelay(*value);

	return 0;
}

static int setCursor(virtual_screen *vscr, WDefaultEntry *entry, void *tdata, void *extra_data)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;
	long widx = (long) extra_data;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) tdata;

	if (widx == WCUR_ROOT) {
		value = wPreferences.cursors.root->value;
	} else if (widx == WCUR_NORMAL) {
		/* Only for documentation, this case is not used */
		value = wPreferences.cursors.normal->value;
	} else if (widx == WCUR_MOVE) {
		value = wPreferences.cursors.move->value;
	} else if (widx == WCUR_RESIZE) {
		value = wPreferences.cursors.resize->value;
	} else if (widx == WCUR_TOPLEFTRESIZE) {
		value = wPreferences.cursors.resizetopleft->value;
	} else if (widx == WCUR_TOPRIGHTRESIZE) {
		value = wPreferences.cursors.resizetopright->value;
	} else if (widx == WCUR_BOTTOMLEFTRESIZE) {
		value = wPreferences.cursors.resizebottomleft->value;
	} else if (widx == WCUR_BOTTOMRIGHTRESIZE) {
		value = wPreferences.cursors.resizebottomright->value;
	} else if (widx == WCUR_VERTICALRESIZE) {
		value = wPreferences.cursors.resizevertical->value;
	} else if (widx == WCUR_HORIZONRESIZE) {
		value = wPreferences.cursors.resizehorizontal->value;
	} else if (widx == WCUR_WAIT) {
		value = wPreferences.cursors.wait->value;
	} else if (widx == WCUR_ARROW) {
		value = wPreferences.cursors.arrow->value;
	} else if (widx == WCUR_QUESTION) {
		value = wPreferences.cursors.question->value;
	} else if (widx == WCUR_TEXT) {
		value = wPreferences.cursors.text->value;
	} else if (widx == WCUR_SELECT) {
		value = wPreferences.cursors.select->value;
	} else if (widx == WCUR_EMPTY) {
		/* Only for documentation, this case is not used */
	}

	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));

		if (widx == WCUR_ROOT) {
			value = wPreferences.cursors.root->defvalue;
		} else if (widx == WCUR_NORMAL) {
			/* Only for documentation, this case is not used */
			value = wPreferences.cursors.normal->value;
		} else if (widx == WCUR_MOVE) {
			value = wPreferences.cursors.move->defvalue;
		} else if (widx == WCUR_RESIZE) {
			value = wPreferences.cursors.resize->defvalue;
		} else if (widx == WCUR_TOPLEFTRESIZE) {
			value = wPreferences.cursors.resizetopleft->defvalue;
		} else if (widx == WCUR_TOPRIGHTRESIZE) {
			value = wPreferences.cursors.resizetopright->defvalue;
		} else if (widx == WCUR_BOTTOMLEFTRESIZE) {
			value = wPreferences.cursors.resizebottomleft->defvalue;
		} else if (widx == WCUR_BOTTOMRIGHTRESIZE) {
			value = wPreferences.cursors.resizebottomright->defvalue;
		} else if (widx == WCUR_VERTICALRESIZE) {
			value = wPreferences.cursors.resizevertical->defvalue;
		} else if (widx == WCUR_HORIZONRESIZE) {
			value = wPreferences.cursors.resizehorizontal->defvalue;
		} else if (widx == WCUR_WAIT) {
			value = wPreferences.cursors.wait->defvalue;
		} else if (widx == WCUR_ARROW) {
			value = wPreferences.cursors.arrow->defvalue;
		} else if (widx == WCUR_QUESTION) {
			value = wPreferences.cursors.question->defvalue;
		} else if (widx == WCUR_TEXT) {
			value = wPreferences.cursors.text->defvalue;
		} else if (widx == WCUR_SELECT) {
			value = wPreferences.cursors.select->defvalue;
		} else if (widx == WCUR_EMPTY) {
			/* Only for documentation, this case is not used */
		}

		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[widx] != None)
		XFreeCursor(dpy, wPreferences.cursor[widx]);

	wPreferences.cursor[widx] = cursor;

	if (widx == WCUR_ROOT && cursor != None)
		XDefineCursor(dpy, vscr->screen_ptr->root_win, cursor);

	return 0;
}

char *get_wmstate_file(virtual_screen *vscr)
{
	char *str;
	char buf[16];

	if (w_global.screen_count == 1) {
		str = wdefaultspathfordomain("WMState");
	} else {
		snprintf(buf, sizeof(buf), "WMState.%i", vscr->id);
		str = wdefaultspathfordomain(buf);
	}

	return str;
}

static void convert_window_place_origin(WScreen *scr)
{
	if (wPreferences.window_place_origin.x < 0)
		wPreferences.window_place_origin.x = 0;
	else if (wPreferences.window_place_origin.x > scr->scr_width / 3)
		wPreferences.window_place_origin.x = scr->scr_width / 3;
	if (wPreferences.window_place_origin.y < 0)
		wPreferences.window_place_origin.y = 0;
	else if (wPreferences.window_place_origin.y > scr->scr_height / 3)
		wPreferences.window_place_origin.y = scr->scr_height / 3;
}

void apply_defaults_to_screen(virtual_screen *vscr, WScreen *scr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	convert_window_place_origin(scr);
}
