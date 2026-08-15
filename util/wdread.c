/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifdef __GLIBC__
#define _GNU_SOURCE		/* getopt_long */
#endif

/*
 * WindowMaker defaults DB reader
 */
#include "config.h"

#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_STDNORETURN
#include <stdnoreturn.h>
#endif

#include <WINGs/WUtil.h>

#include <wconfig.h>

static const char *prog_name;

static noreturn void print_help(int print_usage, int exitval)
{
	printf("Usage: %s [OPTIONS] <domain> <key>\n", prog_name);
	if (print_usage) {
		puts("Read <key> from <domain>'s database");
		puts("");
		puts("  -h, --help              display this help message");
		puts("  -v, --version           output version information and exit");
	}
	exit(exitval);
}

int main(int argc, char **argv)
{
	char path[PATH_MAX];
	WMPropList *key, *value, *dict;
	int ch;

	struct option longopts[] = {
		{ "version",	no_argument,		NULL,			'v' },
		{ "help",	no_argument,		NULL,			'h' },
		{ NULL,		0,			NULL,			0 }
	};

	prog_name = argv[0];
	while ((ch = getopt_long(argc, argv, "hv", longopts, NULL)) != -1)
		switch(ch) {
			case 'v':
				printf("%s (Window Maker %s)\n", prog_name, VERSION);
				return 0;
				/* NOTREACHED */
			case 'h':
				print_help(1, 0);
				/* NOTREACHED */
			case 0:
				break;
			default:
				print_help(0, 1);
				/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		print_help(0, 1);

	key = WMCreatePLString(argv[1]);

	snprintf(path, sizeof(path), "%s", wdefaultspathfordomain(argv[0]));

	dict = WMReadPropListFromFile(path);
	if (dict == NULL)
		return 1;	/* bad domain */

	value = WMGetFromPLDictionary(dict, key);
	if (value == NULL)
		return 2;	/* bad key */

	printf("%s\n", WMGetPropListDescription(value, True));
	return 0;
}
