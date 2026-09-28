/*
 * Xiphos Bible Study Tool
 * parallel_dialog.c - dialog for detached parallel
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

#include "gui/parallel_dialog.h"
#include "gui/parallel_view.h"
#include "gui/xiphos.h"
#include "gui/navbar_versekey_parallel.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/sword.h"
#include "main/lists.h"
#include "main/navbar.h"
#include "main/navbar_versekey.h"
#include "main/parallel_view.h"
#include "main/settings.h"
#include "main/url.hh"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

extern gboolean do_display;

GtkWidget *entrycbIntBook;
GtkWidget *sbIntChapter;
GtkWidget *sbIntVerse;
GtkWidget *entryIntLookup;

/******************************************************************************
 * static
 */
static GtkWidget *dialog_parallel;

static GtkWidget *parallel_UnDock_Dialog;
static GtkWidget *vboxInt;
static gboolean ApplyChangeBook;
extern NAVBAR_VERSEKEY navbar_parallel;

static GtkWidget *create_parallel_dialog(void);
static void sync_with_main(void);

static void
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, gpointer user_data)
{
	gui_popup_menu_parallel(GTK_WIDGET(html));
}

/******************************************************************************
 * Name
 *   undock_parallel_page
 *
 * Synopsis
 *   #include "parallel_dialog.h"
 *
 *   void undock_parallel_page(void)
 *
 * Description
 *
 *
 *
 *
 *
 * Return value
 *   void
 */

void gui_undock_parallel_page(void)
{
	ApplyChangeBook = FALSE;
	parallel_UnDock_Dialog = create_parallel_dialog();

	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
				   FALSE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
				      0);

	gtk_widget_show(parallel_UnDock_Dialog);
	settings.dockedInt = FALSE;
	ApplyChangeBook = TRUE;
	sync_with_main();
}

/******************************************************************************
 * Name
 *   on_btnDockInt_clicked
 *
 * Synopsis
 *   #include "parallel_dialog.h"
 *
 *   void on_btnDockInt_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *
 *
 *
 *
 *
 * Return value
 *   void
 */

void gui_btnDockInt_clicked(GtkButton *button, gpointer user_data)
{
	gui_widget_destroy(parallel_UnDock_Dialog);
}

/******************************************************************************
 * Name
 *   on_dlgparallel_destroy
 *
 * Synopsis
 *   #include "parallel_dialog.h"
 *
 *   void on_dlgparallel_destroy(GtkObject *object, gpointer user_data)
 *
 * Description
 *
 *
 *
 *
 *
 * Return value
 *   void
 */

static void on_dlgparallel_destroy(GObject *object, gpointer user_data)
{

	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
				   TRUE);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
				      1);
	settings.dockedInt = TRUE;
	main_update_parallel_page();

	settings.display_parallel = 0;
	widgets.html_parallel_dialog = NULL;
	xml_set_value("Xiphos", "layout", "parallelopen", "0");
}

/******************************************************************************
 * Name
 *   sync_with_main
 *
 * Synopsis
 *   #include "gui/parallel_dialog.h"
 *
 *   void sync_with_main(void)
 *
 * Description navbar.lookup_entry
 *   bring the the Parallel Dialog into sync with main window
 *
 * Return value
 *   void
 */

static void sync_with_main(void)
{
	gchar *buf = NULL;

	buf = (gchar *)main_url_encode(settings.currentverse);

	if (buf && (strlen(buf) > 3)) {
		gchar *url =
		    g_strdup_printf("passagestudy.jsp?action=showParallel&"
				    "type=verse&value=%s",
				    buf);
		main_url_handler(url, TRUE);
		g_free(url);
	}
}

/******************************************************************************
 * Name
 *   gui_keep_parallel_dialog_in_sync
 *
 * Synopsis
 *   #include "gui/parallel_dialog.h"
 *
 *   void gui_keep_parallel_dialog_in_sync(const char * key)
 *
 *
 *
 * Return value
 *   void
 */

void gui_keep_parallel_dialog_in_sync(void)
{
	if (gui_toggle_get_active(GTK_WIDGET(navbar_parallel.button_sync)))
		sync_with_main();
}

/******************************************************************************
 * Name
 *   create_nav_toolbar
 *
 * Synopsis
 *   #include ".h"
 *
 *   GtkWidget *create_nav_toolbar(void)
 *
 * Description
 *    create navigation toolbar and
 *
 * Return value
 *   void
 */

static GtkWidget *create_nav_toolbar(void)
{
	return gui_navbar_versekey_parallel_new();
}

/******************************************************************************
 * Name
 *   on_parallel_configure_event
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   gboolean on_parallel_configure_event(GtkWidget * widget,
 *				   GdkEventConfigure * event,
 *				   gpointer user_data)
 *
 * Description
 *   remember placement+size of parallel window.
 *   cloned from on_configure_event
 *
 * Return value
 *   gboolean
 */

static void on_parallel_configure_event(GObject *window, GParamSpec *pspec,
					gpointer user_data)
{
	gchar layout[10];
	gint width, height;

	(void)pspec;
	(void)user_data;
	/* GTK 4 keeps the window's size as its default size; the position
	 * belongs to the compositor */
	gtk_window_get_default_size(GTK_WINDOW(window), &width, &height);
	if (width <= 0 || height <= 0)
		return;
	settings.parallel_width = width;
	settings.parallel_height = height;

	sprintf(layout, "%d", settings.parallel_width);
	xml_set_value("Xiphos", "layout", "parallel_width", layout);

	sprintf(layout, "%d", settings.parallel_height);
	xml_set_value("Xiphos", "layout", "parallel_height", layout);
	xml_save_settings_doc(settings.fnconfigure);
}

/******************************************************************************
 * Name
 *   create_parallel_dialog
 *
 * Synopsis
 *   #include "parallel_dialog.h"
 *
 *   GtkWidget *create_parallel_dialog(void)
 *
 * Description
 *
 *
 *
 * Return value
 *   GtkWidget *
 */
static GtkWidget *create_parallel_dialog(void)
{
	GtkWidget *box_parallel_labels;
	GtkWidget *dialog_vbox25;
	GtkWidget *toolbar29;
	GtkWidget *dialog_action_area25;
	GtkWidget *hbuttonbox4;
	GtkWidget *btnDockInt;
	gchar title[256];

	sprintf(title, "%s - %s", settings.program_title, _("Parallel"));

	dialog_parallel = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog_parallel), title);

	g_object_set_data(G_OBJECT(dialog_parallel),
			  "dialog_parallel", dialog_parallel);
	gtk_window_set_default_size(GTK_WINDOW(dialog_parallel), settings.parallel_width, settings.parallel_height);
	gtk_window_set_resizable(GTK_WINDOW(dialog_parallel), TRUE);

	dialog_vbox25 =
	    gtk_dialog_get_content_area(GTK_DIALOG(dialog_parallel));
	g_object_set_data(G_OBJECT(dialog_parallel), "dialog_vbox25",
			  dialog_vbox25);
	gtk_widget_show(dialog_vbox25);

	UI_VBOX(vboxInt, FALSE, 0);
	gtk_widget_show(vboxInt);
	gui_box_pack(GTK_BOX(dialog_vbox25), vboxInt, TRUE, TRUE, 0);
	toolbar29 = create_nav_toolbar();
	gtk_widget_show(toolbar29);
	gtk_box_append(GTK_BOX(vboxInt), toolbar29);

	UI_HBOX(box_parallel_labels, TRUE, 2);
	gtk_widget_show(box_parallel_labels);
	gtk_box_append(GTK_BOX(vboxInt), box_parallel_labels);
	gui_widget_set_margins(box_parallel_labels, 2);


	widgets.html_parallel_dialog =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, PARALLEL_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(widgets.html_parallel_dialog,
				     "bible-parallel");
	gtk_widget_show(widgets.html_parallel_dialog);
	gui_box_pack(GTK_BOX(vboxInt), widgets.html_parallel_dialog, TRUE, TRUE, 0);

	g_signal_connect((gpointer)widgets.html_parallel_dialog,
			 "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb), NULL);

	dialog_action_area25 =
	    gtk_dialog_get_content_area(GTK_DIALOG(dialog_parallel));
	g_object_set_data(G_OBJECT(dialog_parallel),
			  "dialog_action_area25", dialog_action_area25);
	gtk_widget_show(dialog_action_area25);
	gui_widget_set_margins(dialog_action_area25, 10);

	hbuttonbox4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show(hbuttonbox4);

	gui_box_pack(GTK_BOX(dialog_action_area25), hbuttonbox4, FALSE, TRUE, 3);

	gtk_widget_set_halign(hbuttonbox4, GTK_ALIGN_END);
	btnDockInt =
	    gtk_button_new_from_icon_name("window-close");
	gtk_widget_show(btnDockInt);
	gtk_box_append(GTK_BOX(hbuttonbox4), btnDockInt);

	g_signal_connect(G_OBJECT(dialog_parallel), "destroy",
			 G_CALLBACK(on_dlgparallel_destroy), NULL);
	g_signal_connect(G_OBJECT(btnDockInt), "clicked",
			 G_CALLBACK(gui_btnDockInt_clicked), NULL);

	g_signal_connect(dialog_parallel, "notify::default-width",
			 G_CALLBACK(on_parallel_configure_event), NULL);
	g_signal_connect(dialog_parallel, "notify::default-height",
			 G_CALLBACK(on_parallel_configure_event), NULL);

	settings.display_parallel = 1;
	xml_set_value("Xiphos", "layout", "parallelopen", "1");

	set_window_icon(GTK_WINDOW(dialog_parallel));

	return dialog_parallel;
}

/******   end of file   ******/
