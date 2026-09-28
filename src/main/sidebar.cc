/*
 * Xiphos Bible Study Tool
 * sidebar.cc - sidebar interface to sword
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gtk/gtk.h>
#include <swmgr.h>
#include <swmodule.h>

#include "gui/xiphos.h"
#include "gui/main_menu.h"
#include "gui/widgets.h"
#include "gui/sidebar.h"
#include "gui/table_helpers.h"
#include "gui/tabbed_browser.h"
#include "gui/utilities.h"
#include "gui/dialog.h"

#include "main/backend_access.h"
#include "main/sidebar.h"
#include "main/configs.h"
#include "main/lists.h"
#include "main/mod_mgr.h"
#include "main/navbar.h"
#include "main/navbar_book.h"
#include "main/sword_treekey.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"
#include "main/xml.h"

#include "backend/sword_main.hh"

#include "gui/debug_glib_null.h"

enum {
	COL_OPEN_PIXBUF,
	COL_CLOSED_PIXBUF,
	COL_CAPTION,
	COL_MODULE,
	COL_OFFSET,
	N_COLUMNS
};

TreePixbufs *pixbufs;

/******************************************************************************
 * Name
 *   main_open_bookmark_in_new_tab
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void main_open_bookmark_in_new_tab(gchar * mod_name, gchar * key)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void main_open_bookmark_in_new_tab(gchar *mod_name, gchar *key)
{
	gint module_type = main_get_mod_type(mod_name);

	switch (module_type) {
	case -1:
		return;
		break;
	case TEXT_TYPE:
	case COMMENTARY_TYPE:
		if (strcmp(settings.currentverse, key)) {
			xml_set_value("Xiphos", "keys", "verse",
				      key);
			settings.currentverse =
			    xml_get_value("keys", "verse");
		}
		break;
	case DICTIONARY_TYPE:
		xml_set_value("Xiphos", "keys", "dictionary", key);
		settings.dictkey = xml_get_value("keys", "dictionary");
		break;
	case BOOK_TYPE:
		xml_set_value("Xiphos", "keys", "offset", key);
		settings.book_offset =
		    atol(xml_get_value("keys", "offset"));
		break;
	}
	gui_open_module_in_new_tab(mod_name);
}

/******************************************************************************
 * Name
 *   main_display_verse_list_in_sidebar
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void main_display_verse_list_in_sidebar(gchar * key, gchar * module_name,
 *				       gchar * verse_list)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void main_display_verse_list_in_sidebar(gchar *key,
					gchar *module_name,
					gchar *verse_list)
{
	GList *tmp = NULL;
	GListStore *list_store;
	RESULTS *list_item;

	/* improper xref encoding, not BCV: gets null list from Sword. */
	if (verse_list == NULL) {
		gui_generic_warning(_("Module error: Unusable xref encoding.\n"
				      "Please report this, with module & verse"));
		return;
	}

	is_search_result = FALSE;

	XI_warning(("verse_list = %s", verse_list));
	list_of_verses = g_list_first(list_of_verses);
	if (list_of_verses) {
		GList *chaser = list_of_verses;
		while (chaser) {
			list_item = (RESULTS *)chaser->data;
			g_free(list_item->module);
			g_free(list_item->key);
			g_free(list_item);
			chaser = g_list_next(chaser);
		}
		g_list_free(list_of_verses);
		list_of_verses = NULL;
	}

	g_strlcpy(settings.sb_search_mod, module_name,
		  sizeof(settings.sb_search_mod));

	list_store = elim_table_get_store(sidebar.results_list);
	g_list_store_remove_all(list_store);

	g_strlcpy(sidebar.mod_name, module_name,
		  sizeof(sidebar.mod_name));

	if (!strncmp(verse_list, "See ", 4))
		verse_list += 4;

	if ((*verse_list != '/') &&
	    ((tmp = main_parse_verse_list(module_name, verse_list, key)) != NULL)) {
		// normal verse list.
		while (tmp != NULL) {
			ElimRow *row = elim_row_new(1);

			elim_row_set_string(row, 0, (const char *)tmp->data);
			g_list_store_append(list_store, row);
			g_object_unref(row);

			list_item = g_new(RESULTS, 1);
			list_item->module = g_strdup(module_name);
			list_item->key = g_strdup((const char *)tmp->data);
			list_of_verses = g_list_append(list_of_verses,
						       (RESULTS *)list_item);

			tmp = g_list_next(tmp);
		}
	} else {
		// not normal verse list.  probably a genbook or dict key.
		ElimRow *row = elim_row_new(1);

		elim_row_set_string(row, 0, verse_list);
		g_list_store_append(list_store, row);
		g_object_unref(row);

		list_item = g_new(RESULTS, 1);
		list_item->module = g_strdup(module_name);
		list_item->key = g_strdup((const char *)verse_list);
		list_of_verses = g_list_append(list_of_verses,
					       (RESULTS *)list_item);
	}

	if (!g_list_model_get_n_items(G_LIST_MODEL(list_store)))
		return;
	gui_sidebar_results_menu_set_enabled(TRUE);
	elim_table_select(sidebar.results_list, 0, FALSE);

	gui_verselist_button_release_event(NULL, NULL, NULL);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_sidebar), 3);
	gtk_widget_grab_focus(GTK_WIDGET(sidebar.results_list));
}

/* The rows of the module tree: an icon, the caption, and what it opens (a
 * module, and for a book module the offset of its entry). */
static GdkTexture *texture_opened, *texture_closed, *texture_helpdoc;

static GListStore *module_roots(void)
{
	return elim_table_get_store(sidebar.module_list);
}

static ElimRow *add_row(GListStore *roots, ElimRow *parent, GdkTexture *icon,
			const gchar *caption, const gchar *module,
			const gchar *offset)
{
	ElimRow *row = elim_tree_append(roots, parent, N_COLUMNS);

	elim_row_set_object(row, COL_OPEN_PIXBUF, G_OBJECT(icon));
	elim_row_set_string(row, COL_CAPTION, caption);
	elim_row_set_string(row, COL_MODULE, module);
	elim_row_set_string(row, COL_OFFSET, offset);
	return row;
}

static GdkTexture *icon_texture(int icon)
{
	main_create_pixbufs();
	return icon == MOD_TREE_ICON_LEAF ? texture_helpdoc
	       : icon == MOD_TREE_ICON_CLOSED ? texture_closed
					      : texture_opened;
}

ElimRow *main_mod_tree_add(GListStore *roots, ElimRow *parent, ElimRow *after,
			   int icon, const gchar *caption, const gchar *module,
			   const gchar *offset)
{
	ElimRow *row = after ? elim_tree_insert_after(roots, parent, after, N_COLUMNS)
			     : elim_tree_append(roots, parent, N_COLUMNS);

	main_mod_tree_set(row, icon, caption, module, offset);
	return row;
}

void main_mod_tree_set(ElimRow *row, int icon, const gchar *caption,
		       const gchar *module, const gchar *offset)
{
	elim_row_set_object(row, COL_OPEN_PIXBUF, G_OBJECT(icon_texture(icon)));
	elim_row_set_string(row, COL_CAPTION, caption);
	elim_row_set_string(row, COL_MODULE, module);
	elim_row_set_string(row, COL_OFFSET, offset);
}

void main_mod_tree_set_icon(ElimRow *row, int icon)
{
	elim_row_set_object(row, COL_OPEN_PIXBUF, G_OBJECT(icon_texture(icon)));
	elim_row_set_object(row, COL_CLOSED_PIXBUF, G_OBJECT(icon_texture(icon)));
}

/* A folder of the tree: its rows go under it. */
static ElimRow *add_folder(GListStore *roots, ElimRow *parent, const gchar *caption)
{
	return add_row(roots, parent, texture_opened, caption, NULL, caption);
}

/* The text of COLUMN of ROW, NULL when it is empty. */
static gchar *row_dup(ElimRow *row, guint column)
{
	const gchar *text = elim_row_get_string(row, column);

	return *text ? g_strdup(text) : NULL;
}

/******************************************************************************
 * Name
 *  main_add_children_to_tree
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void main_add_children_to_tree(ElimRow *row,
 *				 gchar *mod_name, unsigned long offset)
 *
 * Description
 *   the entries of a book, under its row: the tree of a general book is made
 *   as the reader opens it
 *
 * Return value
 *   void
 */

static void add_children_to_tree(ElimRow *row,
				 gchar *mod_name, unsigned long offset)
{
	gchar buf[256];
	gchar *tmpbuf = NULL;
	GListStore *roots = module_roots();

	elim_row_set_object(row, COL_OPEN_PIXBUF, G_OBJECT(texture_opened));
	elim_row_set_object(row, COL_CLOSED_PIXBUF, G_OBJECT(texture_closed));

	XI_message(("offset: %ld", backend->get_treekey_offset()));
	XI_message(("%s", backend->display_mod->getName()));
	if (backend->treekey_first_child(offset)) {
		XI_message(("treekey_first_child1 %s", mod_name));

		offset = backend->get_treekey_offset();

		XI_message(("offset: %ld", offset));

		sprintf(buf, "%lu", offset);
		tmpbuf = backend->treekey_get_local_name(offset);
		add_row(roots, row,
			backend->treekey_has_children(offset) ? texture_closed : texture_helpdoc,
			tmpbuf, mod_name, buf);
		free(tmpbuf);
	}

	while (backend->treekey_next_sibling(offset)) {
		offset = backend->get_treekey_offset();
		sprintf(buf, "%lu", offset);
		tmpbuf = backend->treekey_get_local_name(offset);
		add_row(roots, row,
			backend->treekey_has_children(offset) ? texture_closed : texture_helpdoc,
			tmpbuf, mod_name, buf);
		free(tmpbuf);
	}
}

/******************************************************************************
 * Name
 *  main_create_pixbufs
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void main_create_pixbufs(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void main_create_pixbufs(void)
{
	if (pixbufs)
		return;
	GtkTextDirection dir = gtk_widget_get_direction(GTK_WIDGET(widgets.app));

	pixbufs = g_new0(TreePixbufs, 1);

	/* Themed symbolic icons rather than the old raster books: they
	 * take the colour of the text beside them, so they read correctly
	 * on the light and the dark theme alike. Direction no longer
	 * matters -- a folder glyph has no handedness, unlike the drawn
	 * book that needed its own right-to-left artwork. */
	(void)dir;
	pixbufs->pixbuf_closed =
	    symbolic_pixbuf("folder-symbolic", 16, GTK_WIDGET(widgets.app));
	pixbufs->pixbuf_opened =
	    symbolic_pixbuf("folder-open-symbolic", 16, GTK_WIDGET(widgets.app));

	/* Leaf entries: "gtk-dnd" rendered as a two-pixel speck. */
	pixbufs->pixbuf_helpdoc =
	    symbolic_pixbuf("text-x-generic-symbolic", 16, GTK_WIDGET(widgets.app));

	/* what the rows show is these as textures, made once */
	texture_closed = gdk_texture_new_for_pixbuf(pixbufs->pixbuf_closed);
	texture_opened = gdk_texture_new_for_pixbuf(pixbufs->pixbuf_opened);
	texture_helpdoc = gdk_texture_new_for_pixbuf(pixbufs->pixbuf_helpdoc);
}

/******************************************************************************
 * Name
 *   main_mod_treeview_button_one
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void main_mod_treeview_button_one(ElimRow *selected)
 *
 * Description
 *   the reader clicked SELECTED, a row of the module tree
 *
 * Return value
 *   void
 */

void main_mod_treeview_button_one(ElimRow *selected)
{
	gint sbtype;
	gchar *cap = NULL;
	gchar *mod = NULL;
	gchar *key = NULL;

	static int old_page = 0;

	cap = row_dup(selected, COL_CAPTION);
	mod = row_dup(selected, COL_MODULE);
	key = row_dup(selected, COL_OFFSET);
	if (!cap) {
		g_free(mod);
		g_free(key);
		return;
	}

	if (!g_utf8_collate(cap, _("Parallel View"))) {
		if (settings.dockedInt) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
						      1);
		}
		if (settings.showparatab) {
			old_page = gtk_notebook_get_current_page(
			    GTK_NOTEBOOK(widgets.notebook_main));

			gtk_notebook_set_current_page(
			    GTK_NOTEBOOK(widgets.notebook_main),
			    gtk_notebook_page_num(
				GTK_NOTEBOOK(widgets.notebook_main),
				widgets.parallel_tab));
		}
	}

	if (!g_utf8_collate(cap, _("Standard View"))) {
		gtk_notebook_set_current_page(
		    GTK_NOTEBOOK(widgets.notebook_main),
		    old_page);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
					      0);
	}
	/* let's not do anything else if the parallel tab is showing */
	if (settings.paratab_showing)
		goto done;

	if (!g_utf8_collate(cap, _("Commentaries"))) {
		if (!settings.comm_showing) {
			settings.comm_showing = TRUE;
			gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book),
						      0);
		}
	}

	if (!g_utf8_collate(cap, _("General Books"))) {
		if (settings.comm_showing) {
			settings.comm_showing = FALSE;
			gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book),
						      1);
		}
	}

	if (!mod)
		goto done;

	sbtype = main_get_mod_type(mod);
	switch (sbtype) {
	case TEXT_TYPE:
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
					      0);
// MainWindowModule is set in main_bible_display(), not here.

		if (!gui_main_menu_get_state("bible"))
			gui_main_menu_change_state("bible", TRUE);

		if (key)
			main_url_handler(key, TRUE);
		else
			main_display_bible_from_module(settings.MainWindowModule,
						       settings.currentverse, mod);
		break;

	case COMMENTARY_TYPE:
	case PERCOM_TYPE:
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book), 0);
		settings.comm_showing = TRUE;

		if ((!gui_main_menu_get_state("commentary")) ||
		    (!settings.comm_showing)) {
			gui_main_menu_change_state("commentary", TRUE);
		}
		main_display_commentary(mod, settings.currentverse);
		break;

	case DICTIONARY_TYPE:
		if (!gui_main_menu_get_state("dictionary"))
			gui_main_menu_change_state("dictionary", TRUE);
		main_display_dictionary(mod, settings.dictkey);
		break;

	case BOOK_TYPE:
	case PRAYERLIST_TYPE:
		XI_message(("key %s", (key ? key : "-null-")));
		settings.comm_showing = FALSE;
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book), 1);
		backend->set_module(mod);
		backend->set_treekey(key ? atoi(key) : 0);
		if (!elim_row_n_children(selected) && !key) {
			add_children_to_tree(selected, mod, 0);
		}
		if (!elim_row_n_children(selected) && backend->treekey_has_children(key ? atoi(key) : 0)) {
			add_children_to_tree(selected, mod, atol(key));
		}

		elim_tree_expand_row(sidebar.module_list, selected, FALSE);

		if ((!gui_main_menu_get_state("commentary")) ||
		    (!settings.comm_showing)) {
			gui_main_menu_change_state("commentary", TRUE);
		}

		main_display_book(mod, (key ? key : (gchar *)"0"));
		main_setup_navbar_book(mod, (key ? atoi(key) : 0));
		break;
	}
done:
	g_free(cap);
	g_free(mod);
	g_free(key);
}

/******************************************************************************
 * Name
 *   language_add_folders
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void language_add_folders(GListStore * roots,
 *			       ElimRow * folder, gchar ** languages)
 *
 * Description
 *   fast creation of the tree's language folders
 *
 * Return value
 *   void
 */
static void
language_add_folders(GListStore *roots,
		     ElimRow *folder,
		     gchar **languages)
{
	int j;

	if (!folder || !languages || !languages[0])
		return;
	for (j = 0; languages[j]; ++j)
		add_row(roots, folder, texture_opened,
			((g_utf8_validate(languages[j], -1, NULL))
			     ? languages[j]
			     : _("Unknown")),
			NULL, NULL);
}

/******************************************************************************
 * Name
 *   add_module_to_prayerlist_folder
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void add_module_to_prayerlist_folder(GListStore * roots,
 *		      ElimRow * folder, gchar * module_name)
 *
 * Description
 *
 *
 * Return value
 *   void
 */
static void add_module_to_prayerlist_folder(GListStore *roots,
					    ElimRow *folder,
					    gchar *module_name)
{
	add_row(roots, folder, texture_closed, module_name, module_name, NULL);
}

/******************************************************************************
 * Name
 *   add_module_to_language_folder
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void add_module_to_language_folder(GListStore * roots,
 *		      ElimRow * folder, gchar * language, gchar * module_name)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void add_module_to_language_folder(GListStore *roots,
					  ElimRow *folder,
					  const gchar *language,
					  gchar *module_name,
					  const gchar *description)
{
	guint i;

	if (!folder)
		return;

	/* Check language */
	const gchar *buf = language;
	if (!g_utf8_validate(buf, -1, NULL))
		language = _("Unknown");
	if (!g_unichar_isalnum(g_utf8_get_char(buf)) || (language == NULL))
		language = _("Unknown");

	for (i = 0; i < elim_row_n_children(folder); i++) {
		/* Walk through the list, reading each row */
		ElimRow *language_folder = elim_row_get_child(folder, i);
		const gchar *abbreviation = main_name_to_abbrev(module_name);

		if (!strcmp(language, elim_row_get_string(language_folder, COL_CAPTION))) {
			gchar *content;

			content = g_strdup_printf("%s: %s",
						  (abbreviation
						       ? abbreviation
						       : module_name),
						  description);
			add_row(roots, language_folder, texture_closed, content,
				module_name, NULL);
			g_free(content);
			return;
		}
	}
}

static gboolean semilist_contains(const char *list, const char *name)
{
	gchar *needle;
	gboolean found;
	if (!list || !*list || !name)
		return FALSE;
	needle = g_strdup_printf(";%s;", name);
	found = (strstr(list, needle) != NULL);
	g_free(needle);
	return found;
}

static char *semilist_add(char *list, const char *name)
{
	gchar *result;
	if (semilist_contains(list, name))
		return list;
	if (!list || !*list)
		result = g_strdup_printf(";%s;", name);
	else
		result = g_strdup_printf("%s%s;", list, name);
	g_free(list);
	return result;
}

static char *semilist_remove(char *list, const char *name)
{
	gchar *needle, *pos, *result;
	if (!semilist_contains(list, name))
		return list;
	needle = g_strdup_printf(";%s;", name);
	pos = strstr(list, needle);
	result = g_strdup_printf("%.*s%s", (int)(pos - list + 1), list, pos + strlen(needle));
	g_free(needle);
	g_free(list);
	return result;
}

gboolean module_is_favorite(const gchar *name)
{
	return semilist_contains(settings.favorite_modules, name);
}

gboolean module_is_hidden(const gchar *name)
{
	return semilist_contains(settings.hidden_modules, name);
}

void module_toggle_favorite(const gchar *name)
{
	if (module_is_favorite(name))
		settings.favorite_modules = semilist_remove(settings.favorite_modules, name);
	else
		settings.favorite_modules = semilist_add(settings.favorite_modules, name);
	xml_set_value("Xiphos", "modules", "favorites", settings.favorite_modules);
	main_load_module_tree(sidebar.module_list);
}

void module_toggle_hidden(const gchar *name)
{
	if (module_is_hidden(name))
		settings.hidden_modules = semilist_remove(settings.hidden_modules, name);
	else
		settings.hidden_modules = semilist_add(settings.hidden_modules, name);
	xml_set_value("Xiphos", "modules", "hidden", settings.hidden_modules);
	main_load_module_tree(sidebar.module_list);
}

static const gchar *category_caption(MOD_MGR *info)
{
	if (info->is_cult)
		return _("Cult/Unorthodox");
	if (info->type[0] == 'B')
		return _("Biblical Texts");
	if (info->type[0] == 'C')
		return _("Commentaries");
	if (info->is_maps)
		return _("Maps");
	if (info->is_images)
		return _("Images");
	if (info->is_devotional)
		return _("Daily Devotionals");
	if (info->is_glossary)
		return _("Glossaries");
	if (info->type[0] == 'L')
		return _("Dictionaries");
	if (info->type[0] == 'G')
		return _("General Books");
	return NULL;
}

static const gchar *module_language_caption(MOD_MGR *info)
{
	const gchar *lang = info->language;
	if (!lang || !g_utf8_validate(lang, -1, NULL))
		return _("Unknown");
	if (!g_unichar_isalnum(g_utf8_get_char(lang)))
		return _("Unknown");
	return lang;
}

static ElimRow *get_or_create_folder(GListStore *store,
				     ElimRow *parent,
				     const gchar *caption)
{
	guint i, n = parent ? elim_row_n_children(parent)
			    : g_list_model_get_n_items(G_LIST_MODEL(store));

	for (i = 0; i < n; i++) {
		ElimRow *row = parent ? elim_row_get_child(parent, i)
				      : elim_table_get(store, i);

		if (!strcmp(elim_row_get_string(row, COL_CAPTION), caption))
			return row;
	}
	return add_folder(store, parent, caption);
}

static void append_module_row(GListStore *store, ElimRow *parent, MOD_MGR *info)
{
	const gchar *abbreviation = main_name_to_abbrev(info->name);
	gchar *content = g_strdup_printf("%s: %s",
					 (abbreviation ? abbreviation : info->name),
					 info->description);

	add_row(store, parent, texture_closed, content, info->name, NULL);
	g_free(content);
}

/* The favorites folder goes first in the tree. */
static ElimRow *prepend_favorites(GListStore *store)
{
	ElimRow *favorites = elim_tree_prepend(store, NULL, N_COLUMNS);

	elim_row_set_object(favorites, COL_OPEN_PIXBUF, G_OBJECT(texture_opened));
	elim_row_set_string(favorites, COL_CAPTION, _("Favorites"));
	elim_row_set_string(favorites, COL_OFFSET, _("Favorites"));
	return favorites;
}

/* The tree in the sidebar shows what STAGING holds, a tree made apart. */
static void show_module_tree(GtkWidget *tree, GListStore *staging)
{
	elim_tree_replace(elim_table_get_store(tree), staging);
	g_object_unref(staging);
}

static int module_lang_cmpstringp(gconstpointer p1, gconstpointer p2)
{
	return ucol_strcollUTF8(collator, (const char *)p1, -1, (const char *)p2, -1, &collator_status);
}

void main_load_module_tree_flat(GtkWidget *tree)
{
	main_create_pixbufs();
	GListStore *store = elim_table_new();
	GList *tmp = mod_mgr_list_reader_modules();
	GList *tmp2;
	GHashTable *cat_rows = g_hash_table_new(g_str_hash, g_str_equal);
	ElimRow *favorites = NULL;
	for (tmp2 = tmp; tmp2; tmp2 = g_list_next(tmp2)) {
		MOD_MGR *info = (MOD_MGR *)tmp2->data;
		const gchar *cat = category_caption(info);
		ElimRow *cat_row;
		gboolean hidden = module_is_hidden(info->name) && !settings.show_hidden_modules;
		if (!cat) {
			XI_warning(("mod `%s' unknown type `%s'", info->name, info->type));
			continue;
		}
		if (!hidden) {
			cat_row = (ElimRow *)g_hash_table_lookup(cat_rows, cat);
			if (!cat_row) {
				cat_row = add_folder(store, NULL, cat);
				g_hash_table_insert(cat_rows, (gpointer)cat, cat_row);
			}
			append_module_row(store, cat_row, info);

			if (module_is_favorite(info->name)) {
				if (!favorites)
					favorites = prepend_favorites(store);
				append_module_row(store, favorites, info);
			}
		}
		g_free(info->name);
		g_free(info->type);
		g_free(info->new_version);
		g_free(info->old_version);
		g_free(info->installsize);
		g_free(info);
	}
	g_list_free(tmp);
	g_hash_table_destroy(cat_rows);
	show_module_tree(tree, store);
}

void main_load_module_tree_by_language(GtkWidget *tree)
{
	main_create_pixbufs();
	GListStore *store = elim_table_new();
	GList *tmp = mod_mgr_list_reader_modules();
	GList *tmp2, *languages = NULL;
	GHashTable *lang_rows = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ElimRow *favorites = NULL;
	if (!collator) {
		char *locale = getenv("LANG");
		collator = ucol_open((locale ? locale : ""), &collator_status);
	}
	for (tmp2 = tmp; tmp2; tmp2 = g_list_next(tmp2)) {
		MOD_MGR *info = (MOD_MGR *)tmp2->data;
		const gchar *lang = module_language_caption(info);
		if (!g_list_find_custom(languages, lang, (GCompareFunc)strcmp)) {
			languages = g_list_insert_sorted(languages, g_strdup(lang),
							 (GCompareFunc)module_lang_cmpstringp);
		}
	}
	for (tmp2 = languages; tmp2; tmp2 = g_list_next(tmp2)) {
		gchar *lang = (gchar *)tmp2->data;
		ElimRow *row = add_folder(store, NULL, lang);
		g_hash_table_insert(lang_rows, g_strdup(lang), row);
	}
	g_list_free_full(languages, g_free);
	for (tmp2 = tmp; tmp2; tmp2 = g_list_next(tmp2)) {
		MOD_MGR *info = (MOD_MGR *)tmp2->data;
		const gchar *cat = category_caption(info);
		const gchar *lang = module_language_caption(info);
		ElimRow *lang_row;
		ElimRow *cat_row;
		gboolean hidden = module_is_hidden(info->name) && !settings.show_hidden_modules;
		if (!cat) {
			XI_warning(("mod `%s' unknown type `%s'", info->name, info->type));
			continue;
		}
		if (!hidden) {
			lang_row = (ElimRow *)g_hash_table_lookup(lang_rows, lang);
			cat_row = get_or_create_folder(store, lang_row, cat);
			append_module_row(store, cat_row, info);

			if (module_is_favorite(info->name)) {
				if (!favorites)
					favorites = prepend_favorites(store);
				append_module_row(store, favorites, info);
			}
		}
		g_free(info->name);
		g_free(info->type);
		g_free(info->new_version);
		g_free(info->old_version);
		g_free(info->installsize);
		g_free(info);
	}
	g_list_free(tmp);
	g_hash_table_destroy(lang_rows);
	show_module_tree(tree, store);
}

/******************************************************************************
 * Name
 *   main_load_module_tree
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void main_load_module_tree(GtkWidget * tree)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void module_tree_mapped(GtkWidget *tree, gpointer unused)
{
	(void)unused;
	if (g_object_get_data(G_OBJECT(tree), "elim-module-tree-pending"))
		main_load_module_tree(tree);
}

static gboolean module_tree_settled(gpointer tree)
{
	if (gtk_widget_get_mapped(GTK_WIDGET(tree)))
		module_tree_mapped(GTK_WIDGET(tree), NULL);
	return G_SOURCE_REMOVE;
}

void main_init_module_tree(GtkWidget *tree)
{
	/* Stable model and columns for early readers; SVGs and module rows are
	 * only needed when the sidebar maps. Explicit reloads still work before
	 * that (installation, preferences, prayer lists). */
	GListStore *store = elim_table_new();
	main_setup_mod_tree_view(tree, store);
	g_object_unref(store);
	g_object_set_data(G_OBJECT(tree), "elim-module-tree-pending", GINT_TO_POINTER(1));
	g_signal_connect(tree, "map", G_CALLBACK(module_tree_mapped), NULL);
	/* GTK 4 maps synchronously: a sidebar still visible while the window
	 * is being built would load the tree at once, and the startup code
	 * hides it right after. Judge once startup has settled. */
	if (gtk_widget_get_mapped(tree))
		g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, module_tree_settled,
				g_object_ref(tree), g_object_unref);
}

void main_load_module_tree(GtkWidget *tree)
{
	g_object_set_data(G_OBJECT(tree), "elim-module-tree-pending", NULL);
	main_create_pixbufs();
	switch (settings.module_tree_grouping) {
	case 1:
		main_load_module_tree_flat(tree);
		return;
	case 2:
		main_load_module_tree_by_language(tree);
		return;
	default:
		break; /* mode 0: category+language, unchanged below */
	}
	GListStore *store;

	ElimRow *text = NULL;
	ElimRow *commentary = NULL;
	ElimRow *dictionary = NULL;
	ElimRow *glossary = NULL;
	ElimRow *devotional = NULL;
	ElimRow *book = NULL;
	ElimRow *map = NULL;
	ElimRow *image = NULL;
	ElimRow *cult = NULL;
	ElimRow *prayerlist = NULL;
	gboolean need_commentary = 0;
	gboolean need_dictionary = 0;
	gboolean need_glossary = 0;
	gboolean need_devotional = 0;
	gboolean need_book = 0;
	gboolean need_map = 0;
	gboolean need_image = 0;
	gboolean need_cult = 0;
	gboolean need_prayerlist = 0;

	ElimRow *favorites = NULL;

	GList *tmp = NULL;
	GList *tmp2 = NULL;

	tmp = mod_mgr_list_reader_modules();

	// find which folders are needed.
	tmp2 = tmp;
	while (tmp2) {
		MOD_MGR *info = (MOD_MGR *)tmp2->data;
		if (info->is_cult)
			need_cult++;
		else if (info->type[0] == 'B')
			;
		else if (info->type[0] == 'C')
			need_commentary++;
		else if (info->is_maps)
			need_map++;
		else if (info->is_images)
			need_image++;
		else if (info->is_devotional)
			need_devotional++;
		else if (info->is_glossary)
			need_glossary++;
		else if (info->type[0] == 'L')
			need_dictionary++;
		else if (info->is_prayerlist)
			need_prayerlist++;
		else if (info->type[0] == 'G')
			need_book++;
		else {
			XI_warning(("mod `%s' unknown type `%s'",
				    info->name, info->type));
		}
		tmp2 = g_list_next(tmp2);
	}

	store = elim_table_new();

	/*  Biblical Texts folders */
	text = add_folder(store, NULL, _("Biblical Texts"));
	add_row(store, text, texture_helpdoc, _("Parallel View"),
		_("Parallel View"), _("Parallel View"));
	add_row(store, text, texture_helpdoc, _("Standard View"),
		_("Standard View"), _("Standard View"));

	/*  Commentaries folders */
	if (need_commentary)
		commentary = add_folder(store, NULL, _("Commentaries"));

	/*  Dictionaries folders */
	if (need_dictionary)
		dictionary = add_folder(store, NULL, _("Dictionaries"));

	/*  Glossaries folders */
	if (need_glossary)
		glossary = add_folder(store, NULL, _("Glossaries"));

	/*  Devotionals folders */
	if (need_devotional)
		devotional = add_folder(store, NULL, _("Daily Devotionals"));

	/*  General Books folders */
	if (need_book)
		book = add_folder(store, NULL, _("General Books"));

	/*  Maps folders */
	if (need_map)
		map = add_folder(store, NULL, _("Maps"));

	/*  Images folders */
	if (need_image)
		image = add_folder(store, NULL, _("Images"));

	/*  Cult/Unorthodox/Questionable folders */
	if (need_cult)
		cult = add_folder(store, NULL, _("Cult/Unorthodox"));

	/*  Prayer lists folder */
	if (settings.prayerlist && need_prayerlist)
		prayerlist = add_folder(store, NULL, _("Prayer List/Journal"));

	language_make_list(tmp, store,
			   text, commentary, map, image,
			   devotional, dictionary, glossary, book, cult,
			   NULL, NULL,
			   language_add_folders, FALSE);

	// fast-n-loose w/known string values to avoid pointless strcmp costs.
	// TEXT_MODS => 'B' ("Biblical Texts")
	// COMM_MODS => 'C' ("Commentaries")
	// DICT_MODS => 'L' ("Lexicons / Dictionaries")
	// BOOK_MODS => 'G' ("Generic Books")
	// see src/main/sword.h regarding these definitions.
	// see also similar code in src/gtk/{mod_mgr,utilities}.c.
	// it is just necessary that we undo some of this inefficiency.

	tmp2 = tmp;
	while (tmp2 != NULL) {
		MOD_MGR *info = (MOD_MGR *)tmp2->data;
		gboolean hidden = module_is_hidden(info->name) && !settings.show_hidden_modules;

		if (!hidden) {
		if (info->is_cult) {
			add_module_to_language_folder(store,
						      cult, info->language,
						      info->name, info->description);
		} else if (info->type[0] == 'B') {
			add_module_to_language_folder(store,
						      text, info->language,
						      info->name, info->description);
		} else if (info->type[0] == 'C') {
			add_module_to_language_folder(store,
						      commentary, info->language,
						      info->name, info->description);
		} else if (info->is_maps) {
			add_module_to_language_folder(store,
						      map, info->language,
						      info->name, info->description);
		} else if (info->is_images) {
			add_module_to_language_folder(store,
						      image, info->language,
						      info->name, info->description);
		} else if (info->is_devotional) {
			add_module_to_language_folder(store,
						      devotional, info->language,
						      info->name, info->description);
		} else if (info->is_glossary) {
			add_module_to_language_folder(store,
						      glossary, info->language,
						      info->name, info->description);
		} else if (info->type[0] == 'L') {
			add_module_to_language_folder(store,
						      dictionary, info->language,
						      info->name, info->description);
		} else if (info->type[0] == 'G') {
			add_module_to_language_folder(store,
						      book, info->language,
						      info->name, info->description);
		} else {
			XI_warning(("mod `%s' unknown type `%s'",
				    info->name, info->type));
		}

		if (module_is_favorite(info->name)) {
			if (!favorites)
				favorites = prepend_favorites(store);
			append_module_row(store, favorites, info);
		}
		}

		g_free(info->name);
		g_free(info->type);
		g_free(info->new_version);
		g_free(info->old_version);
		g_free(info->installsize);
		g_free(info);
		tmp2 = g_list_next(tmp2);
	}
	g_list_free(tmp);

	/* prayer list folders */
	if (settings.prayerlist && need_prayerlist) {
		tmp = get_list(PRAYER_LIST);
		while (tmp != NULL) {
			add_module_to_prayerlist_folder(store,
							prayerlist,
							(gchar *)tmp->data);
			tmp = g_list_next(tmp);
		}
	}
	show_module_tree(tree, store);
}

/******************************************************************************
 * Name
 *  main_setup_mod_tree_view
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void main_setup_mod_tree_view(GtkWidget * tree, GListStore * roots)
 *
 * Description
 *   TREE, a GtkListView, shows the tree of ROOTS the way the module tree is
 *   shown: an icon and the caption of every row. (The module and the offset
 *   are what a row opens; they show nothing.)
 *
 * Return value
 *   void
 */

void main_setup_mod_tree_view(GtkWidget *tree, GListStore *roots)
{
	ElimColumn cols[2];

	/* one icon per row, folder or leaf, open or not */
	cols[0] = elim_column_image(COL_OPEN_PIXBUF, 16, NULL);
	cols[1] = elim_column_text(COL_CAPTION);
	cols[1].text.expand = TRUE;
	elim_tree_setup_row_columns(tree, roots, cols, 2);
}

/******   end of file   ******/
