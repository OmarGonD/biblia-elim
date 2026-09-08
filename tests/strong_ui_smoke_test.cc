#include <glib.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "backend/bible_backend.h"
#include "backend/bible_lexicon.h"
#include "fake_bible_backend.h"
#include "gui/widgets.h"
#include "main/strong_ui.h"

WIDGETS widgets = {};
BibleBackend *bible_backend = nullptr;
static std::string navigatedModule;
static std::string navigatedKey;

extern "C" gchar *main_update_nav_controls(const char *module, const gchar *key)
{
	navigatedModule = module ? module : "";
	navigatedKey = key ? key : "";
	return g_strdup(key);
}

extern "C" void main_display_bible(const char *module, const char *key)
{
	navigatedModule = module ? module : "";
	navigatedKey = key ? key : "";
}

namespace {
class TestLexicon final : public BibleLexicon
{
public:
	LexiconEntry lookupStrong(const StrongId &id) const override
	{
		LexiconEntry result;
		result.id = id;
		if (id == StrongId{StrongLanguage::Greek, 25}) {
			result.lemma = "agapao";
			result.definition = "to love";
			result.valid = true;
		}
		return result;
	}
};

GtkWidget *strongDialog()
{
	GList *windows = gtk_window_list_toplevels();
	GtkWidget *result = nullptr;
	for (GList *item = windows; item; item = item->next) {
		GtkWidget *widget = GTK_WIDGET(item->data);
		if (GTK_IS_DIALOG(widget) &&
		    g_strcmp0(gtk_window_get_title(GTK_WINDOW(widget)), "Strong") == 0) {
			result = widget;
			break;
		}
	}
	g_list_free(windows);
	return result;
}

void descendants(GtkWidget *widget, std::vector<GtkWidget *> &result)
{
	result.push_back(widget);
	if (!GTK_IS_CONTAINER(widget)) return;
	GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
	for (GList *item = children; item; item = item->next)
		descendants(GTK_WIDGET(item->data), result);
	g_list_free(children);
}

bool hasLabel(GtkWidget *root, const char *expected)
{
	std::vector<GtkWidget *> widgetsFound;
	descendants(root, widgetsFound);
	for (GtkWidget *widget : widgetsFound)
		if (GTK_IS_LABEL(widget) &&
		    g_strcmp0(gtk_label_get_text(GTK_LABEL(widget)), expected) == 0)
			return true;
	return false;
}

GtkWidget *firstCombo(GtkWidget *root)
{
	std::vector<GtkWidget *> widgetsFound;
	descendants(root, widgetsFound);
	for (GtkWidget *widget : widgetsFound)
		if (GTK_IS_COMBO_BOX(widget)) return widget;
	return nullptr;
}

GtkWidget *occurrenceButton(GtkWidget *root, const char *key)
{
	std::vector<GtkWidget *> widgetsFound;
	descendants(root, widgetsFound);
	for (GtkWidget *widget : widgetsFound) {
		if (!GTK_IS_BUTTON(widget)) continue;
		const char *stored = static_cast<const char *>(g_object_get_data(
			G_OBJECT(widget), "strong-occurrence-key"));
		if (g_strcmp0(stored, key) == 0) return widget;
	}
	return nullptr;
}

void closeDialog(GtkWidget *dialog)
{
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
	while (gtk_events_pending()) gtk_main_iteration();
}

void testDialogFlow()
{
	FakeBibleBackend backend;
	bible_backend = &backend;
	main_set_strong_lexicon(nullptr);
	main_show_neutral_strong("FakeBible", "John 3:16", 11);
	GtkWidget *dialog = strongDialog();
	g_assert_nonnull(dialog);
	g_assert_true(hasLabel(dialog, "Strong G25"));
	g_assert_true(hasLabel(dialog, "loved"));
	g_assert_true(hasLabel(dialog, "John 3:16"));
	g_assert_false(hasLabel(dialog, "agapao"));
	GtkWidget *first = occurrenceButton(dialog, "John 3:16");
	g_assert_nonnull(first);
	gtk_button_clicked(GTK_BUTTON(first));
	g_assert_cmpstr(navigatedModule.c_str(), ==, "FakeBible");
	g_assert_cmpstr(navigatedKey.c_str(), ==, "John 3:16");

	TestLexicon lexicon;
	main_set_strong_lexicon(&lexicon);
	main_show_neutral_strong("FakeBible", "John 3:16", 11);
	dialog = strongDialog();
	g_assert_nonnull(dialog);
	g_assert_true(hasLabel(dialog, "agapao"));
	g_assert_true(hasLabel(dialog, "to love"));
	closeDialog(dialog);

	main_show_neutral_strong("FakeBible", "John 3:17", 13);
	dialog = strongDialog();
	g_assert_nonnull(dialog);
	GtkWidget *combo = firstCombo(dialog);
	g_assert_nonnull(combo);
	g_assert_cmpint(gtk_combo_box_get_active(GTK_COMBO_BOX(combo)), ==, -1);
	GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
	g_assert_cmpint(gtk_tree_model_iter_n_children(model, nullptr), ==, 2);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 1);
	g_assert_true(hasLabel(dialog, "Strong G5547"));
	closeDialog(dialog);
	main_set_strong_lexicon(nullptr);
}
}

int main(int argc, char **argv)
{
	gtk_test_init(&argc, &argv, nullptr);
	widgets.app = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_test_add_func("/strong-ui/dialog-flow", testDialogFlow);
	const int result = g_test_run();
	gtk_widget_destroy(widgets.app);
	return result;
}
