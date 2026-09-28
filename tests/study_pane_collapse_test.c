/*
 * Closing the study pane must hand the leftover width to the Bible pane.
 * GtkPaned only reallocates after the toplevel is mapped; hiding the
 * second child then gives the first child the splitter's allocation.
 */
#include <gtk/gtk.h>
#include <stdio.h>

static int failures;
static GMainLoop *loop;
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

	while (g_main_context_pending(NULL) && n++ < 200)
		g_main_context_iteration(NULL, FALSE);
}

static GtkWidget *
expanding_pane(const char *name)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget *label = gtk_label_new(name);

	gtk_widget_set_vexpand(label, TRUE);
	gtk_box_append(GTK_BOX(box), label);
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

	gtk_widget_hide(study);
	gtk_paned_set_position(GTK_PANED(hpaned), splitter);
	gtk_widget_queue_resize(hpaned);
	gtk_widget_queue_resize(bible);
	pump();

	gtk_widget_get_allocation(bible, &bible_alloc);
	fprintf(stderr, "collapse: splitter=%d bible=%d mapped=%d\n", splitter,
		bible_alloc.width, gtk_widget_get_mapped(win));
	CHECK(bible_alloc.width >= splitter - 24);

	g_main_loop_quit(loop);
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
	(void)argc;
	(void)argv;
	if (!gtk_init_check()) {
		fprintf(stderr, "study_pane_collapse_test: no display; SKIP\n");
		return 0;
	}

	win = gtk_window_new();
	gtk_window_set_default_size(GTK_WINDOW(win), 800, 400);
	hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	bible = expanding_pane("bible");
	study = expanding_pane("study");
	gtk_paned_set_start_child(GTK_PANED(hpaned), bible);
	gtk_paned_set_resize_start_child(GTK_PANED(hpaned), TRUE);
	gtk_paned_set_shrink_start_child(GTK_PANED(hpaned), FALSE);
	gtk_paned_set_end_child(GTK_PANED(hpaned), study);
	gtk_paned_set_resize_end_child(GTK_PANED(hpaned), TRUE);
	gtk_paned_set_shrink_end_child(GTK_PANED(hpaned), FALSE);
	gtk_window_set_child(GTK_WINDOW(win), hpaned);
	g_signal_connect(win, "map", G_CALLBACK(on_map), NULL);
	gtk_widget_show(win);
	gtk_window_present(GTK_WINDOW(win));
	loop = g_main_loop_new(NULL, FALSE);
	g_timeout_add(2000, (GSourceFunc)g_main_loop_quit, loop);
	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	if (!gtk_widget_get_mapped(win) && failures == 0) {
		fprintf(stderr, "study_pane_collapse_test: window never mapped; SKIP\n");
		gtk_window_destroy(GTK_WINDOW(win));
		return 0;
	}
	gtk_window_destroy(GTK_WINDOW(win));
	pump();
	if (failures) {
		fprintf(stderr, "study_pane_collapse_failures=%d\n", failures);
		return 1;
	}
	fprintf(stderr, "study_pane_collapse_failures=0\n");
	return 0;
}
