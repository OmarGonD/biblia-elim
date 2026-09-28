/*
 * Xiphos Bible Study Tool
 * search_dialog.c - gui for searching Sword modules
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
#include "gui/widget_helpers.h"
#include "gui/dropdown_helpers.h"
#include "gui/table_helpers.h"

#include "xiphos_html/xiphos_html.h"

#include <regex.h>
#include <string.h>
#include <glib.h>

#include "gui/search_dialog.h"
#include "gui/main_window.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"
#include "gtk/gtk.h"
#include "gui/export_bookmarks.h"

#include "main/search_dialog.h"
#include "main/configs.h"
#include "main/lists.h"
#include "main/previewer.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"
#include "main/xml.h"
#include "main/biblesync_glue.h"

#include "gui/debug_glib_null.h"

/******************************************************************************/
#define _BYTE 8
#define _WORD 16
#define _DWORD 32

#define SEARCHING N_("Searching the ")
#define SMODULE N_(" Module")
#define FINDS N_("found in ")
#define HTML_START "<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" /><style type=\"text/css\"><!-- A { text-decoration:none } *[dir=rtl] { text-align: right; } --></style></head>"

SEARCH_DIALOG1 search1;

static gboolean _preview_on;
static gchar *module_selected;
gchar *verse_selected;

GtkWidget *remember_search; /* needed to change button in search stop */

/******************************************************************************
 * Name
 *   on_advsearch_configure_event
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   gboolean on_advsearch_configure_event(GtkWidget * widget,
 *				   GdkEventConfigure * event,
 *				   gpointer user_data)
 *
 * Description
 *   remember placement+size of parallel window.
 *   cloned from on_configure_event
 *
 * Return value
 *   gboolean
 */

static void on_advsearch_configure_event(GObject *window, GParamSpec *pspec,
					 gpointer user_data)
{
	gchar layout[10];
	gint width, height;

	(void)pspec;
	(void)user_data;
	/* GTK 4 keeps the window's size as its default size; the position
	 * belongs to the compositor */
	gtk_window_get_default_size(GTK_WINDOW(window), &width, &height);
	if (width <= 0 || height <= 0)
		return;
	settings.advsearch_width = width;
	settings.advsearch_height = height;

	sprintf(layout, "%d", settings.advsearch_width);
	xml_set_value("Xiphos", "layout", "advsearch_width", layout);

	sprintf(layout, "%d", settings.advsearch_height);
	xml_set_value("Xiphos", "layout", "advsearch_height", layout);
	xml_save_settings_doc(settings.fnconfigure);
}

/* click on treeview folder to expand or collapse it */
static gboolean button_release_event(GtkWidget *widget,
				     GuiButtonEvent *event, gpointer data)
{
	ElimRow *selected = elim_table_get_selected(widget);

	if (!selected || !elim_row_n_children(selected))
		return FALSE;

	/* the arrow of a row opens and closes it by itself */
	if (elim_tree_point_on_expander(widget, event->x, event->y))
		return FALSE;
	if (elim_tree_row_expanded(widget, selected))
		elim_tree_collapse_row(widget, selected);
	else
		elim_tree_expand_row(widget, selected, FALSE);
	return FALSE;
}

void on_comboboxentry2_changed(GObject *combobox, GParamSpec *pspec,
			       gpointer user_data)
{
	main_comboboxentry2_changed(combobox, pspec, user_data);
}

/******************************************************************************
 * Name
 *   button_clean
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void button_clean(GtkButton * button, gpointer user_data)
 *
 * Description
 *   user pressed clear button - clears search results page
 *
 * Return value
 *   void
 */

void button_clean(GtkButton *button, gpointer user_data)
{
	GString *html_text = g_string_new("");

	XI_message(("button_clean"));
	g_list_store_remove_all(elim_table_get_store(search1.listview_results));
	g_list_store_remove_all(elim_table_get_store(search1.listview_verses));

	g_string_printf(html_text,
			HTML_START
			"<body text=\"%s\" bgcolor=\"%s\"> </body></html>",
			settings.bible_text_color,
			settings.bible_bg_color);
	XIPHOS_HTML_OPEN_STREAM(search1.preview_html, "text/html");
	XIPHOS_HTML_WRITE(search1.preview_html, html_text->str,
			  html_text->len);
	XIPHOS_HTML_CLOSE(search1.preview_html);
	g_string_free(html_text, TRUE);
}

/******************************************************************************
 * Name
 *   button_save
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void button_save(GtkButton * button, gpointer user_data)
 *
 * Description
 *   calls main_save_current_adv_search_as_bookmarks() in main/search_dialog.cc
 *   to do the work of saving current search as bookmarks
 *
 * Return value
 *   void
 */

void button_save(GtkButton *button, gpointer user_data)
{
	main_save_current_adv_search_as_bookmarks();
}

/******************************************************************************
 * Name
 *   button_export
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void button_export(GtkButton * button, gpointer user_data)
 *
 * Description
 *   calls main_save_current_adv_search_as_bookmarks() in main/search_dialog.cc
 *   to do the work of saving current search as bookmarks
 *
 * Return value
 *   void
 */

void button_export(GtkButton *button, gpointer user_data)
{
	gui_export_bookmarks_dialog(ADV_SEARCH_RESULTS_EXPORT, NULL);
}

/******************************************************************************
 * Name
 *   on_destroy
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void on_destroy(GtkWidget * dialog, gpointer user_data)
 *
 * Description
 *   destroy the search dialog
 *
 * Return value
 *   void
 */

void _on_destroy(GtkWidget *dialog, gpointer user_data)
{
	/* main_do_dialog_search() initializes these as search starts */
	if (search_active) {
		terminate_search = TRUE;
		sync_windows();
	} else {
		main_close_search_dialog();

		settings.display_advsearch = 0;
		xml_set_value("Xiphos", "layout", "advsearchopen", "0");

		if (module_selected) {
			g_free(module_selected);
			module_selected = NULL;
		}
		if (verse_selected) {
			g_free(verse_selected);
			verse_selected = NULL;
		}
	}
}

/******************************************************************************
 * Name
 *   on_button_begin_search
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void on_button_begin_search(GtkButton * button, gpointer user_data)
 *
 * Description
 *   starts the search
 *
 * Return value
 *   void
 */

void on_button_begin_search(GtkButton *button, gpointer user_data)
{
	if (search_active) {
		terminate_search = TRUE;
		gtk_button_set_icon_name(GTK_BUTTON(remember_search), "edit-find");
		sync_windows();
	} else {
		gtk_button_set_icon_name(GTK_BUTTON(remember_search), "process-stop");

		// do the search
		main_do_dialog_search();

		gtk_button_set_icon_name(GTK_BUTTON(remember_search), "edit-find");
	}
}

/******************************************************************************
 * Name
 *   list_name_changed
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void list_name_changed(GtkEditable * editable,
 *			       gpointer user_data)
 *
 * Description
 *   text in the range name entry has changed
 *   name text in the clist_range is updated to match
 *
 * Return value
 *   void
 */

void list_name_changed(GtkEditable *editable, gpointer user_data)
{
	ElimRow *selected = elim_table_get_selected(search1.module_lists);

	if (!selected)
		return;

	elim_row_set_string(selected, 0, gtk_editable_get_text(GTK_EDITABLE(editable)));
}

/******************************************************************************
 * Name
 *   range_name_changed
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void range_name_changed(GtkEditable * editable,
 *			       gpointer user_data)
 *
 * Description
 *   text in the range name entry has changed
 *   name text in the clist_range is updated to match
 *
 * Return value
 *   void
 */

void range_name_changed(GtkEditable *editable, gpointer user_data)
{
	ElimRow *selected = elim_table_get_selected(search1.list_range_name);

	if (!selected)
		return;

	elim_row_set_string(selected, 0, gtk_editable_get_text(GTK_EDITABLE(editable)));
}

/******************************************************************************
 * Name
 *   range_text_changed
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void range_text_changed(GtkEditable * editable,
 *			       gpointer user_data)
 *
 * Description
 *   text in the range text entry has changed
 *
 * Return value
 *   void
 */

void range_text_changed(GtkEditable *editable, gpointer user_data)
{
	main_range_text_changed(editable);
}

/******************************************************************************
 * Name
 *   new_modlist
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void new_modlist(GtkButton * button, gpointer user_data)
 *
 * Description
 *   adds a new custom module list to the
 *
 * Return value
 *   void
 */

void new_modlist(GtkButton *button, gpointer user_data)
{
	gchar buf[80];
	GListStore *lists = elim_table_get_store(search1.module_lists);
	ElimRow *row = elim_row_new(2);

	{
		// must encode locale-sensitively
		char *num = main_format_number(search1.list_rows);
		sprintf(buf, _("New List %s"), num);
		g_free(num);
	}

	search1.module_count = 0;
	g_list_store_remove_all(elim_table_get_store(search1.listview_modules));

	elim_row_set_string(row, 0, buf);
	g_list_store_append(lists, row);
	g_object_unref(row);
	elim_table_select(search1.module_lists,
			  g_list_model_get_n_items(G_LIST_MODEL(lists)) - 1, TRUE);
	gtk_editable_set_text(GTK_EDITABLE(search1.entry_list_name), buf);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void clear_modules(GtkButton *button, gpointer user_data)
{
	gchar *str;

	str = g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s",
			      _("Clear List?"),
			      _("Are you sure you want to clear the module list?"));

	if (gui_yes_no_dialog(str, "dialog-warning")) {
		ElimRow *selected = elim_table_get_selected(search1.module_lists);

		g_list_store_remove_all(elim_table_get_store(search1.listview_modules));
		if (selected)
			elim_row_set_string(selected, 1, "");
	}
	g_free(str);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void delete_module(GtkButton *button, gpointer user_data)
{
	main_delete_module(search1.listview_modules);
}

/******************************************************************************
 * Name
 *   save_modlist
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void save_modlist(GtkButton * button, gpointer user_data)
 *
 * Description
 *   saves the custom module list
 *
 * Return value
 *   void
 */

void save_modlist(GtkButton *button, gpointer user_data)
{
	main_save_modlist();
}

/******************************************************************************
 * Name
 *   new_range
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void new_range(GtkButton * button, gpointer user_data)
 *
 * Description
 *   adds a new custom range to the clist_range
 *
 * Return value
 *   void
 */

void new_range(GtkButton *button, gpointer user_data)
{
	gchar *text[2];
	GListStore *ranges = elim_table_get_store(search1.list_range_name);
	ElimRow *row = elim_row_new(2);

	text[0] = _("[New Range]");
	text[1] = "";

	elim_row_set_string(row, 0, text[0]);
	elim_row_set_string(row, 1, text[1]);
	g_list_store_append(ranges, row);
	g_object_unref(row);
	elim_table_select(search1.list_range_name,
			  g_list_model_get_n_items(G_LIST_MODEL(ranges)) - 1, TRUE);

	gtk_editable_set_text(GTK_EDITABLE(search1.entry_range_name), text[0]);
	gtk_editable_set_text(GTK_EDITABLE(search1.entry_range_text), "");
}

/******************************************************************************
 * Name
 *   save_range
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void save_range(GtkButton * button, gpointer user_data)
 *
 * Description
 *   saves the custom range list
 *
 * Return value
 *   void
 */

void save_range(GtkButton *button, gpointer user_data)
{
	main_save_range();
}

/******************************************************************************
 * Name
 *   delete_range
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void delete_range(GtkButton * button, gpointer user_data)
 *
 * Description
 *   delete the selected custom range
 *
 * Return value
 *   void
 */

void delete_range(GtkButton *button, gpointer user_data)
{
	main_delete_range();
}

/******************************************************************************
 * Name
 *   delete_list
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void delete_list(GtkButton * button, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void delete_list(GtkButton *button, gpointer user_data)
{
	gchar *name_string = NULL;
	ElimRow *selected = elim_table_get_selected(search1.module_lists);
	guint position = elim_table_get_selected_position(search1.module_lists);
	gchar *str;

	if (!selected)
		return;
	name_string = g_strdup(elim_row_get_string(selected, 0));

	str = g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s %s",
			      _("Delete list?"),
			      _("Are you sure you want to delete:"),
			      name_string);

	if (!gui_yes_no_dialog(str, "dialog-warning")) {
		g_free(name_string);
		g_free(str);
		return;
	}

	g_list_store_remove(elim_table_get_store(search1.module_lists), position);
	xml_remove_node("modlists", "modlist", name_string);
	--search1.list_rows;
	save_modlist(NULL, NULL);

	g_free(name_string);
	g_free(str);
}

/******************************************************************************
 * Name
 *   scope_toggled
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void scope_toggled(GtkToggleButton *togglebutton,
						gpointer user_data)
 *
 * Description
 *   remember which scope button was pressed last
 *   does not remember rb_last
 *
 * Return value
 *   void
 */

void scope_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	search1.which_scope = togglebutton;
	if (gui_toggle_get_active(GTK_WIDGET(search1.rb_custom_range)))
		gtk_widget_set_sensitive(search1.combo_range, TRUE);
	else
		gtk_widget_set_sensitive(search1.combo_range, FALSE);
}

/******************************************************************************
 * Name
 *   mod_list_toggled
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void mod_list_toggled(GtkToggleButton *togglebutton,
 *			   gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void mod_list_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	if (gui_toggle_get_active(togglebutton)) {
		main_comboboxentry2_changed(G_OBJECT(search1.combo_list),
					    NULL, user_data);
	}
	if (gui_toggle_get_active(GTK_WIDGET(search1.rb_custom_list)))
		gtk_widget_set_sensitive(search1.combo_list, TRUE);
	else
		gtk_widget_set_sensitive(search1.combo_list, FALSE);
}

/******************************************************************************
 * Name
 *   optimized_toggled
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void optimized_toggled(GtkToggleButton *togglebutton,
 *			    gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void optimized_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	if (gui_toggle_get_active(togglebutton)) {
		gtk_widget_show(search1.button_intro_lucene);
		gtk_widget_set_sensitive(search1.cb_case_sensitive, FALSE);
	} else {
		gtk_widget_hide(search1.button_intro_lucene);
		gtk_widget_set_sensitive(search1.cb_case_sensitive, TRUE);
	}
}

/******************************************************************************
 * Name
 *   on_lucene_intro_clicked
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void on_lucene_intro_clicked(GtkToggleButton *button,
 *				  gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

#define LUCENE_INTRO \
	_("<b>Syntax overview for optimized \"lucene\" searches</b>\nSearch for verses that contain...\n\nloved one\n\t \"loved\" or \"one\"\n\tThis is the same as searching for loved OR one\n\"loved one\"\n\tThe phrase \"loved one\"\nlove*\n\tA word starting with \"love\"\n\t(love OR loves OR loved OR etc...)\nloved AND one\n\tThe word \"loved\" and the word \"one\"\n\t&amp;&amp; can be used in place of AND\n+loved one\n\tVerses that <b>must</b> contain \"loved\" and <b>may</b> contain \"one\"\nloved NOT one\n\t\"loved\" but not \"one\"\n(loved one) AND God\n\t\"loved\" or \"one\" and \"God\"\nlemma:G2316\n\tSearch for the Strong's Greek (\"G\") word number 2316.\n\tAlso, select Strong's display on the <i>Attribute Search</i> tab.\n\nFor complete details, search the web for \"lucene search syntax\".")

void on_lucene_intro_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new_with_markup(NULL, /* no need for a parent window */
						    GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_MESSAGE_INFO,
						    GTK_BUTTONS_OK,
						    LUCENE_INTRO);
	g_signal_connect_swapped(dialog, "response",
				 G_CALLBACK(gui_widget_destroy), dialog);
	gtk_widget_show(dialog);
}

/******************************************************************************
 * Name
 *   attributes_toggled
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void attributes_toggled(GtkToggleButton *togglebutton,
 *			     gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void attributes_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	if (gui_toggle_get_active(togglebutton))
		gtk_widget_show(search1.button_intro_attributes);
	else
		gtk_widget_hide(search1.button_intro_attributes);
}

/******************************************************************************
 * Name
 *   on_attributes_intro_clicked
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void on_attributes_intro_clicked(GtkToggleButton *button,
 *				  gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

#define ATTRIBUTES_INTRO \
	_("<b>Attribute-based searches</b>\nSearches for content contained in markup outside the main text. Attributes are footnotes, Strong's numbers, and morphological symbols.\n\nBe aware that most such searches can now be done faster via optimized \"lucene\" searches using keyword qualifiers.\n\nTo use attribute searches, you must select the appropriate button on the <i>Attribute Search</i> tab.\n* Footnote text is searched just like regular text.\n* Strong's words are specified as a prefix letter H or G (Hebrew or Greek) and the numeric word identifier, e.g. G2316 to find \"θεός\" (\"God\").\n* Morphological tags are identified literally, e.g. N-ASF for \"noun, accusative singular feminine\" -- see the Robinson module for details.")

void on_attributes_intro_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new_with_markup(NULL, /* no need for a parent window */
						    GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_MESSAGE_INFO,
						    GTK_BUTTONS_OK,
						    ATTRIBUTES_INTRO);
	g_signal_connect_swapped(dialog, "response",
				 G_CALLBACK(gui_widget_destroy), dialog);
	gtk_widget_show(dialog);
}

/******************************************************************************
 * Name
 *   current_module_toggled
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void current_module_toggled(GtkToggleButton *togglebutton,
						gpointer user_data)
 *
 * Description
 *   sets rb_last to insensitive if either use module list
 *   or use custom list is clicked
 *   also set scope button to last one used before rb_last
 *
 * Return value
 *   void
 */

void current_module_toggled(GtkToggleButton *togglebutton,
			    gpointer user_data)
{
	if (gui_toggle_get_active(togglebutton)) {
		main_change_mods_select_label(search1.search_mod);
		gtk_widget_set_sensitive(search1.rb_last, TRUE);
		gtk_widget_set_sensitive(search1.combo_list, FALSE);
	} else {
		gtk_widget_set_sensitive(search1.rb_last, FALSE);
		gui_toggle_set_active(search1.which_scope, TRUE);
	}
}

/******************************************************************************
 * Name
 *   mod_selection_changed
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void mod_selection_changed(GObject * selection,
 *		      GtkWidget * tree_widget)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void mod_selection_changed(GObject *selection, GParamSpec *pspec,
				  GtkWidget *tree_widget)
{
	(void)selection;
	(void)pspec;
	main_mod_selection_changed(tree_widget);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void (GObject * selection,
 *		     					 gpointer data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void _selection_finds_list_changed(GObject *selection,
					  GParamSpec *pspec,
					  gpointer data)
{
	(void)selection;
	(void)pspec;
	(void)data;
	main_selection_finds_list_changed();
}

/******************************************************************************
 * Name
 *   selection_modules_lists_changed
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void selection_modules_lists_changed(GObject * selection,
 *		     					 gpointer data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void _selection_modules_lists_changed(GObject *selection,
					     GParamSpec *pspec,
					     gpointer data)
{
	(void)selection;
	(void)pspec;
	(void)data;
	main_selection_modules_lists_changed();
}

/******************************************************************************
 * Name
 *   _modules_lists_changed
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void _modules_lists_changed(GObject * selection,
 *		     					 gpointer data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void _modules_lists_changed(GObject *selection, GParamSpec *pspec,
				   GtkWidget *tree_widget)
{
	ElimRow *selected = elim_table_get_selected(tree_widget);

	(void)selection;
	(void)pspec;
	if (!selected)
		return;
	if (elim_row_n_children(selected)) {
		g_free(module_selected);
		module_selected = NULL;
		return;
	}

	if (*elim_row_get_string(selected, UTIL_COL_MODULE)) {
		g_free(module_selected);
		module_selected = g_strdup(elim_row_get_string(selected, UTIL_COL_MODULE));
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void (GObject * selection,
 *		     					 gpointer data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

/* a double click or Enter on a verse opens it in the main window */
static void _verselist_activated(GtkWidget *view, guint position,
				 gpointer data)
{
	(void)data;
	/* a double click picks the row first, so the picked row is the one */
	elim_table_select(view, position, FALSE);
	main_finds_verselist_selection_changed(view, TRUE);
}

/******************************************************************************
 * Name
 *  on_treeview_button_advsearch_press_event
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *   gboolean on_treeview_button_advsearch_press_event(GtkWidget * widget,
 *						       GuiButtonEvent * event,
 *						       gpointer data)
 *
 * Description
 *   handles context menu kickoff.
 *
 * Return value
 *   gboolean
 */

static gboolean on_treeview_button_press_event_advsearch(GtkWidget *
							     widget,
							 GuiButtonEvent *
							     event,
							 gpointer
							     user_data)
{
	if (event->button == 3) {
		gui_popup_menu_model_at_pointer(search1.menu_item_send_search,
						widget);
		return TRUE;
	} else {
		return FALSE;
	}
}

/******************************************************************************
 * Name
 *   on_send_list_via_biblesync_advsearch_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *   GtkWidget* on_send_list_via_biblesync_advsearch_activate
 *
 * Description
 *   ship the adv.search window's current verse list.
 *
 * Return value
 *   GtkWidget*
 */

G_MODULE_EXPORT void
on_send_list_via_biblesync_advsearch_activate(gpointer menuitem,
					      gpointer user_data)
{
	if (biblesync_active_xmit_allowed()) {
		GListStore *store = elim_table_get_store(search1.listview_verses);
		guint position, n = g_list_model_get_n_items(G_LIST_MODEL(store));

		GString *vlist = g_string_new("");
		gchar *module = NULL, *text, *buf;

		gboolean first = TRUE;

		for (position = 0; position < n; position++) {
			// retrieve a verse ref line.
			text = g_strdup(elim_row_get_string(elim_table_get(store, position), 0));

			// painful parse of "NET: Revelation of John 2:12 and text here..."
			// first `:' finds end of module name.
			buf = strchr(text, ':');
			*buf = '\0';
			// key starts 2 characters after the (former) ':'.
			gchar *key = buf + 2;
			// next : is the middle of chapter:verse.
			buf = strchr(key, ':');
			// if that works, then space after that ends key.
			// it might not work, if not bible/comm...to be caught just below.
			if (buf)
				buf = strchr(buf, ' ');
			if (buf)
				*buf = '\0';

			if (first) {
				/* we can send only verse nav -- no lex, no books */
				module = g_strdup(text);
				if (!main_is_module(module) ||
				    ((main_get_mod_type(module) !=
				      TEXT_TYPE) &&
				     (main_get_mod_type(module) !=
				      COMMENTARY_TYPE))) {
					g_free(module);
					g_free(text);
					g_string_free(vlist, TRUE);
					gui_generic_warning(_("Module is neither Bible nor commentary"));
					return;
				}
			} else {
				vlist = g_string_append_c(vlist, ';');
			}
			vlist = g_string_append(vlist, (char *)
						main_get_osisref_from_key(module, key));

			g_free(text);
			first = FALSE;
		}

		biblesync_transmit_verse_list(module, vlist->str);
		g_free(module);
		g_string_free(vlist, TRUE);
	} else {
		gui_generic_warning(_("BibleSync is not active for transmit."));
	}
}

/******************************************************************************
 * Name
 *   create_results_menu_advsearch
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *   GtkWidget* create_results_menu_advsearch (void)
 *
 * Description
 *   initialize context menu in adv.search results.
 *
 * Return value
 *   GtkWidget*
 */

static void send_list_action(GSimpleAction *action, GVariant *parameter,
			     gpointer data)
{
	(void)action;
	(void)parameter;
	on_send_list_via_biblesync_advsearch_activate(NULL, data);
}

/* The results list's menu: its action lives on the list (see below). */
GMenuModel *create_results_menu_advsearch(void)
{
	GMenu *menu = g_menu_new();

	g_menu_append(menu, _("Enviar lista por BibleSync"),
		      "busqueda.enviar-biblesync");
	return G_MENU_MODEL(menu);
}

/******************************************************************************
 * Name
 *   selection_range_lists_changed
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void selection_range_lists_changed(GObject * selection,
 *		     					 gpointer data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void selection_range_lists_changed(GObject *selection,
					  GParamSpec *pspec,
					  gpointer data)
{
	gchar *name, *range;
	ElimRow *selected = elim_table_get_selected(search1.list_range_name);

	(void)selection;
	(void)pspec;
	(void)data;
	if (!selected)
		return;

	/* the entries' "changed" handlers write into the selected row */
	name = g_strdup(elim_row_get_string(selected, 0));
	range = g_strdup(elim_row_get_string(selected, 1));
	gtk_editable_set_text(GTK_EDITABLE(search1.entry_range_name), name);
	gtk_editable_set_text(GTK_EDITABLE(search1.entry_range_text), range);
	g_free(name);
	g_free(range);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void (GObject * selection,
 *		     					 gpointer data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void selection_verselist_changed(GObject *selection, GParamSpec *pspec,
					gpointer data)
{
	(void)selection;
	(void)pspec;
	(void)data;
	main_finds_verselist_selection_changed(search1.listview_verses, FALSE);
}

/******************************************************************************
 * Name
 *   setup_two_text_columns
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void setup_two_text_columns(GtkWidget * listview)
 *
 * Description
 *   the lists of this dialog are two-column lists without a header: the
 *   first column wraps and takes the width there is, the second is the
 *   value that goes with it. The rows are ElimRow of two strings.
 *
 * Return value
 *   void
 */

static void _setup_two_text_columns(GtkWidget *listview)
{
	ElimTextColumn columns[2] = { elim_text_column(0), elim_text_column(1) };
	GListStore *store = elim_table_new();

	columns[0].expand = TRUE;
	columns[0].wrap = TRUE;
	elim_table_setup_rows(listview, store, columns, 2);
	/* the view keeps the store */
	g_object_unref(store);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

static void _setup_combobox(GtkDropDown *combo)
{
	elim_dropdown_prepare(combo);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

static void _setup_listviews(GtkWidget *listview, GCallback callback)
{
	_setup_two_text_columns(listview);
	if (!callback)
		return;
	/* CALLBACK hears the reader pick a row (a "notify::selected" handler) */
	g_signal_connect(elim_table_selection(listview), "notify::selected",
			 callback, NULL);
}

/* the verse list: picking a verse previews it, activating one opens it;
 * CALLBACK is the activation handler */
static void _setup_listviews2(GtkWidget *listview, GCallback callback)
{
	_setup_two_text_columns(listview);
	if (!callback)
		return;
	g_signal_connect(listview, "activate", callback, NULL);
	g_signal_connect(elim_table_selection(listview), "notify::selected",
			 G_CALLBACK(selection_verselist_changed), NULL);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

static void _setup_treeview(GtkWidget *treeview)
{
	gui_load_module_tree(treeview, FALSE);

	g_signal_connect(elim_table_selection(treeview), "notify::selected-item",
			 G_CALLBACK(mod_selection_changed), treeview);

	gui_widget_on_button(GTK_WIDGET(treeview), GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)button_release_event, GINT_TO_POINTER(0));
}

static void _setup_treeview2(GtkWidget *treeview)
{
	gui_load_module_tree(treeview, FALSE);

	g_signal_connect(elim_table_selection(treeview), "notify::selected-item",
			 G_CALLBACK(_modules_lists_changed), treeview);
}

void on_closebutton2_clicked(GtkButton *button, gpointer user_data)
{
	gtk_widget_hide(search1.mod_sel_dialog);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

void _on_dialog2_response(GtkDialog *dialog, gint response_id,
			  gpointer user_data)
{
	switch (response_id) {
	case GTK_RESPONSE_CLOSE:
		gtk_widget_hide(GTK_WIDGET(dialog));
		break;
	case GTK_RESPONSE_APPLY:
		main_add_mod_to_list(search1.listview_modules,
				     module_selected);
		break;
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *   Creates the module selection dialog
 *
 * Return value
 *   void
 */


/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *   Shows the module selection dialog
 *
 * Return value
 *   void
 */

void
on_toolbutton12_clicked(GtkButton *toolbutton, gpointer user_data)
{
	gtk_widget_show(search1.mod_sel_dialog);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

/* add html widgets */
static void _add_html_widget(GtkWidget *vbox)
{

	search1.preview_html =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, DIALOG_SEARCH_PREVIEW_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(search1.preview_html, "search-previewer");
	gtk_widget_show(search1.preview_html);
	gui_box_pack(GTK_BOX(vbox), search1.preview_html, TRUE, TRUE, 0);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

void _on_dialog_response(GtkDialog *dialog, gint response_id,
			 gpointer user_data)
{
	switch (response_id) {
	case GTK_RESPONSE_CLOSE:
		if (search1.mod_sel_dialog) {
			gui_widget_destroy(GTK_WIDGET(search1.mod_sel_dialog));
			search1.mod_sel_dialog = NULL;
		}
		gui_widget_destroy(GTK_WIDGET(dialog));
		break;
	}
}

/******************************************************************************
 * Name
 *   _create_search_dialog
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void _create_search_dialog(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void _create_search_dialog(void)
{
	GtkBuilder *gxml;
	GtkWidget *toolbutton1;
	GtkWidget *toolbutton2;
	GtkWidget *toolbutton3;
	GtkWidget *toolbutton4;
	GtkWidget *toolbutton5;
	GtkWidget *toolbutton6;
	GtkWidget *toolbutton7;
	GtkWidget *toolbutton8;
	GtkWidget *toolbutton10;
	GtkWidget *toolbutton11;
	GtkWidget *toolbutton12;
	GtkWidget *toolbutton13;
	module_selected = NULL;
	verse_selected = NULL;
	_preview_on = TRUE;

/* build the widget */
	gxml = elim_gtk_builder_new();
	gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/search-dialog.gtkbuilder", NULL);
	g_return_if_fail(gxml != NULL);

	/* lookup the root widget */
	search1.dialog = UI_GET_ITEM(gxml, "dialog");
	/* Hija de la ventana principal: así el gestor de ventanas la trata
	 * como el diálogo que es -- flota sobre el texto, se puede mover y
	 * se cierra con ella -- en vez de encajarla como una ventana más y
	 * dejarla clavada encima de lo que uno quiere leer. */
	if (widgets.app)
		gtk_window_set_transient_for(GTK_WINDOW(search1.dialog),
					     GTK_WINDOW(widgets.app));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(search1.dialog), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(search1.dialog), settings.advsearch_width, settings.advsearch_height);

	g_signal_connect(search1.dialog, "response",
			 G_CALLBACK(_on_dialog_response), NULL);
	g_signal_connect(search1.dialog, "destroy",
			 G_CALLBACK(_on_destroy), NULL);

	remember_search = UI_GET_ITEM(gxml, "button1");
	g_signal_connect(remember_search, "clicked",
			 G_CALLBACK(on_button_begin_search), NULL);

	search1.label_search_module = UI_GET_ITEM(gxml, "label5");
	search1.search_entry = UI_GET_ITEM(gxml, "entry1");
	g_signal_connect(search1.search_entry, "activate",
			 G_CALLBACK(on_button_begin_search), NULL);

	search1.notebook = UI_GET_ITEM(gxml, "notebook1");
	search1.treeview = UI_GET_ITEM(gxml, "treeview1");
	_setup_treeview(search1.treeview);

	search1.list_range_name = UI_GET_ITEM(gxml, "treeview4");
	_setup_listviews(search1.list_range_name,
			 (GCallback)selection_range_lists_changed);

	search1.list_ranges = UI_GET_ITEM(gxml, "treeview5");
	_setup_listviews(search1.list_ranges, NULL);

	search1.module_lists = UI_GET_ITEM(gxml, "treeview6");
	_setup_listviews(search1.module_lists,
			 (GCallback)_selection_modules_lists_changed);

	search1.listview_modules = UI_GET_ITEM(gxml, "treeview7");
	_setup_listviews(search1.listview_modules, NULL);

	/* scope radio buttons */
	search1.rb_no_scope = UI_GET_ITEM(gxml, "radiobutton1");
	g_signal_connect(search1.rb_no_scope, "toggled",
			 G_CALLBACK(scope_toggled), NULL);

	search1.rb_last = UI_GET_ITEM(gxml, "radiobutton2");
	search1.which_scope = (GtkToggleButton *)(search1.rb_no_scope);
	search1.rb_custom_range = UI_GET_ITEM(gxml, "radiobutton3");
	g_signal_connect(search1.rb_custom_range, "toggled",
			 G_CALLBACK(scope_toggled), NULL);
	/* modules radio buttons */
	search1.rb_current_module = UI_GET_ITEM(gxml, "radiobutton4");
	g_signal_connect(search1.rb_current_module, "toggled",
			 G_CALLBACK(current_module_toggled), NULL);
	search1.rb_mod_list = UI_GET_ITEM(gxml, "radiobutton5");
	g_signal_connect(search1.rb_mod_list, "toggled",
			 G_CALLBACK(mod_list_toggled), NULL);
	search1.rb_custom_list = UI_GET_ITEM(gxml, "radiobutton6");
	g_signal_connect(search1.rb_custom_list, "toggled",
			 G_CALLBACK(mod_list_toggled), NULL);

	/* search type selection */
	search1.rb_words = UI_GET_ITEM(gxml, "radiobutton9");
	search1.rb_regexp = UI_GET_ITEM(gxml, "radiobutton10");
	search1.rb_exact_phrase = UI_GET_ITEM(gxml, "radiobutton11");
	search1.rb_optimized = UI_GET_ITEM(gxml, "radiobutton16");
	g_signal_connect(search1.rb_optimized, "toggled",
			 G_CALLBACK(optimized_toggled), NULL);

	search1.button_intro_lucene =
	    UI_GET_ITEM(gxml, "button_intro_lucene");
	g_signal_connect(search1.button_intro_lucene, "clicked",
			 G_CALLBACK(on_lucene_intro_clicked), NULL);
	gtk_widget_show(search1.button_intro_lucene);

	search1.rb_attributes = UI_GET_ITEM(gxml, "radiobutton12");
	g_signal_connect(search1.rb_attributes, "toggled",
			 G_CALLBACK(attributes_toggled), NULL);

	search1.button_intro_attributes =
	    UI_GET_ITEM(gxml, "button_intro_attributes");
	g_signal_connect(search1.button_intro_attributes, "clicked",
			 G_CALLBACK(on_attributes_intro_clicked), NULL);
	gtk_widget_hide(search1.button_intro_attributes);

	/* attributes radio buttons */
	search1.rb_strongs = UI_GET_ITEM(gxml, "radiobutton13");
	search1.rb_morphs = UI_GET_ITEM(gxml, "radiobutton15");
	search1.rb_footnotes = UI_GET_ITEM(gxml, "radiobutton14");

	/*   */
	search1.cb_case_sensitive = UI_GET_ITEM(gxml, "checkbutton1");

	/* display options check buttons */
	search1.cb_include_strongs = UI_GET_ITEM(gxml, "checkbutton2");
	search1.cb_include_morphs = UI_GET_ITEM(gxml, "checkbutton3");
	search1.cb_include_footnotes = UI_GET_ITEM(gxml, "checkbutton4");

	toolbutton1 = UI_GET_ITEM(gxml, "toolbutton1");
	toolbutton2 = UI_GET_ITEM(gxml, "toolbutton2");
	toolbutton3 = UI_GET_ITEM(gxml, "toolbutton3");
	toolbutton4 = UI_GET_ITEM(gxml, "toolbutton4");
	toolbutton5 = UI_GET_ITEM(gxml, "toolbutton5");
	toolbutton6 = UI_GET_ITEM(gxml, "toolbutton6");
	toolbutton7 = UI_GET_ITEM(gxml, "toolbutton7");
	toolbutton8 = UI_GET_ITEM(gxml, "toolbutton8");
	toolbutton10 = UI_GET_ITEM(gxml, "toolbutton10");
	toolbutton11 = UI_GET_ITEM(gxml, "toolbutton11");
	toolbutton12 = UI_GET_ITEM(gxml, "toolbutton12");
	toolbutton13 = UI_GET_ITEM(gxml, "toolbutton_export");
	search1.combo_list = UI_GET_ITEM(gxml, "comboboxentry2");

	g_signal_connect(toolbutton1, "clicked",
			 G_CALLBACK(button_save), NULL);

	g_signal_connect(toolbutton2, "clicked",
			 G_CALLBACK(button_clean), NULL);

	g_signal_connect(toolbutton3, "clicked",
			 G_CALLBACK(new_range), NULL);

	g_signal_connect(toolbutton4, "clicked",
			 G_CALLBACK(save_range), NULL);

	g_signal_connect(toolbutton5, "clicked",
			 G_CALLBACK(delete_range), NULL);

	g_signal_connect(toolbutton6, "clicked",
			 G_CALLBACK(new_modlist), NULL);

	g_signal_connect(toolbutton7, "clicked",
			 G_CALLBACK(save_modlist), NULL);

	g_signal_connect(toolbutton8, "clicked",
			 G_CALLBACK(delete_list), NULL);

	g_signal_connect(toolbutton10, "clicked",
			 G_CALLBACK(clear_modules), NULL);

	g_signal_connect(toolbutton11, "clicked",
			 G_CALLBACK(delete_module), NULL);

	g_signal_connect(toolbutton12, "clicked",
			 G_CALLBACK(on_toolbutton12_clicked), NULL);

	g_signal_connect(toolbutton13, "clicked",
			 G_CALLBACK(button_export), NULL);

	_setup_combobox(GTK_DROP_DOWN(search1.combo_list));
	g_signal_connect(search1.combo_list, "notify::selected",
			 G_CALLBACK(on_comboboxentry2_changed), NULL);

	search1.entry_list_name = UI_GET_ITEM(gxml, "entry4");
	g_signal_connect(search1.entry_list_name, "changed",
			 G_CALLBACK(list_name_changed), NULL);

	search1.combo_range = UI_GET_ITEM(gxml, "comboboxentry1");
	_setup_combobox(GTK_DROP_DOWN(search1.combo_range));

	search1.entry_range_name = UI_GET_ITEM(gxml, "entry2");
	g_signal_connect(search1.entry_range_name, "changed",
			 G_CALLBACK(range_name_changed), NULL);
	search1.entry_range_text = UI_GET_ITEM(gxml, "entry3");
	g_signal_connect(search1.entry_range_text, "changed",
			 G_CALLBACK(range_text_changed), NULL);

	search1.progressbar = UI_GET_ITEM(gxml, "progressbar1");
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(search1.progressbar), TRUE);
	search1.label_mod_select = UI_GET_ITEM(gxml, "label5");
	search1.listview_results = UI_GET_ITEM(gxml, "treeview9");

	/* setup module select dialog */
	search1.mod_sel_dialog = UI_GET_ITEM(gxml, "dialog2");
	g_signal_connect((gpointer)search1.mod_sel_dialog, "response",
			 G_CALLBACK(_on_dialog2_response), NULL);
	search1.mod_sel_dlg_treeview = UI_GET_ITEM(gxml, "treeview8");
	_setup_treeview2(search1.mod_sel_dlg_treeview);
	gtk_widget_hide(search1.mod_sel_dialog);

	_setup_listviews(search1.listview_results,
			 (GCallback)_selection_finds_list_changed);
	search1.listview_verses = UI_GET_ITEM(gxml, "treeview10");
	search1.menu_item_send_search = create_results_menu_advsearch();
	gui_insert_single_action(search1.listview_verses, "busqueda",
				 "enviar-biblesync", NULL,
				 G_CALLBACK(send_list_action), NULL);
	_setup_listviews2(search1.listview_verses,
			  G_CALLBACK(_verselist_activated));
	gui_widget_on_button(GTK_WIDGET(search1.listview_verses), GTK_PHASE_CAPTURE, (GuiButtonFunc)on_treeview_button_press_event_advsearch, NULL, NULL);

	_add_html_widget(GTK_WIDGET(gtk_builder_get_object(gxml, "vbox12")));

	g_signal_connect(search1.dialog, "notify::default-width",
			 G_CALLBACK(on_advsearch_configure_event), NULL);
	g_signal_connect(search1.dialog, "notify::default-height",
			 G_CALLBACK(on_advsearch_configure_event), NULL);

	settings.display_advsearch = 1;
	xml_set_value("Xiphos", "layout", "advsearchopen", "1");

	/* disable match case initially */
	gtk_widget_set_sensitive(search1.cb_case_sensitive, FALSE);

}

/******************************************************************************
 * Name
 *   gui_create_search_dialog
 *
 * Synopsis
 *   #include "gui/search_dialog.h"
 *
 *   void gui_create_search_dialog(void)
 *
 * Description
 *   calls _create_search_dialog() to create the search dialog
 *
 * Return value
 *   void
 */

void gui_create_search_dialog(void)
{
	_create_search_dialog();
}
