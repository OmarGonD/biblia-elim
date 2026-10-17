#include <gtk/gtk.h>

/* Load the complete UI, then embed it using the same ownership transfer as
 * StudyPad. A partially parsed Builder tree must never count as a visible UI. */
static void settle_layout(void)
{
	gint64 until = g_get_monotonic_time() + 250 * G_TIME_SPAN_MILLISECOND;
	while (g_get_monotonic_time() < until) {
		while (g_main_context_iteration(NULL, FALSE)) {}
		g_usleep(1000);
	}
}

int main(int argc, char **argv)
{
	g_assert_cmpint(argc, >=, 2);
	gtk_init();
	GError *error = NULL;
	GtkBuilder *builder = gtk_builder_new();
	if (!gtk_builder_add_from_file(builder, argv[1], &error)) {
		g_printerr("StudyPad UI load failed: %s\n", error->message);
		g_clear_error(&error);
		g_object_unref(builder);
		return 1;
	}
	GtkWindow *temporary = GTK_WINDOW(gtk_builder_get_object(builder, "window"));
	GtkWidget *body = GTK_WIDGET(gtk_builder_get_object(builder, "vbox1"));
	GtkScrolledWindow *canvas = GTK_SCROLLED_WINDOW(
		gtk_builder_get_object(builder, "scrolledwindow1"));
	g_assert_nonnull(temporary);
	g_assert_nonnull(body);
	g_assert_nonnull(canvas);
	GtkWidget *text = gtk_text_view_new();
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)),
		"Estudio de ejemplo: el editor permanece visible junto a la Biblia.", -1);
	gtk_scrolled_window_set_child(canvas, text);
	g_object_ref(body);
	gtk_window_set_child(temporary, NULL);
	gtk_window_destroy(temporary);
	GtkWindow *host = GTK_WINDOW(gtk_window_new());
	gtk_window_set_child(host, body);
	g_object_unref(body);
	const int widths[] = {900, 1400, 2000};
	const char *controls[] = {
		"toolbutton_new", "toolbutton_import_word", "toolbutton_save",
		"toolbutton_insert_citation", "toolbutton_template", "toolbutton_history",
		"toolbutton_export_docx", "toolbutton_export_pdf", "toolbutton_bold",
		"toolbuttonunderline", "colorbutton1", "toolbutton_align_justify",
		"scrolledwindow1"
	};
	for (guint w = 0; w < G_N_ELEMENTS(widths); w++) {
		gtk_window_set_default_size(host, widths[w], 640);
		gtk_window_present(host);
		settle_layout();
		g_assert_cmpint(gtk_widget_get_width(GTK_WIDGET(host)), >=, widths[w]);
		for (guint i = 0; i < G_N_ELEMENTS(controls); i++) {
			GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, controls[i]));
			g_assert_nonnull(widget);
			g_assert_true(gtk_widget_get_mapped(widget));
			g_assert_cmpint(gtk_widget_get_width(widget), >, 0);
			g_assert_cmpint(gtk_widget_get_height(widget), >, 0);
		}
		g_assert_true(gtk_widget_get_mapped(text));
		g_print("StudyPad width %d: %u controls mapped; canvas %dx%d\n",
			widths[w], (guint)G_N_ELEMENTS(controls),
			gtk_widget_get_width(text), gtk_widget_get_height(text));
	}
	if (argc > 2) {
		GdkPaintable *paintable = gtk_widget_paintable_new(GTK_WIDGET(host));
		GtkSnapshot *snapshot = gtk_snapshot_new();
		gdk_paintable_snapshot(paintable, GDK_SNAPSHOT(snapshot),
			gtk_widget_get_width(GTK_WIDGET(host)), gtk_widget_get_height(GTK_WIDGET(host)));
		GskRenderNode *node = gtk_snapshot_free_to_node(snapshot);
		g_assert_nonnull(node);
		GdkTexture *image = gsk_renderer_render_texture(
			gtk_native_get_renderer(GTK_NATIVE(host)), node, NULL);
		g_assert_true(gdk_texture_save_to_png(image, argv[2]));
		g_object_unref(image);
		gsk_render_node_unref(node);
		g_object_unref(paintable);
	}
	gtk_window_destroy(host);
	gtk_window_destroy(GTK_WINDOW(gtk_builder_get_object(builder, "dialog_find_replace")));
	g_object_unref(builder);
	return 0;
}
