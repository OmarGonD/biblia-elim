/*
 * Xiphos Bible Study Tool
 * navbar_book_dialog.c - navigation bar for genbook dialog modules
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
#include <glib/gi18n.h>

#include "gui/navbar_book_dialog.h"
#include "gui/utilities.h"

#include "main/module_dialogs.h"
#include "main/navbar_book.h"
#include "main/navbar_book_dialog.h"
#include "main/settings.h"

/******************************************************************************
 * Name
 *   menu_deactivate_callback
 *
 * Synopsis
 *   #include "gui/navbar_book_dialog.h"
 *
 *   void menu_deactivate_callback (GtkWidget *widget, gpointer user_data)
 *
 * Description
 *   return toogle button to normal
 *
 * Return value
 *   void
 */

static void menu_deactivate_callback(GtkWidget *widget,
				     gpointer user_data)
{
	GtkWidget *menu_button;

	menu_button = GTK_WIDGET(user_data);

	gui_toggle_set_active(GTK_WIDGET(menu_button),
				     FALSE);
}

static gboolean select_button_press_callback(GtkWidget *widget,
					     GuiButtonEvent *event,
					     gpointer user_data)
{
	GMenuModel *model;
	GtkWidget *popover;

	if (!settings.havebook)
		return FALSE;
	if ((event->type != GDK_BUTTON_PRESS) || event->button != 1)
		return FALSE;
	model = main_navbar_book_dialog_drop_down_new(user_data, widget);
	gtk_widget_grab_focus(widget);
	gui_toggle_set_active(GTK_WIDGET(widget), TRUE);
	popover = gui_popup_menu_model_at_widget(model, widget);
	g_object_unref(model);
	if (popover)
		g_signal_connect(popover, "closed",
				 G_CALLBACK(menu_deactivate_callback), widget);
	return TRUE;
}

/******************************************************************************
 * Name
 *  on_button_parent_clicked
 *
 * Synopsis
 *   #include "gui/navbar_book_dialog.h"
 *
 *  void on_button_parent_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *   call main_navbar_book_dialog_parent() in main/navbar_book.cc
 *
 * Return value
 *   void
 */

static void on_button_parent_clicked(GtkButton *button, gpointer user_data)
{
	if (!settings.havebook)
		return;
	main_navbar_book_dialog_parent(user_data);
}

/******************************************************************************
 * Name
 *  on_button_child_clicked
 *
 * Synopsis
 *   #include "gui/navbar_book_dialog.h"
 *
 *  void on_button_child_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *   call main_navbar_book_dialog_first_child() in main/navbar_book.cc
 *
 * Return value
 *   void
 */

static void on_button_child_clicked(GtkButton *button, gpointer user_data)
{
	if (!settings.havebook)
		return;
	main_navbar_book_dialog_first_child(user_data);
}

/******************************************************************************
 * Name
 *  on_button_prev_clicked
 *
 * Synopsis
 *   #include "gui/navbar_book_dialog.h"
 *
 *  void on_button_prev_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *   call main_navbar_book_dialog_prev() in main/navbar_book.cc
 *
 * Return value
 *   void
 */

static void on_button_prev_clicked(GtkButton *button, gpointer user_data)
{
	if (!settings.havebook)
		return;
	main_navbar_book_dialog_prev(user_data);
}

/******************************************************************************
 * Name
 *  on_button_next_clicked
 *
 * Synopsis
 *   #include "gui/navbar_book_dialog.h"
 *
 *  void on_button_next_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *   call main_navbar_book_dialog_next(); in main/navbar_book.cc
 *
 * Return value
 *   void
 */

static void on_button_next_clicked(GtkButton *button, gpointer user_data)
{
	if (!settings.havebook)
		return;
	main_navbar_book_dialog_next(user_data);
}

/******************************************************************************
 * Name
 *  gui_navbar_book_dialog_new
 *
 * Synopsis
 *   #include "gui/navbar_book_dialog.h"
 *
 *  GtkWidget *gui_navbar_book_dialog_new(void)
 *
 * Description
 *   create a new gen book navigation toolbar and return it
 *
 * Return value
 *   GtkWidget *
 */

GtkWidget *gui_navbar_book_dialog_new(DIALOG_DATA *d)
{
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *image1;
	GtkWidget *arrow1;
	GtkWidget *image2;

	UI_VBOX(vbox1, FALSE, 0);
	gtk_widget_show(vbox1);

	UI_HBOX(hbox1, FALSE, 0);
	gtk_widget_show(hbox1);
	gtk_box_append(GTK_BOX(vbox1), hbox1);

	d->navbar_book.lookup_entry = gtk_entry_new();
	gtk_widget_show(d->navbar_book.lookup_entry);
	gui_box_pack(GTK_BOX(hbox1), d->navbar_book.lookup_entry, TRUE, TRUE, 0);
	gtk_editable_set_editable(GTK_EDITABLE(d->navbar_book.lookup_entry), FALSE);
	gtk_entry_set_invisible_char(GTK_ENTRY(d->navbar_book.lookup_entry), 9679);

	d->navbar_book.button_list = gtk_toggle_button_new();
	gtk_widget_show(d->navbar_book.button_list);
	gtk_box_append(GTK_BOX(hbox1), d->navbar_book.button_list);

	{
		gchar *arrow_path = image_locator("arrow_down_box.png");
		arrow1 = gtk_image_new_from_file(arrow_path);
		g_free(arrow_path);
	}
	gtk_widget_show(arrow1);
	gtk_button_set_child(GTK_BUTTON(d->navbar_book.button_list), arrow1);

	d->navbar_book.button_left = gtk_button_new();
	gtk_widget_show(d->navbar_book.button_left);
	gtk_box_append(GTK_BOX(hbox1), d->navbar_book.button_left);
	gtk_widget_set_tooltip_text(d->navbar_book.button_left,
				    _("Go outward, to the section containing this one"));

	gtk_button_set_has_frame(GTK_BUTTON(d->navbar_book.button_left), FALSE);
	gtk_widget_set_focus_on_click(GTK_WIDGET(d->navbar_book.button_left), FALSE);

	image1 = gtk_image_new_from_icon_name("go-previous-symbolic");
	gtk_widget_show(image1);
	gtk_button_set_child(GTK_BUTTON(d->navbar_book.button_left), image1);

	d->navbar_book.button_up = gtk_button_new();
	gtk_widget_show(d->navbar_book.button_up);
	gtk_box_append(GTK_BOX(hbox1), d->navbar_book.button_up);
	gtk_widget_set_tooltip_text(d->navbar_book.button_up,
				    _("Go to previous item"));
	gtk_button_set_has_frame(GTK_BUTTON(d->navbar_book.button_up), FALSE);
	gtk_widget_set_focus_on_click(GTK_WIDGET(d->navbar_book.button_up), FALSE);

	image1 = gtk_image_new_from_icon_name("go-up-symbolic");
	gtk_widget_show(image1);
	gtk_button_set_child(GTK_BUTTON(d->navbar_book.button_up), image1);

	d->navbar_book.button_down = gtk_button_new();
	gtk_widget_show(d->navbar_book.button_down);
	gtk_box_append(GTK_BOX(hbox1), d->navbar_book.button_down);
	gtk_widget_set_tooltip_text(d->navbar_book.button_down,
				    _("Go to next item"));
	gtk_button_set_has_frame(GTK_BUTTON(d->navbar_book.button_down), FALSE);
	gtk_widget_set_focus_on_click(GTK_WIDGET(d->navbar_book.button_down), FALSE);
	image2 =
	    gtk_image_new_from_icon_name("go-down-symbolic");
	gtk_widget_show(image2);
	gtk_button_set_child(GTK_BUTTON(d->navbar_book.button_down), image2);

	d->navbar_book.button_right = gtk_button_new();
	gtk_widget_show(d->navbar_book.button_right);
	gtk_box_append(GTK_BOX(hbox1), d->navbar_book.button_right);
	gtk_widget_set_tooltip_text(d->navbar_book.button_right,
				    _("Go inward, to the first subsection"));

	gtk_button_set_has_frame(GTK_BUTTON(d->navbar_book.button_right), FALSE);
	gtk_widget_set_focus_on_click(GTK_WIDGET(d->navbar_book.button_right), FALSE);
	image1 = gtk_image_new_from_icon_name("go-next-symbolic");
	gtk_widget_show(image1);
	gtk_button_set_child(GTK_BUTTON(d->navbar_book.button_right), image1);

	g_signal_connect((gpointer)d->navbar_book.button_up, "clicked",
			 G_CALLBACK(on_button_prev_clicked), d);
	g_signal_connect((gpointer)d->navbar_book.button_down, "clicked",
			 G_CALLBACK(on_button_next_clicked), d);
	g_signal_connect((gpointer)d->navbar_book.button_left, "clicked",
			 G_CALLBACK(on_button_parent_clicked), d);
	g_signal_connect((gpointer)d->navbar_book.button_right, "clicked",
			 G_CALLBACK(on_button_child_clicked), d);
	gui_widget_on_button(GTK_WIDGET(d->navbar_book.button_list), GTK_PHASE_CAPTURE, (GuiButtonFunc)select_button_press_callback, NULL, d);
	return vbox1;
}
