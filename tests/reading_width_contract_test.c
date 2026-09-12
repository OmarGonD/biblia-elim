/*
 * UI-WIDTH-101 permanent contract: unused reading width is not caused by
 * GtkTextView geometry / margins changing between content loads. Long wrapped
 * prose fills the view; short structural lines (headings) leave large
 * remaining_right_space without changing margins.
 */
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,          \
				__LINE__, #cond);                              \
			failures++;                                            \
		}                                                              \
	} while (0)

#define CONTROL_TEXT                                                           \
	"Este es un texto de control destinado exclusivamente a medir "        \
	"el ancho de representación del GtkTextView. Este mismo texto debe "   \
	"aparecer exactamente igual independientemente del módulo "            \
	"seleccionado. "

typedef struct {
	gint remaining;
	gint line_w;
} LineGeom;

static void
pump(void)
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

static LineGeom
first_long_line(GtkTextView *tv)
{
	GtkTextBuffer *buf = gtk_text_view_get_buffer(tv);
	GtkTextIter iter;
	GdkRectangle visible;
	LineGeom best = { .remaining = -1, .line_w = -1 };
	int i;

	gtk_text_view_get_visible_rect(tv, &visible);
	gtk_text_buffer_get_start_iter(buf, &iter);
	for (i = 0; i < 12 && !gtk_text_iter_is_end(&iter); i++) {
		GtkTextIter start = iter;
		GtkTextIter end = iter;
		GdkRectangle loc0, loc1;
		gint line_w, remaining;

		gtk_text_view_backward_display_line_start(tv, &start);
		gtk_text_view_forward_display_line_end(tv, &end);
		gtk_text_view_get_iter_location(tv, &start, &loc0);
		gtk_text_view_get_iter_location(tv, &end, &loc1);
		line_w = (loc1.x + loc1.width) - loc0.x;
		remaining = visible.x + visible.width - (loc1.x + loc1.width);
		if (line_w > best.line_w) {
			best.line_w = line_w;
			best.remaining = remaining;
		}
		if (!gtk_text_view_forward_display_line(tv, &iter))
			break;
	}
	return best;
}

static void
check_fill(GtkTextView *tv, const char *label, gint max_remaining)
{
	LineGeom g = first_long_line(tv);
	fprintf(stderr, "%s: longest_line_w=%d remaining=%d\n", label, g.line_w,
		g.remaining);
	CHECK(g.line_w > 400);
	CHECK(g.remaining >= 0);
	CHECK(g.remaining <= max_remaining);
}

static void
run_contract(void)
{
	GtkWidget *win;
	GtkWidget *scroll;
	GtkTextView *tv;
	GtkTextBuffer *buf;
	GString *long_text;
	gint left_a, right_a, left_b, right_b;
	gint alloc_a, alloc_b;
	LineGeom heading_geom;
	int i;

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(win), 820, 400);
	scroll = gtk_scrolled_window_new(NULL, NULL);
	tv = GTK_TEXT_VIEW(gtk_text_view_new());
	gtk_text_view_set_wrap_mode(tv, GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin(tv, 14);
	gtk_text_view_set_right_margin(tv, 14);
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(tv));
	gtk_container_add(GTK_CONTAINER(win), scroll);
	gtk_widget_show_all(win);
	pump();

	buf = gtk_text_view_get_buffer(tv);
	long_text = g_string_new(NULL);
	for (i = 0; i < 8; i++)
		g_string_append(long_text, CONTROL_TEXT);

	/* Load A: continuous prose (SpaRV-like). */
	gtk_text_buffer_set_text(buf, long_text->str, -1);
	pump();
	left_a = gtk_text_view_get_left_margin(tv);
	right_a = gtk_text_view_get_right_margin(tv);
	alloc_a = gtk_widget_get_allocated_width(GTK_WIDGET(tv));
	check_fill(tv, "prose_a", 80);

	/* Load B: identical text after a "module switch". */
	gtk_text_buffer_set_text(buf, long_text->str, -1);
	pump();
	left_b = gtk_text_view_get_left_margin(tv);
	right_b = gtk_text_view_get_right_margin(tv);
	alloc_b = gtk_widget_get_allocated_width(GTK_WIDGET(tv));
	CHECK(left_a == left_b);
	CHECK(right_a == right_b);
	CHECK(alloc_a == alloc_b);
	CHECK(left_a == 14 && right_a == 14);
	check_fill(tv, "prose_b", 80);

	/* Structural short lines (SpaPlatense-like headings) leave large
	 * remaining space without changing margins. */
	gtk_text_buffer_set_text(buf,
				 "PRÓLOGO\n"
				 "I. LA IGLESIA EN JERUSALÉN\n"
				 "Últimos avisos de Jesús\n",
				 -1);
	pump();
	CHECK(gtk_text_view_get_left_margin(tv) == 14);
	CHECK(gtk_text_view_get_right_margin(tv) == 14);
	heading_geom = first_long_line(tv);
	fprintf(stderr, "heading: line_w=%d remaining=%d\n", heading_geom.line_w,
		heading_geom.remaining);
	CHECK(heading_geom.remaining > 200);

	/* Mixed: heading then long prose — long prose still fills. */
	{
		GString *mixed = g_string_new("PRÓLOGO\nÚltimos avisos\n");
		g_string_append(mixed, long_text->str);
		gtk_text_buffer_set_text(buf, mixed->str, -1);
		pump();
		CHECK(gtk_text_view_get_left_margin(tv) == 14);
		check_fill(tv, "mixed_prose", 80);
		g_string_free(mixed, TRUE);
	}

	g_string_free(long_text, TRUE);
	gtk_widget_destroy(win);
	pump();
}

int
main(int argc, char **argv)
{
	if (!gtk_init_check(&argc, &argv)) {
		fprintf(stderr,
			"reading_width_contract_test: no display; SKIP\n");
		return 0;
	}
	run_contract();
	if (failures) {
		fprintf(stderr, "reading_width_contract_failures=%d\n", failures);
		return 1;
	}
	fprintf(stderr, "reading_width_contract_failures=0\n");
	return 0;
}
