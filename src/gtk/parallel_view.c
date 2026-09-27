/*
 * Xiphos Bible Study Tool
 * gs_parallel.c - support for displaying multiple modules
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

#include "xiphos_html/xiphos_html.h"

#include "gui/parallel_view.h"
#include "gui/parallel_dialog.h"
#include "gui/main_window.h"
#include "gui/xiphos.h"
#include "gui/widgets.h"
#include "gui/tabbed_browser.h"
#include "gui/preferences_dialog.h"

#include "main/parallel_view.h"
#include "main/global_ops.hh"
#include "main/lists.h"
#include "main/sword.h"
#include "main/xml.h"

extern gboolean shift_key_pressed;

/******************************************************************************
 * static
 */

/******************************************************************************
 * Name
 *   on_undockInt_activate
 *
 * Synopsis
 *   #include "gui/parallel.h
 *
 *   void on_undockInt_activate(gpointer unused)
 *
 * Description
 *   undock/dock parallel page
 *
 * Return value
 *   void
 */

void on_undockInt_activate(gpointer unused)
{
	if (settings.dockedInt) {
		settings.dockedInt = FALSE;
		gui_undock_parallel_page();

	} else {
		settings.dockedInt = TRUE;
		gui_btnDockInt_clicked(NULL, NULL);
	}
}

/******************************************************************************
 * Name
 *   on_paratab_activate
 *
 * Synopsis
 *   #include "gui/parallel_view.h
 *
 *   void on_paratab_activate(gpointer unused)
 *
 * Description
 *   open parallel view in a tab
 *
 * Return value
 *   void
 */

void on_paratab_activate(gpointer unused)
{
	gui_open_parallel_view_in_new_tab();
}

static void on_detach(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	(void)action;
	(void)parameter;
	(void)data;
	on_undockInt_activate(NULL);
}

static gboolean destroy_popover_idle(gpointer popover)
{
	gtk_widget_destroy(GTK_WIDGET(popover));
	return G_SOURCE_REMOVE;
}

/* After the chosen item's action has run. */
static void destroy_popover_later(GtkPopover *popover, gpointer unused)
{
	(void)unused;
	g_idle_add(destroy_popover_idle, popover);
}

/* GTK4-PORT-101 step 2: a GMenu popover at the pointer over RELATIVE,
 * with fresh «paralelo» actions (their states follow the settings). */
void gui_popup_menu_parallel(GtkWidget *relative)
{
	GSimpleActionGroup *actions = g_simple_action_group_new();
	GMenu *menu = g_menu_new();
	if (!settings.showparatab) {
		static const GActionEntry detach[] = {
			{ "separar", on_detach, NULL, NULL, NULL, { 0 } },
		};
		g_action_map_add_action_entries(G_ACTION_MAP(actions), detach, 1, NULL);
		g_menu_append(menu, _("Detach/Attach"), "paralelo.separar");
	}
	GMenu *options = g_menu_new();
	main_parallel_options_menu(options, G_ACTION_MAP(actions));
	g_menu_append_submenu(menu, _("Module Options"), G_MENU_MODEL(options));
	g_object_unref(options);
	gtk_widget_insert_action_group(relative, "paralelo", G_ACTION_GROUP(actions));
	g_object_unref(actions);

	GtkWidget *popover = gtk_popover_new_from_model(relative, G_MENU_MODEL(menu));
	g_object_unref(menu);
	GdkRectangle at = { 0, 0, 1, 1 };
	GdkWindow *window = gtk_widget_get_window(relative);
	GdkSeat *seat = gdk_display_get_default_seat(gtk_widget_get_display(relative));
	if (window && seat) {
		int wx, wy;
		gdk_window_get_device_position(window, gdk_seat_get_pointer(seat),
					       &wx, &wy, NULL);
		GtkAllocation alloc;
		gtk_widget_get_allocation(relative, &alloc);
		/* A no-window widget reports window coordinates. */
		at.x = gtk_widget_get_has_window(relative) ? wx : wx - alloc.x;
		at.y = gtk_widget_get_has_window(relative) ? wy : wy - alloc.y;
	}
	gtk_popover_set_pointing_to(GTK_POPOVER(popover), &at);
	g_signal_connect(popover, "closed", G_CALLBACK(destroy_popover_later), NULL);
	gtk_popover_popup(GTK_POPOVER(popover));
}

static gboolean
on_enter_notify_event(GtkWidget *widget,
		      GdkEventCrossing *event, gpointer user_data)
{
	gtk_widget_grab_focus(GTK_WIDGET(
	    wk_html_get_view(WK_HTML(widgets.html_parallel))));
	return FALSE;
}

static void
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, gpointer user_data)
{
	gui_popup_menu_parallel(GTK_WIDGET(html));
}

/******************************************************************************
 * Name
 *   gui_create_parallel_page
 *
 * Synopsis
 *   #include "gui/parallel.h
 *
 *   void gui_create_parallel_page(guint page_num)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_create_parallel_page(void)
{
	GtkWidget *label;
#ifndef USE_WEBKIT2
	GtkWidget *scrolled_window;
#endif

	/*
	 * parallel page
	 */
	settings.dockedInt = TRUE;

#ifndef USE_WEBKIT2
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolled_window);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(widgets.notebook_bible_parallel),
			  scrolled_window);
#endif

	widgets.html_parallel =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, PARALLEL_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(widgets.html_parallel, "bible-parallel");
	gtk_widget_show(widgets.html_parallel);
#ifdef USE_WEBKIT2
	gtk_container_add(GTK_CONTAINER(widgets.notebook_bible_parallel), widgets.html_parallel);
#else
	widgets.frame_parallel = scrolled_window;
	gtk_container_add(GTK_CONTAINER(scrolled_window),
			  widgets.html_parallel);
#endif

	g_signal_connect((gpointer)widgets.html_parallel,
			 "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb), NULL);

	label = gtk_label_new(_("Parallel View"));
	gtk_widget_show(label);
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
				   gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
							     1),
				   label);

	g_signal_connect((gpointer)widgets.html_parallel,
			 "enter_notify_event",
			 G_CALLBACK(on_enter_notify_event), NULL);
}
