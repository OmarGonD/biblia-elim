/*
 * Closing the study pane must hand the leftover width to the Bible pane.
 * GtkPaned only reallocates after the toplevel is mapped; hiding the
 * second child then gives the first child the splitter's allocation.
 */
#include <gtk/gtk.h>
#include <stdio.h>

static int failures;
static GtkWidget *win;
static GtkWidget *hpaned;
static GtkWidget *bible;
static GtkWidget *study;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,          \
				__LINE__, #cond);                              \
			failures++;                                            \
		}                                                              \
	} while (0)

static void
pump(void)
{
	int n = 0;

	while (gtk_events_pending() && n++ < 200)
		gtk_main_iteration();
}

static GtkWidget *
expanding_pane(const char *name)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget *label = gtk_label_new(name);

	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
	gtk_widget_set_hexpand(box, TRUE);
	gtk_widget_set_vexpand(box, TRUE);
	gtk_widget_set_size_request(box, 50, -1);
	return box;
}

static gboolean
run_collapse(gpointer unused)
{
	GtkAllocation bible_alloc;
	gint splitter;

	(void)unused;
	pump();
	splitter = gtk_widget_get_allocated_width(hpaned);
	CHECK(splitter > 400);
	gtk_paned_set_position(GTK_PANED(hpaned), splitter / 2);
	pump();
	gtk_widget_get_allocation(bible, &bible_alloc);
	CHECK(bible_alloc.width < splitter - 80);

	gtk_widget_set_no_show_all(study, TRUE);
	gtk_widget_hide(study);
	gtk_paned_set_position(GTK_PANED(hpaned), splitter);
	gtk_widget_queue_resize(hpaned);
	gtk_widget_queue_resize(bible);
	pump();

	gtk_widget_get_allocation(bible, &bible_alloc);
	fprintf(stderr, "collapse: splitter=%d bible=%d mapped=%d\n", splitter,
		bible_alloc.width, gtk_widget_get_mapped(win));
	CHECK(bible_alloc.width >= splitter - 24);

	gtk_main_quit();
	return G_SOURCE_REMOVE;
}

static void
on_map(GtkWidget *widget, gpointer unused)
{
	(void)widget;
	(void)unused;
	/* One idle after map is not always enough for the first configure
	 * to land; a short timeout waits for the mapped allocation. */
	g_timeout_add(50, run_collapse, NULL);
}

int
main(int argc, char **argv)
{
	if (!gtk_init_check(&argc, &argv)) {
		fprintf(stderr, "study_pane_collapse_test: no display; SKIP\n");
		return 0;
	}

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(win), 800, 400);
	hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	bible = expanding_pane("bible");
	study = expanding_pane("study");
	gtk_paned_pack1(GTK_PANED(hpaned), bible, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(hpaned), study, TRUE, FALSE);
	gtk_container_add(GTK_CONTAINER(win), hpaned);
	g_signal_connect(win, "map", G_CALLBACK(on_map), NULL);
	gtk_widget_show_all(win);
	gtk_window_present(GTK_WINDOW(win));
	g_timeout_add(2000, (GSourceFunc)gtk_main_quit, NULL);
	gtk_main();

	if (!gtk_widget_get_mapped(win) && failures == 0) {
		fprintf(stderr, "study_pane_collapse_test: window never mapped; SKIP\n");
		gtk_widget_destroy(win);
		return 0;
	}
	gtk_widget_destroy(win);
	pump();
	if (failures) {
		fprintf(stderr, "study_pane_collapse_failures=%d\n", failures);
		return 1;
	}
	fprintf(stderr, "study_pane_collapse_failures=0\n");
	return 0;
}
