/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
 * and individual contributors; see LICENSE for full attribution.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WMKEYTREE_H
#define WMKEYTREE_H

#include <X11/Xlib.h>

struct SHBinding;

/*
 * A key-chain trie (F5, §8F5). Each node is one key of a binding sequence.
 * Internal nodes (first_child != NULL) represent a typed prefix; leaf nodes
 * carry the action payload. The trie is rebuilt from the SHBinding list
 * (shbinding.c), never from a materialized menu, so leaves cannot dangle when
 * the root menu is rebuilt — the F5 SIGSEGV root cause.
 */

typedef enum {
	WKN_SHBINDING   /* action: pointer to an SHBinding in the list */
} WKeyActionType;

/*
 * A single action attached to a trie leaf node. Multiple bindings may share
 * the same key sequence and are chained through 'next' (insertion order).
 */
typedef struct WKeyAction {
	WKeyActionType type;
	union {
		struct SHBinding *binding;
	} u;
	struct WKeyAction *next;
} WKeyAction;

typedef struct WKeyNode {
	unsigned int modifier;
	KeyCode keycode;

	WKeyAction *actions;   /* non-NULL only for leaf nodes */

	struct WKeyNode *parent;
	struct WKeyNode *first_child;
	struct WKeyNode *next_sibling;
} WKeyNode;

/* Global trie root. */
extern WKeyNode *wKeyTreeRoot;

/*
 * Insert a key sequence into *root.
 *   mods[0]/keys[0] - first (leader) key
 *   mods[1..n-1]/keys[1..n-1] - follower keys
 * Shared prefixes are merged automatically. Returns the leaf node.
 * Returns NULL if nkeys <= 0.
 */
WKeyNode *wKeyTreeInsert(WKeyNode **root, unsigned int *mods, KeyCode *keys, int nkeys);

/*
 * Find the first sibling in the list starting at 'siblings' that matches
 * (mod, key). Returns NULL if not found.
 */
WKeyNode *wKeyTreeFind(WKeyNode *siblings, unsigned int mod, KeyCode key);

/*
 * Recursively free the entire subtree rooted at 'node'.
 */
void wKeyTreeDestroy(WKeyNode *node);

/*
 * Allocate a new WKeyAction of type WKN_SHBINDING, append it to leaf->actions,
 * set its payload to 'b', and return it. (Assumes the caller will implement
 * WKeyNodeFind for chains when wiring the executor in F5-I.)
 */
WKeyAction *wKeyNodeAddBinding(WKeyNode *leaf, struct SHBinding *b);

#endif /* WMKEYTREE_H */
