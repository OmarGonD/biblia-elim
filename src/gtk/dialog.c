/*
 * Xiphos Bible Study Tool
 * dialog.c -
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
#include "gui/dialog.h"
#include "gui/widgets.h"
#include "gui/utilities.h"

#include "main/settings.h"
#include "main/sword.h"

#include "gui/debug_glib_null.h"

static GtkWidget *entry1 = NULL;
static GtkWidget *entry2 = NULL;
static GtkWidget *entry3 = NULL;
static GtkWidget *entry4 = NULL;
static GtkWidget *entry5 = NULL;
static GtkWidget *entry6 = NULL;
static gint retval = 3;

GS_DIALOG *standard_info;
GtkWidget *dialog_request;

/******************************************************************************
 * Name
 *   get_entry_text
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   void get_entry_text(GS_DIALOG * info)
 *
 * Description
 *   get dialog entry widget's text
 *
 * Return value
 *   void
 */

static void get_entry_text(GS_DIALOG *info)
{
	if (entry1) {
		if (info->text1)
			g_free(info->text1);
		info->text1 =
		    g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry1)));
	}
	if (entry2) {
		if (info->text2)
			g_free(info->text2);
		info->text2 =
		    g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry2)));
	}
	if (entry3) {
		if (info->text3)
			g_free(info->text3);
		info->text3 =
		    g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry3)));
	}
	if (entry4) {
		if (info->text4)
			g_free(info->text4);
		info->text4 =
		    g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry4)));
	}
	if (entry5) {
		if (info->text5)
			g_free(info->text5);
		info->text5 =
		    g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry5)));
	}
	if (entry6) {
		if (info->text6)
			g_free(info->text6);
		info->text6 =
		    g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry6)));
	}
}

/******************************************************************************
 * Name
 *   on_dialog_response
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   void on_dialog_response(GtkDialog * dialog, gint response_id, GS_DIALOG * info)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void on_dialog_response(GtkDialog *dialog, gint response_id,
			       GS_DIALOG *info)
{
	switch (response_id) {
	case GTK_RESPONSE_OK:
		retval = GS_OK;
		get_entry_text(info);
		break;
	case GTK_RESPONSE_YES:
		retval = GS_YES;
		get_entry_text(info);
		break;
	case GTK_RESPONSE_NO:
		retval = GS_NO;
		break;
	case GTK_RESPONSE_CANCEL:
		retval = GS_CANCEL;
		break;
	}
}

/******************************************************************************
 * Name
 *   on_dialog_enter
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   void on_dialog_enter(void)
 *
 * Description
 *   canned Enter key route to on_dialog_response
 *
 * Return value
 *   void
 */

static void on_dialog_enter(void)
{
	on_dialog_response(GTK_DIALOG(dialog_request), GTK_RESPONSE_OK,
			   standard_info);
	gui_widget_destroy(GTK_WIDGET(dialog_request));
}

static GtkWidget *create_dialog_alert(GS_DIALOG *info)
{
	GtkWidget *dialog_alert;
	GtkWidget *dialog_vbox2;
	GtkWidget *hbox3;
	GtkWidget *vbox2;
	GtkWidget *label7;

	dialog_alert = gtk_dialog_new();
	gui_widget_set_margins(dialog_alert, 5);
	gtk_window_set_title(GTK_WINDOW(dialog_alert), " ");


	dialog_vbox2 =
	    gtk_dialog_get_content_area(GTK_DIALOG(dialog_alert));
	gtk_widget_show(dialog_vbox2);

	UI_HBOX(hbox3, FALSE, 12);
	gtk_widget_show(hbox3);
	gui_box_pack(GTK_BOX(dialog_vbox2), hbox3, TRUE, TRUE, 0);
	gui_widget_set_margins(hbox3, 6);

	if (info->stock_icon) {
		GtkWidget *image5 =
		    gtk_image_new_from_icon_name(info->stock_icon);
		gtk_widget_show(image5);
		gtk_box_append(GTK_BOX(hbox3), image5);
	}

	UI_VBOX(vbox2, FALSE, 6);
	gtk_widget_show(vbox2);
	gui_box_pack(GTK_BOX(hbox3), vbox2, TRUE, TRUE, 0);

	label7 = gtk_label_new(info->label_top);
	gtk_widget_show(label7);
	gtk_box_append(GTK_BOX(vbox2), label7);
	gtk_label_set_use_markup(GTK_LABEL(label7), TRUE);
	gtk_label_set_justify(GTK_LABEL(label7), GTK_JUSTIFY_LEFT);
	gtk_label_set_wrap(GTK_LABEL(label7), TRUE);

	if (info->label2) {
		gtk_window_set_default_size(GTK_WINDOW(dialog_alert), 380,
					    200);
		GtkWidget *scrolledwindow = gtk_scrolled_window_new();
		gtk_widget_show(scrolledwindow);
		gui_box_pack(GTK_BOX(vbox2), scrolledwindow, TRUE, TRUE, 0);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
					       GTK_POLICY_AUTOMATIC,
					       GTK_POLICY_AUTOMATIC);

		GtkWidget *viewport = gtk_viewport_new(NULL, NULL);
		gtk_widget_show(viewport);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow), viewport);

		GtkWidget *label10 = gtk_label_new(info->label2);
		gtk_widget_show(label10);
		gtk_viewport_set_child(GTK_VIEWPORT(viewport), label10);
		gtk_label_set_justify(GTK_LABEL(label10),
				      GTK_JUSTIFY_LEFT);
	}

	if (info->ok)
		gtk_dialog_add_button(GTK_DIALOG(dialog_alert),
				      _("_OK"), GTK_RESPONSE_OK);
	if (info->yes)
		gtk_dialog_add_button(GTK_DIALOG(dialog_alert),
				      _("_Yes"), GTK_RESPONSE_YES);
	if (info->save)
		gtk_dialog_add_button(GTK_DIALOG(dialog_alert),
				      _("_Save"), GTK_RESPONSE_YES);
	if (info->no)
		gtk_dialog_add_button(GTK_DIALOG(dialog_alert),
				      _("_No"), GTK_RESPONSE_NO);
	if (info->cancel)
		gtk_dialog_add_button(GTK_DIALOG(dialog_alert),
				      _("_Cancel"), GTK_RESPONSE_CANCEL);
	if (info->no_save)
		gtk_dialog_add_button(GTK_DIALOG(dialog_alert),
				      _("Close without Saving"),
				      GTK_RESPONSE_NO);

	g_signal_connect((gpointer)dialog_alert, "response",
			 G_CALLBACK(on_dialog_response), info);

	return dialog_alert;
}

/******************************************************************************
 * Name
 *   create_dialog_request
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   GtkWidget *create_dialog_request(GS_DIALOG * info)
 *
 * Description
 *   creates the dialog
 *
 * Return value
 *   GtkWidget *
 */

static GtkWidget *create_dialog_request(GS_DIALOG *info)
{
	GtkWidget *dialog_vbox3 = NULL;
	GtkWidget *hbox4 = NULL;
	GtkWidget *vbox3 = NULL;
	GtkWidget *label8 = NULL;
	GtkWidget *table2 = NULL;
	GtkWidget *label9 = NULL;
	GtkWidget *label10 = NULL;
	GtkWidget *label11 = NULL;
	GtkWidget *label12 = NULL;
	GtkWidget *label13 = NULL;
	GtkWidget *label14 = NULL;
	gint nextrow = 0;

	dialog_request = gtk_dialog_new();
	info->dialog = dialog_request;
	gui_widget_set_margins(dialog_request, 6);
	gtk_window_set_title(GTK_WINDOW(dialog_request),
			     (info->title ? info->title : " "));
	gtk_window_set_modal(GTK_WINDOW(dialog_request), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(dialog_request), FALSE);
	dialog_vbox3 =
	    gtk_dialog_get_content_area(GTK_DIALOG(dialog_request));
	gtk_widget_show(dialog_vbox3);

	UI_HBOX(hbox4, FALSE, 12);
	gtk_widget_show(hbox4);
	gui_box_pack(GTK_BOX(dialog_vbox3), hbox4, TRUE, TRUE, 0);
	gui_widget_set_margins(hbox4, 6);

	if (info->stock_icon) {
		GtkWidget *image6 =
		    gtk_image_new_from_icon_name(info->stock_icon);

		gtk_widget_show(image6);
		gtk_box_append(GTK_BOX(hbox4), image6);
	}
	UI_VBOX(vbox3, FALSE, 4);
	gtk_widget_show(vbox3);
	gui_box_pack(GTK_BOX(hbox4), vbox3, TRUE, TRUE, 0);

	label8 = gtk_label_new(info->label_top);
	gtk_widget_show(label8);
	gtk_box_append(GTK_BOX(vbox3), label8);
	gtk_label_set_use_markup(GTK_LABEL(label8), TRUE);
	gtk_label_set_justify(GTK_LABEL(label8), GTK_JUSTIFY_LEFT);
	gtk_label_set_wrap(GTK_LABEL(label8), TRUE);

	if (info->label1) {
		label9 = gtk_label_new(info->label1);
		gtk_widget_show(label9);
		gtk_label_set_justify(GTK_LABEL(label9), GTK_JUSTIFY_LEFT);

		entry1 = gtk_entry_new();
		gtk_widget_show(entry1);
		if (info->text1)
			gtk_editable_set_text(GTK_EDITABLE(entry1), info->text1);
		g_signal_connect((gpointer)entry1, "activate",
				 G_CALLBACK(on_dialog_enter), info);
	}

	if (info->label2) {
		label10 = gtk_label_new(info->label2);
		gtk_widget_show(label10);
		gtk_label_set_justify(GTK_LABEL(label10),
				      GTK_JUSTIFY_LEFT);

		entry2 = gtk_entry_new();
		gtk_widget_show(entry2);
		if (info->text2)
			gtk_editable_set_text(GTK_EDITABLE(entry2), info->text2);
		g_signal_connect((gpointer)entry2, "activate",
				 G_CALLBACK(on_dialog_enter), info);
	}

	if (info->label3) {
		label11 = gtk_label_new(info->label3);
		gtk_widget_show(label11);
		gtk_label_set_justify(GTK_LABEL(label11),
				      GTK_JUSTIFY_LEFT);

		entry3 = gtk_entry_new();
		gtk_widget_show(entry3);
		if (info->text3)
			gtk_editable_set_text(GTK_EDITABLE(entry3), info->text3);
		g_signal_connect((gpointer)entry3, "activate",
				 G_CALLBACK(on_dialog_enter), info);
	}

	if (info->label4) {
		label12 = gtk_label_new(info->label4);
		gtk_widget_show(label12);
		gtk_label_set_justify(GTK_LABEL(label12),
				      GTK_JUSTIFY_LEFT);

		entry4 = gtk_entry_new();
		gtk_widget_show(entry4);

		if (info->text4)
			gtk_editable_set_text(GTK_EDITABLE(entry4), info->text4);
		g_signal_connect((gpointer)entry4, "activate",
				 G_CALLBACK(on_dialog_enter), info);
	}

	if (info->label5) {
		label13 = gtk_label_new(info->label5);
		gtk_widget_show(label13);
		gtk_label_set_justify(GTK_LABEL(label13),
				      GTK_JUSTIFY_LEFT);

		entry5 = gtk_entry_new();
		gtk_widget_show(entry5);

		if (info->text5)
			gtk_editable_set_text(GTK_EDITABLE(entry5), info->text5);
		g_signal_connect((gpointer)entry5, "activate",
				 G_CALLBACK(on_dialog_enter), info);
	}

	if (info->label6) {
		label14 = gtk_label_new(info->label6);
		gtk_widget_show(label14);
		gtk_label_set_justify(GTK_LABEL(label14),
				      GTK_JUSTIFY_LEFT);

		entry6 = gtk_entry_new();
		gtk_widget_show(entry6);

		if (info->text6)
			gtk_editable_set_text(GTK_EDITABLE(entry6), info->text6);
		g_signal_connect((gpointer)entry6, "activate",
				 G_CALLBACK(on_dialog_enter), info);
	}
	table2 = gtk_grid_new();
	gtk_widget_show(table2);
	gui_box_pack(GTK_BOX(vbox3), table2, TRUE, TRUE, 0);
	gtk_grid_set_row_spacing(GTK_GRID(table2), 3);
	gtk_grid_set_column_spacing(GTK_GRID(table2), 3);

	if (info->label1) {
		gtk_grid_attach(GTK_GRID(table2), label9, 0, 0, 1, 1);

		gtk_grid_attach(GTK_GRID(table2), entry1, 1, 0, 1, 1);
		++nextrow;
	}

	if (info->label2) {
		gtk_grid_insert_row(GTK_GRID(table2), nextrow);

		gtk_grid_attach(GTK_GRID(table2), label10, 0, 1, 1, 1);

		gtk_grid_attach(GTK_GRID(table2), entry2, 1, 1, 1, 1);
		++nextrow;
	}

	if (info->label3) {
		gtk_grid_insert_row(GTK_GRID(table2), nextrow);

		gtk_grid_attach(GTK_GRID(table2), label11, 0, 2, 1, 1);

		gtk_grid_attach(GTK_GRID(table2), entry3, 1, 2, 1, 1);
		++nextrow;
	}

	if (info->label4) {
		gtk_grid_insert_row(GTK_GRID(table2), nextrow);

		gtk_grid_attach(GTK_GRID(table2), label12, 0, 3, 1, 1);

		gtk_grid_attach(GTK_GRID(table2), entry4, 1, 3, 1, 1);
		++nextrow;
	}

	if (info->label5) {
		gtk_grid_insert_row(GTK_GRID(table2), nextrow);

		gtk_grid_attach(GTK_GRID(table2), label13, 0, 4, 1, 1);

		gtk_grid_attach(GTK_GRID(table2), entry5, 1, 4, 1, 1);
		++nextrow;
	}

	if (info->label6) {
		gtk_grid_insert_row(GTK_GRID(table2), nextrow);

		gtk_grid_attach(GTK_GRID(table2), label14, 0, 5, 1, 1);

		gtk_grid_attach(GTK_GRID(table2), entry6, 1, 5, 1, 1);
	}
	if (info->no)
		gtk_dialog_add_button(GTK_DIALOG(dialog_request),
				      _("_No"), GTK_RESPONSE_NO);
	if (info->yes)
		gtk_dialog_add_button(GTK_DIALOG(dialog_request),
				      _("_Yes"), GTK_RESPONSE_YES);
	if (info->cancel)
		gtk_dialog_add_button(GTK_DIALOG(dialog_request),
				      _("_Cancel"), GTK_RESPONSE_CANCEL);
	if (info->ok)
		gtk_dialog_add_button(GTK_DIALOG(dialog_request),
				      _("_OK"), GTK_RESPONSE_OK);
	g_signal_connect((gpointer)dialog_request, "response",
			 G_CALLBACK(on_dialog_response), info);

	return dialog_request;
}

/******************************************************************************
 * Name
 *   gui_new_dialog
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   GS_DIALOG *gui_new_dialog(void)
 *
 * Description
 *
 *
 * Return value
 *   GS_DIALOG *
 */

GS_DIALOG *gui_new_dialog(void)
{
	standard_info = g_new0(GS_DIALOG, 1);

	/* set entrys to null */
	entry1 = NULL;
	entry2 = NULL;
	entry3 = NULL;
	entry4 = NULL;
	entry5 = NULL;
	entry6 = NULL;

	return standard_info;
}

/******************************************************************************
 * Name
 *   gui_generic_warning_modal
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   GS_DIALOG *gui_generic_warning_modal(char *message)
 *
 * Description
 *   Issues a generic warning dialog box, to keep user informed.
 *   This version is "old style," waits for user to hit OK.
 *   For tragic conditions, e.g. startup problems before exit.
 *
 * Return value
 *   void
 */

/* El nombre con el que la aplicación se presenta en sus propios avisos.
 * Se pregunta en vez de escribirlo: gui_init() ya lo puso una vez con
 * g_set_application_name(), y así no hay dos sitios que puedan decir
 * cosas distintas. Antes aquí ponía «Xiphos», que es de donde viene el
 * código pero no lo que el lector tiene instalado. */
static const char *
nombre_aplicacion(void)
{
	const char *nombre = g_get_application_name();

	/* Un aviso muy temprano puede llegar antes de gui_init(). */
	return (nombre && *nombre) ? nombre : "Biblia Elim";
}

void gui_generic_warning_modal(const char *message)
{
	GS_DIALOG *dialog;
	gchar *dialog_text;

	dialog = gui_new_dialog();
	dialog->stock_icon =
	    "dialog-information";

	dialog_text = g_strdup_printf("<span weight=\"bold\">%s:</span>",
				      nombre_aplicacion());
	dialog->label_top = dialog_text;
	dialog->label2 = (char *)message;
	dialog->ok = TRUE;

	gui_alert_dialog(dialog);
	g_free(dialog);
	g_free(dialog_text);
}

/******************************************************************************
 * Name
 *   gui_generic_warning
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   GS_DIALOG *gui_generic_warning(char *message)
 *
 * Description
 *   Issues a generic warning dialog box, to keep user informed.
 *   Non-modal, for transient things not needing user interaction.
 *
 * Return value
 *   void
 */

void gui_generic_warning(const char *message)
{
	GtkWidget *dialog;

	/* settings_init() can warn (missing languages file, etc.) before
	 * gui_init() has called gtk_init. Creating a GtkMessageDialog
	 * then SIGSEGVs inside GTK's style provider. */
	if (gdk_display_get_default() == NULL) {
		g_printerr("%s: %s\n", nombre_aplicacion(),
			   message ? message : "");
		return;
	}

	dialog = gtk_message_dialog_new_with_markup(NULL, /* no need for a parent window */
						    GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_MESSAGE_INFO,
						    GTK_BUTTONS_OK,
						    "<b><big>%s</big></b>\n\n%s",
						    nombre_aplicacion(),
						    message);

	g_signal_connect_swapped(dialog, "response",
				 G_CALLBACK(gui_widget_destroy), dialog);
	gtk_widget_show(dialog);
}

/******************************************************************************
 * Name
 *   gui_gs_dialog
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   gint gui_gs_dialog(GS_DIALOG * info)
 *
 * Description
 *
 *
 * Return value
 *   gint
 */

gint gui_gs_dialog(GS_DIALOG *info)
{
	static gboolean is_running = FALSE;

	if (!is_running) {
		GtkWidget *dialog = create_dialog_request(info);
		retval = 4;
		is_running = TRUE;
		gui_dialog_run((GtkDialog *)dialog);
		is_running = FALSE;
		gui_widget_destroy(dialog);
		return retval;
	}
	return 4;
}

/******************************************************************************
 * Name
 *   gui_alert_dialog
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   gint gui_alert_dialog(GS_DIALOG * info)
 *
 * Description
 *
 *
 * Return value
 *   gint
 */

gint gui_alert_dialog(GS_DIALOG *info)
{
	static gboolean is_running = FALSE;

	if (!is_running) {
		GtkWidget *dialog = create_dialog_alert(info); //gs_dialog_build(info);
		retval = 4;
		is_running = TRUE;
		gui_dialog_run((GtkDialog *)dialog);
		is_running = FALSE;
		gui_widget_destroy(dialog);
		return retval;
	}
	return 4;
}

/******************************************************************************
 * Name
 *   gui_close_confirmation_dialog
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   gint gui_close_confirmation_dialog(GS_DIALOG * info)
 *
 * Description
 *
 *
 * Return value
 *   gint
 */

gint gui_close_confirmation_dialog(GS_DIALOG *info)
{
	static gboolean is_running = FALSE;

	if (!is_running) {
		GtkWidget *dialog = create_dialog_alert(info);
		gtk_dialog_add_button(GTK_DIALOG(dialog),
				      _("Close _without Saving"),
				      GTK_RESPONSE_NO);
		gtk_dialog_add_buttons(GTK_DIALOG(dialog),
				       "_Cancel", GTK_RESPONSE_CANCEL,
				       "document-save", GTK_RESPONSE_YES,
				       NULL);
		retval = 4;
		is_running = TRUE;
		gui_dialog_run((GtkDialog *)dialog);
		is_running = FALSE;
		gui_widget_destroy(dialog);
		return retval;
	}
	return 4;
}

/******************************************************************************
 * Name
 *   gui_yes_no_dialog
 *
 * Synopsis
 *   #include "gui/dialog.h"
 *
 *   gint gui_yes_no_dialog(char *question)
 *
 * Description
 *   ask a simple synchronous yes/no question.
 *
 * Return value
 *   gint
 */

gint gui_yes_no_dialog(char *question, char *icon)
{
	GS_DIALOG *yes_no;
	gint result;

	yes_no = gui_new_dialog();
	yes_no->stock_icon = (icon ? icon : "dialog-question");
	yes_no->label_top = question;
	yes_no->yes = TRUE;
	yes_no->no = TRUE;

	result = gui_alert_dialog(yes_no);
	g_free(yes_no);
	return result == GS_YES;
}
