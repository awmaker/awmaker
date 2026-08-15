/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Alfredo K. Kojima, Dan Pascu, the Window Maker Team,
 * and individual contributors; see LICENSE for full attribution.
 * Fork modifications: Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KEYBINDS_META_H_
#define KEYBINDS_META_H_

/*
 * One entry per built-in window-manager keybinding.
 *
 * `key`   - the defaults-domain key string (as read/written in ~/GNUstep/Defaults)
 * `wkbd`  - the WKBD_* enum index (see keybinds.h), -1 when the binding has no WKBD
 * `title` - the canonical translatable (N_()) title shown to the user
 *
 * This is the single source of truth: WPrefs.app/KeyboardShortcuts.c (was
 * keyOptions[]), src/dialog_keybinds.c (was wkbd_name[]) and the keybind rows
 * of src/defaults.c (optionList) all derive from this table.
 */
typedef struct {
        const char *key;
        int wkbd;
        const char *title;
} KeyBindingMeta;

extern const KeyBindingMeta keybinds_meta[];
extern const int nb_keybindings;

/* The canonical title (already N_()'d; still wrapped in _() by the caller). */
const char *KeyBindingTitle(int wkbd);
/* The defaults-domain key string for a WKBD_* index, or NULL if unknown. */
const char *KeyBindingForKey(int wkbd);
/* The WKBD_* index for a defaults-domain key string, or -1 if unknown. */
int WkbdForKey(const char *key);

#endif /* KEYBINDS_META_H_ */
