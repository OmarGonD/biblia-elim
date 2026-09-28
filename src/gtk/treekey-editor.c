/*
 * Xiphos Bible Study Tool
 * treekey-editor.c - functions to manage a treeview of keys
 *
 * Copyright (C) 2007-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "gui/widget_helpers.h"
#include "editor/editor.h"

#include "gui/treekey-editor.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/table_helpers.h"

#include "main/sidebar.h"
#include "main/sword_treekey.h"
#include "main/sword.h"

#include "gui/debug_glib_null.h"


typedef struct _item_info INFO;
struct _item_info
{
	gchar *book;
	gchar *local_name; /* tree node name */
	gchar *offset;

	ElimRow *row;
	GListStore *roots;
};

INFO *_get_info(GtkWidget *tree);

enum {
	COL_OPEN_PIXBUF,
	COL_CLOSED_PIXBUF,
	COL_CAPTION,
	COL_MODULE,
	COL_OFFSET,
	N_COLUMNS
};


INFO *_get_info(GtkWidget *tree)
{
	INFO *info = g_new0(INFO, 1);

	info->roots = elim_table_get_store(tree);
	info->row = elim_table_get_selected(tree);
	/* a name, a module and an offset even when nothing is picked */
	info->local_name = g_strdup(info->row ? elim_row_get_string(info->row, COL_CAPTION) : "");
	info->book = g_strdup(info->row ? elim_row_get_string(info->row, COL_MODULE) : "");
	info->offset = g_strdup(info->row ? elim_row_get_string(info->row, COL_OFFSET) : "");
	return info;
}

static void _button_one(EDITOR *e)
{
	INFO *info;

	editor_save_book(e);

	info = _get_info(e->treeview);

	if (atol(info->offset) == 0)
		gtk_widget_set_sensitive(e->html_widget, FALSE);
	else
		gtk_widget_set_sensitive(e->html_widget, TRUE);

	if (e->module)
		g_free(e->module);
	e->module = g_strdup(info->book);

	if (e->key)
		g_free(e->key);
	e->key = g_strdup(info->offset);
	editor_load_book(e);

	g_free(info->book);
	g_free(info->local_name);
	g_free(info->offset);
	g_free(info);
}

static void
on_add_sibling_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	INFO *info;
	EDITOR *e = (EDITOR *)user_data;
	GtkWidget *tree = GTK_WIDGET(e->treeview);
	gint test;
	GS_DIALOG *d;

	info = _get_info(tree);

	d = gui_new_dialog();
	d->stock_icon = "dialog-question";
	d->title = _("Prayer List/Journal Item");
	d->label_top = _("New name");
	d->label1 = _("Name: ");
	d->text1 = g_strdup(info->local_name);
	d->ok = TRUE;
	d->cancel = TRUE;

	test = gui_gs_dialog(d);
	if (test == GS_OK) {
		unsigned long l_offset = main_treekey_append_sibling(info->book,
								     d->text1,
								     info->offset);
		if (l_offset) {
			char *buf = g_strdup_printf("%ld", l_offset);
			if (info->row)
				main_mod_tree_add(info->roots,
						  elim_row_get_parent(info->row),
						  info->row, MOD_TREE_ICON_LEAF,
						  d->text1, info->book, buf);
			if (e->key)
				g_free(e->key);
			e->key = g_strdup(buf);
			editor_load_book(e);
			g_free(buf);
		}
	}
	g_free(info->book);
	g_free(info->local_name);
	g_free(info->offset);
	g_free(info);
	g_free(d->text1);
	g_free(d);
}

static void
on_add_child_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	INFO *info;
	EDITOR *e = (EDITOR *)user_data;
	GtkWidget *tree = GTK_WIDGET(e->treeview);
	gint test;
	GS_DIALOG *d;

	info = _get_info(tree);

	d = gui_new_dialog();
	d->stock_icon = "dialog-question";
	d->title = _("Prayer List/Journal Item");
	d->label_top = _("New name");
	d->label1 = _("Name: ");
	d->text1 = g_strdup(info->local_name);
	d->ok = TRUE;
	d->cancel = TRUE;

	test = gui_gs_dialog(d);
	if (test == GS_OK) {
		unsigned long l_offset = main_treekey_append_child(info->book,
								   d->text1,
								   info->offset);
		if (l_offset) {
			char *buf = NULL;

			buf = g_strdup_printf("%ld", l_offset);
			if (info->row) {
				/* change treenode pixbuf from leaf to branch */
				main_mod_tree_set_icon(info->row, MOD_TREE_ICON_CLOSED);
				main_mod_tree_add(info->roots, info->row, NULL,
						  MOD_TREE_ICON_LEAF, d->text1,
						  info->book, buf);
			}
			if (e->key)
				g_free(e->key);
			e->key = g_strdup(buf);
			editor_load_book(e);
			g_free(buf);
		}
	}

	g_free(info->book);
	g_free(info->local_name);
	g_free(info->offset);
	g_free(info);
	g_free(d->text1);
	g_free(d);
}

static void
on_remove_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	INFO *info;
	EDITOR *editor = (EDITOR *)user_data;
	GtkWidget *tree = GTK_WIDGET(editor->treeview);
	gchar *str;
	gchar *icon_name;

	info = _get_info(tree);
	str = g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s/%s",
			      _("Remove the selected item"),
			      info->book, info->local_name);
	icon_name = g_strdup("dialog-warning");
	if (gui_yes_no_dialog(str, icon_name)) {
		if (info->row)
			elim_tree_remove(info->roots, info->row);
		main_treekey_remove(info->book, info->local_name,
				    info->offset);
	}

	g_free(info->book);
	g_free(info->local_name);
	g_free(info->offset);
	g_free(info);
	g_free(str);
	g_free(icon_name);
}

static void
on_edit_activate2(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	INFO *info;
	EDITOR *editor = (EDITOR *)user_data;
	GtkWidget *tree = GTK_WIDGET(editor->treeview);
	gint test;
	GS_DIALOG *d;

	info = _get_info(tree);

	d = gui_new_dialog();
	d->stock_icon = "dialog-question";
	d->title = _("Prayer List/Journal Item");
	d->label_top = _("New name");
	d->label1 = _("Name: ");
	d->text1 = g_strdup(info->local_name);
	d->ok = TRUE;
	d->cancel = TRUE;

	test = gui_gs_dialog(d);
	if (test == GS_OK) {
		main_treekey_set_local_name(info->book,
					    d->text1, info->offset);
		if (info->row)
			elim_row_set_string(info->row, COL_CAPTION, d->text1);
	}

	g_free(info->book);
	g_free(info->local_name);
	g_free(info->offset);
	g_free(info);
	g_free(d->text1);
	g_free(d);
}

/* GTK4-PORT-101 step 2: the tree's context menu is a GMenu over «arbol»
 * actions installed on the tree view, with the editor as their data. */
static void install_tree_actions(GtkWidget *treeview, EDITOR *editor)
{
	const GActionEntry actions[] = {
		{ "hijo", on_add_child_activate, NULL, NULL, NULL, { 0 } },
		{ "hermano", on_add_sibling_activate, NULL, NULL, NULL, { 0 } },
		{ "quitar", on_remove_activate, NULL, NULL, NULL, { 0 } },
		{ "editar", on_edit_activate2, NULL, NULL, NULL, { 0 } },
	};
	GSimpleActionGroup *group = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(group), actions,
					G_N_ELEMENTS(actions), editor);
	gui_widget_insert_action_group(treeview, "arbol", G_ACTION_GROUP(group));
	g_object_unref(group);
}

static GMenuModel *tree_menu_model(void)
{
	GMenu *menu = g_menu_new();
	g_menu_append(menu, _("Añadir subelemento"), "arbol.hijo");
	g_menu_append(menu, _("Añadir elemento"), "arbol.hermano");
	g_menu_append(menu, _("Quitar"), "arbol.quitar");
	g_menu_append(menu, _("Editar"), "arbol.editar");
	return G_MENU_MODEL(menu);
}

static void popup_tree_menu(GtkWidget *treeview, GuiButtonEvent *event)
{
	GMenuModel *model = tree_menu_model();
	GtkWidget *popover = gtk_popover_menu_new_from_model(model);
	gtk_widget_set_parent(popover, treeview);
	g_object_unref(model);
	/* the event is in the tree view's own coordinates */
	GdkRectangle at = { (int)event->x, (int)event->y, 1, 1 };
	gtk_popover_set_pointing_to(GTK_POPOVER(popover), &at);
	gui_popover_destroy_on_close(popover);
	gtk_popover_popup(GTK_POPOVER(popover));
}

static gboolean on_button_release(GtkWidget *widget,
				  GuiButtonEvent *event, EDITOR *editor)
{
	ElimRow *selected;

	switch (event->button) {
	case 1:
		_button_one(editor);
		break;

	case 2:

		break;

	case 3:
		selected = elim_table_get_selected(widget);
		/* the root of the tree has no menu */
		if (selected && elim_row_get_depth(selected) >= 1)
			popup_tree_menu(widget, event);
		return FALSE;
	}
	return FALSE;
}

GtkWidget *gui_create_editor_tree(EDITOR *editor)
{
	GtkWidget *treeview;
	treeview = gtk_list_view_new(NULL, NULL);
	XI_message(("\ngui_create_editor_tree Mod Name:%s\n",
		    editor->module));
	main_load_book_tree_in_editor(treeview,
				      editor->module);
	install_tree_actions(treeview, editor);

	gui_widget_on_button(GTK_WIDGET(treeview), GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_button_release, editor);
	return treeview;
}
