/*
 * Xiphos Bible Study Tool
 * navbar.cc - glue between all navbars and sword
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gtk/gtk.h>
#include "main/module_dialogs.h"
#include "main/navbar.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"

//#include "gui/toolbar_nav.h"
#include "gui/commentary_dialog.h"
#include "gui/bibletext_dialog.h"

#include "backend/bible_backend.h"

#include "gui/debug_glib_null.h"

//#ifdef OLD_NAVBAR

gboolean do_display;
gboolean do_display_dict;

void main_navbar_set(NAVBAR navbar, const char *key)
{
	char *gkey = NULL;
	int book;
	GtkTreeIter iter;
	gint i, x;

	if (!navbar.module_name)
		return;

	BibleKeyInfo key_info;
	if (!bible_backend->resolveKey(navbar.module_name, key ? key : "", key_info))
		return;

	navbar.key = g_strdup(key_info.key.c_str());
	if (!navbar.is_dialog) {
	}
	GtkTreeModel *chapter_store = gtk_combo_box_get_model(
	    GTK_COMBO_BOX(navbar.comboboxentry_chapter));
	GtkTreeModel *verse_store = gtk_combo_box_get_model(
	    GTK_COMBO_BOX(navbar.comboboxentry_verse));

	do_display = FALSE;

	XI_message(("DEBUG navbar: module=%s key_in=%s testament=%d book=%d chapter=%d/%d verse=%d/%d text=%s\n",
		    navbar.module_name, key,
		    key_info.reference.testament, key_info.reference.book,
		    key_info.reference.chapter, key_info.chapterCount,
		    key_info.reference.verse, key_info.verseCount,
		    key_info.key.c_str()));
		
	// we need the book index to highlight "active" in the pulldown.
	book = key_info.bookIndex;

	gtk_combo_box_set_active((GtkComboBox *)navbar.comboboxentry_book,
				 book - 1);

	gtk_list_store_clear(GTK_LIST_STORE(chapter_store));

	int xchapter = key_info.reference.chapter;
	int xverse = key_info.reference.verse;

	x = key_info.chapterCount;
	for (i = 1; i <= x; i++) {
		char *num = main_format_number(i);
		gtk_list_store_append(GTK_LIST_STORE(chapter_store), &iter);
		gtk_list_store_set(GTK_LIST_STORE(chapter_store),
				   &iter,
				   0,
				   num,
				   -1);
		g_free(num);
	}
	gtk_combo_box_set_active((GtkComboBox *)navbar.comboboxentry_chapter,
				 xchapter - 1);

	gtk_list_store_clear(GTK_LIST_STORE(verse_store));

	x = key_info.verseCount;
	for (i = 1; i <= x; i++) {
		char *num = main_format_number(i);
		gtk_list_store_append(GTK_LIST_STORE(verse_store), &iter);
		gtk_list_store_set(GTK_LIST_STORE(verse_store),
				   &iter,
				   0,
				   num,
				   -1);
		g_free(num);
	}
	gtk_combo_box_set_active((GtkComboBox *)navbar.comboboxentry_verse,
				 xverse - 1);
	gtk_entry_set_text(GTK_ENTRY(navbar.lookup_entry),
			   navbar.key);
	do_display = TRUE;
	g_free(gkey);

}
