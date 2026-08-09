/*
 * awmaker - Abstracting Window Maker
 *
 * Fork of GNU Window Maker (GPL-2).
 * Copyright (C) Rodolfo Garcia Penas (kix) <kix@kix.es>
 * and individual contributors; see LICENSE for full attribution.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wconfig.h"

#include <string.h>

#include <WINGs/WUtil.h>

#include "keytree.h"

/* Global trie root. */
WKeyNode *wKeyTreeRoot = NULL;

WKeyNode *wKeyTreeFind(WKeyNode *siblings, unsigned int mod, KeyCode key)
{
	WKeyNode *p = siblings;

	while (p != NULL) {
		if (p->modifier == mod && p->keycode == key)
			return p;
		p = p->next_sibling;
	}
	return NULL;
}

WKeyNode *wKeyTreeInsert(WKeyNode **root, unsigned int *mods, KeyCode *keys, int nkeys)
{
	WKeyNode **slot = root;
	WKeyNode *parent = NULL;
	int i;

	if (nkeys <= 0)
		return NULL;

	for (i = 0; i < nkeys; i++) {
		WKeyNode *node = wKeyTreeFind(*slot, mods[i], keys[i]);

		if (node == NULL) {
			node = wmalloc(sizeof(WKeyNode));
			memset(node, 0, sizeof(WKeyNode));
			node->modifier = mods[i];
			node->keycode = keys[i];
			node->parent = parent;
			node->next_sibling = *slot;
			*slot = node;
		}

		parent = node;
		slot = &node->first_child;
	}

	return parent;   /* leaf */
}

void wKeyTreeDestroy(WKeyNode *node)
{
	while (node != NULL) {
		WKeyNode *next = node->next_sibling;
		WKeyAction *act, *next_act;

		wKeyTreeDestroy(node->first_child);
		for (act = node->actions; act != NULL; act = next_act) {
			next_act = act->next;
			wfree(act);
		}
		wfree(node);
		node = next;
	}
}

WKeyAction *wKeyNodeAddBinding(WKeyNode *leaf, struct SHBinding *b)
{
	WKeyAction *act = wmalloc(sizeof(WKeyAction));
	WKeyAction *p;

	memset(act, 0, sizeof(WKeyAction));
	act->type = WKN_SHBINDING;
	act->u.binding = b;

	/* Append to end of list to preserve insertion order */
	if (leaf->actions == NULL) {
		leaf->actions = act;
	} else {
		p = leaf->actions;
		while (p->next)
			p = p->next;
		p->next = act;
	}
	return act;
}
