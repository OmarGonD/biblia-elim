/* Native, deterministic word-cloud layout. Coordinates are independent of the
 * window size; resizing scales the complete cloud without moving its words. */
#ifndef NUBE_CANVAS_H
#define NUBE_CANVAS_H
#include <gtk/gtk.h>
#include <math.h>
#include <cairo.h>
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif
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
	/* The part of the 1100x650 layout space the words occupy, plus a
	 * margin: drawing fits this box, not the whole space. */
	double vx, vy, vw, vh;
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

/* The placement spiral, computed once: step s sits at angle 0.075 s and
 * radius 0.042 s; x is stretched per panel. */
#define CLOUD_SPIRAL_STEPS 14000
typedef struct { double x[CLOUD_SPIRAL_STEPS], y[CLOUD_SPIRAL_STEPS]; } CloudSpiral;

static const CloudSpiral *cloud_spiral(void)
{
	static CloudSpiral *spiral = NULL;
	if (!spiral) {
		spiral = g_new(CloudSpiral, 1);
		for (int step = 0; step < CLOUD_SPIRAL_STEPS; ++step) {
			double angle = step * 0.075, radius = 0.042 * step;
			spiral->x[step] = cos(angle) * radius;
			spiral->y[step] = sin(angle) * radius;
		}
	}
	return spiral;
}

static void cloud_fit_view(CloudLayout *cloud)
{
	if (!cloud->words->len) {
		cloud->vx = 0; cloud->vy = 0; cloud->vw = 1100; cloud->vh = 650;
		return;
	}
	double x0 = G_MAXDOUBLE, y0 = G_MAXDOUBLE, x1 = -G_MAXDOUBLE, y1 = -G_MAXDOUBLE;
	for (guint i = 0; i < cloud->words->len; ++i) {
		CloudWord *w = &g_array_index(cloud->words, CloudWord, i);
		x0 = MIN(x0, w->x); y0 = MIN(y0, w->y);
		x1 = MAX(x1, w->x + w->w); y1 = MAX(y1, w->y + w->h);
	}
	const double margin = 18;
	cloud->vx = x0 - margin; cloud->vy = y0 - margin;
	cloud->vw = x1 - x0 + 2 * margin; cloud->vh = y1 - y0 + 2 * margin;
}

/* Give two clouds the same view size around their own centres, so a word
 * drawn in either panel keeps the same on-screen scale. */
static void cloud_share_view(CloudLayout *a, CloudLayout *b)
{
	double w = MAX(a->vw, b->vw), h = MAX(a->vh, b->vh);
	CloudLayout *both[2] = { a, b };
	for (int i = 0; i < 2; ++i) {
		both[i]->vx -= (w - both[i]->vw) / 2;
		both[i]->vy -= (h - both[i]->vh) / 2;
		both[i]->vw = w;
		both[i]->vh = h;
	}
}

static CloudLayout *cloud_build_scaled(GtkWidget *widget, NUBE_CONTEO *count,
		gboolean compare, const char *background, int shared_max, double initial,
		double aspect)
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
	/* Spread the spiral like the panel it will fill (width / height): a
	 * wide panel gets a wide cloud. 0 keeps the classic 1.5 ellipse. */
	double spread = aspect > 0 ?
		CLAMP(1.5 * aspect / (1100.0 / 650.0), 0.55, 2.6) : 1.5;
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
			/* Rare words recede so the frequent ones lead. */
			w.color.alpha = 0.62 + 0.38 * sqrt((double)frequency / maximum);
			gboolean placed = FALSE;
			const CloudSpiral *spiral = cloud_spiral();
			/* The word hit last is likely to be hit again one step
			 * further along the spiral: test it first. */
			guint last_hit = G_MAXUINT;
			for (int step = 0; step < CLOUD_SPIRAL_STEPS; ++step) {
				w.x = 550 + spiral->x[step] * spread - w.w / 2;
				w.y = 325 + spiral->y[step] - w.h / 2;
				if (w.x < 12 || w.y < 12 || w.x + w.w > 1088 || w.y + w.h > 638)
					continue;
				gboolean hit = last_hit < cloud->words->len &&
					cloud_overlaps(&w, &g_array_index(cloud->words, CloudWord, last_hit));
				for (guint j = 0; j < cloud->words->len && !hit; ++j)
					if (cloud_overlaps(&w, &g_array_index(cloud->words, CloudWord, j))) {
						hit = TRUE;
						last_hit = j;
					}
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
	cloud_fit_view(cloud);
	return cloud;
}

static CloudLayout *cloud_build(GtkWidget *widget, NUBE_CONTEO *count,
		gboolean compare, const char *background, double aspect)
{
	return cloud_build_scaled(widget, count, compare, background, 0, 1.0, aspect);
}

static gint cloud_frequency_desc(gconstpointer a, gconstpointer b)
{
	const NUBE_PALABRA *wa = *(NUBE_PALABRA *const *)a;
	const NUBE_PALABRA *wb = *(NUBE_PALABRA *const *)b;
	return wa->cuenta == wb->cuenta ? g_strcmp0(wa->palabra, wb->palabra) :
		wb->cuenta - wa->cuenta;
}

static void cloud_build_pair(GtkWidget *a, GtkWidget *b, NUBE_CONTEO *source,
		const char *background, double aspect,
		CloudLayout **out_a, CloudLayout **out_b)
{
	NUBE_CONTEO books[2] = { 0 };
	int maximum = 1;
	for (int side = 0; side < 2; ++side) {
		books[side].palabras = g_ptr_array_new_with_free_func(g_free);
		/* Books of very different length compare by share, not by count:
		 * sizes follow parts per million of each book's words, so 3 % of
		 * a short epistle looks like 3 % of Numbers. */
		int total = side ? source->total_b : source->total;
		for (guint i = 0; i < source->palabras->len; ++i) {
			NUBE_PALABRA *word = g_ptr_array_index(source->palabras, i);
			int frequency = side ? word->cuenta_b : word->cuenta;
			if (!frequency) continue;
			NUBE_PALABRA *copy = g_new0(NUBE_PALABRA, 1);
			copy->palabra = word->palabra;
			copy->etiqueta = word->etiqueta;
			copy->cuenta = total > 0 ?
				MAX(1, (int)lround(1e6 * frequency / total)) : frequency;
			maximum = MAX(maximum, frequency);
			g_ptr_array_add(books[side].palabras, copy);
		}
		g_ptr_array_sort(books[side].palabras, cloud_frequency_desc);
	}
	*out_a = cloud_build_scaled(a, &books[0], FALSE, background, maximum, 1, aspect);
	*out_b = cloud_build_scaled(b, &books[1], FALSE, background, maximum, 1, aspect);
	/* Both panels use identical frequency and packing scales. */
	double factor = MIN((*out_a)->factor, (*out_b)->factor);
	if ((*out_a)->factor != factor) {
		cloud_free(*out_a);
		*out_a = cloud_build_scaled(a, &books[0], FALSE, background, maximum, factor, aspect);
	}
	if ((*out_b)->factor != factor) {
		cloud_free(*out_b);
		*out_b = cloud_build_scaled(b, &books[1], FALSE, background, maximum, factor, aspect);
	}
	cloud_share_view(*out_a, *out_b);
	for (int side = 0; side < 2; ++side)
		g_ptr_array_free(books[side].palabras, TRUE);
}

/* A book of MODEL's COLUMN chosen at random, never EXCLUDE (compared
 * with g_utf8_collate). NULL when no other book exists. */
static gchar *cloud_random_book(GtkTreeModel *model, int column, const char *exclude)
{
	GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
	GtkTreeIter iter;
	for (gboolean ok = gtk_tree_model_get_iter_first(model, &iter); ok;
	     ok = gtk_tree_model_iter_next(model, &iter)) {
		gchar *name = NULL;
		gtk_tree_model_get(model, &iter, column, &name, -1);
		if (name && *name && !(exclude && g_utf8_collate(name, exclude) == 0))
			g_ptr_array_add(names, name);
		else
			g_free(name);
	}
	gchar *chosen = names->len ?
		g_strdup(g_ptr_array_index(names, g_random_int_range(0, names->len))) : NULL;
	g_ptr_array_free(names, TRUE);
	return chosen;
}

/* Paint CLOUD as a rounded card of WIDTH x HEIGHT at the current origin. */
static void cloud_paint(cairo_t *cr, CloudLayout *cloud, double width, double height)
{
	cairo_save(cr);
	/* A rounded card in the reading colours, with a hairline edge. */
	const double r = 10, inset = 0.5;
	cairo_new_sub_path(cr);
	cairo_arc(cr, width - r - inset, r + inset, r, -G_PI / 2, 0);
	cairo_arc(cr, width - r - inset, height - r - inset, r, 0, G_PI / 2);
	cairo_arc(cr, r + inset, height - r - inset, r, G_PI / 2, G_PI);
	cairo_arc(cr, r + inset, r + inset, r, G_PI, 3 * G_PI / 2);
	cairo_close_path(cr);
	gdk_cairo_set_source_rgba(cr, &cloud->background);
	cairo_fill_preserve(cr);
	gboolean dark = cloud->background.red + cloud->background.green +
		cloud->background.blue < 1.5;
	cairo_set_source_rgba(cr, dark ? 1 : 0, dark ? 1 : 0, dark ? 1 : 0, 0.12);
	cairo_set_line_width(cr, 1);
	cairo_stroke(cr);
	double scale = MIN(width / cloud->vw, height / cloud->vh);
	cairo_translate(cr, (width - cloud->vw * scale) / 2, (height - cloud->vh * scale) / 2);
	cairo_scale(cr, scale, scale);
	cairo_translate(cr, -cloud->vx, -cloud->vy);
	for (guint i = 0; i < cloud->words->len; ++i) {
		CloudWord *w = &g_array_index(cloud->words, CloudWord, i);
		cairo_save(cr);
		cairo_translate(cr, w->x, w->y);
		if (w->vertical) { cairo_translate(cr, 0, w->h); cairo_rotate(cr, -G_PI / 2); }
		gdk_cairo_set_source_rgba(cr, &w->color);
		pango_cairo_update_layout(cr, w->layout);
		pango_cairo_show_layout(cr, w->layout);
		cairo_restore(cr);
	}
	cairo_restore(cr);
}

static gboolean cloud_draw(GtkWidget *widget, cairo_t *cr, gpointer unused)
{
	(void)unused;
	CloudLayout *cloud = g_object_get_data(G_OBJECT(widget), "cloud");
	if (!cloud) return FALSE;
	cloud_paint(cr, cloud, gtk_widget_get_allocated_width(widget),
		    gtk_widget_get_allocated_height(widget));
	return FALSE;
}

/* Save one cloud, or two side by side (B may be NULL), each under its
 * Pango-markup title, to PATH: SVG when PATH ends in ".svg", else PNG at
 * twice the nominal size. TEXT colours the titles. */
static gboolean cloud_export(const char *path, CloudLayout *a, const char *title_a,
		CloudLayout *b, const char *title_b, const char *text, GError **error)
{
	const double margin = 32, gap = 32, title_h = 56;
	int panels = b ? 2 : 1;
	double pw = b ? 900 : 1400;
	double ph = CLAMP(pw * MAX(a->vh / a->vw, b ? b->vh / b->vw : 0), 300, 1200);
	double width = 2 * margin + panels * pw + (panels - 1) * gap;
	double height = 2 * margin + title_h + ph;
	gchar *lower = g_ascii_strdown(path, -1);
	gboolean svg = g_str_has_suffix(lower, ".svg");
	g_free(lower);
#ifndef CAIRO_HAS_SVG_SURFACE
	svg = FALSE;
#endif
	cairo_surface_t *surface;
#ifdef CAIRO_HAS_SVG_SURFACE
	if (svg)
		surface = cairo_svg_surface_create(path, width, height);
	else
#endif
		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			(int)ceil(width * 2), (int)ceil(height * 2));
	cairo_t *cr = cairo_create(surface);
	if (!svg)
		cairo_scale(cr, 2, 2);
	gdk_cairo_set_source_rgba(cr, &a->background);
	cairo_paint(cr);
	GdkRGBA ink = { 0.13, 0.13, 0.13, 1 };
	if (text)
		gdk_rgba_parse(&ink, text);
	CloudLayout *clouds[2] = { a, b };
	const char *titles[2] = { title_a, title_b };
	for (int i = 0; i < panels; ++i) {
		double x = margin + i * (pw + gap);
		if (titles[i]) {
			PangoLayout *title = pango_cairo_create_layout(cr);
			PangoFontDescription *font = pango_font_description_from_string("Sans 15");
			pango_layout_set_font_description(title, font);
			pango_font_description_free(font);
			pango_layout_set_markup(title, titles[i], -1);
			int tw, th;
			pango_layout_get_pixel_size(title, &tw, &th);
			cairo_move_to(cr, x + (pw - tw) / 2, margin + (title_h - th) / 2 - 6);
			gdk_cairo_set_source_rgba(cr, &ink);
			pango_cairo_show_layout(cr, title);
			g_object_unref(title);
		}
		cairo_save(cr);
		cairo_translate(cr, x, margin + title_h);
		cloud_paint(cr, clouds[i], pw, ph);
		cairo_restore(cr);
	}
	cairo_destroy(cr);
	cairo_status_t status;
	if (svg) {
		cairo_surface_finish(surface);
		status = cairo_surface_status(surface);
	} else {
		status = cairo_surface_write_to_png(surface, path);
	}
	cairo_surface_destroy(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
			    cairo_status_to_string(status));
		return FALSE;
	}
	return TRUE;
}
#endif
