/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMMENUGEN_H
#define WMMENUGEN_H

#include <WINGs/WUtil.h>

#include "../src/wconfig.h"

/* flags attached to a particular WMMenuEntry */
#define F_TERMINAL		(1 << 0)
#define F_RESTART_SELF		(1 << 1)
#define F_RESTART_OTHER		(1 << 2)
#define F_QUIT			(1 << 3)
#define F_FREE_CMD_LINE		(1 << 4)


/* a representation of a Window Maker menu entry. all menus are
 * transformed into this form.
 */
typedef struct {
	char		*Name;		/* display name; submenu path of submenu */
	char		*CmdLine;	/* command to execute, NULL if submenu */
	char		*SubMenu;	/* submenu to place entry in; only used when an entry is */
					/* added to the tree by the parser; new entries created in */
					/* main (submenu creation) should set this to NULL */
	int		 Flags;		/* flags */
} WMMenuEntry;

/* the abstract menu tree
 */
extern WMTreeNode *menu;

extern char *env_lang, *env_ctry, *env_enc, *env_mod;

/* Type for the call-back function to add a menu entry to the current menu */
typedef void cb_add_menu_entry(WMMenuEntry *entry);

/* wmmenu_misc.c
 */
void  parse_locale(const char *what, char **env_lang, char **env_ctry, char **env_enc, char **env_mod);
char *find_terminal_emulator(void);
Bool fileInPath(const char *file);

/* implemented parsers
 */
void parse_xdg(const char *file, cb_add_menu_entry *addWMMenuEntryCallback);
void parse_wmconfig(const char *file, cb_add_menu_entry *addWMMenuEntryCallback);
Bool wmconfig_validate_file(const char *filename, const struct stat *st, int tflags, struct FTW *ftw);

#endif  /* WMMENUGEN_H */
