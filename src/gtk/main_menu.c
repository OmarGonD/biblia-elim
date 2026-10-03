/*
 * Xiphos Bible Study Tool
 * main_menu.c - creation of and call backs for xiphos main menu
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
#include <unistd.h>

#include "editor/editor.h"

#include "gui/about_xiphos.h"
#include "gui/gui.h"
#include "gui/about_sword.h"
#include "gui/about_modules.h"
#include "gui/about_trans.h"
#include "gui/xiphos.h"
#include "gui/main_window.h"
#include "gui/main_menu.h"
#include "gui/notas_verso.h"
#include "gui/instalar_biblias.h"
#include "gui/mod_mgr.h"
#include "gui/preferences_dialog.h"
#include "gui/navigation_prefs_dialog.h"
#include "gui/parallel_tab.h"
#include "gui/sidebar.h"
#include "gui/sidebar_dialog.h"
#include "gui/sqlite_module_manager_dialog.h"
#include "gui/search_dialog.h"
#include "gui/nube_palabras.h"
#include "gui/planes_lectura.h"
#include "gui/progreso_lectura.h"
#include "gui/versiculo_dia.h"
#include "gui/memorizacion.h"
#include "gui/testimonios.h"
#include "gui/buscar_notas.h"
#include "gui/pulpito.h"
#include "gui/diccionario.h"
#include "gui/interlineal.h"
#include "gui/lectura_sync.h"
#include "gui/tabbed_browser.h"
#include "gui/utilities.h"

extern void gui_open_sqlite_module_manager(void);
#include "gui/widgets.h"
#include "gui/elim_tema.h"
#include "gui/export_dialog.h"

#include "main/lists.h"
#include "main/sword.h"
#include "main/search_dialog.h"
#include "main/tab_history.h"
#include "gui/atajos.h"
#include "main/url.hh"
#include "main/xml.h"

#include <glib/gstdio.h>

#include "gui/debug_glib_null.h"

//global function to handle gtk_uri calls
/*void
link_uri_hook(GtkLinkButton *button,
	      const gchar *link,
	      gpointer user_data)
{
	xiphos_open_default(link);
}*/

/******************************************************************************
 * Name
 *  on_help_contents_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_help_contents_activate(gpointer menuitem,
 *						gpointer user_data)
 *
 * Description
 *   display the help contents file
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_help_contents_activate(gpointer menuitem, gpointer user_data)
{
#ifdef WIN32
	const char *lang = g_getenv("LANG");
	gchar *help_file =
	    g_win32_get_package_installation_directory_of_module(NULL);
	help_file = g_strconcat(help_file, "\0", NULL);
	if (!strncmp(lang, "fr", 2))
		help_file =
		    g_build_filename(help_file, "share", "help",
				     "xiphos_fr.chm", NULL);
	else if (!strncmp(lang, "fa", 2))
		help_file =
		    g_build_filename(help_file, "share", "help",
				     "xiphos_fa.chm", NULL);
	else if (!strncmp(lang, "it", 2))
		help_file =
		    g_build_filename(help_file, "share", "help",
				     "xiphos_it.chm", NULL);
	else
		help_file =
		    g_build_filename(help_file, "share", "help",
				     "xiphos_C.chm", NULL);
	xiphos_open_default(help_file);
	g_free(help_file);
#else
	xiphos_open_default("help:xiphos");
#endif /* WIN32 */
}

/******************************************************************************
 * Name
 *  on_report_bug_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_report_bug_activate(gpointer menuitem,
 *						gpointer user_data)
 *
 * Description
 *   open web browser to github bug tracker
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_report_bug_activate(gpointer menuitem, gpointer user_data)
{
	xiphos_open_default(PACKAGE_BUGREPORT);
}

/******************************************************************************
 * Name
 *  on_about_the_sword_project_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_about_the_sword_project1_activate(gpointer menuitem,
 *						gpointer user_data)
 *
 * Description
 *   display - The SWORD Project - about information
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_about_the_sword_project_activate(gpointer menuitem,
				    gpointer user_data)
{
	GtkWidget *dlg;

	dlg = gui_create_about_sword();
	gtk_widget_show(dlg);
}

/******************************************************************************
 * Name
 *  on_about_biblesync_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_about_bibelsync_activate(gpointer menuitem,
 *				      gpointer user_data)
 *
 * Description
 *   display - BibleSync - about information
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_about_biblesync_activate(gpointer menuitem,
			    gpointer user_data)
{
	GtkWidget *dlg;

	dlg = gui_create_about_biblesync();
	gtk_widget_show(dlg);
}

/******************************************************************************
 * Name
 *  on_about_translation_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_about_translation_activate(gpointer menuitem,
 *					gpointer user_data)
 *
 * Description
 *   display - The SWORD Project - about information
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_about_translation_activate(gpointer menuitem, gpointer user_data)
{
	GtkWidget *dlg;

	dlg = gui_create_about_trans();
	gtk_widget_show(dlg);
}

/******************************************************************************
 * Name
 *  on_daily_devotion_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_daily_devotion_activate(gpointer menuitem,
 *						gpointer user_data)
 *
 * Description
 *   display daily devotion in shortcut bar viewer
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_daily_devotion_activate(gpointer menuitem, gpointer user_data)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_dict_devot), 1);
	main_display_devotional(widgets.html_devotional);
}

/******************************************************************************
 * Name
 *  on_preferences_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_preferences_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   open preferences dialog
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_preferences_activate(gpointer menuitem, gpointer user_data)
{
	gui_setup_preferences_dialog();
}

/* Ver > Navegación y rueda: wheel distance and verse-following */
G_MODULE_EXPORT void
on_navigation_prefs_activate(gpointer menuitem, gpointer user_data)
{
	(void)menuitem;
	(void)user_data;
	gui_navigation_prefs_dialog_show();
}

G_MODULE_EXPORT void
on_export_bible_activate(gpointer menuitem, gpointer user_data)
{
	(void)menuitem;
	(void)user_data;
	gui_export_book_dialog();
}

/******************************************************************************
 * Name
 *  on_search_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_search_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   display search group in shortcut bar
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void on_search_activate(gpointer menuitem,
					gpointer user_data)
{
	if (!settings.showshortcutbar)
		gui_sidebar_showhide();
	gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_sidebar), 2);
}

/******************************************************************************
 * Name
 *  on_linked_tabs_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_linked_tabs_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   toggle linking tabs together.
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_linked_tabs_activate(gpointer menuitem, gpointer user_data)
{
	settings.linkedtabs = (GPOINTER_TO_INT(user_data) != 0);
	xml_set_value("Xiphos", "misc", "pinnedtabs",
		      (settings.linkedtabs ? "1" : "0"));
	if (settings.showparatab)
		gui_force_parallel_tab_sync();
}

/******************************************************************************
 * Name
 *  on_read_aloud_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_read_aloud_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   toggle reading scripture out loud.
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_read_aloud_activate(gpointer menuitem, gpointer user_data)
{
	settings.readaloud = (GPOINTER_TO_INT(user_data) != 0);
	xml_set_value("Xiphos", "misc", "readaloud",
		      (settings.readaloud ? "1" : "0"));
	if (settings.readaloud)
		main_display_bible(NULL, settings.currentverse);
}

/******************************************************************************
 * Name
 *  on_show_verse_numbers_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_show_verse_numbers_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   toggle showing verse numbers together.
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_show_verse_numbers_activate(gpointer menuitem,
			       gpointer user_data)
{
	settings.showversenum = (GPOINTER_TO_INT(user_data) != 0);
	xml_set_value("Xiphos", "misc", "showversenum",
		      (settings.showversenum ? "1" : "0"));
	main_display_commentary(NULL, settings.currentverse);
	main_display_bible(NULL, settings.currentverse);
}

/******************************************************************************
 * Name
 *  on_versehighlight_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_versehighlight_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   toggle special current verse highlight.
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_versehighlight_activate(gpointer menuitem, gpointer user_data)
{
	settings.versehighlight = (GPOINTER_TO_INT(user_data) != 0);
	xml_set_value("Xiphos", "misc", "versehighlight",
		      (settings.versehighlight ? "1" : "0"));
	main_display_bible(NULL, settings.currentverse);
}

/******************************************************************************
 * Name
 *  on_annotate_highlight_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_annotate_highlight_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   toggle annotated verse highlight.
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_annotate_highlight_activate(gpointer menuitem, gpointer user_data)
{
	settings.annotate_highlight = (GPOINTER_TO_INT(user_data) != 0);
	xml_set_value("Xiphos", "misc", "annotatehighlight",
		      (settings.annotate_highlight ? "1" : "0"));
	main_display_bible(NULL, settings.currentverse);
}

/******************************************************************************
 * Name
 *  gui_parallel_tab_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void gui_parallel_tab_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   open tab for full parallel view
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
gui_parallel_tab_activate(gpointer menuitem, gpointer user_data)
{
	if (!settings.browsing)
		return;
	if (!settings.showparatab && !(GPOINTER_TO_INT(user_data) != 0)) {
		xml_set_value("Xiphos", "misc", "showparatab", "0");
		return;
	}

	settings.showparatab = (GPOINTER_TO_INT(user_data) != 0);
	xml_set_value("Xiphos", "misc", "showparatab",
		      (settings.showparatab ? "1" : "0"));
	if (settings.showparatab) {
		gui_open_parallel_view_in_new_tab();
		gui_force_parallel_tab_sync();
	} else
		gui_close_passage_tab(gtk_notebook_page_num(GTK_NOTEBOOK(widgets.notebook_main),
							    widgets.parallel_tab));
}

/******************************************************************************
 * Name
 *  on_side_preview_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_side_preview_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   toggle special previewer in sidebar.
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_side_preview_activate(gpointer menuitem, gpointer user_data)
{
	settings.show_previewer_in_sidebar =
	    (GPOINTER_TO_INT(user_data) != 0);
	xml_set_value("Xiphos", "misc", "show_side_preview",
		      (settings.show_previewer_in_sidebar ? "1" : "0"));
	gui_show_previewer_in_sidebar(settings.show_previewer_in_sidebar);
}

/******************************************************************************
 * Name
 *  on_quit_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_quit_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   do a nice orderly shut down and exit xiphos
 *   by calling gui_widget_destroy() which will call
 *   on_mainwindow_destroy()
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_quit_activate(gpointer menuitem, gpointer user_data)
{
	/* discover main window internal geometry before we go. */
	final_pane_sizes();

#if defined(WIN32) && defined(HAVE_DBUS)
	/* we started dbus-daemon ourselves, so we must kill it, too. */
	extern GPid dbus_pid;

	g_spawn_close_pid(dbus_pid);
#endif

	/* offer to save all editors remaining open */
	editor_maybe_save_all();
	/* y la nota de versículo que estuviera a medio escribir */
	gui_verse_notes_guardar_pendiente();
	/* y la reflexión del día, por lo mismo */
	gui_versiculo_dia_guardar_pendiente();
	/* y la Biblia que se esté pasando a SQLite, para no dejarla a medias */
	gui_stop_sword_conversion();

	shutdown_frontend();
	/* shutdown the sword stuff */
	main_shutdown_backend();
	gui_main_quit();
	exit(0);
}

/******************************************************************************
 * Name
 *  on_about_xiphos_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_about_xiphos_activate(gpointer menuitem,
 *					gpointer user_data)
 *
 * Description
 *   display xiphos about dialog
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_about_xiphos_activate(gpointer menuitem, gpointer user_data)
{
	GtkWidget *AboutBox;

	AboutBox = gui_create_about_xiphos();
	gtk_widget_show(AboutBox);
}

/******************************************************************************
 * Name
 *  on_save_session_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_save_session_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   ask for a file name (with file-chooser) and save the current tabs to that file
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_save_session_activate(gpointer menuitem, gpointer user_data)
{
	GtkWidget *dialog;
	gchar *tabs_dir;

	tabs_dir = g_strdup_printf("%s/tabs/", settings.gSwordDir);

	if (g_access(tabs_dir, F_OK) == -1) {
		if ((g_mkdir(tabs_dir, S_IRWXU)) == -1) {
			fprintf(stderr, "can't create tabs dir");
			return;
		}
	}

	dialog = gtk_file_chooser_dialog_new(_("Save Session"),
					     NULL,
					     GTK_FILE_CHOOSER_ACTION_SAVE,
					     "_Cancel",
					     GTK_RESPONSE_CANCEL, "_Save",
					     GTK_RESPONSE_ACCEPT,
					     NULL);
	gui_fit_dialog_to_screen(GTK_WINDOW(dialog));

	gui_file_chooser_set_current_folder((GtkFileChooser *)dialog,
					    tabs_dir);
	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;

		filename =
		    gui_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		filename = g_path_get_basename(filename);
		gui_save_tabs(filename);
		g_free(filename);
	}

	gui_widget_destroy(dialog);
}

/******************************************************************************
 * Name
 *  redisplay_to_realign
 *
 * Synopsis
 *   void redisplay_to_realign()
 *
 * Description
 *    when en/disabling panes, we must redisplay in order that text
 *    (especially current verse) not suddenly find itself out of view.
 *
 * Return value
 *   void
 */
void redisplay_to_realign()
{
	static int realign_busy = FALSE;
	int save_comm_show = settings.comm_showing;

	if (realign_busy || change_tabs_no_redisplay)
		return;
	realign_busy = TRUE;

	/* first realize the pane size updates. */
	sync_windows();

	/* then just redisplay everything as-is. */
	gchar *url = g_strdup_printf("sword://%s/%s",
				     settings.MainWindowModule,
				     settings.currentverse);
	main_url_handler(url, TRUE);
	g_free(url);
	if (settings.DictWindowModule && *settings.DictWindowModule /* not empty */
	    && settings.dictkey) {
		url = g_strdup_printf("sword://%s/%s",
				      settings.DictWindowModule,
				      settings.dictkey);
		main_url_handler(url, TRUE);
		g_free(url);
	}
	if (settings.book_mod && *settings.book_mod /* not empty */
	    && settings.book_key) {
		url = g_strdup_printf("sword://%s/%s",
				      settings.book_mod,
				      settings.book_key);
		main_url_handler(url, TRUE);
		g_free(url);
	}

	settings.comm_showing = save_comm_show;
	/* no expulsar al usuario de la pestaña "Notas" (índice 1) por un
	 * realineado de rutina -- ver el mismo guard en
	 * gui_set_bible_comm_layout(), main_window.c. */
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book)) != 1)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book),
					      (settings.comm_showing ? 0 : 1));

	realign_busy = FALSE;
}

/******************************************************************************
 * Name
 *  on_open_session_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_open_session_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *   ask for file name (with file-chooser) and load tabs from that file
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_open_session_activate(gpointer menuitem, gpointer user_data)
{
	GtkWidget *dialog;
	gchar *tabs_dir;

	tabs_dir = g_strdup_printf("%s/tabs/", settings.gSwordDir);

	if (g_access(tabs_dir, F_OK) == -1) {
		if ((g_mkdir(tabs_dir, S_IRWXU)) == -1) {
			fprintf(stderr, "can't create tabs dir");
			return;
		}
	}

	dialog = gtk_file_chooser_dialog_new(_("Open Session"),
					     NULL,
					     GTK_FILE_CHOOSER_ACTION_OPEN,
					     _("_Cancel"),
					     GTK_RESPONSE_CANCEL, _("_Open"),
					     GTK_RESPONSE_ACCEPT,
					     NULL);
	gui_fit_dialog_to_screen(GTK_WINDOW(dialog));

	gui_file_chooser_set_current_folder((GtkFileChooser *)dialog,
					    tabs_dir);
	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;

		filename =
		    gui_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		filename = g_path_get_basename(filename);
		gui_close_all_tabs();
		gui_load_tabs(filename);
		redisplay_to_realign();
		g_free(filename);
	}

	gui_widget_destroy(dialog);
}

/******************************************************************************
 * Name
 *  on_show_bible_text_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_show_bible_text_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *    toggle Bibletext window
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_show_bible_text_activate(gpointer menuitem,
			    gpointer user_data)
{
	gui_show_hide_texts((GPOINTER_TO_INT(user_data) != 0));
	redisplay_to_realign();
}

G_MODULE_EXPORT void
on_preview_activate(gpointer menuitem, gpointer user_data)
{
	gui_show_hide_preview((GPOINTER_TO_INT(user_data) != 0));
	redisplay_to_realign();
}

G_MODULE_EXPORT void
on_reading_mode_activate(gpointer menuitem, gpointer user_data)
{
	gui_toggle_reading_mode((GPOINTER_TO_INT(user_data) != 0));
}

/******************************************************************************
 * Name
 *  on_show_commentary_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_show_commentary_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *    toggle commentary window *** we need to change the name of this
 *    function because there no longer an upper_workbook
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_show_commentary_activate(gpointer menuitem,
			    gpointer user_data)
{
	gboolean show = (GPOINTER_TO_INT(user_data) != 0);

	/* Esta opción representa específicamente los comentarios propios de
	 * la edición, no la pestaña Libro ni las notas personales. */
	if (show)
		settings.comm_showing = TRUE;
	gui_show_hide_comms(show);
	/* Redisplay after the paned has allocated the new Bible width.
	 * Doing it here laid the chapter out at the old wrap, which left
	 * verses in a left-hand column once the study pane closed. */
	gui_schedule_bible_text_reflow(TRUE);
	/* Una nota personal del versículo puede seleccionar temporalmente su
	 * pestaña durante el redisplay; la acción explícita del menú debe ser
	 * la última palabra. */
	if (show)
		gtk_notebook_set_current_page(
		    GTK_NOTEBOOK(widgets.notebook_comm_book), 0);
}

/******************************************************************************
 * Name
 *   on_show_dictionary_lexicon_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_show_dictionary_lexicon_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *    toggle lower_workbook view (on or off)
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_show_dictionary_lexicon_activate(gpointer menuitem,
				    gpointer user_data)
{
	gui_show_hide_dicts((GPOINTER_TO_INT(user_data) != 0));
	redisplay_to_realign();
}

/******************************************************************************
 * Name
 *   on_module_manager_activate
 *
 * Synopsis
 *   #include "gui/main_menu.h"
 *
 *   void on_module_manager_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */
G_MODULE_EXPORT void
on_module_manager_activate(gpointer menuitem, gpointer user_data)
{
	if (!main_backend_is_sword())
		gui_open_sqlite_module_manager();
	else
		gui_instalar_biblias();
}

G_MODULE_EXPORT void
on_advanced_search_activate(gpointer menuitem, gpointer user_data)
{
	main_open_search_dialog();
}

G_MODULE_EXPORT void
on_nube_palabras_activate(gpointer menuitem, gpointer user_data)
{
	gui_nube_palabras_dialog();
}

G_MODULE_EXPORT void
on_planes_lectura_activate(gpointer menuitem, gpointer user_data)
{
	gui_planes_lectura_dialog();
}

G_MODULE_EXPORT void
on_progreso_lectura_activate(gpointer menuitem, gpointer user_data)
{
	gui_progreso_lectura_dialog(widgets.app ? GTK_WINDOW(widgets.app)
						: NULL);
}

G_MODULE_EXPORT void
on_versiculo_dia_activate(gpointer menuitem, gpointer user_data)
{
	gui_versiculo_dia_dialog(widgets.app ? GTK_WINDOW(widgets.app)
					     : NULL);
}

G_MODULE_EXPORT void
on_memorizacion_activate(gpointer menuitem, gpointer user_data)
{
	gui_memorizacion_dialog(widgets.app ? GTK_WINDOW(widgets.app) : NULL);
}

G_MODULE_EXPORT void
on_pulpito_activate(gpointer menuitem, gpointer user_data)
{
	gui_pulpito_elegir(widgets.app ? GTK_WINDOW(widgets.app) : NULL);
}

G_MODULE_EXPORT void
on_diccionario_activate(gpointer menuitem, gpointer user_data)
{
	gui_diccionario_dialog();
}

G_MODULE_EXPORT void
on_buscar_notas_activate(gpointer menuitem, gpointer user_data)
{
	gui_buscar_notas_dialog(widgets.app ? GTK_WINDOW(widgets.app) : NULL);
}

G_MODULE_EXPORT void
on_studypad_activate(gpointer menuitem, gpointer user_data)
{
	editor_open_studypad();
}

G_MODULE_EXPORT void
on_testimonios_activate(gpointer menuitem, gpointer user_data)
{
	gui_testimonios_dialog(widgets.app ? GTK_WINDOW(widgets.app) : NULL);
}

G_MODULE_EXPORT void
on_attach_detach_sidebar_activate(gpointer menuitem,
				  gpointer user_data)
{
	gui_attach_detach_sidebar();
}

G_MODULE_EXPORT void
on_sidebar_showhide_activate(gpointer menuitem, gpointer user_data)
{
	gui_sidebar_showhide();
}

static GSimpleActionGroup *main_menu_actions;
static GMenu *main_menu_model;

#define MENU_ACTION(name, handler) \
static void name(GSimpleAction *action, GVariant *parameter, gpointer data) \
{ (void)action; (void)parameter; handler(NULL, data); }

MENU_ACTION(export_action, on_export_bible_activate)
MENU_ACTION(install_action, on_module_manager_activate)
MENU_ACTION(preferences_action, on_preferences_activate)
MENU_ACTION(navigation_action, on_navigation_prefs_activate)
MENU_ACTION(quit_action, on_quit_activate)
MENU_ACTION(search_action, on_search_activate)
MENU_ACTION(advanced_search_action, on_advanced_search_activate)
MENU_ACTION(notes_search_action, on_buscar_notas_activate)
MENU_ACTION(studypad_action, on_studypad_activate)
MENU_ACTION(dictionary_dialog_action, on_diccionario_activate)
MENU_ACTION(cloud_action, on_nube_palabras_activate)
MENU_ACTION(testimonies_action, on_testimonios_activate)
MENU_ACTION(plans_action, on_planes_lectura_activate)
MENU_ACTION(progress_action, on_progreso_lectura_activate)
MENU_ACTION(daily_verse_action, on_versiculo_dia_activate)
MENU_ACTION(memorize_action, on_memorizacion_activate)
MENU_ACTION(pulpit_action, on_pulpito_activate)
MENU_ACTION(devotion_action, on_daily_devotion_activate)
MENU_ACTION(sidebar_action, on_sidebar_showhide_activate)
MENU_ACTION(open_session_action, on_open_session_activate)
MENU_ACTION(save_session_action, on_save_session_activate)
MENU_ACTION(help_action, on_help_contents_activate)
static void shortcuts_action(GSimpleAction *action, GVariant *parameter, gpointer data)
{ (void)action; (void)parameter; (void)data; gui_atajos_mostrar(); }
MENU_ACTION(report_action, on_report_bug_activate)
MENU_ACTION(about_action, on_about_xiphos_activate)
MENU_ACTION(sword_action, on_about_the_sword_project_activate)
MENU_ACTION(biblesync_action, on_about_biblesync_activate)
MENU_ACTION(translation_action, on_about_translation_activate)

static void state_changed(GSimpleAction *action, GVariant *state, gpointer data)
{
	const gchar *name = g_action_get_name(G_ACTION(action));
	gboolean active = g_variant_get_boolean(state);
	g_simple_action_set_state(action, state);
	gpointer value = GINT_TO_POINTER(active);
	if (!g_strcmp0(name, "linked-tabs")) on_linked_tabs_activate(NULL, value);
	else if (!g_strcmp0(name, "read-aloud")) on_read_aloud_activate(NULL, value);
	else if (!g_strcmp0(name, "verse-numbers")) on_show_verse_numbers_activate(NULL, value);
	else if (!g_strcmp0(name, "current-highlight")) on_versehighlight_activate(NULL, value);
	else if (!g_strcmp0(name, "annotated-highlight")) on_annotate_highlight_activate(NULL, value);
	else if (!g_strcmp0(name, "parallel-tab")) gui_parallel_tab_activate(NULL, value);
	else if (!g_strcmp0(name, "side-preview")) on_side_preview_activate(NULL, value);
	else if (!g_strcmp0(name, "bible")) on_show_bible_text_activate(NULL, value);
	else if (!g_strcmp0(name, "preview")) on_preview_activate(NULL, value);
	else if (!g_strcmp0(name, "reading-mode")) on_reading_mode_activate(NULL, value);
	else if (!g_strcmp0(name, "commentary")) on_show_commentary_activate(NULL, value);
	else if (!g_strcmp0(name, "dictionary")) on_show_dictionary_lexicon_activate(NULL, value);
	else if (!g_strcmp0(name, "split")) gui_lectura_sync_set_visible(active);
	else if (!g_strcmp0(name, "interlinear")) gui_interlineal_set_active(active);
}

static void theme_changed(GSimpleAction *action, GVariant *state, gpointer data)
{
	(void)data;
	g_simple_action_set_state(action, state);
	gui_elim_tema_set(g_variant_get_string(state, NULL));
}

static void add_state(const gchar *name, gboolean state)
{
	GSimpleAction *action = g_simple_action_new_stateful(name, NULL,
						     g_variant_new_boolean(state));
	g_signal_connect(action, "change-state", G_CALLBACK(state_changed), NULL);
	g_action_map_add_action(G_ACTION_MAP(main_menu_actions), G_ACTION(action));
	g_object_unref(action);
}

void gui_main_menu_set_state(const gchar *name, gboolean state)
{
	GAction *action = main_menu_actions ? g_action_map_lookup_action(
	    G_ACTION_MAP(main_menu_actions), name) : NULL;
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(state));
}

void gui_main_menu_set_theme(const gchar *mode)
{
	GAction *action = main_menu_actions ? g_action_map_lookup_action(
	    G_ACTION_MAP(main_menu_actions), "theme") : NULL;
	if (action && mode)
		g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_string(mode));
}

void gui_main_menu_change_state(const gchar *name, gboolean state)
{
	if (main_menu_actions)
		g_action_group_change_action_state(G_ACTION_GROUP(main_menu_actions), name,
						   g_variant_new_boolean(state));
}

gboolean gui_main_menu_get_state(const gchar *name)
{
	GVariant *state = main_menu_actions ? g_action_group_get_action_state(
	    G_ACTION_GROUP(main_menu_actions), name) : NULL;
	gboolean value = state ? g_variant_get_boolean(state) : FALSE;
	g_clear_pointer(&state, g_variant_unref);
	return value;
}

GActionGroup *gui_main_menu_actions(void)
{
	return main_menu_actions ? G_ACTION_GROUP(main_menu_actions) : NULL;
}

GMenuModel *gui_main_menu_model(void)
{
	return main_menu_model ? G_MENU_MODEL(main_menu_model) : NULL;
}

static GMenu *section(void) { return g_menu_new(); }

static void add_section(GMenu *menu, GMenu *items)
{
	g_menu_append_section(menu, NULL, G_MENU_MODEL(items));
	g_object_unref(items);
}

static void add_submenu(GMenu *menu, const gchar *label, GMenu *submenu)
{
	g_menu_append_submenu(menu, label, G_MENU_MODEL(submenu));
	g_object_unref(submenu);
}

/* ACCEL only labels the entry, right-aligned as in any GTK menu: the key
 * itself is handled by the main window's key handler, so nothing fires
 * twice. */
static void append(GMenu *menu, const gchar *label, const gchar *action,
		   const gchar *accel)
{
	GMenuItem *item = g_menu_item_new(label, action);
	if (accel)
		g_menu_item_set_attribute(item, "accel", "s", accel);
	g_menu_append_item(menu, item);
	g_object_unref(item);
}

static void append_target(GMenu *menu, const gchar *label, const gchar *action,
			  const gchar *target)
{
	GMenuItem *item = g_menu_item_new(label, NULL);
	g_menu_item_set_action_and_target(item, action, "s", target);
	g_menu_append_item(menu, item);
	g_object_unref(item);
}

GtkWidget *gui_create_main_menu(void)
{
	main_menu_actions = g_simple_action_group_new();
	const GActionEntry entries[] = {
		{"export", export_action}, {"install", install_action},
		{"preferences", preferences_action}, {"navigation", navigation_action},
		{"quit", quit_action}, {"search", search_action},
		{"advanced-search", advanced_search_action}, {"notes-search", notes_search_action},
		{"studypad", studypad_action},
		{"dictionary-dialog", dictionary_dialog_action}, {"cloud", cloud_action},
		{"testimonies", testimonies_action}, {"plans", plans_action},
		{"progress", progress_action}, {"daily-verse", daily_verse_action},
		{"memorize", memorize_action}, {"pulpit", pulpit_action},
		{"devotion", devotion_action}, {"sidebar", sidebar_action},
		{"open-session", open_session_action}, {"save-session", save_session_action},
		{"help", help_action}, {"shortcuts", shortcuts_action}, {"report", report_action}, {"about", about_action},
		{"sword", sword_action}, {"biblesync", biblesync_action},
		{"translation", translation_action}
	};
	g_action_map_add_action_entries(G_ACTION_MAP(main_menu_actions), entries,
					G_N_ELEMENTS(entries), NULL);
	add_state("linked-tabs", settings.linkedtabs);
	add_state("read-aloud", settings.readaloud);
	add_state("verse-numbers", settings.showversenum);
	add_state("current-highlight", settings.versehighlight);
	add_state("annotated-highlight", settings.annotate_highlight);
	add_state("parallel-tab", settings.showparatab);
	add_state("side-preview", settings.show_previewer_in_sidebar);
	add_state("bible", settings.showtexts);
	add_state("preview", settings.showpreview);
	add_state("reading-mode", settings.reading_mode);
	add_state("commentary", settings.showcomms);
	add_state("dictionary", settings.showdicts);
	add_state("split", settings.show_lectura_sync);
	add_state("interlinear", settings.show_interlineal);
	GSimpleAction *theme = g_simple_action_new_stateful("theme", G_VARIANT_TYPE_STRING,
		g_variant_new_string(settings.ui_mode ? settings.ui_mode : "omarchy"));
	g_signal_connect(theme, "change-state", G_CALLBACK(theme_changed), NULL);
	g_action_map_add_action(G_ACTION_MAP(main_menu_actions), G_ACTION(theme));
	g_object_unref(theme);

	GMenu *bar = g_menu_new(), *m, *s, *sub;
	main_menu_model = bar;

	m = section();
	s = section();
	append(s, _("_Exportar pasaje…"), "menu.export", NULL);
	add_section(m, s);
	s = section();
	append(s, _("_Instalar Biblias…"), "menu.install", "F4");
	append(s, _("_Preferencias…"), "menu.preferences", "F2");
	append(s, _("Nave_gación y rueda…"), "menu.navigation", NULL);
	add_section(m, s);
	s = section();
	append(s, _("_Salir"), "menu.quit", "<Control>q");
	add_section(m, s);
	add_submenu(bar, _("_Archivo"), m);

	m = section();
	append(m, _("_Buscar en la Biblia"), "menu.search", NULL);
	append(m, _("Búsqueda a_vanzada…"), "menu.advanced-search", "F3");
	append(m, _("Buscar en mis _notas…"), "menu.notes-search", NULL);
	add_submenu(bar, _("_Buscar"), m);

	m = section();
	append(m, _("_Mis estudios…"), "menu.studypad", NULL);
	append(m, _("Biblia _interlineal"), "menu.interlinear", NULL);
	append(m, _("Di_ccionario…"), "menu.dictionary-dialog", NULL);
	append(m, _("_Nube de palabras…"), "menu.cloud", NULL);
	append(m, _("_Jesús en la historia…"), "menu.testimonies", NULL);
	add_submenu(bar, _("_Estudio"), m);

	m = section();
	s = section();
	append(s, _("_Planes de lectura…"), "menu.plans", NULL);
	append(s, _("_Mi progreso…"), "menu.progress", NULL);
	append(s, _("_Versículo del día…"), "menu.daily-verse", NULL);
	append(s, _("Me_morización…"), "menu.memorize", NULL);
	add_section(m, s);
	s = section();
	append(s, _("_Leer en voz alta"), "menu.read-aloud", "<Control>r");
	append(s, _("Abrir en p_úlpito…"), "menu.pulpit", NULL);
	/* «Vista previa de la devoción diaria» and «Paneles › Diccionario»
	 * stay out of the menu, as they were hidden before: their actions
	 * remain for the code that sets their state. */
	add_section(m, s);
	add_submenu(bar, _("_Lectura"), m);

	m = section();
	sub = section();
	append_target(sub, _("_Omarchy"), "menu.theme", "omarchy");
	append_target(sub, _("_Claro"), "menu.theme", "claro");
	append_target(sub, _("Osc_uro"), "menu.theme", "oscuro");
	append_target(sub, _("Claro _luna"), "menu.theme", "claroluna");
	append_target(sub, _("_Pergamino"), "menu.theme", "pergamino");
	add_submenu(m, _("_Apariencia"), sub);
	s = section();
	append(s, _("_Modo lectura"), "menu.reading-mode", "<Control><Shift>f");
	append(s, _("Pantalla _dividida"), "menu.split", NULL);
	add_section(m, s);
	sub = section();
	s = section();
	append(s, _("_Biblia"), "menu.bible", NULL);
	append(s, _("_Vista previa"), "menu.preview", NULL);
	append(s, _("_Comentarios del autor"), "menu.commentary", NULL);
	add_section(sub, s);
	s = section();
	append(s, _("Mostrar u ocultar panel _lateral"), "menu.sidebar", "<Control>s");
	append(s, _("Vista previa _en el lateral"), "menu.side-preview", NULL);
	add_section(sub, s);
	add_submenu(m, _("_Paneles"), sub);
	sub = section();
	append(sub, _("_Números de versículo"), "menu.verse-numbers", NULL);
	append(sub, _("Resaltar versículo _actual"), "menu.current-highlight", NULL);
	append(sub, _("Resaltar versículos _anotados"), "menu.annotated-highlight", NULL);
	add_submenu(m, _("_Texto"), sub);
	sub = section();
	s = section();
	append(s, _("_Vincular pestañas"), "menu.linked-tabs", NULL);
	append(s, _("Vista _paralela en pestaña"), "menu.parallel-tab", NULL);
	add_section(sub, s);
	s = section();
	append(s, _("_Abrir sesión…"), "menu.open-session", NULL);
	append(s, _("_Guardar sesión…"), "menu.save-session", NULL);
	add_section(sub, s);
	add_submenu(m, _("Pe_stañas"), sub);
	add_submenu(bar, _("_Ver"), m);

	m = section();
	s = section();
	append(s, _("_Contenido"), "menu.help", "F1");
	append(s, _("_Atajos de teclado"), "menu.shortcuts", "<Control>slash");
	append(s, _("Informar de un _error"), "menu.report", NULL);
	add_section(m, s);
	s = section();
	append(s, _("Acerca de _Biblia Elim"), "menu.about", NULL);
	sub = section();
	append(sub, _("_SWORD"), "menu.sword", NULL);
	append(sub, _("Bible_Sync"), "menu.biblesync", NULL);
	append(sub, _("_Traducción"), "menu.translation", NULL);
	add_submenu(s, _("_Créditos"), sub);
	add_section(m, s);
	add_submenu(bar, _("A_yuda"), m);

	GtkWidget *menu = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(bar));
	gtk_widget_insert_action_group(menu, "menu", G_ACTION_GROUP(main_menu_actions));
	g_object_set_data_full(G_OBJECT(menu), "elim-menu-actions",
			       g_object_ref(main_menu_actions), g_object_unref);
	gtk_widget_show(menu);
	return menu;
}
