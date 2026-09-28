/*
 * Xiphos Bible Study Tool
 * sword_treekey.cc - treekey stuff for book editor
 *
 * Copyright (C) 2008-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <swmodule.h>
#include <swmgr.h>
#include <treekeyidx.h>
#include <rawgenbook.h>
#include <swconfig.h>
#include <iostream>
#include <string>
#include <stdio.h>

#include "main/mod_mgr.h"
#include "main/sword_treekey.h"
#include "main/settings.h"
#include "main/sidebar.h"
#include "main/sword.h"

#include "gui/dialog.h"
#include "gui/sidebar.h"

#include "backend/sword_main.hh"

#ifndef NO_SWORD_NAMESPACE
using sword::TreeKeyIdx;
using sword::RawGenBook;
using sword::SWKey;
using sword::SWConfig;
using sword::SWModule;
#endif


static char *mod_name;
static gchar buf[256];
static gchar *tmpbuf;

/* ROW takes what TREEKEY says: a branch or a leaf. */
static void set_row(ElimRow *row, TreeKeyIdx &treeKey)
{
	main_mod_tree_set(row,
			  treeKey.hasChildren() ? MOD_TREE_ICON_CLOSED : MOD_TREE_ICON_LEAF,
			  tmpbuf, mod_name, buf);
}

static void load_treeview(GListStore *roots, ElimRow *row,
			  TreeKeyIdx treeKey, int level = 1)
{
	/*if (!target)
		target = &treeKey;*/

	sprintf(buf, "%lu", treeKey.getOffset());
	tmpbuf = (char *)treeKey.getLocalName();
	if (atol(buf) == 0)
		tmpbuf = mod_name;
	set_row(row, treeKey);

	if (treeKey.firstChild()) {
		load_treeview(roots, main_mod_tree_add(roots, row, NULL, MOD_TREE_ICON_LEAF,
						       "", "", ""),
			      treeKey, level + 1);
		treeKey.parent();
	}
	if (treeKey.nextSibling())
		load_treeview(roots, main_mod_tree_add(roots, elim_row_get_parent(row), row,
						       MOD_TREE_ICON_LEAF, "", "", ""),
			      treeKey, level);
}

/*********************************    *************************************/
void setEntryText(RawGenBook *book, const gchar *text)
{
	TreeKeyIdx *treeKey = (TreeKeyIdx *)(SWKey *)(*book);
	if (treeKey->getOffset()) {

		(*book) << text;
	}
}

void appendChild(TreeKeyIdx *treeKey, const gchar *name)
{
	treeKey->appendChild();
	treeKey->setLocalName(name);
	treeKey->save();
	XI_message(("name: %s\nlocalName: %s", name,
		    treeKey->getLocalName()));
}

void setLocalName(TreeKeyIdx *treeKey, char *new_name)
{

	treeKey->setLocalName(buf);
	treeKey->save();
}

unsigned long main_treekey_remove(char *book, char *name, char *offset)
{
	SWMgr *mgr = backend->get_mgr();
	SWModule *mod = mgr->Modules[book];

	if (!g_ascii_isdigit(offset[0]))
		return 0; /* make sure offset is a number */

	if (!mod)
		return 0;

	TreeKeyIdx *treekey = (TreeKeyIdx *)mod->createKey();
	treekey->setOffset(atol(offset));
	mod->setKey(treekey);
	mod->getKeyText(); //snap to entry
	mod->deleteEntry();
	treekey->remove();
	return treekey->getOffset();
}

unsigned long main_treekey_append_sibling(char *book, char *name, char *offset)
{
	SWMgr *mgr = backend->get_mgr();
	SWModule *mod = mgr->Modules[book];

	if (!g_ascii_isdigit(offset[0]))
		return 0; /* make sure offset is a number */

	if (!mod)
		return 0;

	TreeKeyIdx *treekey = (TreeKeyIdx *)mod->createKey();
	treekey->setOffset(atol(offset));
	XI_message(("offset1: %ld", treekey->getOffset()));
	treekey->append();
	treekey->setLocalName(name);
	treekey->save();

	mod->setKey(treekey);
	mod->getKeyText(); //snap to entry
	(*mod) << name;

	XI_message(("offset2: %ld", treekey->getOffset()));
	return treekey->getOffset();
}

unsigned long main_treekey_append_child(char *book, char *name, char *offset)
{
	SWMgr *mgr = backend->get_mgr();
	SWModule *mod = mgr->Modules[book];

	if (!g_ascii_isdigit(offset[0]))
		return 0; /* make sure offset is a number */

	if (!mod)
		return 0;

	TreeKeyIdx *treekey = (TreeKeyIdx *)mod->createKey();
	treekey->setOffset(atol(offset));
	treekey->appendChild();
	treekey->setLocalName(name);
	treekey->save();

	mod->setKey(treekey);
	mod->getKeyText(); //snap to entry
	(*mod) << name;
	XI_message(("book: %s\nlocalName: %s\noffset :%s", book, name,
		    offset));
	return treekey->getOffset();
}

void main_treekey_set_local_name(char *book, char *name, char *offset)
{
	SWMgr *mgr = backend->get_mgr();
	SWModule *mod = mgr->Modules[book];

	if (!g_ascii_isdigit(offset[0]))
		return; /* make sure offset is a number */

	if (!mod)
		return;

	TreeKeyIdx *treekey = (TreeKeyIdx *)mod->createKey();
	treekey->setOffset(atol(offset));
	treekey->setLocalName(name);
	treekey->save();

	XI_message(("book: %s\nlocalName: %s\noffset :%s", book, name,
		    offset));
}

char *main_get_book_raw_text(char *book, char *offset)
{
	SWMgr *mgr = backend->get_mgr();
	SWModule *mod = mgr->Modules[book];

	if (!g_ascii_isdigit(offset[0]))
		return NULL; /* make sure offset is a number */

	if (!mod)
		return NULL;

	TreeKeyIdx *treekey = (TreeKeyIdx *)mod->createKey();
	TreeKeyIdx treenode = *treekey;
	treenode.setOffset(atol(offset));
	mod->setKey(treenode);
	mod->getKeyText(); //snap to entry

	return strdup(mod->getRawEntry());
}

void main_treekey_save_book_text(char *book, char *offset, char *text)
{
	SWMgr *mgr = backend->get_mgr();
	SWModule *mod = mgr->Modules[book];

	if (!offset || !g_ascii_isdigit(offset[0]))
		return; /* make sure offset is a number */

	if (!mod || (atol(offset) == 0))
		return;

	TreeKeyIdx *treekey = (TreeKeyIdx *)mod->createKey();
	TreeKeyIdx treenode = *treekey;
	treenode.setOffset(atol(offset));
	mod->setKey(treenode);
	mod->getKeyText(); //snap to entry
	(*mod) << text;

	// on recommendation of cppcheck, test of "book" for non-NULL
	// was removed here.  this should be ok because above we use
	// book as Modules subscript.  so both or neither blows up.
	if (settings.book_mod && !strcmp(settings.book_mod, book)) {
		XI_message(("main_treekey_save_book_text"));
		main_display_book(book, offset);
	}
}

/*********************************    *************************************/

void main_load_book_tree_in_editor(GtkWidget *treeview, char *book)
{
	GListStore *store;
	SWMgr *mgr = backend->get_mgr();
	SWModule *mod = mgr->Modules[book];

	XI_message(("main_load_book_tree_in_editor book: %s", book));

	if (!mod)
		return;
	tmpbuf = book;
	mod_name = book;
	TreeKeyIdx root = *((TreeKeyIdx *)mod->createKey());
	root.root();

	store = elim_table_new();
	main_setup_mod_tree_view(treeview, store);
	load_treeview(store, main_mod_tree_add(store, NULL, NULL, MOD_TREE_ICON_LEAF,
					       "", "", ""),
		      root);
	g_object_unref(store);
}
