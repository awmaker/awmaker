/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef WMGNUSTEP_H_
#define WMGNUSTEP_H_

#include <X11/Xproto.h>


#define GNUSTEP_WM_MINIATURIZE_WINDOW "_GNUSTEP_WM_MINIATURIZE_WINDOW"

#define GNUSTEP_WM_ATTR_NAME  "_GNUSTEP_WM_ATTR"

#define	GNUSTEP_TITLEBAR_STATE 	"_GNUSTEP_TITLEBAR_STATE"
enum {
    WMTitleBarKey = 0,
    WMTitleBarNormal = 1,
    WMTitleBarMain = 2
};

#ifndef _DEFINED_GNUSTEP_WINDOW_INFO
#define	_DEFINED_GNUSTEP_WINDOW_INFO
/*
 * Window levels are taken from GNUstep (gui/AppKit/NSWindow.h)
 * NSDesktopWindowLevel intended to be the level at which things
 * on the desktop sit ... so you should be able
 * to put a desktop background just below it.
 *
 * Applications are actually permitted to use any value in the
 * range INT_MIN+1 to INT_MAX
 */
enum {
    WMDesktopWindowLevel = -1000, /* GNUstep addition     */
    WMNormalWindowLevel = 0,
    WMFloatingWindowLevel = 3,
    WMSubmenuWindowLevel = 3,
    WMTornOffMenuWindowLevel = 3,
    WMMainMenuWindowLevel = 20,
    WMDockWindowLevel = 21,       /* Deprecated - use NSStatusWindowLevel */
    WMStatusWindowLevel = 21,
    WMModalPanelWindowLevel = 100,
    WMPopUpMenuWindowLevel = 101,
    WMScreenSaverWindowLevel = 1000
};


/* window attributes */
enum {
    WMBorderlessWindowMask = 0,
    WMTitledWindowMask = 1,
    WMClosableWindowMask = 2,
    WMMiniaturizableWindowMask = 4,
    WMResizableWindowMask = 8,
    WMIconWindowMask = 64,
    WMMiniWindowMask = 128
};
#endif

/* window manager -> appkit notifications */
#define GNUSTEP_WM_NOTIFICATION		"GNUSTEP_WM_NOTIFICATION"


typedef struct {
    CARD32 flags;
    CARD32 window_style;
    CARD32 window_level;
    CARD32 reserved;
    Pixmap miniaturize_pixmap;	       /* pixmap for miniaturize button */
    Pixmap close_pixmap;	       /* pixmap for close button */
    Pixmap miniaturize_mask;	       /* miniaturize pixmap mask */
    Pixmap close_mask;		       /* close pixmap mask */
    CARD32 extra_flags;
} GNUstepWMAttributes;

#define GSWindowStyleAttr 	(1<<0)
#define GSWindowLevelAttr 	(1<<1)
#define GSMiniaturizePixmapAttr	(1<<3)
#define GSClosePixmapAttr	(1<<4)
#define GSMiniaturizeMaskAttr	(1<<5)
#define GSCloseMaskAttr		(1<<6)
#define GSExtraFlagsAttr	(1<<7)

/* extra flags */
#define GSDocumentEditedFlag	(1<<0)

#define GSNoApplicationIconFlag	(1<<5)


#define WMFHideOtherApplications	10
#define WMFHideApplication		12

#endif

