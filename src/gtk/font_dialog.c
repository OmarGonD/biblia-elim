/*
 * Xiphos Bible Study Tool
 * font_dialog.c - dialog to set module font
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
#include <ctype.h>

#include "gui/font_dialog.h"
#include "gui/utilities.h"

#include "main/sword.h"
#include "main/settings.h"

#include "gui/debug_glib_null.h"

static GtkWidget *dlg;
static GtkWidget *combo_entry_size;
static GtkWidget *combo_entry_columns;
static GtkWidget *font_button;
static GtkWidget *checkbutton_no_font;
static GtkWidget *label_mod;
static GtkWidget *label_current_font;
static GtkWidget *button_ok;
static gboolean new_font_set = FALSE;
static const char *new_gdk_font;
static MOD_FONT *mf;

/******************************************************************************
 * Name
 *   get_html_font_name
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   gchar *get_html_font_name(gchar *fontname)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static gchar *get_html_font_name(gchar *fontname)
{
	gchar buf[80];
	gint idx, i = 0;

	/* strip trailing size digits from picker-chosen name. */
	idx = strlen(fontname) - 1;
	while ((idx > 0) && isdigit(fontname[idx]))
		fontname[idx--] = '\0';
	if (fontname[idx] = ' ')
		fontname[idx] = '\0';

	return g_strdup(fontname);
}

/******************************************************************************
 * Name
 *   ok_clicked
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   void ok_clicked(GtkButton * button, MOD_FONT * mf)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void ok_clicked(GtkButton *button, gpointer data)
{
	static gchar *file = NULL;
	gchar *new_font = NULL;
	gchar *font_name = NULL;

	XI_message(("ok_clicked: %s",
		    (new_gdk_font ? new_gdk_font : "-none-")));
	if (file == NULL)
		file =
		    g_strdup_printf("%s/fonts.conf", settings.gSwordDir);

	if (!mf->no_font) {
		if (new_font_set) {
			new_font = g_strdup(new_gdk_font);
			font_name = get_html_font_name(new_font);
			mf->new_font = font_name;
		} else {
			new_font = g_strdup(mf->old_font);
			font_name = g_strdup(mf->old_font);
			mf->new_font = font_name;
		}
	} else {
		mf->new_font = "none";
		new_gdk_font = "none";
	}

	mf->new_font_size = gtk_editable_get_text(GTK_EDITABLE(combo_entry_size));

	mf->columns = gtk_editable_get_text(GTK_EDITABLE(combo_entry_columns));

	save_conf_file_item(file, mf->mod_name, "Font", mf->new_font);
	save_conf_file_item(file, mf->mod_name, "Fontsize", mf->new_font_size);
	save_conf_file_item(file, mf->mod_name, "Columns",
			    (strcmp(mf->columns, "default") ? mf->columns : "-1"));

	//evidently new_gdk_font is never set on Windows
	XI_message(("\nFont: %s\nFontsize: %s\n",
		    (mf->new_font ? mf->new_font : "-none-"),
		    (mf->new_font_size ? mf->new_font_size : "-none-")));

	gui_widget_destroy(dlg);
	if (font_name)
		g_free(font_name);
	if (new_font)
		g_free(new_font);
}

/******************************************************************************
 * Name
 *   cancel_clicked
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   oid cancel_clicked(GtkButton * button, MOD_FONT * mf)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void cancel_clicked(GtkButton *button, gpointer data)
{
	gui_widget_destroy(dlg);
}

/******************************************************************************
 * Name
 *   dialog_de"gui/stroy
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   void dialog_destroy(GtkObject * object, MOD_FONT * mf)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

/* the dialog waits in this loop until it is closed */
static GMainLoop *font_loop;

static void dialog_destroy(GObject *object, gpointer data)
{
	free_font(mf);
	mf = NULL;
	new_font_set = 0;
	if (font_loop && g_main_loop_is_running(font_loop))
		g_main_loop_quit(font_loop);
}

/******************************************************************************
 * Name
 *   font_set
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   void font_set(GtkFontButton * button,
 *		     gchar * arg1, MOD_FONT * mf)
 *
 * Description
 *   the gnomefontpicker gave us a gdkfont
 *   the gtkfontbutton gives up a fontname plus size 'Sans 14'
 *   get_html_font_name () deals with that now.
 *   new_gdk_font is now in the form 'Sans 14'
 *
 * Return value
 *   void
 */

static void font_set(GtkFontButton *button, gchar *arg1, gpointer data)
{
	new_gdk_font = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(button));
	XI_message(("%s", new_gdk_font));
	new_font_set = 1;
	gui_toggle_set_active(GTK_WIDGET(checkbutton_no_font), FALSE);
	gtk_widget_set_sensitive(button_ok, TRUE);
}

/******************************************************************************
 * Name
 *   no_font_toggled
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   void no_font_toggled(GtkToggleButton * togglebutton,
 *			    MOD_FONT * mf)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void no_font_toggled(GtkToggleButton *togglebutton, gpointer data)
{
	mf->no_font = gui_toggle_get_active(togglebutton);
	gtk_widget_set_sensitive(button_ok, TRUE);
}

/******************************************************************************
 * Name
 *   size_changed
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   void size_changed(GtkEditable * editable, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void size_changed(GtkEditable *editable, gpointer user_data)
{
	gtk_widget_set_sensitive(button_ok, TRUE);
}

/******************************************************************************
 * Name
 *   create_dialog_mod_font
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   GtkWidget *create_dialog_mod_font(MOD_FONT * mf)
 *
 * Description
 *
 *
 * Return value
 *   GtkWidget *
 */

static GtkWidget *create_dialog_mod_font()
{
	GtkWidget *dialog_mod_font;
	GtkWidget *dialog_vbox21;
	GtkWidget *hbox_picker;
	GtkWidget *combo_size;
	GtkWidget *combo_count;
	GtkWidget *vbox56;
	GtkWidget *hbox67;
	GtkWidget *pixmap6;
	GtkWidget *vbox57;
	GtkWidget *label206;
	GtkWidget *labelcolumns;
	GtkWidget *dialog_action_area21;
	GtkWidget *hbuttonbox1;
	GtkWidget *hboxcolumns;
	GtkWidget *button_cancel;

	dialog_mod_font = gtk_dialog_new();
	g_object_set_data(G_OBJECT(dialog_mod_font),
			  "dialog_mod_font", dialog_mod_font);
	gtk_window_set_title(GTK_WINDOW(dialog_mod_font),
			     _("Set Module Font"));
	//GTK_WINDOW(dialog_mod_font)->type = GTK_WINDOW_TOPLEVEL;  //FIXME:
	gtk_window_set_default_size(GTK_WINDOW(dialog_mod_font), 301, 164);
	gtk_window_set_resizable(GTK_WINDOW(dialog_mod_font), FALSE);

	dialog_vbox21 =
	    gtk_dialog_get_content_area(GTK_DIALOG(dialog_mod_font));
	g_object_set_data(G_OBJECT(dialog_mod_font), "dialog_vbox21",
			  dialog_vbox21);
	gtk_widget_show(dialog_vbox21);

	UI_VBOX(vbox56, FALSE, 0);
	gtk_widget_show(vbox56);
	gui_box_pack(GTK_BOX(dialog_vbox21), vbox56, TRUE, TRUE, 0);

	UI_HBOX(hbox67, FALSE, 0);
	gtk_widget_show(hbox67);
	gui_box_pack(GTK_BOX(vbox56), hbox67, TRUE, TRUE, 0);

	pixmap6 =
	    gtk_image_new_from_icon_name("gtk-select-font");
	gtk_widget_show(pixmap6);
	gui_box_pack(GTK_BOX(hbox67), pixmap6, TRUE, TRUE, 0);

	UI_VBOX(vbox57, FALSE, 0);
	gtk_widget_show(vbox57);
	gui_box_pack(GTK_BOX(hbox67), vbox57, TRUE, TRUE, 0);

	label206 = gtk_label_new(_("Change font for"));
	gtk_widget_show(label206);
	gtk_box_append(GTK_BOX(vbox57), label206);

	label_mod = gtk_label_new(_("Module"));
	gtk_widget_show(label_mod);
	gtk_box_append(GTK_BOX(vbox57), label_mod);

	label_current_font = gtk_label_new(_("Current font: "));
	gtk_widget_show(label_current_font);
	gtk_box_append(GTK_BOX(vbox57), label_current_font);

	UI_HBOX(hbox_picker, FALSE, 6);
	gtk_widget_show(hbox_picker);
	gtk_box_append(GTK_BOX(vbox56), hbox_picker);

	font_button = gtk_font_button_new();
	gtk_widget_show(font_button);
	gtk_box_append(GTK_BOX(hbox_picker), font_button);
	gtk_widget_set_size_request(font_button, 240, -1);
	gtk_font_chooser_set_level(GTK_FONT_CHOOSER(font_button),
				   GTK_FONT_CHOOSER_LEVEL_FAMILY |
				       GTK_FONT_CHOOSER_LEVEL_STYLE);
	combo_size = gtk_combo_box_text_new_with_entry();
	gtk_widget_show(combo_size);
	gui_box_pack(GTK_BOX(hbox_picker), combo_size, TRUE, TRUE, 0);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "+5");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "+4");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "+3");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "+2");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "+1");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "+0");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "-1");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "-2");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_size), "-3");
	combo_entry_size = gtk_combo_box_get_child(GTK_COMBO_BOX(combo_size));
	gtk_editable_set_text(GTK_EDITABLE(gtk_combo_box_get_child(GTK_COMBO_BOX(combo_size))), _("+0"));
	UI_HBOX(hboxcolumns, FALSE, 0);
	gtk_widget_show(hboxcolumns);
	gtk_box_append(GTK_BOX(vbox56), hboxcolumns);

	labelcolumns = gtk_label_new(_("Display columns"));
	gtk_widget_show(labelcolumns);
	gtk_box_append(GTK_BOX(hboxcolumns), labelcolumns);

	combo_count = gtk_combo_box_text_new_with_entry();
	gtk_widget_set_size_request(combo_count, 240, -1);
	gtk_widget_show(combo_count);
	gui_box_pack(GTK_BOX(hboxcolumns), combo_count, FALSE, TRUE, 150);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), "1");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), "2");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), "3");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), "4");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), "5");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), "6");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), "7");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), "8");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_count), _("default"));
	combo_entry_columns = gtk_combo_box_get_child(GTK_COMBO_BOX(combo_count));
	gtk_editable_set_text(GTK_EDITABLE(gtk_combo_box_get_child(GTK_COMBO_BOX(combo_count))), "");

	checkbutton_no_font =
	    gtk_check_button_new_with_label(_("Use the default font for this module"));
	gtk_widget_show(checkbutton_no_font);
	gui_toggle_set_active(GTK_WIDGET(checkbutton_no_font), mf->no_font);
	gtk_box_append(GTK_BOX(vbox56), checkbutton_no_font);

	dialog_action_area21 =
	    gtk_dialog_get_content_area(GTK_DIALOG(dialog_mod_font));
	g_object_set_data(G_OBJECT(dialog_mod_font),
			  "dialog_action_area21", dialog_action_area21);
	gtk_widget_show(dialog_action_area21);
	gui_widget_set_margins(dialog_action_area21, 10);

	hbuttonbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show(hbuttonbox1);
	gui_box_pack(GTK_BOX(dialog_action_area21), hbuttonbox1, TRUE, TRUE, 0);
	gtk_widget_set_halign(hbuttonbox1, GTK_ALIGN_FILL);

	button_cancel =
	    gtk_button_new_with_label(_("Cancel"));
	gtk_widget_show(button_cancel);
	gtk_box_append(GTK_BOX(hbuttonbox1), button_cancel);
	button_ok =
	    gtk_button_new_with_label(_("OK"));

	gtk_widget_show(button_ok);
	gtk_box_append(GTK_BOX(hbuttonbox1), button_ok);

	g_signal_connect(G_OBJECT(dialog_mod_font), "destroy",
			 G_CALLBACK(dialog_destroy), mf);
	g_signal_connect(G_OBJECT(font_button), "font_set",
			 G_CALLBACK(font_set), mf);
	g_signal_connect(G_OBJECT(checkbutton_no_font), "toggled",
			 G_CALLBACK(no_font_toggled), mf);
	g_signal_connect(G_OBJECT(button_ok), "clicked",
			 G_CALLBACK(ok_clicked), mf);
	g_signal_connect(G_OBJECT(button_cancel), "clicked",
			 G_CALLBACK(cancel_clicked), mf);
	g_signal_connect(G_OBJECT(combo_size), "changed",
			 G_CALLBACK(size_changed), NULL);
	g_signal_connect(G_OBJECT(combo_count), "changed",
			 G_CALLBACK(size_changed), NULL); /* dual use */

	return dialog_mod_font;
}

/******************************************************************************
 * Name
 *   gui_set_module_font
 *
 * Synopsis
 *   #include "gui/font_dialog.h"
 *
 *   void gui_set_module_font(gchar * mod_name)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_set_module_font(gchar *mod_name)
{
	mf = get_font(mod_name);

	if (!strncmp(mf->old_font, "none", 4))
		mf->no_font = 1;

	dlg = create_dialog_mod_font();
	gtk_widget_show(dlg);

	gtk_label_set_text(GTK_LABEL(label_mod), mf->mod_name);
	gtk_label_set_text(GTK_LABEL(label_current_font), mf->old_font);
	if (mf->old_font) {
		gchar *str = g_strdup_printf("%s 12", mf->old_font);
		gtk_font_chooser_set_font(GTK_FONT_CHOOSER(font_button), str);
		g_free(str);
	}
	if (mf->old_font_size) {
		gtk_editable_set_text(GTK_EDITABLE(combo_entry_size), mf->old_font_size);
	}
	gtk_widget_set_sensitive(button_ok, FALSE);
	/* this is here so the program will wait on the font information
	   so it can display the module with the new font - if there is a
	   better way please fix it :) */
	font_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(font_loop);
	g_clear_pointer(&font_loop, g_main_loop_unref);
}
