/*
 * NOTES-STORE-101: the reader's notes in their own file.
 */
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include "main/notes_store.h"

static gchar *dir;
static gchar *path;

static gchar *read_file(const gchar *p)
{
	gchar *contents = NULL;
	g_assert_true(g_file_get_contents(p, &contents, NULL, NULL));
	return contents;
}

static void test_roundtrip_and_order(void)
{
	/* A value with every character an attribute has to escape, and the
	 * newlines a multi-paragraph note carries. */
	const gchar *tricky = "«Dijo» <b>&\"'\n\nsegundo párrafo\ttab";

	g_assert_false(notes_store_open(path));
	g_assert_cmpuint(notes_store_count(NOTES_SECTION_NOTES), ==, 0);
	notes_store_set(NOTES_SECTION_NOTES, "SpaRV Ps.23.1", "a");
	notes_store_set(NOTES_SECTION_NOTES, "SpaPlatense Ps.22.1", tricky);
	notes_store_set(NOTES_SECTION_NOTES, "SpaRV Gen.1.1#42", "b");
	notes_store_set(NOTES_SECTION_LINKS, "l1", "MV:Ps.23.1|HL:42");
	notes_store_set(NOTES_SECTION_NOTES, "SpaRV Ps.23.1", "a2"); /* update */
	notes_store_remove(NOTES_SECTION_NOTES, "SpaRV Gen.1.1#42");
	g_assert_true(notes_store_save());

	g_assert_true(notes_store_open(path));
	GPtrArray *e = notes_store_entries(NOTES_SECTION_NOTES);
	g_assert_cmpuint(e->len, ==, 2);
	g_assert_cmpstr(((NotesEntry *)e->pdata[0])->label, ==, "SpaRV Ps.23.1");
	g_assert_cmpstr(((NotesEntry *)e->pdata[0])->value, ==, "a2");
	g_assert_cmpstr(((NotesEntry *)e->pdata[1])->value, ==, tricky);
	g_ptr_array_unref(e);
	gchar *link = notes_store_get(NOTES_SECTION_LINKS, "l1");
	g_assert_cmpstr(link, ==, "MV:Ps.23.1|HL:42");
	g_free(link);
	g_assert_null(notes_store_get(NOTES_SECTION_NOTES, "SpaRV Gen.1.1#42"));
}

static void test_entries_are_a_snapshot(void)
{
	g_assert_true(notes_store_open(path));
	GPtrArray *e = notes_store_entries(NOTES_SECTION_NOTES);
	/* Removing while walking the snapshot is safe. */
	for (guint i = 0; i < e->len; i++)
		notes_store_remove(NOTES_SECTION_NOTES,
				   ((NotesEntry *)e->pdata[i])->label);
	g_assert_cmpuint(e->len, ==, 2);
	g_assert_cmpuint(notes_store_count(NOTES_SECTION_NOTES), ==, 0);
	g_ptr_array_unref(e);
	notes_store_close(); /* not saved: the file keeps two notes */
	g_assert_true(notes_store_open(path));
	g_assert_cmpuint(notes_store_count(NOTES_SECTION_NOTES), ==, 2);
}

static void test_backups_rotate_once_per_session(void)
{
	gchar *bak1 = g_strdup_printf("%s.bak.1", path);
	gchar *bak6 = g_strdup_printf("%s.bak.%d", path,
				      NOTES_STORE_BACKUPS + 1);
	gchar *before, *after;

	g_assert_true(notes_store_open(path));
	before = read_file(path);
	notes_store_set(NOTES_SECTION_NOTES, "x", "1");
	g_assert_true(notes_store_save());
	after = read_file(bak1);
	g_assert_cmpstr(after, ==, before);	/* the pre-session file */
	g_free(after);
	notes_store_set(NOTES_SECTION_NOTES, "x", "2");
	g_assert_true(notes_store_save());
	after = read_file(bak1);
	g_assert_cmpstr(after, ==, before);	/* not rotated again */
	g_free(after);
	g_free(before);

	for (int session = 0; session < NOTES_STORE_BACKUPS + 3; session++) {
		g_assert_true(notes_store_open(path));
		gchar *v = g_strdup_printf("%d", session);
		notes_store_set(NOTES_SECTION_NOTES, "x", v);
		g_free(v);
		g_assert_true(notes_store_save());
	}
	g_assert_false(g_file_test(bak6, G_FILE_TEST_EXISTS));
	for (int i = 1; i <= NOTES_STORE_BACKUPS; i++) {
		gchar *b = g_strdup_printf("%s.bak.%d", path, i);
		g_assert_true(g_file_test(b, G_FILE_TEST_EXISTS));
		g_free(b);
	}
	g_free(bak1);
	g_free(bak6);
}

static void test_no_temporary_files_left(void)
{
	GDir *d = g_dir_open(dir, 0, NULL);
	const gchar *name;
	while ((name = g_dir_read_name(d)))
		g_assert_true(g_str_has_prefix(name, "notas.xml"));
	g_dir_close(d);
	d = g_dir_open(dir, 0, NULL);
	while ((name = g_dir_read_name(d)))
		g_assert_true(!strcmp(name, "notas.xml") ||
			      g_str_has_prefix(name, "notas.xml.bak."));
	g_dir_close(d);
}

static void test_unreadable_file_is_set_aside(void)
{
	gchar *bad = g_build_filename(dir, "roto.xml", NULL);
	g_assert_true(g_file_set_contents(bad, "<notas><note label=", -1, NULL));
	g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*no se pudo leer*");
	g_assert_false(notes_store_open(bad));
	g_test_assert_expected_messages();
	g_assert_false(g_file_test(bad, G_FILE_TEST_EXISTS));
	gboolean aside = FALSE;
	GDir *d = g_dir_open(dir, 0, NULL);
	const gchar *name;
	while ((name = g_dir_read_name(d)))
		if (g_str_has_prefix(name, "roto.xml.ilegible-"))
			aside = TRUE;
	g_dir_close(d);
	g_assert_true(aside);
	g_free(bad);
	notes_store_close();
}

/* NOTES-DATES-101: the store stamps what it is given. */
static void test_dates(void)
{
	gchar *dpath = g_build_filename(dir, "fechas.xml", NULL);
	gint64 before = g_get_real_time() / G_USEC_PER_SEC;
	NotesEntry *e;
	guint64 gen;

	g_assert_false(notes_store_open(dpath));
	notes_store_set(NOTES_SECTION_NOTES, "SpaRV Ps.23.1", "a");
	e = notes_store_lookup(NOTES_SECTION_NOTES, "SpaRV Ps.23.1");
	g_assert_nonnull(e);
	g_assert_cmpint(e->created, >=, before);
	g_assert_cmpint(e->modified, ==, e->created);
	notes_entry_free(e);

	/* An old note (imported, or dated before this session)... */
	notes_store_set_full(NOTES_SECTION_NOTES, "SpaRV Ps.23.1", "a", 1000, 2000);
	gen = notes_store_generation();
	/* ...rewritten with the same value is not modified. */
	notes_store_set(NOTES_SECTION_NOTES, "SpaRV Ps.23.1", "a");
	g_assert_cmpuint(notes_store_generation(), ==, gen);
	e = notes_store_lookup(NOTES_SECTION_NOTES, "SpaRV Ps.23.1");
	g_assert_cmpint(e->created, ==, 1000);
	g_assert_cmpint(e->modified, ==, 2000);
	notes_entry_free(e);
	/* Changed, it keeps its creation date. */
	notes_store_set(NOTES_SECTION_NOTES, "SpaRV Ps.23.1", "b");
	g_assert_cmpuint(notes_store_generation(), >, gen);
	e = notes_store_lookup(NOTES_SECTION_NOTES, "SpaRV Ps.23.1");
	g_assert_cmpint(e->created, ==, 1000);
	g_assert_cmpint(e->modified, >=, before);
	notes_entry_free(e);

	/* Dates survive the file; a note without them (older file) is 0. */
	notes_store_set_full(NOTES_SECTION_NOTES, "SpaRV Gen.1.1", "c", 0, 0);
	g_assert_true(notes_store_save());
	g_assert_true(notes_store_open(dpath));
	e = notes_store_lookup(NOTES_SECTION_NOTES, "SpaRV Ps.23.1");
	g_assert_cmpint(e->created, ==, 1000);
	g_assert_cmpint(e->modified, >=, before);
	notes_entry_free(e);
	e = notes_store_lookup(NOTES_SECTION_NOTES, "SpaRV Gen.1.1");
	g_assert_cmpint(e->created, ==, 0);
	g_assert_cmpint(e->modified, ==, 0);
	notes_entry_free(e);
	g_assert_null(notes_store_lookup(NOTES_SECTION_NOTES, "nada"));
	notes_store_close();
	g_free(dpath);
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	dir = g_dir_make_tmp("notes-store-XXXXXX", NULL);
	path = g_build_filename(dir, "notas.xml", NULL);
	g_test_add_func("/notes_store/roundtrip_and_order", test_roundtrip_and_order);
	g_test_add_func("/notes_store/entries_are_a_snapshot", test_entries_are_a_snapshot);
	g_test_add_func("/notes_store/backups_rotate_once_per_session",
			test_backups_rotate_once_per_session);
	g_test_add_func("/notes_store/no_temporary_files_left", test_no_temporary_files_left);
	g_test_add_func("/notes_store/unreadable_file_is_set_aside",
			test_unreadable_file_is_set_aside);
	g_test_add_func("/notes_store/dates", test_dates);
	int rc = g_test_run();
	notes_store_close();
	return rc;
}
