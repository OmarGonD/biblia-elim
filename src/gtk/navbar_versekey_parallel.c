/*
 * Xiphos Bible Study Tool
 * navbar_versekey_parallel.c - navigation bar for the parallel dialog
 *
 * Copyright (C) 2007-2026 Xiphos Developer Team
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

#include "gui/navbar_versekey_parallel.h"
#include "gui/utilities.h"

#include "main/navbar_versekey.h"
#include "main/parallel_view.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"

#include "gui/debug_glib_null.h"
#include "main/xml.h"
#include "gui/preferences_dialog.h"
#include "navbar_entry_reference.h"

NAVBAR_VERSEKEY navbar_parallel;
gboolean sync_on;


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
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	main_versekey_popup_book(navbar_parallel, NB_PARALLEL,
				 NULL, NULL, widget);
	return TRUE;
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
						     GdkEventButton *event,
						     gpointer user_data)
{
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	main_versekey_popup_chapter(navbar_parallel, NB_PARALLEL,
				    NULL, NULL, widget);
	return TRUE;
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
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	main_versekey_popup_verse(navbar_parallel, NB_PARALLEL,
				    NULL, NULL, widget);
	return TRUE;
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
	const gchar *buf = gtk_entry_get_text(entry);
	NavbarEntryReference reference;
	if (buf == NULL)
		return;

	reference = navbar_entry_reference_parse(buf);
	settings.special_anchor = reference.anchor;

	/* gross.  we need a valid key.
	 * but we have multiple modules whose v11n may not even be the same.
	 * fall back 10 yards and punt: arbitrarily take the 1st one.
	 * if there aren't any, use main window bible.
	 */
	gchar *valid_key =
	    main_get_valid_key((settings.parallel_list ? settings.parallel_list[0]
						       : settings.MainWindowModule),
			       reference.key);
	g_free(settings.cvparallel);
	settings.cvparallel = valid_key;

	if (settings.cvparallel == NULL) {
		settings.special_anchor = NULL;
		navbar_entry_reference_clear(&reference);
		return;
	}

	navbar_parallel.valid_key = TRUE;
	main_navbar_versekey_set(navbar_parallel, settings.cvparallel);
	if (settings.dockedInt)
		main_update_parallel_page();
	else
		main_update_parallel_page_detached();
	if (sync_on) {
		/* cvparallel was validated in the 1st parallel module; the
		 * main window reads its own Bible, so carry the reference
		 * over, or leave the main window alone without one. */
		const char *control = settings.parallel_list
					  ? settings.parallel_list[0]
					  : settings.MainWindowModule;
		const char *real_control = main_abbrev_to_name(control);
		gchar *main_key = main_reference_for_module(
		    real_control ? real_control : control,
		    settings.cvparallel, settings.MainWindowModule);
		if (main_key) {
			const gchar *main_window_url =
			    g_strdup_printf("sword:///%s%s",
					    main_key,
					    reference.anchor ? reference.anchor : "");
			sword_uri(main_window_url, TRUE);
			g_free((gchar *)main_window_url);
			g_free(main_key);
		}
	}
	settings.special_anchor = NULL;
	navbar_entry_reference_clear(&reference);
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
	main_navbar_versekey_spin_verse(navbar_parallel,
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
	main_navbar_versekey_spin_chapter(navbar_parallel,
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
	main_navbar_versekey_spin_book(navbar_parallel,
				       event->scroll.direction);
	return FALSE;
}

static void sync_with_main(GtkToggleButton *button, gpointer data)
{
	sync_on = FALSE;
	if (gtk_toggle_button_get_active(button)) {
		sync_on = TRUE;
		gchar *buf = (gchar *)main_url_encode(settings.currentverse);
		if (buf && (strlen(buf) > 3)) {
			gchar *url =
				g_strdup_printf("passagestudy.jsp?action=showParallel&"
						"type=verse&value=%s",
						buf); // xml_get_value("keys", "verse")));
			main_url_handler(url, TRUE);
			g_free(url);
		}
	}
}

/******************************************************************************
 * Name
 *   on_up_enter_notify_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_up_enter_notify_event(GtkWidget * widget, GdkEventCrossing * event,
 *                                       gpointer  user_data)
 *
 * Description
 *   mimic a button by hiding/showing arrow pixmaps when mouse enters spin
 *   buttons
 *
 * Return value
 *   gboolean
 */

static gboolean on_up_enter_notify_event(GtkWidget *widget,
					 GdkEventCrossing *event,
					 gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data)) {
	case BOOK_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_book_up);
		gtk_widget_show(navbar_parallel.arrow_book_up_box);
		break;
	case CHAPTER_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_chapter_up);
		gtk_widget_show(navbar_parallel.arrow_chapter_up_box);
		break;
	case VERSE_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_verse_up);
		gtk_widget_show(navbar_parallel.arrow_verse_up_box);
		break;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_down_enter_notify_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_down_enter_notify_event(GtkWidget * widget, GdkEventCrossing * event,
                                        gpointer user_data)
 *
 * Description
 *   mimic a button by hiding/showing arrow pixmaps when mouse enters spin
 *   buttons
 *
 * Return value
 *   gboolean
 */

static gboolean on_down_enter_notify_event(GtkWidget *widget,
					   GdkEventCrossing *event,
					   gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data)) {
	case BOOK_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_book_down);
		gtk_widget_show(navbar_parallel.arrow_book_down_box);
		break;
	case CHAPTER_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_chapter_down);
		gtk_widget_show(navbar_parallel.arrow_chapter_down_box);
		break;
	case VERSE_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_verse_down);
		gtk_widget_show(navbar_parallel.arrow_verse_down_box);
		break;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_up_leave_notify_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_up_leave_notify_event(GtkWidget * widget, GdkEventCrossing * event,
                                        gpointer user_data)
 *
 * Description
 *   mimic a button by hiding/showing arrow pixmaps when mouse leaves spin
 *   buttons
 *
 * Return value
 *   gboolean
 */

static gboolean on_up_leave_notify_event(GtkWidget *widget,
					 GdkEventCrossing *event,
					 gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data)) {
	case BOOK_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_book_up_box);
		gtk_widget_show(navbar_parallel.arrow_book_up);
		break;
	case CHAPTER_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_chapter_up_box);
		gtk_widget_show(navbar_parallel.arrow_chapter_up);
		break;
	case VERSE_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_verse_up_box);
		gtk_widget_show(navbar_parallel.arrow_verse_up);
		break;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_down_leave_notify_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_down_leave_notify_event(GtkWidget * widget, GdkEventCrossing * event,
                                        gpointer user_data)
 *
 * Description
 *   mimic a button by hiding/showing arrow pixmaps when mouse leaves spin
 *   buttons
 *
 * Return value
 *   gboolean
 */

static gboolean on_down_leave_notify_event(GtkWidget *widget,
					   GdkEventCrossing *event,
					   gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data)) {
	case BOOK_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_book_down_box);
		gtk_widget_show(navbar_parallel.arrow_book_down);
		break;
	case CHAPTER_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_chapter_down_box);
		gtk_widget_show(navbar_parallel.arrow_chapter_down);
		break;
	case VERSE_BUTTON:
		gtk_widget_hide(navbar_parallel.arrow_verse_down_box);
		gtk_widget_show(navbar_parallel.arrow_verse_down);
		break;
	}
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
		main_navbar_versekey_spin_book(navbar_parallel, 0);
		break;
	case CHAPTER_BUTTON:
		main_navbar_versekey_spin_chapter(navbar_parallel, 0);
		break;
	case VERSE_BUTTON:
		main_navbar_versekey_spin_verse(navbar_parallel, 0);
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
		main_navbar_versekey_spin_book(navbar_parallel, 1);
		break;
	case CHAPTER_BUTTON:
		main_navbar_versekey_spin_chapter(navbar_parallel, 1);
		break;
	case VERSE_BUTTON:
		main_navbar_versekey_spin_verse(navbar_parallel, 1);
		break;
	}
	return FALSE;
}

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
	/*     */
	g_signal_connect((gpointer)navbar.button_book_up,
			 "enter_notify_event",
			 G_CALLBACK(on_up_enter_notify_event),
			 GINT_TO_POINTER(BOOK_BUTTON));
	g_signal_connect((gpointer)navbar.button_book_up,
			 "leave_notify_event",
			 G_CALLBACK(on_up_leave_notify_event),
			 GINT_TO_POINTER(BOOK_BUTTON));

	g_signal_connect((gpointer)navbar.button_book_down,
			 "enter_notify_event",
			 G_CALLBACK(on_down_enter_notify_event),
			 GINT_TO_POINTER(BOOK_BUTTON));
	g_signal_connect((gpointer)navbar.button_book_down,
			 "leave_notify_event",
			 G_CALLBACK(on_down_leave_notify_event),
			 GINT_TO_POINTER(BOOK_BUTTON));

	/*    */
	g_signal_connect((gpointer)navbar.button_chapter_up,
			 "enter_notify_event",
			 G_CALLBACK(on_up_enter_notify_event),
			 GINT_TO_POINTER(CHAPTER_BUTTON));
	g_signal_connect((gpointer)navbar.button_chapter_up,
			 "leave_notify_event",
			 G_CALLBACK(on_up_leave_notify_event),
			 GINT_TO_POINTER(CHAPTER_BUTTON));

	g_signal_connect((gpointer)navbar.button_chapter_down,
			 "enter_notify_event",
			 G_CALLBACK(on_down_enter_notify_event),
			 GINT_TO_POINTER(CHAPTER_BUTTON));
	g_signal_connect((gpointer)navbar.button_chapter_down,
			 "leave_notify_event",
			 G_CALLBACK(on_down_leave_notify_event),
			 GINT_TO_POINTER(CHAPTER_BUTTON));

	/*    */
	g_signal_connect((gpointer)navbar.button_verse_up,
			 "enter_notify_event",
			 G_CALLBACK(on_up_enter_notify_event),
			 GINT_TO_POINTER(VERSE_BUTTON));
	g_signal_connect((gpointer)navbar.button_verse_up,
			 "leave_notify_event",
			 G_CALLBACK(on_up_leave_notify_event),
			 GINT_TO_POINTER(VERSE_BUTTON));

	g_signal_connect((gpointer)navbar.button_verse_down,
			 "enter_notify_event",
			 G_CALLBACK(on_down_enter_notify_event),
			 GINT_TO_POINTER(VERSE_BUTTON));
	g_signal_connect((gpointer)navbar.button_verse_down,
			 "leave_notify_event",
			 G_CALLBACK(on_down_leave_notify_event),
			 GINT_TO_POINTER(VERSE_BUTTON));

	g_signal_connect((gpointer)navbar.button_sync,
			 "toggled", G_CALLBACK(G_CALLBACK(sync_with_main)),
			 NULL);
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

static void on_parallel_set_activate(GSimpleAction *action, GVariant *state,
				     gpointer user_data)
{
	const gchar *name = g_variant_get_string(state, NULL);
	gchar **modules;
	(void)user_data;

	modules = get_parallel_set(name);
	if (!modules)
		return;
	g_simple_action_set_state(action, state);

	g_strfreev(settings.parallel_list);
	settings.parallel_list = modules;

	if (settings.parallel_set_current)
		g_free(settings.parallel_set_current);
	settings.parallel_set_current = g_strdup(name);
	xml_set_or_create_value("modules", "parallel_set_current", name);
	/* update Sets button label */
	gchar *display = key_to_name(name);
	gchar *label = g_strdup_printf(_("Set: %s"), display);
	g_free(display);	gtk_button_set_label(GTK_BUTTON(navbar_parallel.button_sets), label);
	g_free(label);
	xml_save_settings_doc(settings.fnconfigure);
	
	main_update_parallel_page();
	if (!settings.dockedInt && settings.parallel_list && settings.parallel_list[0]) {
		gui_navbar_parallel_set_module(settings.parallel_list[0]);
		gui_reassign_strdup(&settings.cvparallel, settings.currentverse);
		main_update_parallel_page_detached();
	}

}

static void on_parallel_sets_manage_clicked(GSimpleAction *action,
					    GVariant *parameter,
					    gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	gui_setup_preferences_dialog();
	gui_prefs_goto_parallel_page();
}

/******************************************************************************
 * Name
 *   on_parallel_sets_button_clicked
 *
 * Description
 *   show popup menu of saved parallel sets
 *
 * Return value
 *   void
 */
static void parallel_sets_menu_setup(GtkWidget *widget)
{
	gchar **names;

	if (!settings.parallel_set_names || !*settings.parallel_set_names)
		return;

	GMenu *menu = g_menu_new();
	GMenu *sets = g_menu_new();
	names = g_strsplit(settings.parallel_set_names, ",", -1);

	for (gint i = 0; names[i]; ++i) {
		gchar *display = key_to_name(names[i]);
		GMenuItem *item = g_menu_item_new(display, NULL);
		g_menu_item_set_action_and_target(item, "conjuntos.elegir", "s",
					      names[i]);
		g_menu_append_item(sets, item);
		g_object_unref(item);
		g_free(display);
	}
	g_strfreev(names);
	g_menu_append_section(menu, NULL, G_MENU_MODEL(sets));
	g_object_unref(sets);
	GMenu *manage = g_menu_new();
	g_menu_append(manage, _("Manage..."), "conjuntos.administrar");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(manage));
	g_object_unref(manage);

	GSimpleActionGroup *actions = g_simple_action_group_new();
	GSimpleAction *choose = g_simple_action_new_stateful(
	    "elegir", G_VARIANT_TYPE_STRING,
	    g_variant_new_string(settings.parallel_set_current
				     ? settings.parallel_set_current : ""));
	g_signal_connect(choose, "change-state",
			 G_CALLBACK(on_parallel_set_activate), NULL);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(choose));
	g_object_unref(choose);
	GSimpleAction *manage_action = g_simple_action_new("administrar", NULL);
	g_signal_connect(manage_action, "activate",
			 G_CALLBACK(on_parallel_sets_manage_clicked), NULL);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(manage_action));
	g_object_unref(manage_action);
	gtk_widget_insert_action_group(widget, "conjuntos", G_ACTION_GROUP(actions));
	g_object_unref(actions);
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(widget), G_MENU_MODEL(menu));
	g_object_unref(menu);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/navbar_versekey_editor.h"
 *
 *  GtkWidget *
 *
 * Description
 *   create a new Bible navigation toolbar and return it
 *
 * Return value
 *   GtkWidget *
 */
 
GtkWidget *gui_navbar_versekey_parallel_new(void)
{
	GtkBuilder *gxml;
#if GTK_CHECK_VERSION(3, 4, 0)
	GtkWidget *eventbox;
#endif

/* build the widget */
	gxml = elim_gtk_builder_new();
	gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/navbar_versekey.gtkbuilder", NULL);
	navbar_parallel.dialog = TRUE;
	navbar_parallel.module_name =
	    g_string_new(settings.MainWindowModule);
	navbar_parallel.key = g_string_new(settings.currentverse);
	navbar_parallel.valid_key = TRUE;

	navbar_parallel.navbar = UI_GET_ITEM(gxml, "navbar");
	navbar_parallel.button_history_back =
	    UI_GET_ITEM(gxml, "button_history_back");
	navbar_parallel.button_history_next =
	    UI_GET_ITEM(gxml, "button_history_foward");
	navbar_parallel.button_history_menu =
	    UI_GET_ITEM(gxml, "togglebutton_history_list");

	navbar_parallel.button_sync =
	    UI_GET_ITEM(gxml, "togglebutton_sync");

	gtk_widget_show(navbar_parallel.button_sync);
	gtk_widget_set_tooltip_text(navbar_parallel.button_sync,
				    _("Synchronize this window's scrolling with the main window"));
	gtk_widget_hide(navbar_parallel.button_history_back);
	gtk_widget_hide(navbar_parallel.button_history_next);
	gtk_widget_hide(navbar_parallel.button_history_menu);
	navbar_parallel.button_book_up = UI_GET_ITEM(gxml, "eventbox9");
	navbar_parallel.button_book_down = UI_GET_ITEM(gxml, "eventbox6");
	navbar_parallel.button_chapter_up = UI_GET_ITEM(gxml, "eventbox8");
	navbar_parallel.button_chapter_down =
	    UI_GET_ITEM(gxml, "eventbox4");
	navbar_parallel.button_verse_up = UI_GET_ITEM(gxml, "eventbox7");
	navbar_parallel.button_verse_down = UI_GET_ITEM(gxml, "eventbox1");

	navbar_parallel.arrow_book_up_box = UI_GET_ITEM(gxml, "image13");
	navbar_parallel.arrow_book_up = UI_GET_ITEM(gxml, "image12");
	navbar_parallel.arrow_book_down_box = UI_GET_ITEM(gxml, "image15");
	navbar_parallel.arrow_book_down = UI_GET_ITEM(gxml, "image14");
	navbar_parallel.arrow_chapter_up_box = UI_GET_ITEM(gxml, "image9");
	navbar_parallel.arrow_chapter_up = UI_GET_ITEM(gxml, "image8");
	navbar_parallel.arrow_chapter_down_box =
	    UI_GET_ITEM(gxml, "image11");
	navbar_parallel.arrow_chapter_down = UI_GET_ITEM(gxml, "image10");
	navbar_parallel.arrow_verse_up_box = UI_GET_ITEM(gxml, "image7");
	navbar_parallel.arrow_verse_up = UI_GET_ITEM(gxml, "image6");
	navbar_parallel.arrow_verse_down_box =
	    UI_GET_ITEM(gxml, "image16");
	navbar_parallel.arrow_verse_down = UI_GET_ITEM(gxml, "image5");

	navbar_parallel.button_book_menu =
	    UI_GET_ITEM(gxml, "togglebutton_book");
	navbar_parallel.button_chapter_menu =
	    UI_GET_ITEM(gxml, "togglebutton_chapter");
	navbar_parallel.button_verse_menu =
	    UI_GET_ITEM(gxml, "togglebutton_verse");
	navbar_parallel.lookup_entry = UI_GET_ITEM(gxml, "entry_lookup");
	navbar_parallel.label_book_menu = UI_GET_ITEM(gxml, "label_book");
	navbar_parallel.label_chapter_menu =
	    UI_GET_ITEM(gxml, "label_chapter");
	navbar_parallel.label_verse_menu =
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
	_connect_signals(navbar_parallel);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(navbar_parallel.button_sync),
				     settings.linkedtabs);
	/* parallel sets button */
	gchar *sets_label;
	if (settings.parallel_set_current && *settings.parallel_set_current) {
		gchar *display = key_to_name(settings.parallel_set_current);
		sets_label = g_strdup_printf(_("Set: %s"), display);
		g_free(display);
	} else {
		sets_label = g_strdup_printf(_("Set: %s"), "—");
	}
	/* remove old button if it exists (navbar widget is recreated each call) */
	if (navbar_parallel.button_sets) {
		gtk_widget_destroy(navbar_parallel.button_sets);
		navbar_parallel.button_sets = NULL;
	}
	GtkWidget *button_sets = gtk_menu_button_new();
	gtk_button_set_label(GTK_BUTTON(button_sets), sets_label);
	gtk_widget_set_tooltip_text(button_sets, _("Switch parallel module set"));
	gtk_widget_show(button_sets);
	navbar_parallel.button_sets = button_sets;
	parallel_sets_menu_setup(button_sets);
	gtk_box_pack_end(GTK_BOX(navbar_parallel.navbar), button_sets,
			 FALSE, FALSE, 2);
	g_free(sets_label);
	return navbar_parallel.navbar;
}

/******************************************************************************
 * Name
 * gui_navbar_parallel_set_module
 *
 */


void gui_navbar_parallel_set_module(const gchar *module_name)
{
	if (module_name)
		navbar_parallel.module_name = g_string_assign(navbar_parallel.module_name, module_name);
}
