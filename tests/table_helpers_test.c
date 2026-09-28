/*
 * gui/table_helpers.h: rows of string/int columns in a GtkColumnView.
 * Real widgets, so it runs under a display (xvfb-run). The view is put in
 * a window and shown, so its cells are made and bound like the app's are.
 */
#include "gui/entry_suggest.h"
#include "gui/table_helpers.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                 \
	} while (0)

static int changed_column = -1;

static void
note_change(ElimRow *row, guint column, gpointer data)
{
	(void)row;
	(void)data;
	changed_column = (int)column;
}

static void
pump(void)
{
	while (g_main_context_iteration(NULL, FALSE))
		;
}

static void
add(GListStore *store, const char *name, int count)
{
	ElimRow *row = elim_row_new(3);

	elim_row_set_string(row, 0, name);
	elim_row_set_int(row, 1, count);
	elim_row_set_string(row, 2, "hidden key");
	g_list_store_append(store, row);
	g_object_unref(row);
}

static void
test_rows(void)
{
	ElimRow *row = elim_row_new(2);
	char *text;

	CHECK(elim_row_n_columns(row) == 2);
	CHECK(strcmp(elim_row_get_string(row, 0), "") == 0);
	CHECK(elim_row_get_int(row, 1) == 0);

	elim_row_set_string(row, 0, "Juan 3:16");
	elim_row_set_int(row, 1, 7);
	elim_row_set_double(row, 1, 7.5);
	CHECK(elim_row_get_double(row, 1) == 7.5);
	text = elim_row_dup_text(row, 1);
	{	/* the decimal separator follows the locale gtk_init() selected */
		char *expected = g_strdup_printf("%g", 7.5);
		CHECK(strcmp(text, expected) == 0);
		g_free(expected);
	}
	g_free(text);
	elim_row_set_int(row, 1, 7);
	CHECK(strcmp(elim_row_get_string(row, 0), "Juan 3:16") == 0);
	CHECK(elim_row_get_int(row, 1) == 7);
	CHECK(strcmp(elim_row_get_string(row, 1), "") == 0);	/* an int has no string */
	CHECK(elim_row_get_int(row, 0) == 0);			/* and a string no int */

	text = elim_row_dup_text(row, 1);
	CHECK(strcmp(text, "7") == 0);
	g_free(text);
	text = elim_row_dup_text(row, 0);
	CHECK(strcmp(text, "Juan 3:16") == 0);
	g_free(text);

	/* a column set again changes type with it, and says which one */
	g_signal_connect(row, "changed", G_CALLBACK(note_change), NULL);
	elim_row_set_string(row, 1, "seven");
	CHECK(changed_column == 1);
	CHECK(strcmp(elim_row_get_string(row, 1), "seven") == 0);
	CHECK(elim_row_get_int(row, 1) == 0);
	elim_row_set_string(row, 0, NULL);	/* NULL is empty */
	CHECK(strcmp(elim_row_get_string(row, 0), "") == 0);

	g_object_unref(row);
}

static void
test_sortable_view(void)
{
	GListStore *store = elim_table_new();
	GtkWidget *view = gtk_column_view_new(NULL);
	GtkWidget *window = gtk_window_new();
	ElimTextColumn name = elim_text_column(0);
	ElimTextColumn value = elim_text_column(1);
	GtkColumnViewColumn *numbers;
	ElimRow *first = elim_row_new(2);
	ElimRow *second = elim_row_new(2);

	gtk_window_set_child(GTK_WINDOW(window), view);
	elim_table_setup_sortable(view, store);
	elim_table_add_sortable_column(view, "Name", &name, 0,
				       ELIM_TABLE_SORT_STRING);
	numbers = elim_table_add_sortable_column(view, "Number", &value, 1,
					 ELIM_TABLE_SORT_DOUBLE);
	elim_row_set_string(first, 0, "ten");
	elim_row_set_double(first, 1, 10.0);
	elim_row_set_string(second, 0, "two");
	elim_row_set_double(second, 1, 2.0);
	g_list_store_append(store, first);
	g_list_store_append(store, second);
	g_object_unref(first);
	g_object_unref(second);
	gtk_column_view_sort_by_column(GTK_COLUMN_VIEW(view), numbers,
				    GTK_SORT_ASCENDING);
	gtk_window_present(GTK_WINDOW(window));
	pump();
	{
		GtkSelectionModel *selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(view));
		ElimRow *row = ELIM_ROW(g_list_model_get_item(G_LIST_MODEL(selection), 0));
		CHECK(row && elim_row_get_double(row, 1) == 2.0);
		g_object_unref(row);
	}
	gtk_window_destroy(GTK_WINDOW(window));
	pump();
	g_object_unref(store);
}

static void
test_move_row(void)
{
	GListStore *store = elim_table_new();
	GtkWidget *view = gtk_list_view_new(NULL, NULL);
	ElimTextColumn spec = elim_text_column(0);
	ElimRow *one = elim_row_new(1);
	ElimRow *two = elim_row_new(1);
	ElimRow *three = elim_row_new(1);

	elim_table_setup_list(view, store, &spec);
	elim_row_set_string(one, 0, "one");
	elim_row_set_string(two, 0, "two");
	elim_row_set_string(three, 0, "three");
	g_list_store_append(store, one);
	g_list_store_append(store, two);
	g_list_store_append(store, three);
	CHECK(elim_table_move_row(view, one, three, TRUE));
	CHECK(elim_table_get(store, 0) == two);
	CHECK(elim_table_get(store, 1) == three);
	CHECK(elim_table_get(store, 2) == one);
	CHECK(!elim_table_move_row(view, one, one, FALSE));
	CHECK(elim_table_remove_row(view, three));
	CHECK(g_list_model_get_n_items(G_LIST_MODEL(store)) == 2);
	g_object_unref(one);
	g_object_unref(two);
	g_object_unref(three);
	g_object_unref(view);
	g_object_unref(store);
}

static void
test_view(void)
{
	GListStore *store = elim_table_new();
	GtkWidget *window = gtk_window_new();
	GtkWidget *view;
	GtkWidget *scroll = gtk_scrolled_window_new();

	view = elim_table_view_new(store);
	CHECK(elim_table_get_store(view) == store);
	elim_table_add_text_column(view, "Verse", 0, TRUE);
	elim_table_add_text_column(view, "Count", 1, FALSE);
	CHECK(g_list_model_get_n_items(gtk_column_view_get_columns(GTK_COLUMN_VIEW(view))) == 2);

	add(store, "Génesis 1:1", 3);
	add(store, "Éxodo 3:14", 9);
	CHECK(g_list_model_get_n_items(G_LIST_MODEL(store)) == 2);

	/* nothing is selected until the reader picks a row */
	CHECK(elim_table_get_selected(view) == NULL);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
	gtk_window_set_child(GTK_WINDOW(window), scroll);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
	gtk_window_present(GTK_WINDOW(window));
	pump();
	/* showing it must not pick a row either */
	CHECK(elim_table_get_selected(view) == NULL);

	gtk_selection_model_select_item(gtk_column_view_get_model(GTK_COLUMN_VIEW(view)),
					1, TRUE);
	{
		ElimRow *row = elim_table_get_selected(view);

		CHECK(row != NULL);
		CHECK(row && strcmp(elim_row_get_string(row, 0), "Éxodo 3:14") == 0);
		CHECK(row && elim_row_get_int(row, 1) == 9);
		CHECK(row && strcmp(elim_row_get_string(row, 2), "hidden key") == 0);
	}

	/* a row changed while its cell is on screen: the store is not told,
	 * the row is */
	{
		ElimRow *row = ELIM_ROW(g_list_model_get_item(G_LIST_MODEL(store), 0));

		elim_row_set_int(row, 1, 4);
		pump();
		CHECK(elim_row_get_int(row, 1) == 4);
		g_object_unref(row);
	}

	/* rows removed take the selection with them */
	g_list_store_remove_all(store);
	pump();
	CHECK(elim_table_get_selected(view) == NULL);

	gtk_window_destroy(GTK_WINDOW(window));
	pump();
	g_object_unref(store);
}

static int toggles;

static void
flip(ElimRow *row, gpointer data)
{
	(void)data;
	toggles++;
	elim_row_set_int(row, 0, !elim_row_get_int(row, 0));
}

static void
leave_alone(ElimRow *row, gpointer data)
{
	(void)row;
	(void)data;
	toggles++;
}

static GListStore *refill_store;

/* what the reading plans do: a click refills the whole list */
static void
refill(ElimRow *row, gpointer data)
{
	ElimRow *fresh = elim_row_new(3);

	(void)data;
	toggles++;
	elim_row_set_int(row, 0, 1);
	g_list_store_remove_all(refill_store);
	elim_row_set_int(fresh, 0, 1);
	g_list_store_append(refill_store, fresh);
	g_object_unref(fresh);
}

static GtkWidget *
first_check(GtkWidget *widget)
{
	GtkWidget *child;

	if (GTK_IS_CHECK_BUTTON(widget))
		return widget;
	for (child = gtk_widget_get_first_child(widget); child;
	     child = gtk_widget_get_next_sibling(child)) {
		GtkWidget *found = first_check(child);

		if (found)
			return found;
	}
	return NULL;
}

static void
test_toggle_and_tooltip(void)
{
	GListStore *store = elim_table_new();
	GtkWidget *window = gtk_window_new();
	GtkWidget *view = elim_table_view_new(store);
	GtkWidget *scroll = gtk_scrolled_window_new();
	ElimRow *row = elim_row_new(3);
	GtkColumnViewColumn *checks, *names;
	GtkWidget *check;

	checks = elim_table_add_toggle_column(view, " ", 0, flip, NULL);
	{
		ElimTextColumn spec = elim_text_column(1);

		spec.expand = TRUE;
		spec.fixed_width = 120;
		spec.tooltip_column = 2;
		names = elim_table_add_column(view, "Name", &spec);
	}
	CHECK(gtk_column_view_column_get_fixed_width(names) == 120);
	CHECK(gtk_column_view_column_get_resizable(names));
	(void)checks;

	elim_row_set_int(row, 0, 0);
	elim_row_set_string(row, 1, "Reina-Valera");
	elim_row_set_string(row, 2, "Reina-Valera 1960 (full text)");
	g_list_store_append(store, row);
	CHECK(elim_table_get(store, 0) == row);
	CHECK(elim_table_get(store, 1) == NULL);
	g_object_unref(row);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
	gtk_window_set_child(GTK_WINDOW(window), scroll);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
	gtk_window_present(GTK_WINDOW(window));
	pump();

	check = first_check(view);
	CHECK(check != NULL);
	if (check) {
		CHECK(!gtk_check_button_get_active(GTK_CHECK_BUTTON(check)));

		/* the reader clicks: the function is told, the row changes, the
		 * box follows it */
		toggles = 0;
		gtk_check_button_set_active(GTK_CHECK_BUTTON(check), TRUE);
		pump();
		CHECK(toggles == 1);
		CHECK(elim_row_get_int(row, 0) == 1);
		CHECK(gtk_check_button_get_active(GTK_CHECK_BUTTON(check)));

		/* the row changes by itself: the box follows without a call */
		elim_row_set_int(row, 0, 0);
		pump();
		CHECK(toggles == 1);
		CHECK(!gtk_check_button_get_active(GTK_CHECK_BUTTON(check)));
	}
	gtk_window_destroy(GTK_WINDOW(window));
	pump();

	/* a click whose function empties and refills the store, the cell being
	 * clicked included, must not touch the cell afterwards */
	{
		GListStore *store3 = elim_table_new();
		GtkWidget *view3 = elim_table_view_new(store3);
		GtkWidget *window3 = gtk_window_new();
		GtkWidget *box3;
		ElimRow *row3 = elim_row_new(3);

		g_list_store_append(store3, row3);
		g_object_unref(row3);
		refill_store = store3;
		elim_table_add_toggle_column(view3, " ", 0, refill, NULL);
		gtk_window_set_child(GTK_WINDOW(window3), view3);
		gtk_window_set_default_size(GTK_WINDOW(window3), 200, 100);
		gtk_window_present(GTK_WINDOW(window3));
		pump();
		box3 = first_check(view3);
		CHECK(box3 != NULL);
		if (box3) {
			toggles = 0;
			gtk_check_button_set_active(GTK_CHECK_BUTTON(box3), TRUE);
			pump();
			CHECK(toggles == 1);
			CHECK(g_list_model_get_n_items(G_LIST_MODEL(store3)) == 1);
		}
		gtk_window_destroy(GTK_WINDOW(window3));
		pump();
		g_object_unref(store3);
	}

	/* a function that leaves the row alone leaves the box as it was */
	{
		GtkWidget *view2 = elim_table_view_new(store);
		GtkWidget *window2 = gtk_window_new();
		GtkWidget *box;

		elim_table_add_toggle_column(view2, " ", 0, leave_alone, NULL);
		gtk_window_set_child(GTK_WINDOW(window2), view2);
		gtk_window_set_default_size(GTK_WINDOW(window2), 200, 100);
		gtk_window_present(GTK_WINDOW(window2));
		pump();
		box = first_check(view2);
		CHECK(box != NULL);
		if (box) {
			toggles = 0;
			gtk_check_button_set_active(GTK_CHECK_BUTTON(box), TRUE);
			pump();
			CHECK(toggles == 1);
			CHECK(!gtk_check_button_get_active(GTK_CHECK_BUTTON(box)));
		}
		gtk_window_destroy(GTK_WINDOW(window2));
		pump();
	}
	g_object_unref(store);
}

static int selection_changes;

static void
count_selection(GObject *selection, GParamSpec *pspec, gpointer data)
{
	(void)selection;
	(void)pspec;
	(void)data;
	selection_changes++;
}

static int activated = -1;

static void
note_activate(GtkWidget *view, guint position, gpointer data)
{
	(void)view;
	(void)data;
	activated = (int)position;
}

/* A one-column list (no header), markup, a weight per row, browse mode,
 * picking a row from the program, and the reader activating a row. */
static void
test_list_and_selection(void)
{
	GListStore *store = elim_table_new();
	GtkWidget *window = gtk_window_new();
	GtkWidget *list = gtk_list_view_new(NULL, NULL);
	ElimTextColumn spec = elim_text_column(0);
	int i;

	spec.markup = TRUE;
	spec.weight_column = 1;
	spec.pad_y = 4;
	elim_table_setup_list(list, store, &spec);
	elim_table_set_browse(list, TRUE);
	g_signal_connect(elim_table_selection(list), "notify::selected",
			 G_CALLBACK(count_selection), NULL);
	g_signal_connect(list, "activate", G_CALLBACK(note_activate), NULL);
	CHECK(elim_table_get_store(list) == store);
	CHECK(elim_table_get_selected(list) == NULL);
	CHECK(elim_table_get_selected_position(list) == GTK_INVALID_LIST_POSITION);

	for (i = 0; i < 30; i++) {
		ElimRow *row = elim_row_new(2);
		char *markup = g_strdup_printf("<b>Plan %d</b>\n<small>%d días</small>",
					       i, i);

		elim_row_set_string(row, 0, markup);
		elim_row_set_int(row, 1, i == 0 ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
		g_list_store_append(store, row);
		g_object_unref(row);
		g_free(markup);
	}

	/* browse mode: a row is picked as soon as there are rows */
	CHECK(elim_table_get_selected_position(list) == 0);
	CHECK(selection_changes >= 1);

	gtk_window_set_child(GTK_WINDOW(window), list);
	gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);
	gtk_window_present(GTK_WINDOW(window));
	pump();

	selection_changes = 0;
	elim_table_select(list, 25, TRUE);
	pump();
	CHECK(elim_table_get_selected_position(list) == 25);
	CHECK(selection_changes == 1);
	{
		ElimRow *row = elim_table_get_selected(list);

		CHECK(row && strcmp(elim_row_get_string(row, 0), "<b>Plan 25</b>\n<small>25 días</small>") == 0);
	}

	/* the reader activates a row */
	g_signal_emit_by_name(list, "activate", 3u);
	CHECK(activated == 3);

	/* a row's weight changes while it is on screen */
	{
		ElimRow *row = elim_table_get(store, 25);

		elim_row_set_int(row, 1, PANGO_WEIGHT_BOLD);
		pump();
	}

	/* out of the store: browse mode drops the selection with the rows */
	g_list_store_remove_all(store);
	pump();
	CHECK(elim_table_get_selected(list) == NULL);

	gtk_window_destroy(GTK_WINDOW(window));
	pump();
	g_object_unref(store);
}

static void
collect_labels(GtkWidget *widget, GPtrArray *labels)
{
	GtkWidget *child;

	if (GTK_IS_LABEL(widget))
		g_ptr_array_add(labels, widget);
	for (child = gtk_widget_get_first_child(widget); child;
	     child = gtk_widget_get_next_sibling(child))
		collect_labels(child, labels);
}

static int store_changes;

static void
count_store_changes(GListModel *model, guint position, guint removed, guint added,
		    gpointer data)
{
	(void)model;
	(void)position;
	(void)removed;
	(void)added;
	(void)data;
	store_changes++;
}

/* A list of two columns with no header (the search dialog's lists): the
 * columns line up from row to row, the first one wraps, and the whole list
 * can be replaced in one change. */
static void
test_rows_no_header(void)
{
	GListStore *store = elim_table_new();
	GtkWidget *window = gtk_window_new();
	GtkWidget *scroll = gtk_scrolled_window_new();
	GtkWidget *list = gtk_list_view_new(NULL, NULL);
	ElimTextColumn columns[2] = { elim_text_column(0), elim_text_column(1) };
	ElimRow *rows[3];
	GPtrArray *labels = g_ptr_array_new();
	char *long_text = g_strnfill(120, 'x');
	int width, i;

	columns[0].expand = TRUE;
	columns[0].wrap = TRUE;
	elim_table_setup_rows(list, store, columns, 2);
	CHECK(elim_table_get_store(list) == store);
	CHECK(elim_table_get_selected(list) == NULL);

	for (i = 0; i < 3; i++)
		rows[i] = elim_row_new(2);
	elim_row_set_string(rows[0], 0, "NET: Juan 3:16");
	elim_row_set_string(rows[0], 1, "ab");
	elim_row_set_string(rows[1], 0, "un versículo muy largo");
	elim_row_set_string(rows[1], 1, "a much longer second column");
	elim_row_set_string(rows[2], 0, long_text);
	elim_row_set_string(rows[2], 1, "cd");

	/* the list changes once, not once per row, and nothing is picked */
	g_signal_connect(store, "items-changed", G_CALLBACK(count_store_changes), NULL);
	g_list_store_append(store, rows[0]);
	store_changes = 0;
	elim_table_replace(store, rows, 3);
	CHECK(store_changes == 1);
	CHECK(g_list_model_get_n_items(G_LIST_MODEL(store)) == 3);
	CHECK(elim_table_get(store, 2) == rows[2]);
	CHECK(elim_table_get_selected(list) == NULL);
	for (i = 0; i < 3; i++)
		g_object_unref(rows[i]);	/* the store keeps its own */
	CHECK(g_list_model_get_n_items(G_LIST_MODEL(store)) == 3);

	/* no sideways scrolling: the list is as wide as its window, the first
	 * column wraps and the second is cut short */
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
	gtk_window_set_child(GTK_WINDOW(window), scroll);
	/* wide enough for the columns' natural widths: a window as narrow as the
	 * list can be made would squeeze the second column differently in each
	 * row, which is the ellipsis doing its work, not misaligned columns */
	gtk_window_set_default_size(GTK_WINDOW(window), 900, 300);
	gtk_window_present(GTK_WINDOW(window));
	pump();

	collect_labels(list, labels);
	CHECK(labels->len == 6);
	if (labels->len == 6) {
		GtkWidget **l = (GtkWidget **)labels->pdata;

		CHECK(strcmp(gtk_label_get_text(GTK_LABEL(l[0])), "NET: Juan 3:16") == 0);
		CHECK(strcmp(gtk_label_get_text(GTK_LABEL(l[1])), "ab") == 0);
		CHECK(strcmp(gtk_label_get_text(GTK_LABEL(l[3])),
			     "a much longer second column") == 0);

		/* the second column is as wide as its widest cell in every row */
		width = gtk_widget_get_width(l[3]);
		CHECK(width > 0);
		CHECK(gtk_widget_get_width(l[1]) == width);
		/* the row with the long first cell is aligned too, when the window
		 * has the room (a narrow one, as a window-less X server may give,
		 * makes that cell take what the second column would keep) */
		if (gtk_widget_get_width(window) >= 900)
			CHECK(gtk_widget_get_width(l[5]) == width);
		else
			CHECK(gtk_widget_get_width(l[5]) <= width);

		/* a long first cell wraps onto more lines, and the list can
		 * still be made as narrow as a small window (the minimum is what
		 * is looked at) */
		CHECK(gtk_widget_get_height(l[4]) > gtk_widget_get_height(l[0]));
		{
			int minimum = 0;

			gtk_widget_measure(scroll, GTK_ORIENTATION_HORIZONTAL, -1, &minimum,
					   NULL, NULL, NULL);
			CHECK(minimum > 0 && minimum <= 300);
		}
	}

	/* a row changed while on screen updates both cells */
	elim_row_set_string(elim_table_get(store, 0), 1, "changed");
	pump();
	CHECK(labels->len == 6 &&
	      strcmp(gtk_label_get_text(GTK_LABEL(labels->pdata[1])), "changed") == 0);

	/* picking a row works as in any table */
	elim_table_select(list, 1, FALSE);
	CHECK(elim_table_get_selected(list) == elim_table_get(store, 1));

	/* and emptying it drops the pick */
	g_list_store_remove_all(store);
	pump();
	CHECK(elim_table_get_selected(list) == NULL);

	g_ptr_array_free(labels, TRUE);
	g_free(long_text);
	gtk_window_destroy(GTK_WINDOW(window));
	pump();
	g_object_unref(store);
}

static int populated;
static int expanded_events;
static ElimRow *last_expanded;

static void
populate_lazy(GtkWidget *view, ElimRow *row, gpointer data)
{
	(void)view;
	(void)data;
	populated++;
	elim_row_set_string(elim_tree_append(NULL, row, 4), 0, "Mateo");
	elim_row_set_string(elim_tree_append(NULL, row, 4), 0, "Marcos");
}

static void
note_expanded(GtkWidget *view, ElimRow *row, gboolean expanded, gpointer data)
{
	(void)view;
	(void)data;
	expanded_events += expanded ? 1 : -1;
	last_expanded = row;
}

static guint
n_showing(GtkWidget *view)
{
	return g_list_model_get_n_items(
	    gtk_single_selection_get_model(elim_table_selection(view)));
}

static void
collect_images(GtkWidget *widget, GPtrArray *found)
{
	GtkWidget *child;

	if (GTK_IS_IMAGE(widget) || GTK_IS_CHECK_BUTTON(widget))
		g_ptr_array_add(found, widget);
	for (child = gtk_widget_get_first_child(widget); child;
	     child = gtk_widget_get_next_sibling(child))
		collect_images(child, found);
}

static void
test_tree(void)
{
	GListStore *roots = elim_table_new();
	GtkWidget *window = gtk_window_new();
	GtkWidget *scroll = gtk_scrolled_window_new();
	GtkWidget *view = elim_tree_view_new(roots);
	ElimRow *old, *genesis, *exodus, *lazy, *empty;
	GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 8, 8);
	GPtrArray *images = g_ptr_array_new();
	GtkTreeListRow *tree_row;
	ElimTextColumn spec = elim_text_column(0);

	spec.expander = TRUE;
	spec.expand = TRUE;
	elim_table_add_column(view, "Book", &spec);
	elim_table_add_image_column(view, "Icon", 1, 16, "an icon");
	elim_table_add_toggle_column_visible(view, "Pick", 2, 3, flip, NULL);
	elim_tree_set_populate(view, populate_lazy, NULL);
	elim_tree_set_expanded_func(view, note_expanded, NULL);

	old = elim_tree_append(roots, NULL, 4);
	elim_row_set_string(old, 0, "Antiguo Testamento");
	genesis = elim_tree_append(roots, old, 4);
	elim_row_set_string(genesis, 0, "Genesis");
	elim_row_set_pixbuf(genesis, 1, pixbuf);
	elim_row_set_int(genesis, 3, 1);
	exodus = elim_tree_append(roots, old, 4);
	elim_row_set_string(exodus, 0, "Exodo");
	elim_row_set_string(exodus, 1, "emblem-default");
	lazy = elim_tree_append(roots, NULL, 4);
	elim_row_set_string(lazy, 0, "Nuevo Testamento");
	elim_row_set_lazy(lazy, TRUE);
	empty = elim_tree_append(roots, NULL, 4);
	elim_row_set_string(empty, 0, "Apocrifos");

	/* the shape of the tree */
	CHECK(elim_row_get_parent(genesis) == old && elim_row_get_parent(old) == NULL);
	CHECK(elim_row_n_children(old) == 2 && elim_row_get_child(old, 1) == exodus);
	CHECK(elim_row_n_children(empty) == 0 && elim_row_get_depth(exodus) == 1);
	CHECK(elim_row_get_siblings(exodus, roots) == elim_row_get_siblings(genesis, roots));
	CHECK(elim_row_get_siblings(old, roots) == roots);
	CHECK(elim_row_is_lazy(lazy) && !elim_row_is_lazy(old));
	CHECK(elim_table_find(elim_row_get_siblings(exodus, roots), exodus) == 1);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
	gtk_window_set_child(GTK_WINDOW(window), scroll);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
	gtk_window_present(GTK_WINDOW(window));
	pump();

	/* only the roots show, and none is picked */
	CHECK(n_showing(view) == 3);
	CHECK(elim_table_row_at(view, 0) == old && elim_table_row_at(view, 2) == empty);
	CHECK(elim_table_row_at(view, 3) == NULL);
	CHECK(elim_table_get_selected(view) == NULL);
	CHECK(elim_table_get_store(view) == roots);
	CHECK(!elim_tree_row_expanded(view, old));

	/* opening a row shows its children right after it */
	elim_tree_expand_row(view, old, FALSE);
	pump();
	CHECK(n_showing(view) == 5);
	CHECK(elim_tree_row_expanded(view, old));
	CHECK(elim_table_row_at(view, 1) == genesis && elim_table_row_at(view, 2) == exodus);
	CHECK(elim_table_row_at(view, 3) == lazy);

	/* the cells show an image (a texture, and a themed icon) and the box
	 * only where its row asks for it */
	g_ptr_array_set_size(images, 0);
	collect_images(view, images);
	{
		guint i, with_paintable = 0, with_icon = 0, boxes = 0, boxes_shown = 0;

		for (i = 0; i < images->len; i++) {
			GtkWidget *widget = images->pdata[i];

			if (GTK_IS_CHECK_BUTTON(widget)) {
				boxes++;
				boxes_shown += gtk_widget_get_opacity(widget) > 0.5 ? 1 : 0;
			} else if (gtk_image_get_storage_type(GTK_IMAGE(widget)) ==
				   GTK_IMAGE_PAINTABLE) {
				with_paintable++;
			} else if (gtk_image_get_storage_type(GTK_IMAGE(widget)) ==
				   GTK_IMAGE_ICON_NAME) {
				with_icon++;
			}
		}
		CHECK(with_paintable == 1);
		CHECK(with_icon == 1);
		/* four rows show, and only Genesis' box (its column 3 is 1) does */
		CHECK(boxes == 5 && boxes_shown == 1);
	}

	/* a row whose children are still to be made makes them once, when opened */
	CHECK(populated == 0);
	elim_tree_expand_row(view, lazy, FALSE);
	pump();
	CHECK(populated == 1 && !elim_row_is_lazy(lazy));
	CHECK(elim_row_n_children(lazy) == 2 && n_showing(view) == 7);
	elim_tree_collapse_row(view, lazy);
	pump();
	CHECK(n_showing(view) == 5 && !elim_tree_row_expanded(view, lazy));
	elim_tree_expand_row(view, lazy, FALSE);
	pump();
	CHECK(populated == 1 && n_showing(view) == 7);

	/* the reader opens and closes a row: the view is told, and a lazy row
	 * is made then too */
	elim_tree_collapse_all(view);
	pump();
	CHECK(n_showing(view) == 3);
	expanded_events = 0;
	tree_row = GTK_TREE_LIST_ROW(g_list_model_get_item(
	    gtk_single_selection_get_model(elim_table_selection(view)), 0));
	gtk_tree_list_row_set_expanded(tree_row, TRUE);
	pump();
	CHECK(expanded_events == 1 && last_expanded == old);
	gtk_tree_list_row_set_expanded(tree_row, FALSE);
	pump();
	CHECK(expanded_events == 0);
	g_object_unref(tree_row);

	/* picking a row inside a closed one opens what is above it */
	CHECK(!elim_tree_row_expanded(view, old));
	CHECK(elim_tree_select_row(view, exodus, TRUE));
	pump();
	CHECK(elim_tree_row_expanded(view, old));
	CHECK(elim_table_get_selected(view) == exodus);
	CHECK(elim_table_get_selected_position(view) == 2);
	elim_tree_unselect(view);
	CHECK(elim_table_get_selected(view) == NULL);

	elim_tree_expand_all(view);
	pump();
	CHECK(n_showing(view) == 7);

	/* a row that is removed goes with what is under it */
	CHECK(elim_tree_remove(roots, exodus));
	pump();
	CHECK(elim_row_n_children(old) == 1 && n_showing(view) == 6);
	CHECK(!elim_tree_remove(roots, exodus));
	CHECK(elim_tree_remove(roots, lazy));
	pump();
	CHECK(n_showing(view) == 3);

	/* inserting after a sibling keeps the order */
	{
		ElimRow *between = elim_tree_insert_after(roots, old, genesis, 4);
		ElimRow *first = elim_tree_prepend(roots, old, 4);

		CHECK(elim_row_get_child(old, 0) == first);
		CHECK(elim_row_get_child(old, 1) == genesis);
		CHECK(elim_row_get_child(old, 2) == between);
	}

	gtk_window_destroy(GTK_WINDOW(window));
	pump();

	/* a child that outlives its parent does not keep pointing at it */
	{
		GListStore *store = elim_table_new();
		ElimRow *parent = elim_tree_append(store, NULL, 1);
		ElimRow *child = g_object_ref(elim_tree_append(store, parent, 1));

		CHECK(elim_row_get_parent(child) == parent);
		g_list_store_remove_all(store);
		CHECK(elim_row_get_parent(child) == NULL);
		g_object_unref(child);
		g_object_unref(store);
	}

	g_ptr_array_free(images, TRUE);
	g_object_unref(pixbuf);
	g_object_unref(roots);
}

static void
collect_expanders(GtkWidget *widget, GPtrArray *found)
{
	GtkWidget *child;

	if (GTK_IS_TREE_EXPANDER(widget))
		g_ptr_array_add(found, widget);
	for (child = gtk_widget_get_first_child(widget); child;
	     child = gtk_widget_get_next_sibling(child))
		collect_expanders(child, found);
}

/* A tree on a GtkListView, filled and opened before it is shown: the order in
 * which the dialogs build theirs. */
static void
test_tree_list_view(void)
{
	GListStore *roots = elim_table_new();
	GtkWidget *view = gtk_list_view_new(NULL, NULL);
	GtkWidget *window = gtk_window_new();
	ElimTextColumn spec = elim_text_column(0);
	guint g, f;

	gtk_window_set_child(GTK_WINDOW(window), view);
	elim_tree_setup_list(view, roots, &spec);
	for (g = 0; g < 3; g++) {
		ElimRow *group = elim_tree_append(roots, NULL, 1);

		elim_row_set_string(group, 0, "group");
		for (f = 0; f < 2; f++)
			elim_row_set_string(elim_tree_append(roots, group, 1), 0, "leaf");
	}
	elim_tree_expand_all(view);
	CHECK(n_showing(view) == 9);
	gtk_window_present(GTK_WINDOW(window));
	pump();
	CHECK(n_showing(view) == 9);
	elim_tree_collapse_all(view);
	CHECK(n_showing(view) == 3);
	CHECK(elim_tree_select_row(view, elim_row_get_child(elim_table_get(roots, 2), 1), FALSE));
	CHECK(n_showing(view) == 5 && elim_table_get_selected_position(view) == 4);

	/* the arrow shows for a row with children only, and follows a leaf that
	 * gets its first child while it is on screen */
	{
		GPtrArray *expanders = g_ptr_array_new();
		ElimRow *leaf = elim_row_get_child(elim_table_get(roots, 2), 1);
		guint i, arrows = 0;

		elim_tree_expand_all(view);
		pump();
		collect_expanders(view, expanders);
		for (i = 0; i < expanders->len; i++)
			arrows += gtk_tree_expander_get_hide_expander(expanders->pdata[i]) ? 0 : 1;
		CHECK(expanders->len == 9 && arrows == 3);
		elim_tree_append(roots, leaf, 1);
		pump();
		g_ptr_array_set_size(expanders, 0);
		collect_expanders(view, expanders);
		arrows = 0;
		for (i = 0; i < expanders->len; i++)
			arrows += gtk_tree_expander_get_hide_expander(expanders->pdata[i]) ? 0 : 1;
		CHECK(arrows == 4);
		elim_row_set_lazy(elim_table_get(roots, 0), FALSE);
		g_ptr_array_free(expanders, TRUE);
	}
	gtk_window_destroy(GTK_WINDOW(window));
	pump();
	g_object_unref(roots);
}

static gboolean
prefix_match(const char *key, const char *candidate, gpointer data)
{
	(void)data;
	return g_str_has_prefix(candidate, key);
}

static void
test_entry_suggest(void)
{
	GtkWidget *window = gtk_window_new();
	GtkWidget *entry = gtk_entry_new();
	GtkWidget *other = gtk_button_new_with_label("elsewhere");
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GList *candidates = NULL;
	ElimEntrySuggest *suggest;
	GtkWidget *list;
	GListStore *shown;
	int activations = 0;
	guint i;

	for (i = 0; i < 20; i++)
		candidates = g_list_append(candidates, g_strdup_printf("Adon%02u", i));
	candidates = g_list_append(candidates, g_strdup("Elohim"));

	gtk_box_append(GTK_BOX(box), other);
	gtk_box_append(GTK_BOX(box), entry);
	gtk_window_set_child(GTK_WINDOW(window), box);
	suggest = elim_entry_suggest_new(entry, prefix_match, NULL);
	elim_entry_suggest_set_candidates(suggest, candidates);
	g_signal_connect_swapped(entry, "activate", G_CALLBACK(g_atomic_int_inc), &activations);
	list = elim_entry_suggest_get_list(suggest);
	shown = elim_table_get_store(list);
	gtk_window_present(GTK_WINDOW(window));
	pump();

	/* a program that fills the entry in is not the reader typing */
	gtk_widget_grab_focus(other);
	pump();
	gtk_editable_set_text(GTK_EDITABLE(entry), "Adon");
	pump();
	CHECK(!elim_entry_suggest_is_open(suggest));

	/* typing in it lists what matches, at most a dozen */
	gtk_widget_grab_focus(entry);
	pump();
	gtk_editable_set_text(GTK_EDITABLE(entry), "Adon0");
	pump();
	CHECK(elim_entry_suggest_is_open(suggest));
	CHECK(g_list_model_get_n_items(G_LIST_MODEL(shown)) == 10);
	gtk_editable_set_text(GTK_EDITABLE(entry), "Ad");
	pump();
	CHECK(g_list_model_get_n_items(G_LIST_MODEL(shown)) == 12);
	CHECK(elim_table_get_selected(list) == NULL);
	gtk_editable_set_text(GTK_EDITABLE(entry), "Zz");
	pump();
	CHECK(!elim_entry_suggest_is_open(suggest));
	gtk_editable_set_text(GTK_EDITABLE(entry), "");
	pump();
	CHECK(!elim_entry_suggest_is_open(suggest));

	/* Enter with none picked is the entry's own */
	gtk_editable_set_text(GTK_EDITABLE(entry), "Ado");
	pump();
	CHECK(elim_entry_suggest_is_open(suggest));
	g_signal_emit_by_name(entry, "activate");
	CHECK(activations == 1 && elim_entry_suggest_is_open(suggest));

	/* Down picks the first, Up unpicks; Enter takes the picked one and is
	 * not the entry's activation */
	elim_table_select(list, 2, FALSE);
	g_signal_emit_by_name(entry, "activate");
	CHECK(activations == 1);
	CHECK(strcmp(gtk_editable_get_text(GTK_EDITABLE(entry)), "Adon02") == 0);
	CHECK(!elim_entry_suggest_is_open(suggest));

	/* a click on a suggestion (the list's activation) takes it too */
	gtk_editable_set_text(GTK_EDITABLE(entry), "E");
	pump();
	CHECK(elim_entry_suggest_is_open(suggest));
	g_signal_emit_by_name(list, "activate", 0u);
	CHECK(strcmp(gtk_editable_get_text(GTK_EDITABLE(entry)), "Elohim") == 0);
	CHECK(!elim_entry_suggest_is_open(suggest));

	g_list_free_full(candidates, g_free);
	gtk_window_destroy(GTK_WINDOW(window));
	pump();
}

static void
collect_type(GtkWidget *widget, GType type, GPtrArray *found)
{
	GtkWidget *child;

	if (G_TYPE_CHECK_INSTANCE_TYPE(widget, type))
		g_ptr_array_add(found, widget);
	for (child = gtk_widget_get_first_child(widget); child;
	     child = gtk_widget_get_next_sibling(child))
		collect_type(child, type, found);
}

/* A tree of rows on a GtkListView, no header: a check box that can show its
 * mixed state, a name, a progress bar and an image side by side behind the
 * expander, the way the book pickers and the reading progress list are. */
static void
test_tree_rows(void)
{
	GListStore *roots = elim_table_new();
	GtkWidget *view = gtk_list_view_new(NULL, NULL);
	GtkWidget *window = gtk_window_new();
	ElimColumn cols[4];
	ElimRow *group, *first, *second;
	GPtrArray *found = g_ptr_array_new();
	guint i, mixed, active, bars, shown;

	cols[0] = elim_column_toggle(0, flip, NULL);
	cols[0].inconsistent_column = 1;
	cols[0].visible_column = 6;
	cols[1] = elim_column_text(2);
	cols[1].text.expand = TRUE;
	cols[2] = elim_column_progress(3, 4);
	cols[3] = elim_column_image(5, 16, "state");
	gtk_window_set_child(GTK_WINDOW(window), view);
	elim_tree_setup_row_columns(view, roots, cols, 4);

	group = elim_tree_append(roots, NULL, 7);
	elim_row_set_int(group, 1, 1);
	elim_row_set_string(group, 2, "Pentateuco");
	elim_row_set_int(group, 3, 50);
	elim_row_set_string(group, 4, "1 de 2");
	elim_row_set_string(group, 5, "emblem-default");
	elim_row_set_int(group, 6, 1);
	first = elim_tree_append(roots, group, 7);
	elim_row_set_int(first, 0, 1);
	elim_row_set_string(first, 2, "Genesis");
	elim_row_set_int(first, 3, 100);
	elim_row_set_string(first, 4, "50 de 50");
	elim_row_set_int(first, 6, 1);
	second = elim_tree_append(roots, group, 7);
	elim_row_set_string(second, 2, "Exodo");
	elim_row_set_int(second, 6, 0);	/* no box for this one */
	elim_tree_expand_all(view);
	gtk_window_present(GTK_WINDOW(window));
	pump();

	CHECK(n_showing(view) == 3);
	collect_type(view, GTK_TYPE_CHECK_BUTTON, found);
	mixed = active = shown = 0;
	for (i = 0; i < found->len; i++) {
		GtkCheckButton *check = found->pdata[i];

		mixed += gtk_check_button_get_inconsistent(check) ? 1 : 0;
		active += gtk_check_button_get_active(check) ? 1 : 0;
		shown += gtk_widget_get_opacity(GTK_WIDGET(check)) > 0.5 ? 1 : 0;
	}
	/* three boxes, all made; the group's is mixed, Genesis' is on, and the
	 * third is blank but still there so that the names line up */
	CHECK(found->len == 3 && mixed == 1 && active == 1 && shown == 2);

	g_ptr_array_set_size(found, 0);
	collect_type(view, GTK_TYPE_PROGRESS_BAR, found);
	bars = 0;
	for (i = 0; i < found->len; i++) {
		GtkProgressBar *bar = found->pdata[i];

		if (strcmp(gtk_progress_bar_get_text(bar), "50 de 50") == 0)
			CHECK(gtk_progress_bar_get_fraction(bar) == 1.0);
		if (strcmp(gtk_progress_bar_get_text(bar), "1 de 2") == 0)
			CHECK(gtk_progress_bar_get_fraction(bar) == 0.5);
		bars++;
	}
	CHECK(bars == 3);

	/* a row changed while it shows moves its cells */
	elim_row_set_int(group, 3, 75);
	elim_row_set_int(group, 1, 0);
	elim_row_set_int(second, 6, 1);
	pump();
	g_ptr_array_set_size(found, 0);
	collect_type(view, GTK_TYPE_CHECK_BUTTON, found);
	mixed = shown = 0;
	for (i = 0; i < found->len; i++) {
		mixed += gtk_check_button_get_inconsistent(found->pdata[i]) ? 1 : 0;
		shown += gtk_widget_get_opacity(found->pdata[i]) > 0.5 ? 1 : 0;
	}
	CHECK(mixed == 0 && shown == 3);

	/* a click on a box runs the function the column was given */
	g_ptr_array_set_size(found, 0);
	collect_type(view, GTK_TYPE_CHECK_BUTTON, found);
	toggles = 0;
	for (i = 0; i < found->len; i++)
		if (!gtk_check_button_get_active(found->pdata[i]) &&
		    gtk_widget_get_opacity(found->pdata[i]) > 0.5) {
			gtk_check_button_set_active(found->pdata[i], TRUE);
			break;
		}
	pump();
	CHECK(toggles == 1);

	g_ptr_array_free(found, TRUE);
	gtk_window_destroy(GTK_WINDOW(window));
	pump();
	g_object_unref(roots);
}

static int tree_changes;

static void
count_tree_change(GListStore *roots, gpointer data)
{
	(void)roots;
	(void)data;
	tree_changes++;
}

static void
test_tree_move(void)
{
	GListStore *roots = elim_table_new();
	GtkWidget *view = gtk_list_view_new(NULL, NULL);
	GtkWidget *window = gtk_window_new();
	ElimTextColumn spec = elim_text_column(0);
	ElimRow *top, *a, *a1, *a2, *b, *b1, *c;

	gtk_window_set_child(GTK_WINDOW(window), view);
	elim_tree_setup_list(view, roots, &spec);
	top = elim_tree_append(roots, NULL, 1);
	a = elim_tree_append(roots, top, 1);
	a1 = elim_tree_append(roots, a, 1);
	a2 = elim_tree_append(roots, a, 1);
	b = elim_tree_append(roots, top, 1);
	b1 = elim_tree_append(roots, b, 1);
	c = elim_tree_append(roots, top, 1);
	elim_row_set_string(top, 0, "top");
	elim_row_set_string(a, 0, "a");
	elim_row_set_string(a1, 0, "a1");
	elim_row_set_string(a2, 0, "a2");
	elim_row_set_string(b, 0, "b");
	elim_row_set_string(b1, 0, "b1");
	elim_row_set_string(c, 0, "c");
	elim_tree_expand_all(view);
	gtk_window_present(GTK_WINDOW(window));
	pump();
	CHECK(n_showing(view) == 7);

	/* a change of the tree, however many rows, is told once when the main
	 * loop is next reached */
	elim_tree_set_changed_func(roots, count_tree_change, NULL);
	tree_changes = 0;
	elim_row_set_string(c, 0, "c!");
	elim_row_set_int(c, 0, 1);
	elim_row_set_string(c, 0, "c");
	CHECK(tree_changes == 0);
	pump();
	CHECK(tree_changes == 1);
	pump();
	CHECK(tree_changes == 1);

	/* a row cannot go onto itself or into what it holds */
	CHECK(!elim_tree_move_row(view, a, a, ELIM_DROP_INTO));
	CHECK(!elim_tree_move_row(view, a, a1, ELIM_DROP_INTO));
	CHECK(!elim_tree_move_row(view, a, a2, ELIM_DROP_AFTER));
	CHECK(elim_row_n_children(a) == 2);

	/* after a sibling further down: the order follows, and the open rows stay
	 * open where they land */
	tree_changes = 0;
	CHECK(elim_tree_move_row(view, a, c, ELIM_DROP_AFTER));
	pump();
	CHECK(tree_changes == 1);
	CHECK(elim_row_get_child(top, 0) == b && elim_row_get_child(top, 1) == c &&
	      elim_row_get_child(top, 2) == a);
	CHECK(elim_row_get_parent(a) == top && elim_tree_row_expanded(view, a));
	CHECK(n_showing(view) == 7);
	CHECK(elim_table_get_selected(view) == a);

	/* before a sibling: it lands ahead of it, not behind */
	CHECK(elim_tree_move_row(view, a, b, ELIM_DROP_BEFORE));
	CHECK(elim_row_get_child(top, 0) == a && elim_row_get_child(top, 1) == b);

	/* into a row: as its last child, and the row opens to show it */
	elim_tree_collapse_row(view, b);
	CHECK(elim_tree_move_row(view, c, b, ELIM_DROP_INTO));
	pump();
	CHECK(elim_row_n_children(b) == 2 && elim_row_get_child(b, 1) == c);
	CHECK(elim_row_get_parent(c) == b && elim_tree_row_expanded(view, b));
	CHECK(elim_row_n_children(top) == 2);

	/* into a row that had no child, which was a leaf until now */
	CHECK(elim_tree_move_row(view, b1, c, ELIM_DROP_INTO));
	CHECK(elim_row_n_children(c) == 1 && elim_row_get_parent(b1) == c);
	pump();
	CHECK(elim_tree_row_expanded(view, c));
	CHECK(n_showing(view) == 7);

	/* out to the top of the tree */
	CHECK(elim_tree_move_row(view, a2, top, ELIM_DROP_AFTER));
	CHECK(elim_row_get_parent(a2) == NULL && elim_table_get(roots, 1) == a2);

	/* the drag has to be allowed */
	CHECK(!elim_tree_get_reorderable(view));
	elim_tree_set_reorderable(view, TRUE);
	CHECK(elim_tree_get_reorderable(view));

	gtk_window_destroy(GTK_WINDOW(window));
	pump();
	g_object_unref(roots);
}

int
main(void)
{
	if (!gtk_init_check()) {
		fprintf(stderr, "no display: skipped\n");
		return 77;
	}
	test_rows();
	test_sortable_view();
	test_move_row();
	test_view();
	test_toggle_and_tooltip();
	test_list_and_selection();
	test_rows_no_header();
	test_tree();
	test_tree_list_view();
	test_tree_rows();
	test_tree_move();
	test_entry_suggest();
	printf("table_helpers_test failures=%d\n", failures);
	return failures ? 1 : 0;
}
