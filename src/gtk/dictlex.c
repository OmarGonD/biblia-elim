/*
 * Xiphos Bible Study Tool
 * dictlex.c - gui for commentary modules
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

#include "gui/dictlex.h"
#include "gui/diccionario.h"
#include "gui/bookmark_dialog.h"
#include "gui/bookmarks_treeview.h"
#include "gui/xiphos.h"
#include "gui/cipher_key_dialog.h"
#include "gui/dictlex_dialog.h"
#include "gui/main_window.h"
#include "gui/menu_popup.h"
#include "gui/sidebar.h"
#include "gui/find_dialog.h"
#include "gui/font_dialog.h"
#include "gui/widgets.h"
#include "gui/utilities.h"
#include "gui/search_sidebar.h"

#include "main/sword.h"
#include "main/settings.h"
#include "main/lists.h"
#include "main/url.hh"
#include "main/xml.h"

#include "gui/debug_glib_null.h"
#include <time.h>
void button_dict_back_clicked(GtkButton *button, gpointer user_data);
void button_dict_forward_clicked(GtkButton *button, gpointer user_data);
/******************************************************************************
 * externs
 */
extern gboolean dict_display_change;
extern gboolean isrunningSD; /* is the view dictionary dialog runing */

/******************************************************************************
 * static
 */

/******************************************************************************
 * Name
 *   gui_get_clipboard_text_for_lookup
 *
 * Synopsis
 *   #include "gui/dictlex.h"
 *
 *   void gui_get_clipboard_text_for_lookup (GObject *clipboard,
 *						 const gchar *text,
 *						 gpointer data)
 *
 * Description
 *    an ugly hack to get the selection from widget on a dbl click
 *    and display text in dictionary pane using default dictionary if set or
 *    current dictionary - this called by (gecko|wk)/Yelper.cpp 
 *    Yelper::ProcessMouseDblClickEvent (void* aEvent)
 *    also called by wk-html.c   button_press_handler() -- still ugly
 *
 * Return value
 *   void
 */

void gui_get_clipboard_text_for_lookup(GObject *clipboard,
				       GAsyncResult *result, gpointer data)
{
	char *key = NULL;
	gchar *dict = NULL;
	int len = 0;
	gchar *text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(clipboard),
						     result, NULL);

	if (text == NULL)
		return;
	XI_message(("src/gtk/dictlex.c: text =>%s<", text));

	key = g_strdelimit((char *)text, "&.,\"<>;:?", ' ');
	key = g_strstrip((char *)key);
	len = strlen(key);

	if (key[len - 1] == 's' || key[len - 1] == 'd')
		key[len - 1] = '\0';
	if (key[len - 1] == 'h' && key[len - 2] == 't' && key[len - 3] == 'e')
		key[len - 3] = '\0';

	if (settings.useDefaultDict)
		dict = g_strdup(settings.DefaultDict);
	else
		dict = g_strdup(settings.DictWindowModule);

	if (settings.havedict && dict && *dict)
		main_display_dictionary(dict, key);
	/* Léxico embebido: siempre disponible sin internet ni módulos extra. */
	gui_diccionario_mostrar(key);

	if (dict)
		g_free(dict);
	g_free(text);
}

/******************************************************************************
 * Name
 *   on_entryDictLookup_changed
 *
 * Synopsis
 *   #include "_dictlex.h"
 *
 *   void on_entryDictLookup_changed(GtkEditable * editable,
						       DL_DATA * d)
 *
 * Description
 *    look up text in dictionary entry widget
 *
 * Return value
 *   void
 */

void on_entryDictLookup_changed(GtkEditable *editable, gpointer data)
{
	main_dictionary_entry_changed(settings.DictWindowModule);
}

void dict_key_entry_changed(GtkEntry *entry, gpointer data)
{
	gchar *buf = NULL;

	buf = (gchar *)gtk_editable_get_text(GTK_EDITABLE(entry));
	XI_message(("dict_key_entry_changed: %s", buf));
	if (strlen(buf) < 2)
		return;

	main_display_dictionary(settings.DictWindowModule, buf);
}

void button_back_clicked(GtkButton *button, gpointer user_data)
{
	if (settings.havedict)
		main_dictionary_button_clicked(0);
}

void button_forward_clicked(GtkButton *button, gpointer user_data)
{
	if (settings.havedict)
		main_dictionary_button_clicked(1);
}

/******************************************************************************
 * Name
 *   dict_find_all_strongs
 *
 * Synopsis
 *   #include "gui/.h"
 *
 *   void dict_find_all_strongs (GtkWidget *widget, gpointer user_data)
 *
 * Description
 *   copy the number w/[HG] prefix as lemma to sidebar search to start
 *   search in MainWindowModule for all uses of this strong's number.
 *
 * Return value
 *   void
 */
void dict_find_all_strongs(GtkButton *button,
			   gpointer user_data)
{
	const gchar *feature, *key, *start, *lemma;
	gboolean hebrew, greek;

	/* we should be here iff dict displays a strong's dict. */
	feature = main_get_mod_config_entry(settings.DictWindowModule, "Feature");
	if (!feature)
		return;
	hebrew = !strcmp(feature, "HebrewDef");
	greek  = !strcmp(feature, "GreekDef");
	/* if we are here without _either_ heb or grk, we shouldn't be here. */
	if (!hebrew && !greek)
		return;

	/* get current dict key at its useful beginning. */
	key = gtk_editable_get_text(GTK_EDITABLE(widgets.entry_dict));
	for (start = key; start && (*start == '0'); ++start)
		/* nothing */ ;

	/* heb numbers use a pointless leading 0, grk does not. */
	if (hebrew && (start > key))
		--start;

	lemma = g_strdup_printf("lemma:%c%s", (hebrew ? 'H' : 'G'), start);
	gtk_editable_set_text(GTK_EDITABLE(ss.entrySearch), lemma);
	g_free((gpointer)lemma);

	/* artificial: no button or userdata context. */
	sidebar_on_search_button_clicked(NULL, NULL);
}

/******************************************************************************
 * Name
 *   menu_deactivate_callback
 *
 * Synopsis
 *   #include "gui/.h"
 *
 *   void menu_deactivate_callback (GtkWidget *widget, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void menu_deactivate_callback(GtkWidget *widget,
				     gpointer user_data)
{
	GtkWidget *menu_button;

	menu_button = GTK_WIDGET(user_data);

	gui_toggle_set_active(GTK_WIDGET(menu_button),
				     FALSE);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GuiButtonEvent *event,
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
					     GuiButtonEvent *event,
					     gpointer user_data)
{
	if (!settings.DictWindowModule ||
	    (*settings.DictWindowModule == '\0'))
		return 0;

	if ((event->type != GDK_BUTTON_PRESS) || event->button != 1)
		return FALSE;

	GMenuModel *menu =
	    main_dictionary_drop_down_new(settings.DictWindowModule,
					  settings.dictkey, widget);
	GtkWidget *popover;

	if (!menu)
		return FALSE;
	gtk_widget_grab_focus(widget);
	gui_toggle_set_active(GTK_WIDGET(widget), TRUE);
	popover = gui_popup_menu_model_at_widget(menu, widget);
	g_object_unref(menu);
	if (popover)
		g_signal_connect(popover, "closed",
				 G_CALLBACK(menu_deactivate_callback), widget);
	return TRUE;
}

static void
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, gpointer user_data)
{
	gui_menu_popup(html, settings.DictWindowModule, NULL);
}

GtkWidget *gui_create_dictionary_pane(void)
{
	GtkWidget *box_dict;
	GtkWidget *hbox2;
	GtkWidget *button10;
	GtkWidget *image1;
	GtkWidget *button11;
	GtkWidget *image2;
	GtkWidget *arrow1;
	GtkWidget *dict_drop_down;
	GtkWidget *img_back;
	GtkWidget *img_forward;
	

	UI_VBOX(box_dict, FALSE, 0);
	gtk_widget_show(box_dict);

	gui_widget_set_margins(box_dict, 1);

	UI_HBOX(hbox2, FALSE, 0);
	gtk_widget_show(hbox2);
	gtk_box_append(GTK_BOX(box_dict), hbox2);

	widgets.entry_dict = gtk_entry_new();
	gtk_widget_show(widgets.entry_dict);
	gui_box_pack(GTK_BOX(hbox2), widgets.entry_dict, TRUE, TRUE, 0);

	/* button to induce search for all occurrences of this word */
	/* initially hidden -- shown iff display is a strong's dict */
	widgets.all_strongs = gtk_button_new_from_icon_name("edit-find");
	gtk_widget_set_tooltip_text(GTK_WIDGET(widgets.all_strongs),
				    _("Do sidebar search for this Strong's number"));
	gtk_widget_hide(widgets.all_strongs);
	gtk_box_append(GTK_BOX(hbox2), widgets.all_strongs);

	dict_drop_down = gtk_toggle_button_new();
	gtk_widget_show(dict_drop_down);
	gtk_box_append(GTK_BOX(hbox2), dict_drop_down);

	arrow1 =
	    gtk_image_new_from_icon_name("open-menu-symbolic");
	gtk_widget_show(arrow1);
	gtk_button_set_child(GTK_BUTTON(dict_drop_down), arrow1);

	/* history button back/forward */
	widgets.button_dict_back = gtk_button_new();
	gtk_widget_show(widgets.button_dict_back);
	gtk_box_append(GTK_BOX(hbox2), widgets.button_dict_back);
	gtk_button_set_has_frame(GTK_BUTTON(widgets.button_dict_back), FALSE);
	gtk_widget_set_sensitive(widgets.button_dict_back, FALSE);
	gtk_widget_set_tooltip_text(widgets.button_dict_back, _("Go back in history"));
	img_back =
	    gtk_image_new_from_icon_name("go-previous-symbolic");
	gtk_widget_show(img_back);
	gtk_button_set_child(GTK_BUTTON(widgets.button_dict_back), img_back);

	widgets.button_dict_forward = gtk_button_new();
	gtk_widget_show(widgets.button_dict_forward);
	gtk_box_append(GTK_BOX(hbox2), widgets.button_dict_forward);
	gtk_button_set_has_frame(GTK_BUTTON(widgets.button_dict_forward), FALSE);
	gtk_widget_set_sensitive(widgets.button_dict_forward, FALSE);
	gtk_widget_set_tooltip_text(widgets.button_dict_forward, _("Go forward in history"));
	img_forward =
	    gtk_image_new_from_icon_name("go-next-symbolic");
	gtk_widget_show(img_forward);
	gtk_button_set_child(GTK_BUTTON(widgets.button_dict_forward), img_forward);

	button10 = gtk_button_new();
	gtk_widget_show(button10);
	gtk_box_append(GTK_BOX(hbox2), button10);
	gtk_button_set_has_frame(GTK_BUTTON(button10), FALSE);

	image1 =
	    gtk_image_new_from_icon_name("go-up-symbolic");
	gtk_widget_show(image1);
	gtk_button_set_child(GTK_BUTTON(button10), image1);

	button11 = gtk_button_new();
	gtk_widget_show(button11);
	gtk_box_append(GTK_BOX(hbox2), button11);
	gtk_button_set_has_frame(GTK_BUTTON(button11), FALSE);

	image2 =
	    gtk_image_new_from_icon_name("go-down-symbolic");
	gtk_widget_show(image2);
	gtk_button_set_child(GTK_BUTTON(button11), image2);


	widgets.html_dict =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, DICTIONARY_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(widgets.html_dict, "dictionary");
	gtk_widget_show(widgets.html_dict);
	gui_box_pack(GTK_BOX(box_dict), widgets.html_dict, TRUE, TRUE, 0);
	g_signal_connect((gpointer)widgets.html_dict,
			 "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb), NULL);

	gui_widget_on_button(GTK_WIDGET(dict_drop_down), GTK_PHASE_CAPTURE, (GuiButtonFunc)select_button_press_callback, NULL, NULL);
	g_signal_connect(G_OBJECT(widgets.entry_dict), "activate",
			 G_CALLBACK(dict_key_entry_changed), NULL);

	g_signal_connect((gpointer)widgets.button_dict_back, "clicked",
			G_CALLBACK(button_dict_back_clicked), NULL);
	g_signal_connect((gpointer)widgets.button_dict_forward, "clicked",
			G_CALLBACK(button_dict_forward_clicked), NULL);
	g_signal_connect((gpointer)button10, "clicked",
			 G_CALLBACK(button_back_clicked), NULL);
	g_signal_connect((gpointer)button11, "clicked",
			 G_CALLBACK(button_forward_clicked), NULL);

	g_signal_connect((gpointer)widgets.all_strongs, "clicked",
			 G_CALLBACK(dict_find_all_strongs), NULL);

	return box_dict;
}
/*callbacks for dict history nav */

void button_dict_back_clicked(GtkButton *button, gpointer user_data)
{
    main_dict_history_back();
}

void button_dict_forward_clicked(GtkButton *button, gpointer user_data)
{
    main_dict_history_forward();
}

/*callbacks for devotional nav */
void devot_key_entry_changed(GtkEntry *entry, gpointer data)
{
    gchar *buf = (gchar *)gtk_editable_get_text(GTK_EDITABLE(entry));
    if (strlen(buf) < 4)   /* MM.DD = 5 chars minimum */
        return;
    main_display_devotional(widgets.html_devotional);
}

void button_devot_back_clicked(GtkButton *button, gpointer user_data)
{
    if (!settings.devotionalmod) return;
	main_devotional_button_clicked(0);
}

void button_devot_forward_clicked(GtkButton *button, gpointer user_data)
{
	if (!settings.devotionalmod) return;
	main_devotional_button_clicked(1);
}

/* Convert MM.DD with local time ex: "25 février" */
static gchar *devot_date_to_local(const gchar *mmdd)
{
    if (!mmdd || strlen(mmdd) != 5) return g_strdup("--");
	int month = atoi(mmdd);        /* MM */
	int day   = atoi(mmdd + 3);    /* DD */
	if (month < 1 || month > 12 || day < 1 || day > 31)
		return g_strdup("--");

	/* local format via strftime */
	struct tm t = {0};
	t.tm_mon  = month - 1;
	t.tm_mday = day;
	t.tm_year = 2000 - 1900;
	mktime(&t);

	gchar buf[64];
	strftime(buf, sizeof(buf), "%e %B", &t);  /* "25 février" in fr, "Feb 25" in en */
	return g_strdup(g_strstrip(buf));
}

/* Callback */
static void on_calendar_day_selected(GtkCalendar *calendar, gpointer user_data)
{
	GDateTime *date = gtk_calendar_get_date(calendar);
	gint month = g_date_time_get_month(date); /* 1-12 */
	gint day = g_date_time_get_day_of_month(date);

	g_date_time_unref(date);

	gchar mmdd[6];
	g_snprintf(mmdd, sizeof(mmdd), "%02d.%02d", month, day);

	if (widgets.entry_devotional)
		gtk_editable_set_text(GTK_EDITABLE(widgets.entry_devotional), mmdd);

	/* update button label */
	gchar *local = devot_date_to_local(mmdd);
	gtk_button_set_label(GTK_BUTTON(widgets.button_devotional_date), local);
	g_free(local);

	/* close the popover */
	gtk_popover_popdown(GTK_POPOVER(widgets.popover_devotional));

	/* display devotional */
	main_display_devotional(widgets.html_devotional);
}

/* A double press on a day picks it (GTK 3's "day-selected-double-click"). */
static void on_calendar_double_press(GtkGestureClick *gesture, gint n_press,
				     gdouble x, gdouble y, gpointer user_data)
{
	GtkWidget *calendar = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));

	if (n_press == 2)
		on_calendar_day_selected(GTK_CALENDAR(calendar), user_data);
}

/* Callback : click on date button → open/close the popover */
static void on_button_devotional_date_clicked(GtkButton *button, gpointer user_data)
{
	/* sync calendar with current date */
	const gchar *mmdd = gtk_editable_get_text(GTK_EDITABLE(widgets.entry_devotional));
	if (mmdd && strlen(mmdd) == 5) {
		int month = atoi(mmdd);
		int day   = atoi(mmdd + 3);
		GDateTime *now = g_date_time_new_now_local();
		GDateTime *date = g_date_time_new_local(g_date_time_get_year(now),
							month, day, 0, 0, 0);
		if (date) {
			gtk_calendar_select_day(GTK_CALENDAR(widgets.calendar_devotional),
						date);
			g_date_time_unref(date);
		}
		g_date_time_unref(now);
	}
	gtk_popover_popup(GTK_POPOVER(widgets.popover_devotional));
}

GtkWidget *gui_create_devotional_pane(void)
{
	GtkWidget *box_devot;
	GtkWidget *hbox;
	GtkWidget *button_prev, *button_next;
	GtkWidget *image_prev, *image_next;

	UI_VBOX(box_devot, FALSE, 0);
	gtk_widget_show(box_devot);
	gui_widget_set_margins(box_devot, 1);

	UI_HBOX(hbox, FALSE, 0);
	gtk_widget_show(hbox);
	gtk_box_append(GTK_BOX(box_devot), hbox);

	widgets.entry_devotional = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(widgets.entry_devotional), 5);
	widgets.button_devotional_date = gtk_button_new_with_label("--");
	gtk_widget_show(widgets.button_devotional_date);
	gtk_widget_set_tooltip_text(widgets.button_devotional_date,
								_("Click to choose a date"));
	gtk_box_append(GTK_BOX(hbox), widgets.button_devotional_date);
	/* GtkPopover stick to the button */
	widgets.popover_devotional = gui_popover_new(widgets.button_devotional_date);
	gtk_popover_set_position(GTK_POPOVER(widgets.popover_devotional),
                             GTK_POS_BOTTOM);
	/* GtkCalendar in the popover */
	widgets.calendar_devotional = gtk_calendar_new();
	gtk_widget_show(widgets.calendar_devotional);
	gtk_popover_set_child(GTK_POPOVER(widgets.popover_devotional), widgets.calendar_devotional);

	/* prev button */
	button_prev = gtk_button_new();
	gtk_widget_show(button_prev);
	gtk_box_append(GTK_BOX(hbox), button_prev);
	gtk_button_set_has_frame(GTK_BUTTON(button_prev), FALSE);
	image_prev =
		gtk_image_new_from_icon_name("go-up-symbolic");
	gtk_widget_show(image_prev);
	gtk_button_set_child(GTK_BUTTON(button_prev), image_prev);

	/* next button */
	button_next = gtk_button_new();
	gtk_widget_show(button_next);
	gtk_box_append(GTK_BOX(hbox), button_next);
	gtk_button_set_has_frame(GTK_BUTTON(button_next), FALSE);
	image_next =
		gtk_image_new_from_icon_name("go-down-symbolic");
	gtk_widget_show(image_next);
	gtk_button_set_child(GTK_BUTTON(button_next), image_next);


	widgets.html_devotional =
		GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(widgets.html_devotional, "devotional");
	gtk_widget_show(widgets.html_devotional);
	gui_box_pack(GTK_BOX(box_devot), widgets.html_devotional, TRUE, TRUE, 0);

	/* signaux */
	g_signal_connect(G_OBJECT(widgets.button_devotional_date), "clicked",
					 G_CALLBACK(on_button_devotional_date_clicked), NULL);
	/* GTK 4 has no "day-selected-double-click": watch the presses. */
	{
		GtkGesture *doble = gtk_gesture_click_new();

		gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(doble),
							   GTK_PHASE_CAPTURE);
		g_signal_connect(doble, "pressed",
				 G_CALLBACK(on_calendar_double_press), NULL);
		gtk_widget_add_controller(widgets.calendar_devotional,
					  GTK_EVENT_CONTROLLER(doble));
	}
	g_signal_connect(G_OBJECT(widgets.entry_devotional), "activate",
					 G_CALLBACK(devot_key_entry_changed), NULL);
	g_signal_connect((gpointer)button_prev, "clicked",
					 G_CALLBACK(button_devot_back_clicked), NULL);
	g_signal_connect((gpointer)button_next, "clicked",
					 G_CALLBACK(button_devot_forward_clicked), NULL);

	return box_devot;
}

//******  end of file  ******/
