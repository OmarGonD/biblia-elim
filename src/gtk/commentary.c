/*
 * Xiphos Bible Study Tool
 * commentary.c - gui for commentary modules
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

#include <errno.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "xiphos_html/xiphos_html.h"

#include "gui/dialog.h"
#include "gui/commentary.h"
#include "gui/bookmark_dialog.h"
#include "gui/bookmarks_treeview.h"
#include "gui/xiphos.h"
#include "gui/cipher_key_dialog.h"
#include "gui/sidebar.h"
#include "gui/main_window.h"
#include "gui/menu_popup.h"
#include "gui/find_dialog.h"
#include "gui/font_dialog.h"
#include "gui/tabbed_browser.h"
#include "gui/widgets.h"
#include "gui/utilities.h"

#ifdef USE_WEBKIT_EDITOR
#include "editor/webkit_editor.h"
#else
#include "editor/slib-editor.h"
#endif

#include "main/settings.h"
#include "main/lists.h"
#include "main/sword.h"
#include "main/xml.h"
#include "main/global_ops.hh"
#include "main/search_sidebar.h"
#include "main/display.hh"
#include "main/url.hh"

#include "gui/debug_glib_null.h"

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/commentary.h"
 *
 *   void access_to_edit_perscomm()
 *
 * Description
 *   kbd shortcut hook to pers.comm editor, from outside this source file.
 *   Note: NOT static.
 *
 * Return value
 *   void
 */

void access_to_edit_percomm()
{
	gchar *personal = "Personal";
	if (!main_is_module(personal))
		return;

	editor_create_new(personal, (gchar *)settings.currentverse,
			  NOTE_EDITOR);
}

static gboolean on_enter_notify_event(GtkWidget *widget,
				      GdkEventCrossing *event,
				      gpointer user_data)
{
	return FALSE;
}

static void
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, gpointer user_data)
{
	gui_menu_popup(html, settings.CommWindowModule, NULL);
}

static void
on_commentary_close_clicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	gui_close_comms_panel();
}

/******************************************************************************
 * Name
 *   gui_create_commentary_pane
 *
 * Synopsis
 *   #include "gui/.h"
 *
 *   GtkWidget *gui_create_commentary_pane(void)
 *
 * Description
 *
 *
 * Return value
 *   GtkWidget*
 */

GtkWidget *gui_create_commentary_pane(void)
{
	GtkWidget *box_comm;
	GtkWidget *header;
	GtkWidget *title;
	GtkWidget *cerrar;
#ifndef USE_WEBKIT2
	GtkWidget *scrolledwindow;
#endif

	UI_VBOX(box_comm, FALSE, 0);
	gtk_widget_show(box_comm);

	header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_margin_start(header, 6);
	gtk_widget_set_margin_end(header, 4);
	gtk_widget_set_margin_top(header, 2);
	gtk_widget_show(header);

	title = gtk_label_new(_("Comentarios del autor"));
	gtk_widget_set_halign(title, GTK_ALIGN_START);
	gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
	gtk_widget_show(title);
	gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);

	cerrar = gtk_button_new_from_icon_name("window-close-symbolic",
					       GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_relief(GTK_BUTTON(cerrar), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(cerrar,
				    _("Cerrar panel de comentarios y notas"));
	gtk_widget_set_focus_on_click(cerrar, FALSE);
	gtk_widget_set_name(cerrar, "comm-panel-close");
	gtk_widget_show(cerrar);
	g_signal_connect(cerrar, "clicked",
			 G_CALLBACK(on_commentary_close_clicked), NULL);
	gtk_box_pack_end(GTK_BOX(header), cerrar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box_comm), header, FALSE, FALSE, 0);

#ifndef USE_WEBKIT2
	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow);
	gtk_box_pack_start(GTK_BOX(box_comm),
			   scrolledwindow, TRUE, TRUE, 0);
#endif

	widgets.html_comm =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, COMMENTARY_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(widgets.html_comm, "commentary");
	gtk_widget_show(widgets.html_comm);
#ifdef USE_WEBKIT2
	gtk_box_pack_start(GTK_BOX(box_comm), widgets.html_comm, TRUE, TRUE, 0);
#else
	gtk_container_add(GTK_CONTAINER(scrolledwindow),
			  widgets.html_comm);
#endif

	g_signal_connect((gpointer)widgets.html_comm,
			 "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb), NULL);
#ifndef USE_WEBKIT2
	g_signal_connect((gpointer)scrolledwindow, "enter_notify_event",
			 G_CALLBACK(on_enter_notify_event), NULL);
#endif

	return box_comm;
}
