/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include <WINGs/WUtil.h>


static const char *const terminals[] = {
	"x-terminal-emulator", /* Debian wrapper to launch user's prefered X terminal */
	"aterm",    /* AfterStep's X terminal, which provides "transparency" */
	"eterm",    /* Enlightenment's terminal, designed for eye-candyness (and efficiency) */
	"gnome-terminal",      /* GNOME project's terminal */
	"konsole",  /* KDE project's terminals */
	"kterm",    /* a Multi-Lingual Terminal based on xterm, originally by Katsuya Sano */
	"mlterm",   /* a Multi-Lingual Terminal emulator written from scratch */
	"rxvt",     /* a slimmed-down xterm */
	"mrxvt",    /* rxvt with support for tabs amongst other things */
	"pterm",    /* terminal based on PuTTY, a popular SSH client for Windows */
	"xterm",    /* the standard terminal provided by the X Window System */
	"dtterm"    /* provided by CDE, a frequent Desktop Environment in proprietary UNIXs */
};

/* pick a terminal emulator by finding the first existing entry of `terminals'
 * in $PATH. the returned pointer should be wfreed later.
 * if $WMMENU_TERMINAL exists in the environment, it's value overrides this
 * detection.
 */
char *find_terminal_emulator(void)
{
	char *path, *t;
	int i;

	t = getenv("WMMENU_TERMINAL");
	if (t)
		return wstrdup(t);

	path = getenv("PATH");
	if (!path)
		return NULL;

	for (i = 0; i < wlengthof(terminals); i++) {
		t = wfindfile(path, terminals[i]);
		if (t) {
			wfree(t);
			return wstrdup(terminals[i]);
		}
	}

	return NULL;
}

/* tokenize `what' (LC_MESSAGES or LANG if `what' is NULL) in the form of
 * `language[_territory][.codeset][@modifier]' into separate language, country,
 * encoding, modifier components, which are allocated on demand and should be
 * wfreed later. components that do not exist in `what' are set to NULL.
 */
void parse_locale(const char *what, char **language, char **country, char **encoding, char **modifier)
{
	char *e, *p;

	*language = *country = *encoding = *modifier = NULL;

	if (what == NULL) {
		e = getenv("LC_MESSAGES");
		if (e == NULL) {
			e = getenv("LANG");	/* this violates the spec */
			if (e == NULL)
				return;
		}
		e = wstrdup(e);
	} else {
		e = wstrdup(what);
	}

	if (strlen(e) == 0 ||
	    strcmp(e, "POSIX") == 0 ||
	    strcmp(e, "C") == 0)
		goto out;

	p = strchr(e, '@');
	if (p) {
		*modifier = wstrdup(p + 1);
		*p = '\0';
	}

	p = strchr(e, '.');
	if (p) {
		*encoding = wstrdup(p + 1);
		*p = '\0';
	}

	p = strchr(e, '_');
	if (p) {
		*country = wstrdup(p + 1);
		*p = '\0';
	}

	if (strlen(e) > 0)
		*language = wstrdup(e);

out:
	free(e);
	return;

}

/* determine whether (first token of) given file is in $PATH
 */
Bool fileInPath(const char *file)
{
	char *p, *t;
	static char *path = NULL;

	if (!file || !*file)
		return False;

	/* if it's an absolute path spec, don't override the user.
	 * s/he might just know better.
	 */
	if (*file == '/')
		return True;

	/* if it has a directory separator at random places,
	 * we might know better.
	 */
	p = strchr(file, '/');
	if (p)
		return False;

	if (!path) {
		path = getenv("PATH");
		if (!path)
			return False;
	}

	p = wstrdup(file);
	t = strpbrk(p, " \t");
	if (t)
		*t = '\0';

	t = wfindfile(path, p);
	wfree(p);

	if (t) {
		wfree(t);
		return True;
	}

	return False;
}
