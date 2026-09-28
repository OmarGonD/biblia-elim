/*
 * Xiphos Bible Study Tool
 * sidebar.h - create and maintain the sidebar
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

#ifndef __SIDEBAR_H_
#define __SIDEBAR_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
typedef struct _sidebar SIDEBAR;
struct _sidebar
{
	GtkWidget *module_list;
	GtkWidget *menu_modules;
	GtkWidget *results_list;
	GtkWidget *notebook;
	GtkWidget *tbtn_view_main;
	GtkWidget *btn_save;
	GtkWidget *htmlshow;
	GtkWidget *html_widget;
	GtkWidget *html_viewer_widget;
	GtkWidget *optionmenu1;
	GMenu *results_menu;
	GSimpleActionGroup *results_actions;
	gchar mod_name[80];
};
extern SIDEBAR sidebar;

typedef struct _search_results RESULTS;
struct _search_results
{
	gchar *module;
	gchar *key;
};

extern GList *list_of_verses;
extern GListStore *model_verselist;
extern gboolean is_search_result;


void gui_set_sidebar_program_start(void);
void gui_sidebar_showhide(void);
GtkWidget *gui_create_sidebar(GtkWidget *paned);
gboolean gui_verselist_button_release_event(GtkWidget *widget,
					    GuiButtonEvent *event,
					    gpointer user_data);
gboolean vpaned_srch_rslt_button_release_event(GtkWidget *widget,
					       GuiButtonEvent *event,
					       gpointer user_data);
void gui_show_previewer_in_sidebar(gint choice);
void gui_sync_module_treeview(gint direction);
GMenuModel *gui_sidebar_results_menu(void);
GActionGroup *gui_sidebar_results_actions(void);
void gui_sidebar_results_menu_set_enabled(gboolean enabled);
GtkWidget *gui_sidebar_results_popup(GtkWidget *relative);
void on_open_in_dialog_activate(gpointer menuitem,
				gpointer user_data);
void on_open_in_tab_activate2(gpointer menuitem,
			      gpointer user_data);
void on_about2_activate(gpointer menuitem,
			gpointer user_data);
void on_toggle_favorite_activate(gpointer menuitem,
				 gpointer user_data);
void on_hide_module_activate(gpointer menuitem,
			     gpointer user_data);
void on_simple_activate(gpointer menuitem,
			gpointer user_data);
void on_subject_activate(gpointer menuitem,
			 gpointer user_data);
void on_monthly_activate(gpointer menuitem,
			 gpointer user_data);
void on_journal_activate(gpointer menuitem,
			 gpointer user_data);
void on_outlined_topic_activate(gpointer menuitem,
				gpointer user_data);
void on_edit_activate(gpointer menuitem, gpointer user_data);
void gui_menu_prayerlist_popup(gpointer menuitem,
			       gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif
