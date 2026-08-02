/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMWDEFAULTS_H_
#define WMWDEFAULTS_H_


/* bit flags for the above window attributes */
#define WA_TITLEBAR  		(1<<0)
#define WA_RESIZABLE  		(1<<1)
#define WA_CLOSABLE  		(1<<2)
#define WA_MINIATURIZABLE  	(1<<3)
#define WA_BROKEN_CLOSE  	(1<<4)
#define WA_SHADEABLE  		(1<<5)
#define WA_FOCUSABLE  		(1<<6)
#define WA_OMNIPRESENT 	 	(1<<7)
#define WA_SKIP_WINDOW_LIST  	(1<<8)
#define WA_SKIP_SWITCHPANEL  	(1<<9)
#define WA_FLOATING  		(1<<10)
#define WA_IGNORE_KEYS 		(1<<11)
#define WA_IGNORE_MOUSE  	(1<<12)
#define WA_IGNORE_HIDE_OTHERS	(1<<13)
#define WA_NOT_APPLICATION	(1<<14)
#define WA_DONT_MOVE_OFF	(1<<15)

int wDefaultGetStartWorkspace(virtual_screen *vscr, const char *instance, const char *class);
void wDefaultPurgeInfo(const char *instance, const char *class);
void wDefaultChangeIcon(const char *instance, const char *class, const char *file);
void wDefaultFillAttributes(const char *instance, const char *class,
			    WWindowAttributes *attr, WWindowAttributes *mask,
			    Bool useGlobalDefault);
char *wDefaultGetIconFile(const char *instance, const char *class, Bool default_icon);
#endif
