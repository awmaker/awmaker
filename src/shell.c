/* shell.c
 *
 *  Copyright (c) 2017 Rodolfo García Peñas <kix@kix.es>
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

#include "window.h"
#include "screen.h"
#include "main.h"
#include "dialog.h"
#include "event.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct {
	virtual_screen *vscr;
	char *command;
} _tuple;

static void shellCommandHandler(pid_t pid, unsigned int status, void *client_data);

static void shellCommandHandler(pid_t pid, unsigned int status, void *client_data)
{
	_tuple *data = (_tuple *) client_data;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) pid;

	if (status == 127) {
		char *buffer;

		buffer = wstrconcat(_("Could not execute command: "), data->command);

		wMessageDialog(data->vscr, _("Error"), buffer, _("OK"), NULL, NULL);
		wfree(buffer);
	}

	wfree(data->command);
	wfree(data);
}

void ExecuteShellCommand(virtual_screen *vscr, const char *command)
{
	static char *shell = NULL;
	pid_t pid;

	/*
	 * This have a problem: if the shell is tcsh (not sure about others)
	 * and ~/.tcshrc have /bin/stty erase ^H somewhere on it, the shell
	 * will block and the command will not be executed.
	 if (!shell) {
	 shell = getenv("SHELL");
	 if (!shell)
	 shell = "/bin/sh";
	 }
	 */
	shell = "/bin/sh";

	pid = fork();

	if (pid == 0) {

		SetupEnvironment(vscr);

#ifdef HAVE_SETSID
		setsid();
#endif
		execl(shell, shell, "-c", command, NULL);
		werror("could not execute %s -c %s", shell, command);
		Exit(-1);
	} else if (pid < 0) {
		werror("cannot fork a new process");
	} else {
		_tuple *data = wmalloc(sizeof(_tuple));

		data->vscr = vscr;
		data->command = wstrdup(command);

		wAddDeathHandler(pid, shellCommandHandler, data);
	}
}

int execute_command(virtual_screen *vscr, char **argv, int argc)
{
	int ret, i;
	char **a;

	pid_t pid = fork();
	ret = pid;
	if (pid == 0) {
		SetupEnvironment(vscr);
#ifdef HAVE_SETSID
		setsid();
#endif
		/* argv is not null-terminated */
		a = (char **) malloc(argc + 1);
		if (!a) {
			werror("out of memory trying to relaunch the application");
			Exit(-1);
		}

		for (i = 0; i < argc; i++)
			a[i] = argv[i];

		a[i] = NULL;

		execvp(a[0], a);
		Exit(-1);
	} else if (pid < 0) {
		werror("cannot fork a new process");
	} else {
		_tuple *data = wmalloc(sizeof(_tuple));

		data->vscr = vscr;
		data->command = wtokenjoin(argv, argc);

		/* not actually a shell command */
		wAddDeathHandler(pid, shellCommandHandler, data);
	}

	return ret;
}

int execute_command2(virtual_screen *vscr, char **argv, int argc)
{
	int ret, i;
	char **args;

	pid_t pid = fork();
	ret = pid;
	if (pid == 0) {
		SetupEnvironment(vscr);

#ifdef HAVE_SETSID
		setsid();
#endif

		args = malloc(sizeof(char *) * (argc + 1));
		if (!args)
			exit(111);

		for (i = 0; i < argc; i++)
			args[i] = argv[i];

		args[argc] = NULL;
		execvp(argv[0], args);
		exit(111);
	}

	return ret;
}
