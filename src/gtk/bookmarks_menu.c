/*
 * Xiphos Bible Study Tool
 * bookmarks_menu.c - gui for bookmarks using menu
 *
 * Copyright (C) 2003-2026 Xiphos Developer Team
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
#include "gui/widget_helpers.h"
#include <glib/gstdio.h>
#include <libxml/parser.h>

#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "gui/xiphos.h"
#include "gui/bookmarks_menu.h"
#include "gui/bookmarks_treeview.h"
#include "gui/table_helpers.h"
#include "gui/export_bookmarks.h"
#include "gui/import_andbible.h"
#include "gui/utilities.h"
#include "gui/main_window.h"
#include "gui/dialog.h"
#include "gui/bibletext_dialog.h"
#include "gui/commentary_dialog.h"
#include "gui/dictlex_dialog.h"
#include "gui/gbs_dialog.h"
#include "gui/widgets.h"

#include "main/settings.h"
#include "main/sword.h"
#include "main/sidebar.h"
#include "main/xml.h"
#include "main/module_dialogs.h"
#include "main/url.hh"

#include "editor/editor.h"

#include "gui/debug_glib_null.h"

BOOKMARK_MENU menu;

gboolean bookmarks_changed;

static void toggle_color_clicked(GtkButton *btn, GtkWidget *other)
{
	gboolean has_color =
		g_strcmp0(gtk_button_get_label(btn), _("No color")) == 0;
	gtk_button_set_label(btn, has_color ? _("Add color") : _("No color"));
	gtk_widget_set_sensitive(other, !has_color);
}

static void color_dialog_button_changed(GtkColorDialogButton *color_button,
						GParamSpec *pspec, GtkButton *toggle_button)
{
	(void)color_button;
	(void)pspec;
	gtk_widget_set_sensitive(GTK_WIDGET(toggle_button), TRUE);
	gtk_button_set_label(toggle_button, _("No color"));
}

/******************************************************************************
 * Name
 *  save_treeview_to_xml_bookmarks
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void save_treeview_to_xml_bookmarks(ElimRow * parent, gchar *file_buf)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void save_treeview_to_xml_bookmarks(ElimRow *parent, gchar *filename)
{
	xmlNodePtr root_node = NULL;
	xmlNodePtr cur_node = NULL;
	xmlDocPtr root_doc;
	guint i;

	if (!bookmarks_changed)
		return;

	root_doc = xmlNewDoc((const xmlChar *)"1.0");

	if (root_doc != NULL) {
		root_node = xmlNewNode(NULL, (const xmlChar *)"SwordBookmarks");
		xmlNewProp(root_node, (const xmlChar *)"syntaxVersion",
			   (const xmlChar *)"1");
		xmlDocSetRootElement(root_doc, root_node);
	}

	for (i = 0; i < elim_row_n_children(parent); i++) {
		ElimRow *row = elim_row_get_child(parent, i);
		gchar *caption = bookmark_row_dup(row, COL_CAPTION);
		gchar *key = bookmark_row_dup(row, COL_KEY);
		gchar *module = bookmark_row_dup(row, COL_MODULE);
		gchar *mod_desc = bookmark_row_dup(row, COL_MODULE_DESC);
		gchar *description = bookmark_row_dup(row, COL_DESCRIPTION);
		gchar *color = bookmark_row_dup(row, COL_COLOR);

		if (elim_row_n_children(row)) {
			/* folder node — write color attribute when present */
			cur_node = xml_add_folder_to_parent_colored(root_node,
									caption,
									color);
			utilities_parse_treeview(cur_node, row);
		} else {
			xml_add_bookmark_to_parent(root_node,
						   description,
						   key, module, mod_desc);
		}
		g_free(caption);
		g_free(key);
		g_free(module);
		g_free(mod_desc);
		g_free(description);
		g_free(color);
	}

	xmlSaveFormatFile(filename, root_doc, 1);
	xmlFreeDoc(root_doc);
	g_free(filename);
	bookmarks_changed = FALSE;
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void bibletime_bookmarks_activate(gpointer menuitem,
						  gpointer user_data)
{
	ElimRow *parent;
	gchar *fname;
	GtkWidget *dialog;

	if (!g_list_model_get_n_items(G_LIST_MODEL(bookmark_roots)))
		return;
	parent = elim_table_get(bookmark_roots, 0);

	dialog = gtk_file_chooser_dialog_new(_("Specify bookmarks file"),
					     GTK_WINDOW(widgets.app),
					     GTK_FILE_CHOOSER_ACTION_OPEN,
					     "_Cancel",
					     GTK_RESPONSE_CANCEL, "_OK",
					     GTK_RESPONSE_ACCEPT,
					     NULL);
	gui_fit_dialog_to_screen(GTK_WINDOW(dialog));
	fname =
	    g_strdup_printf("%s/%s", settings.homedir,
			    ".bibletime/bookmarks.xml");
	gui_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), fname);
	g_free(fname);

	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		BOOKMARK_DATA folder = { 0 };
		ElimRow *imported;

		folder.caption = (gchar *)"Imported";
		folder.opened = bm_pixbufs->pixbuf_opened;
		folder.closed = bm_pixbufs->pixbuf_closed;
		imported = gui_add_item_to_tree(parent, &folder);

		fname =
		    gui_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gui_parse_bookmarks(bookmark_tree, (const xmlChar *)fname,
				    imported);
		g_free(fname);
	}
	gui_widget_destroy(dialog);
}


static gboolean andbible_folder_exists(ElimRow *parent)
{
	guint i;

	for (i = 0; i < elim_row_n_children(parent); i++)
		if (!strcmp(elim_row_get_string(elim_row_get_child(parent, i),
						COL_CAPTION),
			    _("Import AndBible")))
			return TRUE;
	return FALSE;
}

/******************************************************************************
 * Name
 *   remove_existing_andbible_folder
 *
 * Synopsis
 *   #include "gui/import_andbible.h"
 *
 *   void remove_existing_andbible_folder(ElimRow * parent)
 *
 * Description
 *   The "Import AndBible" top-level folder is entirely owned/managed by
 *   andbible_bookmarks_activate(): every import wipes it and rebuilds it
 *   from scratch, rather than trying to patch it in place. That gives
 *   predictable, duplicate-free behaviour (additions, edits and
 *   removals on the AndBible side are all reflected simply by
 *   reimporting) at the cost of not preserving manual edits made
 *   directly inside that folder in Xiphos - which is why it is kept as
 *   its own clearly-named folder rather than merged into the user's
 *   own bookmarks.
 *
 * Return value
 *   void
 */

static void remove_existing_andbible_folder(ElimRow *parent)
{
	guint i;

	for (i = 0; i < elim_row_n_children(parent); i++) {
		ElimRow *child = elim_row_get_child(parent, i);

		if (!strcmp(elim_row_get_string(child, COL_CAPTION),
			    _("Import AndBible"))) {
			elim_tree_remove(bookmark_roots, child);
			return;
		}
	}
}

/******************************************************************************
 * Name
 *   andbible_bookmarks_activate
 *
 * Synopsis
 *   #include "gui/import_andbible.h"
 *
 *   void andbible_bookmarks_activate(gpointer menuitem,
 *				      gpointer user_data)
 *
 * Description
 *   Prompts for an AndBible bookmarks backup (.sqlite3), converts it via
 *   andbible_import_to_temp_xml() (main/import_andbible.cc - uses
 *   libsword's own versification support to turn AndBible's ordinal
 *   verse positions back into references), and merges the result into
 *   the bookmarks tree the same way bibletime_bookmarks_activate() does.
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void andbible_bookmarks_activate(gpointer menuitem,
						  gpointer user_data)
{
	ElimRow *parent;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	if (!g_list_model_get_n_items(G_LIST_MODEL(bookmark_roots)))
		return;
	parent = elim_table_get(bookmark_roots, 0);

	dialog = gtk_file_chooser_dialog_new(
	    _("Select AndBible bookmarks backup (.sqlite3)"),
	    GTK_WINDOW(widgets.app), GTK_FILE_CHOOSER_ACTION_OPEN,
	    "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_ACCEPT,
	    NULL);
	gui_fit_dialog_to_screen(GTK_WINDOW(dialog));

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("AndBible backup (*.sqlite3)"));
	gtk_file_filter_add_pattern(filter, "*.sqlite3");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *sqlite_path, *tmp_xml = NULL, *summary;
		gint n_imported = 0, n_skipped = 0;
		GError *error = NULL;

		sqlite_path =
		    gui_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		if (andbible_folder_exists(parent)) {
			GtkWidget *confirm = gtk_message_dialog_new(
			    GTK_WINDOW(widgets.app), GTK_DIALOG_MODAL,
			    GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s",
			    _("An 'Import AndBible' folder already exists "
			      "and will be replaced by this import. "
			      "Continue?"));
			gint resp = gui_dialog_run(GTK_DIALOG(confirm));
			gui_widget_destroy(confirm);
			if (resp != GTK_RESPONSE_YES) {
				g_free(sqlite_path);
				gui_widget_destroy(dialog);
				return;
			}
		}

		if (!andbible_import_to_temp_xml(sqlite_path,
						  _("Import AndBible"),
						  &tmp_xml, &n_imported,
						  &n_skipped, &error)) {
			GtkWidget *msg = gtk_message_dialog_new(
			    GTK_WINDOW(widgets.app), GTK_DIALOG_MODAL,
			    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s",
			    error ? error->message : _("Unknown error"));
			gui_dialog_run(GTK_DIALOG(msg));
			gui_widget_destroy(msg);
			g_clear_error(&error);
			g_free(sqlite_path);
			gui_widget_destroy(dialog);
			return;
		}
		g_free(sqlite_path);

		/* This folder is fully managed by us: wipe any previous
		 * import before rebuilding it, so re-importing the same
		 * (or an updated) AndBible backup never creates duplicates. */
		remove_existing_andbible_folder(parent);

		BOOKMARK_DATA folder = { 0 };

		folder.caption = (gchar *)_("Import AndBible");
		folder.opened = bm_pixbufs->pixbuf_opened;
		folder.closed = bm_pixbufs->pixbuf_closed;
		gui_parse_bookmarks(bookmark_tree, (const xmlChar *)tmp_xml,
				    gui_add_item_to_tree(parent, &folder));
		g_unlink(tmp_xml);
		g_free(tmp_xml);

		summary = g_strdup_printf(
		    _("%d bookmark(s) imported, %d skipped."), n_imported,
		    n_skipped);
		GtkWidget *msg = gtk_message_dialog_new(
		    GTK_WINDOW(widgets.app), GTK_DIALOG_MODAL,
		    GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", summary);
		gui_dialog_run(GTK_DIALOG(msg));
		gui_widget_destroy(msg);
		g_free(summary);
	}
	gui_widget_destroy(dialog);
}

/******************************************************************************
 * Name
 *   on_allow_reordering_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_allow_reordering_activate(gpointer menuitem,
 *				  gpointer user_data)
 *
 * Description
 *   allow reordering of bookmarks
 *
 * Return value
 *   void
 */

static void on_reorder_state(GSimpleAction *action, GVariant *state, gpointer data)
{
	(void)data;
	g_simple_action_set_state(action, state);
	elim_tree_set_reorderable(bookmark_tree, g_variant_get_boolean(state));
}

static void on_crossref_popup_state(GSimpleAction *action, GVariant *state, gpointer data)
{
	(void)data;
	g_simple_action_set_state(action, state);
	settings.crossref_popup = g_variant_get_boolean(state);
	xml_set_value("Xiphos", "misc", "crossref_popup",
		      settings.crossref_popup ? "1" : "0");
}

static void on_tag_colorize_state(GSimpleAction *action, GVariant *state, gpointer data)
{
	(void)data;
	g_simple_action_set_state(action, state);
	settings.tag_colorize = g_variant_get_boolean(state);
	xml_set_value("Xiphos", "misc", "tag_colorize",
		      settings.tag_colorize ? "1" : "0");
	main_display_bible(NULL, settings.currentverse);
}

/******************************************************************************
 * Name
 *   on_dialog_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_dialog_activate(gpointer menuitem,
 *				  gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_dialog_activate(gpointer menuitem,
					gpointer user_data)
{
	ElimRow *selected = bookmark_selected();
	gchar *key = NULL;
	gchar *module = NULL;

	use_dialog = TRUE;
	if (selected) {
		key = bookmark_row_dup(selected, COL_KEY);
		module = bookmark_row_dup(selected, COL_MODULE);

		if (module && (main_get_mod_type(module) == PERCOM_TYPE)) {
			editor_create_new(module, key, TRUE);
			use_dialog = FALSE;
			g_free(key);
			g_free(module);
			return;
		}

		gchar *url =
		    g_strdup_printf("passagestudy.jsp?action=showBookmark&"
				    "type=%s&value=%s&module=%s",
				    "newDialog", main_url_encode(key),
				    main_url_encode(module));
		main_url_handler(url, TRUE);
		g_free(url);
		g_free(key);
		g_free(module);
	}
	use_dialog = FALSE;
}

/******************************************************************************
 * Name
 *   on_edit_item_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_edit_item_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   edit bookmark
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_edit_item_activate(gpointer menuitem,
					   gpointer user_data)
{
	ElimRow *selected = bookmark_selected();
	gchar *caption = NULL, *key = NULL, *module = NULL;
	gchar *mod_desc = NULL, *description = NULL, *current_color = NULL;

	if (!selected)
		return;
	caption = bookmark_row_dup(selected, COL_CAPTION);
	key = bookmark_row_dup(selected, COL_KEY);
	module = bookmark_row_dup(selected, COL_MODULE);
	mod_desc = bookmark_row_dup(selected, COL_MODULE_DESC);
	description = bookmark_row_dup(selected, COL_DESCRIPTION);
	current_color = bookmark_row_dup(selected, COL_COLOR);

	if (!key || !*key) {

		/* --- Folder: use the dedicated folder dialog --- */
		GtkBuilder *gxml = elim_gtk_builder_new();
		gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/folder.gtkbuilder", NULL);
		GtkWidget *dialog  = GTK_WIDGET(UI_GET_ITEM(gxml, "dialog_folder"));
		GtkWidget *entry   = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_entry_name"));
		GtkWidget *colorbtn = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_color_button"));
		GtkWidget *clearbtn = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_clear_color"));

		if (!dialog || !entry || !colorbtn || !clearbtn) {
			g_object_unref(gxml); goto cleanup;
		}
		gtk_window_set_title(GTK_WINDOW(dialog), _("Edit Tag"));
		gtk_editable_set_text(GTK_EDITABLE(entry), caption ? caption : "");

		if (current_color && *current_color) {
			GdkRGBA rgba;
			if (gdk_rgba_parse(&rgba, current_color))
				gtk_color_dialog_button_set_rgba(
					GTK_COLOR_DIALOG_BUTTON(colorbtn), &rgba);
		} else {
			gtk_button_set_label(GTK_BUTTON(clearbtn), _("Add color"));
		}

		g_signal_connect(clearbtn, "clicked",
			G_CALLBACK(toggle_color_clicked), colorbtn);
		g_signal_connect(colorbtn, "notify::rgba",
			G_CALLBACK(color_dialog_button_changed), GTK_BUTTON(clearbtn));

		if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
			const gchar *name = gtk_editable_get_text(GTK_EDITABLE(entry));
			gchar *new_color = NULL;
			gchar *new_caption;
			if (g_strcmp0(gtk_button_get_label(GTK_BUTTON(clearbtn)), _("No color")) == 0) {
				GdkRGBA rgba;
				rgba = *gtk_color_dialog_button_get_rgba(
					GTK_COLOR_DIALOG_BUTTON(colorbtn));
				if (rgba.red < 0.99 || rgba.green < 0.99 || rgba.blue < 0.99)
					new_color = g_strdup_printf("#%02X%02X%02X",
						(guint)(rgba.red * 255),
						(guint)(rgba.green * 255),
						(guint)(rgba.blue * 255));
			}
			new_caption = g_strdelimit(g_strdup(name), "/|><.'`\"", ' ');

			bookmarks_changed = TRUE;
			elim_row_set_string(selected, COL_CAPTION, new_caption);
			bookmark_row_set_color(selected, new_color);
			g_free(new_caption);
			g_free(new_color);
			gui_save_bookmarks(NULL, NULL);
			main_display_bible(NULL, settings.currentverse);
		}
		gui_widget_destroy(dialog);
		g_object_unref(gxml);
	} else {
		/* --- Leaf bookmark: generic dialog --- */
		GS_DIALOG *info = gui_new_dialog();
		GString *str = g_string_new(NULL);
		g_string_printf(str, "<span weight=\"bold\">%s</span>", _("Edit"));
		info->title     = _("Bookmark");
		info->label_top = str->str;
		info->label1    = _("Bookmark name: ");
		info->text1     = g_strdup(caption);
		info->text2     = g_strdup(key);
		info->text3     = g_strdup(module);
		info->label2    = _("Verse: ");
		info->label3    = _("Module: ");
		info->ok        = TRUE;
		info->cancel    = TRUE;
		if (gui_gs_dialog(info) == GS_OK) {
			BOOKMARK_DATA *data = g_new0(BOOKMARK_DATA, 1);
			data->caption     = g_strdup(info->text1);

			if (data->caption && strstr(data->caption, "@:@:@")) {
				gui_generic_warning_modal(_("Bookmark labels may not contain \"@:@:@\"."));
				g_free(data->caption);
				g_free(data);
				return;
			}

			data->key         = g_strdup(info->text2);
			data->module      = g_strdup(info->text3);
			data->module_desc = g_strdup(main_get_module_description(info->text3));
			data->description = ((description && strlen(description) > 1) ||
						(caption && strcmp(caption, info->text1)))
					   ? g_strdup(info->text1) : NULL;
		data->is_leaf = TRUE;
		data->opened = bm_pixbufs->pixbuf_helpdoc;
			elim_row_set_pixbuf(selected, COL_OPEN_PIXBUF, data->opened);
			elim_row_set_pixbuf(selected, COL_CLOSED_PIXBUF, data->closed);
			elim_row_set_string(selected, COL_CAPTION, data->caption);
			elim_row_set_string(selected, COL_KEY, data->key);
			elim_row_set_string(selected, COL_MODULE, data->module);
			elim_row_set_string(selected, COL_MODULE_DESC, data->module_desc);
			elim_row_set_string(selected, COL_DESCRIPTION, data->description);
			bookmarks_changed = TRUE;
			gui_save_bookmarks(NULL, NULL);
		}
	}
cleanup:
	g_free(caption);
	g_free(key);
	g_free(module);
	g_free(mod_desc);
	g_free(description);
	g_free(current_color);
}


/******************************************************************************
 * Name
 *   on_remove_folder_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_remove_folder_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   remove folder - and save it
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_export_folder_activate(gpointer menuitem,
					       gpointer user_data)
{
	gui_export_bookmarks_dialog(BOOKMARKS_EXPORT, NULL);
}

/******************************************************************************
 * Name
 *   on_delete_item_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_delete_item_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   delete bookmark - if a group delete all in the group
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_delete_item_activate(gpointer menuitem,
					     gpointer user_data)
{
	ElimRow *selected = bookmark_selected();
	gchar *name_string;
	gchar *str;

	if (!selected)
		return;
	name_string = bookmark_row_dup(selected, COL_CAPTION);

	if (elim_row_n_children(selected)) {
		str =
		    g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s %s",
				    _("Remove the selected folder"), name_string,
				    _("(and all its contents)?"));
	} else {
		str =
		    g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s",
				    _("Remove the selected bookmark"), name_string);
	}

	if (gui_yes_no_dialog(str,
			      "dialog-warning")) {
		elim_tree_remove(bookmark_roots, selected);
		bookmarks_changed = TRUE;
		gui_save_bookmarks(NULL, NULL);
	}
	g_free(name_string);
	g_free(str);
}

/******************************************************************************
 * Name
 *   gui_save_bookmarks
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void gui_save_bookmarks(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   save bookmark tree
 *
 * Return value
 *   void
 */

void gui_save_bookmarks(gpointer menuitem, gpointer user_data)
{
	ElimRow *root;
	gchar buf[256];

	if (!g_list_model_get_n_items(G_LIST_MODEL(bookmark_roots)))
		return;
	root = elim_table_get(bookmark_roots, 0);
	if (!elim_row_n_children(root))
		return;

	sprintf(buf, "%s/bookmarks/bookmarks.xml", settings.gSwordDir);
	save_treeview_to_xml_bookmarks(root, g_strdup(buf));
}

/******************************************************************************
 * Name
 *   gui_save_bookmarks
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void gui_save_bookmarks(void)
 *
 * Description
 *   save bookmark tree
 *
 * Return value
 *   void
 */

void gui_save_bookmarks_treeview(void)
{
	gui_save_bookmarks(NULL, NULL);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *
 *
 * Description
 *   expand the bookmark tree
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_expand_activate(gpointer menuitem,
					gpointer user_data)
{
	elim_tree_expand_all(bookmark_tree);
}

/******************************************************************************
 * Name
 *  on_collapse_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_collapse_activate(gpointer menuitem,
 *				 gpointer user_data)
 *
 * Description
 *   collapse the bookmark tree
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_collapse_activate(gpointer menuitem,
					  gpointer user_data)
{
	elim_tree_collapse_all(bookmark_tree);
}

/******************************************************************************
 * Name
 *  on_add_bookmark_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_add_bookmark_activate(gpointer menuitem,
 *						gpointer user_data)
 *
 * Description
 *   add bookmark (current mod and key of active window (text, comm, or dict)
 *   to root node chosen by user
 *
 * Return value
 *   void
 */

void on_add_bookmark_activate(gpointer menuitem, gpointer user_data)
{
	ElimRow *selected = bookmark_selected();
	gchar *key = NULL;
	gchar *mod_name = NULL;
	gint test;
	BOOKMARK_DATA *data;
	GS_DIALOG *info;
	gchar buf[256];
	GString *str = g_string_new(NULL);

	if (!selected)
		return;

	mod_name = main_get_active_pane_module();
	key = main_get_active_pane_key();
	data = g_new(BOOKMARK_DATA, 1);
	info = gui_new_dialog();
	info->title = N_("Bookmark");
	g_string_printf(str, "<span weight=\"bold\">%s</span>", _("Add"));
	info->label_top = str->str;
	sprintf(buf, "%s, %s", key, mod_name);
	info->text1 = g_strdup(buf);
	info->text2 = g_strdup(key);
	info->text3 = g_strdup(mod_name);
	info->label1 = N_("Label: ");
	info->label2 = N_("Verse: ");
	info->label3 = N_("Module: ");
	info->ok = TRUE;
	info->cancel = TRUE;

	test = gui_gs_dialog(info);
	if (test == GS_OK) {
		data->caption = g_strdup(info->text1);
		data->key = g_strdup(info->text2);
		data->module = g_strdup(info->text3);
		data->module_desc =
		    g_strdup(main_get_module_description(info->text3));
		if (!strcmp(data->caption, buf))
			data->description = NULL;
		else
			data->description = g_strdup(info->text1);
		data->color = NULL;  /* bookmark leaves never have a color */
		data->is_leaf = TRUE;
		data->opened = bm_pixbufs->pixbuf_helpdoc;
		data->closed = NULL;
		gui_add_item_to_tree(selected, data);
		bookmarks_changed = TRUE;
		gui_save_bookmarks(NULL, NULL);
	}
// 	g_free(info->text1); /* we used g_strdup() */
}

/******************************************************************************
 * Name
 *  on_insert_bookmark_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_insert_bookmark_activate(gpointer menuitem,
 *						gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_insert_bookmark_activate(gpointer menuitem,
						 gpointer user_data)
{
	on_add_bookmark_activate(menuitem, NULL);
}

/******************************************************************************
 * Name
 *   on_new_subgroup_activate
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void on_new_subgroup_activate(gpointer menuitem,
 *			      gpointer user_data)
 *
 * Description
 *   add new sub group to selected group
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_new_folder_activate(gpointer menuitem,
					    gpointer user_data)
{
	ElimRow *selected = bookmark_selected();
	BOOKMARK_DATA *data;

	if (!selected)
		return;

	GtkBuilder *gxml = elim_gtk_builder_new();
	gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/folder.gtkbuilder", NULL);

	GtkWidget *dialog     = GTK_WIDGET(UI_GET_ITEM(gxml, "dialog_folder"));
	GtkWidget *entry      = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_entry_name"));
	GtkWidget *colorbtn   = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_color_button"));
	GtkWidget *clearbtn   = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_clear_color"));

	if (!dialog || !entry || !colorbtn || !clearbtn) {
		g_printerr("ERROR: dialog_folder widgets not found\n");
		g_object_unref(gxml);
		return;
	}

	gtk_window_set_title(GTK_WINDOW(dialog), _("New Folder"));
	gtk_editable_set_text(GTK_EDITABLE(entry), "");
	gtk_button_set_label(GTK_BUTTON(clearbtn), _("Add color"));
	g_signal_connect(clearbtn, "clicked",
		G_CALLBACK(toggle_color_clicked), colorbtn);
	g_signal_connect(colorbtn, "notify::rgba",
		G_CALLBACK(color_dialog_button_changed), GTK_BUTTON(clearbtn));

	gint response = gui_dialog_run(GTK_DIALOG(dialog));
	if (response == GTK_RESPONSE_OK) {
		const gchar *name = gtk_editable_get_text(GTK_EDITABLE(entry));
		gchar *color = NULL;

		if (g_strcmp0(gtk_button_get_label(GTK_BUTTON(clearbtn)), _("No color")) == 0) {
			GdkRGBA rgba;
			rgba = *gtk_color_dialog_button_get_rgba(
				GTK_COLOR_DIALOG_BUTTON(colorbtn));
			if (rgba.red < 0.99 || rgba.green < 0.99 || rgba.blue < 0.99)
				color = g_strdup_printf("#%02X%02X%02X",
					(guint)(rgba.red   * 255),
					(guint)(rgba.green * 255),
					(guint)(rgba.blue  * 255));
		}

		data = g_new0(BOOKMARK_DATA, 1);
		data->caption = g_strdelimit(g_strdup(name), "/|><.'`\"", ' ');
		data->color   = color;
		data->is_leaf = FALSE;
		data->opened = bm_pixbufs->pixbuf_opened;
		data->closed = bm_pixbufs->pixbuf_closed;
		bookmarks_changed = TRUE;
		gui_add_item_to_tree(selected, data);
		gui_save_bookmarks(NULL, NULL);
	}
	gui_widget_destroy(dialog);
	g_object_unref(gxml);
}

/******************************************************************************
 * Name
 *   on_open_in_tab_activate
 *
 * Synopsis
 *   #include "gui/.h"
 *
 *   void on_open_in_tab_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_open_in_tab_activate(gpointer menuitem,
					     gpointer user_data)
{
	ElimRow *selected = bookmark_selected();
	gchar *key = NULL;
	gchar *module = NULL;
	gchar *url = NULL;

	if (!selected)
		return;
	key = bookmark_row_dup(selected, COL_KEY);
	module = bookmark_row_dup(selected, COL_MODULE);
	url = g_strdup_printf("passagestudy.jsp?action=showBookmark&"
			      "type=%s&value=%s&module=%s",
			      "newTab",
			      main_url_encode(key),
			      main_url_encode(module));
	main_url_handler(url, TRUE);
	g_free(url);
	g_free(key);
	g_free(module);
}

/******************************************************************************
 * Name
 *   create_bookmark_menu
 *
 * Synopsis
 *   #include "gui/bookmarks_menu.h"
 *
 *   void create_bookmark_menu(void)
 *
 * Description
 *   create bookmark tree popup menu
 *  !! CHANGING MENU STRUCTURE DON'T FORGET ALSO CHANGE create_bookmark_menu !!
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_set_tag_color_activate(gpointer menuitem,
											   gpointer user_data)
{
	ElimRow *selected = bookmark_selected();
	gchar *color = NULL;

	if (!selected)
		return;

	/* Only folders get a color */
	if (!elim_row_n_children(selected))
		return;

	GtkWidget *dialog = gtk_color_chooser_dialog_new(
		_("Choose folder color"), GTK_WINDOW(widgets.app));

	/* Pre-load existing color if any */
	color = bookmark_row_dup(selected, COL_COLOR);
	if (color && *color) {
		GdkRGBA rgba;
		if (gdk_rgba_parse(&rgba, color))
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog), &rgba);
	}

	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		GdkRGBA rgba;
		gchar *hex;
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dialog), &rgba);
		hex = g_strdup_printf("#%02X%02X%02X",
			(guint)(rgba.red   * 255),
			(guint)(rgba.green * 255),
			(guint)(rgba.blue  * 255));
		bookmarks_changed = TRUE;
		bookmark_row_set_color(selected, hex);
		g_free(hex);
		gui_save_bookmarks(NULL, NULL);
		main_display_bible(NULL, settings.currentverse);
	}
	gui_widget_destroy(dialog);
	g_free(color);
}

/* Each item's action runs the handler it always ran. */
#define BOOKMARK_ACTION(fn) \
	static void fn##_action(GSimpleAction *a, GVariant *p, gpointer d) \
	{ (void)a; (void)p; (void)d; fn(NULL, NULL); }
BOOKMARK_ACTION(on_open_in_tab_activate)
BOOKMARK_ACTION(on_dialog_activate)
BOOKMARK_ACTION(on_new_folder_activate)
BOOKMARK_ACTION(on_insert_bookmark_activate)
BOOKMARK_ACTION(on_edit_item_activate)
BOOKMARK_ACTION(on_delete_item_activate)
BOOKMARK_ACTION(on_expand_activate)
BOOKMARK_ACTION(on_collapse_activate)
BOOKMARK_ACTION(bibletime_bookmarks_activate)
BOOKMARK_ACTION(andbible_bookmarks_activate)

void gui_create_bookmark_menu(void)
{
	if (menu.actions)
		return;
	const GActionEntry entries[] = {
		{ "en-pestana", on_open_in_tab_activate_action, NULL, NULL, NULL, { 0 } },
		{ "en-dialogo", on_dialog_activate_action, NULL, NULL, NULL, { 0 } },
		{ "carpeta", on_new_folder_activate_action, NULL, NULL, NULL, { 0 } },
		{ "insertar", on_insert_bookmark_activate_action, NULL, NULL, NULL, { 0 } },
		{ "editar", on_edit_item_activate_action, NULL, NULL, NULL, { 0 } },
		{ "eliminar", on_delete_item_activate_action, NULL, NULL, NULL, { 0 } },
		{ "expandir", on_expand_activate_action, NULL, NULL, NULL, { 0 } },
		{ "contraer", on_collapse_activate_action, NULL, NULL, NULL, { 0 } },
		{ "importar", bibletime_bookmarks_activate_action, NULL, NULL, NULL, { 0 } },
		{ "importar-andbible", andbible_bookmarks_activate_action, NULL, NULL, NULL, { 0 } },
		{ "reordenar", NULL, NULL, "false", on_reorder_state, { 0 } },
		{ "referencias", NULL, NULL, settings.crossref_popup ? "true" : "false",
		  on_crossref_popup_state, { 0 } },
		{ "colorear", NULL, NULL, settings.tag_colorize ? "true" : "false",
		  on_tag_colorize_state, { 0 } },
	};
	menu.actions = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(menu.actions), entries,
					G_N_ELEMENTS(entries), NULL);
	/* Row actions wait for a selection. */
	static const char *const row_actions[] = {
		"en-pestana", "en-dialogo", "carpeta", "insertar", "editar", "eliminar"
	};
	for (guint i = 0; i < G_N_ELEMENTS(row_actions); ++i)
		gui_bookmark_menu_enable(row_actions[i], FALSE);
}

void gui_bookmark_menu_enable(const char *action, gboolean enabled)
{
	gui_create_bookmark_menu();
	GAction *found = g_action_map_lookup_action(G_ACTION_MAP(menu.actions), action);
	if (found)
		g_simple_action_set_enabled(G_SIMPLE_ACTION(found), enabled);
}

gboolean gui_bookmark_menu_reordering(void)
{
	gui_create_bookmark_menu();
	GVariant *state = g_action_group_get_action_state(G_ACTION_GROUP(menu.actions),
							  "reordenar");
	gboolean on = state && g_variant_get_boolean(state);
	if (state)
		g_variant_unref(state);
	return on;
}

GtkWidget *gui_bookmark_menu_popup(GtkWidget *tree)
{
	gui_create_bookmark_menu();
	gui_widget_insert_action_group(tree, "marcadores", G_ACTION_GROUP(menu.actions));
	GMenu *model = g_menu_new();
	GMenu *items = g_menu_new();
	if (settings.browsing)
		g_menu_append(items, _("Abrir en pestaña nueva"), "marcadores.en-pestana");
	g_menu_append(items, _("Abrir en un diálogo"), "marcadores.en-dialogo");
	g_menu_append(items, _("Carpeta nueva"), "marcadores.carpeta");
	g_menu_append(items, _("Insertar marcador"), "marcadores.insertar");
	g_menu_append(items, _("_Editar elemento"), "marcadores.editar");
	g_menu_append(items, _("Eliminar elemento"), "marcadores.eliminar");
	g_menu_append_section(model, NULL, G_MENU_MODEL(items));
	g_object_unref(items);
	GMenu *tree_items = g_menu_new();
	g_menu_append(tree_items, _("Expandir todo"), "marcadores.expandir");
	g_menu_append(tree_items, _("Contraer todo"), "marcadores.contraer");
	g_menu_append_section(model, NULL, G_MENU_MODEL(tree_items));
	g_object_unref(tree_items);
	GMenu *toggles = g_menu_new();
	g_menu_append(toggles, _("Permitir reordenar"), "marcadores.reordenar");
	g_menu_append(toggles, _("Menú de referencias cruzadas"), "marcadores.referencias");
	g_menu_append(toggles, _("Colorear versículos por carpeta"), "marcadores.colorear");
	g_menu_append_section(model, NULL, G_MENU_MODEL(toggles));
	g_object_unref(toggles);
	GMenu *imports = g_menu_new();
	g_menu_append(imports, _("Importar marcadores"), "marcadores.importar");
	g_menu_append(imports, _("Importar marcadores de AndBible"), "marcadores.importar-andbible");
	g_menu_append_section(model, NULL, G_MENU_MODEL(imports));
	g_object_unref(imports);
	GtkWidget *popover = gui_popup_menu_model_at_pointer(G_MENU_MODEL(model), tree);
	g_object_unref(model);
	return popover;
}
