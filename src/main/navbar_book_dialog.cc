/*
 * Xiphos Bible Study Tool
 * navbar_book_dialog.cc - glue between all navbar_book_dialog and sword
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
#include <versekey.h>
#include <swmodule.h>

#include "main/module_dialogs.h"
#include "main/navbar_book.h"
#include "main/navbar_book_dialog.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/xml.h"

#include "gui/navbar_book_dialog.h"

#include "backend/sword_main.hh"

DIALOG_DATA *cur_d;

/******************************************************************************
 * Name
 *  check_for_parent
 *
 * Synopsis
 *   #include "main/navbar_book.h"
 *
 *  int check_for_parent(DIALOG_DATA * d)
 *
 * Description
 *   check to see if there is a parent to  the curret item
 *
 * Return value
 *   int
 */

static int check_for_parent(DIALOG_DATA *d)
{
	unsigned long offset_save;
	BackEnd *be = (BackEnd *)d->backend;

	offset_save = d->offset;
	be->set_treekey(d->offset);
	if (be->treekey_parent(d->offset)) {
		d->offset = offset_save;
		be->set_treekey(offset_save);
		return 1;
	}
	return 0;
}

/******************************************************************************
 * Name
 *  check_for_prev_sib
 *
 * Synopsis
 *   #include "main/navbar_book.h"
 *
 *  int check_for_prev_sib(DIALOG_DATA * d)
 *
 * Description
 *   check to see if there is a sibling before the curret one
 *
 * Return value
 *   int
 */

static int check_for_prev_sib(DIALOG_DATA *d)
{
	unsigned long offset_save;
	BackEnd *be = (BackEnd *)d->backend;

	offset_save = d->offset;
	be->set_treekey(d->offset);
	if (be->treekey_prev_sibling(d->offset)) {
		d->offset = offset_save;
		be->set_treekey(offset_save);
		return 1;
	}
	return 0;
}

/******************************************************************************
 * Name
 *  check_for_next_sib
 *
 * Synopsis
 *   #include "main/navbar_book.h"
 *
 *  int check_for_next_sib(DIALOG_DATA * d)
 *
 * Description
 *   check to see if there is another sibling after the current one
 *
 * Return value
 *  int
 */

static int check_for_next_sib(DIALOG_DATA *d)
{
	unsigned long offset_save;
	BackEnd *be = (BackEnd *)d->backend;

	offset_save = d->offset;
	be->set_treekey(d->offset);
	if (be->treekey_next_sibling(d->offset)) {
		d->offset = offset_save;
		be->set_treekey(offset_save);
		return 1;
	}
	return 0;
}

/******************************************************************************
 * Name
 *  main_navbar_book_dialog_parent
 *
 * Synopsis
 *   #include "main/navbar_book.h"
 *
 *  void main_navbar_book_dialog_parent(void)
 *
 * Description
 *   left button clicked - display parent
 *
 * Return value
 *   void
 */

void main_navbar_book_dialog_parent(gpointer data)
{
	DIALOG_DATA *d = (DIALOG_DATA *)data;
	BackEnd *be = (BackEnd *)d->backend;

	be->set_treekey(d->offset);
	if (be->treekey_parent(d->offset)) {
		d->offset = be->get_treekey_offset();
		main_setup_navbar_book_dialog(d);
	}
}

/******************************************************************************
 * Name
 *  main_navbar_book_dialog_first_child
 *
 * Synopsis
 *   #include "main/navbar_book.h"
 *
 *  void main_navbar_book_dialog_first_child(void)
 *
 * Description
 *   right button clicked - display first child
 *
 * Return value
 *   void
 */

void main_navbar_book_dialog_first_child(gpointer data)
{
	DIALOG_DATA *d = (DIALOG_DATA *)data;
	BackEnd *be = (BackEnd *)d->backend;

	be->set_treekey(d->offset);
	if (be->treekey_first_child(d->offset)) {
		d->offset = be->get_treekey_offset();
		main_setup_navbar_book_dialog(d);
	}
}

/******************************************************************************
 * Name
 *  main_navbar_book_dialog_prev
 *
 * Synopsis
 *   #include "main/navbar_book.h"
 *
 *  void main_navbar_book_dialog_prev(void)
 *
 * Description
 *   up button clicked - display previous sibling
 *
 * Return value
 *   void
 */

void main_navbar_book_dialog_prev(gpointer data)
{
	DIALOG_DATA *d = (DIALOG_DATA *)data;
	BackEnd *be = (BackEnd *)d->backend;

	be->set_treekey(d->offset);
	if (be->treekey_prev_sibling(d->offset)) {
		d->offset = be->get_treekey_offset();
		main_setup_navbar_book_dialog(d);
	}
}

/******************************************************************************
 * Name
 *  main_navbar_book_dialog_next
 *
 * Synopsis
 *   #include "main/navbar_book.h"
 *
 *  void main_navbar_book_dialog_next(void)
 *
 * Description
 *   down button clicked - display next sibling
 *
 * Return value
 *   void
 */

void main_navbar_book_dialog_next(gpointer data)
{
	DIALOG_DATA *d = (DIALOG_DATA *)data;
	BackEnd *be = (BackEnd *)d->backend;

	be->set_treekey(d->offset);
	if (be->treekey_next_sibling(d->offset)) {
		d->offset = be->get_treekey_offset();
		main_setup_navbar_book_dialog(d);
	}
}

/******************************************************************************
 * Name
 *  on_menu_select(
 *
 * Synopsis
 *   #include "main/navbar_book_dialog.h"
 *
 *   void on_menu_select(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   user clicked on an item in the drop down list - send the book offset
 *   stored in user_data to main_display_book to show
 *
 * Return value
 *   void
 */

static void on_menu_select(GSimpleAction *action, GVariant *offset,
			   gpointer data)
{
	(void)action;
	(void)data;
	cur_d->offset = g_variant_get_uint64(offset);
	main_setup_navbar_book_dialog(cur_d);
}

/******************************************************************************
 * Name
 *   main_book_dialog_drop_down_new
 *
 * Synopsis
 *   #include "main/navbar_book_dialog.h"
 *
 *   GtkWidget *main_book_dialog_drop_down_new(void)
 *
 * Description
 *   create a menu of all the current items in the current section of current genbook
 *   and return it when the list button is toggled
 *
 * Return value
 *   GtkWidget * (menu)
 */

GMenuModel *main_navbar_book_dialog_drop_down_new(gpointer data,
						 GtkWidget *anchor)
{
	gchar *tmpbuf = NULL;
	GMenu *menu = g_menu_new();
	unsigned long offset;
	DIALOG_DATA *d = (DIALOG_DATA *)data;
	BackEnd *be = (BackEnd *)d->backend;

	cur_d = d;
	be->set_treekey(d->offset);
	offset = d->offset;
	/* take us to the first sibling */
	while (be->treekey_prev_sibling(offset)) {
		offset = be->get_treekey_offset();
	}
	/* one entry per sibling, the first one included */
	do {
		GMenuItem *item;

		tmpbuf = be->treekey_get_local_name(offset);
		item = g_menu_item_new((gchar *)tmpbuf, NULL);
		g_menu_item_set_action_and_target_value(
		    item, "libro.ir", g_variant_new_uint64(offset));
		g_menu_append_item(menu, item);
		g_object_unref(item);
		g_free(tmpbuf);
		if (!be->treekey_next_sibling(offset))
			break;
		offset = be->get_treekey_offset();
	} while (TRUE);

	gui_insert_single_action(anchor, "libro", "ir", G_VARIANT_TYPE_UINT64,
				 G_CALLBACK(on_menu_select), NULL);
	return G_MENU_MODEL(menu);
}

/******************************************************************************
 * Name
 *   main_setup_navbar_book_dialog
 *
 * Synopsis
 *   #include "main/navbar_book_dialog.h"
 *
 *   void main_setup_navbar_book_dialog(gpointer data)
 *
 *
 * Description
 *   set  sensitive state of navbar buttons and set the entry to current book key
 *   data = DIALOG_DATA * dialog
 * Return value
 *   void
 */

void main_setup_navbar_book_dialog(gpointer data)
{
	gchar *tmpbuf = NULL;
	DIALOG_DATA *d = (DIALOG_DATA *)data;
	BackEnd *be = (BackEnd *)d->backend;

	be->set_treekey(d->offset);
	tmpbuf = be->get_key_from_offset(d->offset);
	gtk_editable_set_text(GTK_EDITABLE(d->navbar_book.lookup_entry), tmpbuf);
	gtk_widget_set_tooltip_text(d->navbar_book.lookup_entry, tmpbuf);

	if (check_for_parent(d))
		gtk_widget_set_sensitive(d->navbar_book.button_left, TRUE);
	else
		gtk_widget_set_sensitive(d->navbar_book.button_left, FALSE);

	if (be->treekey_has_children(d->offset)) {
		gtk_widget_set_sensitive(d->navbar_book.button_right, TRUE);
	} else {
		gtk_widget_set_sensitive(d->navbar_book.button_right, FALSE);
	}

	if (check_for_prev_sib(d))
		gtk_widget_set_sensitive(d->navbar_book.button_up, TRUE);
	else
		gtk_widget_set_sensitive(d->navbar_book.button_up, FALSE);

	if (check_for_next_sib(d))
		gtk_widget_set_sensitive(d->navbar_book.button_down, TRUE);
	else
		gtk_widget_set_sensitive(d->navbar_book.button_down, FALSE);

	free(tmpbuf);
	be->set_treekey(d->offset);
	be->display_mod->display();
}
