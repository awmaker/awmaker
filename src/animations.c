/* animations.c - Animations
 *
 *  AWindow Maker window manager
 *
 *  Copyright (c) 2019 Rodolfo García Peñas (kix)
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#include "WindowMaker.h"
#include "animations.h"
#include "framewin.h"

static struct {
	int steps;
	int delay;
} shadePars[5] = {
	{ SHADE_STEPS_UF, SHADE_DELAY_UF },
	{ SHADE_STEPS_F, SHADE_DELAY_F },
	{ SHADE_STEPS_M, SHADE_DELAY_M },
	{ SHADE_STEPS_S, SHADE_DELAY_S },
	{ SHADE_STEPS_US, SHADE_DELAY_US }
};

#define SHADE_STEPS	shadePars[(int)wPreferences.shade_speed].steps
#define SHADE_DELAY	shadePars[(int)wPreferences.shade_speed].delay

/*
 * Do the animation while shading (called with what = SHADE)
 * or unshading (what = UNSHADE).
 */
#ifdef USE_ANIMATIONS
void animation_shade(WWindow *wwin, Bool what)
{
	int y, s, w, h;
	time_t time0 = time(NULL);

	if (wwin->flags.skip_next_animation || wPreferences.no_animations)
		return;

	switch (what) {
	case SHADE:
		if (!w_global.startup.phase1) {
			/* do the shading animation */
			h = wwin->frame->height;
			s = h / SHADE_STEPS;
			if (s < 1)
				s = 1;

			w = wwin->frame->width;
			y = wwin->frame->top_width;
			while (h > wwin->frame->top_width + 1) {
				XMoveWindow(dpy, wwin->client_win, 0, y);
				XResizeWindow(dpy, wwin->frame->core->window, w, h);
				XFlush(dpy);

				if (time(NULL) - time0 > MAX_ANIMATION_TIME)
					break;

				if (SHADE_DELAY > 0)
					wusleep(SHADE_DELAY * 1000L);
				else
					wusleep(10);

				h -= s;
				y -= s;
			}

			XMoveWindow(dpy, wwin->client_win, 0, wwin->frame->top_width);
		}
		break;

	case UNSHADE:
		h = wwin->frame->top_width + wwin->frame->bottom_width;
		y = wwin->frame->top_width - wwin->height;
		s = abs(y) / SHADE_STEPS;
		if (s < 1)
			s = 1;

		w = wwin->frame->width;
		XMoveWindow(dpy, wwin->client_win, 0, y);
		if (s > 0) {
			while (h < wwin->height + wwin->frame->top_width + wwin->frame->bottom_width) {
				XResizeWindow(dpy, wwin->frame->core->window, w, h);
				XMoveWindow(dpy, wwin->client_win, 0, y);
				XFlush(dpy);
				if (SHADE_DELAY > 0)
					wusleep(SHADE_DELAY * 2000L / 3);
				else
					wusleep(10);

				h += s;
				y += s;

				if (time(NULL) - time0 > MAX_ANIMATION_TIME)
					break;
			}
		}

		XMoveWindow(dpy, wwin->client_win, 0, wwin->frame->top_width);
		break;
	}
}
#else
void animation_shade(WWindow *wwin, Bool what)
{
	(void) wwin;
	(void) what;
}
#endif
