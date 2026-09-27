/*
 * Xiphos Bible Study Tool
 * bookmarks_menu.h - gui for bookmarks in a menu
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

#ifndef ___BOOKMARKS_MENU_H_
#define ___BOOKMARKS_MENU_H_

#ifdef __cplusplus
extern "C" {
#endif
/* GTK4-PORT-101 step 2: the bookmark popup is a GMenu over the
 * «marcadores» actions; the tree enables and disables them per row. */
struct _bookmark_menu
{
	GSimpleActionGroup *actions;	/* NULL until first use */
};
typedef struct _bookmark_menu BOOKMARK_MENU;
extern BOOKMARK_MENU menu;
extern GtkTreeSelection *current_selection;
extern GtkTreeStore *model;
extern gboolean use_dialog;
extern gboolean bookmarks_changed;

void gui_save_bookmarks(gpointer menuitem,
			gpointer user_data);
void gui_create_bookmark_menu(void);
void gui_save_bookmarks_treeview(void);
void bibletime_bookmarks_activate(gpointer menuitem,
				  gpointer user_data);
/* Enable or disable a «marcadores» action (see gui_create_bookmark_menu). */
void gui_bookmark_menu_enable(const char *action, gboolean enabled);
gboolean gui_bookmark_menu_reordering(void);
/* Shows the popup at the pointer over TREE; returns the popover. */
GtkWidget *gui_bookmark_menu_popup(GtkWidget *tree);
void on_dialog_activate(gpointer menuitem,
			gpointer user_data);
void on_edit_item_activate(gpointer menuitem,
			   gpointer user_data);
void on_export_folder_activate(gpointer menuitem,
			       gpointer user_data);
void on_delete_item_activate(gpointer menuitem,
			     gpointer user_data);
void on_expand_activate(gpointer menuitem,
			gpointer user_data);
void on_collapse_activate(gpointer menuitem,
			  gpointer user_data);
void on_add_bookmark_activate(gpointer menuitem,
			      gpointer user_data);
void on_insert_bookmark_activate(gpointer menuitem,
				 gpointer user_data);
void on_new_folder_activate(gpointer menuitem,
			    gpointer user_data);
#if GTK_CHECK_VERSION(3, 4, 0)
void on_set_tag_color_activate(gpointer menuitem, gpointer user_data);
#endif
void on_open_in_tab_activate(gpointer menuitem,
			     gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif
