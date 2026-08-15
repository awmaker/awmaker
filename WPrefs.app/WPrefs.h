/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WPREFS_H_
#define WPREFS_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <signal.h>

#include <X11/Xlib.h>

#include <wraster.h>

#include <WINGs/WINGs.h>

/* Needed for HAVE_LIBINTL_H and EXTENDED_WINDOWSHORTCUT.
 * wconfig.h moved to awmcommon/ */
#include <awconfig.h>

/*
 * The macro 'wlengthof' should be used as much as possible, this macro
 * is reserved for the cases where wlengthof cannot be used because the
 * static_assert. Typical symptom is this compiler error (gcc):
 *
 *   error: braced-group within expression allowed only inside a function
 *
 * which appears when you try to create an array whose size is determined using
 * wlengthof on some other array.
 */
#define wlengthof_nocheck(array)  \
	(sizeof(array) / sizeof(array[0]))


/****/

extern char *NOptionValueChanged;
extern Bool xext_xkb_supported;

typedef struct _Panel Panel;

typedef struct {
    unsigned flags;		       /* reserved for WPrefs.c Don't access it */

    void (*createWidgets)(Panel*);     /* called when showing for first time */
    void (*updateDomain)(Panel*);      /* save the changes to the dictionary */
    Bool (*requiresRestart)(Panel*);   /* return True if some static option was changed */
    void (*undoChanges)(Panel*);       /* reset values to those in the dictionary */
    void (*prepareForClose)(Panel*);   /* called when exiting WPrefs */
    void (*showPanel)(Panel*);	       /* called when entering the panel */
    void (*hidePanel)(Panel*);	       /* called when exiting the panel */
} CallbackRec;


/* all Panels must start with the following layout */
typedef struct PanelRec {
    WMBox *box;

    char *sectionName;		       /* section name to display in titlebar */

    char *description;

    CallbackRec callbacks;
} PanelRec;


/* ---[ Wprefs.c ] ------------------------------------------------------- */

void AddSection(Panel *panel, const char *iconFile);

char *LocateImage(const char *name);

void SetButtonAlphaImage(WMScreen *scr, WMButton *bPtr, const char *file);

/* Loads `file' into `icon_normal'. If `icon_greyed' is not NULL,
 * combine `icon_normal' with some grey and then optionally with image
 * `xis', and store it in `icon_greyed' (typically to produce a
 * greyed-out, red-crossed version of `icon_normal') */
void CreateImages(WMScreen *scr, RContext *rc, RImage *xis, const char *file,
		WMPixmap **icon_normal, WMPixmap **icon_greyed);

WMWindow *GetWindow(void);

/* manipulate the dictionary for the WindowMaker domain */

WMPropList *GetObjectForKey(const char *defaultName);

void SetObjectForKey(WMPropList *object, const char *defaultName);

void RemoveObjectForKey(const char *defaultName);

char *GetStringForKey(const char *defaultName);

int GetIntegerForKey(const char *defaultName);

Bool GetBoolForKey(const char *defaultName);

int GetSpeedForKey(const char *defaultName);

void SetIntegerForKey(int value, const char *defaultName);

void SetStringForKey(const char *value, const char *defaultName);

void SetBoolForKey(Bool value, const char *defaultName);

void SetSpeedForKey(int speed, const char *defaultName);


/* ---[ KeyboardShortcuts.c ] -------------------------------------------- */

char *capture_shortcut(Display *dpy, Bool *capturing, Bool convert_case);


/* ---[ double.c ] ------------------------------------------------------- */
typedef struct W_DoubleTest DoubleTest;

DoubleTest *CreateDoubleTest(WMWidget *parent, const char *text);


/* ---[ main.c ] --------------------------------------------------------- */
void AddDeadChildHandler(pid_t pid, void (*handler)(void*), void *data);


/* ---[ xmodifier.c ] ---------------------------------------------------- */
void wXModifierInitialize(Display *display);
int  wXModifierFromKey(const char *key);
char *wXModifierToShortcutLabel(int mask);
int ModifierFromKey(Display *dpy, const char *key);


/* ---[ Panel Initializers ]---------------------------------------------- */

void Initialize(WMScreen *scr);

/* in alphabetical order - in case you'd want to add one */
Panel *InitAppearance(WMWidget *parent);
Panel *InitConfigurations(WMWidget *parent);
Panel *InitDocks(WMWidget *parent);
Panel *InitExpert(WMWidget *parent);
Panel *InitFocus(WMWidget *parent);
Panel *InitFontSimple(WMWidget *parent);
Panel *InitHotCornerShortcuts(WMWidget *parent);
Panel *InitIcons(WMWidget *parent);
Panel *InitKeyboardShortcuts(WMWidget *parent);
Panel *InitMenu(WMWidget *parent);
Panel *InitMenuPreferences(WMWidget *parent);
Panel *InitMouseSettings(WMWidget *parent);
Panel *InitPaths(WMWidget *parent);
Panel *InitPreferences(WMWidget *parent);
Panel *InitWindowHandling(WMWidget *parent);
Panel *InitWorkspace(WMWidget *parent);


#define FRAME_TOP	105
#define FRAME_LEFT	-2
#define FRAME_WIDTH	524
#define FRAME_HEIGHT	235
#endif /* WPREFS_H_ */

