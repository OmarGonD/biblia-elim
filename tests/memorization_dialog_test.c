#include <config.h>
#include "../src/gtk/memorizacion.c"
#include <glib/gstdio.h>
#include <stdarg.h>

SETTINGS settings = {0};
WIDGETS widgets = {0};
static gboolean readable = TRUE;

/* The UI uses the actual Leitner scheduler, XML persistence and table. Only
 * app services and the text source are isolated; backend reads have their own
 * SQLite/SWORD integration regressions. */
GtkBuilder *elim_gtk_builder_new(void) { return gtk_builder_new(); }
void gui_prepare_floating_dialog(GtkWindow *window, GtkWindow *parent)
{
	if (parent) gtk_window_set_transient_for(window, parent);
}
void gui_set_statusbar(const gchar *message) { (void)message; }
void gui_generic_warning(const char *message) { g_error("%s", message); }
void gui_generic_warning_modal(const char *message) { g_error("%s", message); }
const char *main_get_module_description(const char *module)
{
	(void)module;
	return "Biblia SQLite de prueba";
}
gchar *main_texto_de(const char *key)
{
	return readable && key && !strcmp(key, "1Sam 6:3")
		? g_strdup("Si enviáis el arca del Dios de Israel, no la enviéis vacía.") : NULL;
}
gchar *XI_g_strdup_printf(const char *file, int line, const gchar *format, ...)
{
	(void)file; (void)line;
	va_list args; va_start(args, format);
	gchar *result = g_strdup_vprintf(format, args);
	va_end(args); return result;
}
void XI_g_string_printf(const char *file, int line, GString *string, const char *format, ...)
{
	(void)file; (void)line;
	va_list args; va_start(args, format);
	g_string_vprintf(string, format, args);
	va_end(args);
}

static void settle(void)
{
	gint64 end = g_get_monotonic_time() + 250 * G_TIME_SPAN_MILLISECOND;
	while (g_get_monotonic_time() < end) {
		while (g_main_context_iteration(NULL, FALSE)) {}
		g_usleep(1000);
	}
}

static void capture(const char *directory, const char *name)
{
	settle();
	GtkWidget *window = ui->dialog;
	graphene_rect_t footer;
	g_assert_true(gtk_widget_get_mapped(ui->btn_anadir));
	g_assert_cmpint(gtk_widget_get_width(ui->btn_anadir), >, 0);
	g_assert_cmpint(gtk_widget_get_height(ui->btn_anadir), >, 0);
	g_assert_true(gtk_widget_compute_bounds(ui->btn_anadir, window, &footer));
	g_assert_cmpfloat(footer.origin.y + footer.size.height, <=, gtk_widget_get_height(window));
	if (!directory) return;
	GdkPaintable *paintable = gtk_widget_paintable_new(window);
	GtkSnapshot *snapshot = gtk_snapshot_new();
	gdk_paintable_snapshot(paintable, GDK_SNAPSHOT(snapshot),
		gtk_widget_get_width(window), gtk_widget_get_height(window));
	GskRenderNode *node = gtk_snapshot_free_to_node(snapshot);
	g_assert_nonnull(node);
	GdkTexture *image = gsk_renderer_render_texture(
		gtk_native_get_renderer(GTK_NATIVE(window)), node, NULL);
	gchar *path = g_build_filename(directory, name, NULL);
	g_assert_true(gdk_texture_save_to_png(image, path));
	g_free(path);
	g_object_unref(image);
	gsk_render_node_unref(node);
	g_object_unref(paintable);
}

int main(int argc, char **argv)
{
	gtk_init();
	if (argc > 1) {
		GtkCssProvider *theme = gtk_css_provider_new();
		gtk_css_provider_load_from_path(theme, argv[1]);
		gtk_style_context_add_provider_for_display(gdk_display_get_default(),
			GTK_STYLE_PROVIDER(theme), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref(theme);
	}
	const char *captures = argc > 2 ? argv[2] : NULL;
	gchar *directory = g_dir_make_tmp("elim-memorization-XXXXXX", NULL);
	settings.fnconfigure = g_build_filename(directory, "settings.xml", NULL);
	settings.MainWindowModule = g_strdup("SQLiteTest");
	settings.currentverse = g_strdup("1Sam 6:3");
	g_assert_true(g_file_set_contents(settings.fnconfigure, "<Xiphos><memoria/></Xiphos>", -1, NULL));
	g_assert_true(xml_parse_settings_file(settings.fnconfigure));
	GtkWidget *parent = gtk_window_new();
	gui_memorizacion_dialog(GTK_WINDOW(parent));
	settle();
	g_assert_true(gtk_widget_get_visible(ui->box_vacio));
	g_assert_false(gtk_widget_get_visible(ui->box_repaso));
	g_assert_false(gtk_widget_get_visible(ui->box_coleccion));
	g_assert_true(gtk_widget_get_sensitive(ui->btn_anadir));
	g_assert_false(gtk_widget_get_sensitive(ui->btn_quitar));
	capture(captures, "memory-empty.png");
	g_signal_emit_by_name(ui->btn_anadir, "clicked");
	settle();
	g_assert_cmpint(gtk_widget_get_height(ui->dialog), >=, 600);
	g_assert_cmpint(main_memoria_cuantos(), ==, 1);
	g_assert_true(gtk_widget_get_visible(ui->box_repaso));
	g_assert_true(gtk_widget_get_visible(ui->box_coleccion));
	g_assert_false(gtk_widget_get_sensitive(ui->btn_anadir));
	g_assert_false(gtk_widget_get_sensitive(ui->btn_acierto));
	g_signal_emit_by_name(ui->btn_acierto, "clicked");
	g_assert_cmpint(main_memoria_pendientes(), ==, 1);
	g_signal_emit_by_name(ui->btn_ver, "clicked");
	g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(ui->lbl_texto)), "no la enviéis vacía"));
	g_assert_true(gtk_widget_get_sensitive(ui->btn_acierto));
	capture(captures, "memory-review.png");
	g_signal_emit_by_name(ui->btn_acierto, "clicked");
	g_assert_cmpint(main_memoria_pendientes(), ==, 0);
	g_assert_true(gtk_widget_get_visible(ui->box_vacio));
	g_assert_true(gtk_widget_get_visible(ui->box_coleccion));
	GList *verses = main_memoria_todos();
	g_assert_cmpint(((MEM_VERSO *)verses->data)->caja, ==, 2);
	g_assert_cmpint(((MEM_VERSO *)verses->data)->aciertos, ==, 1);
	main_memoria_libre(verses);
	capture(captures, "memory-completed.png");
	elim_table_select(ui->tree, 0, FALSE);
	g_assert_true(gtk_widget_get_sensitive(ui->btn_repasar));
	g_assert_true(gtk_widget_get_sensitive(ui->btn_quitar));
	g_signal_emit_by_name(ui->btn_repasar, "clicked");
	g_assert_false(ui->destapado);
	readable = FALSE;
	g_signal_emit_by_name(ui->btn_ver, "clicked");
	g_assert_false(gtk_widget_get_sensitive(ui->btn_acierto));
	g_assert_false(gtk_widget_get_sensitive(ui->btn_fallo));
	g_assert_true(gtk_widget_get_sensitive(ui->btn_ver));
	g_signal_emit_by_name(ui->btn_fallo, "clicked");
	verses = main_memoria_todos();
	g_assert_cmpint(((MEM_VERSO *)verses->data)->caja, ==, 2);
	main_memoria_libre(verses);
	readable = TRUE;
	g_signal_emit_by_name(ui->btn_ver, "clicked");
	g_signal_emit_by_name(ui->btn_fallo, "clicked");
	verses = main_memoria_todos();
	g_assert_cmpint(((MEM_VERSO *)verses->data)->caja, ==, 1);
	g_assert_cmpint(((MEM_VERSO *)verses->data)->fallos, ==, 1);
	main_memoria_libre(verses);
	elim_table_select(ui->tree, 0, FALSE);
	g_signal_emit_by_name(ui->btn_quitar, "clicked");
	g_assert_cmpint(main_memoria_cuantos(), ==, 0);
	g_assert_false(gtk_widget_get_visible(ui->box_coleccion));
	readable = FALSE;
	gui_memorizacion_anadir(settings.currentverse);
	g_assert_cmpint(main_memoria_cuantos(), ==, 0);
	g_assert_true(gtk_widget_get_visible(ui->lbl_aviso));
	readable = TRUE;
	gui_memorizacion_anadir(settings.currentverse);
	on_cerrar(NULL, NULL);
	g_assert_null(ui);
	xml_free_settings_doc();
	g_assert_true(xml_parse_settings_file(settings.fnconfigure));
	gui_memorizacion_dialog(GTK_WINDOW(parent));
	g_assert_cmpint(main_memoria_cuantos(), ==, 1);
	g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(ui->versos)), ==, 1);
	g_assert_cmpstr(settings.currentverse, ==, "1Sam 6:3");
	on_cerrar(NULL, NULL);
	gtk_window_destroy(GTK_WINDOW(parent));
	xml_free_settings_doc();
	g_remove(settings.fnconfigure);
	g_rmdir(directory);
	g_free(settings.fnconfigure);
	g_free(settings.MainWindowModule);
	g_free(settings.currentverse);
	g_free(directory);
	g_print("memorization: empty/add/reveal/success/failure/selection/remove/persistence OK\n");
	return 0;
}
