/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMDEFAULTS_H_
#define WMDEFAULTS_H_

typedef struct WDDomain {
	const char *domain_name;
	WMPropList *dictionary;
	const char *path;
	time_t timestamp;
} WDDomain;

char *get_wmstate_file(virtual_screen *vscr);
void wDefaultsCheckDomains(void *arg);
void apply_defaults_to_screen(virtual_screen *vscr, WScreen *scr);
void startup_set_defaults_virtual(void);
Bool wCreateDefaultConfig(void);
void set_defaults_global(WMPropList *new_dict);
unsigned int set_defaults_virtual_screen(virtual_screen *vscr);
#endif /* WMDEFAULTS_H_ */
