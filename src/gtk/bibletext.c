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
#include "gui/tabbed_browser.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/settings.h"
#include "main/interlineal.h"
#include "main/lectura_sync.h"
#include "main/lists.h"
#include "main/navbar_versekey.h"
#include "main/sword.h"
#include "main/url.hh"
#include "main/xml.h"
#include "main/global_ops.hh"
#include "main/display.hh"

gboolean shift_key_pressed = FALSE;
guint scroll_adj_signal;
GtkAdjustment *adjustment;

/* Ctrl+scroll over the text pane bumps the same base-font-size bias as
 * the header-bar zoom buttons / Ctrl+Shift+'+'/Ctrl+'-', instead of
 * scrolling the page -- a standard e-reader/browser gesture that was
 * entirely missing. Plain scroll (no Ctrl) is left alone so normal page
 * scrolling still works. */
static gboolean
_scroll_zoom_cb(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
	if (!(event->state & GDK_CONTROL_MASK))
		return FALSE;

	if (event->direction == GDK_SCROLL_UP)
		gui_zoom_base_font(TRUE);
	else if (event->direction == GDK_SCROLL_DOWN)
		gui_zoom_base_font(FALSE);
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
// "Comparar" reading focus: while the split-view panel is open, track
// which verse is at the top of the main pane's viewport as the user
// scrolls, and mirror it -- a native tag highlight up here, the
// matching verse rendered below -- without re-rendering this pane out
// from under the user's own scrolling (a full re-render would fight the
// scroll position instead of following it).
// ---------------------------------------------------------------------

#define READING_FOCUS_DEBOUNCE_MS 50
#define READING_FOCUS_CHROME_PAD 44
#define READING_FOCUS_MAX_STEP 8

static gchar *lectura_sync_focus_anchor = NULL;
static guint lectura_sync_scroll_debounce_id = 0;
/* Ignore adjustment changes caused by programmatic jump / split resize
 * so they don't steal the navigated verse and point Comparar elsewhere. */
static gint64 lectura_sync_ignore_scroll_until = 0;

static void
ignore_scroll_briefly(void)
{
	if (lectura_sync_scroll_debounce_id) {
		g_source_remove(lectura_sync_scroll_debounce_id);
		lectura_sync_scroll_debounce_id = 0;
	}
	/* Cover jump retries (~240 ms) plus a settle so arrow-next is
	 * not immediately overwritten by the still-old viewport. */
	lectura_sync_ignore_scroll_until = g_get_monotonic_time() + 600000;
}

/* First verse whose start sits below the top chrome, so a short verse
 * fully on screen is never skipped in favour of one further down. */
static gchar *
find_focus_anchor(GtkTextView *view)
{
	GdkRectangle vis, loc;
	GtkTextIter iter, vs, ve;
	gchar *anchor, *next;

	gtk_text_view_get_visible_rect(view, &vis);
	gtk_text_view_get_iter_at_location(view, &iter, vis.x,
					   vis.y + READING_FOCUS_CHROME_PAD);
	anchor = wk_html_anchor_at(WK_HTML(widgets.html_text), &iter);
	if (!anchor || !*anchor || !strcmp(anchor, "0"))
		return anchor;
	if (!wk_html_anchor_bounds(WK_HTML(widgets.html_text), anchor, &vs, &ve))
		return anchor;
	gtk_text_view_get_iter_location(view, &vs, &loc);
	if (loc.y >= vis.y + READING_FOCUS_CHROME_PAD - 2)
		return anchor;
	next = wk_html_anchor_at(WK_HTML(widgets.html_text), &ve);
	if (next && *next && strcmp(next, "0") && strcmp(next, anchor)) {
		g_free(anchor);
		return next;
	}
	g_free(next);
	return anchor;
}

/* Do not skip verses: if the viewport jumped over several, advance one
 * at a time so a brief verse still gets the focus band. Snap only when
 * the user clearly dragged a long way. */
static gchar *
step_focus_anchor(const gchar *from, const gchar *toward)
{
	long a, b, n, cand_n;
	gchar *cand;
	GtkTextIter s, e;

	if (!toward)
		return NULL;
	if (!from || !*from)
		return g_strdup(toward);
	a = atol(from);
	b = atol(toward);
	if (a / 1000 != b / 1000)
		return g_strdup(toward);
	n = labs(b - a);
	if (n <= 1 || n > READING_FOCUS_MAX_STEP)
		return g_strdup(toward);
	cand_n = (b > a) ? a + 1 : a - 1;
	cand = g_strdup_printf("%ld", cand_n);
	if (widgets.html_text &&
	    wk_html_anchor_bounds(WK_HTML(widgets.html_text), cand, &s, &e))
		return cand;
	g_free(cand);
	return g_strdup(toward);
}

void
gui_bibletext_lectura_sync_clear_focus(void)
{
	if (lectura_sync_scroll_debounce_id) {
		g_source_remove(lectura_sync_scroll_debounce_id);
		lectura_sync_scroll_debounce_id = 0;
	}
	g_clear_pointer(&lectura_sync_focus_anchor, g_free);
	gui_bibletext_mark_current_verse();
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

void
gui_bibletext_mark_current_verse(void)
{
	gchar *anchor;
	GtkTextIter s, e;

	if (!widgets.html_text)
		return;
	ignore_scroll_briefly();
	anchor = anchor_from_current_verse();
	if (!anchor)
		return;
	if (wk_html_anchor_bounds(WK_HTML(widgets.html_text), anchor, &s, &e))
		wk_html_reading_focus_set(WK_HTML(widgets.html_text), &s, &e,
					  NULL, NULL);
	wk_html_ensure_anchor_visible(WK_HTML(widgets.html_text), anchor);
	g_free(anchor);
}

void
gui_bibletext_lectura_sync_focus_current(void)
{
	gchar *anchor;
	GtkTextIter s, e;

	if (!settings.currentverse || !widgets.html_text)
		return;

	ignore_scroll_briefly();

	anchor = anchor_from_current_verse();
	if (!anchor)
		return;

	if (settings.show_lectura_sync)
		wk_html_ensure_anchor_visible(WK_HTML(widgets.html_text), anchor);

	if (wk_html_anchor_bounds(WK_HTML(widgets.html_text), anchor, &s, &e))
		wk_html_reading_focus_set(WK_HTML(widgets.html_text), &s, &e,
					  NULL, NULL);

	g_free(lectura_sync_focus_anchor);
	lectura_sync_focus_anchor = anchor;
}

void
gui_bibletext_lectura_sync_focus_refresh(void)
{
	GtkTextView *view;
	gchar *anchor;
	GtkTextIter s, e;
	long cv;
	gchar *book, *ref;

	view = bible_view();
	if (!view || !widgets.html_text)
		return;

	anchor = find_focus_anchor(view);
	if (!anchor || !*anchor || !strcmp(anchor, "0")) {
		g_free(anchor);
		return;
	}
	{
		gchar *stepped = step_focus_anchor(lectura_sync_focus_anchor, anchor);
		g_free(anchor);
		anchor = stepped;
	}
	if (lectura_sync_focus_anchor && !strcmp(anchor, lectura_sync_focus_anchor)) {
		g_free(anchor);
		return;
	}

	if (wk_html_anchor_bounds(WK_HTML(widgets.html_text), anchor, &s, &e))
		wk_html_reading_focus_set(WK_HTML(widgets.html_text), &s, &e,
					  NULL, NULL);

	g_free(lectura_sync_focus_anchor);
	lectura_sync_focus_anchor = anchor;

	cv = atol(lectura_sync_focus_anchor);
	book = book_from_current_verse();
	ref = g_strdup_printf("%s %ld:%ld", book, cv / 1000, cv % 1000);
	g_free(book);

	xml_set_value("Xiphos", "keys", "verse", ref);
	settings.currentverse = xml_get_value("keys", "verse");
	main_navbar_versekey_set(navbar_versekey, ref);
	gui_set_tab_label(ref, FALSE);

	if (settings.show_lectura_sync)
		main_lectura_sync_focus_verse(ref);
	if (main_interlineal_quizas_plegar(ref)) {
		main_bible_note_interlinear_html();
		main_display_bible(NULL, ref);
	}
	g_free(ref);
}

static gboolean
on_lectura_sync_scroll_settle(gpointer data)
{
	(void)data;
	lectura_sync_scroll_debounce_id = 0;
	gui_bibletext_lectura_sync_focus_refresh();
	return G_SOURCE_REMOVE;
}

/* Throttle, not debounce: a debounce (cancel + reschedule on every
 * event) only ever fires once scrolling fully stops, so during one
 * continuous scroll gesture the focus silently skips straight from
 * wherever it started to wherever the user let go -- verses in
 * between never got a turn. Letting the first scroll tick schedule an
 * update and ignoring further ticks until it fires instead gives a
 * steady update every READING_FOCUS_DEBOUNCE_MS *throughout* the
 * scroll, so it follows along verse by verse. */
static void
on_lectura_sync_scroll_value_changed(GtkAdjustment *adj, gpointer data)
{
	(void)adj;
	(void)data;
	if (g_get_monotonic_time() < lectura_sync_ignore_scroll_until)
		return;
	if (lectura_sync_scroll_debounce_id)
		return;
	lectura_sync_scroll_debounce_id =
	    g_timeout_add(READING_FOCUS_DEBOUNCE_MS, on_lectura_sync_scroll_settle, NULL);
}

static gboolean
on_bible_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	guint state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
				      GDK_MOD1_MASK);

	(void)widget;
	(void)user_data;
	if (state != 0)
		return FALSE;
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
			 G_CALLBACK(_scroll_zoom_cb), NULL);
	{
		GtkWidget *sw = gtk_widget_get_ancestor(GTK_WIDGET(view),
							GTK_TYPE_SCROLLED_WINDOW);
		if (sw)
			g_signal_connect(sw, "scroll-event",
					 G_CALLBACK(_scroll_zoom_cb), NULL);
	}

	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));
	if (vadj)
		g_signal_connect(vadj, "value-changed",
				 G_CALLBACK(on_lectura_sync_scroll_value_changed), NULL);
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
