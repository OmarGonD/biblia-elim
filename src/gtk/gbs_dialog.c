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


/******************************************************************************
 * static - global to this file only
 */
static DIALOG_DATA *cur_dlg;

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
 *   void tree_selection_changed(GObject * selection, GParamSpec * pspec,
 *		      DIALOG_DATA * g)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void tree_selection_changed(GObject *selection, GParamSpec *pspec,
				   DIALOG_DATA *g)
{
	(void)selection;
	(void)pspec;
	main_dialogs_tree_selection_changed(g->tree, TRUE, g);
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
	GListStore *tree_roots;

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

	dlg->tree = gtk_list_view_new(NULL, NULL);
	tree_roots = elim_table_new();
	main_setup_mod_tree_view(dlg->tree, tree_roots);
	g_object_unref(tree_roots);
	gtk_widget_show(dlg->tree);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow_ctree), dlg->tree);


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

	g_signal_connect(elim_table_selection(dlg->tree), "notify::selected-item",
			 G_CALLBACK(tree_selection_changed),
			 (DIALOG_DATA *)dlg);
	dlg->statusbar = gtk_statusbar_new();
	gtk_widget_show(dlg->statusbar);
	gtk_box_append(GTK_BOX(vbox_dialog), dlg->statusbar);

	g_signal_connect(G_OBJECT(dlg->dialog), "destroy",
			 G_CALLBACK(dialog_destroy), (DIALOG_DATA *)dlg);
	cur_dlg = dlg;
}
