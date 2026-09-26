/*
 * NOTES-EXPORT-101: exporting and importing the reader's notes.
 */
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include "main/note_value.h"
#include "main/notes_exchange.h"
#include "main/notes_store.h"

static gchar *dir;

static void open_fresh(const gchar *name)
{
	gchar *p = g_build_filename(dir, name, NULL);
	g_remove(p);
	g_assert_false(notes_store_open(p));
	g_free(p);
}

static void put(const gchar *label, const gchar *text, const gchar *note,
		gint64 created, gint64 modified)
{
	gchar *v = encode_note_value("#ffee00", text, note, text && *text ? 4 : -1);
	notes_store_set_full(NOTES_SECTION_NOTES, label, v, created, modified);
	g_free(v);
}

static gchar *note_of(const gchar *label)
{
	gchar *v = notes_store_get(NOTES_SECTION_NOTES, label);
	gchar *color = NULL, *text = NULL, *note = NULL;
	gint pos;
	if (!v)
		return NULL;
	g_assert_true(decode_note_value(v, &color, &text, &note, &pos));
	g_free(v);
	g_free(color);
	g_free(text);
	return note;
}

static gchar *sample_export(void)
{
	open_fresh("origen.xml");
	put("SpaRV Ps.23.1", "", "El Señor es mi pastor «de verdad»\nsegundo renglón",
	    1700000000, 1700000500);
	put("SpaRV John.3.16#42", "de tal manera amó", "subrayado", 1700001000,
	    1700001000);
	put("TorresAmat Ps.22.1", "", "nota antigua", 0, 0);
	notes_store_set_full(NOTES_SECTION_LINKS, "l1", "MV:Ps.23.1|HL:42", 5, 5);
	return notes_export_json();
}

static void test_json_is_readable(void)
{
	gchar *json = sample_export();
	/* The exact entry, and the same thing readable. */
	g_assert_nonnull(strstr(json, "\"format\" : \"biblia-elim-notas\""));
	g_assert_nonnull(strstr(json, "\"label\" : \"SpaRV Ps.23.1\""));
	g_assert_nonnull(strstr(json, "\"module\" : \"TorresAmat\""));
	g_assert_nonnull(strstr(json, "\"osisref\" : \"John.3.16\""));
	g_assert_nonnull(strstr(json, "\"text\" : \"de tal manera amó\""));
	g_assert_nonnull(strstr(json, "\"created\" : \"2023-11-14T22:13:20Z\""));
	/* An undated note says so. */
	g_assert_nonnull(strstr(json, "\"created\" : null"));
	g_free(json);
	notes_store_close();
}

static void test_roundtrip_into_empty_store(void)
{
	gchar *json = sample_export();
	GPtrArray *before = notes_store_entries(NOTES_SECTION_NOTES), *after;
	NotesImportResult r;
	GError *error = NULL;

	open_fresh("destino.xml");
	g_assert_true(notes_import_json(json, &r, &error));
	g_assert_no_error(error);
	g_assert_cmpuint(r.added, ==, 3);
	g_assert_cmpuint(r.links_added, ==, 1);
	g_assert_cmpuint(r.identical + r.extra + r.conflicts + r.invalid, ==, 0);

	after = notes_store_entries(NOTES_SECTION_NOTES);
	g_assert_cmpuint(after->len, ==, before->len);
	for (guint i = 0; i < after->len; i++) {
		NotesEntry *a = after->pdata[i], *b = before->pdata[i];
		g_assert_cmpstr(a->label, ==, b->label);
		g_assert_cmpstr(a->value, ==, b->value);
		g_assert_cmpint(a->created, ==, b->created);
		g_assert_cmpint(a->modified, ==, b->modified);
	}
	gchar *link = notes_store_get(NOTES_SECTION_LINKS, "l1");
	g_assert_cmpstr(link, ==, "MV:Ps.23.1|HL:42");
	g_free(link);

	/* Importing the same copy again changes nothing. */
	g_assert_true(notes_import_json(json, &r, NULL));
	g_assert_cmpuint(r.identical, ==, 3);
	g_assert_cmpuint(r.added + r.extra + r.conflicts + r.links_added, ==, 0);
	g_assert_cmpuint(notes_store_count(NOTES_SECTION_NOTES), ==, 3);

	g_ptr_array_unref(before);
	g_ptr_array_unref(after);
	g_free(json);
	notes_store_close();
}

static void test_never_replaces_a_note(void)
{
	gchar *json = sample_export();
	NotesImportResult r;
	gchar *n;

	open_fresh("con_notas.xml");
	put("SpaRV Ps.23.1", "", "la mía", 10, 10);
	put("SpaRV John.3.16#42", "de tal manera", "mi subrayado", 10, 10);
	notes_store_set_full(NOTES_SECTION_LINKS, "l1", "MV:Gen.1.1|HL:9", 5, 5);

	g_assert_true(notes_import_json(json, &r, NULL));
	/* Another note on the same verse: both stay. */
	g_assert_cmpuint(r.extra, ==, 1);
	/* Same highlight or link identity, different content: mine stays. */
	g_assert_cmpuint(r.conflicts, ==, 2);
	g_assert_cmpuint(r.added, ==, 1);
	n = note_of("SpaRV Ps.23.1");
	g_assert_cmpstr(n, ==, "la mía");
	g_free(n);
	n = note_of("SpaRV John.3.16#42");
	g_assert_cmpstr(n, ==, "mi subrayado");
	g_free(n);
	g_assert_cmpuint(notes_store_count(NOTES_SECTION_NOTES), ==, 4);

	gboolean found = FALSE;
	GPtrArray *e = notes_store_entries(NOTES_SECTION_NOTES);
	for (guint i = 0; i < e->len; i++) {
		NotesEntry *x = e->pdata[i];
		if (g_str_has_prefix(x->label, "SpaRV Ps.23.1#MV")) {
			n = note_of(x->label);
			g_assert_cmpstr(n, ==,
					"El Señor es mi pastor «de verdad»\nsegundo renglón");
			g_assert_cmpint(x->created, ==, 1700000000);
			g_free(n);
			found = TRUE;
		}
	}
	g_ptr_array_unref(e);
	g_assert_true(found);

	/* Again: the extra note is recognised, not added a second time. */
	g_assert_true(notes_import_json(json, &r, NULL));
	g_assert_cmpuint(r.extra + r.added, ==, 0);
	g_assert_cmpuint(notes_store_count(NOTES_SECTION_NOTES), ==, 4);
	g_free(json);
	notes_store_close();
}

static void test_rejects_what_is_not_a_copy(void)
{
	const gchar *bad[] = {
	    "",
	    "no es json",
	    "[1,2]",
	    "{\"format\":\"otra-cosa\",\"version\":1,\"notes\":[]}",
	    "{\"format\":\"biblia-elim-notas\",\"version\":2,\"notes\":[]}",
	    "{\"format\":\"biblia-elim-notas\",\"version\":\"1\",\"notes\":[]}",
	    NULL};
	NotesImportResult r;

	open_fresh("rechazo.xml");
	put("SpaRV Ps.23.1", "", "la mía", 10, 10);
	for (int i = 0; bad[i]; i++) {
		GError *error = NULL;
		g_assert_false(notes_import_json(bad[i], &r, &error));
		g_assert_nonnull(error);
		g_error_free(error);
		g_assert_cmpuint(notes_store_count(NOTES_SECTION_NOTES), ==, 1);
	}

	/* A valid copy with some malformed entries: those are counted and
	 * skipped, the rest imported. */
	const gchar *mixed =
	    "{\"format\":\"biblia-elim-notas\",\"version\":1,\"notes\":["
	    "{\"label\":\"SpaRV Gen.1.1\",\"value\":\"|%20|Principio|\"},"
	    "{\"label\":\"sin-espacio\",\"value\":\"||x|\"},"
	    "{\"label\":\"SpaRV Gen\",\"value\":\"||x|\"},"
	    "{\"label\":\"SpaRV Gen.1.2\",\"value\":\"sin barras\"},"
	    "{\"label\":\"SpaRV Gen.1.3\"},"
	    "42],\"links\":[{\"label\":\"l\",\"value\":\"nada\"}]}";
	g_assert_true(notes_import_json(mixed, &r, NULL));
	g_assert_cmpuint(r.added, ==, 1);
	g_assert_cmpuint(r.invalid, ==, 6);
	g_assert_cmpuint(notes_store_count(NOTES_SECTION_NOTES), ==, 2);
	g_assert_cmpuint(notes_store_count(NOTES_SECTION_LINKS), ==, 0);
	notes_store_close();
}

static void test_markdown(void)
{
	NotesMdItem a = {"Salmos", "Salmos 23:1", "SpaRV", NULL,
			 "El Señor es mi pastor", "Escrita el 01/02/2026"};
	NotesMdItem b = {"Salmos", "Salmos 23:2", "SpaRV", "delicados\npastos",
			 "descanso", NULL};
	NotesMdItem c = {"Juan", "Juan 3:16", "TorresAmat", NULL, "amor", NULL};
	GList *items = g_list_append(g_list_append(g_list_append(NULL, &a), &b), &c);
	gchar *md = notes_export_markdown(items, "25/09/2026");

	g_assert_true(g_str_has_prefix(md, "# Mis notas\n"));
	g_assert_nonnull(strstr(md, "25/09/2026 · 3 notas."));
	/* One heading per book, in the order given. */
	g_assert_nonnull(strstr(md, "\n## Salmos\n"));
	g_assert_null(strstr(strstr(md, "\n## Salmos\n") + 1, "\n## Salmos\n"));
	g_assert_true(strstr(md, "\n## Salmos\n") < strstr(md, "\n## Juan\n"));
	g_assert_nonnull(strstr(md, "### Salmos 23:1 (SpaRV)\n\nEl Señor es mi pastor\n"
				    "\n*Escrita el 01/02/2026*\n"));
	/* A phrase over two lines stays one quote. */
	g_assert_nonnull(strstr(md, "> «delicados\n> pastos»\n\ndescanso\n"));
	g_free(md);
	g_list_free(items);
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	dir = g_dir_make_tmp("notes-exchange-XXXXXX", NULL);
	g_test_add_func("/notes_exchange/json_is_readable", test_json_is_readable);
	g_test_add_func("/notes_exchange/roundtrip_into_empty_store",
			test_roundtrip_into_empty_store);
	g_test_add_func("/notes_exchange/never_replaces_a_note",
			test_never_replaces_a_note);
	g_test_add_func("/notes_exchange/rejects_what_is_not_a_copy",
			test_rejects_what_is_not_a_copy);
	g_test_add_func("/notes_exchange/markdown", test_markdown);
	return g_test_run();
}
