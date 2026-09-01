/*
 * Xiphos Bible Study Tool
 * main_window.c - main window gui
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
#include <glib/gi18n.h>

#include "xiphos_html/xiphos_html.h"
#include "main/sword.h"
#include "main/settings.h"
#include "main/interlineal.h"
#include "main/xml.h"
#include "main/search_dialog.h"
#include "main/url.hh"
#include "main/biblesync_glue.h"

#include "biblesync/biblesync-version.hh"

#include "gui/xiphos.h"
#include "gui/main_window.h"
#include "gui/main_menu.h"
#include "gui/sidebar.h"
#include "gui/utilities.h"
#include "gui/bibletext.h"
#include "gui/lectura_sync.h"
#include "gui/parallel_view.h"
#include "main/lists.h"
#include "main/parallel_view.h"
#include "gui/commentary.h"
#include "gui/gbs.h"
#include "gui/dialog.h"
#include "gui/find_dialog.h"
#include "gui/dictlex.h"
#include "gui/search_dialog.h"
#include "gui/commentary_dialog.h"
#include "gui/bibletext_dialog.h"
#include "gui/bookmark_dialog.h"
#include "gui/search_dialog.h"
#include "gui/navbar_versekey.h"
#include "gui/notas_verso.h"
#include "gui/tabbed_browser.h"
#include "gui/widgets.h"
#include "gui/tabbed_browser.h"
#include "gui/menu_popup.h"
#include "gui/preferences_dialog.h"
#include "gui/interlineal.h"

#include "editor/slib-editor.h"

#include "gui/debug_glib_null.h"

/* X keyboard #definitions, to handle shortcuts */
/* we must define the categories of #definitions we need. */
#define XK_MISCELLANY
#define XK_LATIN1
#define XK_XKB_KEYS
#include <X11/keysymdef.h>

WIDGETS widgets;

extern gboolean shift_key_pressed;

static int main_window_created = FALSE;
static gboolean switching_dict_tab = FALSE;
static GtkWidget *header_menu = NULL;
static GtkWidget *reading_exit_button = NULL;
static GtkWidget *reading_compare_button = NULL;
static GtkWidget *reading_compare_pick = NULL;
static guint reading_mode_place_src = 0;
static gulong reading_mode_wse_id = 0;
static guint reading_mode_hover_hide_src = 0;
static gulong reading_mode_motion_id = 0;
static gulong reading_mode_toolbar_enter_id = 0;
static gulong reading_mode_toolbar_leave_id = 0;
static gulong reading_mode_alloc_id = 0;

static void on_reading_mode_button_toggled(GtkToggleButton *button, gpointer data);
static void on_reading_compare_toggled(GtkToggleButton *button, gpointer data);
static void on_reading_compare_pick(GtkButton *button, gpointer data);
static gboolean on_open_bible_icon_draw(GtkWidget *widget, cairo_t *cr, gpointer data);
static gboolean reading_mode_keep_place(gpointer data);
static gboolean reading_mode_on_window_state(GtkWidget *widget, GdkEventWindowState *event, gpointer data);
static void reading_mode_float_toolbar(gboolean floating, GtkTextView *view);

gboolean gui_main_window_ready(void)
{
	return main_window_created;
}

/******************************************************************************
 * Name
 *  gui_show_hide_texts
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void gui_show_hide_texts(gboolean choice)
 *
 * Description
 *    Show/hide bible texts
 *
 * Return value
 *   void
 */

void gui_show_hide_texts(gboolean choice)
{
	settings.showtexts = choice;
	gui_tab_set_showtexts(choice);
	gui_set_tab_label(settings.currentverse, TRUE);
	if (choice == FALSE) {
		if (main_window_created)
			gtk_widget_hide(widgets.vpaned);
		xml_set_value("Xiphos", "misc", "showtexts", "0");
	} else {
		if (main_window_created) {
			gtk_widget_show(widgets.vpaned);
			if (!settings.reading_mode)
				gtk_widget_show(widgets.nav_toolbar);
		}
		xml_set_value("Xiphos", "misc", "showtexts", "1");
	}
	/*if (main_window_created)
	   gui_set_bible_comm_layout(); */
}

/******************************************************************************
 * Name
 *  gui_show_hide_preview
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void gui_show_hide_preview(gboolean choice)
 *
 * Description
 *    Show/hide bible texts
 *
 * Return value
 *   void
 */

void gui_show_hide_preview(gboolean choice)
{
	settings.showpreview = choice;
	gui_tab_set_showpreview(choice);
	if (choice == FALSE) {
		if (main_window_created)
			gtk_widget_hide(widgets.box_side_preview);
		gtk_widget_hide(widgets.vbox_previewer);
		xml_set_value("Xiphos", "misc", "showpreview", "0");
	} else {
		if (main_window_created && !settings.reading_mode) {
			if (settings.show_previewer_in_sidebar)
				gtk_widget_show(widgets.box_side_preview);
			else
				gtk_widget_show(widgets.vbox_previewer);
		}

		xml_set_value("Xiphos", "misc", "showpreview", "1");
	}
}

/******************************************************************************
 * Name
 *  gui_show_hide_comms
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void gui_show_hide_comms(gboolean choice)
 *
 * Description
 *    Show/hide Commentaries
 *
 * Return value
 *   void
 */

void gui_show_hide_comms(gboolean choice)
{
	settings.showcomms = choice;
	gui_tab_set_showcomms(choice);
	gui_set_tab_label(settings.currentverse, TRUE);
	if (choice == FALSE) {
		if (main_window_created)
			gtk_widget_hide(widgets.notebook_comm_book);
		xml_set_value("Xiphos", "misc", "showcomms", "0");
	} else {
		if (main_window_created && !settings.reading_mode)
			gtk_widget_show(widgets.notebook_comm_book);
		xml_set_value("Xiphos", "misc", "showcomms", "1");
	}
	if (main_window_created && !settings.reading_mode)
		gui_set_bible_comm_layout();
}

/******************************************************************************
 * Name
 *  gui_show_hide_dicts
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void gui_show_hide_dicts(gboolean choice)
 *
 * Description
 *    Show/hide Dictionaries-Lexicons
 *
 * Return value
 *   void
 */

void gui_show_hide_dicts(gboolean choice)
{
	/* El panel Diccionario/Devocional ya no tiene punto de entrada
	 * visible en el menú (ver ui/xi-menus.gtkbuilder), pero varios
	 * lugares (memoria por pestaña en tabbed_browser.c, restauración
	 * de sesión) todavía invocan esta función con un "1" heredado de
	 * antes del cambio. Forzar acá, en el único punto por el que
	 * pasan todos, evita que cualquiera de ellos lo reabra. */
	choice = FALSE;
	settings.showdicts = choice;
	gui_tab_set_showdicts(choice);
	gui_set_tab_label(settings.currentverse, TRUE);
	if (choice == FALSE) {
		if (main_window_created)
			gtk_widget_hide(widgets.notebook_dict_devot);
		xml_set_value("Xiphos", "misc", "showdicts", "0");
	} else {
		if (main_window_created && !settings.reading_mode)
			gtk_widget_show(widgets.notebook_dict_devot);
		xml_set_value("Xiphos", "misc", "showdicts", "1");
	}
	if (main_window_created && !settings.reading_mode)
		gui_set_bible_comm_layout();
}

/******************************************************************************
 * Name
 *  gui_toggle_reading_mode
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void gui_toggle_reading_mode(gboolean choice)
 *
 * Description
 *   Distraction-free reading: hide the sidebar, the open-passage tab
 *   row, the previewer and the navigation toolbar, leaving just the
 *   text -- and take the whole screen (real window fullscreen, not
 *   just maximized), widening the text margins and line spacing a
 *   touch so the page reads like an e-reader instead of a stretched
 *   toolbar-less window. Does not touch the individual show/hide
 *   *settings* for those panes -- turning reading mode back off
 *   restores whatever the user had before, rather than forcing
 *   everything back on. Keeps the View-menu checkbox and the
 *   header-bar reading-mode button in sync no matter which of the
 *   three entry points (menu, button, Ctrl+Shift+F) triggered it.
 *
 * Return value
 *   void
 */

#define READING_MODE_SIDE_MARGIN 64
#define READING_MODE_LINE_PAD 4
#define NORMAL_SIDE_MARGIN 14
#define NORMAL_LINE_PAD 1

/* Reading mode caps the line length instead of just padding the sides.
 * A fixed side margin is fine on a laptop panel and falls apart on a
 * wide one: on a 3440px ultrawide it left the text running the full
 * width of the screen.
 *
 * Two settings shape the result, both in settings.xml under <misc>:
 *
 *   reading_mode_width_pct (default 90) -- how much of the window the
 *       reading area takes, the rest split evenly as side margins. 90
 *       leaves 5% clear on each side.
 *
 *   reading_mode_cpl (default 0 = off) -- an optional cap on line
 *       length, in characters. Typography puts the comfortable range
 *       at 45-75, and a single column spanning 90% of an ultrawide is
 *       far past it; set this to bring the column back down and centre
 *       it. Left off by default because the full width is the right
 *       answer once the text is laid out in columns.
 *
 * The cap is measured, not assumed: the font comes from CSS and the
 * user can change family and size, so READING_MODE_SAMPLE (a stretch
 * of ordinary Spanish scripture prose) is run through Pango and its
 * width divided by its length gives a mean character width for
 * whatever face is actually in use. */
#define READING_MODE_WIDTH_PCT_DEFAULT 90
#define READING_MODE_SAMPLE                                             \
	"Jehová es mi pastor; nada me faltará. En lugares de delicados " \
	"pastos me hará descansar, junto a aguas de reposo me pastoreará."

#define READING_MODE_HOVER_SHOW_Y 40
#define READING_MODE_HOVER_HIDE_DELAY_MS 500

/* Width, in pixels, of settings.reading_mode_cpl characters of the font
 * this view is currently rendering with. */
static gint
reading_mode_target_width(GtkTextView *view)
{
	GtkStyleContext *ctx;
	PangoFontDescription *desc = NULL;
	PangoLayout *layout;
	gint sample_w = 0;
	glong sample_len;

	sample_len = g_utf8_strlen(READING_MODE_SAMPLE, -1);
	if (sample_len <= 0)
		return 0;

	ctx = gtk_widget_get_style_context(GTK_WIDGET(view));
	gtk_style_context_get(ctx, gtk_style_context_get_state(ctx),
			      GTK_STYLE_PROPERTY_FONT, &desc, NULL);

	layout = gtk_widget_create_pango_layout(GTK_WIDGET(view),
						READING_MODE_SAMPLE);
	if (desc) {
		pango_layout_set_font_description(layout, desc);
		pango_font_description_free(desc);
	}
	pango_layout_set_width(layout, -1);	/* measure unwrapped */
	pango_layout_get_pixel_size(layout, &sample_w, NULL);
	g_object_unref(layout);

	if (sample_w <= 0)
		return 0;
	return (gint)((sample_w * (gdouble)settings.reading_mode_cpl) / sample_len);
}

/* Side margins for reading mode: a percentage of the window by
 * default, tightened further to centre a capped measure when
 * reading_mode_cpl asks for one. Never narrower than
 * READING_MODE_SIDE_MARGIN, so a small window behaves as it always
 * did and only a wide one is reshaped. */
static void
reading_mode_apply_measure(GtkTextView *view)
{
	gint avail, target, margin;

	if (!view)
		return;

	if (!settings.reading_mode) {
		gtk_text_view_set_left_margin(view, NORMAL_SIDE_MARGIN);
		gtk_text_view_set_right_margin(view, NORMAL_SIDE_MARGIN);
		return;
	}

	avail = gtk_widget_get_allocated_width(GTK_WIDGET(view));

	/* the percentage the reading area keeps; the remainder is split
	 * evenly between the two sides. */
	margin = (avail > 0)
		     ? (avail * (100 - settings.reading_mode_width_pct)) / 200
		     : READING_MODE_SIDE_MARGIN;
	if (margin < READING_MODE_SIDE_MARGIN)
		margin = READING_MODE_SIDE_MARGIN;

	/* an explicit line-length cap wins when it is narrower still. */
	target = (settings.reading_mode_cpl > 0)
		     ? reading_mode_target_width(view)
		     : 0;
	if ((avail > 0) && (target > 0) && (avail - 2 * margin > target))
		margin = (avail - target) / 2;

	/* Setting the margin re-runs allocation, which brings us straight
	 * back here; bail out once it has converged so the loop cannot feed
	 * itself. */
	if (gtk_text_view_get_left_margin(view) == margin &&
	    gtk_text_view_get_right_margin(view) == margin)
		return;

	gtk_text_view_set_left_margin(view, margin);
	gtk_text_view_set_right_margin(view, margin);
}

/* The allocation is not final when reading mode is switched on: the
 * compositor answers gtk_window_fullscreen() a frame or two later, and
 * the user can move the window between monitors afterwards. Recompute
 * on every allocation instead of only at the toggle. */
static void
reading_mode_on_size_allocate(GtkWidget *widget, GdkRectangle *alloc,
			      gpointer data)
{
	(void)alloc;
	(void)data;
	reading_mode_apply_measure(GTK_TEXT_VIEW(widget));
}

/* Switches the reading pane between the single Bible and the
 * verse-aligned comparison, keeping the floating button in step no
 * matter which of the two entry points asked for it. */
static void
reading_compare_set(gboolean on)
{
	if (!settings.reading_mode) {
		gui_generic_warning(
		    _("Comparar en columnas requiere el modo lectura "
		      "(Ctrl+Shift+F)."));
		return;
	}
	if (settings.reading_compare == (on ? 1 : 0))
		return;

	settings.reading_compare = on ? 1 : 0;
	xml_set_value("Xiphos", "misc", "reading_compare",
		      settings.reading_compare ? "1" : "0");

	if (reading_compare_button) {
		g_signal_handlers_block_by_func(reading_compare_button,
						G_CALLBACK(on_reading_compare_toggled), NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reading_compare_button), on);
		g_signal_handlers_unblock_by_func(reading_compare_button,
						  G_CALLBACK(on_reading_compare_toggled), NULL);
	}
	/* picking versions only means anything while comparing */
	if (reading_compare_pick)
		gtk_widget_set_visible(reading_compare_pick, on);
	if (settings.currentverse)
		main_display_bible(NULL, settings.currentverse);
}

/* Writes the chosen set back to both the live settings and
 * modules/parallels, which is where parallel_build_html() reads it
 * from, then repaints. */
static void
reading_compare_set_modules(GList *chosen)
{
	GString *csv = g_string_new(NULL);
	GList *l;

	for (l = chosen; l; l = l->next) {
		if (csv->len)
			g_string_append_c(csv, ',');
		g_string_append(csv, (const gchar *)l->data);
	}
	if (!csv->len) {
		g_string_free(csv, TRUE);
		return;			/* never leave the comparison empty */
	}

	xml_set_value("Xiphos", "modules", "parallels", csv->str);
	if (settings.parallel_list)
		g_strfreev(settings.parallel_list);
	settings.parallel_list = g_strsplit(csv->str, ",", -1);
	g_string_free(csv, TRUE);

	if (settings.reading_mode && settings.reading_compare &&
	    settings.currentverse)
		main_display_bible(NULL, settings.currentverse);
}

static void
on_compare_pick_toggled(GtkToggleButton *check, gpointer data)
{
	GtkWidget *box = GTK_WIDGET(data);
	GList *kids, *k, *chosen = NULL;

	(void)check;
	kids = gtk_container_get_children(GTK_CONTAINER(box));
	for (k = kids; k; k = k->next) {
		GtkWidget *w = GTK_WIDGET(k->data);
		if (!GTK_IS_CHECK_BUTTON(w))
			continue;
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
			chosen = g_list_append(
			    chosen, g_object_get_data(G_OBJECT(w), "modname"));
	}
	g_list_free(kids);
	reading_compare_set_modules(chosen);
	g_list_free(chosen);
}

/* Popover listing every installed Bible, ticked for the ones currently
 * being compared. Built fresh each time it opens so newly installed
 * modules show up without a restart. */
static void
on_reading_compare_pick(GtkButton *button, gpointer data)
{
	GtkWidget *pop, *box;
	GList *bibles, *descs, *l, *d;

	(void)data;
	pop = gtk_popover_new(GTK_WIDGET(button));
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_container_set_border_width(GTK_CONTAINER(box), 8);

	bibles = get_list(TEXT_LIST);
	descs = get_list(TEXT_DESC_LIST);
	for (l = bibles, d = descs; l; l = l->next, d = d ? d->next : NULL) {
		const gchar *name = (const gchar *)l->data;
		const gchar *desc = d ? (const gchar *)d->data : NULL;
		GtkWidget *chk;
		gboolean on = FALSE;
		int i;

		if (!name)
			continue;
		for (i = 0; settings.parallel_list && settings.parallel_list[i]; i++) {
			if (!g_strcmp0(settings.parallel_list[i], name)) {
				on = TRUE;
				break;
			}
		}
		chk = gtk_check_button_new_with_label(desc && *desc ? desc : name);
		g_object_set_data_full(G_OBJECT(chk), "modname",
				       g_strdup(name), g_free);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), on);
		g_signal_connect(chk, "toggled",
				 G_CALLBACK(on_compare_pick_toggled), box);
		gtk_box_pack_start(GTK_BOX(box), chk, FALSE, FALSE, 0);
	}

	gtk_widget_show_all(box);
	gtk_container_add(GTK_CONTAINER(pop), box);
	gtk_popover_set_position(GTK_POPOVER(pop), GTK_POS_BOTTOM);
	gtk_popover_popup(GTK_POPOVER(pop));
}

static void
on_reading_compare_toggled(GtkToggleButton *button, gpointer data)
{
	(void)data;
	reading_compare_set(gtk_toggle_button_get_active(button));
}

static void
reading_mode_hover_cancel_hide(void)
{
	if (reading_mode_hover_hide_src) {
		g_source_remove(reading_mode_hover_hide_src);
		reading_mode_hover_hide_src = 0;
	}
}

static gboolean
reading_mode_hover_hide(gpointer data)
{
	(void)data;
	reading_mode_hover_hide_src = 0;
	if (widgets.nav_toolbar)
		gtk_widget_hide(widgets.nav_toolbar);
	return G_SOURCE_REMOVE;
}

static void
reading_mode_hover_schedule_hide(void)
{
	reading_mode_hover_cancel_hide();
	reading_mode_hover_hide_src = g_timeout_add(READING_MODE_HOVER_HIDE_DELAY_MS,
						    reading_mode_hover_hide, NULL);
}

/* Reveals nav_toolbar (floating, via reading_mode_float_toolbar() below)
 * when the pointer nears the top of the text view, hides it again a
 * moment after the pointer leaves that zone -- unless it moved onto the
 * toolbar itself, tracked separately below. */
static gboolean
reading_mode_on_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	(void)widget;
	(void)data;
	if (!settings.reading_mode || !widgets.nav_toolbar)
		return FALSE;
	if (event->y <= READING_MODE_HOVER_SHOW_Y) {
		reading_mode_hover_cancel_hide();
		if (!gtk_widget_get_visible(widgets.nav_toolbar))
			gtk_widget_show(widgets.nav_toolbar);
	} else if (gtk_widget_get_visible(widgets.nav_toolbar)) {
		reading_mode_hover_schedule_hide();
	}
	return FALSE;
}

static gboolean
reading_mode_toolbar_enter(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	(void)widget;
	(void)event;
	(void)data;
	reading_mode_hover_cancel_hide();
	return FALSE;
}

static gboolean
reading_mode_toolbar_leave(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	(void)widget;
	(void)data;
	/* Also fires when the pointer crosses into one of the toolbar's own
	 * child widgets (an entry, a spin button...) -- NOTIFY_INFERIOR
	 * marks that case, which isn't really "left the toolbar". */
	if (event->detail == GDK_NOTIFY_INFERIOR)
		return FALSE;
	reading_mode_hover_schedule_hide();
	return FALSE;
}

/* Moves nav_toolbar between its normal spot in widgets.page (a fixed
 * layout row, always visible) and widgets.reading_mode_overlay (floating
 * over the text, hidden by default, revealed by hovering near the top --
 * see reading_mode_on_motion() above). Safe to call redundantly; only
 * acts when the toolbar isn't already where it should be. */
static void
reading_mode_float_toolbar(gboolean floating, GtkTextView *view)
{
	if (!widgets.nav_toolbar || !widgets.reading_mode_overlay)
		return;

	if (floating) {
		if (gtk_widget_get_parent(widgets.nav_toolbar) != widgets.reading_mode_overlay) {
			g_object_ref(widgets.nav_toolbar);
			gtk_container_remove(GTK_CONTAINER(widgets.page), widgets.nav_toolbar);
			gtk_overlay_add_overlay(GTK_OVERLAY(widgets.reading_mode_overlay),
						widgets.nav_toolbar);
			gtk_widget_set_halign(widgets.nav_toolbar, GTK_ALIGN_FILL);
			gtk_widget_set_valign(widgets.nav_toolbar, GTK_ALIGN_START);
			gtk_style_context_add_class(gtk_widget_get_style_context(widgets.nav_toolbar),
						    "elim-navbar-floating");
			g_object_unref(widgets.nav_toolbar);
		}
		gtk_widget_hide(widgets.nav_toolbar);
		if (view) {
			gtk_widget_add_events(GTK_WIDGET(view), GDK_POINTER_MOTION_MASK);
			reading_mode_motion_id = g_signal_connect(
			    view, "motion-notify-event", G_CALLBACK(reading_mode_on_motion), NULL);
		}
		gtk_widget_add_events(widgets.nav_toolbar,
				      GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
		reading_mode_toolbar_enter_id = g_signal_connect(
		    widgets.nav_toolbar, "enter-notify-event",
		    G_CALLBACK(reading_mode_toolbar_enter), NULL);
		reading_mode_toolbar_leave_id = g_signal_connect(
		    widgets.nav_toolbar, "leave-notify-event",
		    G_CALLBACK(reading_mode_toolbar_leave), NULL);
	} else {
		reading_mode_hover_cancel_hide();
		if (reading_mode_motion_id && view) {
			g_signal_handler_disconnect(view, reading_mode_motion_id);
			reading_mode_motion_id = 0;
		}
		if (reading_mode_toolbar_enter_id) {
			g_signal_handler_disconnect(widgets.nav_toolbar, reading_mode_toolbar_enter_id);
			reading_mode_toolbar_enter_id = 0;
		}
		if (reading_mode_toolbar_leave_id) {
			g_signal_handler_disconnect(widgets.nav_toolbar, reading_mode_toolbar_leave_id);
			reading_mode_toolbar_leave_id = 0;
		}
		if (gtk_widget_get_parent(widgets.nav_toolbar) == widgets.reading_mode_overlay) {
			g_object_ref(widgets.nav_toolbar);
			gtk_container_remove(GTK_CONTAINER(widgets.reading_mode_overlay),
					     widgets.nav_toolbar);
			gtk_style_context_remove_class(gtk_widget_get_style_context(widgets.nav_toolbar),
						       "elim-navbar-floating");
			gtk_box_pack_start(GTK_BOX(widgets.page), widgets.nav_toolbar, FALSE, FALSE, 0);
			gtk_box_reorder_child(GTK_BOX(widgets.page), widgets.nav_toolbar, 0);
			g_object_unref(widgets.nav_toolbar);
		}
		gtk_widget_show(widgets.nav_toolbar);
	}
}

void gui_toggle_reading_mode(gboolean choice)
{
	GtkTextView *view;
	/* Re-entrancy guard: programmatically syncing the menu checkbox or
	 * header-bar button below re-emits their own "toggled"/"activate"
	 * regardless of what triggered the change, which would otherwise
	 * run this whole function a second time on top of itself -- with
	 * the second run's gtk_window_fullscreen()/unfullscreen() request
	 * landing on the compositor mid-transition from the first, which is
	 * what was leaving the sidebar/nav toolbar stuck hidden after
	 * leaving reading mode. */
	static gboolean in_progress = FALSE;
	if (in_progress)
		return;
	in_progress = TRUE;

	settings.reading_mode = choice;
	xml_set_value("Xiphos", "misc", "reading_mode", choice ? "1" : "0");

	if (!main_window_created) {
		in_progress = FALSE;
		return;
	}

	view = widgets.html_text ? wk_html_get_view(WK_HTML(widgets.html_text)) : NULL;

	if (choice) {
		gtk_widget_hide(widgets.paned_sidebar);
		gtk_widget_hide(widgets.hboxtb);
		gtk_widget_hide(widgets.vbox_previewer);
		gtk_widget_hide(widgets.box_side_preview);
		reading_mode_float_toolbar(TRUE, view);
		if (widgets.vpaned2)
			gtk_widget_hide(widgets.vpaned2);
		if (widgets.appbar)
			gtk_widget_hide(widgets.appbar);
		if (widgets.bar_interlineal)
			gtk_widget_hide(widgets.bar_interlineal);
		if (widgets.box_lectura_sync)
			gtk_widget_hide(widgets.box_lectura_sync);
		if (header_menu)
			gtk_widget_hide(header_menu);
		if (widgets.notebook_bible_parallel) {
			gtk_notebook_set_current_page(
			    GTK_NOTEBOOK(widgets.notebook_bible_parallel), 0);
			gtk_notebook_set_show_tabs(
			    GTK_NOTEBOOK(widgets.notebook_bible_parallel), FALSE);
		}
		gtk_window_fullscreen(GTK_WINDOW(widgets.app));
	} else {
		if (header_menu)
			gtk_widget_show(header_menu);
		if (settings.showshortcutbar) {
			gtk_widget_show(widgets.paned_sidebar);
			/* GtkPaned position is a fixed pixel value that doesn't
			 * re-derive itself across the fullscreen round-trip's
			 * drastic width swing -- restore it explicitly, same as
			 * gui_sidebar_showhide() does whenever it shows this
			 * same pane. */
			gtk_paned_set_position(GTK_PANED(widgets.epaned),
					       settings.sidebar_width);
		}
		if (settings.browsing)
			gtk_widget_show(widgets.hboxtb);
		if (widgets.bar_interlineal)
			gtk_widget_show(widgets.bar_interlineal);
		if (widgets.box_lectura_sync && settings.show_lectura_sync)
			gtk_widget_show(widgets.box_lectura_sync);
		if (widgets.notebook_bible_parallel)
			gtk_notebook_set_show_tabs(
			    GTK_NOTEBOOK(widgets.notebook_bible_parallel), TRUE);
		if (widgets.appbar && settings.statusbar == 1)
			gtk_widget_show(widgets.appbar);
		gui_show_hide_preview(settings.showpreview);
		gui_set_bible_comm_layout();
		gtk_window_unfullscreen(GTK_WINDOW(widgets.app));
		/* gtk_window_unfullscreen() is only a request to the compositor;
		 * the widgets just re-shown above can end up allocated at their
		 * stale (hidden/fullscreen) size for a frame or two until the
		 * resize actually lands. Force GTK to recompute everything
		 * rather than leave that to chance. */
		gtk_widget_queue_resize(widgets.epaned);
		gtk_widget_queue_resize(widgets.vboxMain);
		reading_mode_float_toolbar(FALSE, view);
	}
	if (view) {
		gint line_pad = choice ? READING_MODE_LINE_PAD : NORMAL_LINE_PAD;
		gtk_text_view_set_pixels_above_lines(view, line_pad);
		gtk_text_view_set_pixels_below_lines(view, line_pad);

		/* Follow the allocation only while reading mode is on; outside
		 * it the margin is the fixed NORMAL_SIDE_MARGIN and there is
		 * nothing to recompute. */
		if (choice && !reading_mode_alloc_id) {
			reading_mode_alloc_id = g_signal_connect(
			    view, "size-allocate",
			    G_CALLBACK(reading_mode_on_size_allocate), NULL);
		} else if (!choice && reading_mode_alloc_id) {
			g_signal_handler_disconnect(view, reading_mode_alloc_id);
			reading_mode_alloc_id = 0;
		}
		reading_mode_apply_measure(view);

		if (choice)
			gtk_widget_grab_focus(GTK_WIDGET(view));
	}

	/* keep every entry point showing the same state. Block each
	 * widget's own change handler while syncing it programmatically --
	 * GTK re-emits "toggled" from set_active() regardless of what
	 * triggered the change, so without this a sync here would
	 * re-enter gui_toggle_reading_mode() right back on top of itself. */
	if (widgets.reading_mode_item) {
		g_signal_handlers_block_by_func(widgets.reading_mode_item,
						G_CALLBACK(on_reading_mode_activate), NULL);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widgets.reading_mode_item), choice);
		g_signal_handlers_unblock_by_func(widgets.reading_mode_item,
						  G_CALLBACK(on_reading_mode_activate), NULL);
	}
	if (widgets.reading_mode_button) {
		g_signal_handlers_block_by_func(widgets.reading_mode_button,
						G_CALLBACK(on_reading_mode_button_toggled), NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.reading_mode_button), choice);
		g_signal_handlers_unblock_by_func(widgets.reading_mode_button,
						  G_CALLBACK(on_reading_mode_button_toggled), NULL);
	}
	if (reading_exit_button) {
		g_signal_handlers_block_by_func(reading_exit_button,
						G_CALLBACK(on_reading_mode_button_toggled), NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reading_exit_button), choice);
		g_signal_handlers_unblock_by_func(reading_exit_button,
						  G_CALLBACK(on_reading_mode_button_toggled), NULL);
		if (choice)
			gtk_widget_show(reading_exit_button);
		else
			gtk_widget_hide(reading_exit_button);
	}
	if (reading_compare_pick)
		gtk_widget_set_visible(reading_compare_pick,
				       choice && settings.reading_compare);
	if (reading_compare_button) {
		gtk_widget_set_visible(reading_compare_button, choice);
		if (!choice && settings.reading_compare) {
			/* leaving reading mode drops the comparison with it */
			settings.reading_compare = 0;
			xml_set_value("Xiphos", "misc", "reading_compare", "0");
			g_signal_handlers_block_by_func(reading_compare_button,
							G_CALLBACK(on_reading_compare_toggled), NULL);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reading_compare_button), FALSE);
			g_signal_handlers_unblock_by_func(reading_compare_button,
							  G_CALLBACK(on_reading_compare_toggled), NULL);
		}
	}

	/* Stay on the verse the reader was already on. Fullscreen and pane
	 * hide change allocation, so re-place after the resize. The 140ms
	 * timeout alone wasn't reliable -- gtk_window_(un)fullscreen() is
	 * only a request, and how long the compositor actually takes to
	 * grant it varies (users reported the reading-focus band missing
	 * every time on some setups). "window-state-event" fires exactly
	 * when the fullscreen state actually flips, so it catches the real
	 * moment instead of guessing a duration; the timeout stays too, as
	 * a fallback for a compositor that never sends that event. Whichever
	 * fires first wins and tears down the other. */
	if (reading_mode_place_src)
		g_source_remove(reading_mode_place_src);
	reading_mode_place_src = g_timeout_add(140, reading_mode_keep_place, NULL);

	if (reading_mode_wse_id)
		g_signal_handler_disconnect(widgets.app, reading_mode_wse_id);
	reading_mode_wse_id = g_signal_connect(widgets.app, "window-state-event",
					       G_CALLBACK(reading_mode_on_window_state), NULL);

	in_progress = FALSE;
}

static void
reading_mode_settle(void)
{
	if (reading_mode_place_src) {
		g_source_remove(reading_mode_place_src);
		reading_mode_place_src = 0;
	}
	if (reading_mode_wse_id) {
		g_signal_handler_disconnect(widgets.app, reading_mode_wse_id);
		reading_mode_wse_id = 0;
	}
	if (widgets.html_text && settings.currentverse)
		gui_bibletext_mark_current_verse();
}

static gboolean
reading_mode_keep_place(gpointer data)
{
	(void)data;
	reading_mode_place_src = 0;
	reading_mode_settle();
	return G_SOURCE_REMOVE;
}

static gboolean
reading_mode_on_window_state(GtkWidget *widget, GdkEventWindowState *event, gpointer data)
{
	(void)widget;
	(void)data;
	if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
		reading_mode_settle();
	return FALSE; /* don't block other handlers on this event */
}

/* Open-Bible glyph for the reading-mode toggle: two pages, spine, a
 * bookmark ribbon. Drawn from the header-bar foreground so it tracks
 * light/dark themes without a pixmap. */
static gboolean
on_open_bible_icon_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	GtkWidget *parent = gtk_widget_get_parent(widget);
	GtkStyleContext *ctx = gtk_widget_get_style_context(parent ? parent : widget);
	GtkStateFlags st = gtk_style_context_get_state(ctx);
	GdkRGBA fg;
	int w, h;
	double s, ox, oy, ly;

	(void)data;
	gtk_style_context_get_color(ctx, st, &fg);
	w = gtk_widget_get_allocated_width(widget);
	h = gtk_widget_get_allocated_height(widget);
	s = (w < h) ? w : h;
	if (s < 1.0)
		return FALSE;
	ox = (w - s) * 0.5;
	oy = (h - s) * 0.5;
	cairo_translate(cr, ox, oy);
	cairo_scale(cr, s / 16.0, s / 16.0);

	cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, fg.alpha);
	cairo_set_line_width(cr, 1.15);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

	/* left page */
	cairo_move_to(cr, 8.0, 3.5);
	cairo_curve_to(cr, 6.0, 4.2, 3.4, 3.5, 2.2, 3.5);
	cairo_line_to(cr, 2.0, 12.6);
	cairo_curve_to(cr, 3.4, 12.6, 6.0, 13.2, 8.0, 12.5);
	cairo_close_path(cr);
	cairo_stroke(cr);

	/* right page */
	cairo_move_to(cr, 8.0, 3.5);
	cairo_curve_to(cr, 10.0, 4.2, 12.6, 3.5, 13.8, 3.5);
	cairo_line_to(cr, 14.0, 12.6);
	cairo_curve_to(cr, 12.6, 12.6, 10.0, 13.2, 8.0, 12.5);
	cairo_close_path(cr);
	cairo_stroke(cr);

	/* spine */
	cairo_move_to(cr, 8.0, 3.5);
	cairo_line_to(cr, 8.0, 12.5);
	cairo_stroke(cr);

	/* verse lines on each page */
	cairo_set_line_width(cr, 0.7);
	cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, fg.alpha * 0.72);
	for (ly = 5.6; ly < 11.0; ly += 1.55) {
		cairo_move_to(cr, 3.3, ly);
		cairo_line_to(cr, 6.5, ly + 0.32);
		cairo_stroke(cr);
		cairo_move_to(cr, 9.5, ly + 0.32);
		cairo_line_to(cr, 12.7, ly);
		cairo_stroke(cr);
	}

	/* bookmark ribbon at the top of the spine */
	cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, fg.alpha);
	cairo_set_line_width(cr, 1.05);
	cairo_move_to(cr, 8.0, 3.5);
	cairo_line_to(cr, 8.0, 1.45);
	cairo_line_to(cr, 9.25, 2.25);
	cairo_line_to(cr, 8.0, 3.05);
	cairo_stroke(cr);

	return FALSE;
}

static GtkWidget *
new_open_bible_toggle(const char *tooltip)
{
	GtkWidget *btn, *icon;

	btn = gtk_toggle_button_new();
	icon = gtk_drawing_area_new();
	gtk_widget_set_size_request(icon, 18, 18);
	gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(icon, FALSE);
	gtk_widget_set_vexpand(icon, FALSE);
	g_signal_connect(icon, "draw", G_CALLBACK(on_open_bible_icon_draw), NULL);
	g_signal_connect_swapped(btn, "state-flags-changed",
				 G_CALLBACK(gtk_widget_queue_draw), icon);
	gtk_widget_show(icon);
	gtk_container_add(GTK_CONTAINER(btn), icon);
	gtk_style_context_add_class(gtk_widget_get_style_context(btn), "flat");
	gtk_style_context_add_class(gtk_widget_get_style_context(btn), "circular");
	gtk_style_context_add_class(gtk_widget_get_style_context(btn), "reading-mode");
	gtk_widget_set_tooltip_text(btn, tooltip);
	g_signal_connect(btn, "toggled",
			 G_CALLBACK(on_reading_mode_button_toggled), NULL);
	gtk_widget_show(btn);
	return btn;
}

/******************************************************************************
 * Name
 *  gui_set_bible_comm_layout
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void gui_set_bible_comm_layout(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_set_bible_comm_layout(void)
{
	if (settings.reading_mode)
		return;

	gtk_paned_set_position(GTK_PANED(widgets.hpaned),
			       settings.biblepane_width);
	gtk_paned_set_position(GTK_PANED(widgets.vpaned),
			       settings.biblepane_height);
	gtk_paned_set_position(GTK_PANED(widgets.vpaned2),
			       settings.commpane_height);

	if ((settings.showcomms == TRUE) || (settings.showdicts == TRUE)) {
		gtk_widget_show(widgets.vpaned2);
	}

	gtk_paned_set_position(GTK_PANED(widgets.hpaned),
			       (settings.showtexts
				    ? settings.biblepane_width
				    : 0));

	gtk_paned_set_position(GTK_PANED(widgets.vpaned2),
			       (settings.showcomms
				    ? settings.commpane_height
				    : 0));

	gtk_paned_set_position(GTK_PANED(widgets.vpaned2),
			       (settings.showdicts
				    ? settings.commpane_height
				    : settings.gs_height));

	if ((settings.showcomms == FALSE) && (settings.showdicts == FALSE)) {
		gtk_widget_hide(widgets.vpaned2);
		gtk_paned_set_position(GTK_PANED(widgets.hpaned),
				       settings.gs_width);
	}

	if ((settings.showcomms == TRUE) || (settings.showdicts == TRUE)) {
		gtk_paned_set_position(GTK_PANED(widgets.hpaned),
				       settings.biblepane_width);
	}
	if (((settings.showcomms == FALSE) && (settings.showtexts == FALSE)) || ((settings.comm_showing == FALSE) && (settings.showtexts == FALSE)))
		gtk_widget_hide(widgets.nav_toolbar);
	else
		gtk_widget_show(widgets.nav_toolbar);

	/* Esta función se llama muy seguido (resize, cambios de layout
	 * ajenos) y hasta acá solo sabía de las pestañas Comentario(0)/
	 * Libro(1). No tocar la página si ya está en la pestaña "Notas"
	 * (índice 2, agregada en main_window.c junto a notas_verso.c) --
	 * si no, cada una de esas llamadas de rutina expulsaba al usuario
	 * de la nota que gui_verse_notes_panel_actualizar() acababa de
	 * abrirle. */
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book)) != 2)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book),
					      (settings.comm_showing ? 0 : 1));
}

/******************************************************************************
 * Name
 *  gui_change_window_title
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void gui_change_window_title(gchar * module_name)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_change_window_title(gchar *module_name)
{
	gchar *title;

	if (cur_passage_tab) {
		/* borrowed from tabbed_browser.c:pick_tab_label() */
		/* echo the current tab's module (full name) in title bar */
		if (cur_passage_tab->showtexts || cur_passage_tab->comm_showing) {
			title =
			    (cur_passage_tab->showtexts ? cur_passage_tab->text_mod : (cur_passage_tab->commentary_mod ? cur_passage_tab->commentary_mod : "[no commentary]"));
		} else {
			title = (cur_passage_tab->showcomms
				     ? (cur_passage_tab->book_mod
					    ? cur_passage_tab->book_mod
					    : "[no book]")
				     : (cur_passage_tab->dictlex_mod
					    ? cur_passage_tab->dictlex_mod
					    : "[no dict]"));
		}
	} else
		title = module_name;

	{
		gchar *desc, *full;
		GtkWidget *tb;

		desc = g_strdup(main_get_module_description(title));
		if (!desc)
			desc = g_strdup(main_get_module_description(settings.MainWindowModule));
		full = g_strdup_printf("%s — Biblia Elim",
				       desc ? desc : "[no title]");
		g_free(desc);
		gtk_window_set_title(GTK_WINDOW(widgets.app), full);
		tb = gtk_window_get_titlebar(GTK_WINDOW(widgets.app));
		if (tb && GTK_IS_HEADER_BAR(tb)) {
			gtk_header_bar_set_title(GTK_HEADER_BAR(tb), full);
			gtk_header_bar_set_subtitle(GTK_HEADER_BAR(tb),
						    _("Estudio bíblico"));
		}
		g_free(full);
	}
}

static gboolean delete_event(GtkWidget *widget,
			     GdkEvent *event, gpointer user_data)
{
	on_quit_activate(NULL, NULL);
	return TRUE;
}

/******************************************************************************
 * Name
 *   on_epaned_button_release_event
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   gboolean on_epaned_button_release_event(GtkWidget * widget,
 *			GdkEventButton * event, gpointer user_data)
 *
 * Description
 *    get and store pane sizes
 *
 * Return value
 *   gboolean
 */

static gboolean epaned_button_release_event(GtkWidget *widget,
					    GdkEventButton *event,
					    gpointer user_data)
{
	gint panesize;
	gchar layout[80];

	panesize = gtk_paned_get_position(GTK_PANED(widget));

	if (panesize > 15) {
		if (!strcmp((gchar *)user_data, "epaned")) {
			settings.sidebar_width = panesize;
			sprintf(layout, "%d", settings.sidebar_width);
			xml_set_value("Xiphos", "layout",
				      "shortcutbar", layout);
		}
		if (!strcmp((gchar *)user_data, "vpaned")) {
			settings.biblepane_height = panesize;
			sprintf(layout, "%d", settings.biblepane_height);
			xml_set_value("Xiphos", "layout",
				      "bibleheight", layout);
		}
		if (!strcmp((gchar *)user_data, "vpaned2")) {
			settings.commpane_height = panesize;
			sprintf(layout, "%d", settings.commpane_height);
			xml_set_value("Xiphos", "layout",
				      "commentaryheight", layout);
		}
		if (!strcmp((gchar *)user_data, "hpaned1")) {
			settings.biblepane_width = panesize;
			sprintf(layout, "%d", settings.biblepane_width);
			xml_set_value("Xiphos", "layout",
				      "textpane", layout);
		}
		return FALSE;
	}
	return TRUE;
}

/******************************************************************************
 * Name
 *   final_pane_sizes
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void final_pane_sizes()
 *
 * Description
 *   on quit, make a last query for pane sizes.
 *   this is needed because e.g. "maximize window" in the title bar does
 *   not call on_epaned_button_release_event for each newly-modified pane.
 *
 * Return value
 *   void
 */

void final_pane_sizes()
{
	epaned_button_release_event(GTK_WIDGET(widgets.epaned), NULL,
				    (gchar *)"epaned");
	epaned_button_release_event(GTK_WIDGET(widgets.vpaned), NULL,
				    (gchar *)"vpaned");
	epaned_button_release_event(GTK_WIDGET(widgets.vpaned2), NULL,
				    (gchar *)"vpaned2");
	epaned_button_release_event(GTK_WIDGET(widgets.hpaned), NULL,
				    (gchar *)"hpaned1");
}

/******************************************************************************
 * Name
 *   on_configure_event
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   gboolean on_configure_event(GtkWidget * widget,
 *				   GdkEventConfigure * event,
 *				   gpointer user_data)
 *
 * Description
 *   remember placement+size of main window.
 *
 * Return value
 *   gboolean
 */

static gboolean on_configure_event(GtkWidget *widget,
				   GdkEventConfigure *event,
				   gpointer user_data)
{
	gchar layout[80];
	gint x;
	gint y;

	settings.gs_width = event->width;
	settings.gs_height = event->height;

#if GTK_CHECK_VERSION(3, 12, 0)
	sprintf(layout, "%d", gtk_window_is_maximized(GTK_WINDOW(widgets.app)));
	xml_set_value("Xiphos", "layout", "maximized", layout);
#endif

	sprintf(layout, "%d", settings.gs_width);
	xml_set_value("Xiphos", "layout", "width", layout);

	sprintf(layout, "%d", settings.gs_height);
	xml_set_value("Xiphos", "layout", "height", layout);

	/* On Wayland the compositor owns placement; x/y from
	 * gdk_window_get_root_origin() is often 0 and must not be saved. */
	if (!gui_display_is_wayland()) {
		gdk_window_get_root_origin(gtk_widget_get_window(widgets.app),
					   &x, &y);
		settings.app_x = x;
		settings.app_y = y;
		sprintf(layout, "%d", settings.app_x);
		xml_set_value("Xiphos", "layout", "app_x", layout);
		sprintf(layout, "%d", settings.app_y);
		xml_set_value("Xiphos", "layout", "app_y", layout);
	}
	xml_save_settings_doc(settings.fnconfigure);

	return FALSE;
}

#ifdef USE_GTK_3
static void on_notebook_bible_parallel_switch_page(GtkNotebook *notebook,
						   gpointer arg,
						   gint page_num,
						   GList **tl)
#else
static void on_notebook_bible_parallel_switch_page(GtkNotebook *notebook,
						   GtkNotebookPage *page,
						   gint page_num,
						   GList **tl)
#endif
{
	(void)notebook;
	(void)arg;
	(void)tl;
	if (page_num == 1)
		main_update_parallel_page();
}

#ifdef USE_GTK_3
static void on_notebook_comm_book_switch_page(GtkNotebook *notebook,
					      gpointer arg,
					      gint page_num, GList **tl)
#else
static void on_notebook_comm_book_switch_page(GtkNotebook *notebook,
					      GtkNotebookPage *page,
					      gint page_num, GList **tl)
#endif
{
	gchar *url = NULL;

	/* pestaña "Notas": tiene su propia gestión de contenido
	 * (gui_verse_notes_panel_actualizar()), no participa del
	 * mecanismo comm/book de abajo. */
	if (page_num == 2)
		return;

	if (page_num == 0) {
		settings.comm_showing = TRUE;
		gtk_widget_show(widgets.nav_toolbar);
	} else {
		settings.comm_showing = FALSE;
		if (!settings.showtexts)
			gtk_widget_hide(widgets.nav_toolbar);
	}

	gui_update_tab_struct(NULL,
			      settings.CommWindowModule,
			      NULL,
			      NULL,
			      NULL,
			      NULL,
			      settings.comm_showing,
			      settings.showtexts,
			      settings.showpreview,
			      settings.showcomms, settings.showdicts);
	if (settings.comm_showing)
		main_display_commentary(settings.CommWindowModule,
					settings.currentverse);
	else if (settings.book_mod && *settings.book_mod) {
		url = g_strdup_printf("sword://%s/%ld", settings.book_mod,
				      settings.book_offset);
		main_url_handler(url, TRUE);
		g_free(url);
	}
	gui_set_tab_label(settings.currentverse, TRUE);
}

static void new_base_font_size(gboolean up)
{
	if (up) {
		settings.base_font_size++;
		if (settings.base_font_size > 5)
			settings.base_font_size = 5;
	} else {
		settings.base_font_size--;
		if (settings.base_font_size < -2)
			settings.base_font_size = -2;
	}

	if (settings.base_font_size_str)
		g_free(settings.base_font_size_str);
	settings.base_font_size_str =
	    g_strdup_printf("%+d", settings.base_font_size);

	xml_set_value("Xiphos", "fontsize", "basefontsize",
		      settings.base_font_size_str);
	redisplay_to_realign();
}

/* Header-bar zoom buttons: this is the same base-font-size bias already
 * reachable via Ctrl+Shift+'+'/Ctrl+'-' (see on_vbox1_key_press_event
 * below) -- just given a visible, discoverable control instead of only
 * a keyboard shortcut. */
static void on_zoom_in_clicked(GtkWidget *widget, gpointer data)
{
	new_base_font_size(TRUE);
}

static void on_zoom_out_clicked(GtkWidget *widget, gpointer data)
{
	new_base_font_size(FALSE);
}

/* Header-bar reading-mode button: same distraction-free/fullscreen
 * toggle as the View menu checkbox and Ctrl+Shift+F, just given a
 * one-click control at the top of the window. gui_toggle_reading_mode()
 * itself keeps the menu checkbox and this button's pressed state in
 * sync, so this handler only needs to forward the click. */
static void on_reading_mode_button_toggled(GtkToggleButton *button, gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	if (active == settings.reading_mode)
		return; /* gui_toggle_reading_mode() syncing us back -- not a real click */
	gui_toggle_reading_mode(active);
}

/* Header-bar sidebar button: shows/hides the left panel (módulos,
 * marcadores, búsqueda, lista de versículos), same as the "Ver > Mostrar/
 * ocultar panel lateral" menu item and Ctrl+S. gui_sidebar_showhide()
 * keeps this button's pressed state synced back, including when
 * something else (a búsqueda, por ejemplo) opens the panel on its own. */
static void on_sidebar_toggle_button_toggled(GtkToggleButton *button, gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	if (active == settings.showshortcutbar)
		return; /* gui_sidebar_showhide() syncing us back -- not a real click */
	gui_sidebar_showhide();
}

/* Exposed so bibletext.c can hook Ctrl+scroll on the text pane into the
 * same base-font-size bias, without a second, divergent zoom mechanism. */
void gui_zoom_base_font(int up)
{
	new_base_font_size(up ? TRUE : FALSE);
}

/* temporary shorthand for too-common use */
#define sM settings.MainWindowModule
#define sC settings.CommWindowModule
#define sD settings.DictWindowModule
#define sB settings.book_mod
#define sV settings.currentverse

static void kbd_toggle_option(gboolean cond, gchar *option)
{
	gchar *msg;

	if (cond) {
		int opt = !main_get_one_option(sM, option); // negate.
		main_save_module_options(sM, option, opt);
		gchar *url = g_strdup_printf("sword://%s/%s", sM, sV);
		main_url_handler(url, TRUE);
		g_free(url);
		msg =
		    g_strdup_printf("%s %s", option, (opt ? "on" : "off"));
		gui_set_statusbar(msg);
		g_free(msg);
	} else {
		msg = g_strdup_printf(_("Module has no support for %s."),
				      option);
		gui_generic_warning(msg);
		g_free(msg);
	}
}

static gboolean on_vbox1_key_press_event(GtkWidget *widget, GdkEventKey *event,
					 gpointer user_data)
{
	/* these are the mods we actually use for global keys, we always only check for these set */
	guint state =
	    event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
			    GDK_MOD1_MASK | GDK_MOD4_MASK);
	
	switch (event->keyval) {
	case XK_Escape:
		if (state == 0 && gui_lectura_sync_ficha_activa()) {
			gui_lectura_sync_ficha_clear();
			return TRUE;
		}
		break;

	case XK_Shift_L: /* shift keys - we need this for locking strongs (and */
	case XK_Shift_R: /* other stuff) while moving mouse to previewer */
		shift_key_pressed = TRUE;
	/* no break? hm... */

	case XK_a:
	case XK_A:
		if (state == GDK_MOD1_MASK) { // Alt-A  annotation
			gui_mark_verse_dialog(sM, sV);
		} else if (state ==
			   (GDK_CONTROL_MASK | GDK_MOD1_MASK |
			    GDK_SHIFT_MASK))
			on_biblesync_kbd(3); // BSP audience
		break;

	case XK_b:
	case XK_B:
		if (state == GDK_MOD1_MASK) { // Alt-B  bookmark
			gchar *label = g_strdup_printf("%s, %s", sV, sM);
			gui_bookmark_dialog(label, sM, sV);
			g_free(label);
		}
		break;

	case XK_c:
	case XK_C:
		if (state == GDK_MOD1_MASK) { // Alt-C  commentary pane
			gtk_widget_grab_focus(navbar_versekey.lookup_entry);
			gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book),
						      0);
		}
#if BIBLESYNC_VERSION_NUM >= 2000000000
		else if (state == (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK)) {
			// BSP chat
			if (settings.bs_mode == 0)
				gui_generic_warning(_("BibleSync is not active."));
			else {
				GS_DIALOG *info = gui_new_dialog();
#if GTK_CHECK_VERSION(3, 10, 0)
				info->stock_icon = g_strdup("dialog-question");
#else
				info->stock_icon = g_strdup(GTK_STOCK_DIALOG_QUESTION);
#endif
				info->label_top = g_strdup(_("BibleSync Chat"));
				info->text1 = g_strdup(_("[say this]"));
				info->label1 = _("Comment:");
				info->ok = TRUE;
				info->cancel = TRUE;

				gint test = gui_gs_dialog(info);
				if (test == GS_OK)
					biblesync_chat(info->text1);

				g_free(info->label_top);
				g_free(info->text1);
				g_free(info);
			}
		}
#endif /* biblesync >= 2.0.0 */
		break;

	case XK_d:
	case XK_D:
		if (state == GDK_MOD1_MASK) // Alt-D  dictionary entry
			gtk_widget_grab_focus(widgets.entry_dict);
		break;

	case XK_f:
	case XK_F:
		if (state == GDK_CONTROL_MASK) { // Ctrl-F  find text
			if (settings.showtexts) {
				gui_find_dlg(widgets.html_text,
					     sM, FALSE, NULL);
			} else if (settings.showcomms) {
				if (settings.comm_showing) {
					gui_find_dlg(widgets.html_comm,
						     sC, FALSE, NULL);
				} else {
					gui_find_dlg(widgets.html_book,
						     sB, FALSE, NULL);
				}
			} else if (settings.showdicts) {
				gui_find_dlg(widgets.html_dict,
					     sD, FALSE, NULL);
			} else
				gui_generic_warning(_("Xiphos: No windows."));
		} else if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
			// Ctrl-Shift-F: toggle distraction-free reading mode
			gboolean new_state = !settings.reading_mode;
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widgets.reading_mode_item),
						       new_state);
			gui_toggle_reading_mode(new_state);
		}
		break;


	case XK_g:
	case XK_G:
		if (state == GDK_MOD1_MASK) { // Alt-G  genbook entry
			gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_comm_book),
						      1);
			gtk_widget_grab_focus(navbar_book.lookup_entry);
		}
		break;

	case XK_Up:
	case XK_KP_Up:
		if (state == 0) {
			GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(widgets.app));
			if (focus && (GTK_IS_EDITABLE(focus) || GTK_IS_TREE_VIEW(focus)))
				break;
			if (main_interlineal_bloquea_navegacion())
				return TRUE;
			access_on_up_eventbox_button_release_event(VERSE_BUTTON);
			return TRUE;
		}
		break;

	case XK_Down:
	case XK_KP_Down:
		if (state == 0) {
			GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(widgets.app));
			if (focus && (GTK_IS_EDITABLE(focus) || GTK_IS_TREE_VIEW(focus)))
				break;
			if (main_interlineal_bloquea_navegacion())
				return TRUE;
			access_on_down_eventbox_button_release_event(VERSE_BUTTON);
			return TRUE;
		}
		break;

	case XK_j:
		if (state == 0) { // J    "next verse"
			if (main_interlineal_bloquea_navegacion())
				return TRUE;
			access_on_down_eventbox_button_release_event(VERSE_BUTTON);
		}
		break;

	case XK_k:
		if (state == 0) { // K    "previous verse"
			if (main_interlineal_bloquea_navegacion())
				return TRUE;
			access_on_up_eventbox_button_release_event(VERSE_BUTTON);
		} else if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
			/* Ctrl-Shift-K: verse-aligned comparison inside
			 * reading mode -- one column per module in the
			 * parallel list, rows lined up verse by verse.
			 * Only meaningful there: outside reading mode the
			 * tabs and sidebar are in the way and the columns
			 * have no room. */
			reading_compare_set(!settings.reading_compare);
		}
		break;

	case XK_l:
	case XK_L:
		if (state == GDK_CONTROL_MASK) // Ctrl-L  verse entry
			gtk_widget_grab_focus(navbar_versekey.lookup_entry);
		else if (state == GDK_MOD1_MASK) // Alt-L  lemma
			kbd_toggle_option((main_check_for_global_option(sM, "ThMLLemma") ||
					   main_check_for_global_option(sM, "OSISLemma")),
					  "Lemmas");
		break;

	case XK_m:
	case XK_M:
		if (state == GDK_MOD1_MASK) // Alt-M morph
		{
			kbd_toggle_option((main_check_for_global_option(sM, "GBFMorph") ||
					   main_check_for_global_option(sM, "ThMLMorph") ||
					   main_check_for_global_option(sM, "OSISMorph")),
					  "Morphological Tags");
		}
		break;

	case XK_n:
	case XK_N:
		if (state == GDK_CONTROL_MASK || state == 0 ||
		    state == GDK_SHIFT_MASK) {
			if (main_interlineal_bloquea_navegacion())
				return TRUE;
		}
		if (state == GDK_CONTROL_MASK) // Ctrl-N verse
			access_on_down_eventbox_button_release_event(VERSE_BUTTON);
		else if (state == 0) // n chapter
			access_on_down_eventbox_button_release_event(CHAPTER_BUTTON);
		else if (state == GDK_SHIFT_MASK) // N book
			access_on_down_eventbox_button_release_event(BOOK_BUTTON);
		else if (state == GDK_MOD1_MASK) // Alt-N footnote toggle
			kbd_toggle_option((main_check_for_global_option(sM, "GBFFootnotes") ||
					   main_check_for_global_option(sM, "ThMLFootnotes") ||
					   main_check_for_global_option(sM, "OSISFootnotes")),
					  "Footnotes");
		else if (state == (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK)) {
			// BSP transient navigate
			if (biblesync_active_xmit_allowed()) {
				biblesync_prep_and_xmit(sM, sV);
				gui_set_statusbar(_("BibleSync: Current navigation sent."));
			} else {
				gui_generic_warning(_("BibleSync: Not speaking."));
			}
		}
		break;

	case XK_o:
	case XK_O:
		if (state ==
		    (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK))
			on_biblesync_kbd(0); // BSP off
		break;

	case XK_p:
	case XK_P:
		if (state == GDK_CONTROL_MASK || state == 0 ||
		    state == GDK_SHIFT_MASK) {
			if (main_interlineal_bloquea_navegacion())
				return TRUE;
		}
		if (state == GDK_CONTROL_MASK) // Ctrl-P verse
			access_on_up_eventbox_button_release_event(VERSE_BUTTON);
		else if (state == 0) // p chapter
			access_on_up_eventbox_button_release_event(CHAPTER_BUTTON);
		else if (state == GDK_SHIFT_MASK) // P book
			access_on_up_eventbox_button_release_event(BOOK_BUTTON);
		else if (state == GDK_MOD1_MASK) // Alt-P  parallel detach
			on_undockInt_activate(NULL);
		else if (state ==
			 (GDK_CONTROL_MASK | GDK_MOD1_MASK |
			  GDK_SHIFT_MASK))
			on_biblesync_kbd(1); // BSP personal
		break;

	case XK_q:
	case XK_Q:
		if (state == GDK_CONTROL_MASK) // Ctrl-Q quit
			delete_event(NULL, NULL, NULL);
		break;

	case XK_r:
	case XK_R:
		if (state == GDK_MOD1_MASK) // Alt-R red words
		{
			kbd_toggle_option(((main_check_for_global_option(sM, "GBFRedLetterWords")) ||
					   (main_check_for_global_option(sM, "OSISRedLetterWords"))),
					  "Words of Christ in Red");
		}
		else if (state == GDK_CONTROL_MASK) // Ctrl-R: Toggle read aloud
		{
			settings.readaloud = !settings.readaloud;
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widgets.readaloud_item), 
						       settings.readaloud);
		}

		break;

	case XK_s:
	case XK_S:
		if (state == GDK_CONTROL_MASK) // Ctrl-S toggle sidebar
			on_sidebar_showhide_activate((GtkMenuItem *)NULL, (gpointer)NULL);
		else if (state == GDK_MOD1_MASK) // Alt-S: same as the α button
		{
			gui_interlineal_set_active(!settings.show_interlineal);
		} else if (state ==
			   (GDK_CONTROL_MASK | GDK_MOD1_MASK |
			    GDK_SHIFT_MASK))
			on_biblesync_kbd(2); // BSP speaker
		break;

	case XK_t:
	case XK_T:
		if (state == GDK_CONTROL_MASK) // Ctrl-T open a new tab
			on_notebook_main_new_tab_clicked(NULL, NULL);
		else if (state == GDK_MOD1_MASK) // Alt-T transliteration
			kbd_toggle_option(true, "Transliteration");
		break;

	case XK_x:
	case XK_X:
		if (state == GDK_MOD1_MASK) // Alt-X xref toggle
			kbd_toggle_option((main_check_for_global_option(sM, "ThMLScripref") ||
					   main_check_for_global_option(sM, "OSISScripref")),
					  "Cross-references");
	case XK_Tab:
		if (state == GDK_CONTROL_MASK) // Ctrl-Tab  next tab
			if (GTK_NOTEBOOK(widgets.notebook_main) != NULL) {
				gtk_notebook_next_page(GTK_NOTEBOOK(widgets.notebook_main));
				return TRUE; // Need to prevent Tab from navigating between widgets here
			}
		break;

	case XK_ISO_Left_Tab:
		if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) // Ctrl-Shift-Tab  previous tab
			if (GTK_NOTEBOOK(widgets.notebook_main) != NULL) {
				gtk_notebook_prev_page(GTK_NOTEBOOK(widgets.notebook_main));
				return TRUE; // Need to prevent Shift-Tab from navigating between widgets here
			}
		break;

	case XK_Page_Up:
		if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) { // Ctrl-Shift-PgUp  reorder tab to left
			gint current_tab_idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(widgets.notebook_main));
			GtkWidget *current_tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_main), current_tab_idx);
			if (current_tab_idx != 0) {
				gtk_notebook_reorder_child(GTK_NOTEBOOK(widgets.notebook_main), current_tab, current_tab_idx - 1);
			}
		}
		break;

	case XK_Page_Down:
		if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) { // Ctrl-Shift-PgDown  reorder tab to right
			gint current_tab_idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(widgets.notebook_main));
			gint n_tabs = gtk_notebook_get_n_pages(GTK_NOTEBOOK(widgets.notebook_main));
			GtkWidget *current_tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_main), current_tab_idx);
			if (current_tab_idx < n_tabs - 1) {
				gtk_notebook_reorder_child(GTK_NOTEBOOK(widgets.notebook_main), current_tab, current_tab_idx + 1);
			}
		}
		break;

	case XK_w:
		if (state == GDK_CONTROL_MASK) { // Ctrl-W  close current tab
			if (GTK_NOTEBOOK(widgets.notebook_main) != NULL) {
				gint pagenum = gtk_notebook_get_current_page(GTK_NOTEBOOK(widgets.notebook_main));
				gui_close_passage_tab(pagenum);
			}
		}
		break;

	case XK_z:
	case XK_Z:
		if (state == GDK_MOD1_MASK) // Alt-Z  open personal commentary
			access_to_edit_percomm();
		break;

	case XK_plus: // Ctrl-Plus  Increase base font size
		if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
			new_base_font_size(TRUE);
		break;

	case XK_minus: // Ctrl-Minus  Decrease base font size
		if (state == GDK_CONTROL_MASK)
			new_base_font_size(FALSE);
		break;

	case XK_0: // Ctrl-0 (zero)  Neutralize base font size.
		if (state == GDK_CONTROL_MASK) {
			settings.base_font_size = 1;
			new_base_font_size(FALSE);
		}
		break;

	// ctrl-DIGIT [1-9] selects DIGIT-th tab.
	case XK_1:
	case XK_2:
	case XK_3:
	case XK_4:
	case XK_5:
	case XK_6:
	case XK_7:
	case XK_8:
	case XK_9:
		if (state == GDK_CONTROL_MASK)
			gui_select_nth_tab((event->keyval - XK_0) - 1); /* 0-based list */

	case XK_F1: // F1 help
		if (state == 0)
			on_help_contents_activate(NULL, NULL);
		break;

	case XK_F2: // F2 preferences
		if (state == 0)
			on_preferences_activate(NULL, NULL);
		break;

	case XK_F3: // F3 search
		if (state == 0)
			main_open_search_dialog();
		else if (state == GDK_CONTROL_MASK)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_sidebar),
						      2);
		break;

	case XK_F4: // F4 module manager
		if (state == 0)
			on_module_manager_activate(NULL, NULL);
		else if (state == GDK_CONTROL_MASK)
			gui_close_passage_tab(gtk_notebook_page_num(GTK_NOTEBOOK(widgets.notebook_main),
								    ((PASSAGE_TAB_INFO *)
								     cur_passage_tab)->page_widget));
		break;

	case XK_F10: // Shift-F10 bible module right click
		if (state == GDK_SHIFT_MASK)
			gui_menu_popup(NULL, sM, NULL);
		/* FIXME: needs the html widget as first pram */
		break;

	case XK_F11: // F11 open current bible in separate window, maximized.
		if (state == 0)
			main_dialogs_open(sM, NULL, TRUE);
		break;
	}
	XI_message(("on_vbox1_key_press_event\nkeycode: %d, keysym: %0x, state: %d",
		    event->hardware_keycode, event->keyval, state));
	return FALSE;
}

static gboolean on_vbox1_key_release_event(GtkWidget *widget,
					   GdkEventKey *event,
					   gpointer user_data)
{
	switch (event->keyval) {
	case XK_Shift_L:
	case XK_Shift_R:
		shift_key_pressed = FALSE;
		break;
	}
	return FALSE;
}

#ifdef USE_GTK_3
static void on_notebook_dict_devot_switch_page(GtkNotebook *notebook,
                                               gpointer arg,
                                               gint page_num, GList **tl)
#else
static void on_notebook_dict_devot_switch_page(GtkNotebook *notebook,
                                               GtkNotebookPage *page,
                                               gint page_num, GList **tl)
#endif
{
    if (switching_dict_tab) return;
    if (page_num == 1) {
        if (settings.devotionalmod && *settings.devotionalmod) {
            main_display_devotional(widgets.html_devotional);
        }
    }
}

/******************************************************************************
 * Name
 *   create_mainwindow
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void create_mainwindow(void)
 *
 * Description
 *    create xiphos gui
 *
 * Return value
 *   void
 */

void create_mainwindow(void)
{
	char *imagename;
	GtkWidget *vbox_gs;
	GtkWidget *menu;
	GtkWidget *header_bar;
	GtkWidget *zoom_out_button;
	GtkWidget *zoom_in_button;
	GtkWidget *hbox25;
	GtkWidget *tab_button_icon;
	GtkWidget *label;
#ifndef USE_WEBKIT2
	GtkWidget *scrolledwindow;
#endif
	GtkWidget *box_book;
	GtkWidget *box_devot;
	GdkPixbuf *pixbuf;
	/*
	   GTK_SHADOW_NONE
	   GTK_SHADOW_IN
	   GTK_SHADOW_OUT
	   GTK_SHADOW_ETCHED_IN
	   GTK_SHADOW_ETCHED_OUT
	 */
	settings.shadow_type = GTK_SHADOW_IN;

	XI_print(("%s xiphos-%s\n", "Starting", VERSION));
	XI_print(("%s\n\n", "Building Xiphos interface"));

	widgets.studypad_dialog = NULL;
	widgets.entry_devotional = NULL;

	widgets.html_parallel = NULL;
	widgets.html_parallel_dialog = NULL;

	/* A rough scektch of the main window (widgets.app) and it's children
	 *                widgets.app
	 *                     |
	 *                  vbox_gs
	 *                     |
	 *                  +--|----+
	 *                  |       |
	 *                 menu   hbox25
	 *                           |
	 *                     widgets.epaned
	 *                         |
	 *                 widgets.vboxMain
	 *                 |              |
	 *           widgets.hboxtb   widgets.page
	 *                            |         |
	 *                     widgets.hpaned  nav_toolbar
	 *                     |            |
	 *              widgets.vpaned---+  +----------------------------widgets.vpaned2---------------+
	 *               |               |                                      |                      |
	 * widgets.vbox_previewer   widgets.vbox_text              widgets.notebook_comm_book   widgets.box_dict
	 *                               |                         |                       |
	 *                widgets.notebook_bible_parallel   widgets.box_comm            box_book
	 *                               |
	 *                     widgets.notebook_text
	 *
	 */

	// The toplevel Xiphos window
	widgets.app = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(widgets.app), _("Biblia Elim"));
	gtk_widget_set_name(widgets.app, "elim-app");
	if (settings.darktheme)
		gtk_style_context_add_class(gtk_widget_get_style_context(widgets.app),
					    "elim-dark");
	g_object_set_data(G_OBJECT(widgets.app), "widgets.app", widgets.app);
	{
		int dw = 960, dh = 640;
		gui_default_window_size(&dw, &dh);
		gtk_window_set_default_size(GTK_WINDOW(widgets.app), dw, dh);
	}
	gtk_widget_set_can_focus(widgets.app, 1);
	gtk_window_set_resizable(GTK_WINDOW(widgets.app), TRUE);

	// The app icon.
	// FIXME:: This should be a big copy of the logo because GTK does the scaling (GTK 3.16?)
	imagename = image_locator("biblia-elim.png");
	if (!g_file_test(imagename, G_FILE_TEST_IS_REGULAR)) {
		g_free(imagename);
		imagename = image_locator("gs2-48x48.png");
	}
	pixbuf = gdk_pixbuf_new_from_file(imagename, NULL);
	g_free(imagename);
	gtk_window_set_icon(GTK_WINDOW(widgets.app), pixbuf);
	gtk_window_set_icon_name(GTK_WINDOW(widgets.app), "biblia-elim");
	g_set_prgname("biblia-elim");
	g_set_application_name(_("Biblia Elim"));

	// The main box for our toplevel window.
	UI_VBOX(vbox_gs, FALSE, 0);
	gtk_widget_show(vbox_gs);
	gtk_container_add(GTK_CONTAINER(widgets.app), vbox_gs);

	// Add the main menu, moved into the header bar instead of a
	// full-width classic menu bar row below the titlebar. Packed
	// directly (not wrapped in a GtkMenuButton popover -- a GtkMenuBar's
	// own submenus rely on a pointer/keyboard grab that does not survive
	// being nested inside a GtkPopover's own grab in GTK3, which is why
	// an earlier popover-based version of this had unresponsive menu
	// items). Packing it straight into the header bar keeps the menu
	// bar's normal, working click/submenu behavior while still removing
	// the separate full-width menu row.
	menu = gui_create_main_menu();
	header_menu = menu;
	gtk_widget_show(menu);

	header_bar = gtk_header_bar_new();
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), _("Biblia Elim"));
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), _("Estudio bíblico"));
	gtk_style_context_add_class(gtk_widget_get_style_context(header_bar),
				    "elim-header");
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), menu);

	{
		GtkWidget *sidebar_icon = gtk_image_new_from_icon_name(
		    "view-sidebar-symbolic", GTK_ICON_SIZE_BUTTON);
		widgets.sidebar_toggle_button = gtk_toggle_button_new();
		gtk_container_add(GTK_CONTAINER(widgets.sidebar_toggle_button),
				  sidebar_icon);
		gtk_widget_show(sidebar_icon);
	}
	gtk_widget_set_tooltip_text(widgets.sidebar_toggle_button,
				    _("Mostrar/ocultar panel lateral (Ctrl+S)"));
	gtk_style_context_add_class(
	    gtk_widget_get_style_context(widgets.sidebar_toggle_button), "flat");
	gtk_style_context_add_class(
	    gtk_widget_get_style_context(widgets.sidebar_toggle_button), "circular");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.sidebar_toggle_button),
				     settings.showshortcutbar);
	g_signal_connect(widgets.sidebar_toggle_button, "toggled",
			 G_CALLBACK(on_sidebar_toggle_button_toggled), NULL);
	gtk_widget_show(widgets.sidebar_toggle_button);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), widgets.sidebar_toggle_button);

	widgets.reading_mode_button = new_open_bible_toggle(
	    _("Modo lectura: solo la Biblia (Ctrl+Mayús+F)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.reading_mode_button),
				     settings.reading_mode);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), widgets.reading_mode_button);

	// Quick text-size controls -- surfaces the existing base-font-size
	// bias (previously reachable only via Ctrl+Shift+'+'/Ctrl+'-') as
	// visible buttons, Kindle-style.
	zoom_in_button = gtk_button_new_from_icon_name("zoom-in-symbolic",
						       GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(zoom_in_button, _("Aumentar tamaño del texto"));
	gtk_style_context_add_class(gtk_widget_get_style_context(zoom_in_button), "flat");
	gtk_style_context_add_class(gtk_widget_get_style_context(zoom_in_button), "circular");
	g_signal_connect(zoom_in_button, "clicked",
			 G_CALLBACK(on_zoom_in_clicked), NULL);
	gtk_widget_show(zoom_in_button);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), zoom_in_button);

	zoom_out_button = gtk_button_new_from_icon_name("zoom-out-symbolic",
							GTK_ICON_SIZE_BUTTON);
	gtk_style_context_add_class(gtk_widget_get_style_context(zoom_out_button), "flat");
	gtk_style_context_add_class(gtk_widget_get_style_context(zoom_out_button), "circular");
	gtk_widget_set_tooltip_text(zoom_out_button, _("Reducir tamaño del texto"));
	g_signal_connect(zoom_out_button, "clicked",
			 G_CALLBACK(on_zoom_out_clicked), NULL);
	gtk_widget_show(zoom_out_button);
	gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), zoom_out_button);

	gtk_widget_show(header_bar);
	gtk_window_set_titlebar(GTK_WINDOW(widgets.app), header_bar);

	// Another box
	UI_HBOX(hbox25, FALSE, 0);
	gtk_widget_show(hbox25);
	gtk_box_pack_start(GTK_BOX(vbox_gs), hbox25, TRUE, TRUE, 0);

	// widgets.epaned
	widgets.epaned = UI_HPANE();
	gtk_widget_show(widgets.epaned);
#if !GTK_CHECK_VERSION(3, 14, 0)
	gtk_container_set_border_width(GTK_CONTAINER(widgets.epaned), 4);
#endif
	gtk_box_pack_start(GTK_BOX(hbox25), widgets.epaned, TRUE, TRUE, 0);
	// Another box
	UI_VBOX(widgets.vboxMain, FALSE, 0);
	gtk_widget_show(widgets.vboxMain);
	gtk_paned_pack2(GTK_PANED(widgets.epaned), widgets.vboxMain, TRUE, TRUE);
#if !GTK_CHECK_VERSION(3, 14, 0)
	gtk_container_set_border_width(GTK_CONTAINER(widgets.vboxMain), 2);
#endif

	/*
	 * Notebook to have separate passages opened at once the passages are not
	 * actually open but are switched between similar to bookmarks
	 */
	UI_HBOX(widgets.hboxtb, FALSE, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(widgets.hboxtb),
				    "elim-tabstrip");
	if (settings.browsing)
		gtk_widget_show(widgets.hboxtb);
	gtk_box_pack_start(GTK_BOX(widgets.vboxMain), widgets.hboxtb, FALSE, FALSE, 0);

	widgets.button_new_tab = gtk_button_new();
	// Don't show button here in case !settings.browsing

#if GTK_CHECK_VERSION(3, 10, 0)
	tab_button_icon = gtk_image_new_from_icon_name("tab-new-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
#else
	tab_button_icon = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_SMALL_TOOLBAR);
#endif

	gtk_widget_show(tab_button_icon);
	gtk_container_add(GTK_CONTAINER(widgets.button_new_tab), tab_button_icon);
	gtk_button_set_relief(GTK_BUTTON(widgets.button_new_tab), GTK_RELIEF_NONE);
	gtk_box_pack_start(GTK_BOX(widgets.hboxtb), widgets.button_new_tab, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(widgets.button_new_tab, _("Open a new tab"));

	widgets.notebook_main = gtk_notebook_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(widgets.notebook_main),
				    "elim-tabs");
	gtk_widget_show(widgets.notebook_main);
	gtk_box_pack_start(GTK_BOX(widgets.hboxtb), widgets.notebook_main, TRUE, TRUE, 0);
	gtk_widget_set_size_request(widgets.notebook_main, -1, 25);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(widgets.notebook_main), TRUE);
	gtk_notebook_popup_enable(GTK_NOTEBOOK(widgets.notebook_main));
	gtk_notebook_set_show_border(GTK_NOTEBOOK(widgets.notebook_main), FALSE);
	// Main passage tabbed notebook end

	// Another box
	UI_VBOX(widgets.page, FALSE, 0);
	gtk_widget_show(widgets.page);
	gtk_box_pack_start(GTK_BOX(widgets.vboxMain), widgets.page, TRUE, TRUE, 0);

	//nav toolbar
	widgets.nav_toolbar = gui_navbar_versekey_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(widgets.nav_toolbar),
				    "elim-navbar");
	gtk_box_pack_start(GTK_BOX(widgets.page), widgets.nav_toolbar, FALSE, FALSE, 0);

	// widgets.hpaned
	widgets.hpaned = UI_HPANE();
	gtk_widget_show(widgets.hpaned);

	/* Wrapped in an overlay so reading mode can float nav_toolbar over
	 * the content (hover-to-reveal, see gui_toggle_reading_mode()) --
	 * with no overlay child added, this behaves exactly like packing
	 * hpaned straight into widgets.page, so it changes nothing outside
	 * reading mode. */
	widgets.reading_mode_overlay = gtk_overlay_new();
	gtk_widget_show(widgets.reading_mode_overlay);
	gtk_container_add(GTK_CONTAINER(widgets.reading_mode_overlay), widgets.hpaned);
	gtk_box_pack_start(GTK_BOX(widgets.page), widgets.reading_mode_overlay, TRUE, TRUE, 0);

	// widgets.vpaned
	widgets.vpaned = UI_VPANE();
	gtk_widget_show(widgets.vpaned);
	gtk_widget_set_size_request(widgets.vpaned, 50, -1);
	gtk_paned_pack1(GTK_PANED(widgets.hpaned), widgets.vpaned, TRUE, FALSE);

	// widgets.vpaned2
	widgets.vpaned2 = UI_VPANE();
	gtk_widget_set_size_request(widgets.vpaned2, 50, -1);

	// widgets.vbox_text
	UI_VBOX(widgets.vbox_text, FALSE, 0);
	gtk_widget_show(widgets.vbox_text);
	{
		GtkWidget *ov = gtk_overlay_new();
		gtk_widget_show(ov);
		gtk_container_add(GTK_CONTAINER(ov), widgets.vbox_text);
		gtk_paned_pack1(GTK_PANED(widgets.vpaned), ov, TRUE, TRUE);

		reading_exit_button = new_open_bible_toggle(
		    _("Salir del modo lectura"));
		gtk_style_context_add_class(
		    gtk_widget_get_style_context(reading_exit_button),
		    "reading-exit");
		gtk_widget_set_halign(reading_exit_button, GTK_ALIGN_END);
		gtk_widget_set_valign(reading_exit_button, GTK_ALIGN_START);
		gtk_widget_set_margin_top(reading_exit_button, 10);
		gtk_widget_set_margin_end(reading_exit_button, 14);
		gtk_overlay_add_overlay(GTK_OVERLAY(ov), reading_exit_button);
		gtk_widget_hide(reading_exit_button);

		/* Reading mode hides every toolbar and menu, so the
		 * comparison had no way to be found short of knowing the
		 * keyboard shortcut. It gets a control of its own, next to
		 * the exit button and styled the same, shown and hidden
		 * with it. */
		reading_compare_button = gtk_toggle_button_new_with_label("A|B");
		gtk_widget_set_tooltip_text(
		    reading_compare_button,
		    _("Comparar versiones en columnas (Ctrl+Shift+K)"));
		gtk_widget_set_can_focus(reading_compare_button, FALSE);
		gtk_style_context_add_class(
		    gtk_widget_get_style_context(reading_compare_button),
		    "reading-exit");
		gtk_widget_set_halign(reading_compare_button, GTK_ALIGN_END);
		gtk_widget_set_valign(reading_compare_button, GTK_ALIGN_START);
		gtk_widget_set_margin_top(reading_compare_button, 10);
		gtk_widget_set_margin_end(reading_compare_button, 66);
		g_signal_connect(reading_compare_button, "toggled",
				 G_CALLBACK(on_reading_compare_toggled), NULL);
		gtk_overlay_add_overlay(GTK_OVERLAY(ov), reading_compare_button);
		gtk_widget_hide(reading_compare_button);

		/* Which versions are being compared was only editable by
		 * hand-editing modules/parallels in settings.xml. */
		reading_compare_pick = gtk_button_new_with_label("\u25be");
		gtk_widget_set_tooltip_text(reading_compare_pick,
					    _("Elegir qué versiones comparar"));
		gtk_widget_set_can_focus(reading_compare_pick, FALSE);
		gtk_style_context_add_class(
		    gtk_widget_get_style_context(reading_compare_pick),
		    "reading-exit");
		gtk_widget_set_halign(reading_compare_pick, GTK_ALIGN_END);
		gtk_widget_set_valign(reading_compare_pick, GTK_ALIGN_START);
		gtk_widget_set_margin_top(reading_compare_pick, 10);
		gtk_widget_set_margin_end(reading_compare_pick, 118);
		g_signal_connect(reading_compare_pick, "clicked",
				 G_CALLBACK(on_reading_compare_pick), NULL);
		gtk_overlay_add_overlay(GTK_OVERLAY(ov), reading_compare_pick);
		gtk_widget_hide(reading_compare_pick);
	}

	// Bible/parallel notebook
	widgets.notebook_bible_parallel = gtk_notebook_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(widgets.notebook_bible_parallel),
				    "elim-view-tabs");
	gtk_widget_show(widgets.notebook_bible_parallel);
	gtk_box_pack_start(GTK_BOX(widgets.vbox_text), widgets.notebook_bible_parallel, TRUE, TRUE, 0);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(widgets.notebook_bible_parallel), GTK_POS_BOTTOM);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(widgets.notebook_bible_parallel), TRUE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(widgets.notebook_bible_parallel), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(widgets.notebook_bible_parallel), 1);

	g_signal_connect(G_OBJECT(widgets.notebook_bible_parallel), "switch-page",
			 G_CALLBACK(on_notebook_bible_parallel_switch_page), NULL);

	// Text notebook (The bible text show in the standard view)
	widgets.notebook_text = gui_create_bible_pane();
	gtk_container_add(GTK_CONTAINER(widgets.notebook_bible_parallel), widgets.notebook_text);

	label = gtk_label_new(_("Standard View"));
	gtk_widget_show(label);
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets.notebook_bible_parallel), gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel), 0), label);

	// Another box (For the previewer?)
	UI_VBOX(widgets.vbox_previewer, FALSE, 0);
	gtk_widget_show(widgets.vbox_previewer);
	gtk_paned_pack2(GTK_PANED(widgets.vpaned), widgets.vbox_previewer, TRUE, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(widgets.vbox_previewer), 2);

#ifndef USE_WEBKIT2
	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow);
	gtk_box_pack_start(GTK_BOX(widgets.vbox_previewer), scrolledwindow, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *) scrolledwindow, settings.shadow_type);
#endif
	widgets.html_previewer_text = GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
	gtk_widget_show(widgets.html_previewer_text);
#ifdef USE_WEBKIT2
	gtk_box_pack_start(GTK_BOX(widgets.vbox_previewer), widgets.html_previewer_text, TRUE, TRUE, 0);
#else
	gtk_container_add(GTK_CONTAINER(scrolledwindow), widgets.html_previewer_text);
#endif

	// Commentary/book notebook
	widgets.notebook_comm_book = gtk_notebook_new();
	gtk_widget_show(widgets.notebook_comm_book);

	gtk_paned_pack1(GTK_PANED(widgets.vpaned2), widgets.notebook_comm_book, TRUE, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(widgets.notebook_comm_book), 1);

	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(widgets.notebook_comm_book), GTK_POS_BOTTOM);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(widgets.notebook_comm_book), TRUE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(widgets.notebook_comm_book), FALSE);

	// Commentary pane
	widgets.box_comm = gui_create_commentary_pane();
	gtk_container_add(GTK_CONTAINER(widgets.notebook_comm_book), widgets.box_comm);

	label = gtk_label_new(_("Commentary View"));
	gtk_widget_show(label);
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets.notebook_comm_book), gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_comm_book), 0), label);

	// Book pane
	box_book = gui_create_book_pane();
	gtk_container_add(GTK_CONTAINER(widgets.notebook_comm_book), box_book);

	label = gtk_label_new(_("Book View"));
	gtk_widget_show(label);
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets.notebook_comm_book), gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_comm_book), 1), label);

	// Notas pane (nota del versículo enfocado)
	{
		GtkWidget *box_notas = gui_create_notes_pane();
		gtk_container_add(GTK_CONTAINER(widgets.notebook_comm_book), box_notas);
		label = gtk_label_new(_("Notas"));
		gtk_widget_show(label);
		gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets.notebook_comm_book), gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_comm_book), 2), label);
	}

	// Dict/Devotional notebook
	widgets.notebook_dict_devot = gtk_notebook_new();
	gtk_widget_show(widgets.notebook_dict_devot);
	gtk_paned_pack2(GTK_PANED(widgets.vpaned2), widgets.notebook_dict_devot, TRUE, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(widgets.notebook_dict_devot), 1);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(widgets.notebook_dict_devot), GTK_POS_BOTTOM);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(widgets.notebook_dict_devot), TRUE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(widgets.notebook_dict_devot), FALSE);

	// Tab 0 : Dictionary
	widgets.box_dict = gui_create_dictionary_pane();
	gtk_container_add(GTK_CONTAINER(widgets.notebook_dict_devot), widgets.box_dict);
	label = gtk_label_new(_("Dictionary"));
	gtk_widget_show(label);
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets.notebook_dict_devot),
                           gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_dict_devot), 0),
                           label);

	// Tab 1 : Devotional
box_devot = gui_create_devotional_pane();
	gtk_container_add(GTK_CONTAINER(widgets.notebook_dict_devot), box_devot);
	label = gtk_label_new(_("Devotional"));
	gtk_widget_show(label);
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets.notebook_dict_devot),
	    gtk_notebook_get_nth_page(GTK_NOTEBOOK(widgets.notebook_dict_devot), 1),
	    label);


	// Statusbar
	widgets.appbar = gtk_statusbar_new();

#ifndef USE_GTK_3
	gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(widgets.appbar), TRUE);
#endif
	gtk_box_pack_start(GTK_BOX(vbox_gs), widgets.appbar, FALSE, TRUE, 0);
	gui_set_statusbar(_("Bienvenido a Biblia Elim"));

	gtk_paned_pack2(GTK_PANED(widgets.hpaned), widgets.vpaned2, TRUE, FALSE);
	gtk_widget_grab_focus(navbar_versekey.lookup_entry);

	{
		int w = settings.gs_width, h = settings.gs_height;
		int dw = 960, dh = 640;
		gui_default_window_size(&dw, &dh);
		if (w < 640 || h < 400) {
			w = dw;
			h = dh;
		}
		gtk_window_set_default_size(GTK_WINDOW(widgets.app), w, h);
	}
	gtk_widget_show_all(widgets.app);

	if (reading_exit_button && !settings.reading_mode)
		gtk_widget_hide(reading_exit_button);
	if (settings.statusbar != 1)
		gtk_widget_hide(widgets.appbar);
	/* gtk_widget_show_all() above just unconditionally re-showed every
	 * widget in the window, including the "Comparar" split-view panel
	 * gui_lectura_sync_wrap() had already hidden a moment earlier
	 * (on-demand only, off by default) -- put it back the way the
	 * user's settings actually say. */
	gui_lectura_sync_set_visible(settings.show_lectura_sync);

	/* must connect signals *after* instantiating window above, */
	/* immediately above, otherwise window creation induces */
	/* configure_event, wiping out user's saved geometry specs. */
	/* *important*: drain gtk event queue first (i.e. sync). */

	sync_windows();
	g_signal_connect((gpointer)vbox_gs, "key_press_event", G_CALLBACK(on_vbox1_key_press_event), NULL);
	g_signal_connect((gpointer)vbox_gs, "key_release_event", G_CALLBACK(on_vbox1_key_release_event), NULL);

	g_signal_connect(G_OBJECT(widgets.notebook_comm_book), "switch_page", G_CALLBACK(on_notebook_comm_book_switch_page), NULL);
	
	g_signal_connect(G_OBJECT(widgets.notebook_dict_devot), "switch_page", G_CALLBACK(on_notebook_dict_devot_switch_page), NULL);

	g_signal_connect(G_OBJECT(widgets.app), "delete_event", G_CALLBACK(delete_event), NULL);

	g_signal_connect((gpointer)widgets.app, "configure_event", G_CALLBACK(on_configure_event), NULL);
	g_signal_connect(G_OBJECT(widgets.epaned), "button_release_event", G_CALLBACK(epaned_button_release_event), (gchar *)"epaned");
	g_signal_connect(G_OBJECT(widgets.vpaned), "button_release_event", G_CALLBACK(epaned_button_release_event), (gchar *)"vpaned");
	g_signal_connect(G_OBJECT(widgets.vpaned2), "button_release_event", G_CALLBACK(epaned_button_release_event), (gchar *)"vpaned2");
	g_signal_connect(G_OBJECT(widgets.hpaned), "button_release_event", G_CALLBACK(epaned_button_release_event), (gchar *)"hpaned1");

	main_window_created = TRUE;
}

/******************************************************************************
 * Name
 *   gui_show_main_window
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   void gui_show_main_window(void)
 *
 * Description
 *    display the app after all is created
 *
 * Return value
 *   void
 */

void gui_show_main_window(void)
{
	gtk_widget_show(widgets.app);
}

/******************************************************************************
 * Name
 *   gui_notebook_dict_goto_dict
 *
 */
void gui_notebook_dict_goto_dict(void)
{
    switching_dict_tab = TRUE;
    gtk_notebook_set_current_page(
        GTK_NOTEBOOK(widgets.notebook_dict_devot), 0);
    switching_dict_tab = FALSE;
}
