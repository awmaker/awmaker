/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef WMTEXTURE_H_
#define WMTEXTURE_H_

#include "screen.h"
#include "wcore.h"

/* texture relief */
#define WREL_RAISED	0
#define WREL_SUNKEN	1
#define WREL_FLAT	2
#define WREL_ICON	4
#define WREL_MENUENTRY	6

/* texture types */
#define WREL_BORDER_MASK	1

#define WTEX_SOLID 	((1<<1)|WREL_BORDER_MASK)
#define WTEX_HGRADIENT	((1<<2)|WREL_BORDER_MASK)
#define WTEX_VGRADIENT	((1<<3)|WREL_BORDER_MASK)
#define WTEX_DGRADIENT	((1<<4)|WREL_BORDER_MASK)
#define WTEX_MHGRADIENT	((1<<5)|WREL_BORDER_MASK)
#define WTEX_MVGRADIENT	((1<<6)|WREL_BORDER_MASK)
#define WTEX_MDGRADIENT	((1<<7)|WREL_BORDER_MASK)
#define WTEX_IGRADIENT	((1<<8)|WREL_BORDER_MASK)
#define WTEX_PIXMAP	(1<<10)
#define WTEX_THGRADIENT	((1<<11)|WREL_BORDER_MASK)
#define WTEX_TVGRADIENT	((1<<12)|WREL_BORDER_MASK)
#define WTEX_TDGRADIENT	((1<<13)|WREL_BORDER_MASK)
#define WTEX_FUNCTION	((1<<14)|WREL_BORDER_MASK)

/* pixmap subtypes */
#define WTP_TILE	2
#define WTP_SCALE	4
#define WTP_CENTER	6


typedef struct {
    short type;			       /* type of texture */
    char subtype;		       /* subtype of the texture */
    XColor color;		       /* default background color */
    GC gc;			       /* gc for the background color */
} WTexAny;


typedef struct WTexSolid {
    short type;
    char subtype;
    XColor normal;
    GC normal_gc;

    GC light_gc;
    GC dim_gc;
    GC dark_gc;

    XColor light;
    XColor dim;
    XColor dark;
} WTexSolid;


typedef struct WTexGradient {
    short type;
    char subtype;
    XColor normal;
    GC normal_gc;

    RColor color1;
    RColor color2;
} WTexGradient;


typedef struct WTexMGradient {
    short type;
    char subtype;
    XColor normal;
    GC normal_gc;

    RColor **colors;
} WTexMGradient;


typedef struct WTexIGradient {
    short type;
    char dummy;
    XColor normal;
    GC normal_gc;

    RColor colors1[2];
    RColor colors2[2];
    int thickness1;
    int thickness2;
} WTexIGradient;


typedef struct WTexPixmap {
    short type;
    char subtype;
    XColor normal;
    GC normal_gc;

    struct RImage *pixmap;
} WTexPixmap;

typedef struct WTexTGradient {
    short type;
    char subtype;
    XColor normal;
    GC normal_gc;

    RColor color1;
    RColor color2;
    struct RImage *pixmap;
    int opacity;
} WTexTGradient;

typedef struct WTexFunction {
    short type;
    char subtype;
    XColor normal;
    GC normal_gc;

    void *handle;
    RImage *(*render) (int, char**, int, int, int);
    int argc;
    char **argv;
} WTexFunction;

typedef union WTexture {
    WTexAny any;
    WTexSolid solid;
    WTexGradient gradient;
    WTexIGradient igradient;
    WTexMGradient mgradient;
    WTexPixmap pixmap;
    WTexTGradient tgradient;
    WTexFunction function;
} WTexture;


WTexture *get_texture_from_defstruct(virtual_screen *vscr, defstructpl *ds);
WTexSolid *wTextureMakeSolid(virtual_screen *vscr, XColor *color);
WTexGradient *wTextureMakeGradient(virtual_screen *vscr, int, const RColor *from, const RColor *to);
WTexMGradient *wTextureMakeMGradient(virtual_screen *vscr, int style, RColor **colors);
WTexTGradient *wTextureMakeTGradient(virtual_screen *vscr, int style, const RColor *from, const RColor *to,
				     const char *pixmap_file, int opacity);
WTexIGradient *wTextureMakeIGradient(virtual_screen *vscr, int thickness1, const RColor colors1[], int thickness2, const RColor colors2[]);
WTexPixmap *wTextureMakePixmap(virtual_screen *vscr, int style, const char *pixmap_file, XColor *color);
void wTextureDestroy(virtual_screen *vscr, WTexture *texture);
struct RImage *wTextureRenderImage(WTexture *texture, int width, int height, int relief);

void wDrawBevel(Drawable d, unsigned width, unsigned height, WTexSolid *texture, int relief);
void wDrawBevel_resizebar(Drawable d, unsigned width, unsigned height, WTexSolid *texture, int corner_width);

#endif
