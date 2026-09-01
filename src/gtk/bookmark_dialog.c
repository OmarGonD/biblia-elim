/*
 * Xiphos Bible Study Tool
 * bookmark_dialog.c - gui to popup a dialog for adding a bookmark
 *
 * Copyright (C) 2005-2026 Xiphos Developer Team
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

#include "gui/bookmark_dialog.h"
#include "gui/bookmarks_menu.h"
#include "gui/bookmarks_treeview.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/display.hh"
#include "main/sword.h"
#include "main/settings.h"
#include "main/xml.h"

#include "xiphos_html/xiphos_html.h"

#include "gui/debug_glib_null.h"

extern GtkTreeStore *model;

static GtkWidget *treeview;
static GtkWidget *button_new_folder;
static GtkWidget *button_add_bookmark;
static GtkWidget *entry_label;
static GtkWidget *entry_key;
static GtkWidget *entry_module;
static GtkWidget *textview;
static GtkTextBuffer *textbuffer;
static gchar *note;
static GtkWidget *button_toggle_color;
static GtkWidget *colorbutton_highlight;
static gchar *global_module_name = NULL;
GtkWidget *bookmark_dialog;
GtkWidget *mark_verse_dialog;

void on_buffer_changed(GtkTextBuffer *textbuffer, gpointer user_data)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_start_iter(textbuffer, &start);
	gtk_text_buffer_get_end_iter(textbuffer, &end);
	if (note)
		g_free(note);
	note = gtk_text_buffer_get_text(textbuffer, &start, &end, FALSE);
	XI_message(("note: %s", note));
}

static void toggle_color_clicked(GtkButton *btn, GtkWidget *colorbtn)
{
    gboolean active = gtk_widget_is_sensitive(colorbtn);
    gtk_widget_set_sensitive(colorbtn, !active);
    gtk_button_set_label(btn, active ? _("Add color") : _("No color"));
}

/******************************************************************************
 * Name
 *   add_bookmark_button
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 *   void add_bookmark_button(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void add_bookmark_button(void)
{
	GtkTreeIter selected;
	GtkTreeIter iter;
	BOOKMARK_DATA *data;
	GtkTreeSelection *selection;
	const gchar *module_from_entry;
	const gchar *module_to_use;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	if (!gtk_tree_selection_get_selected(selection, NULL, &selected))
		return;

	data = g_new0(BOOKMARK_DATA, 1);
	data->caption = g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entry_label)));

	if (data->caption && strstr(data->caption, "@:@:@")) {
		gui_generic_warning_modal(_("Bookmark labels may not contain \"@:@:@\"."));
		g_free(data->caption);
		g_free(data);
		return;
	}

	data->key = g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entry_key)));

	module_from_entry = gtk_entry_get_text(GTK_ENTRY(entry_module));
	if (module_from_entry && strlen(module_from_entry) > 0) {
		module_to_use = module_from_entry;
	} else if (global_module_name && strlen(global_module_name) > 0) {
		module_to_use = global_module_name;
	} else {
		module_to_use = "";
	}

	data->module = g_strdup(module_to_use);

	if (data->module && strlen(data->module) > 0) {
		if (!strcmp(data->module, "studypad"))
			data->module_desc = g_strdup("studypad");
		else
			data->module_desc = g_strdup(main_get_module_description(data->module));
	} else {
		data->module_desc = g_strdup("");
	}

	data->description = g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entry_label)));
	data->is_leaf = TRUE;
	data->color = NULL;
	data->opened = bm_pixbufs->pixbuf_helpdoc;
	data->closed = NULL;

	gui_add_item_to_tree(&iter, &selected, data);
	bookmarks_changed = TRUE;
	gui_save_bookmarks(NULL, NULL);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

static void add_folder_button(void)
{
	GtkTreeIter selected;
	GtkTreeIter iter;
	BOOKMARK_DATA *data;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	if (!gtk_tree_selection_get_selected(selection, NULL, &selected))
		return;

	GtkBuilder *gxml = elim_gtk_builder_new();
	gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/folder.gtkbuilder", NULL);

	GtkWidget *entry  = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_entry_name"));
	GtkWidget *dialog = GTK_WIDGET(UI_GET_ITEM(gxml, "dialog_folder"));
	GtkWidget *colorbtn = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_color_button"));
	GtkWidget *clearbtn = GTK_WIDGET(UI_GET_ITEM(gxml, "folder_clear_color"));

	if (!dialog || !entry || !colorbtn || !clearbtn) {
		g_object_unref(gxml);
		return;
	}

	gtk_window_set_title(GTK_WINDOW(dialog), _("New Tag"));
	gtk_entry_set_text(GTK_ENTRY(entry), "");

	/* Cette ligne est commune, on la sort du #ifdef */
	gtk_button_set_label(GTK_BUTTON(clearbtn), _("Add color"));
	g_signal_connect(clearbtn, "clicked",
		G_CALLBACK(toggle_color_clicked), colorbtn);
	g_signal_connect(colorbtn, "color-set",
		G_CALLBACK(toggle_color_clicked), clearbtn);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		const gchar *name = gtk_entry_get_text(GTK_ENTRY(entry));
		gchar *color = NULL;

		if (g_strcmp0(gtk_button_get_label(GTK_BUTTON(clearbtn)), _("No color")) == 0) {
#if GTK_CHECK_VERSION(3, 4, 0)
			GdkRGBA rgba;
			gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(colorbtn), &rgba);
			if (rgba.red < 0.99 || rgba.green < 0.99 || rgba.blue < 0.99)
				color = g_strdup_printf("#%02X%02X%02X",
					(guint)(rgba.red   * 255),
					(guint)(rgba.green * 255),
					(guint)(rgba.blue  * 255));
#else
			/* Fallback for GTK3 older than 3.4 (no GtkColorChooser API yet) */
			GdkColor gdk_color;
			gtk_color_button_get_color(GTK_COLOR_BUTTON(colorbtn), &gdk_color);
			if (gdk_color.red < 65000 || gdk_color.green < 65000 || gdk_color.blue < 65000)
				color = g_strdup_printf("#%02X%02X%02X",
					gdk_color.red >> 8,
					gdk_color.green >> 8,
					gdk_color.blue >> 8);
#endif
		}

		data = g_new0(BOOKMARK_DATA, 1);
		data->caption = g_strdelimit(g_strdup(name), "/|><.'`\"", ' ');
		data->color   = color;
		data->is_leaf = FALSE;
		data->opened = bm_pixbufs->pixbuf_opened;
		data->closed = bm_pixbufs->pixbuf_closed;
		gui_add_item_to_tree(&iter, &selected, data);
		bookmarks_changed = TRUE;
		gui_save_bookmarks(NULL, NULL);

		GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
		gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_path_free(path);
	}
	gtk_widget_destroy(dialog);
	g_object_unref(gxml);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

void on_dialog_response(GtkDialog *dialog,
			gint response_id, gpointer user_data)
{
	switch (response_id) {
	case GTK_RESPONSE_CANCEL: /*  cancel button pressed  */
	case GTK_RESPONSE_NONE:   /*  dialog destroyed  */
		gtk_widget_destroy(GTK_WIDGET(dialog));
		break;
	case GTK_RESPONSE_OK: /*  add button pressed  */
		add_bookmark_button();
		gtk_widget_destroy(GTK_WIDGET(dialog));
		break;
	case GTK_RESPONSE_ACCEPT: /*  add folder pressed  */
		add_folder_button();
		break;
	}
}
/******************************************************************************
 * Name
 *   on_dialog_enter
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 * Description
 *  "Enter" key route to on_dialog_response.
 *
 * Return value
 *   void
 */

void on_dialog_enter(void)
{
	if (bookmark_dialog)
		gtk_dialog_response(GTK_DIALOG(bookmark_dialog), GTK_RESPONSE_OK);
}

/******************************************************************************
 * Name
 *   on_mark_verse_response
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 * Description
 *   finish off a verse-markin event.
 *
 * Return value
 *   void
 */

void on_mark_verse_response(GtkDialog *dialog,
			    gint response_id, gpointer user_data)
{
	gchar *module, *key, *osisref;

	module = (gchar *)gtk_entry_get_text(GTK_ENTRY(entry_module));
	key = (gchar *)gtk_entry_get_text(GTK_ENTRY(entry_key));
	osisref = (gchar *)main_get_osisref_from_key((const char *)module,
						     (const char *)key);

	switch (response_id) {
	case GTK_RESPONSE_CANCEL: /*  cancel button pressed  */
	case GTK_RESPONSE_NONE:   /*  dialog destroyed  */
		break;
	case GTK_RESPONSE_ACCEPT: /*  mark the verse  */
		if (gtk_widget_get_sensitive(colorbutton_highlight)) {
			GdkRGBA rgba;
			gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(colorbutton_highlight), &rgba);
			gchar *color = g_strdup_printf("#%02X%02X%02X",
						       (guint)(rgba.red   * 255),
						       (guint)(rgba.green * 255),
						       (guint)(rgba.blue  * 255));
			note_set_whole_verse(module, osisref, note, color);
			g_free(color);
		} else {
			note_set_whole_verse(module, osisref, note, NULL);
		}
		main_display_bible(NULL, settings.currentverse);
		break;
	case GTK_RESPONSE_OK: /*  unmark the verse  */
		note_remove_whole_verse(module, osisref);
		main_display_bible(NULL, settings.currentverse);
		break;
	}
	g_free(note);
	gtk_widget_destroy(GTK_WIDGET(dialog));
	xml_save_settings_doc(settings.fnconfigure);
}

/******************************************************************************
 * Name
 *   on_mark_verse_enter
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 * Description
 *  "Enter" key route to on_mark_verse_response.
 *
 * Return value
 *   void
 */

void on_mark_verse_enter(void)
{
	on_mark_verse_response(GTK_DIALOG(mark_verse_dialog),
			       GTK_RESPONSE_ACCEPT, NULL);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

gboolean on_treeview_button_release_event(GtkWidget *widget,
					  GdkEventButton *event,
					  gpointer user_data)
{
	GtkTreeSelection *selection = NULL;
	GtkTreeModel *gmodel;
	GtkTreeIter selected;
	gchar *key = NULL;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	if (gtk_tree_selection_get_selected(selection, &gmodel, &selected)) {
		gtk_tree_model_get(gmodel, &selected, 3, &key, -1);
		if (!gtk_tree_model_iter_has_child(gmodel, &selected) && key != NULL) {
			gtk_widget_set_sensitive(button_new_folder, FALSE);
			gtk_widget_set_sensitive(button_add_bookmark,
						 FALSE);
		} else {
			gtk_widget_set_sensitive(button_new_folder, TRUE);
			gtk_widget_set_sensitive(button_add_bookmark,
						 TRUE);
		}
		if (key)
			g_free(key);
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

static void setup_treeview(void)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeSelection *selection = NULL;

	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview),
				GTK_TREE_MODEL(model));
	gui_add_columns(GTK_TREE_VIEW(treeview));
	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
	gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_path_free(path);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

static GtkWidget *_create_bookmark_dialog(gchar *label,
					  gchar *module, gchar *key)
{
	GtkBuilder *gxml;

/* build the widget */
	gxml = elim_gtk_builder_new();
	gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/bookmarks.gtkbuilder", NULL);
	g_return_val_if_fail(gxml != NULL, NULL);

	/* lookup the root widget */
	bookmark_dialog = UI_GET_ITEM(gxml, "dialog");

	/* treeview */
	treeview = UI_GET_ITEM(gxml, "treeview");
	setup_treeview();
	g_signal_connect(treeview, "button-release-event",
			 G_CALLBACK(on_treeview_button_release_event),
			 NULL);
	/* entrys */
	entry_label = UI_GET_ITEM(gxml, "entry1");
	entry_key = UI_GET_ITEM(gxml, "entry2");
	entry_module = UI_GET_ITEM(gxml, "entry3");

	gtk_entry_set_text(GTK_ENTRY(entry_label), label);
	gtk_entry_set_text(GTK_ENTRY(entry_key), key);
	gtk_entry_set_text(GTK_ENTRY(entry_module), module);
	g_signal_connect(entry_label, "activate",
			 G_CALLBACK(on_dialog_enter), NULL);
	g_signal_connect(entry_key, "activate",
			 G_CALLBACK(on_dialog_enter), NULL);
	g_signal_connect(entry_module, "activate",
			 G_CALLBACK(on_dialog_enter), NULL);

	/* dialog buttons */
	button_new_folder = UI_GET_ITEM(gxml, "button1");
	button_add_bookmark = UI_GET_ITEM(gxml, "button3");

	return bookmark_dialog;
}

/******************************************************************************
 * Name
 *   _create_mark_verse_dialog
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 * Description
 *   marks/unmarks a verse for display highlight.
 *
 * Return value
 *   void
 */

static GtkWidget *_create_mark_verse_dialog(gchar *module, gchar *key)
{
	GtkBuilder *gxml;
	GtkWidget *sw;
	gchar osisreference[100];
	gchar *old_note = NULL;

	g_snprintf(osisreference, 100, "%s %s", module,
		   main_get_osisref_from_key((const char *)module,
					     (const char *)key));
	note = NULL;

/* build the widget */
	gxml = elim_gtk_builder_new();
	gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/markverse.gtkbuilder", NULL);
	g_return_val_if_fail(gxml != NULL, NULL);

	/* lookup the root widget */
	mark_verse_dialog = UI_GET_ITEM(gxml, "dialog");
	gtk_window_set_default_size(GTK_WINDOW(mark_verse_dialog),
				    300, 350);

	g_signal_connect(mark_verse_dialog, "response",
			 G_CALLBACK(on_mark_verse_response), NULL);

	/* entrys */
	entry_key = UI_GET_ITEM(gxml, "entry2");
	entry_module = UI_GET_ITEM(gxml, "entry3");
	textview = UI_GET_ITEM(gxml, "textview");

	textbuffer = gtk_text_view_get_buffer((GtkTextView *)textview);
	gtk_entry_set_text(GTK_ENTRY(entry_key), key);
	gtk_entry_set_text(GTK_ENTRY(entry_module), module);

	sw = UI_GET_ITEM(gxml, "scrolledwindow1");

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview),
				    GTK_WRAP_WORD);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	g_signal_connect(entry_key, "activate",
			 G_CALLBACK(on_mark_verse_enter), NULL);
	g_signal_connect(entry_module, "activate",
			 G_CALLBACK(on_mark_verse_enter), NULL);

	old_note = highlight_get_verse_note(module,
					    main_get_osisref_from_key((const char *)module,
								      (const char *)key));
	note = g_strdup((old_note) ? old_note : "");
	gtk_text_buffer_set_text(textbuffer, (old_note) ? old_note : "",
				 -1);
	g_signal_connect(textbuffer, "changed",
			 G_CALLBACK(on_buffer_changed), NULL);

	/* per-verse highlight color, same enable/disable idiom as the
	 * bookmark-folder color button in add_folder_button() below. */
	button_toggle_color = UI_GET_ITEM(gxml, "button_toggle_color");
	colorbutton_highlight = UI_GET_ITEM(gxml, "colorbutton_highlight");

	gchar *old_color =
	    note_get_whole_verse_color(module,
				       main_get_osisref_from_key((const char *)module,
								 (const char *)key));
	if (old_color && *old_color) {
		GdkRGBA rgba;
		if (gdk_rgba_parse(&rgba, old_color))
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(colorbutton_highlight), &rgba);
		gtk_widget_set_sensitive(colorbutton_highlight, TRUE);
		gtk_button_set_label(GTK_BUTTON(button_toggle_color), _("Default color"));
	} else {
		gtk_widget_set_sensitive(colorbutton_highlight, FALSE);
		gtk_button_set_label(GTK_BUTTON(button_toggle_color), _("Add color"));
	}
	g_signal_connect(button_toggle_color, "clicked",
			 G_CALLBACK(toggle_color_clicked), colorbutton_highlight);
	g_signal_connect(colorbutton_highlight, "color-set",
			 G_CALLBACK(toggle_color_clicked), button_toggle_color);

	return mark_verse_dialog;
}

/******************************************************************************
 * Name
 *   gui_bookmark_dialog
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *
 *   void gui_bookmark_dialog(gchar * label, gchar * module_name, gchar * key)
 *
 * Description
 *   calls _create_bookmark_dialog() and use gtk_dialog_run()
 *   to make it modal (needed for saving multiple search results)
 *
 * Return value
 *   void
 */

void gui_bookmark_dialog(gchar *label, gchar *module_name, gchar *key)
{
	GtkWidget *dialog;
	gint response;

	if (global_module_name != NULL) {
		g_free(global_module_name);
		global_module_name = NULL;
	}

	if (module_name != NULL) {
		global_module_name = g_strdup(module_name);
	}

	dialog = _create_bookmark_dialog(label, module_name, key);
	if (!dialog)
		return;

	while (TRUE) {
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		if (response == GTK_RESPONSE_ACCEPT) {
			/* New folder — keep dialog open */
			add_folder_button();
		} else if (response == GTK_RESPONSE_OK) {
			/* Add bookmark — close dialog */
			add_bookmark_button();
			break;
		} else {
			/* Cancel or destroy */
			break;
		}
	}
	gtk_widget_destroy(dialog);
}

/******************************************************************************
 * Name
 *   gui_mark_verse_dialog
 *
 * Synopsis
 *   #include "gui/bookmark_dialog.h"
 *   void gui_mark_verse_dialog(gchar * module_name, gchar * key)
 *
 * Description
 *   marks/unmarks a verse for display highlighting.
 *
 * Return value
 *   void
 */

void gui_mark_verse_dialog(gchar *module_name, gchar *key)
{
	GtkWidget *dialog = _create_mark_verse_dialog(module_name, key);
	if (!dialog)
		return;
	gtk_dialog_run(GTK_DIALOG(dialog));
}
