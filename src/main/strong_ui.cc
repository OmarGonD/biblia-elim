#include "main/strong_ui.h"

#include <chrono>
#include <memory>
#include <string>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "backend/bible_backend.h"
#include "backend/bible_resources.h"
#include "backend/strong_id.h"
#include "gui/widgets.h"
#include "main/strong_interaction.h"
#include "main/sword.h"

namespace {
constexpr std::size_t kPageSize = 50;
BibleLexicon *boundLexicon = nullptr;

struct StrongDialog {
	std::string module;
	BibleApplicationResources resources;
	std::unique_ptr<StrongDetailSession> session;
	GtkWidget *dialog = nullptr;
	GtkWidget *title = nullptr;
	GtkWidget *details = nullptr;
	GtkWidget *results = nullptr;
	GtkWidget *loadMore = nullptr;
};

GtkWidget *textLabel(const std::string &text, bool selectable = true)
{
	GtkWidget *label = gtk_label_new(text.c_str());
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), selectable);
	return label;
}

void clearContainer(GtkWidget *container)
{
	GList *children = gtk_container_get_children(GTK_CONTAINER(container));
	for (GList *item = children; item; item = item->next)
		gtk_widget_destroy(GTK_WIDGET(item->data));
	g_list_free(children);
}

void addField(GtkWidget *box, const char *name, const std::string &value)
{
	if (value.empty()) return;
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	GtkWidget *caption = textLabel(name, false);
	gtk_widget_set_size_request(caption, 120, -1);
	GtkWidget *content = textLabel(value);
	gtk_widget_set_hexpand(content, TRUE);
	gtk_box_pack_start(GTK_BOX(row), caption, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), content, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
}

void navigateOccurrence(GtkButton *button, gpointer userData)
{
	StrongDialog *view = static_cast<StrongDialog *>(userData);
	const char *key = static_cast<const char *>(g_object_get_data(
		G_OBJECT(button), "strong-occurrence-key"));
	if (!key || !*key) return;
	gchar *valid = main_update_nav_controls(view->module.c_str(), key);
	if (valid) {
		main_display_bible(view->module.c_str(), valid);
		g_free(valid);
	}
	gtk_widget_destroy(view->dialog);
}

void appendOccurrence(StrongDialog *view, const StrongOccurrence &occurrence)
{
	GtkWidget *button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_object_set_data_full(G_OBJECT(button), "strong-occurrence-key",
		g_strdup(occurrence.key.c_str()), g_free);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	GtkWidget *reference = textLabel(occurrence.key, false);
	GtkWidget *context = textLabel(occurrence.context);
	gtk_style_context_add_class(gtk_widget_get_style_context(reference),
		"strong-reference");
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_box_pack_start(GTK_BOX(box), reference, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), context, FALSE, FALSE, 0);
	g_signal_connect(button, "clicked", G_CALLBACK(navigateOccurrence), view);
	gtk_container_add(GTK_CONTAINER(view->results), button);
}

void showSelectedStrong(StrongDialog *view)
{
	const StrongDetailState &state = view->session->state();
	const std::string id = formatStrongId(state.selected);
	gtk_label_set_text(GTK_LABEL(view->title), ("Strong " + id).c_str());
	clearContainer(view->details);
	addField(view->details, _("Palabra:"), view->session->word().word);
	if (state.lexicon.valid) {
		addField(view->details, _("Lema:"), state.lexicon.lemma);
		addField(view->details, _("Transliteración:"),
			state.lexicon.transliteration);
		addField(view->details, _("Pronunciación:"),
			state.lexicon.pronunciation);
		addField(view->details, _("Definición:"), state.lexicon.definition);
	}
	clearContainer(view->results);
	for (const StrongOccurrence &occurrence : state.occurrences)
		appendOccurrence(view, occurrence);
	gtk_widget_set_visible(view->loadMore, state.hasMore);
	gtk_widget_show_all(view->details);
	gtk_widget_show_all(view->results);
	gtk_widget_set_visible(view->loadMore, state.hasMore);
	g_debug("Strong UI %s: lexicon=%lld us, first-page=%lld us",
		id.c_str(), state.lexiconMicroseconds, state.pageMicroseconds);
}

void selectStrong(StrongDialog *view, const StrongId &strong)
{
	if (view->session->selectStrong(strong))
		showSelectedStrong(view);
}

void selectorChanged(GtkComboBox *combo, gpointer userData)
{
	StrongDialog *view = static_cast<StrongDialog *>(userData);
	const int index = gtk_combo_box_get_active(combo);
	if (index < 0 || static_cast<std::size_t>(index) >=
	    view->session->word().strongs.size()) return;
	selectStrong(view, view->session->word().strongs[index]);
}

void loadMore(GtkButton *, gpointer userData)
{
	StrongDialog *view = static_cast<StrongDialog *>(userData);
	const std::size_t oldSize = view->session->state().occurrences.size();
	if (!view->session->loadMore()) return;
	const StrongDetailState &state = view->session->state();
	for (std::size_t i = oldSize; i < state.occurrences.size(); ++i)
		appendOccurrence(view, state.occurrences[i]);
	gtk_widget_show_all(view->results);
	gtk_widget_set_visible(view->loadMore, state.hasMore);
}

void dialogResponse(GtkDialog *dialog, gint, gpointer)
{
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

void dialogDestroyed(GtkWidget *, gpointer userData)
{
	delete static_cast<StrongDialog *>(userData);
}

StrongDialog *createDialog(const std::string &module,
	StrongWordContext context)
{
	auto *view = new StrongDialog;
	view->module = module;
	view->resources.bible = bible_backend;
	view->resources.strongLexicon = boundLexicon;
	view->session.reset(new StrongDetailSession(*bible_backend,
		view->resources, module, std::move(context), kPageSize));
	view->dialog = gtk_dialog_new_with_buttons(_("Strong"),
		widgets.app ? GTK_WINDOW(widgets.app) : nullptr,
		GTK_DIALOG_DESTROY_WITH_PARENT, _("Cerrar"), GTK_RESPONSE_CLOSE,
		nullptr);
	gtk_window_set_default_size(GTK_WINDOW(view->dialog), 680, 560);
	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(view->dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content), 12);
	view->title = textLabel(_("Seleccione un Strong"), false);
	PangoAttrList *attributes = pango_attr_list_new();
	pango_attr_list_insert(attributes, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
	gtk_label_set_attributes(GTK_LABEL(view->title), attributes);
	pango_attr_list_unref(attributes);
	gtk_box_pack_start(GTK_BOX(content), view->title, FALSE, FALSE, 0);

	if (view->session->word().strongs.size() > 1) {
		GtkWidget *selector = gtk_combo_box_text_new();
		for (const StrongId &strong : view->session->word().strongs)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(selector),
				formatStrongId(strong).c_str());
		gtk_box_pack_start(GTK_BOX(content), selector, FALSE, FALSE, 6);
		g_signal_connect(selector, "changed", G_CALLBACK(selectorChanged), view);
	}

	view->details = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_box_pack_start(GTK_BOX(content), view->details, FALSE, FALSE, 6);
	GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(content), separator, FALSE, FALSE, 4);
	GtkWidget *heading = textLabel(_("Concordancia"), false);
	gtk_box_pack_start(GTK_BOX(content), heading, FALSE, FALSE, 0);
	GtkWidget *scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	view->results = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(view->results),
		GTK_SELECTION_NONE);
	gtk_container_add(GTK_CONTAINER(scroll), view->results);
	gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);
	view->loadMore = gtk_button_new_with_label(_("Cargar más"));
	g_signal_connect(view->loadMore, "clicked", G_CALLBACK(loadMore), view);
	gtk_box_pack_start(GTK_BOX(content), view->loadMore, FALSE, FALSE, 4);
	g_signal_connect(view->dialog, "response", G_CALLBACK(dialogResponse), nullptr);
	g_signal_connect(view->dialog, "destroy", G_CALLBACK(dialogDestroyed), view);
	return view;
}
}

extern "C" void main_set_strong_lexicon(BibleLexicon *lexicon)
{
	boundLexicon = lexicon;
}

extern "C" void main_show_neutral_strong(const char *module,
	const char *passage, std::size_t byteOffset)
{
	if (!bible_backend || !module || !passage) return;
	BibleKeyInfo key;
	if (!bible_backend->resolveKey(module, passage, key)) return;
	const auto started = std::chrono::steady_clock::now();
	StrongWordResolution resolution = resolveStrongInteraction(*bible_backend,
		module, key.reference, byteOffset);
	const auto resolveUs = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	g_debug("Strong UI resolve: %lld us", static_cast<long long>(resolveUs));
	if (resolution.action == StrongWordAction::None) return;
	StrongDialog *view = createDialog(module, std::move(resolution.context));
	if (resolution.action == StrongWordAction::OpenDetail)
		selectStrong(view, view->session->word().strongs.front());
	gtk_widget_show_all(view->dialog);
	gtk_widget_set_visible(view->loadMore, view->session->state().hasMore);
}
