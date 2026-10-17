/*
 * Xiphos Bible Study Tool
 * gtktextview_editor.c - editor using GtkTextView
 *
 * Copyright (C) 2005-2025 Xiphos Developer Team
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

#include <config.h>

#ifdef USE_GTKTVeditor

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include "gui/widget_helpers.h"

#include "editor/gtktextview_editor.h"
#include "editor/docx_import.h"
#include "editor/study_library.h"
#include "editor/study_document_export.h"
#include "editor/study_templates.h"
#include "editor/link_dialog.h"
#include "gui/table_helpers.h"

#include "main/settings.h"
#include "main/sword.h"
#include "main/sword_treekey.h"
#include "main/url.hh"
#include "main/xml.h"
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>

#include "gui/navbar_versekey_editor.h"
#include "gui/dialog.h"
#include "gui/widgets.h"
#include "gui/xiphos.h"
#include "gui/treekey-editor.h"
#include "gui/utilities.h"

#include "gui/debug_glib_null.h"

FIND_DIALOG find_dialog;
BUTTONS_STATE buttons_state;

static GList *editors_all = NULL;

/* forward declarations */
static gboolean _load_file(EDITOR *e, const gchar *filename);
static gboolean _save_file(EDITOR *e, gboolean force_dialog);
static void _load_text_into_buffer(EDITOR *e, const gchar *text);
static void _save_note(EDITOR *e);
static void _save_book(EDITOR *e);
static void study_organize_dialog(EDITOR *e);
static void study_library_dialog(EDITOR *e);
static void study_template_dialog(EDITOR *e);
static void study_history_dialog(EDITOR *e);
static void study_export_document(EDITOR *e, gboolean as_pdf);
static void study_insert_current_citation(EDITOR *e);
static gboolean editor_is_dirty(EDITOR *e);
static void do_exit(EDITOR *e);
static void change_window_title(GtkWidget *window, const gchar *title);
static void editor_update_title(EDITOR *e, const gchar *title);
GtkWidget *editor_new(const gchar *title, EDITOR *e);
static void _setup_text_tags(GtkTextBuffer *buffer);
static gboolean _on_key_press(GtkWidget *widget, GuiKeyEvent *event, EDITOR *e);
void action_insert_image_activate_cb(GtkWidget *widget, EDITOR *e);
void action_save_activate_cb(GtkWidget *widget, EDITOR *e);
void action_justify_left_activate_cb(GtkWidget *widget, EDITOR *e);
void action_justify_right_activate_cb(GtkWidget *widget, EDITOR *e);
void action_justify_center_activate_cb(GtkWidget *widget, EDITOR *e);
void action_justify_full_activate_cb(GtkWidget *widget, EDITOR *e);

static gboolean
editor_close_request_cb(GtkWindow *window, EDITOR *e)
{
	return delete_event(GTK_WIDGET(window), NULL, e);
}

static void
text_view_link_pressed(GtkGestureClick *gesture, gint n_press,
			       gdouble x, gdouble y, EDITOR *e)
{
	GtkWidget *widget = gtk_event_controller_get_widget(
		GTK_EVENT_CONTROLLER(gesture));
	GtkTextIter iter;
	GSList *tags;

	if (n_press != 1 || !gtk_text_view_get_iter_at_location(
		GTK_TEXT_VIEW(widget), &iter, (gint)x, (gint)y))
		return;

	tags = gtk_text_iter_get_tags(&iter);
	for (GSList *node = tags; node; node = node->next) {
		GtkTextTag *tag = node->data;
		gchar *name = NULL;
		g_object_get(tag, "name", &name, NULL);
		if (name && g_str_has_prefix(name, "sword_link_")) {
			const gchar *uri = g_object_get_data(G_OBJECT(tag), "uri");
			if (uri)
				main_url_handler(uri, TRUE);
			g_free(name);
			break;
		}
		g_free(name);
	}
	g_slist_free(tags);
}

/* ============================================================
 * TextTag names - used throughout for formatting
 * ============================================================ */
#define TAG_BOLD        "bold"
#define TAG_ITALIC      "italic"
#define TAG_UNDERLINE   "underline"
#define TAG_STRIKE      "strikethrough"
#define TAG_SWORD_LINK  "sword-link"

/* ============================================================
 * Helpers
 * ============================================================ */

static GtkTextBuffer *
_get_buffer(EDITOR *e)
{
	return gtk_text_view_get_buffer(GTK_TEXT_VIEW(e->text_widget));
}

static void
change_window_title(GtkWidget *window, const gchar *title)
{
	gtk_window_set_title(GTK_WINDOW(window), title);
}

static void
editor_update_title(EDITOR *e, const gchar *title)
{
	if (e->embedded && e->title_label)
		gtk_label_set_text(GTK_LABEL(e->title_label), title);
	else if (e->window)
		change_window_title(e->window, title);
}

static gboolean
editor_is_dirty(EDITOR *e)
{
	return e->is_changed;
}

/* ============================================================
 * Text tag setup
 * ============================================================ */

static void
_setup_text_tags(GtkTextBuffer *buffer)
{
	gtk_text_buffer_create_tag(buffer, TAG_BOLD,
				   "weight", PANGO_WEIGHT_BOLD,
				   NULL);
	gtk_text_buffer_create_tag(buffer, TAG_ITALIC,
				   "style", PANGO_STYLE_ITALIC,
				   NULL);
	gtk_text_buffer_create_tag(buffer, TAG_UNDERLINE,
				   "underline", PANGO_UNDERLINE_SINGLE,
				   NULL);
	gtk_text_buffer_create_tag(buffer, TAG_STRIKE,
				   "strikethrough", TRUE,
				   NULL);
	/* sword:// links - styled like hyperlinks */
	gtk_text_buffer_create_tag(buffer, TAG_SWORD_LINK,
				   "foreground", "#0000EE",
				   "underline", PANGO_UNDERLINE_SINGLE,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "h1", "weight", PANGO_WEIGHT_BOLD, "scale", 1.65, "pixels-above-lines", 12, "pixels-below-lines", 8, NULL);
	gtk_text_buffer_create_tag(buffer, "h2", "weight", PANGO_WEIGHT_BOLD, "scale", 1.25, "pixels-above-lines", 10, "pixels-below-lines", 5, NULL);
	gtk_text_buffer_create_tag(buffer, "h3", "weight", PANGO_WEIGHT_BOLD, "scale", 1.1, NULL);
	gtk_text_buffer_create_tag(buffer, "h4", "weight", PANGO_WEIGHT_BOLD, "scale", 1.05, NULL);
	gtk_text_buffer_create_tag(buffer, "h5", "weight", PANGO_WEIGHT_BOLD, "scale", 1.0, NULL);
	gtk_text_buffer_create_tag(buffer, "h6", "weight", PANGO_WEIGHT_BOLD, "scale", 0.8, NULL);
	gtk_text_buffer_create_tag(buffer, "address",
		"style", PANGO_STYLE_ITALIC,
		"left-margin", 20,
		"right-margin", 20,
		NULL);
	gtk_text_buffer_create_tag(buffer, "pre",
		"family", "Monospace",
		"wrap-mode", GTK_WRAP_NONE,
		NULL);
		/* list styles */
	gtk_text_buffer_create_tag(buffer, "list-bullet",
				"left-margin", 20,
		NULL);
	gtk_text_buffer_create_tag(buffer, "list-roman",
				"left-margin", 20,
				NULL);
	gtk_text_buffer_create_tag(buffer, "list-numbered",
				"left-margin", 20,
			NULL);
	gtk_text_buffer_create_tag(buffer, "list-alpha",
				"left-margin", 20,
				NULL);
}

/* ============================================================
 * Apply / remove a tag on the current selection
 * ============================================================ */

static void
_toggle_tag(EDITOR *e, const gchar *tag_name)
{
	GtkTextBuffer *buffer = _get_buffer(e);
	GtkTextIter start, end;

	if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		/* no selection - use current line */
		gtk_text_buffer_get_iter_at_mark(buffer, &start,
						 gtk_text_buffer_get_insert(buffer));
		end = start;
		gtk_text_iter_set_line_offset(&start, 0);
		gtk_text_iter_forward_to_line_end(&end);
	}

	GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
	GtkTextTag *tag = gtk_text_tag_table_lookup(table, tag_name);
	if (!tag)
		return;

	/* if entire selection already has tag, remove it; otherwise apply */
	if (gtk_text_iter_has_tag(&start, tag))
		gtk_text_buffer_remove_tag(buffer, tag, &start, &end);
	else
		gtk_text_buffer_apply_tag(buffer, tag, &start, &end);

	e->is_changed = TRUE;
}

/* ============================================================
 * Serialize buffer content to a string (plain text for now;
 * format to be confirmed by Karl - see GitHub discussion)
 * ============================================================ */

static gchar *
_serialize_buffer(EDITOR *e)
{
    GtkTextBuffer *buffer = _get_buffer(e);
    GtkTextIter iter, end;
    GString *html = g_string_new("");

    gtk_text_buffer_get_start_iter(buffer, &iter);
    gtk_text_buffer_get_end_iter(buffer, &end);

    while (!gtk_text_iter_equal(&iter, &end)) {
        gboolean block_closed = FALSE;
        /* tags ouvrants */
        GSList *tags = gtk_text_iter_get_toggled_tags(&iter, TRUE);
        for (GSList *t = tags; t; t = t->next) {
            GtkTextTag *tag = t->data;
            gchar *name = NULL;
            g_object_get(tag, "name", &name, NULL);
            if (!g_strcmp0(name, TAG_BOLD))
                g_string_append(html, "<b>");
            else if (!g_strcmp0(name, TAG_ITALIC))
                g_string_append(html, "<i>");
            else if (!g_strcmp0(name, TAG_UNDERLINE))
                g_string_append(html, "<u>");
            else if (!g_strcmp0(name, TAG_STRIKE))
                g_string_append(html, "<s>");
            else if (!g_strcmp0(name, "h1"))
                g_string_append(html, "<h1>");
            else if (!g_strcmp0(name, "h2"))
                g_string_append(html, "<h2>");
            else if (!g_strcmp0(name, "h3"))
                g_string_append(html, "<h3>");
            else if (!g_strcmp0(name, "h4"))
                g_string_append(html, "<h4>");
            else if (!g_strcmp0(name, "h5"))
                g_string_append(html, "<h5>");
            else if (!g_strcmp0(name, "h6"))
                g_string_append(html, "<h6>");
			else if (!g_strcmp0(name, "list-bullet"))
				g_string_append(html, "<ul><li>");
			else if (!g_strcmp0(name, "list-roman"))
				g_string_append(html, "<ol type=\"i\"><li>");
			else if (!g_strcmp0(name, "list-numbered"))
				g_string_append(html, "<ol><li>");
			else if (!g_strcmp0(name, "list-alpha"))
				g_string_append(html, "<ol type=\"a\"><li>");
			else if (g_str_has_prefix(name, "fg_"))
				g_string_append_printf(html, "<span style=\"color:%s\">", name + 3);
			else if (g_str_has_prefix(name, "bg_"))
				g_string_append_printf(html, "<span style=\"background-color:%s\">", name + 3);
			else if (g_str_has_prefix(name, "sword_link_")) {
				gchar *uri = g_object_get_data(G_OBJECT(tag), "uri");
				g_string_append_printf(html, "<a href=\"%s\">", uri ? uri : "");
			}
			else if (!g_strcmp0(name, "address"))
				g_string_append(html, "<address>");
			else if (!g_strcmp0(name, "pre"))
				g_string_append(html, "<pre>");
			g_free(name);
        }
        g_slist_free(tags);

        /* closing tags */
        tags = gtk_text_iter_get_toggled_tags(&iter, FALSE);
        for (GSList *t = tags; t; t = t->next) {
            GtkTextTag *tag = t->data;
            gchar *name = NULL;
            g_object_get(tag, "name", &name, NULL);
            if ((name && strlen(name) == 2 && name[0] == 'h' &&
                 name[1] >= '1' && name[1] <= '6') ||
                !g_strcmp0(name, "address") || !g_strcmp0(name, "pre") ||
                (name && g_str_has_prefix(name, "list-")))
                block_closed = TRUE;
            if (!g_strcmp0(name, TAG_BOLD))
                g_string_append(html, "</b>");
            else if (!g_strcmp0(name, TAG_ITALIC))
                g_string_append(html, "</i>");
            else if (!g_strcmp0(name, TAG_UNDERLINE))
                g_string_append(html, "</u>");
            else if (!g_strcmp0(name, TAG_STRIKE))
                g_string_append(html, "</s>");
            else if (!g_strcmp0(name, "h1"))
                g_string_append(html, "</h1>");
            else if (!g_strcmp0(name, "h2"))
                g_string_append(html, "</h2>");
            else if (!g_strcmp0(name, "h3"))
                g_string_append(html, "</h3>");
            else if (!g_strcmp0(name, "h4"))
                g_string_append(html, "</h4>");
            else if (!g_strcmp0(name, "h5"))
                g_string_append(html, "</h5>");
            else if (!g_strcmp0(name, "h6"))
                g_string_append(html, "</h6>");
			else if (!g_strcmp0(name, "list-bullet"))
				g_string_append(html, "</li></ul>");
			else if (!g_strcmp0(name, "list-roman") ||
				!g_strcmp0(name, "list-numbered") ||
				!g_strcmp0(name, "list-alpha"))
			g_string_append(html, "</li></ol>");
            else if (g_str_has_prefix(name, "fg_") ||
                     g_str_has_prefix(name, "bg_"))
                g_string_append(html, "</span>");
            else if (g_str_has_prefix(name, "sword_link_"))
                g_string_append(html, "</a>");
            else if (!g_strcmp0(name, "address"))
				g_string_append(html, "</address>");
			else if (!g_strcmp0(name, "pre"))
					g_string_append(html, "</pre>");
            g_free(name);
        }
        g_slist_free(tags);

		GdkPaintable *paintable = gtk_text_iter_get_paintable(&iter);
		if (paintable) {
			const gchar *path = g_object_get_data(G_OBJECT(paintable),
							     "image-path");
			gchar *escaped = g_markup_escape_text(path ? path : "", -1);
			g_string_append_printf(html, "<img src=\"%s\"/>", escaped);
			g_free(escaped);
			gtk_text_iter_forward_char(&iter);
			continue;
		}

		gunichar c = gtk_text_iter_get_char(&iter);
		if (c == '<')
			g_string_append(html, "&lt;");
		else if (c == '>')
			g_string_append(html, "&gt;");
		else if (c == '&')
			g_string_append(html, "&amp;");
		else if (c == '\n') {
			/* A closing block already supplies this paragraph boundary. */
			if (!block_closed) g_string_append(html, "<br/>");
		}
		else {
			gchar buf[7];
			gint len = g_unichar_to_utf8(c, buf);
			buf[len] = '\0';
			g_string_append(html, buf);
		}

        gtk_text_iter_forward_char(&iter);
    }

	/* no wrapper tags - store raw HTML for compatibility */
    return g_string_free(html, FALSE);
}

static gchar *
study_recovery_path(void)
{
	gchar *directory = g_build_filename(g_get_user_data_dir(), "biblia-elim",
					    "recovery", NULL);
	gchar *path = g_build_filename(directory, "studypad.html", NULL);
	g_free(directory);
	return path;
}

static gchar *
study_recovery_source_path(void)
{
	gchar *directory = g_build_filename(g_get_user_data_dir(), "biblia-elim",
					    "recovery", NULL);
	gchar *path = g_build_filename(directory, "source-path", NULL);
	g_free(directory);
	return path;
}

static void
study_recovery_clear(EDITOR *e)
{
	gchar *content, *source;
	if (!e || !e->embedded)
		return;
	content = study_recovery_path();
	source = study_recovery_source_path();
	g_remove(content);
	g_remove(source);
	g_free(content);
	g_free(source);
}

static gboolean
study_autosave_cb(gpointer data)
{
	EDITOR *e = data;
	gchar *content_path, *source_path, *directory, *html;
	GError *error = NULL;
	gboolean saved = FALSE;

	if (!e->embedded || !e->studypad)
		return G_SOURCE_CONTINUE;
	if (!e->is_changed) {
		e->autosave_source = 0;
		return G_SOURCE_REMOVE;
	}
	content_path = study_recovery_path();
	source_path = study_recovery_source_path();
	directory = g_path_get_dirname(content_path);
	html = _serialize_buffer(e);
	if (g_mkdir_with_parents(directory, 0700) == 0 &&
	    g_file_set_contents(content_path, html, -1, &error)) {
		saved = g_file_set_contents(source_path,
					    e->filename ? e->filename : "", -1,
					    &error);
	}
	if (!saved && error) {
		g_warning("StudyPad autosave failed: %s", error->message);
		g_clear_error(&error);
	}
	g_free(html);
	g_free(directory);
	g_free(content_path);
	g_free(source_path);
	return G_SOURCE_CONTINUE;
}

static void
study_recovery_offer(EDITOR *e)
{
	gchar *content_path, *source_path, *html = NULL, *source = NULL;
	if (!e->embedded || !e->studypad)
		return;
	content_path = study_recovery_path();
	source_path = study_recovery_source_path();
	if (g_file_get_contents(content_path, &html, NULL, NULL) &&
	    g_file_get_contents(source_path, &source, NULL, NULL) &&
	    g_strcmp0(source, e->filename ? e->filename : "") == 0 && html && *html) {
		if (gui_yes_no_dialog(
			_("Se encontró un borrador recuperable de tu estudio. ¿Quieres restaurarlo?"),
			"document-save-symbolic")) {
			_load_text_into_buffer(e, html);
			e->is_changed = TRUE;
			editor_update_title(e, _("Estudio recuperado"));
		} else {
			study_recovery_clear(e);
		}
	} else if (g_file_test(content_path, G_FILE_TEST_EXISTS))
		study_recovery_clear(e);
	g_free(html);
	g_free(source);
	g_free(content_path);
	g_free(source_path);
}

/* ============================================================
 * Load content into the buffer
 * ============================================================ */

/* Insert unstyled text; GtkTextBuffer otherwise inherits the preceding
 * heading/bold tags at adjacent HTML boundaries. Ancestors apply their own
 * tags after all descendants have been inserted. */
static void
insert_html_text(GtkTextBuffer *buffer, GtkTextIter *iter, const gchar *text)
{
	gint offset = gtk_text_iter_get_offset(iter);
	gtk_text_buffer_insert(buffer, iter, text, -1);
	GtkTextIter start;
	gtk_text_buffer_get_iter_at_offset(buffer, &start, offset);
	gtk_text_buffer_remove_all_tags(buffer, &start, iter);
}

static void
ensure_html_newline(GtkTextBuffer *buffer, GtkTextIter *iter)
{
	if (!gtk_text_iter_is_start(iter) && !gtk_text_iter_starts_line(iter))
		insert_html_text(buffer, iter, "\n");
}

static void
_parse_html_node(GtkTextBuffer *buffer, GtkTextIter *iter,
		 xmlNodePtr node, EDITOR *e)
{
	for (xmlNodePtr n = node; n; n = n->next) {
		if (n->type == XML_TEXT_NODE) {
			/* remplacer les entités HTML */
			gchar *content = (gchar *)n->content;
			if (content)
				insert_html_text(buffer, iter, content);
		} else if (n->type == XML_ELEMENT_NODE) {
			const gchar *name = (const gchar *)n->name;
			gboolean block = !g_ascii_strcasecmp(name, "p") ||
				!g_ascii_strcasecmp(name, "div") || !g_ascii_strcasecmp(name, "li") ||
				!g_ascii_strcasecmp(name, "blockquote") || !g_ascii_strcasecmp(name, "pre") ||
				!g_ascii_strcasecmp(name, "address") ||
				(strlen(name) == 2 && g_ascii_tolower(name[0]) == 'h' &&
				 name[1] >= '1' && name[1] <= '6');
			if (block)
				ensure_html_newline(buffer, iter);
			GtkTextMark *mark_start =
				gtk_text_buffer_create_mark(buffer, NULL, iter, TRUE);

			if (n->children)
				_parse_html_node(buffer, iter, n->children, e);

			GtkTextIter start;
			gtk_text_buffer_get_iter_at_mark(buffer, &start, mark_start);
			gtk_text_buffer_delete_mark(buffer, mark_start);

			const gchar *tag = NULL;
			if (!g_ascii_strcasecmp((gchar *)n->name, "b"))
				tag = TAG_BOLD;
			else if (!g_ascii_strcasecmp((gchar *)n->name, "i"))
				tag = TAG_ITALIC;
			else if (!g_ascii_strcasecmp((gchar *)n->name, "u"))
				tag = TAG_UNDERLINE;
			else if (!g_ascii_strcasecmp((gchar *)n->name, "s"))
				tag = TAG_STRIKE;
			else if (!g_ascii_strcasecmp((gchar *)n->name, "h1"))
				tag = "h1";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "h2"))
				tag = "h2";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "h3"))
				tag = "h3";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "h4"))
				tag = "h4";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "h5"))
				tag = "h5";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "h6"))
				tag = "h6";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "address"))
				tag = "address";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "ul"))
				tag = "list-bullet";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "ol")) {
				xmlChar *type = xmlGetProp(n, (xmlChar *)"type");
				if (type && !g_ascii_strcasecmp((gchar *)type, "i"))
					tag = "list-roman";
				else if (type && !g_ascii_strcasecmp((gchar *)type, "a"))
					tag = "list-alpha";
				else
					tag = "list-numbered";
				if (type) xmlFree(type);
			}
			else if (!g_ascii_strcasecmp((gchar *)n->name, "pre"))
				tag = "pre";
			else if (!g_ascii_strcasecmp((gchar *)n->name, "br"))
				insert_html_text(buffer, iter, "\n");
			else if (!g_ascii_strcasecmp((gchar *)n->name, "a")) {
				xmlChar *href = xmlGetProp(n, (xmlChar *)"href");
				if (href) {
					gchar *tag_name = g_strdup_printf("sword_link_%s",
									  (gchar *)href);
					GtkTextTagTable *table =
						gtk_text_buffer_get_tag_table(buffer);
					GtkTextTag *atag =
						gtk_text_tag_table_lookup(table, tag_name);
					if (!atag) {
						atag = gtk_text_buffer_create_tag(
							buffer, tag_name,
							"foreground", "#0000EE",
							"underline", PANGO_UNDERLINE_SINGLE,
							NULL);
						g_object_set_data_full(G_OBJECT(atag), "uri",
								       g_strdup((gchar *)href),
								       g_free);
					}
					gtk_text_buffer_apply_tag_by_name(buffer, tag_name,
									  &start, iter);
					g_free(tag_name);
					xmlFree(href);
				}
			} else if (!g_ascii_strcasecmp((gchar *)n->name, "span")) {
				xmlChar *style = xmlGetProp(n, (xmlChar *)"style");
				if (style) {
					gchar *style_str = (gchar *)style;
					GtkTextTagTable *table =
						gtk_text_buffer_get_tag_table(buffer);
					if (g_str_has_prefix(style_str, "color:")) {
						gchar *color = style_str + 6;
						gchar *tag_name = g_strdup_printf("fg_%s", color);
						if (!gtk_text_tag_table_lookup(table, tag_name))
							gtk_text_buffer_create_tag(buffer, tag_name,
										   "foreground", color,
										   NULL);
						gtk_text_buffer_apply_tag_by_name(buffer, tag_name,
										  &start, iter);
						g_free(tag_name);
					} else if (g_str_has_prefix(style_str, "background-color:")) {
						gchar *color = style_str + 17;
						gchar *tag_name = g_strdup_printf("bg_%s", color);
						if (!gtk_text_tag_table_lookup(table, tag_name))
							gtk_text_buffer_create_tag(buffer, tag_name,
										   "background", color,
										   NULL);
						gtk_text_buffer_apply_tag_by_name(buffer, tag_name,
										  &start, iter);
						g_free(tag_name);
					}
					xmlFree(style);
				}
			}

			if (tag)
				gtk_text_buffer_apply_tag_by_name(buffer, tag,
								  &start, iter);
			if (block) {
				if (gtk_text_iter_equal(&start, iter))
					insert_html_text(buffer, iter, "\n");
				else
					ensure_html_newline(buffer, iter);
			}
		}
	}
}

static void
_load_text_into_buffer(EDITOR *e, const gchar *text)
{
	GtkTextBuffer *buffer = _get_buffer(e);
	GtkTextIter iter;
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_set_text(buffer, "", -1);
	gtk_text_buffer_get_start_iter(buffer, &iter);

/* wrap in html/body for libxml2 parser */
gchar *wrapped = g_strdup_printf("<html><body>%s</body></html>", text);
htmlDocPtr doc = htmlReadMemory(wrapped, strlen(wrapped), NULL, "UTF-8",
				HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR);
g_free(wrapped);
if (!doc) {
	gtk_text_buffer_set_text(buffer, text, -1);
	e->is_changed = FALSE;
	return;
}

xmlNodePtr body = NULL;
xmlNodePtr root = xmlDocGetRootElement(doc);
for (xmlNodePtr n = root->children; n; n = n->next) {
	if (!g_ascii_strcasecmp((gchar *)n->name, "body")) {
		body = n;
		break;
	}
}

if (body)
	_parse_html_node(buffer, &iter, body->children, e);

	xmlFreeDoc(doc);
	e->is_changed = FALSE;
}

/* ============================================================
 * File I/O
 * ============================================================ */

static gboolean
_load_file(EDITOR *e, const gchar *filename)
{
	gchar *text = NULL;
	GError *error = NULL;
	gboolean imported_docx = g_str_has_suffix(filename, ".docx") ||
		g_str_has_suffix(filename, ".DOCX");

	if (imported_docx)
		text = study_docx_to_html(filename, &error);
	else if (g_str_has_suffix(filename, ".doc") || g_str_has_suffix(filename, ".DOC"))
		g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			"Los archivos .doc antiguos no son compatibles. Guárdalo como .docx e impórtalo.");
	else
		g_file_get_contents(!strncmp(filename, "file:", 5) ? filename + 5 : filename,
				    &text, NULL, &error);
	if (error) {
		XI_message(("_load_file error: %s", error->message));
		gchar *message = g_strdup_printf(
			_("No se pudo abrir el estudio: %s"), error->message);
		gui_generic_warning(message);
		g_free(message);
		g_error_free(error);
		g_free(text);
		return FALSE;
	}

	_load_text_into_buffer(e, text ? text : "");
	if (text)
		g_free(text);
	if (e->filename)
		g_free(e->filename);
	e->filename = g_strdup(filename);
	/* Never overwrite an uploaded Word source with StudyPad HTML. */
	if (imported_docx) {
		g_free(e->filename);
		e->filename = g_strdup(_("Untitled document"));
		if (e->save_suggestion)
			g_free(e->save_suggestion);
		gchar *base = g_path_get_basename(filename);
		gsize base_len = strlen(base);
		if (base_len >= 5)
			base[base_len - 5] = '\0';
		e->save_suggestion = g_strdup_printf("%s.study.html", base);
		g_free(base);
		e->is_changed = TRUE;
		editor_update_title(e, _("StudyPad — imported Word document"));
	} else {
		g_clear_pointer(&e->save_suggestion, g_free);
		editor_update_title(e, e->filename);
	}
	xml_set_value("Xiphos", "studypad", "lastfile", e->filename);
	g_free(settings.studypadfilename);
	settings.studypadfilename = xml_get_value("studypad", "lastfile");
	if (settings.fnconfigure)
		xml_save_settings_doc(settings.fnconfigure);
	return TRUE;
}

static gboolean
_save_file(EDITOR *e, gboolean force_dialog)
{
	gchar *text = _serialize_buffer(e);
	gchar *chosen = NULL;
	const gchar *filename = e->filename;
	GError *error = NULL;
	gboolean saved;
	gboolean choose_path = force_dialog || !filename ||
		(0 == g_strcmp0(_("Untitled document"), filename));

	if (choose_path) {

		GtkWidget *dialog =
		    gtk_file_chooser_dialog_new(_("Save as"),
						GTK_WINDOW(e->window),
						GTK_FILE_CHOOSER_ACTION_SAVE,
						"_Cancel", GTK_RESPONSE_CANCEL,
						"_OK", GTK_RESPONSE_OK,
						NULL);
		gui_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
						    settings.studypaddir);
		if (e->type == STUDYPAD_EDITOR) {
			GtkFileFilter *html_filter = gtk_file_filter_new();
			gtk_file_filter_set_name(html_filter, _("Estudios HTML (*.html)"));
			gtk_file_filter_add_pattern(html_filter, "*.html");
			gtk_file_filter_add_pattern(html_filter, "*.htm");
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), html_filter);
			gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), html_filter);
			g_object_unref(html_filter);
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
				e->save_suggestion ? e->save_suggestion : _("Estudio.html"));
		}

		if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
			chosen = gui_file_chooser_get_filename(
				GTK_FILE_CHOOSER(dialog));
		gui_widget_destroy(dialog);
		if (!chosen) {
			g_free(text);
			return FALSE;
		}
		filename = chosen;
	}

	if (e->type == STUDYPAD_EDITOR && chosen &&
	    strlen(chosen) >= 5 &&
	    g_ascii_strcasecmp(chosen + strlen(chosen) - 5, ".docx") == 0) {
		/* Keep the imported Word source intact; StudyPad saves its editable
		 * document as HTML rather than writing HTML bytes into a DOCX archive. */
		gchar *message = g_strdup(_("Guarda el estudio como HTML para conservar intacto el documento Word original."));
		gui_generic_warning(message);
		g_free(message);
		g_free(chosen);
		g_free(text);
		return FALSE;
	}
	if (e->type == STUDYPAD_EDITOR && g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
		GError *history_error = NULL;
		if (!study_history_snapshot(filename, &history_error)) {
			gchar *message = g_strdup_printf(
				_("No se pudo guardar una versión anterior: %s"),
				history_error ? history_error->message : _("error desconocido"));
			gui_generic_warning(message);
			g_free(message);
			g_clear_error(&history_error);
		}
	}
	GFile *gfile = g_file_parse_name(filename);
	saved = g_file_replace_contents(gfile, text, strlen(text), NULL, TRUE,
					G_FILE_CREATE_NONE, NULL, NULL, &error);
	g_object_unref(gfile);
	if (!saved) {
		gchar *message = g_strdup_printf(
			_("No se pudo guardar el estudio: %s"),
			error ? error->message : _("error desconocido"));
		gui_generic_warning(message);
		g_free(message);
		g_clear_error(&error);
		g_free(chosen);
		g_free(text);
		return FALSE;
	}

	if (chosen) {
		g_free(e->filename);
		e->filename = g_strdup(chosen);
		editor_update_title(e, e->filename);
		xml_set_value("Xiphos", "studypad", "lastfile", e->filename);
		g_free(settings.studypadfilename);
		settings.studypadfilename = xml_get_value("studypad", "lastfile");
		if (settings.fnconfigure)
			xml_save_settings_doc(settings.fnconfigure);
	}
	g_clear_pointer(&e->save_suggestion, g_free);
	e->is_changed = FALSE;
	study_recovery_clear(e);
	if (e->filename && *e->filename &&
	    g_strcmp0(e->filename, _("Untitled document")) != 0 && settings.gSwordDir) {
		StudyLibraryEntry entry = { 0 };
		if (study_library_get(settings.gSwordDir, e->filename, &entry, NULL)) {
			entry.modified = g_get_real_time();
			study_library_put(settings.gSwordDir, &entry, NULL);
			study_library_entry_clear(&entry);
		}
	}
	g_free(chosen);
	g_free(text);
	return TRUE;
}

static void
study_organize_dialog(EDITOR *e)
{
	if (!e->filename || !*e->filename || !g_strcmp0(e->filename, _("Untitled document"))) {
		gui_generic_warning(_("Guarda el estudio primero para poder organizarlo."));
		return;
	}
	StudyLibraryEntry entry = { 0 };
	study_library_get(settings.gSwordDir, e->filename, &entry, NULL);
	GtkWidget *dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Organizar estudio"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(e->window));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 460, -1);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_widget_set_margin_top(box, 16); gtk_widget_set_margin_bottom(box, 12);
	gtk_widget_set_margin_start(box, 20); gtk_widget_set_margin_end(box, 20);
	GtkWidget *name = gtk_label_new(entry.title);
	gtk_label_set_xalign(GTK_LABEL(name), 0.0); gtk_widget_add_css_class(name, "studypad-save-title");
	gtk_box_append(GTK_BOX(box), name);
	GtkWidget *label = gtk_label_new(_("Carpeta (por ejemplo: Sermones/2026)"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0); gtk_box_append(GTK_BOX(box), label);
	GtkWidget *folder = gtk_entry_new(); gtk_editable_set_text(GTK_EDITABLE(folder), entry.folder); gtk_box_append(GTK_BOX(box), folder);
	label = gtk_label_new(_("Etiquetas (separadas por comas)"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0); gtk_box_append(GTK_BOX(box), label);
	GtkWidget *tags = gtk_entry_new(); gtk_editable_set_text(GTK_EDITABLE(tags), entry.tags); gtk_box_append(GTK_BOX(box), tags);
	GtkWidget *favorite = gtk_check_button_new_with_label(_("Marcar como favorito"));
	gtk_check_button_set_active(GTK_CHECK_BUTTON(favorite), entry.favorite); gtk_box_append(GTK_BOX(box), favorite);
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cancelar"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Guardar organización"), GTK_RESPONSE_OK);
	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		g_free(entry.folder); entry.folder = g_strdup(gtk_editable_get_text(GTK_EDITABLE(folder)));
		g_free(entry.tags); entry.tags = g_strdup(gtk_editable_get_text(GTK_EDITABLE(tags)));
		entry.favorite = gtk_check_button_get_active(GTK_CHECK_BUTTON(favorite));
		entry.modified = g_get_real_time();
		study_library_put(settings.gSwordDir, &entry, NULL);
	}
	gui_widget_destroy(dialog);
	study_library_entry_clear(&entry);
}

static void
study_library_row_activated(GtkListBox *list, GtkListBoxRow *row, EDITOR *e)
{
	const gchar *path = g_object_get_data(G_OBJECT(row), "study-path");
	if (e->is_changed && ask_about_saving(e) == GS_CANCEL)
		return;
	if (path && !_load_file(e, path))
		return;
	if (path)
		study_recovery_clear(e);
	gui_widget_destroy(GTK_WIDGET(gtk_widget_get_ancestor(GTK_WIDGET(list), GTK_TYPE_WINDOW)));
}

static void
study_library_dialog(EDITOR *e)
{
	GtkWidget *dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Biblioteca de estudios"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(e->window));
	gtk_window_set_default_size(GTK_WINDOW(dialog), 620, 460);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_widget_set_margin_top(box, 12); gtk_widget_set_margin_bottom(box, 8);
	gtk_widget_set_margin_start(box, 14); gtk_widget_set_margin_end(box, 14);
	GtkWidget *intro = gtk_label_new(_("Tus estudios, carpetas, etiquetas y favoritos."));
	gtk_label_set_xalign(GTK_LABEL(intro), 0.0); gtk_box_append(GTK_BOX(box), intro);
	GtkWidget *scroll = gtk_scrolled_window_new(); gtk_widget_set_vexpand(scroll, TRUE);
	GtkWidget *list = gtk_list_box_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list); gtk_box_append(GTK_BOX(box), scroll);
	GPtrArray *entries = study_library_list(settings.gSwordDir, NULL);
	for (guint i = 0; i < entries->len; ++i) {
		StudyLibraryEntry *entry = g_ptr_array_index(entries, i);
		GtkWidget *row = gtk_list_box_row_new(), *line = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gchar *title = g_strdup_printf("%s%s", entry->favorite ? "★ " : "", entry->title);
		gchar *meta = g_strdup_printf("%s%s%s", entry->folder, *entry->tags ? "  •  " : "", entry->tags);
		GtkWidget *primary = gtk_label_new(title), *secondary = gtk_label_new(meta);
		gtk_label_set_xalign(GTK_LABEL(primary), 0.0); gtk_label_set_xalign(GTK_LABEL(secondary), 0.0); gtk_widget_add_css_class(secondary, "dim-label");
		gtk_widget_set_margin_top(line, 8); gtk_widget_set_margin_bottom(line, 8); gtk_widget_set_margin_start(line, 10);
		gtk_box_append(GTK_BOX(line), primary); gtk_box_append(GTK_BOX(line), secondary); gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), line);
		g_object_set_data_full(G_OBJECT(row), "study-path", g_strdup(entry->path), g_free); gtk_list_box_append(GTK_LIST_BOX(list), row);
		g_free(title); g_free(meta);
	}
	g_ptr_array_unref(entries);
	g_signal_connect(list, "row-activated", G_CALLBACK(study_library_row_activated), e);
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cerrar"), GTK_RESPONSE_CLOSE);
	g_signal_connect_swapped(dialog, "response", G_CALLBACK(gui_widget_destroy), dialog);
	gtk_widget_show(dialog);
}

static void
_save_note(EDITOR *e)
{
	gchar *text = _serialize_buffer(e);
	main_save_note(e->module, e->key, text);
	e->is_changed = FALSE;
	g_free(text);
}

static void
_save_book(EDITOR *e)
{
	gchar *text = _serialize_buffer(e);

	main_treekey_save_book_text(e->module, e->key, text);
	e->is_changed = FALSE;
	g_free(text);
}

/* ============================================================
 * Cleanup
 * ============================================================ */

static void
do_exit(EDITOR *e)
{
	if (e->autosave_source) {
		g_source_remove(e->autosave_source);
		e->autosave_source = 0;
	}
	study_recovery_clear(e);
	if (e->filename)
		g_free(e->filename);
	g_free(e->save_suggestion);
	if (e->module)
		g_free(e->module);
	if (e->key)
		g_free(e->key);
	if (e->window && !e->embedded)
		gui_widget_destroy(e->window);
	g_free(e);
}

/* ============================================================
 * Ask about saving dialog
 * ============================================================ */

/* The legacy generic alert puts its secondary message in an expanding
 * scroller.  That is useful for long diagnostics but makes a two-line save
 * confirmation grow to the height of the editor.  A StudyPad close prompt is
 * deliberately small, owned by its document window, and never scrolls. */
static gint
study_save_confirmation(EDITOR *e)
{
	GtkWidget *dialog = gtk_dialog_new();
	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
	GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	const gchar *name = e->filename && *e->filename ? e->filename : _("Untitled document");
	gchar *escaped = g_markup_escape_text(name, -1);
	gchar *heading = g_strdup(_("<b>Guardar los cambios antes de cerrar?</b>"));
	gchar *detail = g_strdup_printf(_("El estudio <b>%s</b> tiene cambios sin guardar. "
					     "Si cierras ahora, se perderán."), escaped);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Guardar cambios"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(e->window));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 500, -1);
	gtk_widget_add_css_class(dialog, "studypad-save-dialog");
	gtk_widget_set_margin_top(content, 20);
	gtk_widget_set_margin_bottom(content, 12);
	gtk_widget_set_margin_start(content, 20);
	gtk_widget_set_margin_end(content, 20);
	gtk_box_append(GTK_BOX(content), row);
	GtkWidget *icon = gtk_image_new_from_icon_name("dialog-warning-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
	gtk_widget_set_valign(icon, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(row), icon);
	gtk_widget_set_hexpand(copy, TRUE);
	gtk_box_append(GTK_BOX(row), copy);
	GtkWidget *title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(title), heading);
	gtk_label_set_xalign(GTK_LABEL(title), 0.0);
	gtk_widget_add_css_class(title, "studypad-save-title");
	gtk_box_append(GTK_BOX(copy), title);
	GtkWidget *message = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(message), detail);
	gtk_label_set_xalign(GTK_LABEL(message), 0.0);
	gtk_label_set_wrap(GTK_LABEL(message), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(message), PANGO_WRAP_WORD_CHAR);
	gtk_box_append(GTK_BOX(copy), message);

	GtkWidget *discard = gtk_dialog_add_button(GTK_DIALOG(dialog),
						      _("Cerrar sin guardar"), GTK_RESPONSE_NO);
	gtk_widget_add_css_class(discard, "destructive-action");
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cancelar"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Guardar"), GTK_RESPONSE_YES);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
	gint response = gui_dialog_run(GTK_DIALOG(dialog));
	gui_widget_destroy(dialog);
	g_free(detail);
	g_free(heading);
	g_free(escaped);
	return response == GTK_RESPONSE_YES ? GS_YES :
	       response == GTK_RESPONSE_NO ? GS_NO : GS_CANCEL;
}

gint
ask_about_saving(EDITOR *e)
{
	gint test;
	GS_DIALOG *info;
	gchar *buf = NULL;
	gchar *buf1 = NULL;
	gchar *buf2 = NULL;
	gchar *buf3 = NULL;
	gint retval = FALSE;

	switch (e->type) {
	case BOOK_EDITOR:
	case NOTE_EDITOR:
		info = gui_new_dialog();
		info->stock_icon = "dialog-warning";
		buf = g_strdup_printf("%s: %s", e->module, e->key);
		buf1 = _("Save the changes to document");
		buf2 = _("before closing?");
		buf3 = g_strdup_printf(
		    "<span weight=\"bold\" size=\"larger\">%s %s %s</span>",
		    buf1, buf, buf2);
		info->label_top = buf3;
		info->label2 = _("If you don't save, changes will be permanently lost.");
		info->save = TRUE;
		info->cancel = TRUE;
		info->no_save = TRUE;
		test = gui_alert_dialog(info);
		retval = test;
		if (test == GS_YES) {
			if (e->type == NOTE_EDITOR)
				_save_note(e);
			else
				_save_book(e);
		}
		g_free(info);
		g_free(buf);
		g_free(buf3);
		break;

	case STUDYPAD_EDITOR:
		retval = study_save_confirmation(e);
		test = retval;
		if (test == GS_YES && !_save_file(e, FALSE))
			retval = GS_CANCEL;
		break;
	}
	sync_windows();
	return retval;
}

/* ============================================================
 * Toolbar callbacks
 * ============================================================ */

G_MODULE_EXPORT void
action_bold_activate_cb(GtkWidget *widget, EDITOR *e)
{
    if (buttons_state.nochange)
        return;
    _toggle_tag(e, TAG_BOLD);
}

G_MODULE_EXPORT void
action_italic_activate_cb(GtkWidget *widget, EDITOR *e)
{
	if (buttons_state.nochange)
		return;
	_toggle_tag(e, TAG_ITALIC);
}

G_MODULE_EXPORT void
action_underline_activate_cb(GtkWidget *widget, EDITOR *e)
{
	if (buttons_state.nochange)
		return;
	_toggle_tag(e, TAG_UNDERLINE);
}

G_MODULE_EXPORT void
action_strikethrough_activate_cb(GtkWidget *widget, EDITOR *e)
{
	if (buttons_state.nochange)
		return;
	_toggle_tag(e, TAG_STRIKE);
}

G_MODULE_EXPORT void
action_undo_activate_cb(GtkWidget *widget, EDITOR *e)
{
	/* TODO: GtkTextBuffer does not have built-in undo before GTK 3.96.
	 * For GTK < 4, use a third-party undo manager or implement manually. */
	XI_message(("%s", "undo: not yet implemented"));
}

G_MODULE_EXPORT void
action_redo_activate_cb(GtkWidget *widget, EDITOR *e)
{
	XI_message(("%s", "redo: not yet implemented"));
}

G_MODULE_EXPORT void
action_cut_activate_cb(GtkWidget *widget, EDITOR *e)
{
	GdkClipboard *clipboard = gtk_widget_get_clipboard(e->text_widget);
	gtk_text_buffer_cut_clipboard(_get_buffer(e), clipboard, TRUE);
}

G_MODULE_EXPORT void
action_copy_activate_cb(GtkWidget *widget, EDITOR *e)
{
	GdkClipboard *clipboard = gtk_widget_get_clipboard(e->text_widget);
	gtk_text_buffer_copy_clipboard(_get_buffer(e), clipboard);
}

G_MODULE_EXPORT void
action_paste_activate_cb(GtkWidget *widget, EDITOR *e)
{
	GdkClipboard *clipboard = gtk_widget_get_clipboard(e->text_widget);
	gtk_text_buffer_paste_clipboard(_get_buffer(e), clipboard, NULL, TRUE);
}

G_MODULE_EXPORT void
action_delete_activate_cb(GtkWidget *widget, EDITOR *e)
{
	GtkTextBuffer *buffer = _get_buffer(e);
	GtkTextIter start, end;

	if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
		gtk_text_buffer_delete(buffer, &start, &end);
}

G_MODULE_EXPORT void
action_delete_item_activate_cb(GtkWidget *widget, EDITOR *e)
{
	if (e->studypad)
		return;

	gchar *buf = g_strdup_printf(
	    "<span weight=\"bold\" size=\"larger\">%s %s?</span>",
	    _("Are you sure you want to delete the note for"), e->key);

	if (gui_yes_no_dialog(buf, "dialog-warning")) {
		main_delete_note(e->module, e->key);
		_load_text_into_buffer(e, "");
	}
	g_free(buf);
	e->is_changed = FALSE;
}

G_MODULE_EXPORT void
action_new_activate_cb(GtkWidget *widget, EDITOR *e)
{
	if (e->is_changed && ask_about_saving(e) == GS_CANCEL)
		return;

	_load_text_into_buffer(e, "");
	g_clear_pointer(&e->save_suggestion, g_free);

	if (e->filename)
		g_free(e->filename);
	e->filename = g_strdup(_("Untitled document"));

	xml_set_value("Xiphos", "studypad", "lastfile", e->filename);
	settings.studypadfilename = xml_get_value("studypad", "lastfile");
	editor_update_title(e, e->filename);
	e->is_changed = TRUE;
}

G_MODULE_EXPORT void
action_open_activate_cb(GtkWidget *widget, EDITOR *e)
{
	if (e->is_changed && ask_about_saving(e) == GS_CANCEL)
		return;
	GtkWidget *dialog =
	    gtk_file_chooser_dialog_new(_("Open"), GTK_WINDOW(e->window),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					"_Cancel", GTK_RESPONSE_CANCEL,
					"_Open", GTK_RESPONSE_ACCEPT,
					NULL);
	gui_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
					    settings.studypaddir);
	GtkFileFilter *study_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(study_filter, _("Studies and Word documents"));
	gtk_file_filter_add_pattern(study_filter, "*.html");
	gtk_file_filter_add_pattern(study_filter, "*.htm");
	gtk_file_filter_add_pattern(study_filter, "*.txt");
	gtk_file_filter_add_pattern(study_filter, "*.docx");
	gtk_file_filter_add_pattern(study_filter, "*.DOCX");
	gtk_file_filter_add_pattern(study_filter, "*.doc");
	gtk_file_filter_add_pattern(study_filter, "*.DOC");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), study_filter);

	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename =
		    gui_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (_load_file(e, filename))
			study_recovery_clear(e);
		g_free(filename);
	}
	gui_widget_destroy(dialog);
}

G_MODULE_EXPORT void
action_import_word_activate_cb(GtkWidget *widget, EDITOR *e)
{
	GtkWidget *dialog;
	GtkFileFilter *filter;

	(void)widget;
	if (e->is_changed && ask_about_saving(e) == GS_CANCEL)
		return;
	dialog = gtk_file_chooser_dialog_new(
		_("Importar documento Word"), GTK_WINDOW(e->window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("Cancelar"), GTK_RESPONSE_CANCEL,
		_("Importar"), GTK_RESPONSE_ACCEPT,
		NULL);
	gui_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
					    settings.studypaddir);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Documentos Word (*.docx)"));
	gtk_file_filter_add_pattern(filter, "*.docx");
	gtk_file_filter_add_pattern(filter, "*.DOCX");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
	g_object_unref(filter);
	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gui_file_chooser_get_filename(
			GTK_FILE_CHOOSER(dialog));
		if (filename) {
			if (_load_file(e, filename))
				study_recovery_clear(e);
			g_free(filename);
		}
	}
	gui_widget_destroy(dialog);
}

static void
study_template_selected(GtkDropDown *choice, GParamSpec *pspec, GtkLabel *label)
{
	(void)pspec;
	const StudyTemplate *selected = study_template_get(gtk_drop_down_get_selected(choice));
	gtk_label_set_text(label, selected ? _(selected->description) : "");
}

static void
study_template_dialog(EDITOR *e)
{
	const gchar *names[study_template_count() + 1];
	for (guint i = 0; i < study_template_count(); i++)
		names[i] = _(study_template_get(i)->name);
	names[study_template_count()] = NULL;
	GtkWidget *dialog = gtk_dialog_new();
	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget *choice = gtk_drop_down_new_from_strings(names);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Plantillas de estudio"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(e->window));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_widget_set_margin_top(content, 16);
	gtk_widget_set_margin_bottom(content, 12);
	gtk_widget_set_margin_start(content, 18);
	gtk_widget_set_margin_end(content, 18);
	gtk_box_set_spacing(GTK_BOX(content), 12);
	GtkWidget *label = gtk_label_new(
		_("Elige una estructura para comenzar un nuevo estudio."));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_append(GTK_BOX(content), label);
	gtk_box_append(GTK_BOX(content), choice);
	GtkWidget *description = gtk_label_new(NULL);
	gtk_label_set_wrap(GTK_LABEL(description), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(description), 54);
	gtk_label_set_xalign(GTK_LABEL(description), 0.0);
	gtk_box_append(GTK_BOX(content), description);
	g_signal_connect(choice, "notify::selected", G_CALLBACK(study_template_selected), description);
	study_template_selected(GTK_DROP_DOWN(choice), NULL, GTK_LABEL(description));
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cancelar"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Usar plantilla"), GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(choice));
		if (selected >= 0 && (guint)selected < study_template_count() &&
		    (!e->is_changed || ask_about_saving(e) != GS_CANCEL)) {
			_load_text_into_buffer(e, study_template_get(selected)->html);
			GtkTextIter start;
			gtk_text_buffer_get_start_iter(_get_buffer(e), &start);
			gtk_text_buffer_place_cursor(_get_buffer(e), &start);
			gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(e->text_widget), &start,
				0.0, TRUE, 0.0, 0.0);
			study_recovery_clear(e);
			g_clear_pointer(&e->save_suggestion, g_free);
			g_free(e->filename);
			e->filename = g_strdup(_("Untitled document"));
			xml_set_value("Xiphos", "studypad", "lastfile", e->filename);
			g_free(settings.studypadfilename);
			settings.studypadfilename = xml_get_value("studypad", "lastfile");
			e->is_changed = TRUE;
			editor_update_title(e, names[selected]);
		}
	}
	gui_widget_destroy(dialog);
	gtk_widget_grab_focus(e->text_widget);
}

static void
study_history_dialog(EDITOR *e)
{
	if (!e->filename || !*e->filename ||
	    g_strcmp0(e->filename, _("Untitled document")) == 0) {
		gui_generic_warning(_("Guarda el estudio para empezar a crear su historial."));
		return;
	}
	GPtrArray *versions = study_history_list(e->filename);
	if (!versions->len) {
		g_ptr_array_unref(versions);
		gui_generic_warning(_("Este estudio todavía no tiene versiones anteriores."));
		return;
	}
	GtkWidget *dialog = gtk_dialog_new();
	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget *scroll = gtk_scrolled_window_new();
	GtkWidget *list = gtk_list_box_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Historial del estudio"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(e->window));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 360);
	gtk_widget_set_vexpand(scroll, TRUE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
	gtk_box_append(GTK_BOX(content), scroll);
	for (guint i = 0; i < versions->len; i++) {
		const gchar *path = g_ptr_array_index(versions, i);
		GFile *file = g_file_new_for_path(path);
		GFileInfo *info = g_file_query_info(file,
			"time::modified", G_FILE_QUERY_INFO_NONE, NULL, NULL);
		guint64 modified = info ? g_file_info_get_attribute_uint64(
			info, G_FILE_ATTRIBUTE_TIME_MODIFIED) : 0;
		GDateTime *date = g_date_time_new_from_unix_local(modified);
		gchar *stamp = g_date_time_format(date, "%Y-%m-%d %H:%M:%S");
		GtkWidget *row = gtk_list_box_row_new();
		GtkWidget *label = gtk_label_new(stamp);
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_widget_set_margin_start(label, 10);
		gtk_widget_set_margin_top(label, 8);
		gtk_widget_set_margin_bottom(label, 8);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
		g_object_set_data_full(G_OBJECT(row), "history-path",
				       g_strdup(path), g_free);
		gtk_list_box_append(GTK_LIST_BOX(list), row);
		if (date) g_date_time_unref(date);
		g_free(stamp);
		g_clear_object(&info);
		g_object_unref(file);
	}
	gtk_list_box_select_row(GTK_LIST_BOX(list),
				gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), 0));
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cancelar"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Restaurar versión"), GTK_RESPONSE_ACCEPT);
	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list));
		const gchar *selected = row ? g_object_get_data(G_OBJECT(row),
									"history-path") : NULL;
		if (selected && (!e->is_changed || ask_about_saving(e) != GS_CANCEL)) {
			gchar *html = NULL;
			if (g_file_get_contents(selected, &html, NULL, NULL)) {
				_load_text_into_buffer(e, html);
				e->is_changed = TRUE;
				editor_update_title(e, _("Versión restaurada"));
				g_free(html);
			}
		}
	}
	gui_widget_destroy(dialog);
	g_ptr_array_unref(versions);
}

static void
study_export_document(EDITOR *e, gboolean as_pdf)
{
	GtkWidget *dialog = gtk_file_chooser_dialog_new(
		as_pdf ? _("Exportar estudio a PDF") : _("Exportar estudio a Word"),
		GTK_WINDOW(e->window), GTK_FILE_CHOOSER_ACTION_SAVE,
		_("Cancelar"), GTK_RESPONSE_CANCEL,
		_("Exportar"), GTK_RESPONSE_ACCEPT, NULL);
	GtkFileFilter *filter = gtk_file_filter_new();
	const gchar *extension = as_pdf ? ".pdf" : ".docx";
	gtk_file_filter_set_name(filter, as_pdf ? _("Documento PDF") : _("Documento Word (*.docx)"));
	gtk_file_filter_add_pattern(filter, as_pdf ? "*.pdf" : "*.docx");
	gtk_file_filter_add_pattern(filter, as_pdf ? "*.PDF" : "*.DOCX");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
	g_object_unref(filter);
	gui_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), settings.studypaddir);
	gchar *base = e->filename && g_strcmp0(e->filename, _("Untitled document")) != 0
		? g_path_get_basename(e->filename) : g_strdup(_("Estudio"));
	gchar *dot = strrchr(base, '.');
	if (dot) *dot = '\0';
	gchar *suggested = g_strconcat(base, extension, NULL);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), suggested);
	g_free(suggested);
	g_free(base);
	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *path = gui_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (path) {
			gchar *lower = g_ascii_strdown(path, -1);
			if (!g_str_has_suffix(lower, extension)) {
				gchar *extended = g_strconcat(path, extension, NULL);
				g_free(path);
				path = extended;
			}
			GError *error = NULL;
			gchar *html = _serialize_buffer(e);
			gboolean ok = as_pdf
				? study_document_export_pdf(html, path, &error)
				: study_document_export_docx(html, path, &error);
			if (!ok) {
				gchar *message = g_strdup_printf(
					_("No se pudo exportar el estudio: %s"),
					error ? error->message : _("error desconocido"));
				gui_generic_warning(message);
				g_free(message);
				g_clear_error(&error);
			}
			g_free(html);
			g_free(lower);
			g_free(path);
		}
	}
	gui_widget_destroy(dialog);
}

static void
study_insert_current_citation(EDITOR *e)
{
	if (!settings.MainWindowModule || !settings.currentverse ||
	    !*settings.currentverse) {
		gui_generic_warning(_("No hay un pasaje bíblico activo para citar."));
		return;
	}
	gchar *module = (gchar *)main_url_encode(settings.MainWindowModule);
	gchar *reference = (gchar *)main_url_encode(settings.currentverse);
	gchar *uri = g_strdup_printf(
		"passagestudy.jsp?action=showRef&type=scripRef&module=%s&value=%s",
		module ? module : "", reference ? reference : "");
	gchar *tag_name = g_strdup_printf("sword_link_%s", uri);
	GtkTextBuffer *buffer = _get_buffer(e);
	GtkTextIter start, end;
	if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
		gtk_text_buffer_delete(buffer, &start, &end);
	gtk_text_buffer_get_iter_at_mark(buffer, &start,
					 gtk_text_buffer_get_insert(buffer));
	GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
	GtkTextTag *tag = gtk_text_tag_table_lookup(table, tag_name);
	if (!tag) {
		tag = gtk_text_buffer_create_tag(buffer, tag_name,
						"foreground", "#285a86",
						"underline", PANGO_UNDERLINE_SINGLE, NULL);
		g_object_set_data_full(G_OBJECT(tag), "uri", g_strdup(uri), g_free);
	}
	gtk_text_buffer_insert_with_tags(buffer, &start, settings.currentverse, -1,
					 tag, NULL);
	g_free(tag_name);
	g_free(uri);
	g_free(reference);
	g_free(module);
}

static void
on_template_clicked(GtkButton *button, EDITOR *e)
{
	(void)button;
	study_template_dialog(e);
}

static void
on_history_clicked(GtkButton *button, EDITOR *e)
{
	(void)button;
	study_history_dialog(e);
}

static void
on_export_docx_clicked(GtkButton *button, EDITOR *e)
{
	(void)button;
	study_export_document(e, FALSE);
}

static void
on_export_pdf_clicked(GtkButton *button, EDITOR *e)
{
	(void)button;
	study_export_document(e, TRUE);
}

static void
on_insert_citation_clicked(GtkButton *button, EDITOR *e)
{
	(void)button;
	study_insert_current_citation(e);
}

G_MODULE_EXPORT void
action_save_activate_cb(GtkWidget *widget, EDITOR *e)
{
	switch (e->type) {
	case STUDYPAD_EDITOR:
		_save_file(e, FALSE);
		break;
	case NOTE_EDITOR:
		_save_note(e);
		break;
	case BOOK_EDITOR:
		_save_book(e);
		break;
	default:
		XI_message(("\naction_save_cb oops!\n"));
		break;
	}
}

G_MODULE_EXPORT void
action_save_as_activate_cb(GtkWidget *widget, EDITOR *e)
{
	_save_file(e, TRUE);
}

G_MODULE_EXPORT void
action_quit_activate_cb(GtkWidget *widget, EDITOR *e)
{
	delete_event(NULL, NULL, e);
}

G_MODULE_EXPORT void
action_print_cb(GtkWidget *widget, EDITOR *e)
{
	/* TODO: implement printing via GtkPrintOperation */
	XI_message(("%s", "print: not yet implemented"));
}

G_MODULE_EXPORT void
action_insert_sword_link_activate_cb(GtkWidget *widget, gpointer data)
{
	editor_link_dialog((EDITOR *)data);
}

G_MODULE_EXPORT void
action_insert_link_activate_cb(GtkWidget *widget, EDITOR *e)
{
	editor_link_dialog(e);
}

G_MODULE_EXPORT void
colorbutton1_color_set_cb(GtkWidget *widget, GParamSpec *pspec, EDITOR *e)
{
	GtkTextBuffer *buffer = _get_buffer(e);
	GtkTextIter start, end;

	if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
		return;

	GdkRGBA color;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &color);
	gchar *color_str = gdk_rgba_to_string(&color);

	/* créer un tag unique pour cette couleur */
	gchar *tag_name = g_strdup_printf("fg_%s", color_str);
	GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
	if (!gtk_text_tag_table_lookup(table, tag_name))
		gtk_text_buffer_create_tag(buffer, tag_name,
		                           "foreground", color_str, NULL);
	gtk_text_buffer_apply_tag_by_name(buffer, tag_name, &start, &end);

	g_free(color_str);
	g_free(tag_name);
	e->is_changed = TRUE;
}

G_MODULE_EXPORT void
colorbutton_highlight_color_set_cb(GtkWidget *widget, GParamSpec *pspec,
				   EDITOR *e)
{
    GtkTextBuffer *buffer = _get_buffer(e);
    GtkTextIter start, end;

    if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
        return;

    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &color);
    gchar *color_str = gdk_rgba_to_string(&color);

    gchar *tag_name = g_strdup_printf("bg_%s", color_str);
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    if (!gtk_text_tag_table_lookup(table, tag_name))
        gtk_text_buffer_create_tag(buffer, tag_name,
                                   "background", color_str, NULL);
    gtk_text_buffer_apply_tag_by_name(buffer, tag_name, &start, &end);

    g_free(color_str);
    g_free(tag_name);
    e->is_changed = TRUE;
}

G_MODULE_EXPORT void
combo_box_changed_cb(GObject *object, GParamSpec *pspec, EDITOR *e)
{
	if (buttons_state.nochange)
		return;

	GtkTextBuffer *buffer = _get_buffer(e);
	GtkTextIter start, end;

if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		/* no selection - use current line */
		gtk_text_buffer_get_iter_at_mark(buffer, &start,
						 gtk_text_buffer_get_insert(buffer));
		end = start;
		gtk_text_iter_set_line_offset(&start, 0);
		gtk_text_iter_forward_to_line_end(&end);
	}
	gint choice = (gint)gtk_drop_down_get_selected(GTK_DROP_DOWN(object));

	/* detect previous list tag BEFORE removing tags */
	const gchar *prev_tags[] = {
		"list-bullet", "list-roman", "list-numbered", "list-alpha", NULL
	};
	const gchar *prefixes[] = { "\u2022 ", "i. ", "1. ", "a. " };
	gint prev_choice = -1;
	for (gint i = 0; prev_tags[i]; i++) {
		if (gtk_text_iter_has_tag(&start,
			gtk_text_tag_table_lookup(
				gtk_text_buffer_get_tag_table(buffer),
				prev_tags[i]))) {
			prev_choice = i;
			break;
		}
	}

	/* remove all existing style tags */
	const gchar *style_tags[] = {
		"h1", "h2", "h3", "h4", "h5", "h6",
		"address", "pre",
		"list-bullet", "list-roman", "list-numbered", "list-alpha", NULL
	};
	for (gint i = 0; style_tags[i]; i++)
		gtk_text_buffer_remove_tag_by_name(buffer, style_tags[i], &start, &end);

	/* if switching back to Normal, remove list prefix */
	if (prev_choice >= 0 && choice == 0) {
		GtkTextIter line_start = start;
		gtk_text_iter_set_line_offset(&line_start, 0);
		GtkTextIter prefix_end = line_start;
		const gchar *prefix = prefixes[prev_choice];
		gtk_text_iter_forward_chars(&prefix_end, g_utf8_strlen(prefix, -1));
		gchar *line_text = gtk_text_buffer_get_text(buffer,
							    &line_start, &prefix_end, FALSE);
		if (!g_strcmp0(line_text, prefix))
			gtk_text_buffer_delete(buffer, &line_start, &prefix_end);
		g_free(line_text);
	}

	const gchar *tag = NULL;
	switch (choice) {
	case 1:  tag = "h1";           break;
	case 2:  tag = "h2";           break;
	case 3:  tag = "h3";           break;
	case 4:  tag = "h4";           break;
	case 5:  tag = "h5";           break;
	case 6:  tag = "h6";           break;
	case 7:  tag = "address";      break;
	case 8:  tag = "pre";          break;
	case 9:  tag = "list-bullet";  break;
	case 10: /* list-roman - not yet implemented */   break;
	case 11: /* list-numbered - not yet implemented */break;
	case 12: /* list-alpha - not yet implemented */   break;
	default: break;
	}

	if (tag) {
		gtk_text_buffer_apply_tag_by_name(buffer, tag, &start, &end);

		/* insert list prefix at beginning of line */
		const gchar *prefix = NULL;
		if (!g_strcmp0(tag, "list-bullet"))    prefix = "\u2022 ";
		if (!g_strcmp0(tag, "list-roman"))     prefix = "i. ";
		if (!g_strcmp0(tag, "list-numbered"))  prefix = "1. ";
		if (!g_strcmp0(tag, "list-alpha"))     prefix = "a. ";
		if (prefix) {
			GtkTextIter line_start = start;
			gtk_text_iter_set_line_offset(&line_start, 0);
			gtk_text_buffer_insert(buffer, &line_start, prefix, -1);
		}
	}

	e->is_changed = TRUE;
}

G_MODULE_EXPORT void
find_replace_response_cb(GtkDialog *dialog, gint response_id, EDITOR *e)
{
	GtkTextBuffer *buffer = _get_buffer(e);
	GtkTextIter start, end, match_start, match_end;
	const gchar *needle;
	gboolean found;

	switch (response_id) {
	case GTK_RESPONSE_CANCEL:
		gtk_widget_hide(find_dialog.window);
		break;
	case 1: /* Find */
		needle = gtk_editable_get_text(GTK_EDITABLE(find_dialog.find_entry));
		gtk_text_buffer_get_start_iter(buffer, &start);
		found = gtk_text_iter_forward_search(&start, needle,
						     GTK_TEXT_SEARCH_CASE_INSENSITIVE,
						     &match_start, &match_end,
						     NULL);
		if (found) {
			gtk_text_buffer_select_range(buffer,
						     &match_start, &match_end);
			gtk_text_view_scroll_to_iter(
			    GTK_TEXT_VIEW(e->text_widget),
			    &match_start, 0.0, FALSE, 0.0, 0.0);
		}
		break;
	case 2: /* Replace */
		needle = gtk_editable_get_text(GTK_EDITABLE(find_dialog.find_entry));
		const gchar *replacement =
		    gtk_editable_get_text(GTK_EDITABLE(find_dialog.replace_entry));
		if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
			gtk_text_buffer_delete(buffer, &start, &end);
			gtk_text_buffer_insert(buffer, &start,
					       replacement, -1);
		}
		break;
	default:
		gtk_widget_hide(find_dialog.window);
		break;
	}
}

G_MODULE_EXPORT void
action_find_activate_cb(GtkWidget *widget, EDITOR *e)
{
	gtk_widget_show(find_dialog.window);
	gtk_widget_hide(find_dialog.box_replace);
	gtk_widget_hide(find_dialog.button_replace);
}

G_MODULE_EXPORT void
action_replace_activate_cb(GtkWidget *widget, EDITOR *e)
{
	gtk_widget_show(find_dialog.window);
	gtk_widget_show(find_dialog.box_replace);
	gtk_widget_show(find_dialog.button_replace);
}

void
set_button_state(BUTTONS_STATE state, EDITOR *e)
{
	gui_toggle_set_active(e->toolitems.bold, state.bold);
	gui_toggle_set_active(e->toolitems.italic, state.italic);
	gui_toggle_set_active(e->toolitems.underline, state.underline);
	gui_toggle_set_active(e->toolitems.strike, state.strike);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(e->toolitems.cb), state.style);
}

/* ============================================================
 * delete_event / window close
 * ============================================================ */

G_MODULE_EXPORT int
delete_event(GtkWidget *widget, gpointer event, EDITOR *e)
{
	if (e->is_changed) {
		switch (ask_about_saving(e)) {
		case GS_YES:
		case GS_NO:
			break;
		case GS_CANCEL:
			return TRUE;
		}
	}
	editors_all = g_list_remove(editors_all, e);
	do_exit(e);
	return FALSE;
}

/* ============================================================
 * Signals
 * ============================================================ */

static void
_on_buffer_changed(GtkTextBuffer *buffer, EDITOR *e)
{
	e->is_changed = TRUE;
	if (e->embedded && !e->autosave_source)
		e->autosave_source = g_timeout_add_seconds(2, study_autosave_cb, e);
}

/* ============================================================
 * Create on_key_press
 * ============================================================ */
 
/* Zoom changes the view's base font; document tags and exported text retain
 * their original formatting. The provider belongs to this text view. */
static void
study_editor_set_zoom(EDITOR *e, gint percent, gboolean persist)
{
	e->zoom_percent = CLAMP(percent, 50, 200);
	GtkCssProvider *provider = g_object_get_data(G_OBJECT(e->text_widget), "study-zoom-css");
	if (!provider) {
		provider = gtk_css_provider_new();
		gtk_style_context_add_provider(gtk_widget_get_style_context(e->text_widget),
			GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
		g_object_set_data_full(G_OBJECT(e->text_widget), "study-zoom-css", provider, g_object_unref);
	}
	/* CSS numbers always use a decimal point, independently of the UI locale. */
	char size[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_formatd(size, sizeof(size), "%.3f", 13.0 * e->zoom_percent / 100.0);
	gchar *css = g_strdup_printf("textview.studypad-document { font-size: %spt; }", size);
	gtk_css_provider_load_from_string(provider, css);
	g_free(css);
	if (e->zoom_label) {
		gchar *label = g_strdup_printf("%d %%", e->zoom_percent);
		gtk_button_set_label(GTK_BUTTON(e->zoom_label), label);
		g_free(label);
	}
	if (e->zoom_out) gtk_widget_set_sensitive(e->zoom_out, e->zoom_percent > 50);
	if (e->zoom_in) gtk_widget_set_sensitive(e->zoom_in, e->zoom_percent < 200);
	if (persist) {
		gchar *value = g_strdup_printf("%d", e->zoom_percent);
		xml_set_value("Xiphos", "studypad", "zoom", value);
		g_free(value);
	}
}

static void study_zoom_out(GtkButton *button, EDITOR *e)
{
	(void)button;
	study_editor_set_zoom(e, e->zoom_percent - 10, TRUE);
}

static void study_zoom_in(GtkButton *button, EDITOR *e)
{
	(void)button;
	study_editor_set_zoom(e, e->zoom_percent + 10, TRUE);
}

static void study_zoom_reset(GtkButton *button, EDITOR *e)
{
	(void)button;
	study_editor_set_zoom(e, 100, TRUE);
}

static gboolean study_zoom_scroll(GtkEventControllerScroll *controller,
				 gdouble dx, gdouble dy, EDITOR *e)
{
	(void)dx;
	if (!(gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(controller)) & GDK_CONTROL_MASK))
		return FALSE;
	if (dy != 0)
		study_editor_set_zoom(e, e->zoom_percent + (dy < 0 ? 10 : -10), TRUE);
	return TRUE;
}

static gboolean
_on_key_press(GtkWidget *widget, GuiKeyEvent *event, EDITOR *e)

{
	if (e->studypad && (event->state & GDK_CONTROL_MASK)) {
		switch (event->keyval) {
		case GDK_KEY_plus: case GDK_KEY_equal: case GDK_KEY_KP_Add:
			study_zoom_in(NULL, e); return TRUE;
		case GDK_KEY_minus: case GDK_KEY_KP_Subtract:
			study_zoom_out(NULL, e); return TRUE;
		case GDK_KEY_0: case GDK_KEY_KP_0:
			study_zoom_reset(NULL, e); return TRUE;
		}
	}
	if ((event->state & GDK_CONTROL_MASK) &&
	    (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S)) {
		action_save_activate_cb(NULL, e);
		return TRUE;
	}
	if (event->keyval != GDK_KEY_Return)
		return FALSE;

	GtkTextBuffer *buffer = _get_buffer(e);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_mark(buffer, &iter,
					 gtk_text_buffer_get_insert(buffer));

	GtkTextIter line_start = iter;
	gtk_text_iter_set_line_offset(&line_start, 0);
		
	const gchar *active_tag = NULL;
	GtkTextIter scan = line_start;
	GtkTextIter line_end = line_start;
	gtk_text_iter_forward_to_line_end(&line_end);
	while (!gtk_text_iter_equal(&scan, &line_end)) {
		gtk_text_iter_forward_char(&scan);
		GtkTextTag *tag = gtk_text_tag_table_lookup(
			gtk_text_buffer_get_tag_table(buffer), "list-bullet");
		if (gtk_text_iter_has_tag(&scan, tag)) {
			active_tag = "list-bullet";
			break;
		}
	}

	if (!active_tag){
		return FALSE;
	}

	/* count previous lines with same tag */
	gint count = 0;
	scan = line_start;
	while (gtk_text_iter_backward_line(&scan)) {
		GtkTextTag *tag = gtk_text_tag_table_lookup(
			gtk_text_buffer_get_tag_table(buffer), active_tag);
		if (gtk_text_iter_has_tag(&scan, tag))
			count++;
		else
			break;
	}

	/* build prefix */
	gchar *prefix = NULL;
	if (!g_strcmp0(active_tag, "list-bullet"))
		prefix = g_strdup("\u2022 ");

	if (prefix) {
		gchar *text = g_strdup_printf("\n%s", prefix);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
							 text, -1, active_tag, NULL);
		g_free(text);
		g_free(prefix);
		return TRUE;
	}

	return FALSE;
}

/* ============================================================
 * Create editor widget
 * ============================================================ */

static void
create_editor_window(GtkWidget *scrollwindow, EDITOR *e)
{
	GtkWidget *textview = gtk_text_view_new();
	GtkTextBuffer *buffer =
	    gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	e->text_widget = textview;
	e->html_widget = textview;

	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 48);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 48);
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(textview), 30);
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(textview), 40);
	gtk_widget_add_css_class(textview, "studypad-document");
	gtk_widget_show(textview);

	_setup_text_tags(buffer);

	/* wire sword link click handler */
	/* track modifications */
	g_signal_connect(buffer, "changed",
			 G_CALLBACK(_on_buffer_changed), e);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollwindow), textview);
	gtk_widget_add_css_class(scrollwindow, "studypad-canvas");
	e->is_changed = FALSE;
	buttons_state.nochange = 0;
	GtkGesture *link_gesture = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(link_gesture),
				      GDK_BUTTON_PRIMARY);
	g_signal_connect(link_gesture, "pressed",
			 G_CALLBACK(text_view_link_pressed), e);
	gtk_widget_add_controller(textview, GTK_EVENT_CONTROLLER(link_gesture));
	gui_widget_on_key_phase(GTK_WIDGET(textview), GTK_PHASE_CAPTURE, (GuiKeyFunc)_on_key_press, NULL, e);
}

/* ============================================================
 * editor_new - build the editor window from .ui file
 * ============================================================ */

GtkWidget *
editor_new(const gchar *title, EDITOR *e)
{
	GtkWidget *window;
	GtkWidget *scrollwindow;
	GtkBuilder *builder;
	GError *error = NULL;
	gpointer item;
	GtkWidget *recent_item;

	buttons_state.nochange = 1;

	builder = elim_gtk_builder_new();

	if (!gtk_builder_add_from_resource(builder,
					    "/org/bibliaelim/ui/gtk_tvedit.gtkbuilder",
					    &error)) {
		g_warning("Couldn't load builder file: %s", error->message);
		g_error_free(error);
	}

	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	e->window = window;
	gtk_window_set_title(GTK_WINDOW(window), title);

	e->toolitems.bold = GTK_TOGGLE_BUTTON(
	    gtk_builder_get_object(builder, "toolbutton_bold"));
	e->toolitems.italic = GTK_TOGGLE_BUTTON(
	    gtk_builder_get_object(builder, "toolbutton_italic"));
	e->toolitems.underline = GTK_TOGGLE_BUTTON(
	    gtk_builder_get_object(builder, "toolbuttonunderline"));
	e->toolitems.strike = GTK_TOGGLE_BUTTON(
	    gtk_builder_get_object(builder, "toolbutton_strikethrough"));
	e->toolitems.open = GTK_BUTTON(
	    gtk_builder_get_object(builder, "toolbutton_open"));
	e->toolitems.newdoc = GTK_BUTTON(
	    gtk_builder_get_object(builder, "toolbutton_new"));
	e->toolitems.deletedoc = GTK_BUTTON(
	    gtk_builder_get_object(builder, "toolbutton_delete"));
	e->toolitems.color = GTK_WIDGET(
	    gtk_builder_get_object(builder, "colorbutton1"));
	e->toolitems.cb = GTK_WIDGET(
	    gtk_builder_get_object(builder, "comboboxtext1"));

	gtk_drop_down_set_selected(GTK_DROP_DOWN(e->toolitems.cb), 0);

	item = gtk_builder_get_object(builder, "menuitem_recent");

	switch (e->type) {
	case STUDYPAD_EDITOR:
		if (e->toolitems.deletedoc)
			gtk_widget_hide(GTK_WIDGET(e->toolitems.deletedoc));
		/* GTK4 has no GtkRecentChooserMenu.  Recent files will be
		 * exposed through a GMenu action in a later editor pass. */
		recent_item = NULL;
		break;
	case NOTE_EDITOR:
		if (e->toolitems.open)
			gtk_widget_hide(GTK_WIDGET(e->toolitems.open));
		if (e->toolitems.newdoc)
			gtk_widget_hide(GTK_WIDGET(e->toolitems.newdoc));
		gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(
			builder, "toolbutton_import_word")));
		if (item)
			gtk_widget_hide(GTK_WIDGET(item));
		break;
	case BOOK_EDITOR:
		if (e->toolitems.open)
			gtk_widget_hide(GTK_WIDGET(e->toolitems.open));
		if (e->toolitems.newdoc)
			gtk_widget_hide(GTK_WIDGET(e->toolitems.newdoc));
		gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(
			builder, "toolbutton_import_word")));
		if (item)
			gtk_widget_hide(GTK_WIDGET(item));
		break;
	}

	e->navbar_box = GTK_WIDGET(gtk_builder_get_object(builder, "box_navbar"));
	e->box = GTK_WIDGET(gtk_builder_get_object(builder, "vbox1"));

	scrollwindow = GTK_WIDGET(
	    gtk_builder_get_object(builder, "scrolledwindow1"));
	create_editor_window(scrollwindow, e);
	e->is_changed = FALSE;
	if (e->studypad) {
		e->zoom_label = GTK_WIDGET(gtk_builder_get_object(builder, "study_zoom_reset"));
		e->zoom_out = GTK_WIDGET(gtk_builder_get_object(builder, "study_zoom_out"));
		e->zoom_in = GTK_WIDGET(gtk_builder_get_object(builder, "study_zoom_in"));
		g_signal_connect(e->zoom_out, "clicked", G_CALLBACK(study_zoom_out), e);
		g_signal_connect(e->zoom_in, "clicked", G_CALLBACK(study_zoom_in), e);
		g_signal_connect(e->zoom_label, "clicked", G_CALLBACK(study_zoom_reset), e);
		gchar *saved_zoom = xml_get_value("studypad", "zoom");
		gint percent = saved_zoom ? atoi(saved_zoom) : 100;
		study_editor_set_zoom(e, percent > 0 ? percent : 100, FALSE);
		g_free(saved_zoom);
		GtkEventController *scroll = gtk_event_controller_scroll_new(
			GTK_EVENT_CONTROLLER_SCROLL_VERTICAL | GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
		gtk_event_controller_set_propagation_phase(scroll, GTK_PHASE_CAPTURE);
		g_signal_connect(scroll, "scroll", G_CALLBACK(study_zoom_scroll), e);
		gtk_widget_add_controller(e->text_widget, scroll);
	} else {
		gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(builder, "study_zoom_group")), FALSE);
	}

	/* GTK4 removed gtk_builder_connect_signals(); connect the editor
	 * controls explicitly so Builder remains declarative and type-safe. */
	if (!e->embedded)
		g_signal_connect(e->window, "close-request",
				 G_CALLBACK(editor_close_request_cb), e);
	if (e->type != STUDYPAD_EDITOR) {
		const gchar *study_controls[] = {
			"toolbutton_insert_citation", "toolbutton_template",
			"toolbutton_history", "toolbutton_export_docx",
			"toolbutton_export_pdf", NULL
		};
		for (guint i = 0; study_controls[i]; i++)
			gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(
				builder, study_controls[i])));
	}
	g_signal_connect(e->toolitems.bold, "toggled",
			 G_CALLBACK(action_bold_activate_cb), e);
	g_signal_connect(e->toolitems.italic, "toggled",
			 G_CALLBACK(action_italic_activate_cb), e);
	g_signal_connect(e->toolitems.underline, "toggled",
			 G_CALLBACK(action_underline_activate_cb), e);
	g_signal_connect(e->toolitems.strike, "toggled",
			 G_CALLBACK(action_strikethrough_activate_cb), e);
	g_signal_connect(e->toolitems.newdoc, "clicked",
			 G_CALLBACK(action_new_activate_cb), e);
	g_signal_connect(e->toolitems.open, "clicked",
			 G_CALLBACK(action_open_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_import_word"),
			 "clicked", G_CALLBACK(action_import_word_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_save"),
			 "clicked", G_CALLBACK(action_save_activate_cb), e);
	g_signal_connect_swapped(gtk_builder_get_object(builder, "toolbutton_library"),
			 "clicked", G_CALLBACK(study_library_dialog), e);
	g_signal_connect_swapped(gtk_builder_get_object(builder, "toolbutton_organize"),
			 "clicked", G_CALLBACK(study_organize_dialog), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_template"),
			 "clicked", G_CALLBACK(on_template_clicked), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_history"),
			 "clicked", G_CALLBACK(on_history_clicked), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_export_docx"),
			 "clicked", G_CALLBACK(on_export_docx_clicked), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_export_pdf"),
			 "clicked", G_CALLBACK(on_export_pdf_clicked), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_cut"),
			 "clicked", G_CALLBACK(action_cut_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_copy"),
			 "clicked", G_CALLBACK(action_copy_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_paste"),
			 "clicked", G_CALLBACK(action_paste_activate_cb), e);
	if (e->toolitems.deletedoc)
		g_signal_connect(e->toolitems.deletedoc, "clicked",
				 G_CALLBACK(action_delete_item_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_find"),
			 "clicked", G_CALLBACK(action_find_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_replace"),
			 "clicked", G_CALLBACK(action_replace_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_image"),
			 "clicked", G_CALLBACK(action_insert_image_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_sword_link"),
			 "clicked", G_CALLBACK(action_insert_sword_link_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_insert_citation"),
			 "clicked", G_CALLBACK(on_insert_citation_clicked), e);
	g_signal_connect(e->toolitems.color, "notify::rgba",
			 G_CALLBACK(colorbutton1_color_set_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "colorbutton_highlight"),
			 "notify::rgba", G_CALLBACK(colorbutton_highlight_color_set_cb), e);
	g_signal_connect(e->toolitems.cb, "notify::selected",
			 G_CALLBACK(combo_box_changed_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_align_left"),
			 "clicked", G_CALLBACK(action_justify_left_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_align_center"),
			 "clicked", G_CALLBACK(action_justify_center_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_align_right"),
			 "clicked", G_CALLBACK(action_justify_right_activate_cb), e);
	g_signal_connect(gtk_builder_get_object(builder, "toolbutton_align_justify"),
			 "clicked", G_CALLBACK(action_justify_full_activate_cb), e);

	find_dialog.window = GTK_WIDGET(
	    gtk_builder_get_object(builder, "dialog_find_replace"));
	find_dialog.find_entry = GTK_WIDGET(
	    gtk_builder_get_object(builder, "entry1"));
	find_dialog.replace_entry = GTK_WIDGET(
	    gtk_builder_get_object(builder, "entry2"));
	find_dialog.box_replace = GTK_WIDGET(
	    gtk_builder_get_object(builder, "box4"));
	find_dialog.button_replace = GTK_WIDGET(
	    gtk_builder_get_object(builder, "button_replace"));

	g_object_unref(builder);
	return window;
}

GtkWidget *
editor_create_embedded_studypad(GtkWindow *parent, GtkWidget **title_label_out)
{
	EDITOR *editor = g_new0(EDITOR, 1);
	GtkWidget *window, *contents, *label;

	editor->studypad = TRUE;
	editor->type = STUDYPAD_EDITOR;
	editor->embedded = TRUE;
	editor->sync = FALSE;
	window = editor_new(_("StudyPad"), editor);
	contents = editor->box;
	/* Keep the builder's root alive while it is detached from its temporary
	 * window. The main window becomes the owner of all editor dialogs. */
	g_object_ref(contents);
	gtk_window_set_child(GTK_WINDOW(window), NULL);
	gtk_window_destroy(GTK_WINDOW(window));
	gtk_widget_show(contents);
	editor->window = GTK_WIDGET(parent);

	label = gtk_label_new(_("Estudio nuevo"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_widget_add_css_class(label, "study-editor-title");
	editor->title_label = label;
	if (title_label_out)
		*title_label_out = label;
	if (!editor->filename)
		editor->filename = g_strdup(_("Untitled document"));
	if (settings.studypadfilename &&
	    g_strcmp0(settings.studypadfilename, _("Untitled document")) != 0 &&
	    g_file_test(settings.studypadfilename, G_FILE_TEST_IS_REGULAR)) {
		gchar *last_file = g_strdup(settings.studypadfilename);
		_load_file(editor, last_file);
		g_free(last_file);
	} else {
		editor_update_title(editor, _("Estudio nuevo"));
	}
	editors_all = g_list_append(editors_all, editor);
	editor->is_changed = FALSE;
	study_recovery_offer(editor);
	return contents;
}

/* ============================================================
 * Public API
 * ============================================================ */

void
editor_load_note(EDITOR *e, const gchar *module_name, const gchar *key)
{
	gchar *title = NULL, *text = NULL;

	if (e->is_changed)
		_save_note(e);

	if (module_name) {
		if (e->module)
			g_free(e->module);
		e->module = g_strdup(module_name);
	}
	if (key) {
		if (e->key)
			g_free(e->key);
		e->key = g_strdup(key);
	}

	text = main_get_raw_text((gchar *)e->module, (gchar *)e->key);
	_load_text_into_buffer(e, text ? text : "");
	if (text)
		g_free(text);

	if (e->type == NOTE_EDITOR) {
		e->navbar.valid_key = TRUE;
		main_navbar_versekey_set(e->navbar, e->key);
	}

	title = g_strdup_printf("%s - %s", e->module, e->key);
	editor_update_title(e, title);
	g_free(title);
}

void
editor_load_book(EDITOR *e)
{
	gchar *title = NULL, *text = NULL;

	if (!g_ascii_isdigit(e->key[0]))
		return;

	if (atol(e->key) != 0)
		text = main_get_book_raw_text(e->module, e->key);
	else
		text = g_strdup(e->module);

	_load_text_into_buffer(e, text ? text : "");
	if (text)
		g_free(text);

	title = g_strdup_printf("%s", e->module);
	editor_update_title(e, title);
	g_free(title);
	e->is_changed = FALSE;
}

void
editor_save_book(EDITOR *e)
{
	if (editor_is_dirty(e))
		_save_book(e);
}

void
editor_sync_with_main(void)
{
	GList *tmp = g_list_first(editors_all);

	while (tmp != NULL) {
		EDITOR *e = (EDITOR *)tmp->data;
		if (e->type == NOTE_EDITOR && e->sync)
			editor_load_note(e, NULL, settings.currentverse);
		tmp = g_list_next(tmp);
	}
}

gboolean
editor_maybe_save_all(void)
{
	GHashTable *choices = g_hash_table_new(g_direct_hash, g_direct_equal);
	GList *tmp;
	for (tmp = g_list_first(editors_all); tmp; tmp = g_list_next(tmp)) {
		EDITOR *e = tmp->data;
		gint response;
		if (!e->is_changed)
			continue;
		response = ask_about_saving(e);
		if (response == GS_CANCEL) {
			g_hash_table_destroy(choices);
			return FALSE;
		}
		g_hash_table_insert(choices, e, GINT_TO_POINTER(response + 1));
	}

	while (editors_all) {
		EDITOR *e = editors_all->data;
		gpointer stored = g_hash_table_lookup(choices, e);
		if (stored && GPOINTER_TO_INT(stored) - 1 == GS_NO)
			e->is_changed = FALSE;
		delete_event(NULL, NULL, e);
	}
	g_hash_table_destroy(choices);
	return TRUE;
}

gboolean
editor_close_embedded_studypad(GtkWidget *contents)
{
	GList *node;

	if (!contents)
		return FALSE;
	for (node = editors_all; node; node = node->next) {
		EDITOR *e = node->data;
		GtkWidget *parent;
		GtkWidget *root;

		if (!e->embedded || !e->studypad || e->box != contents)
			continue;
		if (e->is_changed && ask_about_saving(e) == GS_CANCEL)
			return FALSE;

		root = e->box;
		parent = gtk_widget_get_parent(root);
		if (parent && !GTK_IS_BOX(parent))
			return FALSE;
		g_object_ref(root);
		if (parent)
			gtk_box_remove(GTK_BOX(parent), root);
		editors_all = g_list_delete_link(editors_all, node);
		do_exit(e);
		g_object_unref(root);
		return TRUE;
	}
	return FALSE;
}

static gint
_create_new(const gchar *filename, const gchar *key, gint editor_type)
{
	EDITOR *editor;
	GtkWidget *toolbar_nav = NULL;

	editor = g_new0(EDITOR, 1);
	editor->text_widget = NULL;
	editor->save_suggestion = NULL;
	editor->title_label = NULL;
	editor->embedded = FALSE;
	editor->sync = FALSE;
	editor->type = editor_type;

	switch (editor_type) {
	case STUDYPAD_EDITOR:
		editor->studypad = TRUE;
		editor->bookeditor = FALSE;
		editor->noteeditor = FALSE;
		editor->module = NULL;
		editor->key = NULL;
		editor->filename = NULL;
		widgets.studypad_dialog = editor_new(_("StudyPad"), editor);
		/* A newly created StudyPad has no caller window to present it later.
		 * Without this, the first menu activation built it invisibly and only
		 * the second activation exposed it. */
		gtk_widget_show(editor->window);
		gtk_window_present(GTK_WINDOW(editor->window));
		if (filename) {
			_load_file(editor, filename);
		}
		break;

	case NOTE_EDITOR:
		editor->noteeditor = TRUE;
		editor->bookeditor = FALSE;
		editor->studypad = FALSE;
		editor->filename = NULL;
		editor->module = g_strdup(filename);
		editor->key = g_strdup(key);
		editor->navbar.key = NULL;
		editor_new(_("Note Editor"), editor);

		toolbar_nav = gui_navbar_versekey_editor_new(editor);
		gtk_widget_show(toolbar_nav);
		gtk_box_append(GTK_BOX(editor->navbar_box), GTK_WIDGET(toolbar_nav));

		editor_load_note(editor, NULL, NULL);
		break;

	case BOOK_EDITOR:
		editor->bookeditor = TRUE;
		editor->noteeditor = FALSE;
		editor->studypad = FALSE;
		editor->filename = NULL;
		editor->module = g_strdup(filename);
		editor->key = g_strdup(key);
		editor_new(_("Prayer List/Journal Editor"), editor);

		GtkWidget *box;
		UI_VBOX(box, TRUE, 0);
		gtk_widget_show(box);
		GtkWidget *hpaned1 = UI_HPANE();
		gtk_widget_show(hpaned1);
		gtk_paned_set_end_child(GTK_PANED(hpaned1), box);
	gtk_paned_set_resize_end_child(GTK_PANED(hpaned1), TRUE);
	gtk_paned_set_shrink_end_child(GTK_PANED(hpaned1), TRUE);

		GtkWidget *scrollbar = gtk_scrolled_window_new();
		gtk_widget_show(scrollbar);
		gtk_paned_set_start_child(GTK_PANED(hpaned1), GTK_WIDGET(scrollbar));
	gtk_paned_set_resize_start_child(GTK_PANED(hpaned1), TRUE);
	gtk_paned_set_shrink_start_child(GTK_PANED(hpaned1), TRUE);
		gtk_scrolled_window_set_policy(
		    GTK_SCROLLED_WINDOW(scrollbar),
		    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW((GtkScrolledWindow *)scrollbar), TRUE);

		editor->treeview = gui_create_editor_tree(editor);
		gtk_widget_show(editor->treeview);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollbar), editor->treeview);
		gtk_paned_set_position(GTK_PANED(hpaned1), 125);
		elim_tree_expand_all(editor->treeview);

		gtk_widget_unparent(editor->box);
		gtk_box_append(GTK_BOX(box), editor->box);
		gtk_window_set_child(GTK_WINDOW(editor->window), hpaned1);

		editor_load_book(editor);
		break;
	}

	editor->is_changed = FALSE;
	editors_all = g_list_append(editors_all, (EDITOR *)editor);
	return 1;
}

gint
editor_create_new(const gchar *filename, const gchar *key,
		  gint editor_type)
{
	GList *tmp = g_list_first(editors_all);

	while (tmp != NULL) {
		EDITOR *e = (EDITOR *)tmp->data;
		switch (editor_type) {
		case STUDYPAD_EDITOR:
			if (e->studypad) {
				if (filename) {
					if (editor_is_dirty(e) &&
					    ask_about_saving(e) == GS_CANCEL)
						return 1;
					if (!_load_file(e, filename))
						return 1;
				}
				gtk_widget_show(e->window);
				gtk_window_present(GTK_WINDOW(e->window));
				return 1;
			}
			break;
		case NOTE_EDITOR:
			if (!e->noteeditor)
				break;
			if (editor_is_dirty(e))
				_save_note(e);
			if (e->module)
				g_free(e->module);
			e->module = g_strdup(filename);
			if (e->key)
				g_free(e->key);
			e->key = g_strdup(key);
			gtk_widget_show(e->window);
			gtk_window_present(GTK_WINDOW(e->window));
			editor_load_note(e, NULL, NULL);
			return 1;
		case BOOK_EDITOR:
			if (!e->bookeditor)
				break;
			if (editor_is_dirty(e))
				_save_book(e);
			if (e->module)
				g_free(e->module);
			e->module = g_strdup(filename);
			if (e->key)
				g_free(e->key);
			e->key = g_strdup(key);
			gtk_widget_show(e->window);
			gtk_window_present(GTK_WINDOW(e->window));
			main_load_book_tree_in_editor(e->treeview, e->module);
			editor_load_book(e);
			return 1;
		}
		tmp = g_list_next(tmp);
	}

	XI_message(("filename %s, key %s",
		    filename ? filename : "-null-",
		    key ? key : "-null-"));
	return _create_new(filename, key, editor_type);
}

void
editor_open_studypad(void)
{
	editor_create_new(NULL, NULL, STUDYPAD_EDITOR);
}

G_MODULE_EXPORT void
action_about_activate_cb(GtkWidget *widget, EDITOR *e) {}

G_MODULE_EXPORT void
action_increase_indent_activate_cb(GtkWidget *widget, EDITOR *e) {}

G_MODULE_EXPORT void
action_decrease_indent_activate_cb(GtkWidget *widget, EDITOR *e) {}

G_MODULE_EXPORT void
action_insert_rule_activate_cb(GtkWidget *widget, EDITOR *e) {}

G_MODULE_EXPORT void
action_insert_table_activate_cb(GtkWidget *widget, EDITOR *e) {}

G_MODULE_EXPORT void
action_insert_emoticon_activate_cb(GtkWidget *widget, EDITOR *e) {}

G_MODULE_EXPORT void
action_insert_image_activate_cb(GtkWidget *widget, EDITOR *e)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        _("Select an image file"), GTK_WINDOW(e->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_OK", GTK_RESPONSE_ACCEPT,
        NULL);

    if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename = gui_file_chooser_get_filename(
            GTK_FILE_CHOOSER(dialog));
        GtkTextBuffer *buffer = _get_buffer(e);
        GtkTextIter cursor;
        gtk_text_buffer_get_iter_at_mark(buffer, &cursor,
            gtk_text_buffer_get_insert(buffer));
		GError *error = NULL;
		GdkTexture *texture = gdk_texture_new_from_filename(filename, &error);
		if (texture) {
			g_object_set_data_full(G_OBJECT(texture), "image-path",
					       g_strdup(filename), g_free);
			gtk_text_buffer_insert_paintable(buffer, &cursor,
							GDK_PAINTABLE(texture));
			g_object_unref(texture);
			e->is_changed = TRUE;
		} else {
			g_warning("Could not load image '%s': %s", filename,
				  error ? error->message : "unknown error");
			g_clear_error(&error);
		}
        g_free(filename);
    }
    gui_widget_destroy(dialog);
}

G_MODULE_EXPORT void
action_insert_outline_activate_cb(GtkWidget *widget, EDITOR *e) {}

G_MODULE_EXPORT void
action_justify_left_activate_cb(GtkWidget *widget, EDITOR *e)
{
    gtk_text_view_set_justification(GTK_TEXT_VIEW(e->text_widget),
                                    GTK_JUSTIFY_LEFT);
}

G_MODULE_EXPORT void
action_justify_right_activate_cb(GtkWidget *widget, EDITOR *e)
{
    gtk_text_view_set_justification(GTK_TEXT_VIEW(e->text_widget),
                                    GTK_JUSTIFY_RIGHT);
}

G_MODULE_EXPORT void
action_justify_center_activate_cb(GtkWidget *widget, EDITOR *e)
{
    gtk_text_view_set_justification(GTK_TEXT_VIEW(e->text_widget),
                                    GTK_JUSTIFY_CENTER);
}

G_MODULE_EXPORT void
action_justify_full_activate_cb(GtkWidget *widget, EDITOR *e)
{
    gtk_text_view_set_justification(GTK_TEXT_VIEW(e->text_widget),
                                    GTK_JUSTIFY_FILL);
}

G_MODULE_EXPORT void
action_print_preview_cb(GtkWidget *widget, EDITOR *e) {}

G_MODULE_EXPORT void
action_font_activate_cb(GtkWidget *widget, EDITOR *e) {}

#endif /* USE_GTKTVeditor */
