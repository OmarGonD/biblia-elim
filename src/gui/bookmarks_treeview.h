/*
 * Xiphos Bible Study Tool
 * bookmarks_treeview.h - gui for bookmarks in treeview
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

#ifndef ___BOOKMARKS_TREEVIEW_H_
#define ___BOOKMARKS_TREEVIEW_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <gtk/gtk.h>
#include <libxml/parser.h>

#include "gui/table_helpers.h"

/* The bookmarks are a tree of ElimRow (gui/table_helpers.h): one root row,
 * "Bookmarks", and under it the folders and the bookmarks. A folder is a row
 * with no key; a bookmark has one. The columns of a row: */
enum {
	COL_OPEN_PIXBUF,
	COL_CLOSED_PIXBUF,
	COL_CAPTION,
	COL_KEY,
	COL_MODULE,
	COL_MODULE_DESC,
	COL_DESCRIPTION,
	COL_COLOR,      /* hex color string for tag groups, e.g. "#378ADD"
			 * NULL or "" means no color assigned (plain bookmark folder) */
	COL_DOT,	/* the swatch of that color, made by bookmark_row_set_color() */
	N_COLUMNS
};

typedef struct
{
	GdkPixbuf *pixbuf_opened;
	GdkPixbuf *pixbuf_closed;
	GdkPixbuf *pixbuf_helpdoc;
} BookMarksPixbufs;

struct _bookmark_data
{
	xmlNodePtr parent;
	gchar *caption;
	gchar *key;
	gchar *module;
	gchar *module_desc;
	gchar *description;
	gchar *color;   /* hex color string for folders/tag groups, NULL for leaves */
	gboolean is_leaf;
	GdkPixbuf *opened;
	GdkPixbuf *closed;
};
typedef struct _bookmark_data BOOKMARK_DATA;

extern BookMarksPixbufs *bm_pixbufs;
/* The bookmark tree of the sidebar, and the store of its root rows. */
extern GtkWidget *bookmark_tree;
extern GListStore *bookmark_roots;

void gui_load_removed(const xmlChar *file);
/* Makes LIST_VIEW (a GtkListView) show the bookmark tree: an icon, the
 * swatch of the folder's color and the caption of every row. */
void gui_setup_bookmark_view(GtkWidget *list_view);
/* Adds DATA as a child of PARENT (the root of the tree when NULL). */
ElimRow *gui_add_item_to_tree(ElimRow *parent, BOOKMARK_DATA *data);
/* Sets the color of a row, and its swatch; COLOR NULL or "" for none. */
void bookmark_row_set_color(ElimRow *row, const gchar *color);
/* The text of COLUMN of ROW, NULL when it is empty (a folder has no key). */
gchar *bookmark_row_dup(ElimRow *row, guint column);
/* The row that is picked in the bookmark tree of the sidebar, or NULL. */
ElimRow *bookmark_selected(void);
void gui_verselist_to_bookmarks(GList *verses,
				gint save_as_single);
GtkWidget *gui_create_bookmark_tree(void);
void bookmark_debug_dump_colors(void);
gchar *bookmark_get_tag_color_for_key(const gchar *osiskey);
gchar *bookmark_get_tag_info_for_key(const gchar *versekey_text);
void gui_parse_bookmarks(GtkWidget *tree, const xmlChar *file,
			 ElimRow *parent);
GtkWidget *gui_create_dialog_add_bookmark(gchar *label,
					  gchar *module_name,
					  gchar *key);

#ifdef __cplusplus
}
#endif
#endif
