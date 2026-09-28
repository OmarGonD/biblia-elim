#include <glib.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "backend/bible_backend.h"
#include "backend/bible_lexicon.h"
#include "fake_bible_backend.h"
#include "gui/dropdown_helpers.h"
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

GtkWidget *wordDialog()
{
	GList *windows = gtk_window_list_toplevels();
	GtkWidget *result = nullptr;
	for (GList *item = windows; item; item = item->next) {
		GtkWidget *widget = GTK_WIDGET(item->data);
		if (GTK_IS_DIALOG(widget)) {
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
	for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
		child = gtk_widget_get_next_sibling(child))
		descendants(child, result);
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

GtkWidget *labelNamed(GtkWidget *root, const char *expected)
{
	std::vector<GtkWidget *> widgetsFound;
	descendants(root, widgetsFound);
	for (GtkWidget *widget : widgetsFound)
		if (GTK_IS_LABEL(widget) &&
		    g_strcmp0(gtk_label_get_text(GTK_LABEL(widget)), expected) == 0)
			return widget;
	return nullptr;
}

GtkWidget *firstCombo(GtkWidget *root)
{
	std::vector<GtkWidget *> widgetsFound;
	descendants(root, widgetsFound);
	for (GtkWidget *widget : widgetsFound)
		if (GTK_IS_DROP_DOWN(widget)) return widget;
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

/* The Strong concordance list and the morphology occurrence list both key
 * their occurrence buttons the same way (navigateOccurrence() is shared,
 * MORPH-111), so for a word that has both, "is there a button for this
 * key" cannot tell which list grew. Counting can. */
int occurrenceButtonCount(GtkWidget *root, const char *key)
{
	std::vector<GtkWidget *> widgetsFound;
	descendants(root, widgetsFound);
	int count = 0;
	for (GtkWidget *widget : widgetsFound) {
		if (!GTK_IS_BUTTON(widget)) continue;
		const char *stored = static_cast<const char *>(g_object_get_data(
			G_OBJECT(widget), "strong-occurrence-key"));
		if (g_strcmp0(stored, key) == 0) ++count;
	}
	return count;
}

void closeDialog(GtkWidget *dialog)
{
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
	while (g_main_context_pending(nullptr)) g_main_context_iteration(nullptr, FALSE);
}

void testDialogFlow()
{
	FakeBibleBackend backend;
	bible_backend = &backend;
	main_set_strong_lexicon(nullptr);
	main_show_neutral_word("FakeBible", "John 3:16", 11);
	GtkWidget *dialog = wordDialog();
	g_assert_nonnull(dialog);
	g_assert_true(hasLabel(dialog, "Strong G25"));
	g_assert_true(hasLabel(dialog, "loved"));
	g_assert_true(hasLabel(dialog, "robinson"));
	g_assert_true(hasLabel(dialog, "V-AAI-3S"));
	g_assert_true(hasLabel(dialog, "custom.alpha"));
	g_assert_true(hasLabel(dialog, "opaque/code"));
	g_assert_true(hasLabel(dialog, "Esquema morfológico 1:"));
	g_assert_true(hasLabel(dialog, "Código morfológico 2:"));
	/* MORPH-110: "loved" carries two morphology tags -- {"robinson",
	 * "V-AAI-3S"} (recognized, MORPH-108) and {"custom.alpha",
	 * "opaque/code"} (an unrecognized/opaque scheme). Only the first
	 * gets a decoded Spanish label, additive to the raw scheme:code
	 * fields already asserted above; the second stays raw-only, no
	 * guessed-at grammar. */
	g_assert_true(hasLabel(dialog, "Análisis gramatical 1:"));
	g_assert_true(hasLabel(dialog,
		"Verbo · aoristo · activo · indicativo · 3ª persona · singular"));
	g_assert_false(hasLabel(dialog, "Análisis gramatical 2:"));
	g_assert_true(gtk_label_get_selectable(GTK_LABEL(
		labelNamed(dialog, "opaque/code"))));
	g_assert_true(hasLabel(dialog, "John 3:16"));
	g_assert_false(hasLabel(dialog, "agapao"));
	GtkWidget *first = occurrenceButton(dialog, "John 3:16");
	g_assert_nonnull(first);
	g_signal_emit_by_name(first, "clicked");
	g_assert_cmpstr(navigatedModule.c_str(), ==, "FakeBible");
	g_assert_cmpstr(navigatedKey.c_str(), ==, "John 3:16");

	TestLexicon lexicon;
	main_set_strong_lexicon(&lexicon);
	main_show_neutral_strong("FakeBible", "John 3:16", 11);
	dialog = wordDialog();
	g_assert_nonnull(dialog);
	g_assert_true(hasLabel(dialog, "agapao"));
	g_assert_true(hasLabel(dialog, "to love"));
	closeDialog(dialog);

	main_show_neutral_strong("FakeBible", "John 3:17", 13);
	dialog = wordDialog();
	g_assert_nonnull(dialog);
	GtkWidget *combo = firstCombo(dialog);
	g_assert_nonnull(combo);
	g_assert_cmpint(elim_dropdown_get_active(GTK_DROP_DOWN(combo)), ==, 0); /* row 0: nothing chosen yet */
	g_assert_cmpint(g_list_model_get_n_items(gtk_drop_down_get_model(GTK_DROP_DOWN(combo))), ==, 3);
	elim_dropdown_set_active(GTK_DROP_DOWN(combo), 2);
	g_assert_true(hasLabel(dialog, "Strong G5547"));
	closeDialog(dialog);

	main_set_strong_lexicon(nullptr);
	main_show_neutral_word("FakeBible", "John 3:16", 21);
	dialog = wordDialog();
	g_assert_nonnull(dialog);
	g_assert_true(hasLabel(dialog, "Detalles de palabra"));
	g_assert_true(hasLabel(dialog, "world"));
	g_assert_true(hasLabel(dialog, "Sin especificar"));
	g_assert_true(hasLabel(dialog, "HR/Ncfsa"));
	g_assert_true(gtk_label_get_selectable(GTK_LABEL(
		labelNamed(dialog, "HR/Ncfsa"))));
	/* MORPH-110: "world"'s tag has no declared scheme at all, the
	 * legacy bare-code case MORPH-108 keeps decoding exactly as
	 * before -- so it still gets an additive decoded label too. */
	g_assert_true(hasLabel(dialog, "Análisis gramatical:"));
	g_assert_true(hasLabel(dialog,
		"Preposición + Sustantivo · común · femenino · singular · absoluto"));
	g_assert_false(hasLabel(dialog, "Concordancia"));
	g_assert_null(firstCombo(dialog));
	closeDialog(dialog);
	main_set_strong_lexicon(nullptr);
}

/* MORPH-111: the morphology occurrence browser -- a second instance of
 * the exact same paging pattern (results GtkListBox + "Cargar más",
 * combo box for more than one candidate) as the Strong concordance
 * above, over findMorphologyOccurrencePage() instead. */
void testMorphologyOccurrenceBrowser()
{
	FakeBibleBackend backend;
	bible_backend = &backend;
	main_set_strong_lexicon(nullptr);

	/* "loved" carries two morphology tags: nothing auto-loads for them
	 * (a combo offers a choice, same as the multi-Strong "Son" case
	 * below), but it also has a single Strong number (G25), which *does*
	 * auto-load its own "John 3:16" occurrence button into the
	 * Concordancia list above -- both lists key their buttons the same
	 * way (navigateOccurrence() is shared), so the count, not mere
	 * presence, is what proves the morphology list actually grew. */
	main_show_neutral_word("FakeBible", "John 3:16", 11);
	GtkWidget *dialog = wordDialog();
	g_assert_nonnull(dialog);
	g_assert_true(hasLabel(dialog, "Ocurrencias morfológicas"));
	g_assert_cmpint(occurrenceButtonCount(dialog, "John 3:16"), ==, 1);
	GtkWidget *combo = firstCombo(dialog);
	g_assert_nonnull(combo);
	g_assert_cmpint(elim_dropdown_get_active(GTK_DROP_DOWN(combo)), ==, 0); /* row 0: nothing chosen yet */
	g_assert_cmpint(g_list_model_get_n_items(gtk_drop_down_get_model(GTK_DROP_DOWN(combo))), ==, 3);
	elim_dropdown_set_active(GTK_DROP_DOWN(combo), 1);
	g_assert_cmpint(occurrenceButtonCount(dialog, "John 3:16"), ==, 2);
	GtkWidget *occurrence = occurrenceButton(dialog, "John 3:16");
	g_assert_nonnull(occurrence);
	g_signal_emit_by_name(occurrence, "clicked");
	g_assert_cmpstr(navigatedModule.c_str(), ==, "FakeBible");
	g_assert_cmpstr(navigatedKey.c_str(), ==, "John 3:16");

	/* "world" carries exactly one morphology tag (no scheme at all, the
	 * MORPH-108 legacy-safe bare-code path) and no Strong number: the
	 * occurrence list loads right away, with no combo needed, and the
	 * section shows up even though this word has no Strong concordance
	 * at all. */
	main_show_neutral_word("FakeBible", "John 3:16", 21);
	dialog = wordDialog();
	g_assert_nonnull(dialog);
	g_assert_true(hasLabel(dialog, "Ocurrencias morfológicas"));
	g_assert_null(firstCombo(dialog));
	g_assert_nonnull(occurrenceButton(dialog, "John 3:16"));
	closeDialog(dialog);

	/* "Son" (John 3:17, offset 13) has no morphology tags at all: the
	 * section must not appear, exactly as "Concordancia" does not
	 * appear for a word with no Strong number. */
	main_show_neutral_word("FakeBible", "John 3:17", 13);
	dialog = wordDialog();
	g_assert_nonnull(dialog);
	g_assert_false(hasLabel(dialog, "Ocurrencias morfológicas"));
	closeDialog(dialog);
}
}

int main(int argc, char **argv)
{
	gtk_test_init(&argc, &argv, nullptr);
	widgets.app = gtk_window_new();
	g_test_add_func("/strong-ui/dialog-flow", testDialogFlow);
	g_test_add_func("/strong-ui/morphology-occurrence-browser",
		testMorphologyOccurrenceBrowser);
	const int result = g_test_run();
	gtk_window_destroy(GTK_WINDOW(widgets.app));
	return result;
}

BibleBackend &main_backend_for(const char *) { return *bible_backend; }
