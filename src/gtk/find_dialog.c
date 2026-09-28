/*
 * Xiphos Bible Study Tool
 * find_dialog.c
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

#include "gui/find_dialog.h"
#include "gui/utilities.h"
#include "main/settings.h"
#include "main/sword.h"
#include "xiphos_html/xiphos_html.h"

typedef struct _find_dialog FIND_DIALOG;

struct _find_dialog
{
	GtkWidget *dialog;
	GtkWidget *htmlwidget;
	GtkWidget *entry;
	GtkWidget *find;
	GtkWidget *next;
	GtkWidget *close;
	GtkWidget *regex;
	GtkWidget *backward;
	GtkWidget *case_sensitive;
	gboolean regular;
};

static FIND_DIALOG *dialog;

/******************************************************************************
 * Name
 *   dialog_destroy
 *
 * Synopsis
 *   #include ".h"
 *
 *   void dialog_destroy(GtkObject *object, gpointer data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void dialog_destroy(GObject *object, gpointer data)
{
	g_free(dialog);
	dialog = NULL;
	XI_print(("\nall done\n"));
}

/******************************************************************************
 * Name
 *  find_clicked
 *
 * Synopsis
 *   #include ".h"
 *
 *   void find_clicked(GtkButton * button, FIND_DIALOG * d)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void find_clicked(GtkButton *button, FIND_DIALOG *d)
{
	gchar *text = (gchar *)gtk_editable_get_text(GTK_EDITABLE(d->entry));
	sprintf(settings.findText, "%s", text);

	XIPHOS_HTML_FIND((void *)d->htmlwidget, text);
}

/******************************************************************************
 * Name
 *  next_clicked
 *
 * Synopsis
 *   #include ".h"
 *
 *   void next_clicked(GtkButton * button, FIND_DIALOG * d)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void next_clicked(GtkButton *button, FIND_DIALOG *d)
{
	XIPHOS_HTML_FIND_AGAIN((void *)d->htmlwidget, 1);
	gui_toggle_set_active(GTK_WIDGET(d->backward), 0);
}

/******************************************************************************
 * Name
 *  close_clicked
 *
 * Synopsis
 *   #include ".h"
 *
 *   void close_clicked(GtkButton * button, FIND_DIALOG * d)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void close_clicked(GtkButton *button, FIND_DIALOG *d)
{
	gui_widget_destroy(d->dialog);
}

/******************************************************************************
 * Name
 *  create_find_dialog
 *
 * Synopsis
 *   #include ".h"
 *
 *   void create_find_dialog(GtkWidget * htmlwidget)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void create_find_dialog(GtkWidget *htmlwidget)
{
	GtkWidget *dialog_vbox29;
	GtkWidget *vbox45;
	GtkWidget *label180;
	GtkWidget *hbox66;
	GtkWidget *dialog_action_area29;
	GtkWidget *hbuttonbox8;
	GtkWidget *image;


	dialog = g_new(FIND_DIALOG, 1); /* must be freed */
	dialog->regular = FALSE;
	dialog->htmlwidget = htmlwidget;

	dialog->dialog = gtk_dialog_new();
	g_object_set_data(G_OBJECT(dialog->dialog),
			  "dialog->dialog", dialog->dialog);
	gtk_window_set_title(GTK_WINDOW(dialog->dialog), _("Find"));
	/*gtk_window_set_policy(GTK_WINDOW(dialog->dialog), TRUE, TRUE,
	   FALSE); */
	gtk_window_set_resizable(GTK_WINDOW(dialog->dialog), FALSE);
	dialog_vbox29 =
	    gtk_dialog_get_content_area(GTK_DIALOG(dialog->dialog));
	gui_widget_set_margins(dialog_vbox29, 6);
	g_object_set_data(G_OBJECT(dialog->dialog), "dialog_vbox29",
			  dialog_vbox29);
	gtk_widget_show(dialog_vbox29);

	UI_VBOX(vbox45, FALSE, 12);
	gtk_widget_show(vbox45);
	gui_box_pack(GTK_BOX(dialog_vbox29), vbox45, TRUE, TRUE, 0);

	label180 = gtk_label_new(_("Enter Word or Phrase"));
	gtk_widget_show(label180);
	gtk_box_append(GTK_BOX(vbox45), label180);

	dialog->entry = gtk_entry_new();
	gtk_widget_show(dialog->entry);
	gtk_box_append(GTK_BOX(vbox45), dialog->entry);
	gtk_widget_set_size_request(dialog->entry, 291, -1);

	UI_HBOX(hbox66, FALSE, 0);
	gtk_widget_show(hbox66);
	gui_box_pack(GTK_BOX(vbox45), hbox66, TRUE, TRUE, 0);

	dialog->backward =
	    gtk_check_button_new_with_label(_("Search backwards"));
	gtk_widget_show(dialog->backward);
	gtk_box_append(GTK_BOX(hbox66), dialog->backward);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(dialog->backward), FALSE);

	dialog_action_area29 =
	    gtk_dialog_get_content_area(GTK_DIALOG(dialog->dialog));
	g_object_set_data(G_OBJECT(dialog->dialog),
			  "dialog_action_area29", dialog_action_area29);
	gtk_widget_show(dialog_action_area29);
	gui_widget_set_margins(dialog_action_area29, 10);

	hbuttonbox8 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show(hbuttonbox8);
	gui_box_pack(GTK_BOX(dialog_action_area29), hbuttonbox8, TRUE, TRUE, 0);

	dialog->find =
	    gtk_button_new_from_icon_name("edit-find-symbolic");
	gtk_button_set_label(GTK_BUTTON(dialog->find), _("Find"));
	gtk_widget_show(dialog->find);
	gtk_box_append(GTK_BOX(hbuttonbox8), dialog->find);

	dialog->next = gtk_button_new_from_icon_name("edit-find-symbolic");
	gtk_button_set_label(GTK_BUTTON(dialog->next), _("Find Next"));
	gtk_widget_show(dialog->next);
	gtk_box_append(GTK_BOX(hbuttonbox8), dialog->next);

	dialog->close =
	    // Don't use an icon with GTK3
	    gtk_button_new_with_label(_("Close"));
	gtk_widget_show(dialog->close);
	gtk_box_append(GTK_BOX(hbuttonbox8), dialog->close);

	g_signal_connect(G_OBJECT(dialog->dialog), "destroy",
			 G_CALLBACK(dialog_destroy), dialog);
	g_signal_connect(G_OBJECT(dialog->find), "clicked",
			 G_CALLBACK(find_clicked), dialog);
	g_signal_connect(G_OBJECT(dialog->next), "clicked",
			 G_CALLBACK(next_clicked), dialog);
	g_signal_connect(G_OBJECT(dialog->close), "clicked",
			 G_CALLBACK(close_clicked), dialog);
}

/******************************************************************************
 * Name
 *  find_dialog
 *
 * Synopsis
 *   #include ".h"
 *
 *   void find_dialog(GtkWidget * htmlwidget, const gchar * title)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void find_dialog(GtkWidget *htmlwidget, const gchar *title)
{
	if (dialog) {
		dialog->htmlwidget = htmlwidget;
		gtk_widget_show(GTK_WIDGET(dialog->dialog));
		gtk_window_present(GTK_WINDOW(dialog->dialog));
	} else {
		create_find_dialog(htmlwidget);
		gtk_widget_show(GTK_WIDGET(dialog->dialog));
	}
}

/******************************************************************************
 * Name
 *  gui_find_dlg
 *
 * Synopsis
 *   #include ".h"
 *
 *   void gui_find_dlg(GtkWidget * htmlwidget, gchar * mod_name,
 *		  gboolean regular, gchar * text)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_find_dlg(GtkWidget *htmlwidget, gchar *mod_name,
		  gboolean regular, gchar *text)
{
	gchar buf[256];

	sprintf(buf, "%s in %s", _("Find"), mod_name);

	find_dialog(htmlwidget, buf);
}

/*** end of file ***/
