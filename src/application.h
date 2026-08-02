/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef WMAPPLICATION_H_
#define WMAPPLICATION_H_

/* for tracking single application instances */
typedef struct WApplication {
    struct WApplication *next;
    struct WApplication *prev;

    Window main_window;		       /* ID of the group leader */
    struct WWindow *main_window_desc;  /* main (leader) window descriptor */
    WMenu *app_menu;		       /* application menu */
    WMenu *user_menu;		       /* user menu */
    struct WAppIcon *app_icon;
    int refcount;
    struct WWindow *last_focused;      /* focused window before hide */
    int last_workspace;		       /* last workspace used to work on the
                                        * app */
    WMHandlerID *urgent_bounce_timer;
    struct {
        unsigned int skip_next_animation:1;
        unsigned int hidden:1;
        unsigned int emulated:1;
        unsigned int bouncing:1;
    } flags;
} WApplication;


WApplication *wApplicationCreate(struct WWindow *wwin);
WApplication *wApplicationOf(Window window);
void wApplicationDestroy(WApplication *wapp);

void wAppBounce(WApplication *);
void wAppBounceWhileUrgent(WApplication *);
void wApplicationActivate(WApplication *);
void wApplicationDeactivate(WApplication *);
#endif
