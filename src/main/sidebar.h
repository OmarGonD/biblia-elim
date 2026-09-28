/*
 * Xiphos Bible Study Tool
 * sidebar.h - sidebar interface to sword
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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "gui/table_helpers.h"

typedef struct
{
	GdkPixbuf *pixbuf_opened;
	GdkPixbuf *pixbuf_closed;
	GdkPixbuf *pixbuf_helpdoc;
} TreePixbufs;
extern TreePixbufs *pixbufs;

void main_open_bookmark_in_new_tab(gchar *mod_name, gchar *key);
void main_display_verse_list_in_sidebar(gchar *key,
					gchar *module_name,
					gchar *verse_list);
void main_create_pixbufs(void);
void main_load_module_tree(GtkWidget *tree);
void main_init_module_tree(GtkWidget *tree);
void main_load_module_tree_flat(GtkWidget *tree);
void main_load_module_tree_by_language(GtkWidget *tree);
gboolean module_is_favorite(const gchar *name);
gboolean module_is_hidden(const gchar *name);
void module_toggle_favorite(const gchar *name);
void module_toggle_hidden(const gchar *name);
/* The icons of a row of the module tree. */
enum { MOD_TREE_ICON_OPENED, MOD_TREE_ICON_CLOSED, MOD_TREE_ICON_LEAF };
/* A row in the module-tree layout under PARENT of the tree of ROOTS, after
 * the sibling AFTER (last when NULL). */
ElimRow *main_mod_tree_add(GListStore *roots, ElimRow *parent, ElimRow *after,
			   int icon, const gchar *caption, const gchar *module,
			   const gchar *offset);
void main_mod_tree_set(ElimRow *row, int icon, const gchar *caption,
		       const gchar *module, const gchar *offset);
void main_mod_tree_set_icon(ElimRow *row, int icon);
/* Makes TREE (a GtkListView) show the tree of ROOTS the way the module tree
 * of the sidebar is shown (gui/table_helpers.h). */
void main_setup_mod_tree_view(GtkWidget *tree, GListStore *roots);
/* The reader clicked SELECTED, a row of the module tree. */
void main_mod_treeview_button_one(ElimRow *selected);

#ifdef __cplusplus
}
#endif
