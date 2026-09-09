#include <glib.h>
#include <gtk/gtk.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "backend/bible_backend.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "gui/widgets.h"
#include "main/strong_interaction.h"
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
	if (!GTK_IS_CONTAINER(widget)) return;
	GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
	for (GList *item = children; item; item = item->next)
		walk(GTK_WIDGET(item->data), all);
	g_list_free(children);
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
	while (gtk_events_pending()) gtk_main_iteration();
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
	widgets.app = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_test_add_func("/strong-ui/rv1909", testRvFlow);
	const int result = g_test_run();
	gtk_widget_destroy(widgets.app);
	return result;
}
