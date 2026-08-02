/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TEXTUREPANEL_H_
#define TEXTUREPANEL_H_



typedef struct _TexturePanel TexturePanel;


TexturePanel *CreateTexturePanel(WMWindow *keyWindow);

void ShowTexturePanel(TexturePanel *panel);

void HideTexturePanel(TexturePanel *panel);

void SetTexturePanelTexture(TexturePanel *panel, const char *name,
                            WMPropList *texture);


char *GetTexturePanelTextureName(TexturePanel *panel);

WMPropList *GetTexturePanelTexture(TexturePanel *panel);

RImage *RenderTexturePanelTexture(TexturePanel *panel, unsigned width,
                                  unsigned height);

void SetTexturePanelOkAction(TexturePanel *panel, WMCallback *action,
                             void *clientData);

void SetTexturePanelCancelAction(TexturePanel *panel, WMCallback *action,
                                 void *clientData);

void SetTexturePanelPixmapPath(TexturePanel *panel, WMPropList *array);

#endif

