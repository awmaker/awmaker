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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <wraster.h>

#include "WindowMaker.h"
#include "texture.h"
#include "window.h"
#include "misc.h"


static void bevelImage(RImage *image, int relief);
static RImage *get_texture_image(virtual_screen *vscr, const char *pixmap_file);
static WTexture *parse_texture(virtual_screen *vscr, WMPropList *pl);

WTexSolid *wTextureMakeSolid(virtual_screen *vscr, XColor *color)
{
	WScreen *scr = vscr->screen_ptr;
	WTexSolid *texture;
	int gcm;
	XGCValues gcv;

	texture = wmalloc(sizeof(WTexture));

	texture->type = WTEX_SOLID;
	texture->subtype = 0;

	XAllocColor(dpy, scr->w_colormap, color);
	texture->normal = *color;
	if (color->red == 0 && color->blue == 0 && color->green == 0) {
		texture->light.red = 0xb6da;
		texture->light.green = 0xb6da;
		texture->light.blue = 0xb6da;
		texture->dim.red = 0x6185;
		texture->dim.green = 0x6185;
		texture->dim.blue = 0x6185;
	} else {
		RColor rgb;
		RHSVColor hsv, hsv2;
		int v;

		rgb.red = color->red >> 8;
		rgb.green = color->green >> 8;
		rgb.blue = color->blue >> 8;
		RRGBtoHSV(&rgb, &hsv);
		RHSVtoRGB(&hsv, &rgb);
		hsv2 = hsv;

		v = hsv.value * 16 / 10;
		hsv.value = (v > 255 ? 255 : v);
		RHSVtoRGB(&hsv, &rgb);
		texture->light.red = rgb.red << 8;
		texture->light.green = rgb.green << 8;
		texture->light.blue = rgb.blue << 8;

		hsv2.value = hsv2.value / 2;
		RHSVtoRGB(&hsv2, &rgb);
		texture->dim.red = rgb.red << 8;
		texture->dim.green = rgb.green << 8;
		texture->dim.blue = rgb.blue << 8;
	}
	texture->dark.red = 0;
	texture->dark.green = 0;
	texture->dark.blue = 0;
	XAllocColor(dpy, scr->w_colormap, &texture->light);
	XAllocColor(dpy, scr->w_colormap, &texture->dim);
	XAllocColor(dpy, scr->w_colormap, &texture->dark);

	gcm = GCForeground | GCBackground | GCGraphicsExposures;
	gcv.graphics_exposures = False;

	gcv.background = gcv.foreground = texture->light.pixel;
	texture->light_gc = XCreateGC(dpy, scr->w_win, gcm, &gcv);

	gcv.background = gcv.foreground = texture->dim.pixel;
	texture->dim_gc = XCreateGC(dpy, scr->w_win, gcm, &gcv);

	gcv.background = gcv.foreground = texture->dark.pixel;
	texture->dark_gc = XCreateGC(dpy, scr->w_win, gcm, &gcv);

	gcv.background = gcv.foreground = color->pixel;
	texture->normal_gc = XCreateGC(dpy, scr->w_win, gcm, &gcv);

	return texture;
}

static int dummyErrorHandler(Display *foo, XErrorEvent *bar)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) foo;
	(void) bar;

	return 0;
}

void wTextureDestroy(virtual_screen *vscr, WTexture *texture)
{
	WScreen *scr = vscr->screen_ptr;
	int i;
	int count = 0;
	unsigned long colors[8];

	/* some stupid servers don't like white or black being freed... */
#define CANFREE(c) (c!=scr->black_pixel && c!=scr->white_pixel && c!=0)
	switch (texture->any.type) {
	case WTEX_SOLID:
		XFreeGC(dpy, texture->solid.light_gc);
		XFreeGC(dpy, texture->solid.dark_gc);
		XFreeGC(dpy, texture->solid.dim_gc);
		if (CANFREE(texture->solid.light.pixel))
			colors[count++] = texture->solid.light.pixel;

		if (CANFREE(texture->solid.dim.pixel))
			colors[count++] = texture->solid.dim.pixel;

		if (CANFREE(texture->solid.dark.pixel))
			colors[count++] = texture->solid.dark.pixel;

		break;

	case WTEX_PIXMAP:
		RReleaseImage(texture->pixmap.pixmap);
		break;

	case WTEX_MHGRADIENT:
	case WTEX_MVGRADIENT:
	case WTEX_MDGRADIENT:
		for (i = 0; texture->mgradient.colors[i] != NULL; i++)
			wfree(texture->mgradient.colors[i]);

		wfree(texture->mgradient.colors);
		break;

	case WTEX_THGRADIENT:
	case WTEX_TVGRADIENT:
	case WTEX_TDGRADIENT:
		RReleaseImage(texture->tgradient.pixmap);
		break;
	}

	if (CANFREE(texture->any.color.pixel))
		colors[count++] = texture->any.color.pixel;

	if (count > 0) {
		XErrorHandler oldhandler;

		/* ignore error from buggy servers that don't know how
		 * to do reference counting for colors. */
		XSync(dpy, 0);
		oldhandler = XSetErrorHandler(dummyErrorHandler);
		XFreeColors(dpy, scr->w_colormap, colors, count, 0);
		XSync(dpy, 0);
		XSetErrorHandler(oldhandler);
	}

	XFreeGC(dpy, texture->any.gc);
	wfree(texture);
#undef CANFREE
}

WTexGradient *wTextureMakeGradient(virtual_screen *vscr, int style, const RColor *from, const RColor *to)
{
	WScreen *scr = vscr->screen_ptr;
	WTexGradient *texture;
	XGCValues gcv;

	texture = wmalloc(sizeof(WTexture));
	texture->type = style;
	texture->subtype = 0;

	texture->color1 = *from;
	texture->color2 = *to;

	texture->normal.red = (from->red + to->red) << 7;
	texture->normal.green = (from->green + to->green) << 7;
	texture->normal.blue = (from->blue + to->blue) << 7;

	XAllocColor(dpy, scr->w_colormap, &texture->normal);
	gcv.background = gcv.foreground = texture->normal.pixel;
	gcv.graphics_exposures = False;
	texture->normal_gc = XCreateGC(dpy, scr->w_win, GCForeground | GCBackground | GCGraphicsExposures, &gcv);

	return texture;
}

WTexIGradient *wTextureMakeIGradient(virtual_screen *vscr, int thickness1, const RColor colors1[2],
				     int thickness2, const RColor colors2[2])
{
	WScreen *scr = vscr->screen_ptr;
	WTexIGradient *texture;
	XGCValues gcv;
	int i;

	texture = wmalloc(sizeof(WTexture));
	texture->type = WTEX_IGRADIENT;
	for (i = 0; i < 2; i++) {
		texture->colors1[i] = colors1[i];
		texture->colors2[i] = colors2[i];
	}

	texture->thickness1 = thickness1;
	texture->thickness2 = thickness2;
	if (thickness1 >= thickness2) {
		texture->normal.red = (colors1[0].red + colors1[1].red) << 7;
		texture->normal.green = (colors1[0].green + colors1[1].green) << 7;
		texture->normal.blue = (colors1[0].blue + colors1[1].blue) << 7;
	} else {
		texture->normal.red = (colors2[0].red + colors2[1].red) << 7;
		texture->normal.green = (colors2[0].green + colors2[1].green) << 7;
		texture->normal.blue = (colors2[0].blue + colors2[1].blue) << 7;
	}

	XAllocColor(dpy, scr->w_colormap, &texture->normal);
	gcv.background = gcv.foreground = texture->normal.pixel;
	gcv.graphics_exposures = False;
	texture->normal_gc = XCreateGC(dpy, scr->w_win, GCForeground | GCBackground | GCGraphicsExposures, &gcv);

	return texture;
}

WTexMGradient *wTextureMakeMGradient(virtual_screen *vscr, int style, RColor **colors)
{
	WScreen *scr = vscr->screen_ptr;
	WTexMGradient *texture;
	XGCValues gcv;
	int i;

	texture = wmalloc(sizeof(WTexture));
	texture->type = style;
	texture->subtype = 0;

	i = 0;
	while (colors[i] != NULL)
		i++;

	i--;
	texture->normal.red = (colors[0]->red << 8);
	texture->normal.green = (colors[0]->green << 8);
	texture->normal.blue = (colors[0]->blue << 8);

	texture->colors = colors;

	XAllocColor(dpy, scr->w_colormap, &texture->normal);
	gcv.background = gcv.foreground = texture->normal.pixel;
	gcv.graphics_exposures = False;
	texture->normal_gc = XCreateGC(dpy, scr->w_win, GCForeground | GCBackground | GCGraphicsExposures, &gcv);

	return texture;
}

WTexPixmap *wTextureMakePixmap(virtual_screen *vscr, int style, const char *pixmap_file, XColor *color)
{
	WScreen *scr = vscr->screen_ptr;
	WTexPixmap *texture;
	XGCValues gcv;
	RImage *image;

	image = get_texture_image(vscr, pixmap_file);
	if (!image)
		return NULL;

	texture = wmalloc(sizeof(WTexture));
	texture->type = WTEX_PIXMAP;
	texture->subtype = style;

	texture->normal = *color;

	XAllocColor(dpy, scr->w_colormap, &texture->normal);
	gcv.background = gcv.foreground = texture->normal.pixel;
	gcv.graphics_exposures = False;
	texture->normal_gc = XCreateGC(dpy, scr->w_win, GCForeground | GCBackground | GCGraphicsExposures, &gcv);

	texture->pixmap = image;

	return texture;
}

WTexTGradient *wTextureMakeTGradient(virtual_screen *vscr, int style,
				     const RColor *from, const RColor *to,
				     const char *pixmap_file, int opacity)
{
	WScreen *scr = vscr->screen_ptr;
	WTexTGradient *texture;
	XGCValues gcv;
	RImage *image;

	image = get_texture_image(vscr, pixmap_file);
	if (!image)
		return NULL;

	texture = wmalloc(sizeof(WTexture));
	texture->type = style;

	texture->opacity = opacity;

	texture->color1 = *from;
	texture->color2 = *to;

	texture->normal.red = (from->red + to->red) << 7;
	texture->normal.green = (from->green + to->green) << 7;
	texture->normal.blue = (from->blue + to->blue) << 7;

	XAllocColor(dpy, scr->w_colormap, &texture->normal);
	gcv.background = gcv.foreground = texture->normal.pixel;
	gcv.graphics_exposures = False;
	texture->normal_gc = XCreateGC(dpy, scr->w_win, GCForeground | GCBackground | GCGraphicsExposures, &gcv);

	texture->pixmap = image;

	return texture;
}

static RImage *get_texture_image(virtual_screen *vscr, const char *pixmap_file)
{
	WScreen *scr = vscr->screen_ptr;
	char *file;
	RImage *image;

	file = FindImage(wPreferences.pixmap_path, pixmap_file);
	if (!file) {
		wwarning(_("image file \"%s\" used as texture could not be found."), pixmap_file);
		return NULL;
	}

	image = RLoadImage(scr->rcontext, file, 0);
	if (!image) {
		wwarning(_("could not load texture pixmap \"%s\":%s"), file, RMessageForError(RErrorCode));
		wfree(file);
		return NULL;
	}

	wfree(file);

	return image;
}

RImage *wTextureRenderImage(WTexture *texture, int width, int height, int relief)
{
	RImage *image = NULL;
	RColor color1;
	int d;
	int subtype;

	switch (texture->any.type) {
	case WTEX_SOLID:
		image = RCreateImage(width, height, False);

		color1.red = texture->solid.normal.red >> 8;
		color1.green = texture->solid.normal.green >> 8;
		color1.blue = texture->solid.normal.blue >> 8;
		color1.alpha = 255;

		RClearImage(image, &color1);
		break;

	case WTEX_PIXMAP:
		if (texture->pixmap.subtype == WTP_TILE) {
			image = RMakeTiledImage(texture->pixmap.pixmap, width, height);
		} else if (texture->pixmap.subtype == WTP_CENTER) {
			color1.red = texture->pixmap.normal.red >> 8;
			color1.green = texture->pixmap.normal.green >> 8;
			color1.blue = texture->pixmap.normal.blue >> 8;
			color1.alpha = 255;
			image = RMakeCenteredImage(texture->pixmap.pixmap, width, height, &color1);
		} else {
			image = RScaleImage(texture->pixmap.pixmap, width, height);
		}
		break;

	case WTEX_IGRADIENT:
		image = RRenderInterwovenGradient(width, height,
						  texture->igradient.colors1,
						  texture->igradient.thickness1,
						  texture->igradient.colors2, texture->igradient.thickness2);
		break;

	case WTEX_HGRADIENT:
		subtype = RGRD_HORIZONTAL;
		goto render_gradient;

	case WTEX_VGRADIENT:
		subtype = RGRD_VERTICAL;
		goto render_gradient;

	case WTEX_DGRADIENT:
		subtype = RGRD_DIAGONAL;
 render_gradient:

		image = RRenderGradient(width, height, &texture->gradient.color1,
					&texture->gradient.color2, subtype);
		break;

	case WTEX_MHGRADIENT:
		subtype = RGRD_HORIZONTAL;
		goto render_mgradient;

	case WTEX_MVGRADIENT:
		subtype = RGRD_VERTICAL;
		goto render_mgradient;

	case WTEX_MDGRADIENT:
		subtype = RGRD_DIAGONAL;
 render_mgradient:
		image = RRenderMultiGradient(width, height, &(texture->mgradient.colors[1]), subtype);
		break;

	case WTEX_THGRADIENT:
		subtype = RGRD_HORIZONTAL;
		goto render_tgradient;

	case WTEX_TVGRADIENT:
		subtype = RGRD_VERTICAL;
		goto render_tgradient;

	case WTEX_TDGRADIENT:
		subtype = RGRD_DIAGONAL;
 render_tgradient:
		{
			RImage *grad;

			image = RMakeTiledImage(texture->tgradient.pixmap, width, height);
			if (!image)
				break;

			grad = RRenderGradient(width, height, &texture->tgradient.color1,
					       &texture->tgradient.color2, subtype);
			if (!grad) {
				RReleaseImage(image);
				image = NULL;
				break;
			}

			RCombineImagesWithOpaqueness(image, grad, texture->tgradient.opacity);
			RReleaseImage(grad);
		}
		break;
	default:
		puts("ERROR in wTextureRenderImage()");
		image = NULL;
		break;
	}

	if (!image) {
		RColor gray;

		wwarning(_("could not render texture: %s"), RMessageForError(RErrorCode));

		image = RCreateImage(width, height, False);
		if (image == NULL) {
			wwarning(_("could not allocate image buffer"));
			return NULL;
		}

		gray.red = 190;
		gray.green = 190;
		gray.blue = 190;
		gray.alpha = 255;
		RClearImage(image, &gray);
	}

	/* render bevel */

	switch (relief) {
	case WREL_ICON:
		d = RBEV_RAISED3;
		break;

	case WREL_RAISED:
		d = RBEV_RAISED2;
		break;

	case WREL_SUNKEN:
		d = RBEV_SUNKEN;
		break;

	case WREL_FLAT:
		d = 0;
		break;

	case WREL_MENUENTRY:
		d = -WREL_MENUENTRY;
		break;

	default:
		d = 0;
	}

	if (d > 0)
		RBevelImage(image, d);
	else if (d < 0)
		bevelImage(image, -d);

	return image;
}

static void bevelImage(RImage *image, int relief)
{
	int width = image->width;
	int height = image->height;
	RColor color;

	switch (relief) {
	case WREL_MENUENTRY:
		color.red = color.green = color.blue = 80;
		color.alpha = 0;
		ROperateLine(image, RAddOperation, 1, 0, width - 2, 0, &color);
		ROperateLine(image, RAddOperation, 0, 0, 0, height - 1, &color);

		color.red = color.green = color.blue = 40;
		color.alpha = 0;
		ROperateLine(image, RSubtractOperation, width - 1, 0, width - 1, height - 1, &color);
		ROperateLine(image, RSubtractOperation, 1, height - 2, width - 2, height - 2, &color);

		color.red = color.green = color.blue = 0;
		color.alpha = 255;
		RDrawLine(image, 0, height - 1, width - 1, height - 1, &color);
	}
}

void wDrawBevel(Drawable d, unsigned width, unsigned height, WTexSolid *texture, int relief)
{
	GC light, dim, dark;
	XSegment segs[4];

	switch (relief) {
	case WREL_MENUENTRY:
	case WREL_RAISED:
	case WREL_ICON:
		light = texture->light_gc;
		dim = texture->dim_gc;
		dark = texture->dark_gc;

		segs[0].x1 = 1;
		segs[0].x2 = width - 2;
		segs[0].y2 = segs[0].y1 = height - 2;
		segs[1].x1 = width - 2;
		segs[1].y1 = 1;
		segs[1].x2 = width - 2;
		segs[1].y2 = height - 2;
		if (wPreferences.new_style == TS_NEXT)
			XDrawSegments(dpy, d, dark, segs, 2);
		else
			XDrawSegments(dpy, d, dim, segs, 2);

		segs[0].x1 = 0;
		segs[0].x2 = width - 1;
		segs[0].y2 = segs[0].y1 = height - 1;
		segs[1].x1 = segs[1].x2 = width - 1;
		segs[1].y1 = 0;
		segs[1].y2 = height - 1;
		if (wPreferences.new_style == TS_NEXT)
			XDrawSegments(dpy, d, light, segs, 2);
		else
			XDrawSegments(dpy, d, dark, segs, 2);

		segs[0].x1 = segs[0].y1 = segs[0].y2 = 0;
		segs[0].x2 = width - 2;
		segs[1].x1 = segs[1].y1 = 0;
		segs[1].x2 = 0;
		segs[1].y2 = height - 2;
		if (wPreferences.new_style == TS_NEXT)
			XDrawSegments(dpy, d, dark, segs, 2);
		else
			XDrawSegments(dpy, d, light, segs, 2);

		if (relief == WREL_ICON) {
			segs[0].x1 = segs[0].y1 = segs[0].y2 = 1;
			segs[0].x2 = width - 2;
			segs[1].x1 = segs[1].y1 = 1;
			segs[1].x2 = 1;
			segs[1].y2 = height - 2;
			XDrawSegments(dpy, d, light, segs, 2);
		}
	}
}

void wDrawBevel_resizebar(Drawable d, unsigned width, unsigned height, WTexSolid *texture, int corner_width)
{
	XDrawLine(dpy, d, texture->dim_gc, 0, 0, width, 0);
	XDrawLine(dpy, d, texture->light_gc, 0, 1, width, 1);

	XDrawLine(dpy, d, texture->dim_gc, corner_width, 2, corner_width, height);
	XDrawLine(dpy, d, texture->light_gc, corner_width + 1, 2, corner_width + 1, height);

	XDrawLine(dpy, d, texture->dim_gc, width - corner_width - 2, 2, width - corner_width - 2, height);
	XDrawLine(dpy, d, texture->light_gc, width - corner_width - 1, 2, width - corner_width - 1, height);

#ifdef SHADOW_RESIZEBAR
	XDrawLine(dpy, d, texture->light_gc, 0, 1, 0, height - 1);
	XDrawLine(dpy, d, texture->dim_gc, width - 1, 2, width - 1, height - 1);
	XDrawLine(dpy, d, texture->dim_gc, 1, height - 1, corner_width, height - 1);
	XDrawLine(dpy, d, texture->dim_gc, corner_width + 2, height - 1, width - corner_width - 2, height - 1);
	XDrawLine(dpy, d, texture->dim_gc, width - corner_width, height - 1, width - 1, height - 1);
#endif
}

WTexture *get_texture_from_defstruct(virtual_screen *vscr, defstructpl *ds)
{
	char *key;
	static WTexture *texture;
	WMPropList *value, *defvalue;
	int changed = 0;

	key = ds->key;
	value = ds->value;
	defvalue = ds->defvalue;

 again:
	if (!WMIsPLArray(value)) {
		wwarning(_("Wrong option format for key \"%s\". Should be %s."), key, "Texture");
		if (changed == 0) {
			value = ds->defvalue;
			changed = 1;
			wwarning(_("using default value instead"));
			goto again;
		}
		return NULL;
	}

	if (strcmp(key, "WidgetColor") == 0 && !changed) {
		WMPropList *pl;

		pl = WMGetFromPLArray(value, 0);
		if (!pl || !WMIsPLString(pl) || !WMGetFromPLString(pl)
		    || strcasecmp(WMGetFromPLString(pl), "solid") != 0) {
			wwarning(_("Wrong option format for key \"%s\". Should be %s."),
				 key, "Solid Texture");

			value = defvalue;
			changed = 1;
			wwarning(_("using default value instead"));
			goto again;
		}
	}

	texture = parse_texture(vscr, value);

	if (!texture) {
		wwarning(_("Error in texture specification for key \"%s\""), key);
		if (changed == 0) {
			value = defvalue;
			changed = 1;
			wwarning(_("using default value instead"));
			goto again;
		}
		return NULL;
	}

	return texture;
}

/*
 * (solid <color>)
 * (hgradient <color> <color>)
 * (vgradient <color> <color>)
 * (dgradient <color> <color>)
 * (mhgradient <color> <color> ...)
 * (mvgradient <color> <color> ...)
 * (mdgradient <color> <color> ...)
 * (igradient <color1> <color1> <thickness1> <color2> <color2> <thickness2>)
 * (tpixmap <file> <color>)
 * (spixmap <file> <color>)
 * (cpixmap <file> <color>)
 * (thgradient <file> <opaqueness> <color> <color>)
 * (tvgradient <file> <opaqueness> <color> <color>)
 * (tdgradient <file> <opaqueness> <color> <color>)
 * (function <lib> <function> ...)
 */

static WTexture *parse_texture(virtual_screen *vscr, WMPropList *pl)
{
	WMPropList *elem;
	char *val;
	int nelem;
	WTexture *texture = NULL;

	nelem = WMGetPropListItemCount(pl);
	if (nelem < 1)
		return NULL;

	elem = WMGetFromPLArray(pl, 0);
	if (!elem || !WMIsPLString(elem))
		return NULL;
	val = WMGetFromPLString(elem);

	if (strcasecmp(val, "solid") == 0) {
		XColor color;

		if (nelem != 2)
			return NULL;

		/* get color */

		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return NULL;
		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &color)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		texture = (WTexture *) wTextureMakeSolid(vscr, &color);
	} else if (strcasecmp(val, "dgradient") == 0
		   || strcasecmp(val, "vgradient") == 0 || strcasecmp(val, "hgradient") == 0) {
		RColor color1, color2;
		XColor xcolor;
		int type;

		if (nelem != 3) {
			wwarning(_("bad number of arguments in gradient specification"));
			return NULL;
		}

		if (val[0] == 'd' || val[0] == 'D')
			type = WTEX_DGRADIENT;
		else if (val[0] == 'h' || val[0] == 'H')
			type = WTEX_HGRADIENT;
		else
			type = WTEX_VGRADIENT;

		/* get from color */
		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return NULL;
		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		color1.alpha = 255;
		color1.red = xcolor.red >> 8;
		color1.green = xcolor.green >> 8;
		color1.blue = xcolor.blue >> 8;

		/* get to color */
		elem = WMGetFromPLArray(pl, 2);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		color2.alpha = 255;
		color2.red = xcolor.red >> 8;
		color2.green = xcolor.green >> 8;
		color2.blue = xcolor.blue >> 8;

		texture = (WTexture *) wTextureMakeGradient(vscr, type, &color1, &color2);
	} else if (strcasecmp(val, "igradient") == 0) {
		RColor colors1[2], colors2[2];
		int th1, th2;
		XColor xcolor;
		int i;

		if (nelem != 7) {
			wwarning(_("bad number of arguments in gradient specification"));
			return NULL;
		}

		/* get from color */
		for (i = 0; i < 2; i++) {
			elem = WMGetFromPLArray(pl, 1 + i);
			if (!elem || !WMIsPLString(elem))
				return NULL;
			val = WMGetFromPLString(elem);

			if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
				wwarning(_("\"%s\" is not a valid color name"), val);
				return NULL;
			}

			colors1[i].alpha = 255;
			colors1[i].red = xcolor.red >> 8;
			colors1[i].green = xcolor.green >> 8;
			colors1[i].blue = xcolor.blue >> 8;
		}

		elem = WMGetFromPLArray(pl, 3);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);
		th1 = atoi(val);

		/* get from color */
		for (i = 0; i < 2; i++) {
			elem = WMGetFromPLArray(pl, 4 + i);
			if (!elem || !WMIsPLString(elem))
				return NULL;

			val = WMGetFromPLString(elem);

			if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
				wwarning(_("\"%s\" is not a valid color name"), val);
				return NULL;
			}

			colors2[i].alpha = 255;
			colors2[i].red = xcolor.red >> 8;
			colors2[i].green = xcolor.green >> 8;
			colors2[i].blue = xcolor.blue >> 8;
		}

		elem = WMGetFromPLArray(pl, 6);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);
		th2 = atoi(val);
		texture = (WTexture *) wTextureMakeIGradient(vscr, th1, colors1, th2, colors2);
	} else if (strcasecmp(val, "mhgradient") == 0
		   || strcasecmp(val, "mvgradient") == 0 || strcasecmp(val, "mdgradient") == 0) {
		XColor color;
		RColor **colors;
		int i, count;
		int type;

		if (nelem < 3) {
			wwarning(_("too few arguments in multicolor gradient specification"));
			return NULL;
		}

		if (val[1] == 'h' || val[1] == 'H')
			type = WTEX_MHGRADIENT;
		else if (val[1] == 'v' || val[1] == 'V')
			type = WTEX_MVGRADIENT;
		else
			type = WTEX_MDGRADIENT;

		count = nelem - 1;

		colors = wmalloc(sizeof(RColor *) * (count + 1));

		for (i = 0; i < count; i++) {
			elem = WMGetFromPLArray(pl, i + 1);
			if (!elem || !WMIsPLString(elem)) {
				for (--i; i >= 0; --i)
					wfree(colors[i]);

				wfree(colors);
				return NULL;
			}

			val = WMGetFromPLString(elem);

			if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &color)) {
				wwarning(_("\"%s\" is not a valid color name"), val);
				for (--i; i >= 0; --i)
					wfree(colors[i]);

				wfree(colors);
				return NULL;
			} else {
				colors[i] = wmalloc(sizeof(RColor));
				colors[i]->red = color.red >> 8;
				colors[i]->green = color.green >> 8;
				colors[i]->blue = color.blue >> 8;
			}
		}

		colors[i] = NULL;
		texture = (WTexture *) wTextureMakeMGradient(vscr, type, colors);
	} else if (strcasecmp(val, "spixmap") == 0 ||
		   strcasecmp(val, "cpixmap") == 0 || strcasecmp(val, "tpixmap") == 0) {
		XColor color;
		int type;

		if (nelem != 3)
			return NULL;

		if (val[0] == 's' || val[0] == 'S')
			type = WTP_SCALE;
		else if (val[0] == 'c' || val[0] == 'C')
			type = WTP_CENTER;
		else
			type = WTP_TILE;

		/* get color */
		elem = WMGetFromPLArray(pl, 2);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &color)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		/* file name */
		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		texture = (WTexture *) wTextureMakePixmap(vscr, type, val, &color);
	} else if (strcasecmp(val, "thgradient") == 0
		   || strcasecmp(val, "tvgradient") == 0 || strcasecmp(val, "tdgradient") == 0) {
		RColor color1, color2;
		XColor xcolor;
		int opacity;
		int style;

		if (val[1] == 'h' || val[1] == 'H')
			style = WTEX_THGRADIENT;
		else if (val[1] == 'v' || val[1] == 'V')
			style = WTEX_TVGRADIENT;
		else
			style = WTEX_TDGRADIENT;

		if (nelem != 5) {
			wwarning(_("bad number of arguments in textured gradient specification"));
			return NULL;
		}

		/* get from color */
		elem = WMGetFromPLArray(pl, 3);
		if (!elem || !WMIsPLString(elem))
			return NULL;
		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		color1.alpha = 255;
		color1.red = xcolor.red >> 8;
		color1.green = xcolor.green >> 8;
		color1.blue = xcolor.blue >> 8;

		/* get to color */
		elem = WMGetFromPLArray(pl, 4);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		if (!XParseColor(dpy, vscr->screen_ptr->w_colormap, val, &xcolor)) {
			wwarning(_("\"%s\" is not a valid color name"), val);
			return NULL;
		}

		color2.alpha = 255;
		color2.red = xcolor.red >> 8;
		color2.green = xcolor.green >> 8;
		color2.blue = xcolor.blue >> 8;

		/* get opacity */
		elem = WMGetFromPLArray(pl, 2);
		if (!elem || !WMIsPLString(elem))
			opacity = 128;
		else
			val = WMGetFromPLString(elem);

		if (!val || (opacity = atoi(val)) < 0 || opacity > 255) {
			wwarning(_("bad opacity value for tgradient texture \"%s\". Should be [0..255]"), val);
			opacity = 128;
		}

		/* get file name */
		elem = WMGetFromPLArray(pl, 1);
		if (!elem || !WMIsPLString(elem))
			return NULL;

		val = WMGetFromPLString(elem);

		texture = (WTexture *) wTextureMakeTGradient(vscr, style, &color1, &color2, val, opacity);
	} else if (strcasecmp(val, "function") == 0) {
		/* Leave this in to handle the unlikely case of
		 * someone actually having function textures configured */
		wwarning("function texture support has been removed");
		return NULL;
	} else {
		wwarning(_("invalid texture type %s"), val);
		return NULL;
	}

	return texture;
}
