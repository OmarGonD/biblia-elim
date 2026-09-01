/*
 * Xiphos Bible Study Tool
 * navbar_verse.c - navigation bar for versekey modules
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

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "editor/slib-editor.h"

#include "gui/navbar_versekey.h"
#include "gui/bibletext_dialog.h"
#include "gui/tabbed_browser.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/lists.h"
#include "main/module_dialogs.h"
#include "main/navbar_versekey.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/tab_history.h"
#include "main/url.hh"

#include "gui/debug_glib_null.h"

NAVBAR_VERSEKEY navbar_versekey;

extern PASSAGE_TAB_INFO *cur_passage_tab;

/******************************************************************************
 * Name
 *   menu_deactivate_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   void menu_deactivate_callback (GtkWidget *widget, gpointer user_data)
 *
 * Description
 *   return toggle button to normal
 *
 * Return value
 *   void
 */

static void menu_deactivate_callback(GtkWidget *widget,
				     gpointer user_data)
{
	GtkWidget *menu_button;

	menu_button = GTK_WIDGET(user_data);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menu_button),
				     FALSE);
}

/******************************************************************************
 * Name
 *   menu_position_under
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   void menu_position_under(GtkMenu * menu, int * x, int * y,
 *				gboolean * push_in, gpointer user_data)
 *
 * Description
 *   position drop down menu under toogle button
 *
 *
 * Return value
 *   void
 */

static void menu_position_under(GtkMenu *menu, int *x, int *y,
				gboolean *push_in, gpointer user_data)
{
	GtkWidget *widget;
	GtkAllocation allocation;

	g_return_if_fail(GTK_IS_BUTTON(user_data));
#if GTK_CHECK_VERSION(2, 20, 0)
	g_return_if_fail(gtk_widget_get_window(user_data));
#else
	g_return_if_fail(GTK_WIDGET_NO_WINDOW(user_data));
#endif
	widget = GTK_WIDGET(user_data);

	gdk_window_get_origin(gtk_widget_get_window(widget), x, y);
	gtk_widget_get_allocation(widget, &allocation);
	*x += allocation.x;
	*y += allocation.y + allocation.height;

	*push_in = FALSE;
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GdkEventButton *event,
 *			      gpointer user_data)
 *
 * Description
 *    make the tooglebutton act like a gtk optionmenu by dropping a popup
 *    under the button
 *
 * Return value
 *   gboolean
 */

static gboolean select_button_press_callback(GtkWidget *widget,
					     GdkEventButton *event,
					     gpointer user_data)
{
	GtkWidget *menu;

	menu = main_versekey_drop_down_new(cur_passage_tab);
	if (!menu)
		return 0;
	g_signal_connect(menu, "deactivate",
			 G_CALLBACK(menu_deactivate_callback), widget);
	if ((event->type == GDK_BUTTON_PRESS) && event->button == 1) {
		gtk_widget_grab_focus(widget);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
					     TRUE);
#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_widget(GTK_MENU(menu), widget, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
			       menu_position_under, widget, event->button,
			       event->time);
#endif
		return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GdkEventButton *event,
 *			      gpointer user_data)
 *
 * Description
 *    make the tooglebutton act like a gtk optionmenu by dropping a popup
 *    under the button
 *
 * Return value
 *   gboolean
 */

static gboolean select_book_button_press_callback(GtkWidget *widget,
						  GdkEventButton *event,
						  gpointer user_data)
{
	GtkWidget *menu;

	GTimeVal start_time;
	GTimeVal end_time;
#ifdef WIN32
	glong time_diff;
	guint32 time_add;
#endif

	g_get_current_time(&start_time);

	menu = main_versekey_drop_down_book_menu(navbar_versekey, NB_MAIN,
						 NULL, NULL);

	g_get_current_time(&end_time);
#ifdef WIN32
	time_diff =
	    ((end_time.tv_sec - start_time.tv_sec) * 1000000) +
	    (end_time.tv_usec - start_time.tv_usec);
	//if (time_diff > 10000)
	time_add = (guint32)(time_diff / 1000);
#endif
	if (!menu)
		return 0;
	g_signal_connect(menu, "deactivate",
			 G_CALLBACK(menu_deactivate_callback), widget);
	if ((event->type == GDK_BUTTON_PRESS) && event->button == 1) {
		gtk_widget_grab_focus(widget);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
					     TRUE);
#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_widget(GTK_MENU(menu), widget, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
			       menu_position_under, widget, event->button,
#ifdef WIN32
			       event->time + time_add);
#else
			       event->time);
#endif
#endif
		return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GdkEventButton *event,
 *			      gpointer user_data)
 *
 * Description
 *    make the tooglebutton act like a gtk optionmenu by dropping a popup
 *    under the button
 *
 * Return value
 *   gboolean
 */

static gboolean select_chapter_button_press_callback(GtkWidget *widget,
						     GdkEventButton *
							 event,
						     gpointer user_data)
{
	GtkWidget *menu;

	menu = main_versekey_drop_down_chapter_menu(navbar_versekey, NB_MAIN,
						 NULL, NULL);
	if (!menu)
		return 0;
	g_signal_connect(menu, "deactivate",
			 G_CALLBACK(menu_deactivate_callback), widget);
	if ((event->type == GDK_BUTTON_PRESS) && event->button == 1) {
		gtk_widget_grab_focus(widget);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
					     TRUE);
#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_widget(GTK_MENU(menu), widget, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
			       menu_position_under, widget, event->button,
			       event->time);
#endif
		return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GdkEventButton *event,
 *			      gpointer user_data)
 *
 * Description
 *    make the tooglebutton act like a gtk optionmenu by dropping a popup
 *    under the button
 *
 * Return value
 *   gboolean
 */

static gboolean select_verse_button_press_callback(GtkWidget *widget,
						   GdkEventButton *event,
						   gpointer user_data)
{
	GtkWidget *menu;

	menu = main_versekey_drop_down_verse_menu(navbar_versekey, NB_MAIN,
						  NULL, NULL);
	if (!menu)
		return 0;
	g_signal_connect(menu, "deactivate",
			 G_CALLBACK(menu_deactivate_callback), widget);
	if ((event->type == GDK_BUTTON_PRESS) && event->button == 1) {
		gtk_widget_grab_focus(widget);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
					     TRUE);
#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_widget(GTK_MENU(menu), widget, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
			       menu_position_under, widget, event->button,
			       event->time);
#endif
		return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *  on_button_history_next_clicked
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  void on_button_history_next_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void on_button_history_next_clicked(GtkButton *button, gpointer user_data)
{
	main_navigate_tab_history(1);
}

/******************************************************************************
 * Name
 *  on_button_history_back_clicked
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  void on_button_history_back_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void on_button_history_back_clicked(GtkButton *button, gpointer user_data)
{
	main_navigate_tab_history(0);
}

/******************************************************************************
 * Name
 *   on_entry_activate
 *
 * Synopsis
 *   #include "bibletext_dialog.h"
 *
 *   void on_entry_activate(GtkEntry * entry, DIALOG_DATA * c)
 *
 * Description
 *   go to verse in free form entry if user hit <enter>
 *
 * Return value
 *   void
 */

static void on_entry_activate(GtkEntry *entry, gpointer user_data)
{
	gchar *rawtext;
	const gchar *gkey, *buf = gtk_entry_get_text(entry);

	if (buf == NULL)
		return;
	/* handle potential subsection anchor */
	if ((settings.special_anchor = strchr(buf, '#')) || /* thml */
	    (settings.special_anchor = strchr(buf, '!')))   /* osisref */
		*settings.special_anchor = '\0';

	rawtext =
	    main_get_raw_text(navbar_versekey.module_name->str,
			      (gchar *)buf);

	if (!rawtext || (rawtext && (strlen(rawtext) < 2))) {
		gtk_entry_set_text(entry, navbar_versekey.key->str);
		g_free(rawtext);
		return;
	}
	gkey =
	    main_get_valid_key(settings.MainWindowModule, (gchar *)buf);

	// we got a valid key. but was it really a valid key within v11n?
	// for future use in determining whether to show normal navbar content.
	navbar_versekey.valid_key =
	    main_is_Bible_key(settings.MainWindowModule, gkey);

	if (settings.special_anchor)
		*settings.special_anchor = '#'; /* put it back. */
	if (gkey == NULL) {
		gtk_entry_set_text(entry, navbar_versekey.key->str);
		return;
	}

	gchar *url = g_strdup_printf("sword:///%s%s", gkey,
				     (settings.special_anchor ? settings.special_anchor : ""));

	navbar_versekey.module_name =
	    g_string_assign(navbar_versekey.module_name,
			    settings.MainWindowModule);
	main_navbar_versekey_set(navbar_versekey, gkey);
	main_url_handler(url, TRUE);
	if (url)
		g_free(url);
	if (gkey)
		g_free((gchar *)gkey);
}

/******************************************************************************
 * Name
 *  on_button_verse_menu_verse_scroll_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  gboolean on_button_verse_menu_verse_scroll_event(GtkWidget * widget,
 *                                           GdkEvent * event,
 *                                           gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_button_verse_menu_verse_scroll_event(GtkWidget *widget,
							GdkEvent *event,
							gpointer user_data)
{
	main_navbar_versekey_spin_verse(navbar_versekey,
					event->scroll.direction);
	return FALSE;
}

/******************************************************************************
 * Name
 *  on_button_verse_menu_chapter_scroll_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  gboolean on_button_verse_menu_chapter_scroll_event(GtkWidget * widget,
 *                                           GdkEvent * event,
 *                                           gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_button_verse_menu_chapter_scroll_event(GtkWidget *widget,
							  GdkEvent *event,
							  gpointer user_data)
{
	main_navbar_versekey_spin_chapter(navbar_versekey,
					  event->scroll.direction);
	return FALSE;
}

/******************************************************************************
 * Name
 *  on_button_verse_menu_book_scroll_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  gboolean on_button_verse_menu_book_scroll_event(GtkWidget * widget,
 *                                           GdkEvent * event,
 *                                           gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_button_verse_menu_book_scroll_event(GtkWidget *widget,
						       GdkEvent *event,
						       gpointer user_data)
{
	main_navbar_versekey_spin_book(navbar_versekey,
				       event->scroll.direction);
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_up_eventbox_button_release_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_up_eventbox_button_release_event (GtkWidget * widget,
 *                                       	GdkEventButton * event,
 *                                       	gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_up_eventbox_button_release_event(GtkWidget *widget,
						    GdkEventButton *event,
						    gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data)) {
	case BOOK_BUTTON:
		main_navbar_versekey_spin_book(navbar_versekey, 0);
		break;
	case CHAPTER_BUTTON:
		main_navbar_versekey_spin_chapter(navbar_versekey, 0);
		break;
	case VERSE_BUTTON:
		main_navbar_versekey_spin_verse(navbar_versekey, 0);
		break;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_down_eventbox_button_release_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_down_eventbox_button_release_event(GtkWidget * widget,
 *                                      	GdkEventButton * event,
 *                                      	gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_down_eventbox_button_release_event(GtkWidget *widget,
						      GdkEventButton *event,
						      gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data)) {
	case BOOK_BUTTON:
		main_navbar_versekey_spin_book(navbar_versekey, 1);
		break;
	case CHAPTER_BUTTON:
		main_navbar_versekey_spin_chapter(navbar_versekey, 1);
		break;
	case VERSE_BUTTON:
		main_navbar_versekey_spin_verse(navbar_versekey, 1);
		break;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   access_on_up_eventbox_button_release_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_up_eventbox_button_release_event(gpointer element)
 *
 * Description
 *   access to internal static method from main_window.c
 *
 * Return value
 *   gboolean
 */

gboolean access_on_up_eventbox_button_release_event(gint element)
{
	return on_up_eventbox_button_release_event(NULL, NULL, GINT_TO_POINTER(element));
}

/******************************************************************************
 * Name
 *   access_on_down_eventbox_button_release_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_down_eventbox_button_release_event(gpointer element)
 *
 * Description
 *   access to internal static method from main_window.c
 *
 * Return value
 *   gboolean
 */

gboolean access_on_down_eventbox_button_release_event(gint element)
{
	return on_down_eventbox_button_release_event(NULL, NULL, GINT_TO_POINTER(element));
}

/******************************************************************************
 * Name
 *   _connect_signals
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   void _connect_signals(NAVBAR_VERSEKEY navbar)
 *
 * Description
 *
 *
 * Return value
 *  void
 */

static void _connect_signals(NAVBAR_VERSEKEY navbar)
{

	g_signal_connect((gpointer)navbar.lookup_entry,
			 "activate", G_CALLBACK(on_entry_activate), NULL);
	g_signal_connect((gpointer)navbar.button_book_up,
			 "button_release_event",
			 G_CALLBACK(on_up_eventbox_button_release_event),
			 GINT_TO_POINTER(BOOK_BUTTON));
	g_signal_connect((gpointer)navbar.button_book_down,
			 "button_release_event",
			 G_CALLBACK(on_down_eventbox_button_release_event),
			 GINT_TO_POINTER(BOOK_BUTTON));
	g_signal_connect((gpointer)navbar.button_chapter_up,
			 "button_release_event",
			 G_CALLBACK(on_up_eventbox_button_release_event),
			 GINT_TO_POINTER(CHAPTER_BUTTON));
	g_signal_connect((gpointer)navbar.button_chapter_down,
			 "button_release_event",
			 G_CALLBACK(on_down_eventbox_button_release_event),
			 GINT_TO_POINTER(CHAPTER_BUTTON));
	g_signal_connect((gpointer)navbar.button_verse_up,
			 "button_release_event",
			 G_CALLBACK(on_up_eventbox_button_release_event),
			 GINT_TO_POINTER(VERSE_BUTTON));
	g_signal_connect((gpointer)navbar.button_verse_down,
			 "button_release_event",
			 G_CALLBACK(on_down_eventbox_button_release_event),
			 GINT_TO_POINTER(VERSE_BUTTON));

	g_signal_connect((gpointer)navbar.button_history_back,
			 "clicked",
			 G_CALLBACK(on_button_history_back_clicked), NULL);
	g_signal_connect((gpointer)navbar.button_history_next, "clicked",
			 G_CALLBACK(on_button_history_next_clicked), NULL);
	g_signal_connect((gpointer)navbar.button_history_menu,
			 "button_press_event",
			 G_CALLBACK(select_button_press_callback), NULL);
	g_signal_connect((gpointer)navbar.button_book_menu,
			 "button_press_event",
			 G_CALLBACK(select_book_button_press_callback),
			 NULL);
	g_signal_connect((gpointer)navbar.button_chapter_menu,
			 "button_press_event",
			 G_CALLBACK(select_chapter_button_press_callback),
			 NULL);
	g_signal_connect((gpointer)navbar.button_verse_menu,
			 "button_press_event",
			 G_CALLBACK(select_verse_button_press_callback),
			 NULL);
#if !GTK_CHECK_VERSION(3, 4, 0)
	g_signal_connect((gpointer)navbar.button_verse_menu,
			 "scroll_event",
			 G_CALLBACK(on_button_verse_menu_verse_scroll_event), NULL);
	g_signal_connect((gpointer)navbar.button_chapter_menu,
			 "scroll_event",
			 G_CALLBACK(on_button_verse_menu_chapter_scroll_event),
			 NULL);
	g_signal_connect((gpointer)navbar.button_book_menu,
			 "scroll_event",
			 G_CALLBACK(on_button_verse_menu_book_scroll_event), NULL);
#endif
}

/* Picker de versión bíblica en la barra de navegación: cambia el
 * módulo de la ventana principal preservando libro/capítulo/versículo
 * enfocado (main_display_bible() re-usa settings.currentverse tal
 * cual se lo pasamos). */

static void
gui_navbar_fill_version_combo(void)
{
	GList *l, *d;
	int active = 0, index = 0;

	if (!widgets.combo_bible_version)
		return;
	gtk_combo_box_text_remove_all(
	    GTK_COMBO_BOX_TEXT(widgets.combo_bible_version));
	for (l = get_list(TEXT_LIST), d = get_list(TEXT_DESC_LIST); l;
	     l = l->next, d = d ? d->next : NULL) {
		const char *name = (const char *)l->data;
		const char *desc = d ? (const char *)d->data : NULL;

		if (!name)
			continue;
		gtk_combo_box_text_append(
		    GTK_COMBO_BOX_TEXT(widgets.combo_bible_version), name,
		    (desc && *desc) ? desc : name);
		if (settings.MainWindowModule &&
		    !strcmp(name, settings.MainWindowModule))
			active = index;
		index++;
	}
	if (index > 0)
		gtk_combo_box_set_active(
		    GTK_COMBO_BOX(widgets.combo_bible_version), active);
}

void
gui_navbar_version_combo_sync(void)
{
	const char *current;

	if (!widgets.combo_bible_version || !settings.MainWindowModule)
		return;
	current = gtk_combo_box_get_active_id(
	    GTK_COMBO_BOX(widgets.combo_bible_version));
	if (current && !strcmp(current, settings.MainWindowModule))
		return;
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(widgets.combo_bible_version),
				    settings.MainWindowModule);
}

static void
on_combo_bible_version_changed(GtkComboBox *combo, gpointer data)
{
	const char *mod = gtk_combo_box_get_active_id(combo);

	if (!mod || !settings.MainWindowModule ||
	    !strcmp(mod, settings.MainWindowModule))
		return; /* gui_navbar_version_combo_sync() nos sincronizó
			 * desde afuera -- no es un cambio real del usuario. */
	main_display_bible(mod, settings.currentverse);
}

/******************************************************************************
 * Name
 *  gui_navbar_versekey_new
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  GtkWidget *gui_navbar_book_new(void)
 *
 * Description
 *   create a new Bible navigation toolbar and return it
 *
 * Return value
 *   GtkWidget *
 */

GtkWidget *gui_navbar_versekey_new(void)
{

	GtkBuilder *gxml;
#if GTK_CHECK_VERSION(3, 4, 0)
	GtkWidget *eventbox;
#endif

/* build the widget */
	gxml = elim_gtk_builder_new();
	gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/navbar_versekey.gtkbuilder", NULL);
	navbar_versekey.dialog = FALSE;
	navbar_versekey.module_name =
	    g_string_new(settings.MainWindowModule);
	navbar_versekey.key = g_string_new(settings.currentverse);

	navbar_versekey.navbar = UI_GET_ITEM(gxml, "navbar");

	navbar_versekey.button_history_back =
	    UI_GET_ITEM(gxml, "button_history_back");
	navbar_versekey.button_history_next =
	    UI_GET_ITEM(gxml, "button_history_foward");
	navbar_versekey.button_history_menu =
	    UI_GET_ITEM(gxml, "togglebutton_history_list");

	navbar_versekey.button_book_up = UI_GET_ITEM(gxml, "eventbox9");
	navbar_versekey.button_book_down = UI_GET_ITEM(gxml, "eventbox6");
	navbar_versekey.button_chapter_up = UI_GET_ITEM(gxml, "eventbox8");
	navbar_versekey.button_chapter_down =
	    UI_GET_ITEM(gxml, "eventbox4");
	navbar_versekey.button_verse_up = UI_GET_ITEM(gxml, "eventbox7");
	navbar_versekey.button_verse_down = UI_GET_ITEM(gxml, "eventbox1");

	navbar_versekey.arrow_book_up = UI_GET_ITEM(gxml, "image12");
	navbar_versekey.arrow_book_down = UI_GET_ITEM(gxml, "image14");
	navbar_versekey.arrow_chapter_up = UI_GET_ITEM(gxml, "image8");
	navbar_versekey.arrow_chapter_down = UI_GET_ITEM(gxml, "image10");
	navbar_versekey.arrow_verse_up = UI_GET_ITEM(gxml, "image6");
	navbar_versekey.arrow_verse_down = UI_GET_ITEM(gxml, "image5");

	navbar_versekey.button_book_menu =
	    UI_GET_ITEM(gxml, "togglebutton_book");
	navbar_versekey.button_chapter_menu =
	    UI_GET_ITEM(gxml, "togglebutton_chapter");
	navbar_versekey.button_verse_menu =
	    UI_GET_ITEM(gxml, "togglebutton_verse");
	navbar_versekey.lookup_entry = UI_GET_ITEM(gxml, "entry_lookup");
	navbar_versekey.label_book_menu = UI_GET_ITEM(gxml, "label_book");
	navbar_versekey.label_chapter_menu =
	    UI_GET_ITEM(gxml, "label_chapter");
	navbar_versekey.label_verse_menu =
	    UI_GET_ITEM(gxml, "label_verse");
#if GTK_CHECK_VERSION(3, 4, 0)
	eventbox = UI_GET_ITEM(gxml, "eventbox_book");
	g_signal_connect((gpointer)eventbox, "scroll_event",
			 G_CALLBACK(on_button_verse_menu_book_scroll_event), NULL);

	eventbox = UI_GET_ITEM(gxml, "eventbox_chapter");
	g_signal_connect((gpointer)eventbox, "scroll_event",
			 G_CALLBACK(on_button_verse_menu_chapter_scroll_event),
			 NULL);

	eventbox = UI_GET_ITEM(gxml, "eventbox_verse");
	g_signal_connect((gpointer)eventbox, "scroll_event",
			 G_CALLBACK(on_button_verse_menu_verse_scroll_event), NULL);
#endif
	widgets.combo_bible_version = gtk_combo_box_text_new();
	gtk_widget_set_tooltip_text(widgets.combo_bible_version,
				    _("Cambiar de versión (mantiene el versículo enfocado)"));
	gtk_widget_set_valign(widgets.combo_bible_version, GTK_ALIGN_CENTER);
	/* las descripciones de módulo pueden ser bastante largas
	 * ("Biblia Platense (Straubinger)") y un GtkComboBoxText se
	 * ensancha solo para mostrar entera la más larga de la lista --
	 * en esta barra, ya angosta, eso desplazaba el resto de los
	 * controles. Topar el ancho pedido y elidir el texto lo mantiene
	 * compacto; el nombre completo sigue disponible al desplegar la
	 * lista o vía el tooltip. */
	gtk_widget_set_size_request(widgets.combo_bible_version, 150, -1);
	{
		GList *cells = gtk_cell_layout_get_cells(
		    GTK_CELL_LAYOUT(widgets.combo_bible_version));
		if (cells) {
			g_object_set(cells->data, "ellipsize",
				     PANGO_ELLIPSIZE_END, NULL);
			g_list_free(cells);
		}
	}
	gui_navbar_fill_version_combo();
	g_signal_connect(widgets.combo_bible_version, "changed",
			 G_CALLBACK(on_combo_bible_version_changed), NULL);
	gtk_widget_show(widgets.combo_bible_version);
	gtk_box_pack_start(GTK_BOX(navbar_versekey.navbar),
			   widgets.combo_bible_version, FALSE, FALSE, 4);
	/* al frente del todo, antes que los íconos de historial, para que
	 * sea lo primero visible de la barra en vez de quedar al final
	 * (donde se lo pidieron mover porque ahí pasaba desapercibido). */
	gtk_box_reorder_child(GTK_BOX(navbar_versekey.navbar),
			      widgets.combo_bible_version, 0);

	_connect_signals(navbar_versekey);

	return navbar_versekey.navbar;
}
