#include <string.h>
#include <glib/gstdio.h>
#include "gtk/nube_canvas.h"
#include "main/nube_mayusculas.h"

static void anotar(const gchar *palabra, gboolean abre_frase, gpointer m)
{
	nube_mayusculas_anotar(m, palabra, abre_frase);
}

int main(int argc, char **argv)
{
	gtk_init();
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
	/* Never shown: gtk_widget_create_pango_layout() below only needs
	 * canvas parented into a widget tree that reaches a display, not a
	 * mapped one. GTK 4 dropped GtkOffscreenWindow along with the
	 * generic "rasterize any widget" API it offered; the one place this
	 * file used to capture a picture of the cloud now goes through
	 * cloud_paint() directly, the same routine cloud_export() (and the
	 * production draw func) already exercise below. */
	GtkWidget *window = gtk_window_new();
	GtkWidget *canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(canvas, 1100, 650);
	gtk_window_set_child(GTK_WINDOW(window), canvas);
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
	CloudLayout *cloud = cloud_build(canvas, &count, FALSE, "#faf8f3", 0);
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
	/* The view box holds every word, so drawing fits the words, not the
	 * empty layout space around them. */
	for (guint i = 0; i < cloud->words->len; ++i) {
		CloudWord *w = &g_array_index(cloud->words, CloudWord, i);
		g_assert_cmpfloat(w->x, >=, cloud->vx);
		g_assert_cmpfloat(w->y, >=, cloud->vy);
		g_assert_cmpfloat(w->x + w->w, <=, cloud->vx + cloud->vw);
		g_assert_cmpfloat(w->y + w->h, <=, cloud->vy + cloud->vh);
	}
	/* A wide panel gets a wider cloud, still without collisions. */
	CloudLayout *wide = cloud_build(canvas, &count, FALSE, "#faf8f3", 3.0);
	g_assert_cmpuint(wide->words->len, ==, 80);
	for (guint i = 0; i < wide->words->len; ++i)
		for (guint j = 0; j < i; ++j)
			g_assert_false(cloud_overlaps(&g_array_index(wide->words, CloudWord, i),
				&g_array_index(wide->words, CloudWord, j)));
	g_assert_cmpfloat(wide->vw / wide->vh, >, cloud->vw / cloud->vh);
	cloud_free(wide);
	/* Comparing starts from a random book, never book A. */
	GListStore *books = elim_table_new();
	const char *names[] = { "Génesis", "Rut", "Lucas" };
	for (guint i = 0; i < G_N_ELEMENTS(names); ++i) {
		ElimRow *row = elim_row_new(1);
		elim_row_set_string(row, 0, names[i]);
		g_list_store_append(books, row);
		g_object_unref(row);
	}
	gboolean seen[3] = { FALSE, FALSE, FALSE };
	for (int round = 0; round < 200; ++round) {
		gchar *pick = cloud_random_book(books, 0, "Lucas");
		g_assert_nonnull(pick);
		g_assert_cmpstr(pick, !=, "Lucas");
		for (guint i = 0; i < G_N_ELEMENTS(names); ++i)
			seen[i] |= g_strcmp0(pick, names[i]) == 0;
		g_free(pick);
	}
	g_assert_true(seen[0] && seen[1] && !seen[2]);
	GListStore *only = elim_table_new();
	ElimRow *lucas = elim_row_new(1);
	elim_row_set_string(lucas, 0, "Lucas");
	g_list_store_append(only, lucas);
	g_object_unref(lucas);
	g_assert_null(cloud_random_book(only, 0, "Lucas"));
	g_object_unref(only);
	g_object_unref(books);
	g_assert_cmpstr(pango_layout_get_text(g_array_index(cloud->words, CloudWord, 0).layout), ==, "Dios");
	g_assert_cmpstr(pango_layout_get_text(g_array_index(cloud->words, CloudWord, 1).layout), ==, "Jesús");
	g_assert_cmpstr(pango_layout_get_text(g_array_index(cloud->words, CloudWord, 13).layout), ==, "Jerusalén");
	g_object_set_data_full(G_OBJECT(canvas), "cloud", cloud, cloud_free);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(canvas), cloud_draw, NULL, NULL);
	if (argc > 1) {
		cairo_surface_t *surface =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1100, 650);
		cairo_t *cr = cairo_create(surface);
		cloud_paint(cr, cloud, 1100, 650);
		cairo_destroy(cr);
		g_assert_cmpint(cairo_surface_write_to_png(surface, argv[1]), ==,
			CAIRO_STATUS_SUCCESS);
		cairo_surface_destroy(surface);
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
	cloud_build_pair(canvas, canvas, &pair, "#071610", 0, &a, &b);
	g_assert_cmpuint(a->words->len, ==, 2);
	g_assert_cmpuint(b->words->len, ==, 2);
	g_assert_cmpstr(pango_layout_get_text(g_array_index(a->words, CloudWord, 0).layout), ==, "Jesús");
	g_assert_cmpstr(pango_layout_get_text(g_array_index(b->words, CloudWord, 0).layout), ==, "Moisés");
	CloudWord *shared_a = &g_array_index(a->words, CloudWord, 1);
	CloudWord *shared_b = &g_array_index(b->words, CloudWord, 1);
	g_assert_cmpint(pango_font_description_get_size(pango_layout_get_font_description(shared_a->layout)), ==,
		pango_font_description_get_size(pango_layout_get_font_description(shared_b->layout)));
	g_assert_true(gdk_rgba_equal(&shared_a->color, &shared_b->color));
	/* Both panels draw with the same view size, hence the same scale. */
	g_assert_cmpfloat(a->vw, ==, b->vw);
	g_assert_cmpfloat(a->vh, ==, b->vh);
	/* Books of very different length share a scale by proportion: the
	 * same share of each book gets the same size (Numbers vs 2 Thess). */
	NUBE_PALABRA share_words[] = {
		{ .palabra = "jehová", .etiqueta = "Jehová", .cuenta = 396, .cuenta_b = 0 },
		{ .palabra = "dios", .etiqueta = "Dios", .cuenta = 132, .cuenta_b = 0 },
		{ .palabra = "señor", .etiqueta = "Señor", .cuenta = 0, .cuenta_b = 12 },
		{ .palabra = "fe", .etiqueta = "fe", .cuenta = 0, .cuenta_b = 4 }
	};
	NUBE_CONTEO share = { .total = 13200, .total_b = 400 };
	share.palabras = g_ptr_array_new();
	for (int i = 0; i < 4; ++i) g_ptr_array_add(share.palabras, &share_words[i]);
	CloudLayout *big, *small;
	cloud_build_pair(canvas, canvas, &share, "#faf8f3", 0, &big, &small);
	for (guint i = 0; i < 2; ++i)
		g_assert_cmpint(pango_font_description_get_size(pango_layout_get_font_description(
				g_array_index(big->words, CloudWord, i).layout)), ==,
			pango_font_description_get_size(pango_layout_get_font_description(
				g_array_index(small->words, CloudWord, i).layout)));
	cloud_free(big);
	cloud_free(small);
	g_ptr_array_free(share.palabras, TRUE);
	/* Downloads: one cloud, or both side by side, as PNG (2x) or SVG. */
	gchar *dir = g_dir_make_tmp("nube-export-XXXXXX", NULL);
	g_assert_nonnull(dir);
	gchar *png_one = g_build_filename(dir, "una.png", NULL);
	gchar *png_two = g_build_filename(dir, "dos.png", NULL);
	gchar *svg_two = g_build_filename(dir, "dos.svg", NULL);
	GError *error = NULL;
	g_assert_true(cloud_export(png_one, a, "<b>Lucas</b>", NULL, NULL, "#222222", &error));
	g_assert_no_error(error);
	g_assert_true(cloud_export(png_two, a, "<b>Lucas</b>", b, "<b>Salmos</b>", "#eeeeee", &error));
	g_assert_true(cloud_export(svg_two, a, "<b>Lucas</b>", b, "<b>Salmos</b>", "#eeeeee", &error));
	GdkPixbuf *one = gdk_pixbuf_new_from_file(png_one, &error);
	g_assert_no_error(error);
	GdkPixbuf *two = gdk_pixbuf_new_from_file(png_two, &error);
	g_assert_no_error(error);
	g_assert_cmpint(gdk_pixbuf_get_width(one), ==, 2 * (2 * 32 + 1400));
	g_assert_cmpint(gdk_pixbuf_get_width(two), ==, 2 * (2 * 32 + 2 * 900 + 32));
	g_object_unref(one);
	g_object_unref(two);
	gchar *svg_text = NULL;
	g_assert_true(g_file_get_contents(svg_two, &svg_text, NULL, NULL));
	g_assert_nonnull(strstr(svg_text, "<svg"));
	g_free(svg_text);
	g_assert_false(cloud_export("/nonexistent-dir/x.png", a, NULL, NULL, NULL, NULL, &error));
	g_assert_nonnull(error);
	g_clear_error(&error);
	for (gchar **f = (gchar *[]){ png_one, png_two, svg_two, NULL }; *f; ++f) {
		g_remove(*f);
		g_free(*f);
	}
	g_rmdir(dir);
	g_free(dir);
	cloud_free(a);
	cloud_free(b);
	g_ptr_array_free(pair.palabras, TRUE);
	gtk_window_destroy(GTK_WINDOW(window));
	g_ptr_array_free(count.palabras, TRUE);
	for (int i = 0; i < 80; ++i)
		g_free(words[i].etiqueta);
	nube_mayusculas_libre(learned);
	g_print("80 words, no collisions, decreasing sizes, rotations, capitals, view box, aspect, random book and PNG/SVG export: PASS\n");
	return 0;
}
