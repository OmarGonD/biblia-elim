#include "gtk/nube_canvas.h"
#include "main/nube_mayusculas.h"

static void anotar(const gchar *palabra, gboolean abre_frase, gpointer m)
{
	nube_mayusculas_anotar(m, palabra, abre_frase);
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);
	/* Spellings are learned from the counted text (CLOUD-CASE-101). */
	NubeMayusculas *learned = nube_mayusculas_nueva();
	nube_recorrer_palabras("y dijo Noemí a su suegra en el campo de Booz, en "
		"Moab y Bethlehem, hijos de Elimelec, Mahalón, Isaí y Fares; y "
		"habló Dios a Jesús en Jerusalén, y a Pedro, Juan, María, Simón "
		"y Abrahán", anotar, learned);
	const char *spellings[][2] = {
		{ "noemí", "Noemí" }, { "booz", "Booz" },
		{ "moab", "Moab" }, { "bethlehem", "Bethlehem" },
		{ "elimelec", "Elimelec" }, { "mahalón", "Mahalón" },
		{ "isaí", "Isaí" }, { "fares", "Fares" },
		{ "suegra", "suegra" }, { "campo", "campo" }
	};
	for (guint i = 0; i < G_N_ELEMENTS(spellings); ++i) {
		NUBE_PALABRA w = { 0 };
		w.palabra = (gchar *)spellings[i][0];
		w.etiqueta = nube_mayusculas_forma(learned, spellings[i][0]);
		gchar *label = cloud_label(&w);
		g_assert_cmpstr(label, ==, spellings[i][1]);
		g_free(label);
		g_free(w.etiqueta);
	}
	GtkWidget *window = gtk_offscreen_window_new();
	GtkWidget *canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(canvas, 1100, 650);
	gtk_container_add(GTK_CONTAINER(window), canvas);
	NUBE_CONTEO count = { 0 };
	NUBE_PALABRA words[80] = { 0 };
	const char *labels[] = { "dios", "jesús", "señor", "hombre", "hijo", "casa",
		"día", "padre", "diciendo", "pueblo", "reino", "discípulos", "juan",
		"jerusalén", "espíritu", "tierra", "maestro", "palabra", "pedro",
		"camino", "profetas", "maría", "parábola", "simón", "abrahán" };
	count.palabras = g_ptr_array_new();
	for (int i = 0; i < 80; ++i) {
		words[i].palabra = (gchar *)labels[i % G_N_ELEMENTS(labels)];
		words[i].etiqueta = nube_mayusculas_forma(learned, words[i].palabra);
		words[i].cuenta = 124 - i;
		g_ptr_array_add(count.palabras, &words[i]);
	}
	CloudLayout *cloud = cloud_build(canvas, &count, FALSE, "#faf8f3");
	g_assert_cmpuint(cloud->words->len, ==, 80);
	int vertical = 0;
	for (guint i = 0; i < cloud->words->len; ++i) {
		CloudWord *w = &g_array_index(cloud->words, CloudWord, i);
		vertical += w->vertical;
		for (guint j = 0; j < i; ++j)
			g_assert_false(cloud_overlaps(w, &g_array_index(cloud->words, CloudWord, j)));
		if (i) {
			const PangoFontDescription *before = pango_layout_get_font_description(
				g_array_index(cloud->words, CloudWord, i - 1).layout);
			const PangoFontDescription *now = pango_layout_get_font_description(w->layout);
			g_assert_cmpint(pango_font_description_get_size(before), >,
				pango_font_description_get_size(now));
		}
	}
	g_assert_cmpint(vertical, >, 0);
	g_assert_cmpstr(pango_layout_get_text(g_array_index(cloud->words, CloudWord, 0).layout), ==, "Dios");
	g_assert_cmpstr(pango_layout_get_text(g_array_index(cloud->words, CloudWord, 1).layout), ==, "Jesús");
	g_assert_cmpstr(pango_layout_get_text(g_array_index(cloud->words, CloudWord, 13).layout), ==, "Jerusalén");
	g_object_set_data_full(G_OBJECT(canvas), "cloud", cloud, cloud_free);
	g_signal_connect(canvas, "draw", G_CALLBACK(cloud_draw), NULL);
	gtk_widget_show_all(window);
	while (gtk_events_pending()) gtk_main_iteration();
	if (argc > 1) {
		GdkPixbuf *image = gtk_offscreen_window_get_pixbuf(GTK_OFFSCREEN_WINDOW(window));
		g_assert_nonnull(image);
		g_assert_true(gdk_pixbuf_save(image, argv[1], "png", NULL, NULL));
		g_object_unref(image);
	}
	/* Book-exclusive words stay in their own panel; shared frequencies have
	 * identical physical font sizes, even when the books have different maxima. */
	NUBE_PALABRA pair_words[] = {
		{ .palabra = "dios", .etiqueta = "Dios", .cuenta = 50, .cuenta_b = 50 },
		{ .palabra = "jesús", .etiqueta = "Jesús", .cuenta = 124, .cuenta_b = 0 },
		{ .palabra = "moisés", .etiqueta = "Moisés", .cuenta = 0, .cuenta_b = 295 }
	};
	NUBE_CONTEO pair = { 0 };
	pair.palabras = g_ptr_array_new();
	for (int i = 0; i < 3; ++i) g_ptr_array_add(pair.palabras, &pair_words[i]);
	CloudLayout *a, *b;
	cloud_build_pair(canvas, canvas, &pair, "#071610", &a, &b);
	g_assert_cmpuint(a->words->len, ==, 2);
	g_assert_cmpuint(b->words->len, ==, 2);
	g_assert_cmpstr(pango_layout_get_text(g_array_index(a->words, CloudWord, 0).layout), ==, "Jesús");
	g_assert_cmpstr(pango_layout_get_text(g_array_index(b->words, CloudWord, 0).layout), ==, "Moisés");
	CloudWord *shared_a = &g_array_index(a->words, CloudWord, 1);
	CloudWord *shared_b = &g_array_index(b->words, CloudWord, 1);
	g_assert_cmpint(pango_font_description_get_size(pango_layout_get_font_description(shared_a->layout)), ==,
		pango_font_description_get_size(pango_layout_get_font_description(shared_b->layout)));
	g_assert_true(gdk_rgba_equal(&shared_a->color, &shared_b->color));
	cloud_free(a);
	cloud_free(b);
	g_ptr_array_free(pair.palabras, TRUE);
	gtk_widget_destroy(window);
	g_ptr_array_free(count.palabras, TRUE);
	for (int i = 0; i < 80; ++i)
		g_free(words[i].etiqueta);
	nube_mayusculas_libre(learned);
	g_print("80 words, no collisions, decreasing sizes, rotations and capitals: PASS\n");
	return 0;
}
