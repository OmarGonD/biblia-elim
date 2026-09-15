/*
 * Xiphos Bible Study Tool
 * bibletext.c - gui for Bible text modules
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
#include <stdlib.h>
#include <gtk/gtk.h>

#include "xiphos_html/xiphos_html.h"

#include "gui/xiphos.h"
#include "gui/bibletext.h"
#include "gui/bibletext_dialog.h"
#include "gui/bookmark_dialog.h"
#include "gui/bookmarks_treeview.h"
#include "gui/export_dialog.h"
#include "gui/sidebar.h"
#include "gui/cipher_key_dialog.h"
#include "gui/main_menu.h"
#include "gui/main_window.h"
#include "gui/menu_popup.h"
#include "gui/dialog.h"
#include "gui/font_dialog.h"
#include "gui/dictlex.h"
#include "gui/diccionario.h"
#include "gui/interlineal.h"
#include "gui/lectura_sync.h"
#include "gui/panel_load_state.h"
#include "gui/tabbed_browser.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/settings.h"
#include "main/interlineal.h"
#include "main/lectura_sync.h"
#include "main/lists.h"
#include "main/navbar_versekey.h"
#include "main/reading_focus.h"
#include "main/wheel_scroll.h"
#include "main/sword.h"
#include "main/url.hh"
#include "main/xml.h"
#include "main/global_ops.hh"
#include "main/display.hh"

gboolean shift_key_pressed = FALSE;
guint scroll_adj_signal;
GtkAdjustment *adjustment;

/* Ctrl+scroll over the text pane bumps this Bible surface's persistent zoom,
 * the same operation as the header-bar buttons / Ctrl+Shift+'+'/Ctrl+'-', instead of
 * scrolling the page -- a standard e-reader/browser gesture that was
 * entirely missing. Plain scroll (no Ctrl) is left alone so normal page
 * scrolling still works. */
static gboolean
_scroll_zoom_cb(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
	if (!(event->state & GDK_CONTROL_MASK))
		return FALSE;

	if (event->direction == GDK_SCROLL_UP)
		wk_html_zoom(WK_HTML(widgets.html_text), TRUE);
	else if (event->direction == GDK_SCROLL_DOWN)
		wk_html_zoom(WK_HTML(widgets.html_text), FALSE);
	else
		return FALSE;

	return TRUE;
}

static gboolean
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, gpointer user_data)
{
	gui_menu_popup(html, settings.MainWindowModule, NULL);
	return TRUE;
}

/******************************************************************************
 * Name
 *   gui_create_bible_pane
 *
 * Synopsis
 *   #include "gui/bibletext.h"
 *
 *   GtkWidget *gui_create_bible_pane(void)
 *
 * Description
 *
 *
 * Return value
 *   GtkWidget*
 */

/* Kindle-style selection highlighting on the native GtkTextView pane. */

#define DEFAULT_HIGHLIGHT_COLOR "#FFEB3B"

static const gchar *highlight_palette[] = {
	"#FFEB3B",
	"#A5D6A7",
	"#90CAF9",
	"#F48FB1",
	"#FFCC80",
	"#CE93D8",
	"#EF9A9A",
};
#define HIGHLIGHT_PALETTE_N (sizeof(highlight_palette) / sizeof(highlight_palette[0]))

static GtkWidget *highlight_toolbar_popover = NULL;
static GtkWidget *highlight_color_popover = NULL;
static GtkWidget *highlight_note_popover = NULL;
static GtkWidget *highlight_color_button = NULL;
static GtkWidget *highlight_color_da = NULL;
static GtkWidget *highlight_note_button = NULL;
static GtkWidget *highlight_note_link_box = NULL;
static GtkTextView *highlight_note_textview = NULL;
static gchar *current_highlight_label = NULL;
static gchar *current_highlight_text = NULL;
static GtkTextMark *pending_start_mark = NULL;
static GtkTextMark *pending_end_mark = NULL;
static gchar *pending_osisref = NULL;
static gchar *pending_text = NULL;
static gint pending_pos = -1;
static const gchar *pending_color = DEFAULT_HIGHLIGHT_COLOR;

static void free_highlight_segment(gpointer data);

static GtkTextView *
bible_view(void)
{
	if (!widgets.html_text)
		return NULL;
	return wk_html_get_view(WK_HTML(widgets.html_text));
}

static gboolean
on_circle_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	const gchar *color = g_object_get_data(G_OBJECT(widget), "swatch-color");
	GdkRGBA rgba;
	gint w = gtk_widget_get_allocated_width(widget);
	gint h = gtk_widget_get_allocated_height(widget);
	double side = MIN(w, h);
	double radius = MAX((side / 2.0) - 0.5, 1.0);

	(void)user_data;
	if (!color)
		color = DEFAULT_HIGHLIGHT_COLOR;
	if (!gdk_rgba_parse(&rgba, color))
		gdk_rgba_parse(&rgba, DEFAULT_HIGHLIGHT_COLOR);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
	cairo_arc(cr, w / 2.0, h / 2.0, radius, 0, 2 * G_PI);
	cairo_set_source_rgb(cr, rgba.red, rgba.green, rgba.blue);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.28);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	return TRUE;
}

static void
set_circle_color(GtkWidget *da, const gchar *color)
{
	if (!da)
		return;
	g_object_set_data_full(G_OBJECT(da), "swatch-color",
			       g_strdup(color ? color : DEFAULT_HIGHLIGHT_COLOR),
			       g_free);
	gtk_widget_queue_draw(da);
}

static GtkWidget *
make_circle_swatch(const gchar *color, int diameter, GtkWidget **da_out)
{
	GtkWidget *da = gtk_drawing_area_new();
	GtkWidget *btn = gtk_button_new();

	gtk_widget_set_size_request(da, diameter, diameter);
	gtk_widget_set_hexpand(da, FALSE);
	gtk_widget_set_vexpand(da, FALSE);
	gtk_widget_set_halign(da, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(da, GTK_ALIGN_CENTER);
	set_circle_color(da, color);
	g_signal_connect(da, "draw", G_CALLBACK(on_circle_draw), NULL);

	gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus(btn, FALSE);
	gtk_widget_set_hexpand(btn, FALSE);
	gtk_widget_set_vexpand(btn, FALSE);
	gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request(btn, diameter + 4, diameter + 4);
	gtk_style_context_add_class(gtk_widget_get_style_context(btn),
				    "highlight-swatch");
	gtk_container_add(GTK_CONTAINER(btn), da);
	gtk_widget_show(da);
	if (da_out)
		*da_out = da;
	return btn;
}

static void
clear_pending_selection(void)
{
	GtkTextView *view = bible_view();

	if (view && pending_start_mark) {
		GtkTextBuffer *buf = gtk_text_view_get_buffer(view);
		if (gtk_text_buffer_get_mark(buf, "elim-hl-start"))
			gtk_text_buffer_delete_mark_by_name(buf, "elim-hl-start");
		if (gtk_text_buffer_get_mark(buf, "elim-hl-end"))
			gtk_text_buffer_delete_mark_by_name(buf, "elim-hl-end");
	}
	pending_start_mark = NULL;
	pending_end_mark = NULL;
	pending_pos = -1;
	g_clear_pointer(&pending_osisref, g_free);
	g_clear_pointer(&pending_text, g_free);
}

static gchar *
commit_pending_highlight(const gchar *color)
{
	GtkTextView *view;
	GtkTextBuffer *buf;
	GtkTextIter start, end;
	HighlightSegment *seg;
	GList *segments;
	gchar *gid;
	const gchar *use_color = color && *color ? color : pending_color;

	if (current_highlight_label)
		return current_highlight_label;
	if (!pending_start_mark || !pending_end_mark || !pending_text)
		return NULL;

	view = bible_view();
	if (!view)
		return NULL;
	buf = gtk_text_view_get_buffer(view);
	if (!gtk_text_buffer_get_mark(buf, "elim-hl-start") ||
	    gtk_text_mark_get_deleted(pending_start_mark))
		return NULL;

	gtk_text_buffer_get_iter_at_mark(buf, &start, pending_start_mark);
	gtk_text_buffer_get_iter_at_mark(buf, &end, pending_end_mark);
	if (gtk_text_iter_equal(&start, &end))
		return NULL;

	seg = g_new0(HighlightSegment, 1);
	seg->osisref = g_strdup(pending_osisref);
	seg->text = g_strdup(pending_text);
	seg->pos = pending_pos;
	segments = g_list_append(NULL, seg);
	gid = highlight_create_group(settings.MainWindowModule, segments, use_color);
	wk_html_highlight_apply(WK_HTML(widgets.html_text), &start, &end, gid, use_color);
	gtk_text_buffer_select_range(buf, &end, &end);

	g_free(current_highlight_label);
	current_highlight_label = gid;
	pending_color = use_color;
	set_circle_color(highlight_color_da, use_color);
	if (highlight_color_button)
		gtk_widget_set_tooltip_text(highlight_color_button,
					    _("Color de subrayado"));
	g_list_free_full(segments, free_highlight_segment);
	clear_pending_selection();
	return current_highlight_label;
}

static void
on_highlight_delete_clicked(GtkButton *button, gpointer user_data)
{
	if (current_highlight_label) {
		wk_html_highlight_remove(WK_HTML(widgets.html_text),
					 current_highlight_label);
		highlight_remove(current_highlight_label);
		g_clear_pointer(&current_highlight_label, g_free);
	}
	clear_pending_selection();
	gtk_popover_popdown(GTK_POPOVER(highlight_toolbar_popover));
}

static void
on_highlight_color_swatch_clicked(GtkButton *button, gpointer user_data)
{
	const gchar *color = (const gchar *)user_data;

	pending_color = color;
	set_circle_color(highlight_color_da, color);
	if (commit_pending_highlight(color) && current_highlight_label) {
		wk_html_highlight_set_color(WK_HTML(widgets.html_text),
					    current_highlight_label, color);
		highlight_set_color(current_highlight_label, color);
	}
	gtk_popover_popdown(GTK_POPOVER(highlight_color_popover));
}

static void
on_highlight_underline_clicked(GtkButton *button, gpointer user_data)
{
	/* First click on a fresh selection applies the highlight. Once it
	 * is already underlined, a later click opens the color palette. */
	if (!current_highlight_label) {
		commit_pending_highlight(pending_color);
		return;
	}

	if (!highlight_color_popover) {
		highlight_color_popover = gtk_popover_new(highlight_toolbar_popover);
		GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		gtk_container_set_border_width(GTK_CONTAINER(box), 6);
		guint i;
		for (i = 0; i < HIGHLIGHT_PALETTE_N; ++i) {
			GtkWidget *swatch = make_circle_swatch(highlight_palette[i], 22, NULL);
			g_signal_connect(swatch, "clicked",
					 G_CALLBACK(on_highlight_color_swatch_clicked),
					 (gpointer)highlight_palette[i]);
			gtk_widget_show(swatch);
			gtk_box_pack_start(GTK_BOX(box), swatch, FALSE, FALSE, 0);
		}
		gtk_widget_show(box);
		gtk_container_add(GTK_CONTAINER(highlight_color_popover), box);
	}
	gtk_popover_set_relative_to(GTK_POPOVER(highlight_color_popover), GTK_WIDGET(button));
	gtk_popover_popup(GTK_POPOVER(highlight_color_popover));
}

/* Generic "type/edit a note" modal, used both as the on-screen highlight
 * popover's fallback (when the highlight isn't part of the currently
 * rendered chapter) and by the whole-verse note dialog below, which has
 * no on-screen span to pin a popover to. */
static gboolean
run_note_edit_dialog(const gchar *title, const gchar *initial_text, gchar **out_text)
{
	GtkWidget *dialog, *content, *scroll, *tv;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gint response;

	dialog = gtk_dialog_new_with_buttons(
	    title, GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widgets.html_text))),
	    GTK_DIALOG_MODAL,
	    _("_Cancelar"), GTK_RESPONSE_CANCEL,
	    _("_Guardar"), GTK_RESPONSE_OK,
	    NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content), 8);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scroll, 320, 160);
	tv = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
	gtk_text_buffer_set_text(buffer, initial_text ? initial_text : "", -1);
	gtk_container_add(GTK_CONTAINER(scroll), tv);
	gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 4);

	gtk_widget_show_all(dialog);
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == GTK_RESPONSE_OK) {
		gtk_text_buffer_get_start_iter(buffer, &start);
		gtk_text_buffer_get_end_iter(buffer, &end);
		*out_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	}
	gtk_widget_destroy(dialog);
	return response == GTK_RESPONSE_OK;
}

/* Small "type a Bible reference" modal used to enlazar (link) a note to
 * another verse's note. */
static gboolean
run_reference_entry_dialog(const gchar *title, gchar **out_text)
{
	GtkWidget *dialog, *content, *label, *entry;
	gint response;

	dialog = gtk_dialog_new_with_buttons(
	    title, GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widgets.html_text))),
	    GTK_DIALOG_MODAL,
	    _("_Cancelar"), GTK_RESPONSE_CANCEL,
	    _("_Enlazar"), GTK_RESPONSE_OK,
	    NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content), 8);

	label = gtk_label_new(_("Referencia del versículo (p. ej. Juan 3:16):"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 4);

	entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 4);

	gtk_widget_show_all(dialog);
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	*out_text = (response == GTK_RESPONSE_OK)
			? g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)))
			: NULL;
	gtk_widget_destroy(dialog);
	return response == GTK_RESPONSE_OK;
}

/* Jump to the verse a linked note_key ("HL:<gid>" or "MV:<osisref>")
 * points at, reusing the same navigation the bookmarks list already
 * uses for typed cross-references. */
static void
navigate_to_note_key(const gchar *note_key)
{
	gchar *osisref = highlight_note_key_osisref(note_key);
	gchar *url;

	if (!osisref)
		return;
	url = g_strdup_printf("passagestudy.jsp?action=showBookmark&type=currentTab&"
			      "value=%s&module=%s",
			      osisref, settings.MainWindowModule);
	main_url_handler(url, TRUE);
	g_free(url);
	g_free(osisref);
}

static void
on_linked_note_clicked(GtkButton *button, gpointer user_data)
{
	const gchar *note_key = (const gchar *)g_object_get_data(G_OBJECT(button), "note-key");
	(void)user_data;
	navigate_to_note_key(note_key);
}

/* Rebuilds the "Notas enlazadas" list inside the selection-highlight
 * note popover for whichever highlight is currently open. */
static void
rebuild_linked_notes_box(const gchar *note_key)
{
	GList *links, *n;

	if (!highlight_note_link_box)
		return;

	gtk_container_foreach(GTK_CONTAINER(highlight_note_link_box),
			      (GtkCallback)gtk_widget_destroy, NULL);
	if (!note_key)
		return;

	links = highlight_list_linked_notes(note_key);
	if (!links)
		return;

	GtkWidget *hdr = gtk_label_new(_("Notas enlazadas:"));
	gtk_widget_set_halign(hdr, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(highlight_note_link_box), hdr, FALSE, FALSE, 0);

	for (n = links; n; n = n->next) {
		gchar *key = (gchar *)n->data;
		gchar *osis = highlight_note_key_osisref(key);
		GtkWidget *btn = gtk_button_new_with_label(osis ? osis : key);
		gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
		gtk_widget_set_halign(btn, GTK_ALIGN_START);
		g_object_set_data_full(G_OBJECT(btn), "note-key", g_strdup(key), g_free);
		g_signal_connect(btn, "clicked", G_CALLBACK(on_linked_note_clicked), NULL);
		gtk_box_pack_start(GTK_BOX(highlight_note_link_box), btn, FALSE, FALSE, 0);
		g_free(osis);
	}
	g_list_free_full(links, g_free);
	gtk_widget_show_all(highlight_note_link_box);
}

/* Prompt for a reference and link `source_key` (a note_key) to the
 * verse typed in. Shared by the highlight-note popover and the
 * whole-verse notes dialog below. */
static void
do_link_from_key(const gchar *source_key)
{
	gchar *typed = NULL;

	if (!source_key)
		return;
	if (run_reference_entry_dialog(_("Enlazar con otro versículo"), &typed) &&
	    typed && *typed) {
		GList *refs = main_parse_verse_list(settings.MainWindowModule, typed,
						    settings.currentverse);
		if (refs) {
			gchar *osis = g_strdup(main_get_osisref_from_key(
			    settings.MainWindowModule, (const char *)refs->data));
			gchar *target_key = highlight_note_key_verse(osis);
			highlight_link_notes(source_key, target_key);
			g_free(osis);
			g_free(target_key);
		} else {
			gui_generic_warning(_("No se pudo interpretar esa referencia."));
		}
		g_list_free_full(refs, g_free);
	}
	g_free(typed);
}

static void
on_highlight_link_clicked(GtkButton *button, gpointer user_data)
{
	gchar *note_key;
	(void)button;
	(void)user_data;

	if (!current_highlight_label)
		return;
	note_key = highlight_note_key_group(current_highlight_label);
	do_link_from_key(note_key);
	rebuild_linked_notes_box(note_key);
	g_free(note_key);
}

static void
on_highlight_note_save_clicked(GtkButton *button, gpointer user_data)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(highlight_note_textview);
	GtkTextIter start, end;
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	gchar *note = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	if (current_highlight_label)
		highlight_set_note(current_highlight_label, note);

	g_free(note);
	gtk_popover_popdown(GTK_POPOVER(highlight_note_popover));
	gtk_popover_popdown(GTK_POPOVER(highlight_toolbar_popover));
}

static void
on_highlight_note_clicked(GtkButton *button, gpointer user_data)
{
	gchar *existing_note;
	gchar *note_key;

	if (!current_highlight_label)
		commit_pending_highlight(pending_color);

	if (!highlight_note_popover) {
		highlight_note_popover = gtk_popover_new(highlight_toolbar_popover);
		GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
		gtk_container_set_border_width(GTK_CONTAINER(box), 6);

		GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
		gtk_widget_set_size_request(scroll, 240, 100);
		GtkWidget *tv = gtk_text_view_new();
		highlight_note_textview = GTK_TEXT_VIEW(tv);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD);
		gtk_container_add(GTK_CONTAINER(scroll), tv);

		GtkWidget *save = gtk_button_new_with_label(_("Guardar nota"));
		g_signal_connect(save, "clicked",
				 G_CALLBACK(on_highlight_note_save_clicked), NULL);

		GtkWidget *link_btn = gtk_button_new_with_label(_("Enlazar con otro versículo"));
		g_signal_connect(link_btn, "clicked",
				 G_CALLBACK(on_highlight_link_clicked), NULL);

		highlight_note_link_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

		gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(box), save, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), link_btn, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), highlight_note_link_box, FALSE, FALSE, 0);
		gtk_widget_show_all(box);
		gtk_container_add(GTK_CONTAINER(highlight_note_popover), box);
	}

	/* pre-fill with the highlight's existing note (if any) -- editing an
	 * existing note must not start from a blank textview. */
	existing_note = current_highlight_label ? highlight_get_note(current_highlight_label) : NULL;
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(highlight_note_textview),
				 existing_note ? existing_note : "", -1);
	g_free(existing_note);

	note_key = current_highlight_label ? highlight_note_key_group(current_highlight_label) : NULL;
	rebuild_linked_notes_box(note_key);
	g_free(note_key);

	gtk_popover_set_relative_to(GTK_POPOVER(highlight_note_popover), GTK_WIDGET(button));
	gtk_popover_popup(GTK_POPOVER(highlight_note_popover));
}

static void
on_highlight_copy_clicked(GtkButton *button, gpointer user_data)
{
	if (current_highlight_text) {
		GtkClipboard *clipboard =
		    gtk_widget_get_clipboard(GTK_WIDGET(widgets.html_text), GDK_SELECTION_CLIPBOARD);
		gtk_clipboard_set_text(clipboard, current_highlight_text, -1);
	}
}

static void
on_highlight_search_clicked(GtkButton *button, gpointer user_data)
{
	if (!current_highlight_text || !*current_highlight_text)
		return;
	gui_diccionario_mostrar(current_highlight_text);
}

static void
ensure_highlight_toolbar(void)
{
	if (highlight_toolbar_popover)
		return;

	highlight_toolbar_popover = gtk_popover_new(GTK_WIDGET(bible_view()));
	gtk_popover_set_position(GTK_POPOVER(highlight_toolbar_popover), GTK_POS_TOP);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_set_border_width(GTK_CONTAINER(box), 4);

	GtkWidget *del_btn = gtk_button_new_from_icon_name("edit-delete-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(del_btn, _("Eliminar"));
	gtk_button_set_relief(GTK_BUTTON(del_btn), GTK_RELIEF_NONE);
	g_signal_connect(del_btn, "clicked", G_CALLBACK(on_highlight_delete_clicked), NULL);
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(box), del_btn, FALSE, FALSE, 0);

	/* Colored circle: click to underline (or to change color later). */
	highlight_color_button = make_circle_swatch(DEFAULT_HIGHLIGHT_COLOR, 20,
						    &highlight_color_da);
	gtk_widget_set_tooltip_text(highlight_color_button, _("Subrayar"));
	g_signal_connect(highlight_color_button, "clicked",
			 G_CALLBACK(on_highlight_underline_clicked), NULL);
	gtk_widget_show(highlight_color_button);
	gtk_widget_set_margin_start(highlight_color_button, 4);
	gtk_widget_set_margin_end(highlight_color_button, 4);
	gtk_box_pack_start(GTK_BOX(box), highlight_color_button, FALSE, FALSE, 0);

	GtkWidget *note_btn = gtk_button_new_from_icon_name("document-edit-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(note_btn, _("Nota"));
	gtk_button_set_relief(GTK_BUTTON(note_btn), GTK_RELIEF_NONE);
	g_signal_connect(note_btn, "clicked", G_CALLBACK(on_highlight_note_clicked), NULL);
	gtk_widget_show(note_btn);
	gtk_box_pack_start(GTK_BOX(box), note_btn, FALSE, FALSE, 0);
	highlight_note_button = note_btn;

	GtkWidget *copy_btn = gtk_button_new_from_icon_name("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(copy_btn, _("Copiar"));
	gtk_button_set_relief(GTK_BUTTON(copy_btn), GTK_RELIEF_NONE);
	g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_highlight_copy_clicked), NULL);
	gtk_widget_show(copy_btn);
	gtk_box_pack_start(GTK_BOX(box), copy_btn, FALSE, FALSE, 0);

	GtkWidget *search_btn = gtk_button_new_from_icon_name("accessories-dictionary-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(search_btn, _("Diccionario"));
	gtk_button_set_relief(GTK_BUTTON(search_btn), GTK_RELIEF_NONE);
	g_signal_connect(search_btn, "clicked", G_CALLBACK(on_highlight_search_clicked), NULL);
	gtk_widget_show(search_btn);
	gtk_box_pack_start(GTK_BOX(box), search_btn, FALSE, FALSE, 0);

	gtk_widget_show(box);
	gtk_container_add(GTK_CONTAINER(highlight_toolbar_popover), box);

	gtk_style_context_add_class(gtk_widget_get_style_context(highlight_toolbar_popover),
				    "highlight-toolbar");
}

static void
free_highlight_segment(gpointer data)
{
	HighlightSegment *seg = (HighlightSegment *)data;
	g_free(seg->osisref);
	g_free(seg->text);
	g_free(seg);
}

static void
gui_handle_text_selection(const gchar *full_text, GList *segments,
			  double x, double y, double w, double h)
{
	GdkRectangle rect;
	gchar *color;

	if (!full_text || !*full_text || !segments)
		return;

	g_free(current_highlight_label);
	g_free(current_highlight_text);
	current_highlight_text = g_strdup(full_text);
	current_highlight_label = NULL;
	for (GList *n = segments; n && !current_highlight_label; n = n->next) {
		HighlightSegment *seg = (HighlightSegment *)n->data;
		current_highlight_label = highlight_find_overlapping(
		    settings.MainWindowModule, seg->osisref, seg->text);
	}

	ensure_highlight_toolbar();
	color = current_highlight_label
		    ? highlight_get_color(current_highlight_label)
		    : NULL;
	pending_color = DEFAULT_HIGHLIGHT_COLOR;
	set_circle_color(highlight_color_da,
			 color ? color : DEFAULT_HIGHLIGHT_COLOR);
	g_free(color);
	gtk_widget_set_tooltip_text(highlight_color_button,
				    current_highlight_label
					? _("Color de subrayado")
					: _("Subrayar"));

	rect.x = (gint)x;
	rect.y = (gint)y;
	rect.width = (gint)(w > 1 ? w : 1);
	rect.height = (gint)(h > 1 ? h : 1);
	gtk_popover_set_pointing_to(GTK_POPOVER(highlight_toolbar_popover), &rect);
	gtk_popover_popup(GTK_POPOVER(highlight_toolbar_popover));
}

static void
gui_hide_highlight_toolbar(void)
{
	if (highlight_color_popover)
		gtk_popover_popdown(GTK_POPOVER(highlight_color_popover));
	if (highlight_toolbar_popover)
		gtk_popover_popdown(GTK_POPOVER(highlight_toolbar_popover));
	clear_pending_selection();
}

static gchar *
book_from_current_verse(void)
{
	gchar *base = g_strdup(main_get_osisref_from_key(
	    (const char *)settings.MainWindowModule, (const char *)settings.currentverse));
	gchar *dot1 = strrchr(base, '.'); /* strip verse */
	if (dot1)
		*dot1 = '\0';
	gchar *dot2 = strrchr(base, '.'); /* strip chapter -> book remains */
	if (dot2)
		*dot2 = '\0';
	return base;
}

static gchar *
clean_sel_text(const gchar *t)
{
	gchar *s, *p, *q;
	if (!t)
		return g_strdup("");
	s = g_strdup(t);
	p = s;
	while (*p && g_ascii_isspace(*p))
		p++;
	while (*p && g_ascii_isdigit(*p))
		p++;
	while (*p && g_ascii_isspace(*p))
		p++;
	q = s;
	while (*p) {
		if (g_ascii_isspace(*p)) {
			*q++ = ' ';
			while (*p && g_ascii_isspace(*p))
				p++;
		} else
			*q++ = *p++;
	}
	while (q > s && g_ascii_isspace(q[-1]))
		q--;
	*q = '\0';
	return s;
}

static void
rect_for_iters(GtkTextView *view, const GtkTextIter *a, const GtkTextIter *b,
	       GdkRectangle *rect)
{
	GdkRectangle ra, rb;
	gint x, y;
	gtk_text_view_get_iter_location(view, a, &ra);
	gtk_text_view_get_iter_location(view, b, &rb);
	gtk_text_view_buffer_to_window_coords(view, GTK_TEXT_WINDOW_WIDGET,
					      ra.x, ra.y, &x, &y);
	rect->x = x;
	rect->y = y;
	rect->width = MAX(rb.x + rb.width - ra.x, 8);
	rect->height = MAX(ra.height, 12);
}

static void
show_hl_toolbar(const gchar *id, const gchar *text, GdkRectangle *rect)
{
	gchar *color;
	g_free(current_highlight_label);
	g_free(current_highlight_text);
	current_highlight_label = g_strdup(id);
	current_highlight_text = g_strdup(text);
	ensure_highlight_toolbar();
	gtk_popover_set_relative_to(GTK_POPOVER(highlight_toolbar_popover),
				    GTK_WIDGET(bible_view()));
	if (current_highlight_label) {
		clear_pending_selection();
		color = highlight_get_color(current_highlight_label);
	} else {
		color = NULL;
		pending_color = DEFAULT_HIGHLIGHT_COLOR;
	}
	set_circle_color(highlight_color_da,
			 color ? color : DEFAULT_HIGHLIGHT_COLOR);
	g_free(color);
	gtk_widget_set_tooltip_text(highlight_color_button,
				    current_highlight_label
					? _("Color de subrayado")
					: _("Subrayar"));
	gtk_popover_set_pointing_to(GTK_POPOVER(highlight_toolbar_popover), rect);
	gtk_popover_popup(GTK_POPOVER(highlight_toolbar_popover));
}

/* Entry point for clicking the "n"/"n2" note-count marker or an
 * individual note's superscript link in the rendered chapter (the
 * showHlNote passagestudy.jsp action, wired up in main/url.cc). Jumps
 * straight to viewing/editing that note, instead of just the mini
 * toolbar a plain click on the highlighted span would show. */
void
gui_open_highlight_note_by_id(const gchar *group_id)
{
	GtkTextView *view = bible_view();
	GtkTextIter hs, he;
	GdkRectangle rect;

	if (!group_id || !*group_id)
		return;

	if (view && wk_html_highlight_bounds(WK_HTML(widgets.html_text), group_id, &hs, &he)) {
		GtkTextBuffer *buf = gtk_text_view_get_buffer(view);
		gchar *text = gtk_text_buffer_get_text(buf, &hs, &he, FALSE);
		rect_for_iters(view, &hs, &he, &rect);
		show_hl_toolbar(group_id, text, &rect);
		g_free(text);
		if (highlight_note_button)
			on_highlight_note_clicked(GTK_BUTTON(highlight_note_button), NULL);
		return;
	}

	/* highlight isn't part of the currently rendered chapter (e.g. the
	 * "n2" marker was for a verse scrolled off-screen) -- fall back to
	 * a plain modal so the note is still reachable. */
	{
		gchar *existing = highlight_get_note(group_id);
		gchar *new_text = NULL;
		if (run_note_edit_dialog(_("Nota"), existing, &new_text))
			highlight_set_note(group_id, new_text);
		g_free(existing);
		g_free(new_text);
	}
}

static void
offer_highlight_for_selection(GtkTextView *view, GtkTextIter *start, GtkTextIter *end)
{
	gchar *raw, *text, *verse, *book, *osisref, *existing;
	GtkTextBuffer *buf;
	GdkRectangle rect;
	long vnum;
	gint computed_pos;
	WkHtml *html = WK_HTML(widgets.html_text);

	raw = gtk_text_buffer_get_text(gtk_text_view_get_buffer(view), start, end, FALSE);
	text = clean_sel_text(raw);
	g_free(raw);
	if (!text || !*text) {
		g_free(text);
		gui_hide_highlight_toolbar();
		return;
	}

	verse = wk_html_anchor_at(html, start);
	vnum = atol(verse);
	book = book_from_current_verse();
	if (vnum <= 0)
		osisref = g_strdup(main_get_osisref_from_key(
		    (const char *)settings.MainWindowModule,
		    (const char *)settings.currentverse));
	else
		osisref = g_strdup_printf("%s.%ld.%ld", book, vnum / 1000, vnum % 1000);
	g_free(book);

	/* Verse-relative character offset of the selection start, so a
	 * later re-render can relocate this highlight exactly instead of
	 * guessing via substring search (see apply_verse_notes() in
	 * display.cc). -1 if the verse's own anchor bounds can't be found
	 * (shouldn't normally happen -- `verse` just came from this same
	 * buffer). Computed here (before clear_pending_selection() resets
	 * pending_pos to -1 further below) and stashed until it's safe to
	 * assign. */
	{
		GtkTextIter vstart, vend;
		computed_pos = wk_html_anchor_bounds(html, verse, &vstart, &vend)
				   ? gtk_text_iter_get_offset(start) -
					 gtk_text_iter_get_offset(&vstart)
				   : -1;
	}
	g_free(verse);

	existing = highlight_find_overlapping(settings.MainWindowModule, osisref, text);
	if (existing) {
		g_free(osisref);
		rect_for_iters(view, start, end, &rect);
		show_hl_toolbar(existing, text, &rect);
		g_free(existing);
		g_free(text);
		return;
	}

	/* Remember the range; do not underline until the user clicks the circle. */
	buf = gtk_text_view_get_buffer(view);
	clear_pending_selection();
	pending_start_mark = gtk_text_buffer_create_mark(buf, "elim-hl-start", start, TRUE);
	pending_end_mark = gtk_text_buffer_create_mark(buf, "elim-hl-end", end, FALSE);
	pending_osisref = osisref;
	pending_text = g_strdup(text);
	pending_pos = computed_pos;
	pending_color = DEFAULT_HIGHLIGHT_COLOR;

	g_free(current_highlight_label);
	current_highlight_label = NULL;
	rect_for_iters(view, start, end, &rect);
	show_hl_toolbar(NULL, text, &rect);
	g_free(text);
}

static gboolean
on_native_select_done(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GtkTextView *view = GTK_TEXT_VIEW(widget);
	GtkTextBuffer *buf = gtk_text_view_get_buffer(view);
	GtkTextIter start, end, click;
	gchar *hid;
	GdkRectangle rect;

	(void)user_data;
	if (event->type == GDK_KEY_RELEASE) {
		if (!(event->key.state & GDK_SHIFT_MASK))
			return FALSE;
	}

	if (gtk_text_buffer_get_selection_bounds(buf, &start, &end)) {
		hid = wk_html_highlight_id_at(&start);
		if (hid) {
			GtkTextIter hs, he;
			gchar *t;
			wk_html_highlight_bounds(WK_HTML(widgets.html_text), hid, &hs, &he);
			t = gtk_text_buffer_get_text(buf, &hs, &he, FALSE);
			rect_for_iters(view, &hs, &he, &rect);
			show_hl_toolbar(hid, t, &rect);
			g_free(t);
			g_free(hid);
			return FALSE;
		}
		offer_highlight_for_selection(view, &start, &end);
		return FALSE;
	}

	if (event->type == GDK_BUTTON_RELEASE) {
		gint x, y;
		gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_TEXT,
						      (gint)event->button.x,
						      (gint)event->button.y, &x, &y);
		gtk_text_view_get_iter_at_location(view, &click, x, y);
		hid = wk_html_highlight_id_at(&click);
		if (hid) {
			GtkTextIter hs, he;
			gchar *t;
			wk_html_highlight_bounds(WK_HTML(widgets.html_text), hid, &hs, &he);
			t = gtk_text_buffer_get_text(buf, &hs, &he, FALSE);
			rect_for_iters(view, &hs, &he, &rect);
			show_hl_toolbar(hid, t, &rect);
			g_free(t);
			g_free(hid);
			return FALSE;
		}
	}
	gui_hide_highlight_toolbar();
	return FALSE;
}

// ---------------------------------------------------------------------
// Per-verse "Notas de este versículo" dialog: lists every note touching
// a verse (selection highlights with a note, plus whole-verse Mark
// Verse notes), lets the user view/edit each, link it to another
// verse's note, and add a brand new whole-verse note without first
// having to select any text. Entry point is the showHlNotes
// passagestudy.jsp action (the "n"/"n2" marker rendered per-verse by
// append_verse_note_marker() in main/display.cc), wired up in
// main/url.cc.
// ---------------------------------------------------------------------

typedef struct
{
	gchar *module;
	gchar *passage; /* "Book.C.V" */
	GtkWidget *listbox;
	GtkWidget *dialog;
} VerseNotesCtx;

typedef struct
{
	gchar *group_id; /* NULL for a whole-verse (Mark Verse) note */
	gchar *osisref;
	gchar *note_key;
	gchar *note_text;
	VerseNotesCtx *ctx;
} NoteRowCtx;

static void
free_verse_notes_ctx(gpointer data)
{
	VerseNotesCtx *ctx = (VerseNotesCtx *)data;
	if (!ctx)
		return;
	g_free(ctx->module);
	g_free(ctx->passage);
	g_free(ctx);
}

static void
free_note_row_ctx(gpointer data)
{
	NoteRowCtx *r = (NoteRowCtx *)data;
	if (!r)
		return;
	g_free(r->group_id);
	g_free(r->osisref);
	g_free(r->note_key);
	g_free(r->note_text);
	g_free(r);
}

static void rebuild_verse_notes_list(VerseNotesCtx *ctx);

static void
on_verse_note_edit_clicked(GtkButton *button, gpointer user_data)
{
	NoteRowCtx *r = (NoteRowCtx *)g_object_get_data(G_OBJECT(button), "row-ctx");
	gchar *new_text = NULL;
	(void)user_data;

	if (!r)
		return;

	if (r->group_id) {
		/* an on-screen selection highlight: close this dialog and
		 * jump straight to it, where color/delete/link are also
		 * available. Copy the id first -- destroying the dialog
		 * frees `r` (and r->group_id) via free_note_row_ctx. */
		gchar *gid = g_strdup(r->group_id);
		gtk_widget_destroy(r->ctx->dialog);
		gui_open_highlight_note_by_id(gid);
		g_free(gid);
		return;
	}

	if (run_note_edit_dialog(_("Editar nota del versículo"), r->note_text, &new_text)) {
		highlight_set_verse_note(r->ctx->module, r->osisref, new_text);
		main_display_bible(NULL, settings.currentverse);
		rebuild_verse_notes_list(r->ctx);
	}
	g_free(new_text);
}

static void
on_verse_note_link_clicked(GtkButton *button, gpointer user_data)
{
	NoteRowCtx *r = (NoteRowCtx *)g_object_get_data(G_OBJECT(button), "row-ctx");
	(void)user_data;
	if (!r)
		return;
	do_link_from_key(r->note_key);
	rebuild_verse_notes_list(r->ctx);
}

static void
on_verse_note_add_clicked(GtkButton *button, gpointer user_data)
{
	VerseNotesCtx *ctx = (VerseNotesCtx *)g_object_get_data(G_OBJECT(button), "vn-ctx");
	gchar *text = NULL;
	(void)user_data;
	if (!ctx)
		return;
	if (run_note_edit_dialog(_("Nueva nota para este versículo"), NULL, &text)) {
		highlight_set_verse_note(ctx->module, ctx->passage, text);
		main_display_bible(NULL, settings.currentverse);
		rebuild_verse_notes_list(ctx);
	}
	g_free(text);
}

static void
rebuild_verse_notes_list(VerseNotesCtx *ctx)
{
	GList *notes, *n;

	gtk_container_foreach(GTK_CONTAINER(ctx->listbox),
			      (GtkCallback)gtk_widget_destroy, NULL);

	notes = highlight_list_notes(ctx->passage);
	if (!notes) {
		GtkWidget *lbl = gtk_label_new(_("Todavía no hay notas en este versículo."));
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_box_pack_start(GTK_BOX(ctx->listbox), lbl, FALSE, FALSE, 0);
	}

	for (n = notes; n; n = n->next) {
		HighlightNote *note = (HighlightNote *)n->data;
		GtkWidget *frame = gtk_frame_new(NULL);
		GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		GtkWidget *excerpt_lbl, *note_lbl, *hbox, *edit_btn, *link_btn;
		GList *links, *ln;
		NoteRowCtx *r_edit, *r_link;

		gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
		gtk_container_add(GTK_CONTAINER(frame), vbox);

		if (note->text && *note->text) {
			gchar *markup = g_markup_printf_escaped("<i>“%s”</i>", note->text);
			excerpt_lbl = gtk_label_new(NULL);
			gtk_label_set_markup(GTK_LABEL(excerpt_lbl), markup);
			g_free(markup);
		} else {
			excerpt_lbl = gtk_label_new(_("(versículo completo)"));
		}
		gtk_label_set_line_wrap(GTK_LABEL(excerpt_lbl), TRUE);
		gtk_widget_set_halign(excerpt_lbl, GTK_ALIGN_START);
		gtk_box_pack_start(GTK_BOX(vbox), excerpt_lbl, FALSE, FALSE, 0);

		note_lbl = gtk_label_new(note->note ? note->note : "");
		gtk_label_set_line_wrap(GTK_LABEL(note_lbl), TRUE);
		gtk_widget_set_halign(note_lbl, GTK_ALIGN_START);
		gtk_box_pack_start(GTK_BOX(vbox), note_lbl, FALSE, FALSE, 0);

		links = highlight_list_linked_notes(note->note_key);
		if (links) {
			GtkWidget *link_hdr = gtk_label_new(_("Enlazada con:"));
			gtk_widget_set_halign(link_hdr, GTK_ALIGN_START);
			gtk_box_pack_start(GTK_BOX(vbox), link_hdr, FALSE, FALSE, 0);
			for (ln = links; ln; ln = ln->next) {
				gchar *key = (gchar *)ln->data;
				gchar *osis = highlight_note_key_osisref(key);
				GtkWidget *lb = gtk_button_new_with_label(osis ? osis : key);
				gtk_button_set_relief(GTK_BUTTON(lb), GTK_RELIEF_NONE);
				gtk_widget_set_halign(lb, GTK_ALIGN_START);
				g_object_set_data_full(G_OBJECT(lb), "note-key", g_strdup(key), g_free);
				g_signal_connect(lb, "clicked", G_CALLBACK(on_linked_note_clicked), NULL);
				gtk_box_pack_start(GTK_BOX(vbox), lb, FALSE, FALSE, 0);
				g_free(osis);
			}
			g_list_free_full(links, g_free);
		}

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		edit_btn = gtk_button_new_with_label(_("Editar"));
		link_btn = gtk_button_new_with_label(_("Enlazar"));

		r_edit = g_new0(NoteRowCtx, 1);
		r_edit->group_id = g_strdup(note->group_id);
		r_edit->osisref = g_strdup(note->osisref);
		r_edit->note_key = g_strdup(note->note_key);
		r_edit->note_text = g_strdup(note->note);
		r_edit->ctx = ctx;
		g_object_set_data_full(G_OBJECT(edit_btn), "row-ctx", r_edit, free_note_row_ctx);
		g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_verse_note_edit_clicked), NULL);

		r_link = g_new0(NoteRowCtx, 1);
		r_link->group_id = g_strdup(note->group_id);
		r_link->osisref = g_strdup(note->osisref);
		r_link->note_key = g_strdup(note->note_key);
		r_link->note_text = g_strdup(note->note);
		r_link->ctx = ctx;
		g_object_set_data_full(G_OBJECT(link_btn), "row-ctx", r_link, free_note_row_ctx);
		g_signal_connect(link_btn, "clicked", G_CALLBACK(on_verse_note_link_clicked), NULL);

		gtk_box_pack_start(GTK_BOX(hbox), edit_btn, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), link_btn, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(ctx->listbox), frame, FALSE, FALSE, 0);
	}
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);

	gtk_widget_show_all(ctx->listbox);
}

void
gui_show_verse_notes_dialog(const gchar *module, const gchar *passage)
{
	gchar *cita;

	if (!module || !passage)
		return;
	cita = main_interlineal_cita_es(passage);
	gui_lectura_sync_ficha_nota(module, passage, cita);
	g_free(cita);
}

// ---------------------------------------------------------------------
// Reading focus: the band on the verse being read, in step with the
// navbar and settings.currentverse.
//
// Two things move the viewport, and they are kept apart explicitly:
//
//   SCROLL_ORIGIN_NAVIGATION -- the arrows (or any navigation) chose a
//   verse; the view follows it with the least scroll that brings it into
//   the reading zone (reading_zone_scroll_delta()). Scroll changes that
//   follow -- this minimal scroll, wk_html_jump_to_anchor()'s retries,
//   GtkTextView settling its layout -- never re-derive the focus: that
//   was the loop where a scroll caused by navigating to verse 15 picked
//   verse 14 back from the viewport.
//
//   SCROLL_ORIGIN_USER -- a wheel, touchpad or scrollbar gesture; the
//   viewport moves freely and the focus follows it: the verse on the
//   reading line (reading_focus_pick()), measured at most once per frame
//   from the anchors already laid out. Nothing here scrolls, and a jump
//   still retrying is cancelled, so the view is never pulled back.
//
// The origin switches on the input event itself, not on a timer.
//
// When the focus follows the viewport to another verse, that verse
// becomes the current reference: band, navbar (which the arrows step
// from), settings.currentverse and the tab label, once per verse change,
// not per scroll event. keys/verse only changes in memory; settings.xml
// is written at the usual times. With Comparar open the panel follows too.
//
// The pane holds a window of chapters of the book (reading_window.h):
// the focus follows the reader from one chapter into the next, and when it
// reaches a chapter at the window's edge the window moves with it
// (recenter_reading_window()), keeping the text on screen where it is.
// The adjacent book's one-verse previews (anchors "0"/"0next") are not
// verses and never take the focus.
//
// End of the pane: the view carries a bottom reading reserve so the last
// verse can reach the reading line, and a wheel step pushed against the
// bottom (or top) moves the focus one visible verse on, since the viewport
// has nowhere to go. Neither scrolls anything.
// ---------------------------------------------------------------------

typedef enum {
	SCROLL_ORIGIN_NAVIGATION,
	SCROLL_ORIGIN_USER
} ScrollOrigin;

static ScrollOrigin scroll_origin = SCROLL_ORIGIN_NAVIGATION;
static guint reading_focus_tick_id = 0;
/* chapter * 1000 + verse of the verse carrying the band, 0 if none */
static gint reading_focus_id = 0;
/* The reading line's position (buffer y) at the last focus update, so an
 * update sees everything the line went past since, however many scroll
 * events a frame coalesced. */
static gdouble reading_line_doc_y = 0.0;
static gboolean reading_line_valid = FALSE;
static gdouble reading_focus_prev_value = 0.0;
/* Explicit navigation -> scroll handoff (reading_focus.h): the verse a
 * navigation settled on becomes where the first scroll starts from. Taken
 * at that scroll, from the geometry it finds; then the tracking line is
 * shifted by reading_line_offset, which shrinks as the view moves. */
static gboolean focus_rebase_pending = FALSE;
static gint focus_rebase_target = 0;
static gdouble reading_line_offset = 0.0;
static gboolean first_scroll_after_nav = FALSE;
static void rebase_focus_for_user_scroll(const GdkRectangle *vis);
/* A wheel/touchpad step made against the top (-1) or bottom (+1) of the
 * pane, where the viewport cannot move and no value-changed follows. */
static gint reading_focus_edge_push = 0;
static gdouble reading_focus_edge_accum = 0.0;
/* the same GdkEventScroll reaches the view and then its scrolled window */
static gpointer last_scroll_event = NULL;
static guint32 last_scroll_event_time = 0;
static guint scroll_event_seq = 0;
static gdouble last_viewport_value = 0.0;
static gdouble last_range_upper = -1.0, last_range_page = -1.0;
/* A mouse wheel notch glides instead of jumping (wheel_scroll.h). */
static WheelScroll wheel_scroll = { FALSE, 0.0, 0.0, 0 };
static guint wheel_tick_id = 0;
/* The chapter window moving with the reader (reading_window.h). */
static guint window_tick_id = 0;
static gdouble window_last_value = -1.0;
static gdouble window_resume_distance = 0.0;
/* Restoring the view after a re-layout: the text that was on the reading
 * line (verse anchor, character offset) and its distance from the top. */
static gboolean window_restore_pending = FALSE;
static gchar *window_restore_anchor = NULL;
static gint window_restore_offset = 0;
static gdouble window_restore_y = 0.0;
static gint window_restore_frames = 0;
static gdouble window_restore_last_target = -1.0;
static gdouble window_restore_last_upper = -1.0;
static GdkWindow *window_restore_frozen = NULL;
static gint64 window_restore_started = 0;
static void schedule_window_recenter(void);
/* bottom margin wk-html gives the view, before any reading reserve */
static gint reading_reserve_base = -1;
static guint reading_reserve_idle_id = 0;

/* Dónde se quedó leyendo, para la pestaña en el próximo arranque: el
 * versículo al que llevó el scroll, que ya es también
 * settings.currentverse. Una navegación la borra, porque entonces la
 * referencia buena es el versículo navegado. */
static gchar *lectura_posicion = NULL;

static void
cancel_reading_focus_tick(void)
{
	GtkTextView *view = bible_view();

	if (reading_focus_tick_id && view)
		gtk_widget_remove_tick_callback(GTK_WIDGET(view),
						reading_focus_tick_id);
	reading_focus_tick_id = 0;
}

static void
cancel_wheel_scroll(void)
{
	GtkTextView *view = bible_view();

	if (wheel_tick_id && view)
		gtk_widget_remove_tick_callback(GTK_WIDGET(view), wheel_tick_id);
	wheel_tick_id = 0;
	wheel_scroll_cancel(&wheel_scroll);
}

static void
begin_navigation_scroll(void)
{
	cancel_wheel_scroll();
	cancel_reading_focus_tick();
	scroll_origin = SCROLL_ORIGIN_NAVIGATION;
	reading_line_valid = FALSE;
	reading_line_offset = 0.0;
	focus_rebase_pending = FALSE;
	first_scroll_after_nav = FALSE;
	/* a wheel glide carried across a window re-layout must not resume
	 * after a navigation either */
	window_resume_distance = 0.0;
	reading_focus_edge_push = 0;
	reading_focus_edge_accum = 0.0;
	g_clear_pointer(&lectura_posicion, g_free);
}

static void
begin_user_scroll(void)
{
	GtkTextView *view;

	if (scroll_origin == SCROLL_ORIGIN_USER)
		return;
	scroll_origin = SCROLL_ORIGIN_USER;
	if (widgets.html_text)
		wk_html_cancel_anchor_jump(WK_HTML(widgets.html_text));
	/* Called before the scrolled window applies the event: this is where
	 * the reading line starts from. */
	view = bible_view();
	if (view && gtk_widget_get_mapped(GTK_WIDGET(view))) {
		GdkRectangle vis;
		GtkAdjustment *vadj =
		    gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
		gtk_text_view_get_visible_rect(view, &vis);
		reading_line_doc_y = vis.y + vis.height * READING_FOCUS_LINE_RATIO;
		reading_line_valid = vis.height > 1;
		if (vadj)
			reading_focus_prev_value = gtk_adjustment_get_value(vadj);
		rebase_focus_for_user_scroll(&vis);
	}
}

/* The first scroll after an explicit navigation: the tracking line starts
 * inside the navigated verse, wherever the reading zone left it. */
static void
rebase_focus_for_user_scroll(const GdkRectangle *vis)
{
	gchar anchor[16];
	gint top, bottom;

	if (!focus_rebase_pending)
		return;
	focus_rebase_pending = FALSE;
	reading_line_offset = 0.0;
	if (!focus_rebase_target || focus_rebase_target != reading_focus_id ||
	    !widgets.html_text || vis->height <= 1)
		return;
	g_snprintf(anchor, sizeof anchor, "%d", focus_rebase_target);
	if (!wk_html_anchor_block(WK_HTML(widgets.html_text), anchor, &top, &bottom))
		return;
	reading_line_offset = reading_focus_rebase_line_offset(
	    top - vis->y, bottom - vis->y, vis->height);
	first_scroll_after_nav = TRUE;
	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "target=%d targetTop=%d targetBottom=%d scrollTop=%d "
		    "readingLineY=%.1f readingDocumentY=%.1f lineOffset=%.1f",
		    focus_rebase_target, top - vis->y, bottom - vis->y, vis->y,
		    vis->height * READING_FOCUS_LINE_RATIO, reading_line_doc_y,
		    reading_line_offset);
		panel_load_debug("focus", "FOCUS_REBASE_BASELINE", detail);
		g_free(detail);
	}
}

static gchar *
anchor_from_current_verse(void)
{
	gchar *osisref, *dot1, *dot2, *anchor;
	int chapter, verse;

	if (!settings.currentverse || !settings.MainWindowModule)
		return NULL;
	osisref = g_strdup(main_get_osisref_from_key(
	    (const char *)settings.MainWindowModule, (const char *)settings.currentverse));
	dot1 = strrchr(osisref, '.');
	if (!dot1) {
		g_free(osisref);
		return NULL;
	}
	verse = atoi(dot1 + 1);
	*dot1 = '\0';
	dot2 = strrchr(osisref, '.');
	if (!dot2) {
		g_free(osisref);
		return NULL;
	}
	chapter = atoi(dot2 + 1);
	g_free(osisref);
	anchor = g_strdup_printf("%d", (chapter * 1000) + verse);
	return anchor;
}

/* The least scroll that puts the verse inside the reading zone; nothing
 * when it is already comfortably visible. */
static void
scroll_anchor_into_reading_zone(const gchar *anchor)
{
	GtkTextView *view = bible_view();
	GtkAdjustment *vadj;
	GdkRectangle vis;
	gint top, bottom;
	gdouble delta;

	/* Measuring a view that has not been laid out yet makes GtkTextView
	 * build its layout without a size, and the next tag change then
	 * crashes inside gtk_text_layout_changed() (startup with a sword://
	 * argument displays before the first allocation). Until then the
	 * render's own wk_html_jump_to_anchor() positions the verse. */
	if (!view || !widgets.html_text ||
	    !gtk_widget_get_mapped(GTK_WIDGET(view)) ||
	    gtk_widget_get_allocated_height(GTK_WIDGET(view)) <= 1)
		return;
	if (!wk_html_anchor_block(WK_HTML(widgets.html_text), anchor, &top, &bottom))
		return;
	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
	if (!vadj)
		return;
	gtk_text_view_get_visible_rect(view, &vis);
	delta = reading_zone_scroll_delta(top - vis.y, bottom - vis.y, vis.height);
	if (delta != 0.0)
		gtk_adjustment_set_value(vadj, gtk_adjustment_get_value(vadj) + delta);
}

static void
focus_band_on_current_verse(gboolean scroll)
{
	gchar *anchor;
	GtkTextIter s, e;

	if (!widgets.html_text)
		return;
	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "origin=%s focused=%d prevScrollTop=%.1f "
		    "prevReadingDocumentY=%.1f lineValid=%d lineOffset=%.1f",
		    scroll_origin == SCROLL_ORIGIN_USER ? "user" : "navigation",
		    reading_focus_id, reading_focus_prev_value, reading_line_doc_y,
		    reading_line_valid, reading_line_offset);
		panel_load_debug("focus", "FOCUS_STATE_BEFORE_NAV", detail);
		g_free(detail);
	}
	begin_navigation_scroll();
	anchor = anchor_from_current_verse();
	if (!anchor)
		return;
	if (wk_html_anchor_bounds(WK_HTML(widgets.html_text), anchor, &s, &e))
		wk_html_reading_focus_set(WK_HTML(widgets.html_text), &s, &e,
					  NULL, NULL);
	reading_focus_id = atoi(anchor);
	if (scroll)
		scroll_anchor_into_reading_zone(anchor);
	/* The navigation decided the verse: the next scroll starts from it
	 * (rebase_focus_for_user_scroll), not from whatever the reading line
	 * happens to cover. Geometry is taken then -- a full render may still
	 * be positioning the verse now. */
	focus_rebase_target = reading_focus_id;
	focus_rebase_pending = reading_focus_id_is_verse(reading_focus_id);
	if (panel_load_debug_enabled()) {
		GtkTextView *view = bible_view();
		GdkRectangle vis = { 0, 0, 0, 0 };
		gchar *detail;

		if (view && gtk_widget_get_mapped(GTK_WIDGET(view)))
			gtk_text_view_get_visible_rect(view, &vis);
		detail = g_strdup_printf(
		    "target=%d scrollTop=%d readingLineY=%.1f readingDocumentY=%.1f "
		    "pending=%d",
		    reading_focus_id, vis.y, vis.height * READING_FOCUS_LINE_RATIO,
		    vis.y + vis.height * READING_FOCUS_LINE_RATIO,
		    focus_rebase_pending);
		panel_load_debug("focus", "FOCUS_REBASE", detail);
		g_free(detail);
	}
	g_free(anchor);
	/* stepped into a chapter at the window's edge */
	schedule_window_recenter();
}

void
gui_bibletext_mark_current_verse(void)
{
	focus_band_on_current_verse(TRUE);
}

void
gui_bibletext_lectura_sync_focus_current(void)
{
	if (!settings.currentverse)
		return;
	focus_band_on_current_verse(settings.show_lectura_sync);
}

void
gui_bibletext_lectura_sync_clear_focus(void)
{
	reading_focus_id = 0;
	gui_bibletext_mark_current_verse();
}

/* The focus followed the viewport onto verse `id`: it is now the current
 * reference. */
static void
apply_scrolled_focus(gint id)
{
	gchar anchor[16];
	GtkTextIter s, e;
	gchar *book, *ref, *valido;

	g_snprintf(anchor, sizeof anchor, "%d", id);
	if (!wk_html_anchor_bounds(WK_HTML(widgets.html_text), anchor, &s, &e))
		return;
	wk_html_reading_focus_set(WK_HTML(widgets.html_text), &s, &e, NULL, NULL);
	reading_focus_id = id;

	book = book_from_current_verse();
	if (!book || !*book) {
		g_free(book);
		return;
	}
	ref = g_strdup_printf("%s %d:%d", book, id / 1000, id % 1000);
	g_free(book);
	/* El ancla trae el nombre OSIS del libro ("Num 1:33"); keys/verse,
	 * la navbar y la pestaña llevan la clave validada en el idioma del
	 * módulo ("Números 1:33"), como deja main_display_bible(). */
	valido = main_get_valid_key(settings.MainWindowModule, ref);
	if (valido && *valido) {
		g_free(ref);
		ref = valido;
	} else
		g_free(valido);

	xml_set_value("Xiphos", "keys", "verse", ref);
	settings.currentverse = xml_get_value("keys", "verse");
	main_navbar_versekey_set(navbar_versekey, ref);
	gui_set_tab_label(ref, FALSE);
	g_free(lectura_posicion);
	lectura_posicion = g_strdup(ref);
	/* scrolled into a chapter at the window's edge */
	schedule_window_recenter();

	if (settings.show_lectura_sync) {
		main_lectura_sync_focus_verse(ref);
		if (main_interlineal_quizas_plegar(ref)) {
			main_bible_note_interlinear_html();
			main_display_bible(NULL, settings.currentverse);
		}
	}
	g_free(ref);
}

typedef struct {
	GArray *blocks;
	gint y0;
	gint height;
	gint last_visible;
} FocusBlocks;

static gboolean
collect_focus_block(const gchar *name, gint top, gint bottom, gpointer data)
{
	FocusBlocks *fb = data;
	ReadingFocusBlock block;
	gchar *end;
	glong id;

	if (!name || !*name)
		return TRUE;
	id = strtol(name, &end, 10);
	/* "0", "0next", "0hdr", "TOP", chapter anchors: not verses */
	if (*end || !reading_focus_id_is_verse((gint)id))
		return TRUE;
	block.id = (gint)id;
	block.top = top - fb->y0;
	block.bottom = bottom - fb->y0;
	g_array_append_val(fb->blocks, block);
	if (block.bottom > 0 && block.top < fb->height)
		fb->last_visible = block.id;
	return TRUE;
}

/* Whether the viewport is at the top / bottom of what it can scroll. */
static void
scroll_edges(GtkAdjustment *adj, gboolean *at_top, gboolean *at_bottom)
{
	const gdouble tolerance = 1.0; /* px; values are fractional */
	gdouble value = gtk_adjustment_get_value(adj);

	*at_top = value - gtk_adjustment_get_lower(adj) <= tolerance;
	*at_bottom = gtk_adjustment_get_upper(adj) -
			     gtk_adjustment_get_page_size(adj) - value <=
		     tolerance;
}

static void
reading_focus_update(void)
{
	GtkTextView *view = bible_view();
	GtkAdjustment *vadj;
	GdkRectangle vis;
	FocusBlocks fb;
	ReadingFocusStep step;
	gint current, push, lo, hi;
	guint n_blocks;
	gint64 started;
	gdouble line_doc, prev_line_y, value, prev_value, offset;
	gboolean at_top = FALSE, at_bottom = FALSE, pushing;

	if (!view || !widgets.html_text || !settings.currentverse ||
	    !settings.MainWindowModule)
		return;
	/* the interlinear pins the reference, as it does for the arrows */
	if (main_interlineal_bloquea_navegacion())
		return;
	/* the chapter window is being laid out again: positions are not
	 * final until the view is restored */
	if (window_restore_pending)
		return;
	if (!gtk_widget_get_mapped(GTK_WIDGET(view)))
		return;
	gtk_text_view_get_visible_rect(view, &vis);
	if (vis.height <= 1)
		return;
	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
	if (!vadj)
		return;
	scroll_edges(vadj, &at_top, &at_bottom);
	push = reading_focus_edge_push;
	reading_focus_edge_push = 0;
	pushing = (push > 0 && at_bottom) || (push < 0 && at_top);

	started = g_get_monotonic_time();
	current = reading_focus_id;
	if (!current) {
		gchar *anchor = anchor_from_current_verse();
		current = anchor ? atoi(anchor) : 0;
		g_free(anchor);
	}

	/* The verses the line went past since the last update are measured
	 * too, not only the viewport as it is now -- unless the view jumped
	 * too far for that to mean anything (scrollbar drag, Page keys). */
	line_doc = vis.y + vis.height * READING_FOCUS_LINE_RATIO;
	offset = reading_line_offset;
	prev_line_y = vis.height * READING_FOCUS_LINE_RATIO + offset;
	lo = vis.y;
	hi = vis.y + vis.height;
	if (reading_line_valid) {
		if (ABS(reading_line_doc_y - line_doc) <= 2.0 * vis.height) {
			gdouble prev_doc = reading_line_doc_y + reading_line_offset;

			prev_line_y = prev_doc - vis.y;
			/* the handoff shift shrinks by however far the view moved */
			offset = reading_focus_line_offset_decay(
			    offset, line_doc - reading_line_doc_y);
			lo = MIN(lo, (gint)prev_doc - 1);
			hi = MAX(hi, (gint)prev_doc + 2);
		} else {
			offset = 0.0; /* jumped: nothing left to hand off */
			prev_line_y = vis.height * READING_FOCUS_LINE_RATIO;
		}
	}
	lo = MIN(lo, (gint)(line_doc + offset) - 1);
	hi = MAX(hi, (gint)(line_doc + offset) + 2);
	fb.blocks = g_array_new(FALSE, FALSE, sizeof(ReadingFocusBlock));
	fb.y0 = vis.y;
	fb.height = vis.height;
	fb.last_visible = 0;
	wk_html_foreach_anchor_block(WK_HTML(widgets.html_text), lo, hi,
				     collect_focus_block, &fb);
	n_blocks = fb.blocks->len;
	step = reading_focus_track_offset(
	    (const ReadingFocusBlock *)fb.blocks->data, n_blocks, vis.height,
	    current, prev_line_y, pushing ? push : 0, offset);
	g_array_free(fb.blocks, TRUE);

	value = gtk_adjustment_get_value(vadj);
	prev_value = reading_focus_prev_value;
	reading_focus_prev_value = value;
	reading_line_doc_y = line_doc;
	reading_line_valid = TRUE;
	reading_line_offset = offset;

	if (first_scroll_after_nav && panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "target=%d prevFocused=%d candidate=%d picked=%d direction=%d "
		    "deltaScrollTop=%.1f lineOffset=%.1f",
		    focus_rebase_target, current, step.candidate, step.picked,
		    step.direction, value - prev_value, offset);
		panel_load_debug("focus", "FIRST_USER_SCROLL_AFTER_NAV", detail);
		g_free(detail);
	}
	first_scroll_after_nav = FALSE;

	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "prevScrollTop=%.1f scrollTop=%.1f deltaScrollTop=%.1f "
		    "maxScrollTop=%.1f viewportHeight=%d readingLineY=%.0f "
		    "prevLineY=%.1f prevFocused=%d candidate=%d picked=%d "
		    "crossed=%d dir=%d push=%d blocks=%u lastVisible=%d "
		    "atTop=%d atBottom=%d reserve=%d lineOffset=%.1f us=%" G_GINT64_FORMAT,
		    prev_value, value, value - prev_value,
		    gtk_adjustment_get_upper(vadj) -
			gtk_adjustment_get_page_size(vadj),
		    vis.height, vis.height * READING_FOCUS_LINE_RATIO,
		    prev_line_y, current, step.candidate, step.picked,
		    step.crossed, step.direction, pushing ? push : 0, n_blocks,
		    fb.last_visible, at_top, at_bottom,
		    reading_reserve_base >= 0
			? gtk_text_view_get_bottom_margin(view) -
			      reading_reserve_base
			: 0,
		    offset, g_get_monotonic_time() - started);
		panel_load_debug("focus", "FOCUS_CANDIDATE", detail);
		g_free(detail);
		if (step.picked != current) {
			detail = g_strdup_printf("from=%d to=%d crossed=%d",
						 current, step.picked,
						 step.crossed);
			panel_load_debug("focus", "FOCUS_CHANGED", detail);
			g_free(detail);
		}
	}

	if (step.picked && reading_focus_id_is_verse(step.picked) &&
	    step.picked != reading_focus_id)
		apply_scrolled_focus(step.picked);
}

GtkWidget *
gui_bibletext_view(void)
{
	GtkTextView *view = bible_view();

	return view ? GTK_WIDGET(view) : NULL;
}

void
gui_bibletext_reading_focus_flush(void)
{
	if (!reading_focus_tick_id)
		return;
	cancel_reading_focus_tick();
	if (scroll_origin == SCROLL_ORIGIN_USER)
		reading_focus_update();
}

/* Guarda al cerrar la posición a la que llevó el scroll en la pestaña
 * actual (.last_session_tabs); keys/verse ya la tiene. */
void
gui_bibletext_guardar_posicion_lectura(void)
{
	if (!lectura_posicion || !*lectura_posicion)
		return;

	xml_set_value("Xiphos", "keys", "verse", lectura_posicion);
	settings.currentverse = xml_get_value("keys", "verse");
	gui_tab_set_reading_key(lectura_posicion);
}

static gboolean
on_reading_focus_tick(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
	(void)widget;
	(void)clock;
	(void)data;
	reading_focus_tick_id = 0;
	if (scroll_origin == SCROLL_ORIGIN_USER)
		reading_focus_update();
	return G_SOURCE_REMOVE;
}

static void
schedule_reading_focus_update(void)
{
	GtkTextView *view;

	if (scroll_origin != SCROLL_ORIGIN_USER || reading_focus_tick_id)
		return;
	view = bible_view();
	if (view)
		reading_focus_tick_id = gtk_widget_add_tick_callback(
		    GTK_WIDGET(view), on_reading_focus_tick, NULL, NULL);
}

static void
on_reading_scroll_value_changed(GtkAdjustment *adj, gpointer data)
{
	gdouble value = gtk_adjustment_get_value(adj);

	(void)data;
	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "previous=%.1f scrollTop=%.1f delta=%.1f maxScrollTop=%.1f "
		    "origin=%s gliding=%d",
		    last_viewport_value, value, value - last_viewport_value,
		    gtk_adjustment_get_upper(adj) -
			gtk_adjustment_get_page_size(adj),
		    scroll_origin == SCROLL_ORIGIN_USER ? "user" : "navigation",
		    wheel_scroll.active);
		panel_load_debug("focus", "SCROLL_VIEWPORT", detail);
		g_free(detail);
	}
	last_viewport_value = value;
	schedule_reading_focus_update();
}

/* Bottom reading reserve: empty space below the pane's content, as the
 * view's own bottom margin -- outside the buffer, so no anchor, nothing
 * to select, copy or find -- sized so the chapter's last verse can be
 * scrolled up past the reading line.
 *
 * Its inputs are layout geometry only: the viewport height, where the
 * laid-out text ends, where the last verse starts. Never the scroll
 * position -- computed from a fractional scroll value it flipped between
 * two sizes (117/118) as the reader scrolled.
 *
 * It is applied from an idle, never from the adjustment's "changed"
 * handler: GtkTextView emits that while validating its layout to draw, and
 * changing the margin there invalidated the layout under it
 * (Gtk:ERROR gtk_text_view_validate_onscreen: assertion failed:
 * (priv->onscreen_validated)). The idle runs after GTK's own validation
 * and redraw; the range change it causes schedules one more computation,
 * which finds nothing to change. Scrolling alone does not emit "changed". */
typedef struct {
	gboolean found;
	gint top;
} LastVerse;

static gboolean
find_last_verse(const gchar *name, gint top, gint bottom, gpointer data)
{
	LastVerse *lv = data;
	gchar *end;
	glong id;

	(void)bottom;
	if (!name || !*name)
		return TRUE;
	id = strtol(name, &end, 10);
	if (*end || !reading_focus_id_is_verse((gint)id))
		return TRUE;
	lv->found = TRUE;
	lv->top = top;
	return FALSE;
}

static gboolean
apply_reading_reserve(gpointer data)
{
	GtkTextView *view = bible_view();
	GdkRectangle vis;
	GtkTextIter end;
	LastVerse lv = { FALSE, 0 };
	gint margin, wanted, line_y = 0, line_height = 0, content_bottom = 0;
	gint reserve = 0;

	(void)data;
	reading_reserve_idle_id = 0;
	if (!view || !widgets.html_text ||
	    !gtk_widget_get_mapped(GTK_WIDGET(view)))
		return G_SOURCE_REMOVE;
	gtk_text_view_get_visible_rect(view, &vis);
	if (vis.height <= 1)
		return G_SOURCE_REMOVE;
	margin = gtk_text_view_get_bottom_margin(view);
	if (reading_reserve_base < 0)
		reading_reserve_base = margin;

	wk_html_foreach_anchor_block_reverse(WK_HTML(widgets.html_text),
					     find_last_verse, &lv);
	if (lv.found) {
		gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(view), &end);
		gtk_text_view_get_line_yrange(view, &end, &line_y, &line_height);
		/* where scrolling ends without the reserve, in the buffer
		 * coordinates of the anchors */
		content_bottom = line_y + line_height + reading_reserve_base;
		reserve = reading_focus_bottom_reserve(vis.height, content_bottom,
						       lv.top);
	}
	wanted = reading_reserve_base + reserve;
	if (wanted == margin)
		return G_SOURCE_REMOVE;
	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "reserve=%d previous=%d viewportHeight=%d contentBottom=%d "
		    "lastVerseTop=%d found=%d",
		    reserve, margin - reading_reserve_base, vis.height,
		    content_bottom, lv.top, lv.found);
		panel_load_debug("focus", "READING_RESERVE", detail);
		g_free(detail);
	}
	gtk_text_view_set_bottom_margin(view, wanted);
	return G_SOURCE_REMOVE;
}

static void
on_reading_adjustment_changed(GtkAdjustment *adj, gpointer data)
{
	(void)data;
	if (panel_load_debug_enabled() &&
	    (gtk_adjustment_get_upper(adj) != last_range_upper ||
	     gtk_adjustment_get_page_size(adj) != last_range_page)) {
		gchar *detail = g_strdup_printf(
		    "documentHeight=%.1f previous=%.1f viewportHeight=%.1f "
		    "maxScrollTop=%.1f",
		    gtk_adjustment_get_upper(adj), last_range_upper,
		    gtk_adjustment_get_page_size(adj),
		    gtk_adjustment_get_upper(adj) -
			gtk_adjustment_get_page_size(adj));
		panel_load_debug("focus", "SCROLL_RANGE", detail);
		g_free(detail);
	}
	last_range_upper = gtk_adjustment_get_upper(adj);
	last_range_page = gtk_adjustment_get_page_size(adj);
	if (!reading_reserve_idle_id)
		reading_reserve_idle_id =
		    g_idle_add(apply_reading_reserve, NULL);
}

static gint
scroll_event_direction(GdkEventScroll *event, gdouble *dx, gdouble *dy,
		       gdouble *amount)
{
	*dx = 0;
	*dy = 0;
	*amount = 1.0;
	switch (event->direction) {
	case GDK_SCROLL_DOWN:
		*dy = 1;
		return 1;
	case GDK_SCROLL_UP:
		*dy = -1;
		return -1;
	case GDK_SCROLL_SMOOTH:
		if (!gdk_event_get_scroll_deltas((GdkEvent *)event, dx, dy) ||
		    *dy == 0)
			return 0;
		*amount = ABS(*dy);
		return *dy > 0 ? 1 : -1;
	default:
		return 0;
	}
}

static const gchar *
input_source_name(GdkInputSource source)
{
	switch (source) {
	case GDK_SOURCE_MOUSE:
		return "mouse";
	case GDK_SOURCE_TOUCHPAD:
		return "touchpad";
	case GDK_SOURCE_TRACKPOINT:
		return "trackpoint";
	case GDK_SOURCE_TOUCHSCREEN:
		return "touchscreen";
	case GDK_SOURCE_PEN:
	case GDK_SOURCE_ERASER:
	case GDK_SOURCE_CURSOR:
	case GDK_SOURCE_TABLET_PAD:
		return "tablet";
	case GDK_SOURCE_KEYBOARD:
		return "keyboard";
	default:
		return "other";
	}
}

static gboolean
on_wheel_scroll_tick(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
	GtkAdjustment *vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(widget));
	gint64 now = gdk_frame_clock_get_frame_time(clock);
	gdouble max, value, progress;

	(void)data;
	if (!vadj || !wheel_scroll.active) {
		wheel_tick_id = 0;
		return G_SOURCE_REMOVE;
	}
	max = gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj);
	progress = wheel_scroll_progress(&wheel_scroll, now);
	value = wheel_scroll_value(&wheel_scroll, now,
				   gtk_adjustment_get_lower(vadj), max);
	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "current=%.1f next=%.1f target=%.1f progress=%.2f durationMs=%d",
		    gtk_adjustment_get_value(vadj), value, wheel_scroll.target,
		    progress, WHEEL_SCROLL_DURATION_US / 1000);
		panel_load_debug("focus", "SCROLL_TARGET", detail);
		g_free(detail);
	}
	gtk_adjustment_set_value(vadj, value);
	if (!wheel_scroll.active) {
		wheel_tick_id = 0;
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

/* Moves the chapter window around the verse being read, keeping the text
 * on the reading line exactly where it is on screen.
 *
 * The pane is laid out again (main_bible_window_recenter()), into a new
 * buffer, and GtkTextView starts that buffer from the top with its layout
 * mostly unmeasured: for the next two or three frames no scroll request
 * can put the text back where it was (measured on the real pane with
 * Matthew 15-17 -> 16-18: gtk_text_view_scroll_to_mark() landed 650-1300
 * px away; correcting by pixels converges to 0 px within 2-4 frames, but
 * the frames in between were painted 280-2400 px off). So painting is
 * frozen, the view is corrected by pixels on each frame until the text on
 * the reading line and the document height stop moving, and painting
 * resumes: the reader sees the old frame for ~3 frames, then the same text
 * in the same place (0 frames painted out of place on the real pane). A
 * wheel glide in progress is paused and what was left of it carried on
 * from the restored position. */
static gboolean
window_restore_target_iter(GtkTextIter *at)
{
	GtkTextIter s, e;

	if (!window_restore_anchor || !widgets.html_text ||
	    !wk_html_anchor_bounds(WK_HTML(widgets.html_text), window_restore_anchor,
				   &s, &e))
		return FALSE;
	*at = s;
	gtk_text_iter_forward_chars(at, MAX(window_restore_offset, 0));
	return TRUE;
}

static void
finish_window_restore(GtkTextView *view, gboolean converged)
{
	GtkAdjustment *vadj = view ? gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view)) : NULL;
	GtkTextIter s, e;
	gchar *focus_anchor;

	if (window_restore_frozen) {
		gdk_window_thaw_updates(window_restore_frozen);
		g_object_unref(window_restore_frozen);
		window_restore_frozen = NULL;
	}
	window_restore_pending = FALSE;

	/* the focus band, on the new buffer */
	focus_anchor = reading_focus_id ? g_strdup_printf("%d", reading_focus_id)
					: anchor_from_current_verse();
	if (focus_anchor && widgets.html_text &&
	    wk_html_anchor_bounds(WK_HTML(widgets.html_text), focus_anchor, &s, &e))
		wk_html_reading_focus_set(WK_HTML(widgets.html_text), &s, &e, NULL, NULL);
	g_free(focus_anchor);
	reading_line_valid = FALSE;

	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "lineAnchor=%s offset=%d viewportY=%.0f frames=%d converged=%d "
		    "scrollTop=%.1f resumeDistance=%.1f ms=%.1f",
		    window_restore_anchor ? window_restore_anchor : "-",
		    window_restore_offset, window_restore_y, window_restore_frames,
		    converged, vadj ? gtk_adjustment_get_value(vadj) : 0.0,
		    window_resume_distance,
		    (g_get_monotonic_time() - window_restore_started) / 1000.0);
		panel_load_debug("focus", "WINDOW_RESTORED", detail);
		g_free(detail);
	}
	g_clear_pointer(&window_restore_anchor, g_free);

	/* carry on with what was left of the wheel glide */
	if (view && vadj && window_resume_distance != 0.0 &&
	    scroll_origin == SCROLL_ORIGIN_USER) {
		GdkFrameClock *clock = gtk_widget_get_frame_clock(GTK_WIDGET(view));
		if (wheel_scroll_add(&wheel_scroll, gtk_adjustment_get_value(vadj),
				     window_resume_distance,
				     gtk_adjustment_get_lower(vadj),
				     gtk_adjustment_get_upper(vadj) -
					 gtk_adjustment_get_page_size(vadj),
				     clock ? gdk_frame_clock_get_frame_time(clock)
					   : g_get_monotonic_time()) &&
		    !wheel_tick_id)
			wheel_tick_id = gtk_widget_add_tick_callback(
			    GTK_WIDGET(view), on_wheel_scroll_tick, NULL, NULL);
	}
	window_resume_distance = 0.0;
}

static void
recenter_reading_window(void)
{
	GtkTextView *view = bible_view();
	GtkAdjustment *vadj;
	GtkTextIter at, s, e;
	GdkRectangle vis, rect;
	GdkWindow *toplevel;

	if (!view || !widgets.html_text || !settings.currentverse)
		return;
	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
	gtk_text_view_get_visible_rect(view, &vis);
	gtk_text_view_get_iter_at_location(view, &at, vis.x,
		vis.y + (gint)(vis.height * READING_FOCUS_LINE_RATIO));
	gtk_text_view_get_iter_location(view, &at, &rect);

	g_free(window_restore_anchor);
	window_restore_anchor = wk_html_anchor_at(WK_HTML(widgets.html_text), &at);
	window_restore_offset = 0;
	if (window_restore_anchor &&
	    wk_html_anchor_bounds(WK_HTML(widgets.html_text), window_restore_anchor, &s, &e))
		window_restore_offset =
		    gtk_text_iter_get_offset(&at) - gtk_text_iter_get_offset(&s);
	window_restore_y = rect.y - vis.y;
	window_resume_distance = (wheel_scroll.active && vadj)
		? wheel_scroll.target - gtk_adjustment_get_value(vadj) : 0.0;
	cancel_wheel_scroll();

	toplevel = gtk_widget_get_window(gtk_widget_get_toplevel(GTK_WIDGET(view)));
	if (toplevel) {
		window_restore_frozen = g_object_ref(toplevel);
		gdk_window_freeze_updates(window_restore_frozen);
	}
	window_restore_started = g_get_monotonic_time();

	if (!main_bible_window_recenter(settings.currentverse)) {
		window_resume_distance = 0.0;
		finish_window_restore(view, FALSE);
		return;
	}
	wk_html_cancel_anchor_jump(WK_HTML(widgets.html_text));
	window_restore_pending = TRUE;
	window_restore_frames = 0;
	window_restore_last_target = -1.0;
	window_restore_last_upper = -1.0;

	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "key=%s first=%s lineAnchor=%s offset=%d viewportY=%.0f "
		    "resumeDistance=%.1f", settings.currentverse, "window",
		    window_restore_anchor ? window_restore_anchor : "-",
		    window_restore_offset, window_restore_y, window_resume_distance);
		panel_load_debug("focus", "WINDOW_RECENTER", detail);
		g_free(detail);
	}
}

/* One frame of the restore: put the remembered text back at its distance
 * from the top by pixels, until neither it nor the document height moves
 * any more. TRUE while more frames are needed. */
#define WINDOW_RESTORE_MAX_FRAMES 12

static gboolean
step_window_restore(GtkTextView *view, GtkAdjustment *vadj)
{
	GtkTextIter at;
	GdkRectangle vis, rect;
	gdouble target, upper;

	window_restore_frames++;
	if (!window_restore_target_iter(&at)) {
		finish_window_restore(view, FALSE);
		return FALSE;
	}
	gtk_text_view_get_visible_rect(view, &vis);
	gtk_text_view_get_iter_location(view, &at, &rect);
	target = gtk_adjustment_get_value(vadj) + (rect.y - vis.y) - window_restore_y;
	upper = gtk_adjustment_get_upper(vadj);
	if (ABS(target - gtk_adjustment_get_value(vadj)) >= 0.5) {
		gtk_adjustment_set_value(vadj, target);
	} else if (ABS(target - window_restore_last_target) < 0.5 &&
		   upper == window_restore_last_upper) {
		finish_window_restore(view, TRUE);
		return FALSE;
	}
	window_restore_last_target = target;
	window_restore_last_upper = upper;
	if (window_restore_frames >= WINDOW_RESTORE_MAX_FRAMES) {
		finish_window_restore(view, FALSE);
		return FALSE;
	}
	return TRUE;
}

static gboolean
on_window_tick(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
	GtkTextView *view = GTK_TEXT_VIEW(widget);
	GtkAdjustment *vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(widget));
	gdouble value;

	(void)clock;
	(void)data;
	if (!vadj) {
		if (window_restore_pending)
			finish_window_restore(view, FALSE);
		window_tick_id = 0;
		return G_SOURCE_REMOVE;
	}

	if (window_restore_pending) {
		if (step_window_restore(view, vadj))
			return G_SOURCE_CONTINUE;
		window_tick_id = 0;
		return G_SOURCE_REMOVE;
	}

	if (!settings.currentverse ||
	    !main_bible_window_needs_recenter(settings.currentverse)) {
		window_tick_id = 0;
		return G_SOURCE_REMOVE;
	}
	value = gtk_adjustment_get_value(vadj);
	/* A touchpad, the scrollbar or GTK's kinetic scrolling still moving
	 * the view would fight the restored position: wait for a still frame.
	 * A wheel glide is ours, and is carried across the re-layout. */
	if (!wheel_scroll.active && value != window_last_value) {
		window_last_value = value;
		return G_SOURCE_CONTINUE;
	}
	recenter_reading_window();
	if (window_restore_pending)
		return G_SOURCE_CONTINUE;
	window_tick_id = 0;
	return G_SOURCE_REMOVE;
}

static void
schedule_window_recenter(void)
{
	GtkTextView *view = bible_view();
	GtkAdjustment *vadj;

	if (window_tick_id || !view || !settings.currentverse ||
	    !gtk_widget_get_mapped(GTK_WIDGET(view)) ||
	    !main_bible_window_needs_recenter(settings.currentverse))
		return;
	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
	window_last_value = vadj ? gtk_adjustment_get_value(vadj) : -1.0;
	window_tick_id = gtk_widget_add_tick_callback(GTK_WIDGET(view),
						      on_window_tick, NULL, NULL);
}

/* Watches every wheel/touchpad event on the Bible view.
 *
 * A mouse wheel (GDK source MOUSE, smooth or discrete) is handled here:
 * half GTK's own distance for the event (delta_y * page_size^(2/3), the
 * GtkScrolledWindow scroll unit; wheel_scroll_notch_distance()) becomes
 * the target of a short glide
 * driven by the frame clock, and the event is consumed so the scrolled
 * window does not also jump. Touchpads, trackpoints and anything else
 * keep GTK's native scrolling, which already follows the fingers.
 *
 * Against an edge the view cannot move; that step is counted as a push
 * for the reading focus (reading_focus_track). */
static gboolean
on_bible_user_scroll_event(GtkWidget *widget, GdkEventScroll *event,
			   gpointer data)
{
	GtkTextView *view = bible_view();
	GtkAdjustment *vadj;
	GdkDevice *source_device;
	GdkInputSource source = GDK_SOURCE_MOUSE;
	gboolean at_top = FALSE, at_bottom = FALSE, duplicate, emulated, wheel;
	gdouble amount, dx, dy;
	gint direction;

	(void)data;
	/* Ctrl+wheel is zoom (_scroll_zoom_cb), not a scroll */
	if (event->state & GDK_CONTROL_MASK)
		return FALSE;
	begin_user_scroll();
	duplicate = (gpointer)event == last_scroll_event &&
		    event->time == last_scroll_event_time;
	if (!duplicate) {
		last_scroll_event = event;
		last_scroll_event_time = event->time;
		scroll_event_seq++;
	}
	emulated = gdk_event_get_pointer_emulated((GdkEvent *)event);
	source_device = gdk_event_get_source_device((GdkEvent *)event);
	if (source_device)
		source = gdk_device_get_source(source_device);
	wheel = source_device && source == GDK_SOURCE_MOUSE;
	direction = scroll_event_direction(event, &dx, &dy, &amount);
	vadj = view ? gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view)) : NULL;
	if (vadj)
		scroll_edges(vadj, &at_top, &at_bottom);

	if (panel_load_debug_enabled()) {
		gchar *detail = g_strdup_printf(
		    "seq=%u widget=%s source_device=%s source_type=%s type=%s "
		    "dx=%.3f dy=%.3f time=%u emulated=%d duplicate=%d "
		    "scrollTop=%.1f atTop=%d atBottom=%d",
		    scroll_event_seq,
		    GTK_IS_SCROLLED_WINDOW(widget) ? "scrolled-window" : "view",
		    source_device ? gdk_device_get_name(source_device) : "none",
		    source_device ? input_source_name(source) : "none",
		    event->direction == GDK_SCROLL_SMOOTH ? "smooth" : "discrete",
		    dx, dy, event->time, emulated, duplicate,
		    vadj ? gtk_adjustment_get_value(vadj) : 0.0, at_top,
		    at_bottom);
		panel_load_debug("focus", "SCROLL_INPUT", detail);
		g_free(detail);
	}

	if (duplicate || !vadj)
		return FALSE;
	if (!wheel)
		cancel_wheel_scroll(); /* the fingers take over */
	if (wheel && emulated)
		return TRUE; /* the smooth event it copies does the work */
	if (!direction)
		return FALSE;

	if (wheel) {
		GdkFrameClock *clock =
		    gtk_widget_get_frame_clock(GTK_WIDGET(view));
		gint64 now = clock ? gdk_frame_clock_get_frame_time(clock)
				   : g_get_monotonic_time();
		gdouble delta = wheel_scroll_notch_distance(
		    dy, gtk_adjustment_get_page_size(vadj));

		if (wheel_scroll_add(&wheel_scroll, gtk_adjustment_get_value(vadj),
				     delta, gtk_adjustment_get_lower(vadj),
				     gtk_adjustment_get_upper(vadj) -
					 gtk_adjustment_get_page_size(vadj),
				     now)) {
			reading_focus_edge_accum = 0.0;
			if (!wheel_tick_id)
				wheel_tick_id = gtk_widget_add_tick_callback(
				    GTK_WIDGET(view), on_wheel_scroll_tick, NULL,
				    NULL);
			if (panel_load_debug_enabled()) {
				gchar *detail = g_strdup_printf(
				    "seq=%u current=%.1f delta=%.1f target=%.1f "
				    "durationMs=%d", scroll_event_seq,
				    gtk_adjustment_get_value(vadj), delta,
				    wheel_scroll.target,
				    WHEEL_SCROLL_DURATION_US / 1000);
				panel_load_debug("focus", "SCROLL_TARGET", detail);
				g_free(detail);
			}
			return TRUE;
		}
		/* nowhere to glide that way: at the edge, handled below */
	}

	if (!((direction > 0 && at_bottom) || (direction < 0 && at_top))) {
		reading_focus_edge_accum = 0.0;
		return wheel;
	}
	/* Against the edge the viewport does not move, so nothing else
	 * would update the focus. A wheel notch is one step; a touchpad
	 * sends many small deltas, and it takes about a notch's worth of
	 * them to make one. */
	if (reading_focus_edge_accum * direction < 0)
		reading_focus_edge_accum = 0.0;
	reading_focus_edge_accum += amount * direction;
	if (ABS(reading_focus_edge_accum) < 1.0)
		return wheel;
	reading_focus_edge_accum = 0.0;
	reading_focus_edge_push = direction;
	schedule_reading_focus_update();
	return wheel;
}

static gboolean
on_bible_scrollbar_press(GtkWidget *widget, GdkEventButton *event,
			 gpointer data)
{
	(void)widget;
	(void)event;
	(void)data;
	if (panel_load_debug_enabled())
		panel_load_debug("focus", "SCROLL_INPUT", "type=scrollbar");
	cancel_wheel_scroll();
	begin_user_scroll();
	return FALSE;
}

/* Verse navigation with the arrows, for when the Bible view has the
 * focus -- which, while reading, is nearly always. It has to live on
 * the view itself: the window-level handler in main_window.c
 * (on_vbox1_key_press_event) sits on a GtkBox, and GTK3 offers key
 * events to the focused widget first, so a GtkTextView would swallow
 * Up/Down for its own caret long before that box ever saw them.
 *
 * The two handlers therefore cover different focus situations and must
 * agree on what the keys do. They did not: this one skipped
 * main_interlineal_bloquea_navegacion(), the guard every other
 * navigation path consults, so the check meant to be able to pin the
 * view while the interlinear is open was bypassed on the one path the
 * reader actually uses. Keep them in step.
 *
 * Page Up/Down, Home and End move the viewport, not the verse: they are
 * the reader scrolling, like the wheel. */
static gboolean
on_bible_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	guint state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
				      GDK_MOD1_MASK);

	(void)widget;
	(void)user_data;
	switch (event->keyval) {
	case GDK_KEY_Page_Up:
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Up:
	case GDK_KEY_KP_Page_Down:
	case GDK_KEY_Home:
	case GDK_KEY_End:
		if (panel_load_debug_enabled())
			panel_load_debug("focus", "SCROLL_INPUT", "type=keyboard");
		cancel_wheel_scroll();
		begin_user_scroll();
		return FALSE;
	}
	if (state != 0)
		return FALSE;
	if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up ||
	    event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down) {
		if (main_interlineal_bloquea_navegacion())
			return TRUE;
	}
	if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up) {
		access_on_up_eventbox_button_release_event(VERSE_BUTTON);
		return TRUE;
	}
	if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down) {
		access_on_down_eventbox_button_release_event(VERSE_BUTTON);
		return TRUE;
	}
	return FALSE;
}

static void
gui_setup_text_selection_bridge(void)
{
	GtkTextView *view = bible_view();
	GtkAdjustment *vadj;
	if (!view)
		return;
	g_signal_connect(view, "key-press-event",
			 G_CALLBACK(on_bible_key_press), NULL);
	g_signal_connect(view, "button-release-event",
			 G_CALLBACK(on_native_select_done), NULL);
	g_signal_connect(view, "key-release-event",
			 G_CALLBACK(on_native_select_done), NULL);
	g_signal_connect(view, "scroll-event",
			 G_CALLBACK(on_bible_user_scroll_event), NULL);
	g_signal_connect(view, "scroll-event",
			 G_CALLBACK(_scroll_zoom_cb), NULL);
	{
		GtkWidget *sw = gtk_widget_get_ancestor(GTK_WIDGET(view),
							GTK_TYPE_SCROLLED_WINDOW);
		if (sw) {
			GtkWidget *bar = gtk_scrolled_window_get_vscrollbar(
			    GTK_SCROLLED_WINDOW(sw));
			g_signal_connect(sw, "scroll-event",
					 G_CALLBACK(on_bible_user_scroll_event), NULL);
			g_signal_connect(sw, "scroll-event",
					 G_CALLBACK(_scroll_zoom_cb), NULL);
			if (bar)
				g_signal_connect(bar, "button-press-event",
						 G_CALLBACK(on_bible_scrollbar_press),
						 NULL);
		}
	}

	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
	if (vadj) {
		g_signal_connect(vadj, "value-changed",
				 G_CALLBACK(on_reading_scroll_value_changed), NULL);
		g_signal_connect(vadj, "changed",
				 G_CALLBACK(on_reading_adjustment_changed), NULL);
	}
}

GtkWidget *gui_create_bible_pane(void)
{
	GtkWidget *vbox;
	GtkWidget *with_il;
	GtkWidget *split;

	UI_VBOX(vbox, FALSE, 0);
	gtk_widget_show(vbox);

	widgets.html_text =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, TEXT_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(widgets.html_text, "bible-main");
	gtk_widget_show(widgets.html_text);
	with_il = gui_interlineal_wrap(widgets.html_text);
	split = gui_lectura_sync_wrap(with_il);
	gtk_box_pack_start(GTK_BOX(vbox), split, TRUE, TRUE, 0);

	g_signal_connect((gpointer)widgets.html_text,
			 "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb), NULL);

	gui_setup_text_selection_bridge();

	return vbox;
}
