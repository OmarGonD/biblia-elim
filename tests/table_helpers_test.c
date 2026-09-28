/*
 * gui/table_helpers.h: rows of string/int columns in a GtkColumnView.
 * Real widgets, so it runs under a display (xvfb-run). The view is put in
 * a window and shown, so its cells are made and bound like the app's are.
 */
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
	CHECK(strcmp(text, "7.5") == 0);
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
	ElimTextColumn name = elim_text_column(0);
	ElimTextColumn value = elim_text_column(1);
	GtkColumnViewColumn *numbers;
	ElimRow *first = elim_row_new(2);
	ElimRow *second = elim_row_new(2);

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
	pump();
	{
		GtkSelectionModel *selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(view));
		ElimRow *row = ELIM_ROW(g_list_model_get_item(G_LIST_MODEL(selection), 0));
		CHECK(row && elim_row_get_double(row, 1) == 2.0);
		g_object_unref(row);
	}
	g_object_unref(view);
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
	gtk_window_set_default_size(GTK_WINDOW(window), 300, 300);
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
		CHECK(gtk_widget_get_width(l[5]) == width);

		/* a long first cell wraps onto more lines, and the list can
		 * still be made as narrow as a small window (the window of
		 * this test has no window manager and opens at the list's
		 * natural width, so the minimum is what is looked at) */
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
	printf("table_helpers_test failures=%d\n", failures);
	return failures ? 1 : 0;
}
