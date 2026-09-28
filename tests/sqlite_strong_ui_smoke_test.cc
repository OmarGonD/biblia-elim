#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <sqlite3.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "backend/bible_backend.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "gui/widgets.h"
#include "main/strong_interaction.h"
#include "main/strong_lexicon_startup.h"
#include "main/strong_ui.h"

WIDGETS widgets = {};
BibleBackend *bible_backend = nullptr;

extern "C" gchar *main_update_nav_controls(const char *, const gchar *key)
{
	return g_strdup(key);
}
extern "C" void main_display_bible(const char *, const char *) {}

namespace {
GtkWidget *dialog()
{
	GList *windows = gtk_window_list_toplevels();
	GtkWidget *result = nullptr;
	for (GList *item = windows; item; item = item->next)
		if (GTK_IS_DIALOG(item->data)) { result = GTK_WIDGET(item->data); break; }
	g_list_free(windows);
	return result;
}

void walk(GtkWidget *widget, std::vector<GtkWidget *> &all)
{
	all.push_back(widget);
	for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
		child = gtk_widget_get_next_sibling(child))
		walk(child, all);
}

bool hasLabel(GtkWidget *root, const std::string &text)
{
	std::vector<GtkWidget *> all;
	walk(root, all);
	for (GtkWidget *widget : all)
		if (GTK_IS_LABEL(widget) && text == gtk_label_get_text(GTK_LABEL(widget)))
			return true;
	return false;
}

GtkWidget *combo(GtkWidget *root)
{
	std::vector<GtkWidget *> all;
	walk(root, all);
	for (GtkWidget *widget : all) if (GTK_IS_COMBO_BOX(widget)) return widget;
	return nullptr;
}

void close(GtkWidget *widget)
{
	gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_CLOSE);
	while (g_main_context_pending(nullptr)) g_main_context_iteration(nullptr, FALSE);
}

void verifySingle(const char *key, std::size_t offset, const char *strong,
	const char *word)
{
	main_show_neutral_strong("rv1909", key, offset);
	GtkWidget *view = dialog();
	g_assert_nonnull(view);
	g_assert_true(hasLabel(view, std::string("Strong ") + strong));
	g_assert_true(hasLabel(view, word));
	close(view);
}

/* MORPH-109: writes a minimal, standalone lexicon_entries fixture (see
 * src/backend/strong-lexicon-format.md) with a single, unmistakable
 * entry for H430 -- the Strong number San Mateo... no, Génesis 1:1's
 * "Dios" already resolves to in testRvFlow() above. */
void writeLexiconFixture(const std::string &path)
{
	g_remove(path.c_str());
	sqlite3 *db = nullptr;
	g_assert_cmpint(sqlite3_open(path.c_str(), &db), ==, SQLITE_OK);
	g_assert_cmpint(sqlite3_exec(db,
		"CREATE TABLE lexicon_entries("
		"strong TEXT PRIMARY KEY, lemma TEXT, transliteration TEXT,"
		"pronunciation TEXT, definition TEXT);"
		"INSERT INTO lexicon_entries VALUES("
		"'H430','PRUEBA-LEMA-H430','prueba-translit','',"
		"'prueba-definicion');",
		nullptr, nullptr, nullptr), ==, SQLITE_OK);
	sqlite3_close(db);
}

/* main_set_strong_lexicon() has a real caller now
 * (main_bind_strong_lexicon(), src/main/strong_lexicon_startup.cc): a
 * bound lexicon shows lemma/transliteration/definition alongside the
 * bare Strong ID, and an absent lexicon file falls back to exactly
 * today's bare-ID dialog -- no crash, nothing invented. */
void testLexiconBinding()
{
	const std::string fixture = std::string(g_get_tmp_dir()) +
		"/xiphos-strong-ui-lexicon-fixture.sqlite";
	writeLexiconFixture(fixture);

	g_setenv("BIBLIA_ELIM_STRONG_LEXICON", fixture.c_str(), TRUE);
	main_bind_strong_lexicon();
	main_show_neutral_strong("rv1909", "Génesis 1:1", 24);
	GtkWidget *bound = dialog();
	g_assert_nonnull(bound);
	g_assert_true(hasLabel(bound, "Strong H430"));
	g_assert_true(hasLabel(bound, "PRUEBA-LEMA-H430"));
	g_assert_true(hasLabel(bound, "prueba-definicion"));
	close(bound);

	const std::string missing = std::string(g_get_tmp_dir()) +
		"/xiphos-strong-ui-lexicon-does-not-exist.sqlite";
	g_remove(missing.c_str());
	g_setenv("BIBLIA_ELIM_STRONG_LEXICON", missing.c_str(), TRUE);
	main_bind_strong_lexicon();
	main_show_neutral_strong("rv1909", "Génesis 1:1", 24);
	GtkWidget *fallback = dialog();
	g_assert_nonnull(fallback);
	g_assert_true(hasLabel(fallback, "Strong H430"));
	g_assert_false(hasLabel(fallback, "PRUEBA-LEMA-H430"));
	close(fallback);

	g_unsetenv("BIBLIA_ELIM_STRONG_LEXICON");
	g_remove(fixture.c_str());
}

void testRvFlow()
{
	BibleKeyInfo genesis;
	g_assert_true(bible_backend->resolveKey("rv1909", "Génesis 1:1", genesis));
	BibleVerseContent content = bible_backend->getVerseContent(
		"rv1909", genesis.reference);
	const std::string markup = renderAnnotatedVerseText(content, "rv1909",
		genesis.key, true);
	g_assert_nonnull(strstr(markup.c_str(), "data-offset=\"24\""));
	g_assert_null(strstr(markup.c_str(), "H430"));
	verifySingle("Génesis 1:1", 24, "H430", "Dios");
	verifySingle("San Mateo 5:43", 28, "G25", "Amarás");
	verifySingle("San Mateo 1:6", 22, "G3588", "al");
	verifySingle("2 Corintios 8:20", 46, "G100", "abundancia");
	main_show_neutral_strong("rv1909", "San Mateo 1:1", 26);
	GtkWidget *view = dialog();
	g_assert_nonnull(view);
	GtkWidget *selector = combo(view);
	g_assert_nonnull(selector);
	g_assert_cmpint(gtk_combo_box_get_active(GTK_COMBO_BOX(selector)), ==, -1);
	GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(selector));
	g_assert_cmpint(gtk_tree_model_iter_n_children(model, nullptr), ==, 2);
	gtk_combo_box_set_active(GTK_COMBO_BOX(selector), 0);
	g_assert_true(hasLabel(view, "Strong G2424"));
	close(view);
}
}

int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	const std::string directory = argv[1];
	argc = 1;
	gtk_test_init(&argc, &argv, nullptr);
	SqliteBibleBackend backend(directory);
	bible_backend = &backend;
	g_assert_true(backend.moduleCapabilities("rv1909").strongs);
	g_test_add_func("/strong-ui/lexicon-binding", testLexiconBinding);
	widgets.app = gtk_window_new();
	g_test_add_func("/strong-ui/rv1909", testRvFlow);
	const int result = g_test_run();
	gtk_window_destroy(GTK_WINDOW(widgets.app));
	return result;
}

BibleBackend &main_backend_for(const char *) { return *bible_backend; }
