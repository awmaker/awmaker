/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef _editmenu_h_
#define _editmenu_h_

typedef struct W_EditMenu WEditMenu;
typedef struct W_EditMenuItem WEditMenuItem;


typedef struct WEditMenuDelegate {
    void *data;

    void (*itemCloned)(struct WEditMenuDelegate*, WEditMenu*,
                       WEditMenuItem*, WEditMenuItem *);
    void (*itemEdited)(struct WEditMenuDelegate*, WEditMenu*,
                       WEditMenuItem*);
    void (*itemSelected)(struct WEditMenuDelegate*, WEditMenu*,
                         WEditMenuItem*);
    void (*itemDeselected)(struct WEditMenuDelegate*, WEditMenu*,
                           WEditMenuItem*);
    Bool (*shouldRemoveItem)(struct WEditMenuDelegate*, WEditMenu*,
                             WEditMenuItem*);
} WEditMenuDelegate;




WEditMenuItem *WCreateEditMenuItem(WMWidget *parent, const char *title,
                                   Bool isTitle);


char *WGetEditMenuItemTitle(WEditMenuItem *item);

void *WGetEditMenuItemData(WEditMenuItem *item);

void WSetEditMenuItemData(WEditMenuItem *item, void *data,
                          WMCallback *destroyer);

void WSetEditMenuItemImage(WEditMenuItem *item, WMPixmap *pixmap);

WEditMenu *WCreateEditMenu(WMScreen *scr, const char *title);

WEditMenu *WCreateEditMenuPad(WMWidget *parent);

void WSetEditMenuDelegate(WEditMenu *mPtr, WEditMenuDelegate *delegate);

WEditMenuItem *WInsertMenuItemWithTitle(WEditMenu *mPtr, int index,
                                        const char *title);

WEditMenuItem *WAddMenuItemWithTitle(WEditMenu *mPtr, const char *title);

WEditMenuItem *WGetEditMenuItem(WEditMenu *mPtr, int index);

void WSetEditMenuTitle(WEditMenu *mPtr, const char *title);

char *WGetEditMenuTitle(WEditMenu *mPtr);

void WSetEditMenuAcceptsDrop(WEditMenu *mPtr, Bool flag);

void WSetEditMenuSubmenu(WEditMenu *mPtr, WEditMenuItem *item,
                         WEditMenu *submenu);


WEditMenu *WGetEditMenuSubmenu(WEditMenuItem *item);

void WRemoveEditMenuItem(WEditMenu *mPtr, WEditMenuItem *item);

void WSetEditMenuSelectable(WEditMenu *mPtr, Bool flag);

void WSetEditMenuEditable(WEditMenu *mPtr, Bool flag);

void WSetEditMenuIsFactory(WEditMenu *mPtr, Bool flag);

void WSetEditMenuMinSize(WEditMenu *mPtr, WMSize size);

void WSetEditMenuMaxSize(WEditMenu *mPtr, WMSize size);

WMPoint WGetEditMenuLocationForSubmenu(WEditMenu *mPtr, WEditMenu *submenu);

void WTearOffEditMenu(WEditMenu *menu, WEditMenu *submenu);

Bool WEditMenuIsTornOff(WEditMenu *mPtr);


void WEditMenuHide(WEditMenu *menu);

void WEditMenuUnhide(WEditMenu *menu);

void WEditMenuShowAt(WEditMenu *menu, int x, int y);


#endif

