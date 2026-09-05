/*
 * Xiphos Bible Study Tool
 * main_window.h - main window gui
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

#ifndef __MAIN_WINDOW_H_
#define __MAIN_WINDOW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>
//#include "gui/toolbar_nav.h"
#include "main/navbar_versekey.h"

typedef struct _tab_page TAB_PAGE;
struct _tab_page
{
	GtkWidget *vbox;
	GtkWidget *paned_text_preview;
	GtkWidget *paned_text;
	GtkWidget *paned_comm;
	GtkWidget *paned_book;

	// ***** html widgets *****
	GtkWidget *html_text;
	GtkWidget *html_parallel;
	GtkWidget *html_comm;
	GtkWidget *html_dict;
	GtkWidget *html_book;
	GtkWidget *html_preview;

	// ***** backend  *****
	gpointer backend;
	// ***** keys *****
	gchar *text_comm_key;
	gchar *dict_key;
	gchar *book_key;

	NAVBAR_VERSEKEY nav_bar;
};

#include "main/settings.h"

void gui_zoom_base_font(int up);
void gui_show_hide_texts(int choice);
void gui_show_hide_preview(int choice);
void gui_show_hide_comms(int choice);
void gui_show_hide_dicts(int choice);
void gui_toggle_reading_mode(int choice);
/* Syncs the reading strip's interlinear toggle with the live setting,
 * called from the interlinear code whenever that setting changes. */
void gui_reading_interlinear_sync(void);

/* TRUE once create_mainwindow() has finished building the window --
 * main_init_backend() (main.c) runs right after it returns, so before
 * this is TRUE `backend` may still be NULL. Anything that might touch
 * the Sword backend (main_display_bible() and friends) from code that
 * can also run during window construction (e.g. applying a panel's
 * saved visibility) must check this first. */
gboolean gui_main_window_ready(void);
void gui_set_bible_comm_layout(void);
void gui_change_window_title(char *module_name);
void create_mainwindow(void);
void gui_show_main_window(void);
void final_pane_sizes(void);
void gui_notebook_dict_goto_dict(void);

#ifdef __cplusplus
}
#endif
#endif
