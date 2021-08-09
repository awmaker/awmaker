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
#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <pwd.h>
#include <math.h>
#include <time.h>
#include <errno.h>

#include <X11/XKBlib.h>

#include <WINGs/WUtil.h>
#include <wraster.h>

#include "window.h"
#include "misc.h"
#include "WindowMaker.h"
#include "GNUstep.h"
#include "screen.h"
#include "wcore.h"
#include "window.h"
#include "framewin.h"
#include "dialog.h"
#include "xutil.h"
#include "xmodifier.h"
#include "main.h"
#include "event.h"


#define ICON_SIZE wPreferences.icon_size

/**** Local prototypes *****/
static void UnescapeWM_CLASS(const char *str, char **name, char **class);

/* XFetchName Wrapper */
Bool wFetchName(Display *dpy, Window win, char **winname)
{
	XTextProperty text_prop;
	char **list;
	int num;

	if (XGetWMName(dpy, win, &text_prop)) {
		if (text_prop.value && text_prop.nitems > 0) {
			if (text_prop.encoding == XA_STRING) {
				*winname = wstrdup((char *)text_prop.value);
				XFree(text_prop.value);
			} else {
				text_prop.nitems = strlen((char *)text_prop.value);
				if (XmbTextPropertyToTextList(dpy, &text_prop, &list, &num) >=
				    Success && num > 0 && *list) {
					XFree(text_prop.value);
					*winname = wstrdup(*list);
					XFreeStringList(list);
				} else {
					*winname = wstrdup((char *)text_prop.value);
					XFree(text_prop.value);
				}
			}
		} else {
			/* the title is set, but it was set to none */
			*winname = wstrdup("");
		}

		return True;
	} else {
		/* the hint is probably not set */
		*winname = NULL;

		return False;
	}
}

static void eatExpose(void)
{
	XEvent event, foo;

	/* compress all expose events into a single one */
	if (XCheckMaskEvent(dpy, ExposureMask, &event)) {
		/* ignore other exposure events for this window */
		while (XCheckWindowEvent(dpy, event.xexpose.window, ExposureMask, &foo)) ;

		/* eat exposes for other windows */
		eatExpose();

		event.xexpose.count = 0;
		XPutBackEvent(dpy, &event);
	}
}

void move_window(Window win, int from_x, int from_y, int to_x, int to_y)
{
#ifdef USE_ANIMATIONS
	if (wPreferences.no_animations)
		XMoveWindow(dpy, win, to_x, to_y);
	else
		slide_window(win, from_x, from_y, to_x, to_y);
#else
	XMoveWindow(dpy, win, to_x, to_y);

	/* Tell the compiler it is normal that those parameters are not used in this case */
	(void) from_x;
	(void) from_y;
#endif
}

/* wins is an array of Window, sorted from left to right, the first is
 * going to be moved from (from_x,from_y) to (to_x,to_y) and the
 * following windows are going to be offset by (ICON_SIZE*i,0) */
void slide_windows(Window wins[], int n, int from_x, int from_y, int to_x, int to_y)
{
	time_t time0 = time(NULL);
	float dx, dy, x = from_x, y = from_y, px, py;
	Bool is_dx_nul, is_dy_nul;
	int dx_is_bigger = 0, dx_int, dy_int;
	int slide_delay, slide_steps, slide_slowdown;
	int i;

	/* animation parameters */
	static const struct {
		int delay;
		int steps;
		int slowdown;
	} apars[5] = {
		{ICON_SLIDE_DELAY_UF, ICON_SLIDE_STEPS_UF, ICON_SLIDE_SLOWDOWN_UF},
		{ICON_SLIDE_DELAY_F,  ICON_SLIDE_STEPS_F,  ICON_SLIDE_SLOWDOWN_F},
		{ICON_SLIDE_DELAY_M,  ICON_SLIDE_STEPS_M,  ICON_SLIDE_SLOWDOWN_M},
		{ICON_SLIDE_DELAY_S,  ICON_SLIDE_STEPS_S,  ICON_SLIDE_SLOWDOWN_S},
		{ICON_SLIDE_DELAY_US, ICON_SLIDE_STEPS_US, ICON_SLIDE_SLOWDOWN_US}
	};

	slide_slowdown = apars[(int)wPreferences.icon_slide_speed].slowdown;
	slide_steps = apars[(int)wPreferences.icon_slide_speed].steps;
	slide_delay = apars[(int)wPreferences.icon_slide_speed].delay;

	dx_int = to_x - from_x;
	dy_int = to_y - from_y;
	is_dx_nul = (dx_int == 0);
	is_dy_nul = (dy_int == 0);
	dx = (float) dx_int;
	dy = (float) dy_int;

	if (abs(dx_int) > abs(dy_int))
		dx_is_bigger = 1;

	if (dx_is_bigger) {
		px = dx / slide_slowdown;

		if (px < slide_steps && px > 0)
			px = slide_steps;
		else if (px > -slide_steps && px < 0)
			px = -slide_steps;

		py = (is_dx_nul ? 0.0F : px * dy / dx);
	} else {
		py = dy / slide_slowdown;

		if (py < slide_steps && py > 0)
			py = slide_steps;
		else if (py > -slide_steps && py < 0)
			py = -slide_steps;

		px = (is_dy_nul ? 0.0F : py * dx / dy);
	}

	while (((int) x) != to_x || ((int) y) != to_y) {
		x += px;
		y += py;

		if ((px < 0 && (int) x < to_x) || (px > 0 && (int) x > to_x))
			x = (float) to_x;

		if ((py < 0 && (int) y < to_y) || (py > 0 && (int) y > to_y))
			y = (float) to_y;

		if (dx_is_bigger) {
			px = px * (1.0F - 1 / (float) slide_slowdown);

			if (px < slide_steps && px > 0)
				px = slide_steps;
			else if (px > -slide_steps && px < 0)
				px = -slide_steps;

			py = (is_dx_nul ? 0.0F : px * dy / dx);
		} else {
			py = py * (1.0F - 1 / (float) slide_slowdown);

			if (py < slide_steps && py > 0)
				py = slide_steps;
			else if (py > -slide_steps && py < 0)
				py = -slide_steps;

			px = (is_dy_nul ? 0.0F : py * dx / dy);
		}

		for (i = 0; i < n; i++)
			XMoveWindow(dpy, wins[i], (int) x + i * ICON_SIZE, (int) y);

		XFlush(dpy);

		if (slide_delay > 0)
			wusleep(slide_delay * 1000L);
		else
			wusleep(1000L);

		if (time(NULL) - time0 > MAX_ANIMATION_TIME)
			break;
	}

	for (i = 0; i < n; i++)
		XMoveWindow(dpy, wins[i], to_x + i * ICON_SIZE, to_y);

	XSync(dpy, 0);

	/* compress expose events */
	eatExpose();
}

char *ShrinkString(WMFont *font, const char *string, int width)
{
	int p1, p2, t, p, w, w1 = 0;
	char *pos, *text;

	p = strlen(string);
	w = WMWidthOfString(font, string, p);
	text = wmalloc(strlen(string) + 8);
	strcpy(text, string);
	if (w <= width)
		return text;

	pos = strchr(text, ' ');
	if (!pos)
		pos = strchr(text, ':');

	if (pos) {
		*pos = 0;
		p = strlen(text);
		w1 = WMWidthOfString(font, text, p);

		if (w1 > width) {
			p = 0;
			*pos = ' ';
			*text = 0;
		} else {
			*pos = 0;
			width -= w1;
			p++;
		}

		string += p;
		p = strlen(string);
	} else {
		*text = 0;
	}

	strcat(text, "...");
	width -= WMWidthOfString(font, "...", 3);

	p1 = 0;
	p2 = p;
	t = (p2 - p1) / 2;

	while (p2 > p1 && p1 != t) {
		w = WMWidthOfString(font, &string[p - t], t);
		if (w > width) {
			p2 = t;
			t = p1 + (p2 - p1) / 2;
		} else if (w < width) {
			p1 = t;
			t = p1 + (p2 - p1) / 2;
		} else {
			p2 = p1 = t;
		}
	}

	strcat(text, &string[p - p1]);

	return text;
}

char *FindImage(const char *paths, const char *file)
{
	char *tmp, *path = NULL;

	tmp = strrchr(file, ':');
	if (tmp) {
		*tmp = 0;
		path = wfindfile(paths, file);
		*tmp = ':';
	}

	if (!tmp || !path)
		path = wfindfile(paths, file);

	return path;
}

static void timeoutHandler(void *data)
{
	*(int *) data = 1;
}

static char *getTextSelection(WScreen *screen, Atom selection)
{
	int size, bits, buffer = -1;
	int timeout = 0;
	unsigned long len, bytes;
	char *data;
	static Atom clipboard = 0;
	Atom rtype;
	WMHandlerID timer;
	XEvent ev;

	switch (selection) {
	case XA_CUT_BUFFER0:
		buffer = 0;
		break;
	case XA_CUT_BUFFER1:
		buffer = 1;
		break;
	case XA_CUT_BUFFER2:
		buffer = 2;
		break;
	case XA_CUT_BUFFER3:
		buffer = 3;
		break;
	case XA_CUT_BUFFER4:
		buffer = 4;
		break;
	case XA_CUT_BUFFER5:
		buffer = 5;
		break;
	case XA_CUT_BUFFER6:
		buffer = 6;
		break;
	case XA_CUT_BUFFER7:
		buffer = 7;
		break;
	}

	if (buffer >= 0)
		return XFetchBuffer(dpy, &size, buffer);

	if (!clipboard)
		clipboard = XInternAtom(dpy, "CLIPBOARD", False);

	XDeleteProperty(dpy, screen->info_window, clipboard);
	XConvertSelection(dpy, selection, XA_STRING, clipboard, screen->info_window, CurrentTime);

	timer = WMAddTimerHandler(1000, timeoutHandler, &timeout);

	while (!XCheckTypedWindowEvent(dpy, screen->info_window, SelectionNotify, &ev) && !timeout) ;

	if (timeout) {
		wwarning("selection retrieval timed out");
		return NULL;
	}

	WMDeleteTimerHandler(timer);

	/* nobody owns the selection or the current owner has
	 * nothing to do with what we need */
	if (ev.xselection.property == None)
		return NULL;

	if (XGetWindowProperty(dpy, screen->info_window,
			       clipboard, 0, 1024,
			       False, XA_STRING, &rtype, &bits, &len,
			       &bytes, (unsigned char **)&data) != Success)
		return NULL;

	if (rtype != XA_STRING || bits != 8) {
		wwarning("invalid data in text selection");
		if (data)
			XFree(data);

		return NULL;
	}

	return data;
}

static char *getselection(WScreen *scr)
{
	char *tmp;

	tmp = getTextSelection(scr, XA_PRIMARY);
	if (!tmp)
		tmp = getTextSelection(scr, XA_CUT_BUFFER0);

	return tmp;
}

static char *parseuserinputpart(const char *line, int *ptr, const char *endchars)
{
	int depth = 0, begin;
	char *value = NULL;
	begin = ++*ptr;

	while(line[*ptr] != '\0') {
		if(line[*ptr] == '(') {
			++depth;
		} else if(depth > 0 && line[*ptr] == ')') {
			--depth;
		} else if(depth == 0 && strchr(endchars, line[*ptr]) != NULL) {
			value = wmalloc(*ptr - begin + 1);
			strncpy(value, line + begin, *ptr - begin);
			value[*ptr - begin] = '\0';
			break;
		}

		++*ptr;
	}

	return value;
}

static char *getuserinput(virtual_screen *vscr, const char *line, int *ptr, Bool advanced)
{
	char *ret = NULL, *title = NULL, *prompt = NULL, *name = NULL;
	int rv;

	if (line[*ptr] == '(')
		title = parseuserinputpart(line, ptr, ",)");

	if (title != NULL && line[*ptr] == ',')
		prompt = parseuserinputpart(line, ptr, ",)");

	if (prompt != NULL && line[*ptr] == ',')
		name = parseuserinputpart(line, ptr, ")");

	if (advanced)
		rv = wAdvancedInputDialog(vscr,
		title ? _(title):_("Program Arguments"),
		prompt ? _(prompt):_("Enter command arguments:"),
		name, &ret);
	else
		rv = wInputDialog(vscr,
		title ? _(title):_("Program Arguments"),
		prompt ? _(prompt):_("Enter command arguments:"),
		&ret);

	if (title)
		wfree(title);

	if (prompt)
		wfree(prompt);

	if (name)
		wfree(name);

	return rv ? ret : NULL;
}

#define S_NORMAL 0
#define S_ESCAPE 1
#define S_OPTION 2

/*
 * state    	input   new-state	output
 * NORMAL	%	OPTION		<nil>
 * NORMAL	\	ESCAPE		<nil>
 * NORMAL	etc.	NORMAL		<input>
 * ESCAPE	any	NORMAL		<input>
 * OPTION	s	NORMAL		<selection buffer>
 * OPTION	w	NORMAL		<selected window id>
 * OPTION	a	NORMAL		<input text>
 * OPTION	d	NORMAL		<OffiX DND selection object>
 * OPTION	W	NORMAL		<current workspace>
 * OPTION	etc.	NORMAL		%<input>
 */
#define TMPBUFSIZE 64
char *ExpandOptions(virtual_screen *vscr, const char *cmdline)
{
	int ptr, optr, state, len, olen;
	char *out, *nout;
	char *selection = NULL;
	char *user_input = NULL;
	char tmpbuf[TMPBUFSIZE];
	int slen;

	len = strlen(cmdline);
	olen = len + 1;
	out = malloc(olen);
	if (!out) {
		wwarning(_("out of memory during expansion of \"%s\""), cmdline);
		return NULL;
	}

	*out = 0;
	ptr = 0;		/* input line pointer */
	optr = 0;		/* output line pointer */
	state = S_NORMAL;
	while (ptr < len) {
		switch (state) {
		case S_NORMAL:
			switch (cmdline[ptr]) {
			case '\\':
				state = S_ESCAPE;
				break;
			case '%':
				state = S_OPTION;
				break;
			default:
				state = S_NORMAL;
				out[optr++] = cmdline[ptr];
				break;
			}

			break;
		case S_ESCAPE:
			switch (cmdline[ptr]) {
			case 'n':
				out[optr++] = 10;
				break;

			case 'r':
				out[optr++] = 13;
				break;

			case 't':
				out[optr++] = 9;
				break;

			default:
				out[optr++] = cmdline[ptr];
			}

			state = S_NORMAL;
			break;
		case S_OPTION:
			state = S_NORMAL;
			switch (cmdline[ptr]) {
			case 'w':
				if (vscr->window.focused && vscr->window.focused->flags.focused) {
					snprintf(tmpbuf, sizeof(tmpbuf), "0x%x",
						 (unsigned int) vscr->window.focused->client_win);
					slen = strlen(tmpbuf);
					olen += slen;
					nout = realloc(out, olen);
					if (!nout) {
						wwarning(_("out of memory during expansion of '%s' for command \"%s\""), "%w", cmdline);
						wfree(out);
						if (selection)
							XFree(selection);

						return NULL;
					}

					out = nout;
					strcat(out, tmpbuf);
					optr += slen;
				} else {
					out[optr++] = ' ';
				}

				break;

			case 'W':
				snprintf(tmpbuf, sizeof(tmpbuf), "0x%x", (unsigned int) vscr->workspace.current + 1);
				slen = strlen(tmpbuf);
				olen += slen;
				nout = realloc(out, olen);
				if (!nout) {
					wwarning(_("out of memory during expansion of '%s' for command \"%s\""), "%W", cmdline);
					wfree(out);
					if (selection)
						XFree(selection);

					return NULL;
				}

				out = nout;
				strcat(out, tmpbuf);
				optr += slen;
				break;

			case 'a':
			case 'A':
				ptr++;
				user_input = getuserinput(vscr, cmdline, &ptr, cmdline[ptr-1] == 'A');
				if (user_input) {
					slen = strlen(user_input);
					olen += slen;
					nout = realloc(out, olen);
					if (!nout) {
						wwarning(_("out of memory during expansion of '%s' for command \"%s\""), "%a", cmdline);
						wfree(out);
						if (selection)
							XFree(selection);

						return NULL;
					}
					out = nout;
					strcat(out, user_input);
					optr += slen;
				} else {
					/* Not an error, but user has Canceled the dialog box.
					 * This will make the command to not be performed. */
					wfree(out);
					if (selection)
						XFree(selection);

					return NULL;
				}
				break;

#ifdef USE_DOCK_XDND
			case 'd':
				if (!vscr->screen_ptr->xdestring) {
					vscr->screen_ptr->flags.dnd_data_convertion_status = 1;
					wfree(out);
					if (selection)
						XFree(selection);

					return NULL;
				}

				slen = strlen(vscr->screen_ptr->xdestring);
				olen += slen;
				nout = realloc(out, olen);
				if (!nout) {
					wwarning(_("out of memory during expansion of '%s' for command \"%s\""), "%d", cmdline);
					wfree(out);
					if (selection)
						XFree(selection);

					return NULL;
				}

				out = nout;
				strcat(out, vscr->screen_ptr->xdestring);
				optr += slen;
				break;
#endif				/* USE_DOCK_XDND */

			case 's':
				if (!selection)
					selection = getselection(vscr->screen_ptr);

				if (!selection) {
					wwarning(_("selection not available"));
					wfree(out);
					if (selection)
						XFree(selection);

					return NULL;
				}

				slen = strlen(selection);
				olen += slen;
				nout = realloc(out, olen);
				if (!nout) {
					wwarning(_("out of memory during expansion of '%s' for command \"%s\""), "%s", cmdline);
					wfree(out);
					if (selection)
						XFree(selection);

					return NULL;
				}

				out = nout;
				strcat(out, selection);
				optr += slen;
				break;

			default:
				out[optr++] = '%';
				out[optr++] = cmdline[ptr];
			}
			break;
		}
		out[optr] = 0;
		ptr++;
	}

	if (selection)
		XFree(selection);

	return out;
}

void ParseWindowName(WMPropList *value, char **winstance, char **wclass, const char *where)
{
	char *name;

	*winstance = *wclass = NULL;

	if (!WMIsPLString(value)) {
		wwarning(_("bad window name value in %s state info"), where);
		return;
	}

	name = WMGetFromPLString(value);
	if (!name || strlen(name) == 0) {
		wwarning(_("bad window name value in %s state info"), where);
		return;
	}

	UnescapeWM_CLASS(name, winstance, wclass);
}

char *GetShortcutString(const char *shortcut)
{
	char *k, *tmp, *text, *buffer = NULL;

	tmp = text = wstrdup(shortcut);

	/* get modifiers */
	while ((k = strchr(text, '+')) != NULL) {
		int mod;
		char *lbl;

		*k = 0;
		mod = wXModifierFromKey(text);
		if (mod < 0)
			return wstrdup("bug");

		lbl = wXModifierToShortcutLabel(mod);
		if (lbl)
			buffer = wstrappend(buffer, lbl);
		else
			buffer = wstrappend(buffer, text);

		text = k + 1;
	}

	buffer = wstrappend(buffer, text);
	wfree(tmp);

	return buffer;
}

char *GetShortcutKey(WShortKey key)
{
	const char *key_name;
	char buffer[256];
	char *wr;

	void append_string(const char *text)
	{
		const char *string = text;

		while (*string) {
			if (wr >= buffer + sizeof(buffer) - 1)
				break;

			*wr++ = *string++;
		}
	}

	void append_modifier(int modifier_index, const char *fallback_name)
	{
		if (wPreferences.modifier_labels[modifier_index]) {
			append_string(wPreferences.modifier_labels[modifier_index]);
		} else {
			append_string(fallback_name);
		}
	}

	key_name = XKeysymToString(XkbKeycodeToKeysym(dpy, key.keycode, 0, 0));
	if (!key_name)
		return NULL;

	wr = buffer;
	if (key.modifier & ControlMask)
		append_modifier(1, "Control+");

	if (key.modifier & ShiftMask)
		append_modifier(0, "Shift+");

	if (key.modifier & Mod1Mask)
		append_modifier(2, "Mod1+");

	if (key.modifier & Mod2Mask)
		append_modifier(3, "Mod2+");

	if (key.modifier & Mod3Mask)
		append_modifier(4, "Mod3+");

	if (key.modifier & Mod4Mask)
		append_modifier(5, "Mod4+");

	if (key.modifier & Mod5Mask)
		append_modifier(6, "Mod5+");

	append_string(key_name);
	*wr = '\0';

	return GetShortcutString(buffer);
}

char *EscapeWM_CLASS(const char *name, const char *class)
{
	char *ret, *ename = NULL, *eclass = NULL;
	int i, j, l, len;

	if (!name && !class)
		return NULL;

	if (name) {
		l = strlen(name);
		ename = wmalloc(l * 2 + 1);
		j = 0;

		for (i = 0; i < l; i++) {
			if (name[i] == '\\')
				ename[j++] = '\\';
			else if (name[i] == '.')
				ename[j++] = '\\';

			ename[j++] = name[i];
		}

		ename[j] = 0;
	}

	if (class) {
		l = strlen(class);
		eclass = wmalloc(l * 2 + 1);
		j = 0;

		for (i = 0; i < l; i++) {
			if (class[i] == '\\')
				eclass[j++] = '\\';
			else if (class[i] == '.')
				eclass[j++] = '\\';

			eclass[j++] = class[i];
		}

		eclass[j] = 0;
	}

	if (ename && eclass) {
		len = strlen(ename) + strlen(eclass) + 4;
		ret = wmalloc(len);
		snprintf(ret, len, "%s.%s", ename, eclass);
		wfree(ename);
		wfree(eclass);
	} else if (ename) {
		ret = wstrdup(ename);
		wfree(ename);
	} else {
		ret = wstrdup(eclass);
		wfree(eclass);
	}

	return ret;
}

static void UnescapeWM_CLASS(const char *str, char **name, char **class)
{
	int i, j, k, dot;
	int length_of_name;

	j = strlen(str);

	/* separate string in 2 parts */
	length_of_name = 0;
	dot = -1;
	for (i = 0; i < j; i++, length_of_name++) {
		if (str[i] == '\\') {
			i++;
			continue;
		} else if (str[i] == '.') {
			dot = i;
			break;
		}
	}

	/* unescape the name */
	if (length_of_name > 0) {
		*name = wmalloc(length_of_name + 1);
		for (i = 0, k = 0; i < dot; i++)
			if (str[i] != '\\')
				(*name)[k++] = str[i];

		(*name)[k] = '\0';
	} else {
		*name = NULL;
	}

	/* unescape the class */
	if (dot < j-1) {
		*class = wmalloc(j - (dot + 1) + 1);
		for (i = dot + 1, k = 0; i < j; i++)
			if (str[i] != '\\')
				(*class)[k++] = str[i];

		(*class)[k] = 0;
	} else {
		*class = NULL;
	}
}

static void track_bg_helper_death(pid_t pid, unsigned int status, void *client_data)
{
	WScreen *scr = (WScreen *) client_data;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) pid;
	(void) status;

	close(scr->helper_fd);
	scr->helper_fd = 0;
	scr->helper_pid = 0;
	scr->flags.backimage_helper_launched = 0;
}

Bool start_bg_helper(virtual_screen *vscr)
{
	pid_t pid;
	int filedes[2];
	const char *dither;

	if (pipe(filedes) < 0) {
		werror(_("%s failed, can't set workspace specific background image (%s)"),
		       "pipe()", strerror(errno));
		return False;
	}

	pid = fork();
	if (pid < 0) {
		werror(_("%s failed, can't set workspace specific background image (%s)"),
		       "fork()", strerror(errno));
		close(filedes[0]);
		close(filedes[1]);
		return False;

	} else if (pid == 0) {
		/* We don't need this side of the pipe in the child process */
		close(filedes[1]);

		SetupEnvironment(vscr);

		close(STDIN_FILENO);
		if (dup2(filedes[0], STDIN_FILENO) < 0) {
			werror(_("%s failed, can't set workspace specific background image (%s)"),
			       "dup2()", strerror(errno));
			exit(1);
		}

		close(filedes[0]);

		dither = wPreferences.no_dithering ? "-m" : "-d";
		if (wPreferences.smooth_workspace_back)
			execlp("wmsetbg", "wmsetbg", "-helper", "-S", dither, NULL);
		else
			execlp("wmsetbg", "wmsetbg", "-helper", dither, NULL);

		werror(_("could not execute \"%s\": %s"), "wmsetbg", strerror(errno));
		exit(1);
	} else {
		/* We don't need this side of the pipe in the parent process */
		close(filedes[0]);

		if (fcntl(filedes[1], F_SETFD, FD_CLOEXEC) < 0)
			wwarning(_("could not set close-on-exec flag for bg_helper's communication file handle (%s)"),
			         strerror(errno));

		vscr->screen_ptr->helper_fd = filedes[1];
		vscr->screen_ptr->helper_pid = pid;
		vscr->screen_ptr->flags.backimage_helper_launched = 1;

		wAddDeathHandler(pid, track_bg_helper_death, vscr->screen_ptr);

		return True;
	}
}

void SendHelperMessage(virtual_screen *vscr, char type, int workspace, const char *msg)
{
	char *buffer;
	int len, i;
	char buf[16];

	if (!vscr->screen_ptr->flags.backimage_helper_launched)
		return;

	len = (msg ? strlen(msg) : 0) + (workspace >= 0 ? 4 : 0) + 1;
	buffer = wmalloc(len + 5);
	snprintf(buf, sizeof(buf), "%4i", len);
	memcpy(buffer, buf, 4);
	buffer[4] = type;
	i = 5;
	if (workspace >= 0) {
		snprintf(buf, sizeof(buf), "%4i", workspace);
		memcpy(&buffer[i], buf, 4);
		i += 4;
		buffer[i] = 0;
	}

	if (msg)
		strcpy(&buffer[i], msg);

	if (write(vscr->screen_ptr->helper_fd, buffer, len + 4) < 0)
		werror(_("could not send message to background image helper"));

	wfree(buffer);
}

Bool UpdateDomainFile(WDDomain *domain)
{
	struct stat stbuf;
	char path[PATH_MAX];
	WMPropList *shared_dict, *dict;
	Bool result, freeDict = False;

	dict = domain->dictionary;
	if (WMIsPLDictionary(domain->dictionary)) {
		/* retrieve global system dictionary */
		snprintf(path, sizeof(path), "%s/%s", PKGCONFDIR, domain->domain_name);
		if (stat(path, &stbuf) >= 0) {
			shared_dict = WMReadPropListFromFile(path);
			if (shared_dict) {
				if (WMIsPLDictionary(shared_dict)) {
					freeDict = True;
					dict = WMDeepCopyPropList(domain->dictionary);
					WMSubtractPLDictionaries(dict, shared_dict, True);
				}
				WMReleasePropList(shared_dict);
			}
		}
	}

	result = WMWritePropListToFile(dict, domain->path);

	if (freeDict)
		WMReleasePropList(dict);

	return result;
}

char *StrConcatDot(const char *a, const char *b)
{
	int len;
	char *str;

	if (!a)
		a = "";

	if (!b)
		b = "";

	len = strlen(a) + strlen(b) + 4;
	str = wmalloc(len);

	snprintf(str, len, "%s.%s", a, b);

	return str;
}

static char *getCommandForWindow(Window win, int elements)
{
	char **argv, *command = NULL;
	int argc;

	if (XGetCommand(dpy, win, &argv, &argc)) {
		if (argc > 0 && argv != NULL) {
			if (elements == 0)
				elements = argc;

			command = wtokenjoin(argv, WMIN(argc, elements));
			if (command[0] == 0) {
				wfree(command);
				command = NULL;
			}
		}

		if (argv)
			XFreeStringList(argv);
	}

	return command;
}

/* Free result when done */
char *GetCommandForWindow(Window win)
{
	return getCommandForWindow(win, 0);
}

/*
 * For a Window, in a virtual_screen, creates a resized Pixmap.
 * If error, returns -1. If OK, returns 0
 */
int create_minipixmap_for_window(virtual_screen *vscr, Window win, Pixmap *tmp)
{
	WScreen *scr = vscr->screen_ptr;

	XSetClipMask(dpy, scr->copy_gc, None);
	*tmp = XCreatePixmap(dpy, scr->root_win, wPreferences.icon_size, wPreferences.icon_size, scr->depth);
	if (scr->w_visual == DefaultVisual(dpy, scr->screen)) {
		XCopyArea(dpy, win, *tmp, scr->copy_gc, 0, 0, wPreferences.icon_size, wPreferences.icon_size, 0, 0);
	} else {
		XImage *image;

		image = XGetImage(dpy, win, 0, 0, wPreferences.icon_size,
				  wPreferences.icon_size, AllPlanes, ZPixmap);
		if (!image) {
			XUnmapWindow(dpy, win);
			return -1;
		}

		XPutImage(dpy, *tmp, scr->copy_gc, image, 0, 0, 0, 0,
			  wPreferences.icon_size, wPreferences.icon_size);
		XDestroyImage(image);
	}

	return 0;
}

/*
 * For a WWindow, in a virtual_screen, creates a resized Pixmap.
 * If error, returns -1. If OK, returns 0
 */
int create_minipixmap_for_wwindow(virtual_screen *vscr, WWindow *wwin, Pixmap *pixmap)
{
	RImage *mini_preview, *scaled_mini_preview;
	XImage *pimg;
	unsigned int w, h;
	int x, y, ret;
	Window baz;
	XWindowAttributes attribs;

	/* the window doesn't exist anymore */
	if (!XGetWindowAttributes(dpy, wwin->client_win, &attribs))
		return -1;

	XRaiseWindow(dpy, wwin->frame->core->window);
	XTranslateCoordinates(dpy, wwin->client_win, vscr->screen_ptr->root_win, 0, 0, &x, &y, &baz);

	w = attribs.width;
	h = attribs.height;

	if (x - attribs.x + attribs.width > vscr->screen_ptr->scr_width)
		w = vscr->screen_ptr->scr_width - x + attribs.x;

	if (y - attribs.y + attribs.height > vscr->screen_ptr->scr_height)
		h = vscr->screen_ptr->scr_height - y + attribs.y;

	pimg = XGetImage(dpy, wwin->client_win, 0, 0, w, h, AllPlanes, ZPixmap);
	if (!pimg)
		return -1;

	mini_preview = RCreateImageFromXImage(vscr->screen_ptr->rcontext, pimg, NULL);
	XDestroyImage(pimg);

	if (!mini_preview)
		return -1;

	scaled_mini_preview = RSmoothScaleImage(mini_preview,
		wPreferences.minipreview_size - 2 * MINIPREVIEW_BORDER,
		wPreferences.minipreview_size - 2 * MINIPREVIEW_BORDER);

	ret = RConvertImage(vscr->screen_ptr->rcontext, scaled_mini_preview, pixmap);
	RReleaseImage(scaled_mini_preview);
	RReleaseImage(mini_preview);

	if (!ret)
		return -1;

	return 0;
}
