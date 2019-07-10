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
#include <math.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#include "WindowMaker.h"
#include "animations.h"
#include "framewin.h"
#include "event.h"
#include "miniwindow.h"
#include "misc.h"

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

#ifndef HAVE_FLOAT_MATHFUNC
#define sinf(x) ((float)sin((double)(x)))
#define cosf(x) ((float)cos((double)(x)))
#define sqrtf(x) ((float)sqrt((double)(x)))
#define atan2f(y, x) ((float)atan((double)(y) / (double)(x)))
#endif

/*
 * Do the animation while shading (called with what = SHADE)
 * or unshading (what = UNSHADE).
 */
#ifdef USE_ANIMATIONS
static int getAnimationGeometry(WWindow *wwin, int *ix, int *iy, int *iw, int *ih);

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

void animation_catchevents(void)
{
	if (!w_global.startup.phase1)
		/* Catch up with events not processed while animation was running */
		ProcessPendingEvents();
}

static void animateResizeFlip(virtual_screen *vscr, int x, int y, int w, int h, int fx, int fy, int fw, int fh, int steps)
{
#define FRAMES (MINIATURIZE_ANIMATION_FRAMES_F)
	float cx, cy, cw, ch;
	float xstep, ystep, wstep, hstep;
	XPoint points[5];
	float dx, dch, midy;
	float angle, final_angle, delta;

	xstep = (float)(fx - x) / steps;
	ystep = (float)(fy - y) / steps;
	wstep = (float)(fw - w) / steps;
	hstep = (float)(fh - h) / steps;

	cx = (float)x;
	cy = (float)y;
	cw = (float)w;
	ch = (float)h;

	final_angle = 2 * WM_PI * MINIATURIZE_ANIMATION_TWIST_F;
	delta = (float)(final_angle / FRAMES);
	for (angle = 0;; angle += delta) {
		if (angle > final_angle)
			angle = final_angle;

		dx = (cw / 10) - ((cw / 5) * sinf(angle));
		dch = (ch / 2) * cosf(angle);
		midy = cy + (ch / 2);

		points[0].x = cx + dx;
		points[0].y = midy - dch;
		points[1].x = cx + cw - dx;
		points[1].y = points[0].y;
		points[2].x = cx + cw + dx;
		points[2].y = midy + dch;
		points[3].x = cx - dx;
		points[3].y = points[2].y;
		points[4].x = points[0].x;
		points[4].y = points[0].y;

		XGrabServer(dpy);
		XDrawLines(dpy, vscr->screen_ptr->root_win, vscr->screen_ptr->frame_gc, points, 5, CoordModeOrigin);
		XFlush(dpy);
		wusleep(MINIATURIZE_ANIMATION_DELAY_F);

		XDrawLines(dpy, vscr->screen_ptr->root_win, vscr->screen_ptr->frame_gc, points, 5, CoordModeOrigin);
		XUngrabServer(dpy);
		cx += xstep;
		cy += ystep;
		cw += wstep;
		ch += hstep;
		if (angle >= final_angle)
			break;
	}

	XFlush(dpy);
}

#undef FRAMES

static void
animateResizeTwist(virtual_screen *vscr, int x, int y, int w, int h, int fx, int fy, int fw, int fh, int steps)
{
#define FRAMES (MINIATURIZE_ANIMATION_FRAMES_T)
	float cx, cy, cw, ch;
	float xstep, ystep, wstep, hstep;
	XPoint points[5];
	float angle, final_angle, a, d, delta;

	x += w / 2;
	y += h / 2;
	fx += fw / 2;
	fy += fh / 2;

	xstep = (float)(fx - x) / steps;
	ystep = (float)(fy - y) / steps;
	wstep = (float)(fw - w) / steps;
	hstep = (float)(fh - h) / steps;

	cx = (float)x;
	cy = (float)y;
	cw = (float)w;
	ch = (float)h;

	final_angle = 2 * WM_PI * MINIATURIZE_ANIMATION_TWIST_T;
	delta = (float)(final_angle / FRAMES);
	for (angle = 0;; angle += delta) {
		if (angle > final_angle)
			angle = final_angle;

		a = atan2f(ch, cw);
		d = sqrtf((cw / 2) * (cw / 2) + (ch / 2) * (ch / 2));

		points[0].x = cx + cosf(angle - a) * d;
		points[0].y = cy + sinf(angle - a) * d;
		points[1].x = cx + cosf(angle + a) * d;
		points[1].y = cy + sinf(angle + a) * d;
		points[2].x = cx + cosf(angle - a + (float)WM_PI) * d;
		points[2].y = cy + sinf(angle - a + (float)WM_PI) * d;
		points[3].x = cx + cosf(angle + a + (float)WM_PI) * d;
		points[3].y = cy + sinf(angle + a + (float)WM_PI) * d;
		points[4].x = cx + cosf(angle - a) * d;
		points[4].y = cy + sinf(angle - a) * d;
		XGrabServer(dpy);
		XDrawLines(dpy, vscr->screen_ptr->root_win, vscr->screen_ptr->frame_gc, points, 5, CoordModeOrigin);
		XFlush(dpy);
		wusleep(MINIATURIZE_ANIMATION_DELAY_T);

		XDrawLines(dpy, vscr->screen_ptr->root_win, vscr->screen_ptr->frame_gc, points, 5, CoordModeOrigin);
		XUngrabServer(dpy);
		cx += xstep;
		cy += ystep;
		cw += wstep;
		ch += hstep;
		if (angle >= final_angle)
			break;
	}

	XFlush(dpy);
}

#undef FRAMES

static void animateResizeZoom(virtual_screen *vscr, int x, int y, int w, int h, int fx, int fy, int fw, int fh, int steps)
{
#define FRAMES (MINIATURIZE_ANIMATION_FRAMES_Z)
	float cx[FRAMES], cy[FRAMES], cw[FRAMES], ch[FRAMES];
	float xstep, ystep, wstep, hstep;
	int i, j;

	xstep = (float)(fx - x) / steps;
	ystep = (float)(fy - y) / steps;
	wstep = (float)(fw - w) / steps;
	hstep = (float)(fh - h) / steps;

	for (j = 0; j < FRAMES; j++) {
		cx[j] = (float)x;
		cy[j] = (float)y;
		cw[j] = (float)w;
		ch[j] = (float)h;
	}

	XGrabServer(dpy);
	for (i = 0; i < steps; i++) {
		for (j = 0; j < FRAMES; j++)
			XDrawRectangle(dpy, vscr->screen_ptr->root_win, vscr->screen_ptr->frame_gc,
				       (int)cx[j], (int)cy[j], (int)cw[j], (int)ch[j]);

		XFlush(dpy);
		wusleep(MINIATURIZE_ANIMATION_DELAY_Z);

		for (j = 0; j < FRAMES; j++) {
			XDrawRectangle(dpy, vscr->screen_ptr->root_win, vscr->screen_ptr->frame_gc,
				       (int)cx[j], (int)cy[j], (int)cw[j], (int)ch[j]);
			if (j < FRAMES - 1) {
				cx[j] = cx[j + 1];
				cy[j] = cy[j + 1];
				cw[j] = cw[j + 1];
				ch[j] = ch[j + 1];
			} else {
				cx[j] += xstep;
				cy[j] += ystep;
				cw[j] += wstep;
				ch[j] += hstep;
			}
		}
	}

	for (j = 0; j < FRAMES; j++)
		XDrawRectangle(dpy, vscr->screen_ptr->root_win, vscr->screen_ptr->frame_gc, (int)cx[j], (int)cy[j], (int)cw[j], (int)ch[j]);
	XFlush(dpy);
	wusleep(MINIATURIZE_ANIMATION_DELAY_Z);

	for (j = 0; j < FRAMES; j++)
		XDrawRectangle(dpy, vscr->screen_ptr->root_win, vscr->screen_ptr->frame_gc, (int)cx[j], (int)cy[j], (int)cw[j], (int)ch[j]);

	XUngrabServer(dpy);
}

#undef FRAMES

void animateResize(virtual_screen *vscr, int x, int y, int w, int h, int fx, int fy, int fw, int fh)
{
	int style = wPreferences.iconification_style;	/* Catch the value */
	int steps;

	if (style == WIS_NONE)
		return;

	if (style == WIS_RANDOM)
		style = rand() % 3;

	switch (style) {
	case WIS_TWIST:
		steps = MINIATURIZE_ANIMATION_STEPS_T;
		if (steps > 0)
			animateResizeTwist(vscr, x, y, w, h, fx, fy, fw, fh, steps);
		break;
	case WIS_FLIP:
		steps = MINIATURIZE_ANIMATION_STEPS_F;
		if (steps > 0)
			animateResizeFlip(vscr, x, y, w, h, fx, fy, fw, fh, steps);
		break;
	case WIS_ZOOM:
	default:
		steps = MINIATURIZE_ANIMATION_STEPS_Z;
		if (steps > 0)
			animateResizeZoom(vscr, x, y, w, h, fx, fy, fw, fh, steps);
		break;
	}
}

void animation_maximize(WWindow *wwin)
{
	int ix, iy, iw, ih;

	if (getAnimationGeometry(wwin, &ix, &iy, &iw, &ih))
		animateResize(wwin->vscr, ix, iy, iw, ih,
					  wwin->frame_x, wwin->frame_y,
				      wwin->frame->width, wwin->frame->height);
}

void animation_minimize(WWindow *wwin)
{
	int ix, iy, iw, ih;

	if (getAnimationGeometry(wwin, &ix, &iy, &iw, &ih))
		animateResize(wwin->vscr, wwin->frame_x, wwin->frame_y,
				      wwin->frame->width, wwin->frame->height, ix, iy, iw, ih);
}

static int getAnimationGeometry(WWindow *wwin, int *ix, int *iy, int *iw, int *ih)
{
	if (w_global.startup.phase1 || wPreferences.no_animations
		|| wwin->flags.skip_next_animation || wwin->miniwindow->icon == NULL)
		return 0;

	if (!wPreferences.disable_miniwindows && !wwin->flags.net_handle_icon) {
		*ix = miniwindow_get_xpos(wwin);
		*iy = miniwindow_get_ypos(wwin);
		*iw = wPreferences.icon_size;
		*ih = wPreferences.icon_size;
	} else {
		if (wwin->flags.net_handle_icon) {
			*ix = miniwindow_get_xpos(wwin);
			*iy = miniwindow_get_ypos(wwin);
			*iw = wwin->miniwindow->icon_w;
			*ih = wwin->miniwindow->icon_h;
		} else {
			*ix = 0;
			*iy = 0;
			*iw = wwin->vscr->screen_ptr->scr_width;
			*ih = wwin->vscr->screen_ptr->scr_height;
		}
	}

	return 1;
}

void animation_hide(WWindow *wwin, int icon_x, int icon_y, int width, int height)
{

	if (!w_global.startup.phase1 && !wPreferences.no_animations &&
	    !wwin->flags.skip_next_animation)
		animateResize(wwin->vscr, wwin->frame_x, wwin->frame_y,
			      wwin->frame->width, wwin->frame->height,
			      icon_x, icon_y, width, height);
}

void animation_unhide(WWindow *wwin, int icon_x, int icon_y, int width, int height)
{
	if (!w_global.startup.phase1 && !wPreferences.no_animations)
		animateResize(wwin->vscr, icon_x, icon_y,
				      width, height,
				      wwin->frame_x, wwin->frame_y,
				      wwin->frame->width, wwin->frame->height);
}

void animation_slide_window(Window win, int icon_x, int icon_y, int x, int y)
{
	if (!wPreferences.no_animations)
		slide_window(win, icon_x, icon_y, x, y);
}

int animation_iconify_window(WWindow *wwin)
{
	Window clientwin;

	if (w_global.startup.phase1)
		return 0;

	/* Catch up with events not processed while animation was running */
	clientwin = wwin->client_win;
	ProcessPendingEvents();

	/* the window can disappear while ProcessPendingEvents() runs */
	if (!wWindowFor(clientwin))
		return 1;

	return 0;
}

int animation_deiconify_window(WWindow *wwin)
{
	Window clientwin;

	if (w_global.startup.phase1)
		return 0;

	/* Catch up with events not processed while animation was running */
	clientwin = wwin->client_win;
	ProcessPendingEvents();

	/* the window can disappear while ProcessPendingEvents() runs */
	if (!wWindowFor(clientwin)) {
		wwin->vscr->workspace.ignore_change = False;
		return 1;
	}

	return 0;
}
#else
void animation_shade(WWindow *wwin, Bool what)
{
	(void) wwin;
	(void) what;
}

void animation_catchevents(void)
{
}

void animateResize(virtual_screen *vscr, int x, int y, int w, int h, int fx, int fy, int fw, int fh)
{
	(void) vscr;
	(void) x;
	(void) y;
	(void) w;
	(void) y;
	(void) fx;
	(void) fy;
	(void) fw;
	(void) fh;
}

void animation_maximize(WWindow *wwin)
{
	(void) wwin;
}

void animation_minimize(WWindow *wwin)
{
	(void) wwin;
}

void animation_hide(WWindow *wwin, int icon_x, int icon_y, int width, int height)
{
	(void) wwin;
	(void) icon_x;
	(void) icon_y;
	(void) width;
	(void) height;
}

void animation_unhide(WWindow *wwin, int icon_x, int icon_y, int width, int height)
{
	(void) wwin;
	(void) icon_x;
	(void) icon_y;
	(void) width;
	(void) height;
}

void animation_slide_window(Window win, int icon_x, int icon_y, int x, int y)
{
	(void) win;
	(void) icon_x;
	(void) icon_y;
	(void) x;
	(void) y;
}

int animation_iconify_window(WWindow *wwin)
{
	(void) wwin;
	return 0;
}

int animation_deiconify_window(WWindow *wwin)
{
	(void) wwin;
	return 0;
}
#endif
