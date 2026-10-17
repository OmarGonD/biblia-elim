/* Exercise the actual HTML loader, serializer, and view zoom. Unrelated
 * editor actions are discarded by the linker's section garbage collection. */
#include <config.h>
#include <gtk/gtk.h>
#include <gmodule.h>
#undef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#include "../src/editor/gtktextview_editor.c"

gchar *XI_g_strdup_printf(const char *filename, int line, const gchar *format, ...)
{
	(void)filename;
	(void)line;
	va_list arguments;
	va_start(arguments, format);
	gchar *text = g_strdup_vprintf(format, arguments);
	va_end(arguments);
	return text;
}

static gchar *saved_zoom;

static void settle_layout(void)
{
	gint64 until = g_get_monotonic_time() + 200 * G_TIME_SPAN_MILLISECOND;
	while (g_get_monotonic_time() < until) {
		while (g_main_context_iteration(NULL, FALSE)) {}
		g_usleep(1000);
	}
}

static gint view_font_size(EDITOR *e)
{
	return pango_font_description_get_size(pango_context_get_font_description(
		gtk_widget_get_pango_context(e->text_widget)));
}
void xml_set_value(const char *doc, const char *section,
		   const char *item, const char *value)
{
	g_assert_cmpstr(doc, ==, "Xiphos");
	g_assert_cmpstr(section, ==, "studypad");
	g_assert_cmpstr(item, ==, "zoom");
	g_free(saved_zoom);
	saved_zoom = g_strdup(value);
}

static gchar *plain_text(EDITOR *e)
{
	GtkTextIter start, end;
	gtk_text_buffer_get_bounds(_get_buffer(e), &start, &end);
	return gtk_text_buffer_get_text(_get_buffer(e), &start, &end, FALSE);
}

static gboolean has_tag_at(EDITOR *e, const gchar *needle, const gchar *tag_name)
{
	gchar *text = plain_text(e);
	const gchar *where = strstr(text, needle);
	g_assert_nonnull(where);
	GtkTextIter iter;
	gtk_text_buffer_get_iter_at_offset(_get_buffer(e), &iter,
		g_utf8_pointer_to_offset(text, where));
	GtkTextTag *tag = gtk_text_tag_table_lookup(
		gtk_text_buffer_get_tag_table(_get_buffer(e)), tag_name);
	gboolean result = gtk_text_iter_has_tag(&iter, tag);
	g_free(text);
	return result;
}

int main(int argc, char **argv)
{
	gtk_init();
	EDITOR e = {0};
	e.studypad = TRUE;
	e.text_widget = gtk_text_view_new();
	g_object_ref_sink(e.text_widget);
	e.zoom_label = gtk_button_new();
	e.zoom_out = gtk_button_new();
	e.zoom_in = gtk_button_new();
	g_object_ref_sink(e.zoom_label);
	g_object_ref_sink(e.zoom_out);
	g_object_ref_sink(e.zoom_in);
	gtk_widget_add_css_class(e.text_widget, "studypad-document");
	_setup_text_tags(_get_buffer(&e));
	_load_text_into_buffer(&e,
		"<h1>Título</h1><p>Texto <b>negrita <i>cursiva</i></b> normal</p>"
		"<h2>Sección</h2><p>Contenido</p><p></p><p>Final</p>");
	gchar *text = plain_text(&e);
	g_assert_cmpstr(text, ==, "Título\nTexto negrita cursiva normal\nSección\nContenido\n\nFinal\n");
	g_free(text);
	g_assert_true(has_tag_at(&e, "Título", "h1"));
	g_assert_false(has_tag_at(&e, "Texto", "h1"));
	g_assert_true(has_tag_at(&e, "cursiva", TAG_BOLD));
	g_assert_true(has_tag_at(&e, "cursiva", TAG_ITALIC));
	g_assert_false(has_tag_at(&e, " normal", TAG_BOLD));
	g_assert_false(has_tag_at(&e, "Contenido", "h2"));
	for (guint i = 0; i < study_template_count(); i++) {
		const StudyTemplate *entry = study_template_get(i);
		_load_text_into_buffer(&e, entry->html);
		gchar *before = plain_text(&e);
		g_assert_cmpint(gtk_text_buffer_get_line_count(_get_buffer(&e)), >, 10);
		g_assert_false(has_tag_at(&e, "[Completar]", "h1"));
		g_assert_false(has_tag_at(&e, "[Completar]", "h2"));
		for (int pass = 0; pass < 3; pass++) {
			gchar *html = _serialize_buffer(&e);
			_load_text_into_buffer(&e, html);
			gchar *after = plain_text(&e);
			g_assert_cmpstr(after, ==, before);
			g_free(after);
			g_free(html);
		}
		g_print("%s: separated blocks, ordinary prompts, 3 save/reopen cycles OK\n", entry->name);
		g_free(before);
	}
	GtkWindow *window = GTK_WINDOW(gtk_window_new());
	gtk_window_set_default_size(window, 900, 850);
	GtkWidget *scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), e.text_widget);
	gtk_window_set_child(window, scroll);
	gtk_window_present(window);
	study_editor_set_zoom(&e, 100, FALSE);
	settle_layout();
	gint normal_font_size = view_font_size(&e);
	gchar *before = _serialize_buffer(&e);
	e.is_changed = FALSE;
	study_editor_set_zoom(&e, 160, TRUE);
	settle_layout();
	g_assert_cmpint(view_font_size(&e), >, normal_font_size);
	g_assert_cmpint(e.zoom_percent, ==, 160);
	g_assert_cmpstr(saved_zoom, ==, "160");
	study_editor_set_zoom(&e, 1000, FALSE);
	g_assert_cmpint(e.zoom_percent, ==, 200);
	g_assert_false(gtk_widget_get_sensitive(e.zoom_in));
	study_editor_set_zoom(&e, -100, FALSE);
	g_assert_cmpint(e.zoom_percent, ==, 50);
	g_assert_false(gtk_widget_get_sensitive(e.zoom_out));
	study_zoom_reset(NULL, &e);
	g_assert_cmpint(e.zoom_percent, ==, 100);
	g_assert_cmpstr(saved_zoom, ==, "100");
	g_assert_cmpstr(gtk_button_get_label(GTK_BUTTON(e.zoom_label)), ==, "100 %");
	study_zoom_in(NULL, &e);
	g_assert_cmpint(e.zoom_percent, ==, 110);
	study_zoom_out(NULL, &e);
	g_assert_cmpint(e.zoom_percent, ==, 100);
	gchar *after = _serialize_buffer(&e);
	g_assert_cmpstr(before, ==, after);
	g_assert_false(e.is_changed);
	g_print("Zoom: limits, reset, persisted preference, unchanged document OK\n");
	if (argc > 1) {
		/* Optional visual evidence using a real template and application CSS. */
		GtkCssProvider *theme = gtk_css_provider_new();
		gtk_css_provider_load_from_path(theme, argv[1]);
		gtk_style_context_add_provider_for_display(gdk_display_get_default(),
			GTK_STYLE_PROVIDER(theme), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		_load_text_into_buffer(&e, study_template_get(1)->html);
		gtk_text_view_set_left_margin(GTK_TEXT_VIEW(e.text_widget), 40);
		gtk_text_view_set_right_margin(GTK_TEXT_VIEW(e.text_widget), 40);
		gtk_text_view_set_top_margin(GTK_TEXT_VIEW(e.text_widget), 24);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(e.text_widget), GTK_WRAP_WORD_CHAR);
		gtk_window_present(window);
		gint64 until = g_get_monotonic_time() + G_TIME_SPAN_SECOND;
		while (g_get_monotonic_time() < until) {
			while (g_main_context_iteration(NULL, FALSE)) {}
			g_usleep(1000);
		}
		if (argc > 2) {
			GdkPaintable *paintable = gtk_widget_paintable_new(GTK_WIDGET(window));
			GtkSnapshot *snapshot = gtk_snapshot_new();
			gdk_paintable_snapshot(paintable, GDK_SNAPSHOT(snapshot), 900, 850);
			GskRenderNode *node = gtk_snapshot_free_to_node(snapshot);
			g_assert_nonnull(node);
			GdkTexture *image = gsk_renderer_render_texture(
				gtk_native_get_renderer(GTK_NATIVE(window)), node, NULL);
			g_assert_true(gdk_texture_save_to_png(image, argv[2]));
			g_object_unref(image);
			gsk_render_node_unref(node);
			g_object_unref(paintable);
		}
		g_object_unref(theme);
	}
	gtk_window_destroy(window);
	g_free(before);
	g_free(after);
	g_free(saved_zoom);
	g_object_unref(e.zoom_label);
	g_object_unref(e.zoom_out);
	g_object_unref(e.zoom_in);
	g_object_unref(e.text_widget);
	return 0;
}
