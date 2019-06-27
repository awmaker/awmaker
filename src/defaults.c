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
typedef int (WDECallbackConvert) (WDefaultEntry *entry, WMPropList *plvalue, void *addr);
typedef int (WDECallbackUpdate) (virtual_screen *vscr);

struct _WDefaultEntry {
	const char *key;
	const char *default_value;
	void *extra_data;
	void *addr;
	WDECallbackConvert *convert;
	WDECallbackUpdate *update;
	WMPropList *plkey;
	WMPropList *plvalue;	/* default value */
	unsigned int refresh;   /* Flag to comunicate convert and update calls */
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
static WDECallbackUpdate setIfClipPresent;
static WDECallbackUpdate setIfDrawerPresent;
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
static WDECallbackUpdate setKeyGrab_rootmenu;
static WDECallbackUpdate setKeyGrab_windowlist;
static WDECallbackUpdate setKeyGrab_windowmenu;
static WDECallbackUpdate setKeyGrab_dockraiselower;
static WDECallbackUpdate setKeyGrab_clipraiselower;
static WDECallbackUpdate setKeyGrab_miniaturize;
static WDECallbackUpdate setKeyGrab_minimizeall;
static WDECallbackUpdate setKeyGrab_hide;
static WDECallbackUpdate setKeyGrab_hideothers;
static WDECallbackUpdate setKeyGrab_moveresize;
static WDECallbackUpdate setKeyGrab_close;
static WDECallbackUpdate setKeyGrab_maximize;
static WDECallbackUpdate setKeyGrab_maximizev;
static WDECallbackUpdate setKeyGrab_maximizeh;
static WDECallbackUpdate setKeyGrab_maximizelh;
static WDECallbackUpdate setKeyGrab_maximizerh;
static WDECallbackUpdate setKeyGrab_maximizeth;
static WDECallbackUpdate setKeyGrab_maximizebh;
static WDECallbackUpdate setKeyGrab_maximizeltc;
static WDECallbackUpdate setKeyGrab_maximizertc;
static WDECallbackUpdate setKeyGrab_maximizelbc;
static WDECallbackUpdate setKeyGrab_maximizerbc;
static WDECallbackUpdate setKeyGrab_maximus;
static WDECallbackUpdate setKeyGrab_keepontop;
static WDECallbackUpdate setKeyGrab_keepatbottom;
static WDECallbackUpdate setKeyGrab_omnipresent;
static WDECallbackUpdate setKeyGrab_raise;
static WDECallbackUpdate setKeyGrab_lower;
static WDECallbackUpdate setKeyGrab_raiselower;
static WDECallbackUpdate setKeyGrab_shade;
static WDECallbackUpdate setKeyGrab_select;
static WDECallbackUpdate setKeyGrab_workspacemap;
static WDECallbackUpdate setKeyGrab_focusnext;
static WDECallbackUpdate setKeyGrab_focusprev;
static WDECallbackUpdate setKeyGrab_groupnext;
static WDECallbackUpdate setKeyGrab_groupprev;
static WDECallbackUpdate setKeyGrab_workspacenext;
static WDECallbackUpdate setKeyGrab_workspaceprev;
static WDECallbackUpdate setKeyGrab_workspacelast;
static WDECallbackUpdate setKeyGrab_workspacelayernext;
static WDECallbackUpdate setKeyGrab_workspacelayerprev;
static WDECallbackUpdate setKeyGrab_workspace1;
static WDECallbackUpdate setKeyGrab_workspace2;
static WDECallbackUpdate setKeyGrab_workspace3;
static WDECallbackUpdate setKeyGrab_workspace4;
static WDECallbackUpdate setKeyGrab_workspace5;
static WDECallbackUpdate setKeyGrab_workspace6;
static WDECallbackUpdate setKeyGrab_workspace7;
static WDECallbackUpdate setKeyGrab_workspace8;
static WDECallbackUpdate setKeyGrab_workspace9;
static WDECallbackUpdate setKeyGrab_workspace10;
static WDECallbackUpdate setKeyGrab_movetoworkspace1;
static WDECallbackUpdate setKeyGrab_movetoworkspace2;
static WDECallbackUpdate setKeyGrab_movetoworkspace3;
static WDECallbackUpdate setKeyGrab_movetoworkspace4;
static WDECallbackUpdate setKeyGrab_movetoworkspace5;
static WDECallbackUpdate setKeyGrab_movetoworkspace6;
static WDECallbackUpdate setKeyGrab_movetoworkspace7;
static WDECallbackUpdate setKeyGrab_movetoworkspace8;
static WDECallbackUpdate setKeyGrab_movetoworkspace9;
static WDECallbackUpdate setKeyGrab_movetoworkspace10;
static WDECallbackUpdate setKeyGrab_movetonextworkspace;
static WDECallbackUpdate setKeyGrab_movetoprevworkspace;
static WDECallbackUpdate setKeyGrab_movetolastworkspace;
static WDECallbackUpdate setKeyGrab_movetonextworkspacelayer;
static WDECallbackUpdate setKeyGrab_movetoprevworkspacelayer;
static WDECallbackUpdate setKeyGrab_windowshortcut1;
static WDECallbackUpdate setKeyGrab_windowshortcut2;
static WDECallbackUpdate setKeyGrab_windowshortcut3;
static WDECallbackUpdate setKeyGrab_windowshortcut4;
static WDECallbackUpdate setKeyGrab_windowshortcut5;
static WDECallbackUpdate setKeyGrab_windowshortcut6;
static WDECallbackUpdate setKeyGrab_windowshortcut7;
static WDECallbackUpdate setKeyGrab_windowshortcut8;
static WDECallbackUpdate setKeyGrab_windowshortcut9;
static WDECallbackUpdate setKeyGrab_windowshortcut10;
static WDECallbackUpdate setKeyGrab_moveto12to6head;
static WDECallbackUpdate setKeyGrab_moveto6to12head;
static WDECallbackUpdate setKeyGrab_windowrelaunch;
static WDECallbackUpdate setKeyGrab_screenswitch;
static WDECallbackUpdate setKeyGrab_run;
#ifdef KEEP_XKB_LOCK_STATUS
static WDECallbackUpdate setKeyGrab_togglekbdmode;
#endif
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
static WDECallbackUpdate setCursor_root;
static WDECallbackUpdate setCursor_select;
static WDECallbackUpdate setCursor_move;
static WDECallbackUpdate setCursor_resize;
static WDECallbackUpdate setCursor_topleftresize;
static WDECallbackUpdate setCursor_toprightresize;
static WDECallbackUpdate setCursor_bottomleftresize;
static WDECallbackUpdate setCursor_bottomrightresize;
static WDECallbackUpdate setCursor_horizontalresize;
static WDECallbackUpdate setCursor_verticalresize;
static WDECallbackUpdate setCursor_wait;
static WDECallbackUpdate setCursor_arrow;
static WDECallbackUpdate setCursor_question;
static WDECallbackUpdate setCursor_text;

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
enum {
	SOL_COLORMAPSIZE,
	SOL_DISABLEDITHERING,
	SOL_ICONSIZE,
	SOL_MODIFIERKEY,
	SOL_FOCUSMODE,
	SOL_NEWSTYLE,
	SOL_DISABLEDOCK,
	SOL_DISABLECLIP,
	SOL_DISABLEDRAWERS,
	SOL_CLIPMERGEDINDOCK,
	SOL_DISABLEMINIWINDOWS,
	SOL_ENABLEWORKSPACEPAGER
};

WDefaultEntry staticOptionList[] = {

	{"ColormapSize", "4", NULL,
	    &wPreferences.cmap_size, getInt, NULL, NULL, NULL, 0},
	{"DisableDithering", "NO", NULL,
	    &wPreferences.no_dithering, getBool, NULL, NULL, NULL, 0},
	{"IconSize", "64", NULL,
	    &wPreferences.icon_size, getInt, NULL, NULL, NULL, 0},
	{"ModifierKey", "Mod1", NULL,
	    &wPreferences.modifier_mask, getModMask, NULL, NULL, NULL, 0},
	{"FocusMode", "manual", seFocusModes,				/* have a problem when switching from */
	    &wPreferences.focus_mode, getEnum, NULL, NULL, NULL, 0},	/* manual to sloppy without restart */
	{"NewStyle", "new", seTitlebarModes,
	    &wPreferences.new_style, getEnum, NULL, NULL, NULL, 0},
	{"DisableDock", "NO", NULL,
	    &wPreferences.flags.nodock, getBool, setIfDockPresent, NULL, NULL, 0},
	{"DisableClip", "NO", NULL,
	    &wPreferences.flags.noclip, getBool, setIfClipPresent, NULL, NULL, 0},
	{"DisableDrawers", "NO", NULL,
	    &wPreferences.flags.nodrawer, getBool, setIfDrawerPresent, NULL, NULL, 0},
	{"ClipMergedInDock", "NO", NULL,
	    &wPreferences.flags.clip_merged_in_dock, getBool, setClipMergedInDock, NULL, NULL, 0},
	{"DisableMiniwindows", "NO", NULL,
	    &wPreferences.disable_miniwindows, getBool, NULL, NULL, NULL, 0},
	{"EnableWorkspacePager", "NO", NULL,
	    &wPreferences.enable_workspace_pager, getBool, NULL, NULL, NULL, 0}
};

enum {
	OL_ICONPOSITION,
	OL_ICONIFICATIONSTYLE,
	OL_ENFORCEICONMARGIN,
	OL_DISABLEWSMOUSEACTIONS,
	OL_MOUSELEFTBUTTONACTION,
	OL_MOUSEMIDDLEBUTTONACTION,
	OL_MOUSERIGHTBUTTONACTION,
	OL_MOUSEBACKWARDBUTTONACTION,
	OL_MOUSEFORWARDBUTTONACTION,
	OL_MOUSEWHEELACTION,
	OL_MOUSEWHEELTILTACTION,
	OL_PIXMAPPATH,
	OL_ICONPATH,
	OL_COLORMAPMODE,
	OL_AUTOFOCUS,
	OL_RAISEDELAY,
	OL_CIRCULATERAISE,
	OL_SUPERFLUOUS,
	OL_ADVANCETONEWWORKSPACE,
	OL_CYCLEWORKSPACES,
	OL_WORKSPACENAMEDISPLAYPOSITION,
	OL_WORKSPACEBORDER,
	OL_WORKSPACEBORDERSIZE,
	OL_STICKYICONS,
	OL_SAVESESSIONONEXIT,
	OL_WRAPMENUS,
	OL_SCROLLABLEMENUS,
	OL_MENUSCROLLSPEED,
	OL_ICONSLIDESPEED,
	OL_SHADESPEED,
	OL_BOUNCEAPPICONSWHENURGENT,
	OL_RAISEAPPICONSWHENBOUNCING,
	OL_DONOTMAKEAPPICONSBOUNCE,
	OL_DOUBLECLICKTIME,
	OL_CLIPAUTORAISEDELAY,
	OL_CLIPAUTOLOWERDELAY,
	OL_CLIPAUTOEXPANDDELAY,
	OL_CLIPAUTOCOLLAPSEDELAY,
	OL_WRAPAPPICONSINDOCK,
	OL_ALIGNSUBMENUS,
	OL_VIKEYMENUS,
	OL_OPENTRANSIENTONOWNERWORKSPACE,
	OL_WINDOWPLACEMENT,
	OL_IGNOREFOCUSCLICK,
	OL_USESAVEUNDERS,
	OL_OPAQUEMOVE,
	OL_OPAQUERESIZE,
	OL_OPAQUEMOVERESIZEKEYBOARD,
	OL_DISABLEANIMATIONS,
	OL_DONTLINKWORKSPACES,
	OL_WINDOWSNAPPING,
	OL_SNAPEDGEDETECT,
	OL_SNAPCORNERDETECT,
	OL_SNAPTOTOPMAXIMIZESFULLSCREEN,
	OL_DRAGMAXIMIZEDWINDOW,
	OL_MOVEHALFMAXIMIZEDWINDOWSBETWEENSCREENS,
	OL_ALTERNATIVEHALFMAXIMIZED,
	OL_POINTERWITHHALFMAXWINDOWS,
	OL_HIGHLIGHTACTIVEAPP,
	OL_AUTOARRANGEICONS,
	OL_NOWINDOWOVERDOCK,
	OL_NOWINDOWOVERICONS,
	OL_WINDOWPLACEORIGIN,
	OL_RESIZEDISPLAY,
	OL_MOVEDISPLAY,
	OL_DONTCONFIRMKILL,
	OL_WINDOWTITLEBALLOONS,
	OL_MINIWINDOWTITLEBALLOONS,
	OL_MINIWINDOWPREVIEWBALLOONS,
	OL_APPICONBALLOONS,
	OL_HELPBALLOONS,
	OL_EDGERESISTANCE,
	OL_RESIZEINCREMENT,
	OL_ATTRACTION,
	OL_DISABLEBLINKING,
	OL_SINGLECLICKLAUNCH,
	OL_STRICTWINDOZECYCLE,
	OL_SWITCHPANELONLYOPEN,
	OL_MINIPREVIEWSIZE,
	OL_IGNOREGTKHINTS,
	OL_MENUSTYLE,
	OL_WIDGETCOLOR,
	OL_WORKSPACESPECIFICBACK,
	OL_WORKSPACEBACK,
	OL_SMOOTHWORKSPACEBACK,
	OL_ICONBACK,
	OL_TITLEJUSTIFY,
	OL_WINDOWTITLEFONT,
	OL_WINDOWTITLEEXTENDSPACE,
	OL_WINDOWTITLEMINHEIGHT,
	OL_WINDOWTITLEMAXHEIGHT,
	OL_MENUTITLEEXTENDSPACE,
	OL_MENUTITLEMINHEIGHT,
	OL_MENUTITLEMAXHEIGHT,
	OL_MENUTEXTEXTENDSPACE,
	OL_MENUTITLEFONT,
	OL_MENUTEXTFONT,
	OL_ICONTITLEFONT,
	OL_CLIPTITLEFONT,
	OL_SHOWCLIPTITLE,
	OL_LARGEDISPLAYFONT,
	OL_HIGHLIGHTCOLOR,
	OL_HIGHLIGHTTEXTCOLOR,
	OL_CLIPTITLECOLOR,
	OL_CCLIPTITLECOLOR,
	OL_FTITLECOLOR,
	OL_PTITLECOLOR,
	OL_UTITLECOLOR,
	OL_FTITLEBACK,
	OL_PTITLEBACK,
	OL_UTITLEBACK,
	OL_RESIZEBARBACK,
	OL_MENUTITLECOLOR,
	OL_MENUTEXTCOLOR,
	OL_MENUDISABLEDCOLOR,
	OL_MENUTITLEBACK,
	OL_MENUTEXTBACK,
	OL_ICONTITLECOLOR,
	OL_ICONTITLEBACK,
	OL_SWITCHPANELIMAGES,
	OL_MODIFIERKEYLABELS,
	OL_FRAMEBORDERWIDTH,
	OL_FRAMEBORDERCOLOR,
	OL_FRAMEFOCUSEDBORDERCOLOR,
	OL_FRAMESELECTEDBORDERCOLOR,
	OL_WORKSPACEMAPBACK,
	OL_ROOTMENUKEY,
	OL_WINDOWLISTKEY,
	OL_WINDOWMENUKEY,
	OL_DOCKRAISELOWERKEY,
	OL_CLIPRAISELOWERKEY,
	OL_MINIATURIZEKEY,
	OL_MINIMIZEALLKEY,
	OL_HIDEKEY,
	OL_HIDEOTHERSKEY,
	OL_MOVERESIZEKEY,
	OL_CLOSEKEY,
	OL_MAXIMIZEKEY,
	OL_VMAXIMIZEKEY,
	OL_HMAXIMIZEKEY,
	OL_LHMAXIMIZEKEY,
	OL_RHMAXIMIZEKEY,
	OL_THMAXIMIZEKEY,
	OL_BHMAXIMIZEKEY,
	OL_LTCMAXIMIZEKEY,
	OL_RTCMAXIMIZEKEY,
	OL_LBCMAXIMIZEKEY,
	OL_RBCMAXIMIZEKEY,
	OL_MAXIMUSKEY,
	OL_KEEPONTOPKEY,
	OL_KEEPATBOTTOMKEY,
	OL_OMNIPRESENTKEY,
	OL_RAISEKEY,
	OL_LOWERKEY,
	OL_RAISELOWERKEY,
	OL_SHADEKEY,
	OL_SELECTKEY,
	OL_WORKSPACEMAPKEY,
	OL_FOCUSNEXTKEY,
	OL_FOCUSPREVKEY,
	OL_GROUPNEXTKEY,
	OL_GROUPPREVKEY,
	OL_NEXTWORKSPACEKEY,
	OL_PREVWORKSPACEKEY,
	OL_LASTWORKSPACEKEY,
	OL_NEXTWORKSPACELAYERKEY,
	OL_PREVWORKSPACELAYERKEY,
	OL_WORKSPACE1KEY,
	OL_WORKSPACE2KEY,
	OL_WORKSPACE3KEY,
	OL_WORKSPACE4KEY,
	OL_WORKSPACE5KEY,
	OL_WORKSPACE6KEY,
	OL_WORKSPACE7KEY,
	OL_WORKSPACE8KEY,
	OL_WORKSPACE9KEY,
	OL_WORKSPACE10KEY,
	OL_MOVETOWORKSPACE1KEY,
	OL_MOVETOWORKSPACE2KEY,
	OL_MOVETOWORKSPACE3KEY,
	OL_MOVETOWORKSPACE4KEY,
	OL_MOVETOWORKSPACE5KEY,
	OL_MOVETOWORKSPACE6KEY,
	OL_MOVETOWORKSPACE7KEY,
	OL_MOVETOWORKSPACE8KEY,
	OL_MOVETOWORKSPACE9KEY,
	OL_MOVETOWORKSPACE10KEY,
	OL_MOVETONEXTWORKSPACEKEY,
	OL_MOVETOPREVWORKSPACEKEY,
	OL_MOVETOLASTWORKSPACEKEY,
	OL_MOVETONEXTWORKSPACELAYERKEY,
	OL_MOVETOPREVWORKSPACELAYERKEY,
	OL_WINDOWSHORTCUT1KEY,
	OL_WINDOWSHORTCUT2KEY,
	OL_WINDOWSHORTCUT3KEY,
	OL_WINDOWSHORTCUT4KEY,
	OL_WINDOWSHORTCUT5KEY,
	OL_WINDOWSHORTCUT6KEY,
	OL_WINDOWSHORTCUT7KEY,
	OL_WINDOWSHORTCUT8KEY,
	OL_WINDOWSHORTCUT9KEY,
	OL_WINDOWSHORTCUT10KEY,
	OL_MOVETO12TO6HEAD,
	OL_MOVETO6TO12HEAD,
	OL_WINDOWRELAUNCHKEY,
	OL_SCREENSWITCHKEY,
	OL_RUNKEY,
#ifdef KEEP_XKB_LOCK_STATUS
	OL_TOGGLEKBDMODEKEY,
	OL_KBDMODELOCK,
#endif	/* KEEP_XKB_LOCK_STATUS */
	OL_NORMALCURSOR,
	OL_ARROWCURSOR,
	OL_MOVECURSOR,
	OL_RESIZECURSOR,
	OL_TOPLEFTRESIZECURSOR,
	OL_TOPRIGHTRESIZECURSOR,
	OL_BOTTOMLEFTRESIZECURSOR,
	OL_BOTTOMRIGHTRESIZECURSOR,
	OL_VERTICALRESIZECURSOR,
	OL_HORIZONTALRESIZECURSOR,
	OL_WAITCURSOR,
	OL_QUESTIONCURSOR,
	OL_TEXTCURSOR,
	OL_SELECTCURSOR,
	OL_DIALOGHISTORYLINES,
	OL_CYCLEACTIVEHEADONLY,
	OL_CYCLEIGNOREMINIMIZED
};

WDefaultEntry optionList[] = {
	/* dynamic options */

	{"IconPosition", "blh", seIconPositions,
	    &wPreferences.icon_yard, getEnum, setIconPosition, NULL, NULL, 0},
	{"IconificationStyle", "Zoom", seIconificationStyles,
	    &wPreferences.iconification_style, getEnum, NULL, NULL, NULL, 0},
	{"EnforceIconMargin", "NO", NULL,
	    &wPreferences.enforce_icon_margin, getBool, NULL, NULL, NULL, 0},
	{"DisableWSMouseActions", "NO", NULL,
	    &wPreferences.disable_root_mouse, getBool, NULL, NULL, NULL, 0},
	{"MouseLeftButtonAction", "SelectWindows", seMouseButtonActions,
	    &wPreferences.mouse_button1, getEnum, NULL, NULL, NULL, 0},
	{"MouseMiddleButtonAction", "OpenWindowListMenu", seMouseButtonActions,
	    &wPreferences.mouse_button2, getEnum, NULL, NULL, NULL, 0},
	{"MouseRightButtonAction", "OpenApplicationsMenu", seMouseButtonActions,
	    &wPreferences.mouse_button3, getEnum, NULL, NULL, NULL, 0},
	{"MouseBackwardButtonAction", "None", seMouseButtonActions,
	    &wPreferences.mouse_button8, getEnum, NULL, NULL, NULL, 0},
	{"MouseForwardButtonAction", "None", seMouseButtonActions,
	    &wPreferences.mouse_button9, getEnum, NULL, NULL, NULL, 0},
	{"MouseWheelAction", "None", seMouseWheelActions,
	    &wPreferences.mouse_wheel_scroll, getEnum, NULL, NULL, NULL, 0},
	{"MouseWheelTiltAction", "None", seMouseWheelActions,
	    &wPreferences.mouse_wheel_tilt, getEnum, NULL, NULL, NULL, 0},
	{"PixmapPath", DEF_PIXMAP_PATHS, NULL,
	    &wPreferences.pixmap_path, getPathList, NULL, NULL, NULL, 0},
	{"IconPath", DEF_ICON_PATHS, NULL,
	    &wPreferences.icon_path, getPathList, NULL, NULL, NULL, 0},
	{"ColormapMode", "auto", seColormapModes,
	    &wPreferences.colormap_mode, getEnum, NULL, NULL, NULL, 0},
	{"AutoFocus", "YES", NULL,
	    &wPreferences.auto_focus, getBool, NULL, NULL, NULL, 0},
	{"RaiseDelay", "0", NULL,
	    &wPreferences.raise_delay, getInt, NULL, NULL, NULL, 0},
	{"CirculateRaise", "NO", NULL,
	    &wPreferences.circ_raise, getBool, NULL, NULL, NULL, 0},
	{"Superfluous", "YES", NULL,
	    &wPreferences.superfluous, getBool, NULL, NULL, NULL, 0},
	{"AdvanceToNewWorkspace", "NO", NULL,
	    &wPreferences.ws_advance, getBool, NULL, NULL, NULL, 0},
	{"CycleWorkspaces", "NO", NULL,
	    &wPreferences.ws_cycle, getBool, NULL, NULL, NULL, 0},
	{"WorkspaceNameDisplayPosition", "center", seDisplayPositions,
	    &wPreferences.workspace_name_display_position, getEnum, NULL, NULL, NULL, 0},
	{"WorkspaceBorder", "None", seWorkspaceBorder,
	    &wPreferences.workspace_border_position, getEnum, updateUsableArea, NULL, NULL, 0},
	{"WorkspaceBorderSize", "0", NULL,
	    &wPreferences.workspace_border_size, getInt, updateUsableArea, NULL, NULL, 0},
	{"StickyIcons", "NO", NULL,
	    &wPreferences.sticky_icons, getBool, setStickyIcons, NULL, NULL, 0},
	{"SaveSessionOnExit", "NO", NULL,
	    &wPreferences.save_session_on_exit, getBool, NULL, NULL, NULL, 0},
	{"WrapMenus", "NO", NULL,
	    &wPreferences.wrap_menus, getBool, NULL, NULL, NULL, 0},
	{"ScrollableMenus", "YES", NULL,
	    &wPreferences.scrollable_menus, getBool, NULL, NULL, NULL, 0},
	{"MenuScrollSpeed", "fast", seSpeeds,
	    &wPreferences.menu_scroll_speed, getEnum, NULL, NULL, NULL, 0},
	{"IconSlideSpeed", "fast", seSpeeds,
	    &wPreferences.icon_slide_speed, getEnum, NULL, NULL, NULL, 0},
	{"ShadeSpeed", "fast", seSpeeds,
	    &wPreferences.shade_speed, getEnum, NULL, NULL, NULL, 0},
	{"BounceAppIconsWhenUrgent", "YES", NULL,
	    &wPreferences.bounce_appicons_when_urgent, getBool, NULL, NULL, NULL, 0},
	{"RaiseAppIconsWhenBouncing", "NO", NULL,
	    &wPreferences.raise_appicons_when_bouncing, getBool, NULL, NULL, NULL, 0},
	{"DoNotMakeAppIconsBounce", "NO", NULL,
	    &wPreferences.do_not_make_appicons_bounce, getBool, NULL, NULL, NULL, 0},
	{"DoubleClickTime", "250", (void *) &wPreferences.dblclick_time,
	    &wPreferences.dblclick_time, getInt, setDoubleClick, NULL, NULL, 0},
	{"ClipAutoraiseDelay", "600", NULL,
	     &wPreferences.clip_auto_raise_delay, getInt, NULL, NULL, NULL, 0},
	{"ClipAutolowerDelay", "1000", NULL,
	    &wPreferences.clip_auto_lower_delay, getInt, NULL, NULL, NULL, 0},
	{"ClipAutoexpandDelay", "600", NULL,
	    &wPreferences.clip_auto_expand_delay, getInt, NULL, NULL, NULL, 0},
	{"ClipAutocollapseDelay", "1000", NULL,
	    &wPreferences.clip_auto_collapse_delay, getInt, NULL, NULL, NULL, 0},
	{"WrapAppiconsInDock", "YES", NULL,
	    &wPreferences.flags.wrap_appicons_in_dock, getBool, setWrapAppiconsInDock, NULL, NULL, 0},
	{"AlignSubmenus", "NO", NULL,
	    &wPreferences.align_menus, getBool, NULL, NULL, NULL, 0},
	{"ViKeyMenus", "NO", NULL,
	    &wPreferences.vi_key_menus, getBool, NULL, NULL, NULL, 0},
	{"OpenTransientOnOwnerWorkspace", "NO", NULL,
	    &wPreferences.open_transients_with_parent, getBool, NULL, NULL, NULL, 0},
	{"WindowPlacement", "auto", sePlacements,
	    &wPreferences.window_placement, getEnum, NULL, NULL, NULL, 0},
	{"IgnoreFocusClick", "NO", NULL,
	    &wPreferences.ignore_focus_click, getBool, NULL, NULL, NULL, 0},
	{"UseSaveUnders", "NO", NULL,
	    &wPreferences.use_saveunders, getBool, NULL, NULL, NULL, 0},
	{"OpaqueMove", "YES", NULL,
	    &wPreferences.opaque_move, getBool, NULL, NULL, NULL, 0},
	{"OpaqueResize", "NO", NULL,
	    &wPreferences.opaque_resize, getBool, NULL, NULL, NULL, 0},
	{"OpaqueMoveResizeKeyboard", "NO", NULL,
	    &wPreferences.opaque_move_resize_keyboard, getBool, NULL, NULL, NULL, 0},
	{"DisableAnimations", "NO", NULL,
	    &wPreferences.no_animations, getBool, NULL, NULL, NULL, 0},
	{"DontLinkWorkspaces", "YES", NULL,
	    &wPreferences.no_autowrap, getBool, NULL, NULL, NULL, 0},
	{"WindowSnapping", "NO", NULL,
	    &wPreferences.window_snapping, getBool, NULL, NULL, NULL, 0},
	{"SnapEdgeDetect", "1", NULL,
	    &wPreferences.snap_edge_detect, getInt, NULL, NULL, NULL, 0},
	{"SnapCornerDetect", "10", NULL,
	    &wPreferences.snap_corner_detect, getInt, NULL, NULL, NULL, 0},
	{"SnapToTopMaximizesFullscreen", "NO", NULL,
	    &wPreferences.snap_to_top_maximizes_fullscreen, getBool, NULL, NULL, NULL, 0},
	{"DragMaximizedWindow", "Move", seDragMaximizedWindow,
	    &wPreferences.drag_maximized_window, getEnum, NULL, NULL, NULL, 0},
	{"MoveHalfMaximizedWindowsBetweenScreens", "NO", NULL,
	    &wPreferences.move_half_max_between_heads, getBool, NULL, NULL, NULL, 0},
	{"AlternativeHalfMaximized", "NO", NULL,
	    &wPreferences.alt_half_maximize, getBool, NULL, NULL, NULL, 0},
	{"PointerWithHalfMaxWindows", "NO", NULL,
	    &wPreferences.pointer_with_half_max_windows, getBool, NULL, NULL, NULL, 0},
	{"HighlightActiveApp", "YES", NULL,
	    &wPreferences.highlight_active_app, getBool, NULL, NULL, NULL, 0},
	{"AutoArrangeIcons", "NO", NULL,
	    &wPreferences.auto_arrange_icons, getBool, NULL, NULL, NULL, 0},
	{"NoWindowOverDock", "NO", NULL,
	    &wPreferences.no_window_over_dock, getBool, updateUsableArea, NULL, NULL, 0},
	{"NoWindowOverIcons", "NO", NULL,
	    &wPreferences.no_window_over_icons, getBool, updateUsableArea, NULL, NULL, 0},
	{"WindowPlaceOrigin", "(64, 0)", NULL,
	    &wPreferences.window_place_origin, getCoord, NULL, NULL, NULL, 0},
	{"ResizeDisplay", "center", seGeomDisplays,
	    &wPreferences.size_display, getEnum, NULL, NULL, NULL, 0},
	{"MoveDisplay", "floating", seGeomDisplays,
	    &wPreferences.move_display, getEnum, NULL, NULL, NULL, 0},
	{"DontConfirmKill", "NO", NULL,
	    &wPreferences.dont_confirm_kill, getBool, NULL, NULL, NULL, 0},
	{"WindowTitleBalloons", "YES", NULL,
	    &wPreferences.window_balloon, getBool, NULL, NULL, NULL, 0},
	{"MiniwindowTitleBalloons", "NO", NULL,
	    &wPreferences.miniwin_title_balloon, getBool, NULL, NULL, NULL, 0},
	{"MiniwindowPreviewBalloons", "NO", NULL,
	    &wPreferences.miniwin_preview_balloon, getBool, NULL, NULL, NULL, 0},
	{"AppIconBalloons", "NO", NULL,
	    &wPreferences.appicon_balloon, getBool, NULL, NULL, NULL, 0},
	{"HelpBalloons", "NO", NULL,
	    &wPreferences.help_balloon, getBool, NULL, NULL, NULL, 0},
	{"EdgeResistance", "30", NULL,
	    &wPreferences.edge_resistance, getInt, NULL, NULL, NULL, 0},
	{"ResizeIncrement", "0", NULL,
	    &wPreferences.resize_increment, getInt, NULL, NULL, NULL, 0},
	{"Attraction", "NO", NULL,
	    &wPreferences.attract, getBool, NULL, NULL, NULL, 0},
	{"DisableBlinking", "NO", NULL,
	    &wPreferences.dont_blink, getBool, NULL, NULL, NULL, 0},
	{"SingleClickLaunch",	"NO",	NULL,
	    &wPreferences.single_click, getBool, NULL, NULL, NULL, 0},
	{"StrictWindozeCycle",	"YES",	NULL,
	    &wPreferences.strict_windoze_cycle, getBool, NULL, NULL, NULL, 0},
	{"SwitchPanelOnlyOpen",	"NO",	NULL,
	    &wPreferences.panel_only_open, getBool, NULL, NULL, NULL, 0},
	{"MiniPreviewSize", "128", NULL,
	    &wPreferences.minipreview_size, getInt, NULL, NULL, NULL, 0},
	{"IgnoreGtkHints", "NO", NULL,
	    &wPreferences.ignore_gtk_decoration_hints, getBool, NULL, NULL, NULL, 0},

	/* style options */
	{"MenuStyle", "normal", seMenuStyles,
	    &wPreferences.menu_style, getEnum, setMenuStyle, NULL, NULL, 0},
	{"WidgetColor", "(solid, gray)", NULL,
	    &wPreferences.texture.widgetcolor, getTexture, setWidgetColor, NULL, NULL, 0},
	{"WorkspaceSpecificBack", "()", NULL,
	    &wPreferences.workspacespecificback, getWSSpecificBackground, setWorkspaceSpecificBack, NULL, NULL, 0},
	/* WorkspaceBack must come after WorkspaceSpecificBack or
	 * WorkspaceBack won't know WorkspaceSpecificBack was also
	 * specified and 2 copies of wmsetbg will be launched */
	{"WorkspaceBack", "(solid, \"rgb:50/50/75\")", NULL,
	    &wPreferences.workspaceback, getWSBackground, setWorkspaceBack, NULL, NULL, 0},
	{"SmoothWorkspaceBack", "NO", NULL,
	    NULL, getBool, NULL, NULL, NULL, 0},
	{"IconBack", "(dgradient, \"rgb:a6/a6/b6\", \"rgb:51/55/61\")", NULL,
	    &wPreferences.texture.iconback, getTexture, setIconTile, NULL, NULL, 0},
	{"TitleJustify", "center", seJustifications,
	    &wPreferences.title_justification, getEnum, setJustify, NULL, NULL, 0},
	{"WindowTitleFont", DEF_TITLE_FONT, NULL,
	    &wPreferences.font.wintitle, getFont, setWinTitleFont, NULL, NULL, 0},
	{"WindowTitleExtendSpace", DEF_WINDOW_TITLE_EXTEND_SPACE, NULL,
	    &wPreferences.window_title_clearance, getInt, setClearance, NULL, NULL, 0},
	{"WindowTitleMinHeight", "0", NULL,
	    &wPreferences.window_title_min_height, getInt, setClearance, NULL, NULL, 0},
	{"WindowTitleMaxHeight", NUM2STRING(INT_MAX), NULL,
	    &wPreferences.window_title_max_height, getInt, setClearance, NULL, NULL, 0},
	{"MenuTitleExtendSpace", DEF_MENU_TITLE_EXTEND_SPACE, NULL,
	    &wPreferences.menu_title_clearance, getInt, setClearance, NULL, NULL, 0},
	{"MenuTitleMinHeight", "0", NULL,
	    &wPreferences.menu_title_min_height, getInt, setClearance, NULL, NULL, 0},
	{"MenuTitleMaxHeight", NUM2STRING(INT_MAX), NULL,
	    &wPreferences.menu_title_max_height, getInt, setClearance, NULL, NULL, 0},
	{"MenuTextExtendSpace", DEF_MENU_TEXT_EXTEND_SPACE, NULL,
	    &wPreferences.menu_text_clearance, getInt, setClearance, NULL, NULL, 0},
	{"MenuTitleFont", DEF_MENU_TITLE_FONT, NULL,
	    &wPreferences.font.menutitle, getFont, setMenuTitleFont, NULL, NULL, 0},
	{"MenuTextFont", DEF_MENU_ENTRY_FONT, NULL,
	    &wPreferences.font.menutext, getFont, setMenuTextFont, NULL, NULL, 0},
	{"IconTitleFont", DEF_ICON_TITLE_FONT, NULL,
	    &wPreferences.font.icontitle, getFont, setIconTitleFont, NULL, NULL, 0},
	{"ClipTitleFont", DEF_CLIP_TITLE_FONT, NULL,
	    &wPreferences.font.cliptitle, getFont, setClipTitleFont, NULL, NULL, 0},
	{"ShowClipTitle", "YES", NULL,
	    &wPreferences.show_clip_title, getBool, NULL, NULL, NULL, 0},
	{"LargeDisplayFont", DEF_WORKSPACE_NAME_FONT, NULL,
	    &wPreferences.font.largedisplay, getFont, setLargeDisplayFont, NULL, NULL, 0},
	{"HighlightColor", "white", NULL,
	    &wPreferences.color.highlight, getColor, setHightlight, NULL, NULL, 0},
	{"HighlightTextColor", "black", NULL,
	    &wPreferences.color.highlighttext, getColor, setHightlightText, NULL, NULL, 0},
	{"ClipTitleColor", "black", NULL,
	    &wPreferences.color.cliptitle, getColor, setClipTitleColor, NULL, NULL, 0},
	{"CClipTitleColor", "\"rgb:61/61/61\"", NULL,
	    &wPreferences.color.cliptitlecollapsed, getColor, setClipTitleColorCollapsed, NULL, NULL, 0},
	{"FTitleColor", "white", NULL,
	    &wPreferences.color.titlefocused, getColor, setWTitleColorFocused, NULL, NULL, 0},
	{"PTitleColor", "white", NULL,
	    &wPreferences.color.titleowner, getColor, setWTitleColorOwner, NULL, NULL, 0},
	{"UTitleColor", "black", NULL,
	    &wPreferences.color.titleunfocused, getColor, setWTitleColorUnfocused, NULL, NULL, 0},
	{"FTitleBack", "(solid, black)", NULL,
	    &wPreferences.texture.titlebackfocused, getTexture, setFTitleBack, NULL, NULL, 0},
	{"PTitleBack", "(solid, gray40)", NULL,
	    &wPreferences.texture.titlebackowner, getTexture, setPTitleBack, NULL, NULL, 0},
	{"UTitleBack", "(solid, \"rgb:aa/aa/aa\")", NULL,
	    &wPreferences.texture.titlebackunfocused, getTexture, setUTitleBack, NULL, NULL, 0},
	{"ResizebarBack", "(solid, \"rgb:aa/aa/aa\")", NULL,
	    &wPreferences.texture.resizebarback, getTexture, setResizebarBack, NULL, NULL, 0},
	{"MenuTitleColor", "white", NULL,
	    &wPreferences.color.menutitle, getColor, setMenuTitleColor, NULL, NULL, 0},
	{"MenuTextColor", "black", NULL,
	    &wPreferences.color.menutext, getColor, setMenuTextColor, NULL, NULL, 0},
	{"MenuDisabledColor", "gray50", NULL,
	    &wPreferences.color.menudisabled, getColor, setMenuDisabledColor, NULL, NULL, 0},
	{"MenuTitleBack", "(solid, black)", NULL,
	    &wPreferences.texture.menutitleback, getTexture, setMenuTitleBack, NULL, NULL, 0},
	{"MenuTextBack", "(solid, \"rgb:aa/aa/aa\")", NULL,
	    &wPreferences.texture.menutextback, getTexture, setMenuTextBack, NULL, NULL, 0},
	{"IconTitleColor", "white", NULL,
	    &wPreferences.color.icontitle, getColor, setIconTitleColor, NULL, NULL, 0},
	{"IconTitleBack", "black", NULL,
	    &wPreferences.color.icontitleback, getColor, setIconTitleBack, NULL, NULL, 0},
	{"SwitchPanelImages", "(swtile.png, swback.png, 30, 40)", NULL,
	    &wPreferences.sp_options, getPropList, setSwPOptions, NULL, NULL, 0},
	{"ModifierKeyLabels", "(\"Shift+\", \"Control+\", \"Mod1+\", \"Mod2+\", \"Mod3+\", \"Mod4+\", \"Mod5+\")", NULL,
	    &wPreferences.modifierkeylabels, getPropList, setModifierKeyLabels, NULL, NULL, 0},
	{"FrameBorderWidth", "1", NULL,
	    &wPreferences.border_width, getInt, setFrameBorderWidth, NULL, NULL, 0},
	{"FrameBorderColor", "black", NULL,
	    &wPreferences.color.frameborder, getColor, setFrameBorderColor, NULL, NULL, 0},
	{"FrameFocusedBorderColor", "black", NULL,
	    &wPreferences.color.frameborderfocused, getColor, setFrameFocusedBorderColor, NULL, NULL, 0},
	{"FrameSelectedBorderColor", "white", NULL,
	    &wPreferences.color.frameborderselected, getColor, setFrameSelectedBorderColor, NULL, NULL, 0},
	{"WorkspaceMapBack", "(solid, black)", NULL,
	    &wPreferences.texture.workspacemapback, getTexture, setWorkspaceMapBackground, NULL, NULL, 0},

	/* keybindings */

	{"RootMenuKey", "F12", NULL,
	    &wPreferences.key.rootmenu, getKeybind, setKeyGrab_rootmenu, NULL, NULL, 0},
	{"WindowListKey", "F11", NULL,
	    &wPreferences.key.windowlist, getKeybind, setKeyGrab_windowlist, NULL, NULL, 0},
	{"WindowMenuKey", "Control+Escape", NULL,
	    &wPreferences.key.windowmenu, getKeybind, setKeyGrab_windowmenu, NULL, NULL, 0},
	{"DockRaiseLowerKey", "None", NULL,
	    &wPreferences.key.dockraiselower, getKeybind, setKeyGrab_dockraiselower, NULL, NULL, 0},
	{"ClipRaiseLowerKey", "None", NULL,
	    &wPreferences.key.clipraiselower, getKeybind, setKeyGrab_clipraiselower, NULL, NULL, 0},
	{"MiniaturizeKey", "Mod1+M", NULL,
	    &wPreferences.key.miniaturize, getKeybind, setKeyGrab_miniaturize, NULL, NULL, 0},
	{"MinimizeAllKey", "None", NULL,
	    &wPreferences.key.minimizeall, getKeybind, setKeyGrab_minimizeall, NULL, NULL, 0},
	{"HideKey", "Mod1+H", NULL,
	    &wPreferences.key.hide, getKeybind, setKeyGrab_hide, NULL, NULL, 0},
	{"HideOthersKey", "None", NULL,
	    &wPreferences.key.hideothers, getKeybind, setKeyGrab_hideothers, NULL, NULL, 0},
	{"MoveResizeKey", "None", (void *)WKBD_MOVERESIZE,
	    &wPreferences.key.moveresize, getKeybind, setKeyGrab_moveresize, NULL, NULL, 0},
	{"CloseKey", "None", NULL,
	    &wPreferences.key.close, getKeybind, setKeyGrab_close, NULL, NULL, 0},
	{"MaximizeKey", "None", NULL,
	    &wPreferences.key.maximize, getKeybind, setKeyGrab_maximize, NULL, NULL, 0},
	{"VMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizev, getKeybind, setKeyGrab_maximizev, NULL, NULL, 0},
	{"HMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizeh, getKeybind, setKeyGrab_maximizeh, NULL, NULL, 0},
	{"LHMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizelh, getKeybind, setKeyGrab_maximizelh, NULL, NULL, 0},
	{"RHMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizerh, getKeybind, setKeyGrab_maximizerh, NULL, NULL, 0},
	{"THMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizeth, getKeybind, setKeyGrab_maximizeth, NULL, NULL, 0},
	{"BHMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizebh, getKeybind, setKeyGrab_maximizebh, NULL, NULL, 0},
	{"LTCMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizeltc, getKeybind, setKeyGrab_maximizeltc, NULL, NULL, 0},
	{"RTCMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizertc, getKeybind, setKeyGrab_maximizertc, NULL, NULL, 0},
	{"LBCMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizelbc, getKeybind, setKeyGrab_maximizelbc, NULL, NULL, 0},
	{"RBCMaximizeKey", "None", NULL,
	    &wPreferences.key.maximizerbc, getKeybind, setKeyGrab_maximizerbc, NULL, NULL, 0},
	{"MaximusKey", "None", NULL,
	    &wPreferences.key.maximus, getKeybind, setKeyGrab_maximus, NULL, NULL, 0},
	{"KeepOnTopKey", "None", NULL,
	    &wPreferences.key.keepontop, getKeybind, setKeyGrab_keepontop, NULL, NULL, 0},
	{"KeepAtBottomKey", "None", NULL,
	    &wPreferences.key.keepatbottom, getKeybind, setKeyGrab_keepatbottom, NULL, NULL, 0},
	{"OmnipresentKey", "None", NULL,
	    &wPreferences.key.omnipresent, getKeybind, setKeyGrab_omnipresent, NULL, NULL, 0},
	{"RaiseKey", "Mod1+Up", NULL,
	    &wPreferences.key.raise, getKeybind, setKeyGrab_raise, NULL, NULL, 0},
	{"LowerKey", "Mod1+Down", NULL,
	    &wPreferences.key.lower, getKeybind, setKeyGrab_lower, NULL, NULL, 0},
	{"RaiseLowerKey", "None", NULL,
	    &wPreferences.key.raiselower, getKeybind, setKeyGrab_raiselower, NULL, NULL, 0},
	{"ShadeKey", "None", NULL,
	    &wPreferences.key.shade, getKeybind, setKeyGrab_shade, NULL, NULL, 0},
	{"SelectKey", "None", NULL,
	    &wPreferences.key.select, getKeybind, setKeyGrab_select, NULL, NULL, 0},
	{"WorkspaceMapKey", "None", NULL,
	    &wPreferences.key.workspacemap, getKeybind, setKeyGrab_workspacemap, NULL, NULL, 0},
	{"FocusNextKey", "Mod1+Tab", NULL,
	    &wPreferences.key.focusnext, getKeybind, setKeyGrab_focusnext, NULL, NULL, 0},
	{"FocusPrevKey", "Mod1+Shift+Tab", NULL,
	    &wPreferences.key.focusprev, getKeybind, setKeyGrab_focusprev, NULL, NULL, 0},
	{"GroupNextKey", "None", NULL,
	    &wPreferences.key.groupnext, getKeybind, setKeyGrab_groupnext, NULL, NULL, 0},
	{"GroupPrevKey", "None", NULL,
	    &wPreferences.key.groupprev, getKeybind, setKeyGrab_groupprev, NULL, NULL, 0},
	{"NextWorkspaceKey", "Mod1+Control+Right", NULL,
	    &wPreferences.key.workspacenext, getKeybind, setKeyGrab_workspacenext, NULL, NULL, 0},
	{"PrevWorkspaceKey", "Mod1+Control+Left", NULL,
	    &wPreferences.key.workspaceprev, getKeybind, setKeyGrab_workspaceprev, NULL, NULL, 0},
	{"LastWorkspaceKey", "None", NULL,
	    &wPreferences.key.workspacelast, getKeybind, setKeyGrab_workspacelast, NULL, NULL, 0},
	{"NextWorkspaceLayerKey", "None", NULL,
	    &wPreferences.key.workspacelayernext, getKeybind, setKeyGrab_workspacelayernext, NULL, NULL, 0},
	{"PrevWorkspaceLayerKey", "None", NULL,
	    &wPreferences.key.workspacelayerprev, getKeybind, setKeyGrab_workspacelayerprev, NULL, NULL, 0},
	{"Workspace1Key", "Mod1+1", NULL,
	    &wPreferences.key.workspace1, getKeybind, setKeyGrab_workspace1, NULL, NULL, 0},
	{"Workspace2Key", "Mod1+2", NULL,
	    &wPreferences.key.workspace2, getKeybind, setKeyGrab_workspace2, NULL, NULL, 0},
	{"Workspace3Key", "Mod1+3", NULL,
	    &wPreferences.key.workspace3, getKeybind, setKeyGrab_workspace3, NULL, NULL, 0},
	{"Workspace4Key", "Mod1+4", NULL,
	    &wPreferences.key.workspace4, getKeybind, setKeyGrab_workspace4, NULL, NULL, 0},
	{"Workspace5Key", "Mod1+5", NULL,
	    &wPreferences.key.workspace5, getKeybind, setKeyGrab_workspace5, NULL, NULL, 0},
	{"Workspace6Key", "Mod1+6", NULL,
	    &wPreferences.key.workspace6, getKeybind, setKeyGrab_workspace6, NULL, NULL, 0},
	{"Workspace7Key", "Mod1+7", NULL,
	    &wPreferences.key.workspace7, getKeybind, setKeyGrab_workspace7, NULL, NULL, 0},
	{"Workspace8Key", "Mod1+8", NULL,
	    &wPreferences.key.workspace8, getKeybind, setKeyGrab_workspace8, NULL, NULL, 0},
	{"Workspace9Key", "Mod1+9", NULL,
	    &wPreferences.key.workspace9, getKeybind, setKeyGrab_workspace9, NULL, NULL, 0},
	{"Workspace10Key", "Mod1+0", NULL,
	    &wPreferences.key.workspace10, getKeybind, setKeyGrab_workspace10, NULL, NULL, 0},
	{"MoveToWorkspace1Key", "None", NULL,
	    &wPreferences.key.movetoworkspace1, getKeybind, setKeyGrab_movetoworkspace1, NULL, NULL, 0},
	{"MoveToWorkspace2Key", "None", NULL,
	    &wPreferences.key.movetoworkspace2, getKeybind, setKeyGrab_movetoworkspace2, NULL, NULL, 0},
	{"MoveToWorkspace3Key", "None", NULL,
	    &wPreferences.key.movetoworkspace3, getKeybind, setKeyGrab_movetoworkspace3, NULL, NULL, 0},
	{"MoveToWorkspace4Key", "None", NULL,
	    &wPreferences.key.movetoworkspace4, getKeybind, setKeyGrab_movetoworkspace4, NULL, NULL, 0},
	{"MoveToWorkspace5Key", "None", NULL,
	    &wPreferences.key.movetoworkspace5, getKeybind, setKeyGrab_movetoworkspace5, NULL, NULL, 0},
	{"MoveToWorkspace6Key", "None", NULL,
	    &wPreferences.key.movetoworkspace6, getKeybind, setKeyGrab_movetoworkspace6, NULL, NULL, 0},
	{"MoveToWorkspace7Key", "None", NULL,
	    &wPreferences.key.movetoworkspace7, getKeybind, setKeyGrab_movetoworkspace7, NULL, NULL, 0},
	{"MoveToWorkspace8Key", "None", NULL,
	    &wPreferences.key.movetoworkspace8, getKeybind, setKeyGrab_movetoworkspace8, NULL, NULL, 0},
	{"MoveToWorkspace9Key", "None", NULL,
	    &wPreferences.key.movetoworkspace9, getKeybind, setKeyGrab_movetoworkspace9, NULL, NULL, 0},
	{"MoveToWorkspace10Key", "None", NULL,
	    &wPreferences.key.movetoworkspace10, getKeybind, setKeyGrab_movetoworkspace10, NULL, NULL, 0},
	{"MoveToNextWorkspaceKey", "None", NULL,
	    &wPreferences.key.movetonextworkspace, getKeybind, setKeyGrab_movetonextworkspace, NULL, NULL, 0},
	{"MoveToPrevWorkspaceKey", "None", NULL,
	    &wPreferences.key.movetoprevworkspace, getKeybind, setKeyGrab_movetoprevworkspace, NULL, NULL, 0},
	{"MoveToLastWorkspaceKey", "None", NULL,
	    &wPreferences.key.movetolastworkspace, getKeybind, setKeyGrab_movetolastworkspace, NULL, NULL, 0},
	{"MoveToNextWorkspaceLayerKey", "None", NULL,
	    &wPreferences.key.movetonextworkspace, getKeybind, setKeyGrab_movetonextworkspacelayer, NULL, NULL, 0},
	{"MoveToPrevWorkspaceLayerKey", "None", NULL,
	    &wPreferences.key.movetoprevworkspace, getKeybind, setKeyGrab_movetoprevworkspacelayer, NULL, NULL, 0},
	{"WindowShortcut1Key", "None", NULL,
	    &wPreferences.key.windowshortcut1, getKeybind, setKeyGrab_windowshortcut1, NULL, NULL, 0},
	{"WindowShortcut2Key", "None", NULL,
	    &wPreferences.key.windowshortcut2, getKeybind, setKeyGrab_windowshortcut2, NULL, NULL, 0},
	{"WindowShortcut3Key", "None", NULL,
	    &wPreferences.key.windowshortcut3, getKeybind, setKeyGrab_windowshortcut3, NULL, NULL, 0},
	{"WindowShortcut4Key", "None", NULL,
	    &wPreferences.key.windowshortcut4, getKeybind, setKeyGrab_windowshortcut4, NULL, NULL, 0},
	{"WindowShortcut5Key", "None", NULL,
	    &wPreferences.key.windowshortcut5, getKeybind, setKeyGrab_windowshortcut5, NULL, NULL, 0},
	{"WindowShortcut6Key", "None", NULL,
	    &wPreferences.key.windowshortcut6, getKeybind, setKeyGrab_windowshortcut6, NULL, NULL, 0},
	{"WindowShortcut7Key", "None", NULL,
	    &wPreferences.key.windowshortcut7, getKeybind, setKeyGrab_windowshortcut7, NULL, NULL, 0},
	{"WindowShortcut8Key", "None", NULL,
	    &wPreferences.key.windowshortcut8, getKeybind, setKeyGrab_windowshortcut8, NULL, NULL, 0},
	{"WindowShortcut9Key", "None", NULL,
	    &wPreferences.key.windowshortcut9, getKeybind, setKeyGrab_windowshortcut9, NULL, NULL, 0},
	{"WindowShortcut10Key", "None", NULL,
	    &wPreferences.key.windowshortcut10, getKeybind, setKeyGrab_windowshortcut10, NULL, NULL, 0},
	{"MoveTo12to6Head", "None", NULL,
	    &wPreferences.key.moveto12to6head, getKeybind, setKeyGrab_moveto12to6head, NULL, NULL, 0},
	{"MoveTo6to12Head", "None", NULL,
	    &wPreferences.key.moveto6to12head, getKeybind, setKeyGrab_moveto6to12head, NULL, NULL, 0},
	{"WindowRelaunchKey", "None", NULL,
	    &wPreferences.key.windowrelaunch, getKeybind, setKeyGrab_windowrelaunch, NULL, NULL, 0},
	{"ScreenSwitchKey", "None", NULL,
	    &wPreferences.key.screenswitch, getKeybind, setKeyGrab_screenswitch, NULL, NULL, 0},
	{"RunKey", "None", NULL,
	    &wPreferences.key.run, getKeybind, setKeyGrab_run, NULL, NULL, 0},

#ifdef KEEP_XKB_LOCK_STATUS
	{"ToggleKbdModeKey", "None", NULL,
	    &wPreferences.key.togglekbdmode, getKeybind, setKeyGrab_toggle, NULL, NULL, 0},
	{"KbdModeLock", "NO", NULL,
	    &wPreferences.modelock, getBool, NULL, NULL, NULL, 0},
#endif				/* KEEP_XKB_LOCK_STATUS */

	{"NormalCursor", "(builtin, left_ptr)", NULL,
	    &wPreferences.cursors.root, getCursor, setCursor_root, NULL, NULL, 0},
	{"ArrowCursor", "(builtin, top_left_arrow)", NULL,
	    &wPreferences.cursors.arrow, getCursor, setCursor_arrow, NULL, NULL, 0},
	{"MoveCursor", "(builtin, fleur)", NULL,
	    &wPreferences.cursors.move, getCursor, setCursor_move, NULL, NULL, 0},
	{"ResizeCursor", "(builtin, sizing)", NULL,
	    &wPreferences.cursors.resize, getCursor, setCursor_resize, NULL, NULL, 0},
	{"TopLeftResizeCursor", "(builtin, top_left_corner)", NULL,
	    &wPreferences.cursors.resizetopleft, getCursor, setCursor_topleftresize, NULL, NULL, 0},
	{"TopRightResizeCursor", "(builtin, top_right_corner)", NULL,
	    &wPreferences.cursors.resizetopright, getCursor, setCursor_toprightresize, NULL, NULL, 0},
	{"BottomLeftResizeCursor", "(builtin, bottom_left_corner)", NULL,
	    &wPreferences.cursors.resizebottomleft, getCursor, setCursor_bottomleftresize, NULL, NULL, 0},
	{"BottomRightResizeCursor", "(builtin, bottom_right_corner)", NULL,
	    &wPreferences.cursors.resizebottomright, getCursor, setCursor_bottomrightresize, NULL, NULL, 0},
	{"VerticalResizeCursor", "(builtin, sb_v_double_arrow)", NULL,
	    &wPreferences.cursors.resizevertical, getCursor, setCursor_verticalresize, NULL, NULL, 0},
	{"HorizontalResizeCursor", "(builtin, sb_h_double_arrow)", NULL,
	    &wPreferences.cursors.resizehorizontal, getCursor, setCursor_horizontalresize, NULL, NULL, 0},
	{"WaitCursor", "(builtin, watch)", NULL,
	    &wPreferences.cursors.wait, getCursor, setCursor_wait, NULL, NULL, 0},
	{"QuestionCursor", "(builtin, question_arrow)", NULL,
	    &wPreferences.cursors.question, getCursor, setCursor_question, NULL, NULL, 0},
	{"TextCursor", "(builtin, xterm)", NULL,
	    &wPreferences.cursors.text, getCursor, setCursor_text, NULL, NULL, 0},
	{"SelectCursor", "(builtin, cross)", NULL,
	    &wPreferences.cursors.select, getCursor, setCursor_select, NULL, NULL, 0},
	{"DialogHistoryLines", "500", NULL,
	    &wPreferences.history_lines, getInt, NULL, NULL, NULL, 0},
	{"CycleActiveHeadOnly", "NO", NULL,
	    &wPreferences.cycle_active_head_only, getBool, NULL, NULL, NULL, 0},
	{"CycleIgnoreMinimized", "NO", NULL,
	    &wPreferences.cycle_ignore_minimized, getBool, NULL, NULL, NULL, 0}
};

static void init_defaults(void);
static void wReadStaticDefaults(WMPropList *dict);
static void wReadStaticDefaults_update(void);
static void wDefaultsMergeGlobalMenus(WDDomain *menuDomain);
static void wDefaultUpdateIcons(virtual_screen *vscr);
static WDDomain *wDefaultsInitDomain(const char *domain, Bool requireDictionary);
static void backimage_launch_helper(virtual_screen *vscr, WMPropList *value);
static unsigned int default_update(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *plvalue);

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
	wReadStaticDefaults_update();

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
			(*entry->convert) (entry, plvalue, entry->addr);
			entry->refresh = 1;
		}
	}
}

static void wReadStaticDefaults_update(void)
{
	WDefaultEntry *entry;
	unsigned int i;

	for (i = 0; i < wlengthof(staticOptionList); i++) {
		entry = &staticOptionList[i];

		if (entry->update && entry->refresh)
			(*entry->update) (NULL);

		entry->refresh = 0;
	}
}

void set_defaults_global(WMPropList *new_dict)
{
	unsigned int i;
	WMPropList *plvalue;
	WDefaultEntry *entry;

	for (i = 0; i < wlengthof(optionList); i++) {
		entry = &optionList[i];

		plvalue = WMGetFromPLDictionary(new_dict, entry->plkey);
		if (!plvalue) {
			/* no default in  the DB. Use builtin default */
			plvalue = entry->plvalue;
			if (plvalue && new_dict)
				WMPutInPLDictionary(new_dict, entry->plkey, plvalue);
		}

		/* convert data */
		(*entry->convert) (entry, plvalue, entry->addr);
	}
}

unsigned int set_defaults_virtual_screen(virtual_screen *vscr)
{
	unsigned int i, needs_refresh = 0;
	WDefaultEntry *entry;

	for (i = 0; i < wlengthof(optionList); i++) {
		entry = &optionList[i];

		if (entry->update)
			needs_refresh |= (*entry->update) (vscr);
	}

	return needs_refresh;
}

static unsigned int read_defaults_step1(virtual_screen *vscr, WMPropList *new_dict)
{
	unsigned int i, needs_refresh = 0;
	WMPropList *plvalue, *old_value, *old_dict = NULL;
	WDefaultEntry *entry;

	vscr->screen_ptr->flags.update_workspace_back = 0;

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

			needs_refresh |= default_update(vscr, entry, plvalue);
		} else if (!plvalue) {
			/* value was deleted from DB. Keep current value */
		} else if (!old_value) {
			/* set value for the 1st time */
			needs_refresh |= default_update(vscr, entry, plvalue);
		} else if (!WMIsPropListEqualTo(plvalue, old_value)) {
			/* value has changed */
			needs_refresh |= default_update(vscr, entry, plvalue);
		} else {
			/* Value was not changed since last time.
			 * We must continue, except if WorkspaceSpecificBack
			 * was updated previously
			 */
			if (strcmp(entry->key, "WorkspaceBack") == 0 &&
			    vscr->screen_ptr->flags.update_workspace_back &&
			    vscr->screen_ptr->flags.backimage_helper_launched) {
				needs_refresh |= default_update(vscr, entry, plvalue);
			}
		}
	}

	vscr->screen_ptr->flags.update_workspace_back = 0;
	return needs_refresh;
}

static unsigned int default_update(virtual_screen *vscr, WDefaultEntry *entry, WMPropList *plvalue)
{
	unsigned int needs_refresh = 0;
	int ret;

	if (!plvalue)
		return 0;

	/* convert data */
	ret = (*entry->convert) (entry, plvalue, entry->addr);
	entry->refresh = ret;
	if (!ret)
		return 0;

	/*
	 * If the WorkspaceSpecificBack data has been changed
	 * so that the helper will be launched now, we must be
	 * sure to send the default background texture config
	 * to the helper.
	 */
	if (strcmp(entry->key, "WorkspaceSpecificBack") == 0 &&
	    !vscr->screen_ptr->flags.backimage_helper_launched)
		vscr->screen_ptr->flags.update_workspace_back = 1;

	if (entry->refresh && entry->update) {
		needs_refresh = (*entry->update) (vscr);
		entry->refresh = 0;
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
static int getBool(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	static char data;
	const char *val;
	int second_pass = 0;

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

	if (addr)
		*(char *)addr = data;

	return True;
}

static int getInt(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	static int data;
	const char *val;

	GET_STRING_OR_DEFAULT("Integer", val);

	if (sscanf(val, "%i", &data) != 1) {
		wwarning(_("can't convert \"%s\" to integer for key \"%s\""), val, entry->key);
		val = WMGetFromPLString(entry->plvalue);
		wwarning(_("using default \"%s\" instead"), val);
		if (sscanf(val, "%i", &data) != 1) {
			return False;
		}
	}

	if (addr)
		*(int *)addr = data;

	return True;
}

static int getCoord(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	static WCoord data;
	char *val_x, *val_y;
	int nelem, changed = 0;
	WMPropList *elem_x, *elem_y;

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

	if (addr)
		*(WCoord *) addr = data;

	return True;
}

static int getPropList(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;
	(void) addr;

	WMRetainPropList(value);

	*(WMPropList **) addr = value;

	return True;
}

static int getPathList(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	static char *data;
	int i, count, len;
	char *ptr;
	WMPropList *d;
	int changed = 0;

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

static int getEnum(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	static signed char data;

	data = string2index(entry->plkey, value, entry->default_value, (WOptionEnumeration *) entry->extra_data);
	if (data < 0)
		return False;

	if (addr)
		*(signed char *)addr = data;

	return True;
}

static int getTexture(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	defstructpl *defstruct;
	WMPropList *name, *defname;
	int len;
	char *key;

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

	return True;
}

static int getWSBackground(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	WMPropList *elem;
	int changed = 0;
	char *val;
	int nelem;

	/* Parameter not used, but tell the compiler that it is ok */
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

	WMRetainPropList(value);

	*(WMPropList **) addr = value;

	return True;
}

static int
getWSSpecificBackground(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	WMPropList *elem;
	int nelem;
	int changed = 0;

	/* Parameter not used, but tell the compiler that it is ok */
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

	WMRetainPropList(value);

	*(WMPropList **) addr = value;

	return True;
}

static int getFont(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	const char *val;
	char *fontname;
	int len;

	(void) addr;

	GET_STRING_OR_DEFAULT("Font", val);
	len = sizeof(char *) * (strlen(val));

	fontname = wmalloc(len + sizeof(char *));
	snprintf(fontname, len, "%s", val);

	if (addr)
		*(char **)addr = fontname;

	return True;
}

static int getColor(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	const char *val;
	int len, def_len;
	defstruct *color;
	char *colorname, *def_colorname;

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

	return True;
}

static int getKeybind(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	const char *val;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) addr;

	GET_STRING_OR_DEFAULT("Key spec", val);

	if (addr)
		wstrlcpy(addr, val, MAX_SHORTCUT_LENGTH);

	return True;
}

static int getModMask(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	static int mask;
	const char *str;

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

static int getCursor(WDefaultEntry *entry, WMPropList *value, void *addr)
{
	defstructpl *defstruct;
	WMPropList *cursorname, *defcursorname;

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

	return True;
}

#undef CURSOR_ID_NONE

/* ---------------- value setting functions --------------- */
static int setJustify(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return REFRESH_WINDOW_TITLE_COLOR;
}

static int setClearance(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return REFRESH_WINDOW_FONT | REFRESH_BUTTON_IMAGES | REFRESH_MENU_TITLE_FONT | REFRESH_MENU_FONT;
}

static int setIfDockPresent(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	wPreferences.flags.nodrawer = wPreferences.flags.nodrawer || wPreferences.flags.nodock;

	return 0;
}

static int setIfClipPresent(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return 0;
}

static int setIfDrawerPresent(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return 0;
}

static int setClipMergedInDock(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	wPreferences.flags.noclip = wPreferences.flags.noclip || wPreferences.flags.clip_merged_in_dock;
	return 0;
}

static int setWrapAppiconsInDock(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return 0;
}

static int setStickyIcons(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return REFRESH_STICKY_ICONS;
}

static int setIconTile(virtual_screen *vscr)
{
	RImage *img;
	WTexture *texture = NULL;
	int reset = 0;

	texture = get_texture_from_defstruct(vscr, wPreferences.texture.iconback);

	img = wTextureRenderImage(texture, wPreferences.icon_size,
				  wPreferences.icon_size, (texture->any.type & WREL_BORDER_MASK)
				  ? WREL_ICON : WREL_FLAT);
	if (!img) {
		wwarning(_("could not render texture for icon background"));
		return 0;
	}

	if (w_global.tile.icon) {
		reset = 1;
		RReleaseImage(w_global.tile.icon);
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

	if (vscr->screen_ptr->def_icon_rimage) {
		RReleaseImage(vscr->screen_ptr->def_icon_rimage);
		vscr->screen_ptr->def_icon_rimage = NULL;
	}

	if (vscr->screen_ptr->icon_back_texture)
		wTextureDestroy(vscr, (WTexture *) vscr->screen_ptr->icon_back_texture);

	vscr->screen_ptr->icon_back_texture = wTextureMakeSolid(vscr, &(texture->any.color));

	return (reset ? REFRESH_ICON_TILE : 0);
}

static int setWinTitleFont(virtual_screen *vscr)
{
	WMFont *font = NULL;
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

static int setMenuTitleFont(virtual_screen *vscr)
{
	WMFont *font = NULL;
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

static int setMenuTextFont(virtual_screen *vscr)
{
	WMFont *font = NULL;
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

static int setIconTitleFont(virtual_screen *vscr)
{
	WMFont *font = NULL;
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

static int setClipTitleFont(virtual_screen *vscr)
{
	WMFont *font = NULL;
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

static int setLargeDisplayFont(virtual_screen *vscr)
{
	WMFont *font = NULL;
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

static int setHightlight(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.highlight->value, color)) {
		wwarning(_("could not get color for key HighlightColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.highlight->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.highlight->defvalue, color)) {
			wwarning(_("could not get color for key HighlightColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->select_color)
		WMReleaseColor(vscr->screen_ptr->select_color);

	vscr->screen_ptr->select_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_MENU_COLOR;
}

static int setHightlightText(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.highlighttext->value, color)) {
		wwarning(_("could not get color for key HighlightTextColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.highlighttext->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.highlighttext->defvalue, color)) {
			wwarning(_("could not get color for key HighlightTextColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->select_text_color)
		WMReleaseColor(vscr->screen_ptr->select_text_color);

	vscr->screen_ptr->select_text_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_MENU_COLOR;
}

static int setClipTitleColor(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.cliptitle->value, color)) {
		wwarning(_("could not get color for key ClipTitleColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.cliptitle->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.cliptitle->defvalue, color)) {
			wwarning(_("could not get color for key ClipTitleColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->clip_title_color[CLIP_NORMAL])
		WMReleaseColor(vscr->screen_ptr->clip_title_color[CLIP_NORMAL]);

	vscr->screen_ptr->clip_title_color[CLIP_NORMAL] = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);
	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_ICON_TITLE_COLOR;
}

static int setClipTitleColorCollapsed(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.cliptitlecollapsed->value, color)) {
		wwarning(_("could not get color for key CClipTitleColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.cliptitlecollapsed->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.cliptitlecollapsed->defvalue, color)) {
			wwarning(_("could not get color for key CClipTitleColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->clip_title_color[CLIP_COLLAPSED])
		WMReleaseColor(vscr->screen_ptr->clip_title_color[CLIP_COLLAPSED]);

	vscr->screen_ptr->clip_title_color[CLIP_COLLAPSED] = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);
	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_ICON_TITLE_COLOR;
}

static int setWTitleColorFocused(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.titlefocused->value, color)) {
		wwarning(_("could not get color for key FTitleColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.titlefocused->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.titlefocused->defvalue, color)) {
			wwarning(_("could not get color for key FTitleColor"));
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

static int setWTitleColorOwner(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.titleowner->value, color)) {
		wwarning(_("could not get color for key PTitleColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.titleowner->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.titleowner->defvalue, color)) {
			wwarning(_("could not get color for key PTitleColor"));
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

static int setWTitleColorUnfocused(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.titleunfocused->value, color)) {
		wwarning(_("could not get color for key UTitleColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.titleunfocused->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.titleunfocused->defvalue, color)) {
			wwarning(_("could not get color for key UTitleColor"));
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

static int setMenuTitleColor(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.menutitle->value, color)) {
		wwarning(_("could not get color for key MenuTitleColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.menutitle->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.menutitle->defvalue, color)) {
			wwarning(_("could not get color for key MenuTitleColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->menu_title_color[0])
		WMReleaseColor(vscr->screen_ptr->menu_title_color[0]);

	vscr->screen_ptr->menu_title_color[0] = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_MENU_TITLE_COLOR;
}

static int setMenuTextColor(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.menutext->value, color)) {
		wwarning(_("could not get color for key MenuTextColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.menutext->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.menutext->defvalue, color)) {
			wwarning(_("could not get color for key MenuTextColor"));
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

static int setMenuDisabledColor(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.menudisabled->value, color)) {
		wwarning(_("could not get color for key MenuDisabledColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.menudisabled->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.menudisabled->defvalue, color)) {
			wwarning(_("could not get color for key MenuDisabledColor"));
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

static int setIconTitleColor(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.icontitle->value, color)) {
		wwarning(_("could not get color for key IconTitleColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.icontitle->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.icontitle->defvalue, color)) {
			wwarning(_("could not get color for key IconTitleColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->icon_title_color)
		WMReleaseColor(vscr->screen_ptr->icon_title_color);

	vscr->screen_ptr->icon_title_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_ICON_TITLE_COLOR;
}

static int setIconTitleBack(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.icontitleback->value, color)) {
		wwarning(_("could not get color for key IconTitleBack"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.icontitleback->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.icontitleback->defvalue, color)) {
			wwarning(_("could not get color for key IconTitleBack"));
			return False;
		}
	}

	if (vscr->screen_ptr->icon_title_texture)
		wTextureDestroy(vscr, (WTexture *) vscr->screen_ptr->icon_title_texture);

	vscr->screen_ptr->icon_title_texture = wTextureMakeSolid(vscr, color);

	return REFRESH_ICON_TITLE_BACK;
}

static int setFrameBorderWidth(virtual_screen *vscr)
{
	vscr->frame.border_width = wPreferences.border_width;

	return REFRESH_FRAME_BORDER;
}

static int setFrameBorderColor(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborder->value, color)) {
		wwarning(_("could not get color for key FrameBorderColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.frameborder->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborder->defvalue, color)) {
			wwarning(_("could not get color for key FrameBorderColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->frame_border_color)
		WMReleaseColor(vscr->screen_ptr->frame_border_color);

	vscr->screen_ptr->frame_border_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_FRAME_BORDER;
}

static int setFrameFocusedBorderColor(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborderfocused->value, color)) {
		wwarning(_("could not get color for key FrameFocusedBorderColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.frameborderfocused->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborderfocused->defvalue, color)) {
			wwarning(_("could not get color for key FrameFocusedBorderColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->frame_focused_border_color)
		WMReleaseColor(vscr->screen_ptr->frame_focused_border_color);

	vscr->screen_ptr->frame_focused_border_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_FRAME_BORDER;
}

static int setFrameSelectedBorderColor(virtual_screen *vscr)
{
	XColor clr, *color = NULL;
	color = &clr;

	if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborderselected->value, color)) {
		wwarning(_("could not get color for key FrameSelectedBorderColor"));
		wwarning(_("using default \"%s\" instead"), wPreferences.color.frameborderselected->defvalue);
		if (!wGetColor(vscr->screen_ptr, wPreferences.color.frameborderselected->defvalue, color)) {
			wwarning(_("could not get color for key FrameSelectedBorderColor"));
			return False;
		}
	}

	if (vscr->screen_ptr->frame_selected_border_color)
		WMReleaseColor(vscr->screen_ptr->frame_selected_border_color);

	vscr->screen_ptr->frame_selected_border_color = WMCreateRGBColor(vscr->screen_ptr->wmscreen, color->red, color->green, color->blue, True);

	wFreeColor(vscr->screen_ptr, color->pixel);

	return REFRESH_FRAME_BORDER;
}

static int set_workspace_back(virtual_screen *vscr, int opt)
{
	WMPropList *value;
	WMPropList *val;
	char *str;
	int i;

	if (opt == 0) {
		value = wPreferences.workspacespecificback;

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
	}

	if (opt == 1) {
		value = wPreferences.workspaceback;

		if (vscr->screen_ptr->flags.backimage_helper_launched) {
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
			backimage_launch_helper(vscr, value);
		}

		WMReleasePropList(value);
	}

	return 0;
}


static int setWorkspaceSpecificBack(virtual_screen *vscr)
{
	return set_workspace_back(vscr, 0);
}

static void backimage_launch_helper(virtual_screen *vscr, WMPropList *value)
{
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

static int setWorkspaceBack(virtual_screen *vscr)
{
	return set_workspace_back(vscr, 1);
}

static int setWidgetColor(virtual_screen *vscr)
{
	WTexture *texture = get_texture_from_defstruct(vscr, wPreferences.texture.widgetcolor);

	if (vscr->screen_ptr->widget_texture)
		wTextureDestroy(vscr, (WTexture *) vscr->screen_ptr->widget_texture);

	vscr->screen_ptr->widget_texture = *(WTexSolid **) texture;

	return 0;
}

static int setFTitleBack(virtual_screen *vscr)
{
	WTexture *texture = get_texture_from_defstruct(vscr, wPreferences.texture.titlebackfocused);

	if (vscr->screen_ptr->window_title_texture[WS_FOCUSED])
		wTextureDestroy(vscr, vscr->screen_ptr->window_title_texture[WS_FOCUSED]);

	vscr->screen_ptr->window_title_texture[WS_FOCUSED] = texture;

	return REFRESH_WINDOW_TEXTURES;
}

static int setPTitleBack(virtual_screen *vscr)
{
	WTexture *texture = get_texture_from_defstruct(vscr, wPreferences.texture.titlebackowner);

	if (vscr->screen_ptr->window_title_texture[WS_PFOCUSED])
		wTextureDestroy(vscr, vscr->screen_ptr->window_title_texture[WS_PFOCUSED]);

	vscr->screen_ptr->window_title_texture[WS_PFOCUSED] = texture;

	return REFRESH_WINDOW_TEXTURES;
}

static int setUTitleBack(virtual_screen *vscr)
{
	WTexture *texture = get_texture_from_defstruct(vscr, wPreferences.texture.titlebackunfocused);

	if (vscr->screen_ptr->window_title_texture[WS_UNFOCUSED])
		wTextureDestroy(vscr, vscr->screen_ptr->window_title_texture[WS_UNFOCUSED]);

	vscr->screen_ptr->window_title_texture[WS_UNFOCUSED] = texture;

	return REFRESH_WINDOW_TEXTURES;
}

static int setResizebarBack(virtual_screen *vscr)
{
	WTexture *texture = get_texture_from_defstruct(vscr, wPreferences.texture.resizebarback);

	if (vscr->screen_ptr->resizebar_texture[0])
		wTextureDestroy(vscr, vscr->screen_ptr->resizebar_texture[0]);

	vscr->screen_ptr->resizebar_texture[0] = texture;

	return REFRESH_WINDOW_TEXTURES;
}

static int setMenuTitleBack(virtual_screen *vscr)
{
	WTexture *texture = get_texture_from_defstruct(vscr, wPreferences.texture.menutitleback);

	if (vscr->screen_ptr->menu_title_texture[0])
		wTextureDestroy(vscr, vscr->screen_ptr->menu_title_texture[0]);

	vscr->screen_ptr->menu_title_texture[0] = texture;

	return REFRESH_MENU_TITLE_TEXTURE;
}

static int setMenuTextBack(virtual_screen *vscr)
{
	WTexture *texture = get_texture_from_defstruct(vscr, wPreferences.texture.menutextback);

	if (vscr->screen_ptr->menu_item_texture) {
		wTextureDestroy(vscr, vscr->screen_ptr->menu_item_texture);
		wTextureDestroy(vscr, (WTexture *) vscr->screen_ptr->menu_item_auxtexture);
	}

	vscr->screen_ptr->menu_item_texture = texture;
	vscr->screen_ptr->menu_item_auxtexture = wTextureMakeSolid(vscr, &vscr->screen_ptr->menu_item_texture->any.color);

	return REFRESH_MENU_TEXTURE;
}

static void set_keygrab(WShortKey *shortcut, char *value)
{
	char buf[MAX_SHORTCUT_LENGTH];
	KeySym ksym;
	char *k, *b;
	int mod, error = 0;

	wstrlcpy(buf, value, MAX_SHORTCUT_LENGTH);

	if ((strlen(value) == 0) || (strcasecmp(value, "NONE") == 0)) {
		shortcut->keycode = 0;
		shortcut->modifier = 0;
	} else {

		b = buf;

		/* get modifiers */
		shortcut->modifier = 0;
		while ((!error) && ((k = strchr(b, '+')) != NULL)) {
			*k = 0;
			mod = wXModifierFromKey(b);
			if (mod < 0) {
				wwarning(_("Invalid key modifier \"%s\""), b);
				error = 1;
			}
			shortcut->modifier |= mod;

			b = k + 1;
		}

		if (!error) {
			/* get key */
			ksym = XStringToKeysym(b);

			if (ksym == NoSymbol) {
				wwarning(_("Invalid kbd shortcut specification \"%s\""), value);
				error = 1;
			}

			if (!error) {
				shortcut->keycode = XKeysymToKeycode(dpy, ksym);
				if (shortcut->keycode == 0) {
					wwarning(_("Invalid key in shortcut \"%s\""), value);
					error = 1;
				}
			}
		}
	}
}

static int setKeyGrab_rootmenu(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.rootmenu;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_ROOTMENU] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowlist(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowlist;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOWLIST] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowmenu(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowmenu;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOWMENU] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_dockraiselower(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.dockraiselower;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_DOCKRAISELOWER] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_clipraiselower(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.clipraiselower;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_CLIPRAISELOWER] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_miniaturize(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.miniaturize;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MINIATURIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_minimizeall(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.minimizeall;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MINIMIZEALL] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_hide(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.hide;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_HIDE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_hideothers(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.hideothers;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_HIDE_OTHERS] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_moveresize(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.moveresize;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVERESIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_close(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.close;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_CLOSE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximize(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximize;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizev(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizev;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_VMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizeh(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizeh;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_HMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizelh(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizelh;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_LHMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizerh(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizerh;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_RHMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizeth(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizeth;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_THMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizebh(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizebh;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_BHMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizeltc(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizeltc;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_LTCMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizertc(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizertc;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_RTCMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizelbc(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizelbc;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_LBCMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximizerbc(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximizerbc;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_RBCMAXIMIZE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_maximus(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.maximus;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MAXIMUS] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_keepontop(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.keepontop;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_KEEP_ON_TOP] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_keepatbottom(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.keepatbottom;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_KEEP_AT_BOTTOM] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_omnipresent(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.omnipresent;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_OMNIPRESENT] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_raise(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.raise;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_RAISE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_lower(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.lower;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_LOWER] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_raiselower(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.raiselower;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_RAISELOWER] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_shade(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.shade;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_SHADE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_select(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.select;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_SELECT] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_workspacemap(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspacemap;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACEMAP] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_focusnext(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.focusnext;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_FOCUSNEXT] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_focusprev(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.focusprev;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_FOCUSPREV] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_groupnext(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.groupnext;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_GROUPNEXT] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_groupprev(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.groupprev;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_GROUPPREV] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_workspacenext(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspacenext;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_NEXTWORKSPACE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_workspaceprev(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspaceprev;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_PREVWORKSPACE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_workspacelast(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspacelast;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_LASTWORKSPACE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	/* Refresh Workspace Menu, if opened */
	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspacelayernext(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspacelayernext;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_NEXTWSLAYER] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_workspacelayerprev(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspacelayerprev;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_PREVWSLAYER] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_workspace1(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace1;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE1] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace2(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace2;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE2] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace3(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace3;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE3] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace4(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace4;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE4] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace5(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace5;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE5] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace6(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace6;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE6] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace7(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace7;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE7] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace8(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace8;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE8] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace9(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace9;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE9] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_workspace10(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.workspace10;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WORKSPACE10] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace1(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace1;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE1] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace2(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace2;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE2] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace3(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace3;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE3] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace4(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace4;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE4] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace5(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace5;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE5] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace6(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace6;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE6] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace7(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace7;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE7] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace8(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace8;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE8] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace9(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace9;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE9] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetoworkspace10(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoworkspace10;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_WORKSPACE10] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return REFRESH_WORKSPACE_MENU;
}

static int setKeyGrab_movetonextworkspace(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetonextworkspace;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_NEXTWORKSPACE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_movetoprevworkspace(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoprevworkspace;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_PREVWORKSPACE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_movetolastworkspace(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetolastworkspace;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_LASTWORKSPACE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_movetonextworkspacelayer(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetonextworkspace;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_NEXTWSLAYER] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_movetoprevworkspacelayer(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.movetoprevworkspace;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_PREVWSLAYER] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut1(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut1;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW1] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut2(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut2;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW2] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut3(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut3;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW3] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut4(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut4;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW4] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut5(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut5;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW5] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut6(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut6;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW6] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut7(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut7;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW7] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut8(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut8;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW8] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut9(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut9;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW9] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowshortcut10(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut10;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_WINDOW10] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_moveto12to6head(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut10;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_12_TO_6_HEAD] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_moveto6to12head(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowshortcut10;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_MOVE_6_TO_12_HEAD] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_windowrelaunch(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.windowrelaunch;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_RELAUNCH] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_screenswitch(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.screenswitch;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_SWITCH_SCREEN] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

static int setKeyGrab_run(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.run;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_RUN] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}

#ifdef KEEP_XKB_LOCK_STATUS
static int setKeyGrab_togglekbdmode(virtual_screen *vscr)
{
	WShortKey shortcut;
	WWindow *wwin;
	char *value;

	value = wPreferences.key.togglekbdmode;

	set_keygrab(&shortcut, value);
	wKeyBindings[WKBD_TOGGLE] = shortcut;
	wwin = vscr->window.focused;

	while (wwin != NULL) {
		XUngrabKey(dpy, AnyKey, AnyModifier, wwin->frame->core->window);
		if (!WFLAGP(wwin, no_bind_keys))
			wWindowSetKeyGrabs(wwin);

		wwin = wwin->prev;
	}

	return 0;
}
#endif

static int setIconPosition(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return REFRESH_ARRANGE_ICONS;
}

static int updateUsableArea(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return REFRESH_USABLE_AREA;
}

static int setWorkspaceMapBackground(virtual_screen *vscr)
{
	WTexture *texture = get_texture_from_defstruct(vscr, wPreferences.texture.workspacemapback);

	if (wPreferences.wsmbackTexture)
		wTextureDestroy(vscr, wPreferences.wsmbackTexture);

	wPreferences.wsmbackTexture = texture;

	return REFRESH_WINDOW_TEXTURES;
}


static int setMenuStyle(virtual_screen *vscr)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) vscr;

	return REFRESH_MENU_TEXTURE;
}

static RImage *chopOffImage(RImage *image, int x, int y, int w, int h)
{
	RImage *img = RCreateImage(w, h, image->format == RRGBAFormat);

	RCopyArea(img, image, x, y, w, h, 0, 0);

	return img;
}

static int setSwPOptions(virtual_screen *vscr)
{
	WMPropList *array;
	char *path;
	RImage *bgimage;
	int cwidth, cheight;

	array = wPreferences.sp_options;

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
			wwarning(_("Invalid arguments for option SwitchPanelImages"));
			break;
		} else
			path = FindImage(wPreferences.pixmap_path, WMGetFromPLString(WMGetFromPLArray(array, 1)));

		if (!path) {
			wwarning(_("Could not find image \"%s\" for option SwitchPanelImages"),
				 WMGetFromPLString(WMGetFromPLArray(array, 1)));
		} else {
			bgimage = RLoadImage(vscr->screen_ptr->rcontext, path, 0);
			if (!bgimage) {
				wwarning(_("Could not load image \"%s\" for option SwitchPanelImages"), path);
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
			wwarning(_("Invalid arguments for option SwitchPanelImages"));
			break;
		} else {
			path = FindImage(wPreferences.pixmap_path, WMGetFromPLString(WMGetFromPLArray(array, 0)));
		}

		if (!path) {
			wwarning(_("Could not find image \"%s\" for option SwitchPanelImages"),
				 WMGetFromPLString(WMGetFromPLArray(array, 0)));
		} else {
			if (wPreferences.swtileImage)
				RReleaseImage(wPreferences.swtileImage);

			wPreferences.swtileImage = RLoadImage(vscr->screen_ptr->rcontext, path, 0);
			if (!wPreferences.swtileImage)
				wwarning(_("Could not load image \"%s\" for option SwitchPanelImages"), path);

			wfree(path);
		}
		break;

	default:
		wwarning(_("Invalid number of arguments for option SwitchPanelImages"));
		break;
	}

	WMReleasePropList(array);

	return 0;
}

static int setModifierKeyLabels(virtual_screen *vscr)
{
	WMPropList *array;
	int i;

	array = wPreferences.modifierkeylabels;

	if (!WMIsPLArray(array) || WMGetPropListItemCount(array) != 7) {
		wwarning(_("Value for option SwitchPanelImages must be an array of 7 strings"));
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
			wwarning(_("Invalid argument for option ModifierKeyLabels item %d"), i);
			wPreferences.modifier_labels[i] = NULL;
		}
	}

	WMReleasePropList(array);

	return 0;
}

static int setDoubleClick(virtual_screen *vscr)
{
	(void) vscr;

	if (wPreferences.dblclick_time <= 0)
		wPreferences.dblclick_time = 1;

	W_setconf_doubleClickDelay(wPreferences.dblclick_time);

	return 0;
}

static int setCursor_root(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.root->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.root->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_ROOT] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_ROOT]);

	wPreferences.cursor[WCUR_ROOT] = cursor;

	if (cursor != None)
		XDefineCursor(dpy, vscr->screen_ptr->root_win, cursor);

	return 0;
}

static int setCursor_move(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.move->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.move->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_MOVE] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_MOVE]);

	wPreferences.cursor[WCUR_MOVE] = cursor;
	return 0;
}

static int setCursor_resize(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.resize->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.resize->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_RESIZE] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_RESIZE]);

	wPreferences.cursor[WCUR_RESIZE] = cursor;
	return 0;
}

static int setCursor_topleftresize(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.resizetopleft->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.resizetopleft->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_TOPLEFTRESIZE] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_TOPLEFTRESIZE]);

	wPreferences.cursor[WCUR_TOPLEFTRESIZE] = cursor;
	return 0;
}

static int setCursor_toprightresize(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.resizetopright->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.resizetopright->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_TOPRIGHTRESIZE] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_TOPRIGHTRESIZE]);

	wPreferences.cursor[WCUR_TOPRIGHTRESIZE] = cursor;
	return 0;
}

static int setCursor_bottomleftresize(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.resizebottomleft->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.resizebottomleft->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_BOTTOMLEFTRESIZE] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_BOTTOMLEFTRESIZE]);

	wPreferences.cursor[WCUR_BOTTOMLEFTRESIZE] = cursor;
	return 0;
}

static int setCursor_bottomrightresize(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.resizebottomright->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.resizebottomright->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_BOTTOMRIGHTRESIZE] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_BOTTOMRIGHTRESIZE]);

	wPreferences.cursor[WCUR_BOTTOMRIGHTRESIZE] = cursor;
	return 0;
}

static int setCursor_horizontalresize(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.resizehorizontal->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.resizehorizontal->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_HORIZONRESIZE] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_HORIZONRESIZE]);

	wPreferences.cursor[WCUR_HORIZONRESIZE] = cursor;
	return 0;
}

static int setCursor_verticalresize(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.resizevertical->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.resizevertical->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_VERTICALRESIZE] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_VERTICALRESIZE]);

	wPreferences.cursor[WCUR_VERTICALRESIZE] = cursor;
	return 0;
}

static int setCursor_wait(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.wait->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.wait->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_WAIT] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_WAIT]);

	wPreferences.cursor[WCUR_WAIT] = cursor;
	return 0;
}

static int setCursor_arrow(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.arrow->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.arrow->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_ARROW] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_ARROW]);

	wPreferences.cursor[WCUR_ARROW] = cursor;
	return 0;
}

static int setCursor_question(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.question->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.question->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_QUESTION] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_QUESTION]);

	wPreferences.cursor[WCUR_QUESTION] = cursor;
	return 0;
}

static int setCursor_text(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.text->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.text->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_TEXT] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_TEXT]);

	wPreferences.cursor[WCUR_TEXT] = cursor;
	return 0;
}

static int setCursor_select(virtual_screen *vscr)
{
	WMPropList *value = NULL;
	Cursor cursor;
	int status;

	value = wPreferences.cursors.select->value;
	status = parse_cursor(vscr, value, &cursor);
	if (!status) {
		wwarning(_("Error in cursor specification. using default instead"));
		value = wPreferences.cursors.select->defvalue;
		status = parse_cursor(vscr, value, &cursor);
	}

	if (wPreferences.cursor[WCUR_SELECT] != None)
		XFreeCursor(dpy, wPreferences.cursor[WCUR_SELECT]);

	wPreferences.cursor[WCUR_SELECT] = cursor;
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
