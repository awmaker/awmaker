/*
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
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

#ifndef WMDEFAULTS_H_
#define WMDEFAULTS_H_

typedef struct WDDomain {
	const char *domain_name;
	WMPropList *dictionary;
	const char *path;
	time_t timestamp;
} WDDomain;

char *get_wmstate_file(virtual_screen *vscr);
void wReadDefaults(virtual_screen *vscr, WMPropList *new_dict);
void wDefaultsCheckDomains(void *arg);
void apply_defaults_to_screen(virtual_screen *vscr, WScreen *scr);
void startup_set_defaults_virtual(void);
#endif /* WMDEFAULTS_H_ */
