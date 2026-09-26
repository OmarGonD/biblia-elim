/* Native, deterministic word-cloud layout. Coordinates are independent of the
 * window size; resizing scales the complete cloud without moving its words. */
#ifndef NUBE_CANVAS_H
#define NUBE_CANVAS_H
#include <gtk/gtk.h>
#include <math.h>
#include "main/nube_palabras.h"

typedef struct {
	PangoLayout *layout;
	GdkRGBA color;
	double x, y, w, h;
	gboolean vertical;
} CloudWord;
typedef struct {
	GArray *words;
	GdkRGBA background;
	double factor;
} CloudLayout;

/* How the word is shown: as the text writes it, «Noemí» but «campo»
 * (CLOUD-CASE-101, learned from the counted text). The key stays in
 * lowercase for counting. */
static gchar *cloud_label(const NUBE_PALABRA *word)
{
	return g_strdup(word->etiqueta ? word->etiqueta : word->palabra);
}

static void cloud_free(gpointer data)
{
	CloudLayout *cloud = data;
	if (!cloud) return;
	for (guint i = 0; i < cloud->words->len; ++i)
		g_object_unref(g_array_index(cloud->words, CloudWord, i).layout);
	g_array_free(cloud->words, TRUE);
	g_free(cloud);
}

static gboolean cloud_overlaps(const CloudWord *a, const CloudWord *b)
{
	return a->x < b->x + b->w + 5 && a->x + a->w + 5 > b->x &&
	       a->y < b->y + b->h + 5 && a->y + a->h + 5 > b->y;
}

static CloudLayout *cloud_build_scaled(GtkWidget *widget, NUBE_CONTEO *count,
		gboolean compare, const char *background, int shared_max, double initial)
{
	CloudLayout *cloud = g_new0(CloudLayout, 1);
	cloud->words = g_array_new(FALSE, FALSE, sizeof(CloudWord));
	gdk_rgba_parse(&cloud->background, background);
	gboolean dark = cloud->background.red + cloud->background.green +
		cloud->background.blue < 1.5;
	const char *light[] = { "#164b46", "#b34412", "#315c91", "#76345c", "#617219" };
	const char *night[] = { "#a5dccd", "#ffb47e", "#9fcaff", "#e7abd0", "#d3df89" };
	guint n = MIN(count->palabras->len, 80);
	int maximum = 1;
	for (guint i = 0; i < n; ++i) {
		NUBE_PALABRA *w = g_ptr_array_index(count->palabras, i);
		maximum = MAX(maximum, compare ? MAX(w->cuenta, w->cuenta_b) : w->cuenta);
	}
	maximum = MAX(maximum, shared_max);
	/* Retry the whole layout at a smaller common scale if it does not fit.
	 * This preserves frequency order instead of shrinking individual words. */
	for (double factor = initial; factor > 0.12; factor *= 0.85) {
		cloud->factor = factor;
		for (guint i = 0; i < cloud->words->len; ++i)
			g_object_unref(g_array_index(cloud->words, CloudWord, i).layout);
		g_array_set_size(cloud->words, 0);
		gboolean complete = TRUE;
		for (guint i = 0; i < n; ++i) {
			NUBE_PALABRA *word = g_ptr_array_index(count->palabras, i);
			int frequency = compare ? MAX(word->cuenta, word->cuenta_b) : word->cuenta;
			CloudWord w = { 0 };
			gchar *label = cloud_label(word);
			w.layout = gtk_widget_create_pango_layout(widget, label);
			g_free(label);
			PangoFontDescription *font = pango_font_description_from_string("Sans Bold");
			pango_font_description_set_absolute_size(font,
				factor * (14 + 82 * pow((double)frequency / maximum, 0.9)) * PANGO_SCALE);
			pango_layout_set_font_description(w.layout, font);
			pango_font_description_free(font);
			int width, height;
			pango_layout_get_pixel_size(w.layout, &width, &height);
			w.vertical = i > 2 && i % 5 == 3;
			w.w = w.vertical ? height : width;
			w.h = w.vertical ? width : height;
			const char *color = (dark ? night : light)[g_str_hash(word->palabra) % 5];
			if (compare)
				color = word->dif_pct > 0.08 ? (dark ? night[2] : light[2]) :
					word->dif_pct < -0.08 ? (dark ? night[1] : light[1]) :
					(dark ? "#ced4da" : "#6c757d");
			gdk_rgba_parse(&w.color, color);
			gboolean placed = FALSE;
			for (int step = 0; step < 14000; ++step) {
				double angle = step * 0.075, radius = 0.042 * step;
				w.x = 550 + cos(angle) * radius * 1.5 - w.w / 2;
				w.y = 325 + sin(angle) * radius - w.h / 2;
				if (w.x < 12 || w.y < 12 || w.x + w.w > 1088 || w.y + w.h > 638)
					continue;
				gboolean hit = FALSE;
				for (guint j = 0; j < cloud->words->len && !hit; ++j)
					hit = cloud_overlaps(&w, &g_array_index(cloud->words, CloudWord, j));
				if (!hit) { placed = TRUE; break; }
			}
			if (!placed) {
				g_object_unref(w.layout);
				complete = FALSE;
				break;
			}
			g_array_append_val(cloud->words, w);
		}
		if (complete) break;
	}
	return cloud;
}

static CloudLayout *cloud_build(GtkWidget *widget, NUBE_CONTEO *count,
		gboolean compare, const char *background)
{
	return cloud_build_scaled(widget, count, compare, background, 0, 1.0);
}

static gint cloud_frequency_desc(gconstpointer a, gconstpointer b)
{
	const NUBE_PALABRA *wa = *(NUBE_PALABRA *const *)a;
	const NUBE_PALABRA *wb = *(NUBE_PALABRA *const *)b;
	return wa->cuenta == wb->cuenta ? g_strcmp0(wa->palabra, wb->palabra) :
		wb->cuenta - wa->cuenta;
}

static void cloud_build_pair(GtkWidget *a, GtkWidget *b, NUBE_CONTEO *source,
		const char *background, CloudLayout **out_a, CloudLayout **out_b)
{
	NUBE_CONTEO books[2] = { 0 };
	int maximum = 1;
	for (int side = 0; side < 2; ++side) {
		books[side].palabras = g_ptr_array_new_with_free_func(g_free);
		for (guint i = 0; i < source->palabras->len; ++i) {
			NUBE_PALABRA *word = g_ptr_array_index(source->palabras, i);
			int frequency = side ? word->cuenta_b : word->cuenta;
			if (!frequency) continue;
			NUBE_PALABRA *copy = g_new0(NUBE_PALABRA, 1);
			copy->palabra = word->palabra;
			copy->etiqueta = word->etiqueta;
			copy->cuenta = frequency;
			maximum = MAX(maximum, frequency);
			g_ptr_array_add(books[side].palabras, copy);
		}
		g_ptr_array_sort(books[side].palabras, cloud_frequency_desc);
	}
	*out_a = cloud_build_scaled(a, &books[0], FALSE, background, maximum, 1);
	*out_b = cloud_build_scaled(b, &books[1], FALSE, background, maximum, 1);
	/* Both panels use identical frequency and packing scales. */
	double factor = MIN((*out_a)->factor, (*out_b)->factor);
	if ((*out_a)->factor != factor) {
		cloud_free(*out_a);
		*out_a = cloud_build_scaled(a, &books[0], FALSE, background, maximum, factor);
	}
	if ((*out_b)->factor != factor) {
		cloud_free(*out_b);
		*out_b = cloud_build_scaled(b, &books[1], FALSE, background, maximum, factor);
	}
	for (int side = 0; side < 2; ++side)
		g_ptr_array_free(books[side].palabras, TRUE);
}

static gboolean cloud_draw(GtkWidget *widget, cairo_t *cr, gpointer unused)
{
	(void)unused;
	CloudLayout *cloud = g_object_get_data(G_OBJECT(widget), "cloud");
	if (!cloud) return FALSE;
	gdk_cairo_set_source_rgba(cr, &cloud->background);
	cairo_paint(cr);
	double width = gtk_widget_get_allocated_width(widget);
	double height = gtk_widget_get_allocated_height(widget);
	double scale = MIN(width / 1100, height / 650);
	cairo_translate(cr, (width - 1100 * scale) / 2, (height - 650 * scale) / 2);
	cairo_scale(cr, scale, scale);
	for (guint i = 0; i < cloud->words->len; ++i) {
		CloudWord *w = &g_array_index(cloud->words, CloudWord, i);
		cairo_save(cr);
		cairo_translate(cr, w->x, w->y);
		if (w->vertical) { cairo_translate(cr, 0, w->h); cairo_rotate(cr, -G_PI / 2); }
		gdk_cairo_set_source_rgba(cr, &w->color);
		pango_cairo_show_layout(cr, w->layout);
		cairo_restore(cr);
	}
	return FALSE;
}
#endif
