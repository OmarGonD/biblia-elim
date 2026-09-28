/*
 * Xiphos Bible Study Tool
 * commentary_dialog.c - dialog for a commentary module
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

#include "gui/commentary_dialog.h"
#include "gui/dialog.h"
#include "gui/xiphos.h"
#include "gui/main_window.h"
#include "gui/menu_popup.h"
#include "gui/navbar_versekey_dialog.h"
#include "gui/sidebar.h"
#include "gui/widgets.h"
#include "gui/utilities.h"

#include "main/lists.h"
#include "main/navbar.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/navbar_versekey.h"
#include "main/display.hh"

#include "gui/debug_glib_null.h"

extern gboolean dialog_freed;
extern gboolean do_display;

/****************************************************************************************
 * static - global to this file only
 */
DIALOG_DATA *cur_d;

/******************************************************************************
 * Name
 *   on_dialog_destroy
 *
 * Synopsis
 *   #include "commentary_dialog.h"
 *
 *   void on_dialog_destroy(GObject * object,
 *						DIALOG_DATA * d)
 *
 * Description
 *   shut down the View Commentay Dialog
 *
 * Return value
 *   void
 */

static void on_dialog_destroy(GObject *object, DIALOG_DATA *d)
{
	if (!dialog_freed)
		main_free_on_destroy(d);
	dialog_freed = FALSE;
}

/******************************************************************************
 * Name
 *   on_dialog_motion_notify_event
 *
 * Synopsis
 *   #include "commentary_dialog.h"
 *
 *   gboolean on_dialog_motion_notify_event(GtkWidget *widget,
                      GdkEventMotion  *event, DIALOG_DATA * d)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static void on_dialog_motion_notify_event(GtkEventControllerMotion *motion,
					  gdouble x, gdouble y,
					  DIALOG_DATA *d)
{
	(void)motion;
	(void)x;
	(void)y;
	cur_d = d;
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

static GtkWidget *create_nav_toolbar(DIALOG_DATA *d)
{
	d->navbar.type = NB_DIALOG;
	return gui_navbar_versekey_dialog_new(d);
}

static void
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, DIALOG_DATA *d)
{
	gui_menu_popup(html, cur_d->mod_name, cur_d);
}

/******************************************************************************
 * Name
 *   gui_create_commentary_dialog
 *
 * Synopsis
 *   #include "commentary_dialog.h"
 *
 *   void gui_create_commentary_dialog(void)
 *
 * Description
 *   create a Commentary Dialog
 *
 * Return value
 *   void
 */

void gui_create_commentary_dialog(DIALOG_DATA *d, gboolean do_edit)
{
	GtkWidget *vbox30;
	GtkWidget *vbox_toolbars;
	GtkWidget *toolbar_nav;
	GtkWidget *frame19;

	cur_d = d;
	d->dialog = gtk_window_new();

	g_object_set_data(G_OBJECT(d->dialog), "d->dialog", d->dialog);
	gtk_window_set_title(GTK_WINDOW(d->dialog),
			     main_get_module_description(d->mod_name));
	gtk_window_set_resizable(GTK_WINDOW(d->dialog), TRUE);
	if (do_edit)
		gtk_widget_set_size_request(d->dialog, 590, 380);
	else
		gtk_widget_set_size_request(d->dialog, 460, 280);

	UI_VBOX(vbox30, FALSE, 0);
	gtk_widget_show(vbox30);

	gtk_window_set_child(GTK_WINDOW(d->dialog), vbox30);

	UI_VBOX(vbox_toolbars, FALSE, 0);
	gtk_widget_show(vbox_toolbars);
	gtk_box_append(GTK_BOX(vbox30), vbox_toolbars);

	toolbar_nav = create_nav_toolbar(d);
	gtk_widget_show(toolbar_nav);
	gtk_box_append(GTK_BOX(vbox_toolbars), toolbar_nav);

	frame19 = gtk_frame_new(NULL);
	gtk_widget_show(frame19);
	gui_box_pack(GTK_BOX(vbox30), frame19, TRUE, TRUE, 0);


	d->html =
	    GTK_WIDGET(XIPHOS_HTML_NEW(((DIALOG_DATA *)d), TRUE,
				       DIALOG_COMMENTARY_TYPE));
	gtk_widget_show(d->html);
	gtk_frame_set_child(GTK_FRAME(frame19), d->html);
	g_signal_connect((gpointer)d->html,
			 "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb),
			 (DIALOG_DATA *)d);

	g_signal_connect(G_OBJECT(d->dialog), "destroy",
			 G_CALLBACK(on_dialog_destroy), d);
	{
		GtkEventController *motion = gtk_event_controller_motion_new();

		g_signal_connect(motion, "motion",
				 G_CALLBACK(on_dialog_motion_notify_event), d);
		gtk_widget_add_controller(d->dialog, motion);
	}
}
