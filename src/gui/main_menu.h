/*
 * Xiphos Bible Study Tool
 * main_menu.h - creation of and call backs for xiphos main menu
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

#ifndef __MAIN_MENU__H_
#define __MAIN_MENU__H_

#ifdef __cplusplus
extern "C" {
#endif

GtkWidget *gui_create_main_menu(void);
void gui_main_menu_set_state(const gchar *name, gboolean state);
void gui_main_menu_change_state(const gchar *name, gboolean state);
void gui_main_menu_set_theme(const gchar *mode);
gboolean gui_main_menu_get_state(const gchar *name);
GActionGroup *gui_main_menu_actions(void);
GMenuModel *gui_main_menu_model(void);
void gui_parallel_tab_activate(gpointer menuitem,
			       gpointer user_data);
void on_module_manager_activate(gpointer menuitem,
				gpointer user_data);
void on_preferences_activate(gpointer menuitem,
			     gpointer user_data);
void on_help_contents_activate(gpointer menuitem,
			       gpointer user_data);
void on_report_bug_activate(gpointer menuitem,
			    gpointer user_data);
void on_about_the_sword_project_activate(gpointer menuitem,
					 gpointer user_data);
void on_about_biblesync_activate(gpointer menuitem,
				 gpointer user_data);
void on_about_translation_activate(gpointer menuitem,
				   gpointer user_data);
void on_daily_devotion_activate(gpointer menuitem,
				gpointer user_data);
void on_search_activate(gpointer menuitem,
			gpointer user_data);
void on_linked_tabs_activate(gpointer menuitem,
			     gpointer user_data);
void on_read_aloud_activate(gpointer menuitem,
			    gpointer user_data);
void on_show_verse_numbers_activate(gpointer menuitem,
				    gpointer user_data);
void on_versehighlight_activate(gpointer menuitem,
				gpointer user_data);
void on_side_preview_activate(gpointer menuitem,
			      gpointer user_data);
void on_quit_activate(gpointer menuitem, gpointer user_data);
void on_about_xiphos_activate(gpointer menuitem,
			      gpointer user_data);
void on_save_session_activate(gpointer menuitem,
			      gpointer user_data);
void redisplay_to_realign(void);
void on_open_session_activate(gpointer menuitem,
			      gpointer user_data);
void on_show_bible_text_activate(gpointer menuitem,
				 gpointer user_data);
void on_preview_activate(gpointer menuitem,
			 gpointer user_data);
void on_reading_mode_activate(gpointer menuitem,
			      gpointer user_data);
void on_show_commentary_activate(gpointer menuitem,
					 gpointer user_data);
void on_show_dictionary_lexicon_activate(gpointer menuitem,
						     gpointer user_data);
void on_export_bible_activate(gpointer menuitem,
			      gpointer user_data);
void on_advanced_search_activate(gpointer menuitem,
				 gpointer user_data);
void on_nube_palabras_activate(gpointer menuitem,
			       gpointer user_data);
void on_planes_lectura_activate(gpointer menuitem,
				gpointer user_data);
void on_progreso_lectura_activate(gpointer menuitem,
				  gpointer user_data);
void on_versiculo_dia_activate(gpointer menuitem,
			       gpointer user_data);
void on_memorizacion_activate(gpointer menuitem,
			      gpointer user_data);
void on_pulpito_activate(gpointer menuitem,
			 gpointer user_data);
void on_diccionario_activate(gpointer menuitem,
			     gpointer user_data);
void on_testimonios_activate(gpointer menuitem,
			     gpointer user_data);
void on_buscar_notas_activate(gpointer menuitem,
			      gpointer user_data);
void on_attach_detach_sidebar_activate(gpointer menuitem,
				       gpointer user_data);
void on_sidebar_showhide_activate(gpointer menuitem,
				  gpointer user_data);
void link_uri_hook(GtkLinkButton *button, const gchar *link,
		   gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif
