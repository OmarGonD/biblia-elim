#include "main/backend_access.h"
#include "main/strong_ui.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "gui/widget_helpers.h"

#include "backend/bible_backend.h"
#include "backend/bible_resources.h"
#include "backend/strong_id.h"
#include "backend/bible_book_map.h"
#include "gui/widgets.h"
#include "main/morfologia.h"
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
	/* MORPH-111: the morphology occurrence browser, a second instance of
	 * the exact same results/loadMore paging pattern as above, over the
	 * word's morphology tag(s) instead of its Strong number(s). */
	GtkWidget *morphSelector = nullptr;
	GtkWidget *morphResults = nullptr;
	GtkWidget *morphLoadMore = nullptr;
};

GtkWidget *textLabel(const std::string &text, bool selectable = true)
{
	GtkWidget *label = gtk_label_new(text.c_str());
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), selectable);
	return label;
}

void clearContainer(GtkWidget *container)
{
	GtkWidget *child;
	while ((child = gtk_widget_get_first_child(container)))
		gui_widget_remove(child);
}

void addField(GtkWidget *box, const char *name, const std::string &value)
{
	if (value.empty()) return;
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	GtkWidget *caption = textLabel(name, false);
	gtk_widget_set_size_request(caption, 120, -1);
	GtkWidget *content = textLabel(value);
	gtk_widget_set_hexpand(content, TRUE);
	gtk_box_append(GTK_BOX(row), caption);
	gui_box_pack(GTK_BOX(row), content, TRUE, TRUE, 0);
	gtk_box_append(GTK_BOX(box), row);
}

/* MORPH-110: reconstructs the same "esquema:código" (or bare legacy
 * code) main/interlineal.cc feeds morfologia.c's decoder with, so a
 * single tag never gets re-decoded under some other scheme's rules. */
std::string taggedCode(const MorphologyTag &tag)
{
	return tag.scheme.empty() ? tag.code : tag.scheme + ":" + tag.code;
}

void addWordAnnotations(StrongDialog *view)
{
	addField(view->details, _("Palabra:"), view->session->word().word);
	const std::vector<MorphologyTag> &tags =
		view->session->word().morphologyTags;
	for (std::size_t index = 0; index < tags.size(); ++index) {
		const std::string number = tags.size() > 1
			? " " + std::to_string(index + 1) : "";
		addField(view->details,
			(std::string(_("Esquema morfológico")) + number + ":").c_str(),
			tags[index].scheme.empty() ? _("Sin especificar") :
				tags[index].scheme);
		addField(view->details,
			(std::string(_("Código morfológico")) + number + ":").c_str(),
			tags[index].code);
		/* Additive only, and only for a scheme morfologia.c actually
		 * recognizes (MORPH-108): the raw scheme:code above always
		 * stays visible either way, and an unrecognized/custom/opaque
		 * scheme never gets a guessed-at Spanish label. No duplicated
		 * grammar tables here -- this reuses the same validated
		 * decoder the legacy SWORD path uses. */
		const std::string tagged = taggedCode(tags[index]);
		if (main_morf_reconocido(tagged.c_str())) {
			gchar *decoded = main_morf_es(tagged.c_str());
			if (decoded && *decoded)
				addField(view->details,
					(std::string(_("Análisis gramatical")) + number + ":").c_str(),
					decoded);
			g_free(decoded);
		}
	}
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
	gui_widget_destroy(view->dialog);
}

void appendOccurrence(StrongDialog *view, const StrongOccurrence &occurrence)
{
	GtkWidget *button = gtk_button_new();
	gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
	g_object_set_data_full(G_OBJECT(button), "strong-occurrence-key",
		g_strdup(occurrence.key.c_str()), g_free);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	GtkWidget *reference = textLabel(occurrence.key, false);
	GtkWidget *context = textLabel(occurrence.context);
	gtk_widget_add_css_class(reference, "strong-reference");
	gtk_button_set_child(GTK_BUTTON(button), box);
	gtk_box_append(GTK_BOX(box), reference);
	gtk_box_append(GTK_BOX(box), context);
	g_signal_connect(button, "clicked", G_CALLBACK(navigateOccurrence), view);
	gtk_list_box_append(GTK_LIST_BOX(view->results), button);
}

void showSelectedStrong(StrongDialog *view)
{
	const StrongDetailState &state = view->session->state();
	const std::string id = formatStrongId(state.selected);
	gtk_label_set_text(GTK_LABEL(view->title), ("Strong " + id).c_str());
	clearContainer(view->details);
	addWordAnnotations(view);
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
	gtk_widget_show(view->details);
	gtk_widget_show(view->results);
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
	gtk_widget_show(view->results);
	gtk_widget_set_visible(view->loadMore, state.hasMore);
}

/* MORPH-111: the morphology occurrence browser below. Deliberately the
 * same shape as appendOccurrence()/showSelectedStrong()/selectStrong()/
 * loadMore() above -- a second, independent instance of that paging
 * pattern over MorphologyOccurrence instead of StrongOccurrence, not a
 * new one. Reuses navigateOccurrence() itself: it only reads the
 * "strong-occurrence-key" verse key off the button and navigates, which
 * is exactly what a morphology occurrence's .key also gives it. */
void appendMorphologyOccurrence(StrongDialog *view,
	const MorphologyOccurrence &occurrence)
{
	GtkWidget *button = gtk_button_new();
	gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
	g_object_set_data_full(G_OBJECT(button), "strong-occurrence-key",
		g_strdup(occurrence.key.c_str()), g_free);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	GtkWidget *reference = textLabel(occurrence.key, false);
	GtkWidget *context = textLabel(occurrence.context);
	gtk_widget_add_css_class(reference, "strong-reference");
	gtk_button_set_child(GTK_BUTTON(button), box);
	gtk_box_append(GTK_BOX(box), reference);
	gtk_box_append(GTK_BOX(box), context);
	g_signal_connect(button, "clicked", G_CALLBACK(navigateOccurrence), view);
	gtk_list_box_append(GTK_LIST_BOX(view->morphResults), button);
}

void showSelectedMorphology(StrongDialog *view)
{
	const MorphologyDetailState &state = view->session->morphologyState();
	clearContainer(view->morphResults);
	for (const MorphologyOccurrence &occurrence : state.occurrences)
		appendMorphologyOccurrence(view, occurrence);
	gtk_widget_show(view->morphResults);
	gtk_widget_set_visible(view->morphLoadMore, state.hasMore);
	g_debug("Strong UI morphology %s:%s: first-page=%lld us",
		state.selected.scheme.c_str(), state.selected.code.c_str(),
		state.pageMicroseconds);
}

void selectMorphology(StrongDialog *view, const MorphologyTag &morphology)
{
	if (view->session->selectMorphology(morphology))
		showSelectedMorphology(view);
}

void morphSelectorChanged(GtkComboBox *combo, gpointer userData)
{
	StrongDialog *view = static_cast<StrongDialog *>(userData);
	const int index = gtk_combo_box_get_active(combo);
	const std::vector<MorphologyTag> &tags = view->session->word().morphologyTags;
	if (index < 0 || static_cast<std::size_t>(index) >= tags.size()) return;
	selectMorphology(view, tags[index]);
}

void loadMoreMorphology(GtkButton *, gpointer userData)
{
	StrongDialog *view = static_cast<StrongDialog *>(userData);
	const std::size_t oldSize =
		view->session->morphologyState().occurrences.size();
	if (!view->session->loadMoreMorphology()) return;
	const MorphologyDetailState &state = view->session->morphologyState();
	for (std::size_t i = oldSize; i < state.occurrences.size(); ++i)
		appendMorphologyOccurrence(view, state.occurrences[i]);
	gtk_widget_show(view->morphResults);
	gtk_widget_set_visible(view->morphLoadMore, state.hasMore);
}

void dialogResponse(GtkDialog *dialog, gint, gpointer)
{
	gui_widget_destroy(GTK_WIDGET(dialog));
}

void dialogDestroyed(GtkWidget *, gpointer userData)
{
	delete static_cast<StrongDialog *>(userData);
}

StrongDialog *createDialog(const std::string &module,
	BibleAnnotatedWord context)
{
	auto *view = new StrongDialog;
	view->module = module;
	view->resources.bible = bible_backend;
	view->resources.strongLexicon = boundLexicon;
	view->session.reset(new StrongDetailSession(main_backend_for(module),
		view->resources, module, std::move(context), kPageSize));
	const bool hasStrongs = !view->session->word().strongs.empty();
	/* MORPH-111: a word can carry a morphology tag with no Strong number
	 * at all (e.g. an untagged-Strong word from a morphology-only
	 * module) -- the occurrence browser below must not depend on
	 * hasStrongs. */
	const std::vector<MorphologyTag> &morphologyTags =
		view->session->word().morphologyTags;
	const bool hasMorphology = !morphologyTags.empty();
	view->dialog = gtk_dialog_new_with_buttons(hasStrongs ? _("Strong") :
		_("Detalles de palabra"),
		widgets.app ? GTK_WINDOW(widgets.app) : nullptr,
		GTK_DIALOG_DESTROY_WITH_PARENT, _("Cerrar"), GTK_RESPONSE_CLOSE,
		nullptr);
	const bool expanded = hasStrongs || hasMorphology;
	gtk_window_set_default_size(GTK_WINDOW(view->dialog),
		expanded ? 680 : 520, expanded ? 560 : -1);
	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(view->dialog));
	gui_widget_set_margins(content, 12);
	view->title = textLabel(hasStrongs ? _("Seleccione un Strong") :
		_("Detalles de palabra"), false);
	PangoAttrList *attributes = pango_attr_list_new();
	pango_attr_list_insert(attributes, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
	gtk_label_set_attributes(GTK_LABEL(view->title), attributes);
	pango_attr_list_unref(attributes);
	gtk_box_append(GTK_BOX(content), view->title);

	if (view->session->word().strongs.size() > 1) {
		GtkWidget *selector = gtk_combo_box_text_new();
		for (const StrongId &strong : view->session->word().strongs)
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(selector),
				formatStrongId(strong).c_str());
		gui_box_pack(GTK_BOX(content), selector, FALSE, FALSE, 6);
		g_signal_connect(selector, "changed", G_CALLBACK(selectorChanged), view);
	}

	view->details = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gui_box_pack(GTK_BOX(content), view->details, FALSE, FALSE, 6);
	addWordAnnotations(view);
	if (!hasStrongs && !hasMorphology) {
		g_signal_connect(view->dialog, "response",
			G_CALLBACK(dialogResponse), nullptr);
		g_signal_connect(view->dialog, "destroy",
			G_CALLBACK(dialogDestroyed), view);
		return view;
	}

	if (hasStrongs) {
		GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gui_box_pack(GTK_BOX(content), separator, FALSE, FALSE, 4);
		GtkWidget *heading = textLabel(_("Concordancia"), false);
		gtk_box_append(GTK_BOX(content), heading);
		GtkWidget *scroll = gtk_scrolled_window_new();
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_widget_set_vexpand(scroll, TRUE);
		view->results = gtk_list_box_new();
		gtk_list_box_set_selection_mode(GTK_LIST_BOX(view->results),
			GTK_SELECTION_NONE);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view->results);
		gui_box_pack(GTK_BOX(content), scroll, TRUE, TRUE, 0);
		view->loadMore = gtk_button_new_with_label(_("Cargar más"));
		g_signal_connect(view->loadMore, "clicked", G_CALLBACK(loadMore), view);
		gui_box_pack(GTK_BOX(content), view->loadMore, FALSE, FALSE, 4);
	}

	if (hasMorphology) {
		/* MORPH-111: a second, independent instance of the same
		 * paging section as Concordancia above -- its own separator,
		 * heading, scrolled GtkListBox and "Cargar más" button, over
		 * findMorphologyOccurrencePage() (MORPH-107) instead of the
		 * Strong concordance query. A distinct heading ("Ocurrencias
		 * morfológicas", not "Concordancia") keeps it from reading as
		 * a Strong concordance for words that have no Strong number
		 * at all. Bounded to "occurrences of this exact tag" -- no
		 * free-text or cross-module search, no new query shape. */
		GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gui_box_pack(GTK_BOX(content), separator, FALSE, FALSE, 4);
		if (morphologyTags.size() > 1) {
			GtkWidget *selector = gtk_combo_box_text_new();
			for (const MorphologyTag &tag : morphologyTags)
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(selector),
					taggedCode(tag).c_str());
			gui_box_pack(GTK_BOX(content), selector, FALSE, FALSE, 6);
			g_signal_connect(selector, "changed",
				G_CALLBACK(morphSelectorChanged), view);
		}
		GtkWidget *heading = textLabel(_("Ocurrencias morfológicas"), false);
		gtk_box_append(GTK_BOX(content), heading);
		GtkWidget *scroll = gtk_scrolled_window_new();
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_widget_set_vexpand(scroll, TRUE);
		view->morphResults = gtk_list_box_new();
		gtk_list_box_set_selection_mode(GTK_LIST_BOX(view->morphResults),
			GTK_SELECTION_NONE);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
			view->morphResults);
		gui_box_pack(GTK_BOX(content), scroll, TRUE, TRUE, 0);
		view->morphLoadMore = gtk_button_new_with_label(_("Cargar más"));
		g_signal_connect(view->morphLoadMore, "clicked",
			G_CALLBACK(loadMoreMorphology), view);
		gui_box_pack(GTK_BOX(content), view->morphLoadMore, FALSE, FALSE, 4);
	}

	g_signal_connect(view->dialog, "response", G_CALLBACK(dialogResponse), nullptr);
	g_signal_connect(view->dialog, "destroy", G_CALLBACK(dialogDestroyed), view);
	return view;
}
}

extern "C" void main_set_strong_lexicon(BibleLexicon *lexicon)
{
	boundLexicon = lexicon;
}

extern "C" void main_show_neutral_word(const char *module,
	const char *passage, std::size_t byteOffset)
{
	if (!bible_backend || !module || !passage) return;
	BibleKeyInfo key;
	if (!main_backend_for(module).resolveKey(module, passage, key)) return;
	const auto started = std::chrono::steady_clock::now();
	AnnotatedWordResolution resolution = resolveAnnotatedWordInteraction(main_backend_for(module),
		module, key.reference, byteOffset);
	const auto resolveUs = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	g_debug("Strong UI resolve: %lld us", static_cast<long long>(resolveUs));
	if (resolution.action == AnnotatedWordAction::None) return;
	StrongDialog *view = createDialog(module, std::move(resolution.context));
	if (resolution.action == AnnotatedWordAction::OpenDetail &&
	    !view->session->word().strongs.empty())
		selectStrong(view, view->session->word().strongs.front());
	/* MORPH-111: a single morphology tag loads its occurrences right
	 * away, same as a single Strong number above; more than one tag
	 * waits for the combo in createDialog() to pick which, exactly like
	 * ChooseStrong does for multiple Strong numbers. */
	const std::vector<MorphologyTag> &morphologyTags =
		view->session->word().morphologyTags;
	if (morphologyTags.size() == 1)
		selectMorphology(view, morphologyTags.front());
	gtk_widget_show(view->dialog);
	if (view->loadMore)
		gtk_widget_set_visible(view->loadMore, view->session->state().hasMore);
	if (view->morphLoadMore)
		gtk_widget_set_visible(view->morphLoadMore,
			view->session->morphologyState().hasMore);
}

extern "C" void main_show_neutral_strong(const char *module,
	const char *passage, std::size_t byteOffset)
{
	main_show_neutral_word(module, passage, byteOffset);
}

extern "C" void main_show_neutral_footnote(const char *module, const char *passage,
	std::size_t sequence)
{
	if (!bible_backend || !module || !passage) return; BibleKeyInfo key;
	if (!main_backend_for(module).resolveKey(module, passage, key)) return;
	BibleVerseContent c=main_backend_for(module).getVerseContent(module,key.reference);
	if (sequence>=c.footnotes.size()) return; const BibleFootnote &n=c.footnotes[sequence];
	GtkWidget *d=gtk_message_dialog_new(widgets.app?GTK_WINDOW(widgets.app):nullptr,GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_INFO,GTK_BUTTONS_CLOSE,"%s\n\n%s",n.label.empty()?"Nota":n.label.c_str(),n.body.c_str());
	gtk_widget_show(d); gui_dialog_run(GTK_DIALOG(d)); gui_widget_destroy(d);
}

extern "C" void main_show_neutral_crossref(const char *module, const char *passage,
	std::size_t sequence)
{
	if (!bible_backend || !module || !passage) return; BibleKeyInfo key;
	if (!main_backend_for(module).resolveKey(module, passage, key)) return;
	BibleVerseContent c=main_backend_for(module).getVerseContent(module,key.reference);
	if (sequence>=c.crossReferences.size()) return; const auto &x=c.crossReferences[sequence];
	GtkWidget *d=gtk_dialog_new_with_buttons("Referencias",widgets.app?GTK_WINDOW(widgets.app):nullptr,GTK_DIALOG_DESTROY_WITH_PARENT,"Cerrar",GTK_RESPONSE_CLOSE,nullptr);
	GtkWidget *box=gtk_dialog_get_content_area(GTK_DIALOG(d)); gui_widget_set_margins(box, 10); GtkWidget *label=gtk_label_new(x.displayText.c_str()); gtk_label_set_selectable(GTK_LABEL(label),TRUE); gtk_label_set_xalign(GTK_LABEL(label),0); gui_box_pack(GTK_BOX(box), label, FALSE, FALSE, 4);
	for(const auto &r:x.references){ const auto &books=canonicalBibleBooks(); std::string name=(r.book>0&&r.book<=(int)books.size())?books[r.book-1].name:""; std::string target=name+" "+std::to_string(r.chapter)+":"+std::to_string(r.verse); GtkWidget *b=gtk_button_new_with_label(target.c_str()); g_object_set_data_full(G_OBJECT(b),"neutral-target",g_strdup(target.c_str()),g_free); g_object_set_data_full(G_OBJECT(b),"neutral-module",g_strdup(module),g_free); g_signal_connect(b,"clicked",G_CALLBACK(+[](GtkButton *button,gpointer){ const char *k=(const char*)g_object_get_data(G_OBJECT(button),"neutral-target"); const char *m=(const char*)g_object_get_data(G_OBJECT(button),"neutral-module"); if(k&&m){gchar *v=main_update_nav_controls(m,k); if(v){main_display_bible(m,v);g_free(v);}} }),nullptr); gui_box_pack(GTK_BOX(box), b, FALSE, FALSE, 2); }
	gtk_widget_show(d); gui_dialog_run(GTK_DIALOG(d)); gui_widget_destroy(d);
}
