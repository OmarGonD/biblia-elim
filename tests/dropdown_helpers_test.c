/*
 * gui/dropdown_helpers.h: a GtkDropDown answering like the GtkComboBoxText
 * the interface used before. Real widgets, so it runs under a display
 * (xvfb-run); nothing here needs a window to be shown.
 */
#include "gui/dropdown_helpers.h"

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

static int changes;

static void
count_change(GObject *dd, GParamSpec *pspec, gpointer data)
{
	(void)dd;
	(void)pspec;
	(void)data;
	changes++;
}

static gchar *
search_text(GtkStringObject *row, gpointer data)
{
	(void)data;
	return g_strdup_printf("%s abbreviation", gtk_string_object_get_string(row));
}

static void
test_text_and_ids(void)
{
	GtkDropDown *dd = GTK_DROP_DOWN(elim_dropdown_new());

	CHECK(elim_dropdown_n_items(dd) == 0);
	CHECK(elim_dropdown_get_active(dd) == -1);
	CHECK(elim_dropdown_get_active_text(dd) == NULL);
	CHECK(elim_dropdown_get_active_id(dd) == NULL);
	CHECK(strcmp(elim_dropdown_get_active_text_or_empty(dd), "") == 0);

	elim_dropdown_append(dd, "sp", "Español");
	elim_dropdown_append(dd, NULL, "no id");
	elim_dropdown_append(dd, "en", "English");
	CHECK(elim_dropdown_n_items(dd) == 3);

	/* the first row is selected by appending it, as documented */
	CHECK(elim_dropdown_get_active(dd) == 0);
	CHECK(strcmp(elim_dropdown_get_active_id(dd), "sp") == 0);
	CHECK(strcmp(elim_dropdown_get_active_text(dd), "Español") == 0);

	elim_dropdown_set_active(dd, 1);
	CHECK(strcmp(elim_dropdown_get_active_text(dd), "no id") == 0);
	CHECK(elim_dropdown_get_active_id(dd) == NULL);

	CHECK(elim_dropdown_set_active_id(dd, "en"));
	CHECK(elim_dropdown_get_active(dd) == 2);
	CHECK(!elim_dropdown_set_active_id(dd, "nope"));
	CHECK(elim_dropdown_get_active(dd) == 2);	/* unchanged */

	CHECK(elim_dropdown_find_text(dd, "no id") == 1);
	CHECK(elim_dropdown_find_text(dd, "missing") == -1);
	CHECK(elim_dropdown_find_text(dd, NULL) == -1);
	CHECK(strcmp(elim_dropdown_get_text(dd, 0), "Español") == 0);
	CHECK(elim_dropdown_get_text(dd, 3) == NULL);

	/* a dropdown that has rows cannot be left with none selected: a row
	 * that is not there changes nothing */
	elim_dropdown_set_active(dd, -1);
	CHECK(elim_dropdown_get_active(dd) == 2);
	elim_dropdown_set_active(dd, 99);
	CHECK(elim_dropdown_get_active(dd) == 2);
	CHECK(!elim_dropdown_set_active_id(dd, NULL));

	/* removing every row leaves none selected and no stale ids behind */
	elim_dropdown_set_active(dd, 2);
	elim_dropdown_remove_all(dd);
	CHECK(elim_dropdown_n_items(dd) == 0);
	CHECK(elim_dropdown_get_active(dd) == -1);
	elim_dropdown_append(dd, NULL, "fresh");
	CHECK(elim_dropdown_get_active_id(dd) == NULL);
	CHECK(!elim_dropdown_set_active_id(dd, "sp"));

	g_object_ref_sink(dd);
	g_object_unref(dd);
}

/* Rows a GtkBuilder file lists carry no id, and rows appended after them
 * must not inherit the wrong one. */
static void
test_rows_from_a_builder(void)
{
	const char *ui =
	    "<interface><object class='GtkDropDown' id='dd'>"
	    "<property name='model'><object class='GtkStringList'><items>"
	    "<item>-1</item><item>+0</item></items></object></property>"
	    "</object></interface>";
	GtkBuilder *builder = gtk_builder_new_from_string(ui, -1);
	GtkDropDown *dd = GTK_DROP_DOWN(gtk_builder_get_object(builder, "dd"));

	elim_dropdown_prepare(dd);
	CHECK(elim_dropdown_n_items(dd) == 2);
	CHECK(strcmp(elim_dropdown_get_active_text(dd), "-1") == 0);
	elim_dropdown_append(dd, "big", "+5");
	CHECK(elim_dropdown_set_active_id(dd, "big"));
	CHECK(elim_dropdown_get_active(dd) == 2);
	elim_dropdown_set_active(dd, 0);
	CHECK(elim_dropdown_get_active_id(dd) == NULL);

	g_object_unref(builder);
}

static void
test_change_signal(void)
{
	GtkDropDown *dd = GTK_DROP_DOWN(elim_dropdown_new());
	gulong id;

	elim_dropdown_append(dd, NULL, "a");
	elim_dropdown_append(dd, NULL, "b");
	changes = 0;
	id = g_signal_connect(dd, "notify::selected", G_CALLBACK(count_change),
			      NULL);
	elim_dropdown_set_active(dd, 1);
	CHECK(changes == 1);
	elim_dropdown_set_active(dd, 1);	/* same row: nothing to report */
	CHECK(changes == 1);
	g_signal_handler_block(dd, id);
	elim_dropdown_remove_all(dd);
	elim_dropdown_append(dd, NULL, "c");
	g_signal_handler_unblock(dd, id);
	CHECK(changes == 1);	/* a blocked refill is silent */

	g_object_ref_sink(dd);
	g_object_unref(dd);
}

static void
test_search(void)
{
	GtkDropDown *books = GTK_DROP_DOWN(elim_dropdown_new());
	GtkDropDown *plain = GTK_DROP_DOWN(elim_dropdown_new());

	elim_dropdown_append(books, NULL, "Juan");
	elim_dropdown_append(plain, NULL, "Juan");
	elim_dropdown_enable_search(plain);
	CHECK(gtk_drop_down_get_enable_search(plain));
	elim_dropdown_enable_search_with(books, G_CALLBACK(search_text), NULL);
	CHECK(gtk_drop_down_get_enable_search(books));
	CHECK(gtk_drop_down_get_factory(books) != NULL);

	/* the expression is what the reader's typing is matched against */
	{
		GtkStringObject *row = gtk_string_object_new("Juan");
		GValue value = G_VALUE_INIT;

		CHECK(gtk_expression_evaluate(gtk_drop_down_get_expression(books),
					      row, &value));
		CHECK(strcmp(g_value_get_string(&value), "Juan abbreviation") == 0);
		g_value_unset(&value);
		g_object_unref(row);
	}
	CHECK(strcmp(elim_dropdown_get_active_text(books), "Juan") == 0);

	g_object_ref_sink(books);
	g_object_unref(books);
	g_object_ref_sink(plain);
	g_object_unref(plain);
}

int
main(void)
{
	if (!gtk_init_check()) {
		fprintf(stderr, "no display: skipped\n");
		return 77;
	}
	test_text_and_ids();
	test_rows_from_a_builder();
	test_change_signal();
	test_search();
	printf("dropdown_helpers_test failures=%d\n", failures);
	return failures ? 1 : 0;
}
