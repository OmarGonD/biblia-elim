/*
 * Xiphos Bible Study Tool
 * gbs_dialog.c - dialog for displaying a gbs module
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

#include "xiphos_html/xiphos_html.h"

#include "gui/gbs_dialog.h"
#include "gui/navbar_book_dialog.h"
#include "gui/widgets.h"
#include "gui/menu_popup.h"
#include "gui/utilities.h"

#include "main/module_dialogs.h"
#include "main/sidebar.h"
#include "main/sword.h"
#include "main/settings.h"
#include "main/lists.h"

extern gboolean dialog_freed;

enum {
	COL_OPEN_PIXBUF,
	COL_CLOSED_PIXBUF,
	COL_TITLE,
	COL_BOOK,
	COL_OFFSET,
	N_COLUMNS
};

/******************************************************************************
 * static - global to this file only
 */
static DIALOG_DATA *cur_dlg;
static GtkTreeModel *model;

/******************************************************************************
 * Name
 *   dialog_destroy
 *
 * Synopsis
 *   #include "gbs_dialog.h"
 *
 *   void dialog_destroy(GtkObject *object, DL_DIALOG * dlg)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void dialog_destroy(GObject *object, DIALOG_DATA *dlg)
{
	if (!dialog_freed)
		main_free_on_destroy(dlg);
	dialog_freed = FALSE;
}

/******************************************************************************
 * Name
 *   tree_selection_changed
 *
 * Synopsis
 *   #include "gui/gbs.h"
 *
 *   void tree_selection_changed(GtkTreeSelection * selection,
 *		      GtkWidget * tree_widget)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void tree_selection_changed(GtkTreeSelection *selection,
				   DIALOG_DATA *g)
{
	GtkTreeModel *model =
	    gtk_tree_view_get_model(GTK_TREE_VIEW(g->tree));

	main_dialogs_tree_selection_changed(model, selection, TRUE, g);
}

static GtkTreeModel *create_model(void)
{
	GtkTreeStore *model;

	/* create tree store */
	model = gtk_tree_store_new(N_COLUMNS,
				   GDK_TYPE_PIXBUF,
				   GDK_TYPE_PIXBUF,
				   G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_STRING);
	return GTK_TREE_MODEL(model);
}

static void add_columns(GtkTreeView *tree)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	column = gtk_tree_view_column_new();

	/* Only "pixbuf" (never the expander-open/expander-closed pair): GTK4's
	 * deprecated GtkCellRendererPixbuf hands that pair a null GValue for
	 * an expander row even though the model's own column data is valid,
	 * aborting via gdk_texture_new_for_pixbuf's GDK_IS_PIXBUF assertion
	 * (see main_add_mod_tree_columns() in main/sidebar.cc for how this
	 * was diagnosed). A single attribute sidesteps that path. */
	renderer = GTK_CELL_RENDERER(gtk_cell_renderer_pixbuf_new());
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "pixbuf", COL_OPEN_PIXBUF, NULL);

	renderer = GTK_CELL_RENDERER(gtk_cell_renderer_text_new());
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", COL_TITLE, NULL);
	gtk_tree_view_append_column(tree, column);

	column = gtk_tree_view_column_new();
	renderer = GTK_CELL_RENDERER(gtk_cell_renderer_text_new());
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", COL_BOOK, NULL);
	gtk_tree_view_append_column(tree, column);
	gtk_tree_view_column_set_visible(column, FALSE);

	column = gtk_tree_view_column_new();
	renderer = GTK_CELL_RENDERER(gtk_cell_renderer_text_new());
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer,
					    "text", COL_OFFSET, NULL);
	gtk_tree_view_append_column(tree, column);
	gtk_tree_view_column_set_visible(column, FALSE);
}

static void
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, DIALOG_DATA *d)
{
	gui_menu_popup(html, cur_dlg->mod_name, cur_dlg);
}

/******************************************************************************
 * Name
 *   create_gbs_dialog
 *
 * Synopsis
 *   #include "gbs_dialog.h"
 *
 *   void create_gbs_dialog(GBS_DATA * dlg)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_create_gbs_dialog(DIALOG_DATA *dlg)
{
	GtkWidget *vbox_dialog;
	GtkWidget *navbar;
	GtkWidget *hpaned;
	GtkWidget *scrolledwindow_ctree;
	GObject *selection;

	dlg->dialog = gtk_window_new();
	g_object_set_data(G_OBJECT(dlg->dialog), "dlg->dialog",
			  dlg->dialog);
	gtk_window_set_title(GTK_WINDOW(dlg->dialog),
			     main_get_module_description(dlg->mod_name));
	gtk_window_set_default_size(GTK_WINDOW(dlg->dialog), 525, 306);
	gtk_window_set_resizable(GTK_WINDOW(dlg->dialog), TRUE);

	UI_VBOX(vbox_dialog, FALSE, 0);
	gtk_widget_show(vbox_dialog);
	gtk_window_set_child(GTK_WINDOW(dlg->dialog), vbox_dialog);

	navbar = gui_navbar_book_dialog_new(dlg);
	gtk_box_append(GTK_BOX(vbox_dialog), navbar);

	hpaned = UI_HPANE();
	gtk_widget_show(hpaned);
	gui_box_pack(GTK_BOX(vbox_dialog), hpaned, TRUE, TRUE, 0);

	scrolledwindow_ctree = gtk_scrolled_window_new();
	gtk_paned_set_start_child(GTK_PANED(hpaned), scrolledwindow_ctree);
	gtk_paned_set_resize_start_child(GTK_PANED(hpaned), FALSE);
	gtk_paned_set_shrink_start_child(GTK_PANED(hpaned), TRUE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_ctree),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW((GtkScrolledWindow *) scrolledwindow_ctree), TRUE);

	model = create_model();
	dlg->tree = gtk_tree_view_new_with_model(model);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dlg->tree), FALSE);
	gtk_widget_show(dlg->tree);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow_ctree), dlg->tree);
	add_columns(GTK_TREE_VIEW(dlg->tree));

	selection =
	    G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(dlg->tree)));


	dlg->html =
	    GTK_WIDGET(XIPHOS_HTML_NEW(((DIALOG_DATA *)dlg), TRUE, DIALOG_BOOK_TYPE));
	gtk_widget_show(dlg->html);
	gtk_paned_set_end_child(GTK_PANED(hpaned), dlg->html);
	gtk_paned_set_resize_end_child(GTK_PANED(hpaned), FALSE);
	gtk_paned_set_shrink_end_child(GTK_PANED(hpaned), TRUE);
	g_signal_connect((gpointer)dlg->html,
			 "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb),
			 (DIALOG_DATA *)dlg);

	g_signal_connect(selection, "changed",
			 G_CALLBACK(tree_selection_changed),
			 (DIALOG_DATA *)dlg);
	dlg->statusbar = gtk_statusbar_new();
	gtk_widget_show(dlg->statusbar);
	gtk_box_append(GTK_BOX(vbox_dialog), dlg->statusbar);

	g_signal_connect(G_OBJECT(dlg->dialog), "destroy",
			 G_CALLBACK(dialog_destroy), (DIALOG_DATA *)dlg);
	cur_dlg = dlg;
}
