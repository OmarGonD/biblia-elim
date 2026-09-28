/*
 * Xiphos Bible Study Tool
 * dictlex_dialog.c - dialog for displaying a dictlex module
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

#include "gui/dictlex_dialog.h"
#include "gui/main_window.h"
#include "gui/menu_popup.h"
#include "gui/sidebar.h"
#include "gui/utilities.h"

#include "main/module_dialogs.h"
#include "main/sword.h"
#include "main/settings.h"
#include "main/lists.h"

extern gboolean dialog_freed;

/******************************************************************************
 * static - global to this file only
 */
static DIALOG_DATA *cur_dlg;
static gint cell_height;

/******************************************************************************
 * Name
 *   list_selection_changed
 *
 * Synopsis
 *   #include "gui/dictlex.h"
 *
 *   void list_selection_changed(GtkTreeSelection * selection,
 *		      GtkWidget * tree_widget)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void list_selection_changed(GtkTreeSelection *selection,
				   DIALOG_DATA *d)
{
	GtkTreeIter selected;
	gchar *buf = NULL;
	GtkTreeModel *model;

	if (!gtk_tree_selection_get_selected(selection, &model, &selected))
		return;

	gtk_tree_model_get(model, &selected, 0, &buf, -1);
	if (buf) {
		gtk_editable_set_text(GTK_EDITABLE(d->entry), buf);
		g_free(buf);
	}
}

/******************************************************************************
 * Name
 *  html_button_pressed
 *
 * Synopsis
 *   #include "gui/dictionary_dialog.h"
 *
 *   gint html_button_pressed(GtkWidget * html, GuiButtonEvent * event,
 *					VIEW_COMM * vc)
 *
 * Description
 *    mouse button pressed in window - used to set cur_vc to the current
 *    commentary dialog structure
 *
 * Return value
 *   gint
 */
/*
static gint button_press_event(GtkWidget *html,
			       GuiButtonEvent *event,
			       DIALOG_DATA *dlg)
{
	return FALSE;
}
*/

/******************************************************************************
 * Name
 *  dialog_set_focus
 *
 * Synopsis
 *   #include "dictlex_dialog.h"
 *
 *   void dialog_set_focus(GtkWindow * window, GtkWidget * widget,
                                        DL_DATA * dlg)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void dialog_set_focus(GtkWindow *window,
			     GtkWidget *widget, DIALOG_DATA *dlg)
{
}

/******************************************************************************
 * Name
 *   dialog_destroy
 *
 * Synopsis
 *   #include "dictlex_dialog.h"
 *
 *   void dialog_destroy(GObject *object, DL_DATA * dlg)
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
 *  list_button_released
 *
 * Synopsis
 *   #include "gui/dictlex.h"
 *
 *   gint list_button_released(GtkWidget * html, GuiButtonEvent * event,
 *					GSHTMLEditorControlData * d)
 *
 * Description
 *    mouse button released in key list
 *
 * Return value
 *   gint
 */

static gint list_button_released(GtkWidget *html,
				 GuiButtonEvent *event, DIALOG_DATA *d)
{
	switch (event->button) {
	case 1:
		list_selection_changed((GtkTreeSelection *)
				       d->mod_selection,
				       d);
		break;
	case 2:
	case 3:
	default:
		break;
	}

	return FALSE;
}

static void add_columns(GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkRequisition size;


	/* column for fixed toggles */
	renderer = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new_with_attributes("Keys",
							  renderer,
							  "text", 0, NULL);
	gtk_tree_view_column_set_sort_column_id(column, 0);

	gtk_tree_view_append_column(treeview, column);
/* get cell (row) height */
	gtk_cell_renderer_get_preferred_size(renderer,
					     GTK_WIDGET(treeview),
					     NULL, &size);
	cell_height = size.height;
	settings.cell_height = cell_height;
}

/******************************************************************************
 * Name
 *  on_btnSyncDL_clicked
 *
 * Synopsis
 *   #include "gui/dictlex.h"
 *
 *   void on_btnSyncDL_clicked(GtkButton * button, DL_DATA * d)
 *
 * Description
 *    sync current module with current key
 *
 * Return value
 *   void
 */

void on_btnSyncDL_clicked(GtkButton *button, DIALOG_DATA *d)
{
	gchar *key = NULL;

	key = settings.dictkey;
	gtk_editable_set_text(GTK_EDITABLE(d->entry), key);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/dictlex.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void entry_changed(GtkEditable *editable, DIALOG_DATA *d)
{
	gchar *key = NULL;

	if (d->key)
		g_free(d->key);

	key = g_strdup((gchar *)gtk_editable_get_text(GTK_EDITABLE(d->entry)));
	d->key = g_utf8_strup(key, -1);
	g_free(key);
	main_dialogs_dictionary_entry_changed(d);
}

static void
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, DIALOG_DATA *d)
{
	gui_menu_popup(html, cur_dlg->mod_name, cur_dlg);
}

/******************************************************************************
 * Name
 *   gui_create_dictlex_dialog
 *
 * Synopsis
 *   #include "dictlex_dialog.h"
 *
 *   GtkWidget *gui_create_dictlex_dialog(GtkWidget * app)
 *
 * Description
 *    create the interface
 *
 * Return value
 *   GtkWidget *
 */

void gui_create_dictlex_dialog(DIALOG_DATA *dlg)
{
	GtkWidget *hpaned7;
	GtkWidget *vbox;
	GtkWidget *vbox56;
	GtkWidget *hbox_toolbar;
	GtkWidget *tmp_toolbar_icon;
	GtkWidget *btnSyncDL;
	GtkWidget *frameDictHTML;
	GtkWidget *scrolledwindow;
	GtkListStore *model;

	dlg->dialog = gtk_window_new();

	g_object_set_data(G_OBJECT(dlg->dialog), "dlg->dialog",
			  dlg->dialog);
	gtk_window_set_title(GTK_WINDOW(dlg->dialog),
			     main_get_module_description(dlg->mod_name));
	gtk_window_set_default_size(GTK_WINDOW(dlg->dialog), 465, 275);
	gtk_window_set_resizable(GTK_WINDOW(dlg->dialog), TRUE);

	UI_VBOX(vbox, FALSE, 0);
	gtk_widget_show(vbox);
	gtk_window_set_child(GTK_WINDOW(dlg->dialog), vbox);

	dlg->frame = gtk_frame_new(NULL);
	gtk_widget_show(dlg->frame);
	gui_box_pack(GTK_BOX(vbox), dlg->frame, TRUE, TRUE, 0);

	hpaned7 = UI_HPANE();
	gtk_widget_show(hpaned7);
	gtk_frame_set_child(GTK_FRAME(dlg->frame), hpaned7);
	gtk_paned_set_position(GTK_PANED(hpaned7), 150);

	UI_VBOX(vbox56, FALSE, 0);
	gtk_widget_show(vbox56);
	gtk_paned_set_start_child(GTK_PANED(hpaned7), vbox56);
	gtk_paned_set_resize_start_child(GTK_PANED(hpaned7), FALSE);
	gtk_paned_set_shrink_start_child(GTK_PANED(hpaned7), TRUE);

	UI_HBOX(hbox_toolbar, FALSE, 0);
	gtk_widget_show(hbox_toolbar);
	gtk_box_append(GTK_BOX(vbox56), hbox_toolbar);

	btnSyncDL = gtk_button_new();
	gtk_widget_show(btnSyncDL);
	gtk_box_append(GTK_BOX(hbox_toolbar), btnSyncDL);
	gtk_button_set_has_frame(GTK_BUTTON(btnSyncDL), FALSE);

	tmp_toolbar_icon =
	    gtk_image_new_from_icon_name("gtk-refresh");
	gtk_widget_show(tmp_toolbar_icon);
	gtk_button_set_child(GTK_BUTTON(btnSyncDL), tmp_toolbar_icon);

	dlg->entry = gtk_entry_new();
	gtk_widget_show(dlg->entry);
	gui_box_pack(GTK_BOX(hbox_toolbar), dlg->entry, TRUE, TRUE, 0);

	/* create tree model */
	model = gtk_list_store_new(1, G_TYPE_STRING);

	scrolledwindow = gtk_scrolled_window_new();
	gtk_widget_show(scrolledwindow);

	gui_box_pack(GTK_BOX(vbox56), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW((GtkScrolledWindow *) scrolledwindow), TRUE);

	/* create tree view */
	dlg->listview =
	    gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_widget_show(dlg->listview);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow), dlg->listview);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dlg->listview),
					  FALSE);
	add_columns(GTK_TREE_VIEW(dlg->listview));
	dlg->mod_selection = G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(dlg->listview)));

	frameDictHTML = gtk_frame_new(NULL);
	gtk_widget_show(frameDictHTML);
	gtk_paned_set_end_child(GTK_PANED(hpaned7), frameDictHTML);
	gtk_paned_set_resize_end_child(GTK_PANED(hpaned7), TRUE);
	gtk_paned_set_shrink_end_child(GTK_PANED(hpaned7), TRUE);


	dlg->html =
	    GTK_WIDGET(XIPHOS_HTML_NEW((DIALOG_DATA *)dlg, TRUE,
				       DIALOG_DICTIONARY_TYPE));
	gtk_frame_set_child(GTK_FRAME(frameDictHTML), dlg->html);
	gtk_widget_show(dlg->html);
	g_signal_connect((gpointer)dlg->html,
			 "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb), dlg);

	g_signal_connect(G_OBJECT(dlg->dialog), "set_focus",
			 G_CALLBACK(dialog_set_focus), dlg);
	g_signal_connect(G_OBJECT(dlg->dialog), "destroy",
			 G_CALLBACK(dialog_destroy), dlg);
	g_signal_connect(G_OBJECT(btnSyncDL), "clicked",
			 G_CALLBACK(on_btnSyncDL_clicked), dlg);
	g_signal_connect(G_OBJECT(dlg->entry), "changed",
			 G_CALLBACK(entry_changed), (DIALOG_DATA *)dlg);
	gui_widget_on_button(GTK_WIDGET(dlg->listview), GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)list_button_released, dlg);
	cur_dlg = dlg;
}

//******  end of file  ******/
