/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMKEYBIND_H
#define WMKEYBIND_H

/* <X11/X.h> doesn't define these, even though XFree supports them */
#ifndef Button6
#define Button6 6
#endif

#ifndef Button7
#define Button7 7
#endif

#ifndef Button8
#define Button8 8
#endif

#ifndef Button9
#define Button9 9
#endif

enum {
	/* anywhere */
	WKBD_ROOTMENU,
	WKBD_WINDOWMENU,
	WKBD_WINDOWLIST,
	WKBD_WORKSPACEMENU,

	/* window */
	WKBD_MINIATURIZE,
	WKBD_MINIMIZEALL,
	WKBD_HIDE,
	WKBD_HIDE_OTHERS,
	WKBD_MAXIMIZE,
	WKBD_VMAXIMIZE,
	WKBD_HMAXIMIZE,
	WKBD_CENTRAL,
	WKBD_LHMAXIMIZE,
	WKBD_RHMAXIMIZE,
	WKBD_THMAXIMIZE,
	WKBD_BHMAXIMIZE,
	WKBD_LTCMAXIMIZE,
	WKBD_RTCMAXIMIZE,
	WKBD_LBCMAXIMIZE,
	WKBD_RBCMAXIMIZE,
	WKBD_MAXIMUS,
	WKBD_SELECT,
	WKBD_KEEP_ON_TOP,
	WKBD_KEEP_AT_BOTTOM,
	WKBD_OMNIPRESENT,
	WKBD_RAISE,
	WKBD_LOWER,
	WKBD_RAISELOWER,
	WKBD_MOVERESIZE,
	WKBD_SHADE,
	WKBD_WORKSPACEMAP,
	WKBD_FOCUSNEXT,
	WKBD_FOCUSPREV,
	WKBD_FOCUSLEFT,
	WKBD_FOCUSRIGHT,
	WKBD_FOCUSUP,
	WKBD_FOCUSDOWN,
	WKBD_GROUPNEXT,
	WKBD_GROUPPREV,
	WKBD_MARK_SET,
	WKBD_MARK_UNSET,
	WKBD_MARK_BRING,
	WKBD_MARK_JUMP,
	WKBD_MARK_SWAP,

	/* window, menu */
	WKBD_CLOSE,

	/* Dock */
	WKBD_DOCKRAISELOWER,

	/* Clip */
	WKBD_CLIPRAISELOWER,

	/* workspace */
	WKBD_WORKSPACE1,
	WKBD_WORKSPACE2,
	WKBD_WORKSPACE3,
	WKBD_WORKSPACE4,
	WKBD_WORKSPACE5,
	WKBD_WORKSPACE6,
	WKBD_WORKSPACE7,
	WKBD_WORKSPACE8,
	WKBD_WORKSPACE9,
	WKBD_WORKSPACE10,
	WKBD_NEXTWORKSPACE,
	WKBD_PREVWORKSPACE,
	WKBD_LASTWORKSPACE,
	WKBD_NEXTWSLAYER,
	WKBD_PREVWSLAYER,

	/* move to workspace */
	WKBD_MOVE_WORKSPACE1,
	WKBD_MOVE_WORKSPACE2,
	WKBD_MOVE_WORKSPACE3,
	WKBD_MOVE_WORKSPACE4,
	WKBD_MOVE_WORKSPACE5,
	WKBD_MOVE_WORKSPACE6,
	WKBD_MOVE_WORKSPACE7,
	WKBD_MOVE_WORKSPACE8,
	WKBD_MOVE_WORKSPACE9,
	WKBD_MOVE_WORKSPACE10,
	WKBD_MOVE_NEXTWORKSPACE,
	WKBD_MOVE_PREVWORKSPACE,
	WKBD_MOVE_LASTWORKSPACE,
	WKBD_MOVE_NEXTWSLAYER,
	WKBD_MOVE_PREVWSLAYER,

	/* window shortcuts */
	WKBD_WINDOW1,
	WKBD_WINDOW2,
	WKBD_WINDOW3,
	WKBD_WINDOW4,
	WKBD_WINDOW5,
	WKBD_WINDOW6,
	WKBD_WINDOW7,
	WKBD_WINDOW8,
	WKBD_WINDOW9,
	WKBD_WINDOW10,

	/* shortcuts to move window between heads */
	WKBD_MOVE_12_TO_6_HEAD,
	WKBD_MOVE_6_TO_12_HEAD,

	/* launch a new instance of the active window */
	WKBD_RELAUNCH,

	/* screen */
	WKBD_SWITCH_SCREEN,

	/* open "run" dialog */
	WKBD_RUN,

#ifdef KEEP_XKB_LOCK_STATUS
	WKBD_TOGGLE,
#endif
	/* keep this last */
	WKBD_LAST
};

typedef struct WShortKey {
    unsigned int modifier;
    KeyCode keycode;
} WShortKey;

/* ---[ Global Variables ]------------------------------------------------ */

struct SHBinding;	/* shbinding.h (CUN-1: built-ins are SHBinding, RSM_WKBD) */
/* `wKeyBindings` is declared in shbinding.h — include it to use the built-ins. */

/* ---[ Functions ]------------------------------------------------------- */

void wKeyboardInitialize(void);

#endif /* WMKEYBIND_H */
