#include "gui/editable_edition_dialog.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <sqlite3.h>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "backend/editable_edition.h"
#include "backend/bible_book_map.h"
#include "backend/edition_diff.h"
#include "backend/sqlite_module_manager.h"
#include "gui/utilities.h"
#include "gui/widget_helpers.h"
#include "main/sword.h"
#include "main/mod_mgr.h"

namespace {
std::set<GtkWidget*> contributionsButtons;
struct Book { int id; std::string name; };
struct Editor {
	std::unique_ptr<EditableEdition> edition;
	std::vector<Book> books;
	GtkWidget *window = nullptr, *book = nullptr, *chapter = nullptr, *verse = nullptr;
	GtkWidget *text = nullptr, *bookComment = nullptr, *verseComment = nullptr;
	GtkWidget *proposal = nullptr, *rationale = nullptr, *evidence = nullptr;
	GtkWidget *confidence = nullptr, *decisionStatus = nullptr, *history = nullptr;
	GtkWidget *sourceContext = nullptr, *sourceLinks = nullptr;
	GtkWidget *parallel = nullptr, *parallelContext = nullptr;
	std::vector<std::string> parallelIds;
	GtkWidget *status = nullptr, *name = nullptr;
	bool loading = false;
};

std::string moduleDirectory()
{
	const char *configured = main_sqlite_modules_directory();
	return configured && *configured ? configured : sqliteModuleDirectory();
}

std::string modulePath(const std::string &id)
{
	return moduleDirectory() + "/" + id + ".sqlite";
}

BibleReference selectedReference(Editor &editor);
void browseChanges(GtkButton *, gpointer);
void compareSelection(GtkButton *, gpointer);
void setBuffer(GtkWidget *view, const std::string &text);

std::string sourceContextText(Editor &editor, const BibleReference &ref)
{
	sqlite3 *db=nullptr; const auto path=modulePath(editor.edition->metadata().sourceModuleId);
	if(sqlite3_open_v2(path.c_str(),&db,SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK){if(db)sqlite3_close(db);return _("No se pudo abrir la Biblia base para consultar su aparato.");}
	std::string result;sqlite3_stmt*s=nullptr;
	if(sqlite3_prepare_v2(db,"SELECT text FROM verses WHERE book_id=? AND chapter=? AND verse=?",-1,&s,nullptr)==SQLITE_OK){sqlite3_bind_int(s,1,ref.book);sqlite3_bind_int(s,2,ref.chapter);sqlite3_bind_int(s,3,ref.verse);if(sqlite3_step(s)==SQLITE_ROW){const auto*p=reinterpret_cast<const char*>(sqlite3_column_text(s,0));result=std::string(_("Texto de la Biblia base: "))+(p?p:"");}}
	if(s)sqlite3_finalize(s);
	bool hasStrong=false,hasMorphology=false;sqlite3_stmt*probe=nullptr;
	if(sqlite3_prepare_v2(db,"SELECT 1 FROM sqlite_master WHERE type='table' AND name=?",-1,&probe,nullptr)==SQLITE_OK){sqlite3_bind_text(probe,1,"verse_word_strongs",-1,SQLITE_STATIC);hasStrong=sqlite3_step(probe)==SQLITE_ROW;sqlite3_reset(probe);sqlite3_bind_text(probe,1,"verse_word_morphology",-1,SQLITE_STATIC);hasMorphology=sqlite3_step(probe)==SQLITE_ROW;}if(probe)sqlite3_finalize(probe);
	std::string sql="SELECT vw.text";
	if(hasStrong)sql+=",(SELECT group_concat(strong, ', ') FROM verse_word_strongs s WHERE s.book_id=vw.book_id AND s.chapter=vw.chapter AND s.verse=vw.verse AND s.sequence=vw.sequence)";
	if(hasMorphology)sql+=",(SELECT group_concat(code, ', ') FROM verse_word_morphology m WHERE m.book_id=vw.book_id AND m.chapter=vw.chapter AND m.verse=vw.verse AND m.word_sequence=vw.sequence)";
	sql+=" FROM verse_words vw WHERE vw.book_id=? AND vw.chapter=? AND vw.verse=? ORDER BY vw.sequence";
	if(sqlite3_prepare_v2(db,sql.c_str(),-1,&s,nullptr)==SQLITE_OK){sqlite3_bind_int(s,1,ref.book);sqlite3_bind_int(s,2,ref.chapter);sqlite3_bind_int(s,3,ref.verse);std::string words;while(sqlite3_step(s)==SQLITE_ROW){const auto*w=reinterpret_cast<const char*>(sqlite3_column_text(s,0));const auto*g=hasStrong?reinterpret_cast<const char*>(sqlite3_column_text(s,1)):nullptr;const auto*m=hasMorphology?reinterpret_cast<const char*>(sqlite3_column_text(s,1+(hasStrong?1:0))):nullptr;if(!words.empty())words+=" · ";words+=w?w:"";if(g&&*g)words+=" ["+std::string(g)+"]";if(m&&*m)words+=" {"+std::string(m)+"}";}if(!words.empty())result+="\n\n"+std::string(_("Interlineal disponible (palabras, Strong y morfología): "))+words;else if(!result.empty())result+="\n\n"+std::string(_("La Biblia base no contiene alineación interlineal para este versículo."));}else if(!result.empty())result+="\n\n"+std::string(_("La Biblia base no contiene alineación interlineal para este versículo."));
	if(s)sqlite3_finalize(s);sqlite3_close(db);return result.empty()?std::string(_("No se encontró el versículo correspondiente en la Biblia base.")):result;
}

void loadParallel(Editor *editor)
{
	const guint i=gtk_drop_down_get_selected(GTK_DROP_DOWN(editor->parallel));
	if(i>=editor->parallelIds.size()){setBuffer(editor->parallelContext,_("No hay una versión paralela seleccionada."));return;}
	sqlite3*db=nullptr;const auto path=modulePath(editor->parallelIds[i]);
	if(sqlite3_open_v2(path.c_str(),&db,SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK){if(db)sqlite3_close(db);setBuffer(editor->parallelContext,_("No se pudo abrir esta versión."));return;}
	const auto r=selectedReference(*editor);sqlite3_stmt*s=nullptr;std::string text;
	if(sqlite3_prepare_v2(db,"SELECT text FROM verses WHERE book_id=? AND chapter=? AND verse=?",-1,&s,nullptr)==SQLITE_OK){sqlite3_bind_int(s,1,r.book);sqlite3_bind_int(s,2,r.chapter);sqlite3_bind_int(s,3,r.verse);if(sqlite3_step(s)==SQLITE_ROW){const auto*p=reinterpret_cast<const char*>(sqlite3_column_text(s,0));text=p?p:"";}}
	if(s)sqlite3_finalize(s);sqlite3_close(db);setBuffer(editor->parallelContext,text.empty()?_("Esta versión no contiene el versículo equivalente."):text);
}

void parallelChanged(GObject*,GParamSpec*,gpointer data){loadParallel(static_cast<Editor*>(data));}

std::string bufferText(GtkWidget *view)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	GtkTextIter start, end;
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	std::string result = text ? text : "";
	g_free(text);
	return result;
}

// Keep URLs in editable comments and expose the same addresses as links.
void populateSourceLinks(GtkWidget *links, const std::string &text)
{
 while (GtkWidget *child = gtk_widget_get_first_child(links))
  gtk_flow_box_remove(GTK_FLOW_BOX(links), child);
 std::istringstream words(text);
 std::set<std::string> seen;
 std::string url;
 while (words >> url) {
  if (url.compare(0, 8, "https://") && url.compare(0, 7, "http://")) continue;
  GUri *uri = g_uri_parse(url.c_str(), G_URI_FLAGS_NONE, nullptr);
  const char *host = uri ? g_uri_get_host(uri) : nullptr;
  if (host && *host && seen.insert(url).second) {
   std::string label = std::to_string(seen.size()) + " · " + host;
   GtkWidget *link = gtk_link_button_new_with_label(url.c_str(), label.c_str());
   gtk_widget_set_tooltip_text(link, url.c_str());
   gtk_flow_box_append(GTK_FLOW_BOX(links), link);
  }
  if (uri) g_uri_unref(uri);
 }
 gtk_widget_set_visible(links, !seen.empty());
}

void refreshSourceLinks(Editor *editor)
{
 populateSourceLinks(editor->sourceLinks, bufferText(editor->verseComment)+"\n"+bufferText(editor->evidence));
}

void setBuffer(GtkWidget *view, const std::string &text)
{
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)),
				 text.c_str(), static_cast<gint>(text.size()));
}

GtkWidget *textArea(int height)
{
	GtkWidget *view = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
	gtk_widget_set_size_request(view, -1, height);
	return view;
}

bool readBooks(Editor &editor)
{
	sqlite3 *db = nullptr;
	if (sqlite3_open_v2(modulePath(editor.edition->metadata().projectId).c_str(),
			    &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
		if (db) sqlite3_close(db);
		return false;
	}
	sqlite3_stmt *s = nullptr;
	bool ok = sqlite3_prepare_v2(db,
		"SELECT book_id,name FROM books ORDER BY position", -1, &s, nullptr) == SQLITE_OK;
	while (ok && sqlite3_step(s) == SQLITE_ROW) {
		const auto *name = reinterpret_cast<const char *>(sqlite3_column_text(s, 1));
		editor.books.push_back({sqlite3_column_int(s, 0), name ? name : ""});
	}
	if (s) sqlite3_finalize(s);
	sqlite3_close(db);
	return ok && !editor.books.empty();
}

BibleReference selectedReference(Editor &editor)
{
	const guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(editor.book));
	if (selected >= editor.books.size()) return {};
	return {0, editor.books[selected].id,
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editor.chapter)),
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editor.verse))};
}

void loadSelection(Editor *editor)
{
	if (editor->loading) return;
	editor->loading = true;
	BibleReference ref = selectedReference(*editor);
	std::string text, comment, error;
	if (editor->edition->verseText(ref, text, error)) {
		setBuffer(editor->text, text);
		setBuffer(editor->sourceContext, sourceContextText(*editor, ref));
		if (!editor->edition->verseComment(ref, comment, error)) comment.clear();
		setBuffer(editor->verseComment, comment);
		std::vector<EditionRevision> revisions;std::string historyText;
		if(editor->edition->revisions(ref,revisions,error)) for(const auto&r:revisions) historyText+=r.oldText+"\n→ "+r.newText+"\n\n";
		setBuffer(editor->history,historyText.empty()?_("Todavía no hay cambios registrados."):historyText);
		EditionDecision decision; if (editor->edition->decision(ref, decision, error)) {
			setBuffer(editor->proposal, decision.proposal);
			setBuffer(editor->rationale, decision.rationale);
			setBuffer(editor->evidence, decision.evidence);
			auto selectText=[](GtkWidget*w,const char*value){GListModel*m=gtk_drop_down_get_model(GTK_DROP_DOWN(w));for(guint i=0;i<g_list_model_get_n_items(m);++i){auto*o=GTK_STRING_OBJECT(g_list_model_get_item(m,i));bool same=g_ascii_strcasecmp(gtk_string_object_get_string(o),value)==0;g_object_unref(o);if(same){gtk_drop_down_set_selected(GTK_DROP_DOWN(w),i);return;}}};
			selectText(editor->confidence,decision.confidence.c_str());selectText(editor->decisionStatus,decision.status.c_str());
		} else {setBuffer(editor->proposal,"");setBuffer(editor->rationale,"");setBuffer(editor->evidence,"");gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->confidence),0);gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->decisionStatus),0);}
		gtk_label_set_text(GTK_LABEL(editor->status), " ");
	} else {
		setBuffer(editor->text, ""); setBuffer(editor->verseComment, "");
		setBuffer(editor->sourceContext, ""); setBuffer(editor->proposal, "");
		setBuffer(editor->rationale, ""); setBuffer(editor->evidence, "");
		gtk_label_set_text(GTK_LABEL(editor->status), error.c_str());
	}
	comment.clear(); error.clear();
	if (!editor->edition->bookComment(ref.book, comment, error)) comment.clear();
	setBuffer(editor->bookComment, comment);
	refreshSourceLinks(editor);
	editor->loading = false;
}

void selectionChanged(GObject *, GParamSpec *, gpointer data)
{
	auto *editor=static_cast<Editor *>(data);if(editor->loading)return;loadSelection(editor);loadParallel(editor);
}

void spinSelectionChanged(GtkSpinButton *, gpointer data)
{
 selectionChanged(nullptr,nullptr,data);
}

void save(GtkButton *, gpointer data)
{
	auto *editor = static_cast<Editor *>(data);
	const BibleReference ref = selectedReference(*editor);
	std::string error;
	const std::string name = gtk_editable_get_text(GTK_EDITABLE(editor->name));
	std::string previousText;
	editor->edition->verseText(ref,previousText,error);error.clear();
	const std::string newText=bufferText(editor->text);
	bool ok = editor->edition->setName(name, error) &&
		editor->edition->setVerseText(ref, newText, error) &&
		editor->edition->setBookComment(ref.book, bufferText(editor->bookComment), error) &&
		editor->edition->setVerseComment(ref, bufferText(editor->verseComment), error);
	if (ok) {
		const bool undocumented=previousText!=newText&&bufferText(editor->evidence).empty();
		EditionDecision decision; decision.reference=ref; decision.proposal=bufferText(editor->proposal);
		decision.rationale=bufferText(editor->rationale); decision.evidence=bufferText(editor->evidence);
		auto selectedText=[](GtkWidget*w){auto*o=GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(GTK_DROP_DOWN(w)));return std::string(o?gtk_string_object_get_string(o):"");};
		decision.confidence=selectedText(editor->confidence); decision.status=selectedText(editor->decisionStatus);
		if(undocumented){decision.status="Pendiente de revisión";decision.confidence="Por evaluar";gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->decisionStatus),1);gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->confidence),0);}
		ok=editor->edition->setDecision(decision,error);
	}
	std::vector<EditionRevision> history;
	if(ok) ok=editor->edition->revisions(ref,history,error);
	const bool undocumented=ok&&previousText!=newText&&bufferText(editor->evidence).empty();
	const std::string status=ok ? (undocumented ? std::string(_("Guardado; documenta las fuentes y evidencia de este cambio.")) : std::string(_("Cambios guardados · revisiones registradas: "))+std::to_string(history.size())) : error;
	gtk_label_set_text(GTK_LABEL(editor->status),status.c_str());
	if (ok) {
		refreshSourceLinks(editor);
		main_update_module_lists();
	}
}

void editGlossary(GtkButton *, gpointer data)
{
	auto *editor=static_cast<Editor*>(data);
	GtkWidget*d=gtk_dialog_new_with_buttons(_("Glosario del proyecto"),GTK_WINDOW(editor->window),GTK_DIALOG_MODAL,_ ("Cancelar"),GTK_RESPONSE_CANCEL,_ ("Guardar término"),GTK_RESPONSE_ACCEPT,NULL);
	GtkWidget*box=gtk_dialog_get_content_area(GTK_DIALOG(d));gtk_orientable_set_orientation(GTK_ORIENTABLE(box),GTK_ORIENTATION_VERTICAL);gtk_box_set_spacing(GTK_BOX(box),8);gtk_widget_set_margin_start(box,18);gtk_widget_set_margin_end(box,18);gtk_widget_set_margin_top(box,14);gtk_widget_set_margin_bottom(box,14);
	GtkWidget*source=gtk_entry_new(),*target=gtk_entry_new(),*note=textArea(70);
	gtk_box_append(GTK_BOX(box),gtk_label_new(_("Término original / lema")));gtk_box_append(GTK_BOX(box),source);
	gtk_box_append(GTK_BOX(box),gtk_label_new(_("Traducción preferida")));gtk_box_append(GTK_BOX(box),target);
	gtk_box_append(GTK_BOX(box),gtk_label_new(_("Nota de uso")));gtk_box_append(GTK_BOX(box),note);
	std::vector<EditionGlossaryEntry> entries;std::string error;
	if(editor->edition->glossaryEntries(entries,error)&&!entries.empty()){std::string text;for(const auto&e:entries){if(!text.empty())text+="\n";text+=e.sourceTerm+" → "+e.targetTerm;}GtkWidget*list=gtk_label_new(text.c_str());gtk_label_set_xalign(GTK_LABEL(list),0);gtk_label_set_wrap(GTK_LABEL(list),TRUE);gtk_box_append(GTK_BOX(box),list);}
	gtk_widget_show(d);if(gui_dialog_run(GTK_DIALOG(d))==GTK_RESPONSE_ACCEPT){EditionGlossaryEntry e;e.sourceTerm=gtk_editable_get_text(GTK_EDITABLE(source));e.targetTerm=gtk_editable_get_text(GTK_EDITABLE(target));e.note=bufferText(note);if(editor->edition->setGlossaryEntry(e,error))gtk_label_set_text(GTK_LABEL(editor->status),_("Término guardado en el glosario."));else gtk_label_set_text(GTK_LABEL(editor->status),error.c_str());}
	gui_widget_destroy(d);
}

void showEditor(GtkWindow *parent, const std::string &path, const BibleReference &initial = {})
{
	std::string error;
	auto edition = EditableEdition::open(path, error);
	if (!edition) {
		GtkWidget *message = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", error.c_str());
		gui_dialog_run(GTK_DIALOG(message)); gui_widget_destroy(message); return;
	}
	Editor editor;
	editor.edition = std::move(edition);
	if (!readBooks(editor)) return;
	GtkWidget *dialog = gtk_dialog_new_with_buttons(
		_("Editar Biblia de estudio"), parent, GTK_DIALOG_MODAL, NULL);
	gtk_widget_set_name(dialog, "editable-edition-editor");
	editor.window = dialog;
	gtk_window_set_default_size(GTK_WINDOW(dialog), 780, 660);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_orientable_set_orientation(GTK_ORIENTABLE(box), GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing(GTK_BOX(box), 10);
	gtk_widget_set_margin_start(box, 18);
	gtk_widget_set_margin_end(box, 18);
	gtk_widget_set_margin_top(box, 14);
	gtk_widget_set_margin_bottom(box, 10);
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	GtkWidget *titleLabel = gtk_label_new(_("Nombre de la Biblia"));
	editor.name = gtk_entry_new();
	gtk_widget_set_hexpand(editor.name, TRUE);
	gtk_editable_set_text(GTK_EDITABLE(editor.name), editor.edition->metadata().name.c_str());
	gtk_label_set_xalign(GTK_LABEL(titleLabel), 0.0f);
	gtk_grid_attach(GTK_GRID(grid), titleLabel, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), editor.name, 1, 0, 2, 1);
	GtkStringList *bookNames = gtk_string_list_new(nullptr);
	for (const auto &book : editor.books) gtk_string_list_append(bookNames, book.name.c_str());
	editor.book = gtk_drop_down_new(G_LIST_MODEL(bookNames), nullptr);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(editor.book), 0);
	gtk_widget_set_hexpand(editor.book, TRUE);
	editor.chapter = gtk_spin_button_new_with_range(1, 200, 1);
	editor.verse = gtk_spin_button_new_with_range(1, 200, 1);
 gtk_widget_set_name(editor.verse,"edition-verse");
	gtk_widget_set_hexpand(editor.chapter, TRUE);
	gtk_widget_set_hexpand(editor.verse, TRUE);
	gtk_box_append(GTK_BOX(box), grid);
 const std::string provenance = std::string(_("Basada en: ")) + editor.edition->metadata().sourceName;
 GtkWidget *sourceLabel = gtk_label_new(provenance.c_str());
 gtk_label_set_xalign(GTK_LABEL(sourceLabel), 0);gtk_label_set_wrap(GTK_LABEL(sourceLabel), TRUE);
 gtk_box_append(GTK_BOX(box), sourceLabel);
 GtkWidget *editionActions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
 GtkWidget *compare = gtk_button_new_with_label(_("Comparar con la base"));
 GtkWidget *changes = gtk_button_new_with_label(_("Cambios respecto a la base…"));
 gtk_widget_set_name(compare,"edition-compare");gtk_widget_set_name(changes,"edition-changes");
 gtk_box_append(GTK_BOX(editionActions), compare);gtk_box_append(GTK_BOX(editionActions), changes);
 gtk_box_append(GTK_BOX(box), editionActions);
 g_signal_connect(compare,"clicked",G_CALLBACK(compareSelection),&editor);
 g_signal_connect(changes,"clicked",G_CALLBACK(browseChanges),&editor);
	GtkWidget *reference = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
	GtkWidget *bookField = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	GtkWidget *bookLabel = gtk_label_new(_("Libro"));
	gtk_label_set_xalign(GTK_LABEL(bookLabel), 0.0f);
	gtk_box_append(GTK_BOX(bookField), bookLabel);
	gtk_box_append(GTK_BOX(bookField), editor.book);
	gtk_widget_set_hexpand(bookField, TRUE);
	GtkWidget *chapterField = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	GtkWidget *chapterLabel = gtk_label_new(_("Capítulo"));
	gtk_label_set_xalign(GTK_LABEL(chapterLabel), 0.0f);
	gtk_box_append(GTK_BOX(chapterField), chapterLabel);
	gtk_box_append(GTK_BOX(chapterField), editor.chapter);
	GtkWidget *verseField = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	GtkWidget *verseLabel = gtk_label_new(_("Versículo"));
	gtk_label_set_xalign(GTK_LABEL(verseLabel), 0.0f);
	gtk_box_append(GTK_BOX(verseField), verseLabel);
	gtk_box_append(GTK_BOX(verseField), editor.verse);
	gtk_box_append(GTK_BOX(reference), bookField);
	gtk_box_append(GTK_BOX(reference), chapterField);
	gtk_box_append(GTK_BOX(reference), verseField);
	gtk_box_append(GTK_BOX(box), reference);
	GtkWidget *notebook=gtk_notebook_new();gtk_widget_set_vexpand(notebook,TRUE);gtk_widget_set_hexpand(notebook,TRUE);
	GtkWidget *editPage=gtk_box_new(GTK_ORIENTATION_VERTICAL,8),*toolsPage=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
	gtk_widget_set_margin_start(editPage,6);gtk_widget_set_margin_end(editPage,6);gtk_widget_set_margin_top(editPage,6);gtk_widget_set_margin_bottom(editPage,6);
	gtk_widget_set_margin_start(toolsPage,6);gtk_widget_set_margin_end(toolsPage,6);gtk_widget_set_margin_top(toolsPage,6);gtk_widget_set_margin_bottom(toolsPage,6);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),editPage,gtk_label_new(_("Edición")));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),toolsPage,gtk_label_new(_("Ayudas editoriales")));
	gtk_box_append(GTK_BOX(box),notebook);
	auto appendTextSection = [](GtkWidget *parent,const char *title, GtkWidget *view, int height) {
		GtkWidget *label = gtk_label_new(title);
		gtk_widget_add_css_class(label, "heading");
		gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
		GtkWidget *scroll = gtk_scrolled_window_new();
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
					       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
		gtk_widget_set_size_request(scroll, -1, height);
		gtk_widget_set_vexpand(scroll, TRUE);
		gtk_box_append(GTK_BOX(parent), label);
		gtk_box_append(GTK_BOX(parent), scroll);
	};
	editor.text = textArea(110);
	editor.sourceContext = textArea(72);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(editor.sourceContext), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(editor.sourceContext), FALSE);
	editor.bookComment = textArea(80);
	editor.verseComment = textArea(80);
	editor.proposal = textArea(55); editor.rationale = textArea(55); editor.evidence = textArea(55);
	auto choices=[](const char*const*values){GtkStringList*l=gtk_string_list_new(nullptr);for(guint i=0;values[i];++i)gtk_string_list_append(l,_(values[i]));return gtk_drop_down_new(G_LIST_MODEL(l),nullptr);};
	const char*confidenceChoices[]={"Por evaluar","Baja","Media","Alta",nullptr};
	const char*statusChoices[]={"Sin propuesta","Pendiente de revisión","Aprobada","Rechazada","Variante textual",nullptr};
	editor.confidence=choices(confidenceChoices);editor.decisionStatus=choices(statusChoices);
	appendTextSection(toolsPage,_("Biblia base e interlineal disponible"), editor.sourceContext, 90);
	GtkStringList *parallelNames=gtk_string_list_new(nullptr);
	for(const auto&m:listSqliteModules(moduleDirectory())) {
		if(m.info.id==editor.edition->metadata().projectId || sqliteModuleMetadata(m.path,"versification")!=editor.edition->metadata().versification) continue;
		editor.parallelIds.push_back(m.info.id);gtk_string_list_append(parallelNames,m.info.description.c_str());
	}
	editor.parallel=gtk_drop_down_new(G_LIST_MODEL(parallelNames),nullptr);
	editor.parallelContext=textArea(64);gtk_text_view_set_editable(GTK_TEXT_VIEW(editor.parallelContext),FALSE);
	gtk_box_append(GTK_BOX(toolsPage),gtk_label_new(_("Elige una Biblia instalada con la misma versificación.")));
	gtk_box_append(GTK_BOX(toolsPage),editor.parallel);
	appendTextSection(toolsPage,_("Comparación paralela"),editor.parallelContext,80);
	appendTextSection(editPage,_("Texto del versículo"), editor.text, 115);
	appendTextSection(toolsPage,_("Propuesta de traducción"), editor.proposal, 65);
	appendTextSection(toolsPage,_("Fundamento lingüístico"), editor.rationale, 65);
	appendTextSection(toolsPage,_("Fuentes y evidencia"), editor.evidence, 65);
	GtkWidget*decisionFields=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);gtk_box_append(GTK_BOX(decisionFields),gtk_label_new(_("Confianza")));gtk_box_append(GTK_BOX(decisionFields),editor.confidence);gtk_box_append(GTK_BOX(decisionFields),gtk_label_new(_("Estado")));gtk_box_append(GTK_BOX(decisionFields),editor.decisionStatus);gtk_box_append(GTK_BOX(toolsPage),decisionFields);
	editor.history=textArea(55);gtk_text_view_set_editable(GTK_TEXT_VIEW(editor.history),FALSE);appendTextSection(toolsPage,_("Historial del versículo"),editor.history,70);
	appendTextSection(editPage,_("Comentario del libro"), editor.bookComment, 95);
	appendTextSection(editPage,_("Comentario del versículo"), editor.verseComment, 95);
	editor.sourceLinks = gtk_flow_box_new();
 gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(editor.sourceLinks), GTK_SELECTION_NONE);
 gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(editor.sourceLinks), 8);
 gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(editor.sourceLinks), 4);
 gtk_box_append(GTK_BOX(editPage), editor.sourceLinks);
 GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	editor.status = gtk_label_new(" ");
	gtk_widget_set_hexpand(editor.status, TRUE);
	gtk_label_set_xalign(GTK_LABEL(editor.status), 0.0f);
	gtk_label_set_wrap(GTK_LABEL(editor.status), TRUE);
	gtk_box_append(GTK_BOX(actions), editor.status);
	GtkWidget *saveButton = gtk_button_new_with_label(_("Guardar cambios"));
	gtk_widget_set_name(saveButton, "editable-edition-save-button");
	gtk_widget_add_css_class(saveButton, "suggested-action");
	GtkWidget *closeButton = gtk_button_new_with_label(_("Cerrar"));
	GtkWidget *glossaryButton=gtk_button_new_with_label(_("Glosario…"));
	gtk_box_append(GTK_BOX(actions), glossaryButton);
	gtk_box_append(GTK_BOX(actions), closeButton);
	gtk_box_append(GTK_BOX(actions), saveButton);
	gtk_widget_set_halign(actions, GTK_ALIGN_FILL);
	gtk_box_append(GTK_BOX(box), actions);
	g_signal_connect(editor.book, "notify::selected", G_CALLBACK(selectionChanged), &editor);
	g_signal_connect(editor.chapter, "value-changed", G_CALLBACK(spinSelectionChanged), &editor);
	g_signal_connect(editor.verse, "value-changed", G_CALLBACK(spinSelectionChanged), &editor);
	g_signal_connect(editor.parallel,"notify::selected",G_CALLBACK(parallelChanged),&editor);
	g_signal_connect(saveButton, "clicked", G_CALLBACK(save), &editor);
	g_signal_connect(glossaryButton,"clicked",G_CALLBACK(editGlossary),&editor);
	g_signal_connect(closeButton, "clicked", G_CALLBACK(+[](GtkButton *, gpointer data) {
		gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_CLOSE);
	}), dialog);
 if(initial.book>0){
  for(guint i=0;i<editor.books.size();++i)if(editor.books[i].id==initial.book)gtk_drop_down_set_selected(GTK_DROP_DOWN(editor.book),i);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(editor.chapter),initial.chapter);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(editor.verse),initial.verse);
 }
	loadSelection(&editor);
	loadParallel(&editor);
	gui_fit_dialog_to_screen(GTK_WINDOW(dialog));
	gtk_widget_show(dialog); gui_dialog_run(GTK_DIALOG(dialog));
 for(GtkWidget *control:{editor.book,editor.chapter,editor.verse,editor.parallel})
  g_signal_handlers_disconnect_by_data(control,&editor);
 gui_widget_destroy(dialog);
}

void showComparison(GtkWindow *parent, const std::string &path, const BibleReference &ref)
{
 EditableEditionMetadata metadata;std::vector<EditionComparison> items;std::string error;
 if (!EditableEdition::comparisons(path,metadata,items,error,ref.book,ref.chapter)) return;
 const EditionComparison *item=nullptr;
 for (const auto &v:items) if(v.reference.verse==ref.verse) {item=&v;break;}
 if (!item) return;
 GtkWidget *dialog=gtk_dialog_new_with_buttons(_("Comentario y cambios"),parent,GTK_DIALOG_MODAL,_("Cerrar"),GTK_RESPONSE_CLOSE,NULL);
 gtk_widget_set_name(dialog,"edition-comparison");
 gtk_window_set_default_size(GTK_WINDOW(dialog),740,620);
 GtkWidget *scroll=gtk_scrolled_window_new();
 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
 gtk_widget_set_vexpand(scroll,TRUE);
 GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,18);
 for(auto set:{gtk_widget_set_margin_start,gtk_widget_set_margin_end,gtk_widget_set_margin_top,gtk_widget_set_margin_bottom})set(box,22);
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),box);
 gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),scroll);
 auto label=[](GtkWidget *container,const std::string &text,bool markup=false){
  GtkWidget *w=gtk_label_new(nullptr);
  if(markup)gtk_label_set_markup(GTK_LABEL(w),text.c_str());else gtk_label_set_text(GTK_LABEL(w),text.c_str());
  gtk_label_set_wrap(GTK_LABEL(w),TRUE);
  gtk_label_set_wrap_mode(GTK_LABEL(w),PANGO_WRAP_WORD_CHAR);
  gtk_label_set_max_width_chars(GTK_LABEL(w),66);
  // Keep the interface alignment when a paragraph starts with Hebrew/Greek.
  pango_layout_set_auto_dir(gtk_label_get_layout(GTK_LABEL(w)),FALSE);
  g_signal_connect_after(w,"map",G_CALLBACK(+[](GtkWidget *label,gpointer){
   pango_layout_set_auto_dir(gtk_label_get_layout(GTK_LABEL(label)),FALSE);
   gtk_widget_queue_resize(label);
  }),nullptr);
  gtk_label_set_justify(GTK_LABEL(w),GTK_JUSTIFY_LEFT);
  gtk_label_set_selectable(GTK_LABEL(w),TRUE);gtk_label_set_xalign(GTK_LABEL(w),0);
  gtk_box_append(GTK_BOX(container),w);return w;
 };
 auto heading=[](GtkWidget *widget,double scale=1.0){
  PangoAttrList *attributes=pango_attr_list_new();
  pango_attr_list_insert(attributes,pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  pango_attr_list_insert(attributes,pango_attr_scale_new(scale));
  gtk_label_set_attributes(GTK_LABEL(widget),attributes);pango_attr_list_unref(attributes);
 };
 auto section=[&](const char *title){
  GtkWidget *frame=gtk_frame_new(nullptr),*content=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
  for(auto set:{gtk_widget_set_margin_start,gtk_widget_set_margin_end,gtk_widget_set_margin_top,gtk_widget_set_margin_bottom})set(content,16);
  gtk_frame_set_child(GTK_FRAME(frame),content);gtk_box_append(GTK_BOX(box),frame);
  GtkWidget *caption=label(content,title);heading(caption);gtk_widget_add_css_class(caption,"heading");
  return content;
 };
 GtkWidget *header=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);gtk_box_append(GTK_BOX(box),header);
 GtkWidget *title=label(header,item->bookName+" "+std::to_string(ref.chapter)+":"+std::to_string(ref.verse));
 heading(title,1.15);gtk_widget_set_name(title,"edition-comparison-title");
 gtk_widget_add_css_class(label(header,metadata.name),"dim-label");
 gtk_box_append(GTK_BOX(box),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

 // A study note is the primary content; original text is already in the reader.
 std::string note=item->rationale.empty()?item->comment:item->rationale;
 std::string citation;
 for(const auto &marker:{std::string("\n\nFuente:"),std::string("\n\nSource:")}){
  const auto offset=note.find(marker);
  if(offset!=std::string::npos){citation=note.substr(offset+2);note.resize(offset);break;}
 }
 GtkWidget *article=gtk_box_new(GTK_ORIENTATION_VERTICAL,12);
 gtk_widget_set_name(article,"edition-comment-body");gtk_box_append(GTK_BOX(box),article);
 const auto first=note.find('\n');
 const auto paragraph=note.find("\n\n");
 if(first!=std::string::npos && paragraph!=std::string::npos && first<paragraph &&
    first<=120 && paragraph-first<=360 && note.find('\n',first+1)==paragraph){
  GtkWidget *noteTitle=label(article,note.substr(0,first));heading(noteTitle,1.12);
  gtk_widget_set_name(noteTitle,"edition-note-title");
  GtkWidget *lead=label(article,note.substr(first+1,paragraph-first-1));
  gtk_widget_set_name(lead,"edition-note-lead");
  label(article,note.substr(paragraph+2));
 } else if(!note.empty())label(article,note);
 else label(article,_("No hay un comentario registrado para este versículo."));

 if(item->edited()) {
  const auto diff=editionDifferenceMarkup(item->baseText,item->currentText);
  GtkWidget *textSection=section(_("Cambios respecto a la base"));
  gtk_widget_set_name(textSection,"edition-text-comparison");
  gtk_widget_add_css_class(label(textSection,metadata.sourceName),"dim-label");
  gtk_widget_set_name(label(textSection,diff.first,true),"edition-base-text");
  gtk_box_append(GTK_BOX(textSection),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
  heading(label(textSection,metadata.name));
  gtk_widget_set_name(label(textSection,diff.second,true),"edition-current-text");
 }
 if(!item->proposal.empty() && item->proposal!=item->currentText){
  label(section(_("Propuesta de traducción")),item->proposal);
 }
 auto details=[&](const char *caption,const std::string &text){
  GtkWidget *expander=gtk_expander_new(caption),*content=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
  gtk_widget_set_margin_top(content,10);label(content,text);
  gtk_expander_set_child(GTK_EXPANDER(expander),content);gtk_box_append(GTK_BOX(box),expander);
  return expander;
 };
 gtk_box_append(GTK_BOX(box),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
 GtkWidget *sources=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);gtk_box_append(GTK_BOX(box),sources);
 heading(label(sources,_("Fuentes")));
 if(!citation.empty()){
  std::istringstream lines(citation);std::string line,caption;
  while(std::getline(lines,line)){
   if(line.compare(0,8,"https://")==0 || line.compare(0,7,"http://")==0)continue;
   if(!caption.empty())caption+="\n";caption+=line;
  }
  if(!caption.empty())label(sources,caption);
 }
 GtkWidget *links=gtk_flow_box_new();gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(links),GTK_SELECTION_NONE);
 gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(links),8);gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(links),6);
 gtk_box_append(GTK_BOX(sources),links);populateSourceLinks(links,item->comment+"\n"+item->evidence+"\n"+item->rationale);
 if(!gtk_widget_get_visible(links) && item->evidence.empty() && citation.empty())label(sources,_("No se han registrado fuentes."));
 std::string editorial=std::string(_("Basada en: "))+metadata.sourceName;
 editorial+="\n"+std::string(item->edited()?_("Texto modificado respecto a la base."):_("El texto bíblico conserva la versión base."));
 if(item->status=="Pendiente de revisión" || item->status=="pendiente de revisión")
  editorial+="\n"+std::string(item->edited()?_("Traducción pendiente de revisión"):_("Comentario pendiente de revisión"));
 else if(!item->status.empty())editorial+="\n"+item->status;
 gtk_widget_set_name(details(_("Detalles editoriales"),editorial),"edition-editorial-details");
 if(!item->comment.empty() && item->comment!=item->rationale && !item->rationale.empty())
  details(_("Ver registro completo de la nota"),item->comment);
 if(!item->evidence.empty())details(_("Ver evidencia completa"),item->evidence);
 GtkWidget *close=gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog),GTK_RESPONSE_CLOSE);
 gtk_widget_set_margin_top(close,12);gtk_widget_set_margin_bottom(close,10);
 if(!parent || g_strcmp0(gtk_widget_get_name(GTK_WIDGET(parent)),"editable-edition-editor")) {
  GtkWidget *edit=gtk_dialog_add_button(GTK_DIALOG(dialog),_("Editar y ver cambios…"),GTK_RESPONSE_ACCEPT);
  gtk_widget_add_css_class(edit,"suggested-action");
  gtk_widget_set_margin_start(edit,12);gtk_widget_set_margin_end(edit,12);
  gtk_widget_set_margin_top(edit,12);gtk_widget_set_margin_bottom(edit,10);
 }
 gui_fit_dialog_to_screen(GTK_WINDOW(dialog));gtk_widget_show(dialog);
 const int response=gui_dialog_run(GTK_DIALOG(dialog));gui_widget_destroy(dialog);
 if(response==GTK_RESPONSE_ACCEPT)showEditor(parent,path,ref);
}

bool hasUnsavedChanges(Editor *editor)
{
 const auto ref=selectedReference(*editor);std::string stored,error;
 if(!editor->edition->verseText(ref,stored,error) || stored!=bufferText(editor->text))return true;
 editor->edition->verseComment(ref,stored,error);
 if(stored!=bufferText(editor->verseComment))return true;
 editor->edition->bookComment(ref.book,stored,error);
 if(stored!=bufferText(editor->bookComment))return true;
 EditionDecision decision;editor->edition->decision(ref,decision,error);
 return decision.proposal!=bufferText(editor->proposal) || decision.rationale!=bufferText(editor->rationale) ||
        decision.evidence!=bufferText(editor->evidence) ||
        editor->edition->metadata().name!=gtk_editable_get_text(GTK_EDITABLE(editor->name));
}

bool requireSavedChanges(Editor *editor)
{
 if(!hasUnsavedChanges(editor))return true;
 gtk_label_set_text(GTK_LABEL(editor->status),_("Guarda los cambios antes de comparar o navegar a otro aporte."));
 return false;
}

void compareSelection(GtkButton *, gpointer data)
{
 auto *editor=static_cast<Editor*>(data);
 if(!requireSavedChanges(editor))return;
 showComparison(GTK_WINDOW(editor->window),modulePath(editor->edition->metadata().projectId),selectedReference(*editor));
}

struct ChangesView {
 Editor *editor = nullptr;
 GtkWidget *dialog,*list,*filter,*count,*search = nullptr,*goTo = nullptr;
 std::vector<EditionComparison> items;
 std::vector<std::string> searchText;
 BibleReference selected;
 std::string module;
};

// Match Spanish book names and commentary regardless of accents or case.
std::string contributionSearchText(const std::string &text)
{
 gchar *folded = g_utf8_casefold(text.c_str(), -1);
 gchar *normalized = g_utf8_normalize(folded, -1, G_NORMALIZE_NFD);
 std::string result;
 for (const gchar *p = normalized; p && *p; p = g_utf8_next_char(p)) {
  const gunichar c = g_utf8_get_char(p);
  if (!g_unichar_ismark(c)) result.append(p, g_utf8_next_char(p) - p);
 }
 g_free(normalized); g_free(folded);
 return result;
}

std::string contributionReference(const EditionComparison &item)
{
 return item.bookName+" "+std::to_string(item.reference.chapter)+":"+std::to_string(item.reference.verse);
}

void populateChanges(GObject *,GParamSpec *,gpointer data)
{
 auto *view=static_cast<ChangesView*>(data);
 while(GtkWidget *child=gtk_widget_get_first_child(view->list))gtk_list_box_remove(GTK_LIST_BOX(view->list),child);
 const int selected=gtk_drop_down_get_selected(GTK_DROP_DOWN(view->filter));
 const int filter=view->editor?selected:selected-1;
 const std::string query=view->search?contributionSearchText(gtk_editable_get_text(GTK_EDITABLE(view->search))):"";
 unsigned count=0;
 for(std::size_t i=0;i<view->items.size();++i){
  const auto &item=view->items[i];
  if(!(filter<0?(item.edited() || item.commented() || item.pending()):filter==0?item.edited():filter==1?item.commented():item.pending()))continue;
  if(!query.empty() && view->searchText[i].find(query)==std::string::npos)continue;
  std::string text=contributionReference(item);
  if(item.edited())text+=" · "+std::string(_("Editado"));
  if(item.commented())text+=" · "+std::string(_("Comentario"));
  if(item.pending())text+=" · "+std::string(_("Propuesta pendiente"));
  GtkWidget *label=gtk_label_new(text.c_str());gtk_label_set_xalign(GTK_LABEL(label),0);gtk_label_set_wrap(GTK_LABEL(label),TRUE);
  gtk_widget_set_margin_top(label,8);gtk_widget_set_margin_bottom(label,8);
  GtkWidget *content=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
  gtk_widget_set_margin_start(content,12);gtk_widget_set_margin_end(content,12);
  gtk_box_append(GTK_BOX(content),label);
  if(!view->editor){
   const auto &note=item.commented()?item.comment:item.rationale;
   const std::string title=note.substr(0,note.find('\n'));
   if(!title.empty()){
    GtkWidget *heading=gtk_label_new(title.c_str());gtk_label_set_xalign(GTK_LABEL(heading),0);
    gtk_label_set_ellipsize(GTK_LABEL(heading),PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(heading,"heading");gtk_box_prepend(GTK_BOX(content),heading);
    gtk_widget_set_margin_top(heading,10);
   }
  }
  GtkWidget *row=gtk_list_box_row_new();gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row),content);
  g_object_set_data(G_OBJECT(row),"edition-index",GUINT_TO_POINTER(i+1));
  gtk_list_box_append(GTK_LIST_BOX(view->list),row);++count;
 }
 const std::string message=count?std::to_string(count)+" · "+(view->editor?_("Selecciona un versículo para editarlo."):_("Selecciona un aporte para leer su comentario.")):
  view->items.empty()?_("Esta Biblia aún no tiene aportes en sus versículos."):_("No hay versículos para este filtro.");
 gtk_label_set_text(GTK_LABEL(view->count),message.c_str());
}

void showContributions(GtkWindow *parent,const std::string &module)
{
 ChangesView view;view.module=module;EditableEditionMetadata metadata;std::string error;
 if(!EditableEdition::comparisons(modulePath(module),metadata,view.items,error)){
  GtkWidget *message=gtk_message_dialog_new(parent,GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,
   GTK_BUTTONS_CLOSE,"%s",_("No se pudieron cargar los aportes de esta Biblia."));
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message),"%s",error.c_str());
  gtk_widget_show(message);gui_dialog_run(GTK_DIALOG(message));gui_widget_destroy(message);return;
 }
 // Keep only actual contributions; search never scans unchanged Bible text.
 std::vector<EditionComparison> contributions;
 for(auto &item:view.items)
  if(item.edited() || item.commented() || item.pending())contributions.push_back(std::move(item));
 view.items=std::move(contributions);
 for(const auto &item:view.items)
  view.searchText.push_back(contributionSearchText(contributionReference(item)+"\n"+item.comment+"\n"+item.rationale+"\n"+item.evidence+"\n"+item.proposal));
 view.dialog=gtk_dialog_new_with_buttons(_("Aportes a esta Biblia"),parent,GTK_DIALOG_MODAL,_("Cerrar"),GTK_RESPONSE_CLOSE,NULL);
 view.goTo=gtk_dialog_add_button(GTK_DIALOG(view.dialog),_("Ir al versículo"),GTK_RESPONSE_ACCEPT);
 gtk_widget_set_name(view.goTo,"edition-contribution-goto");
 gtk_widget_set_margin_start(view.goTo,12);gtk_widget_set_sensitive(view.goTo,FALSE);
 gtk_widget_set_name(view.dialog,"edition-contributions-dialog");
 gtk_window_set_default_size(GTK_WINDOW(view.dialog),680,560);
 GtkWidget *box=gtk_dialog_get_content_area(GTK_DIALOG(view.dialog));
 gtk_box_set_spacing(GTK_BOX(box),12);
 for(auto set:{gtk_widget_set_margin_start,gtk_widget_set_margin_end,gtk_widget_set_margin_top,gtk_widget_set_margin_bottom})set(box,20);
 GtkWidget *header=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
 GtkWidget *title=gtk_label_new(metadata.name.c_str());gtk_widget_add_css_class(title,"title-2");
 gtk_label_set_xalign(GTK_LABEL(title),0);gtk_label_set_wrap(GTK_LABEL(title),TRUE);gtk_widget_set_hexpand(title,TRUE);
 GtkWidget *exportPdf=gtk_button_new_with_label(_("Exportar PDF…"));gtk_widget_set_name(exportPdf,"edition-contribution-export-pdf");
 gtk_widget_set_valign(exportPdf,GTK_ALIGN_CENTER);gtk_box_append(GTK_BOX(header),title);gtk_box_append(GTK_BOX(header),exportPdf);
 gtk_box_append(GTK_BOX(box),header);
 g_signal_connect(exportPdf,"clicked",G_CALLBACK(+[](GtkButton*,gpointer data){
  auto *v=static_cast<ChangesView*>(data);const auto path=modulePath(v->module);
  gui_export_study_bible_pdf_path(GTK_WINDOW(v->dialog),path.c_str());
 }),&view);
 GtkWidget *hint=gtk_label_new(_("Encuentra comentarios, cambios de texto y propuestas de esta edición."));
 gtk_label_set_xalign(GTK_LABEL(hint),0);gtk_label_set_wrap(GTK_LABEL(hint),TRUE);gtk_box_append(GTK_BOX(box),hint);
 view.search=gtk_search_entry_new();gtk_widget_set_name(view.search,"edition-contribution-search");
 g_object_set(view.search,"placeholder-text",_("Buscar por libro, referencia o contenido…"),NULL);
 gtk_box_append(GTK_BOX(box),view.search);
 const char *choices[]={_("Todos los aportes"),_("Textos modificados"),_("Con comentarios"),_("Propuestas pendientes"),nullptr};
 view.filter=gtk_drop_down_new_from_strings(choices);gtk_widget_set_name(view.filter,"edition-contribution-filter");
 gtk_box_append(GTK_BOX(box),view.filter);
 view.count=gtk_label_new("");gtk_widget_set_name(view.count,"edition-contribution-count");
 gtk_label_set_xalign(GTK_LABEL(view.count),0);gtk_label_set_wrap(GTK_LABEL(view.count),TRUE);gtk_box_append(GTK_BOX(box),view.count);
 view.list=gtk_list_box_new();gtk_widget_set_name(view.list,"edition-contribution-list");
 GtkWidget *scroll=gtk_scrolled_window_new();gtk_widget_set_vexpand(scroll,TRUE);
 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),view.list);gtk_box_append(GTK_BOX(box),scroll);
 g_signal_connect(view.filter,"notify::selected",G_CALLBACK(populateChanges),&view);
 g_signal_connect(view.search,"changed",G_CALLBACK(+[](GtkEditable*,gpointer data){populateChanges(nullptr,nullptr,data);}),&view);
 g_signal_connect(view.list,"row-selected",G_CALLBACK(+[](GtkListBox*,GtkListBoxRow *row,gpointer data){
  auto *v=static_cast<ChangesView*>(data);
  const guint i=row?GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row),"edition-index")):0;
  const bool valid=i && i<=v->items.size();
  v->selected=valid?v->items[i-1].reference:BibleReference{};
  gtk_widget_set_sensitive(v->goTo,valid);
 }),&view);
 g_signal_connect(view.list,"row-activated",G_CALLBACK(+[](GtkListBox*,GtkListBoxRow *row,gpointer data){
  auto *v=static_cast<ChangesView*>(data);const guint i=GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row),"edition-index"));
  if(i && i<=v->items.size()){
   gtk_list_box_select_row(GTK_LIST_BOX(v->list),row);
   const auto ref=v->items[i-1].reference;
   // Reading an item must not dismiss the browser or reset its filters/scroll.
   showComparison(GTK_WINDOW(v->dialog),modulePath(v->module),ref);
  }
 }),&view);
 populateChanges(nullptr,nullptr,&view);gui_fit_dialog_to_screen(GTK_WINDOW(view.dialog));
 gtk_widget_show(view.dialog);const int response=gui_dialog_run(GTK_DIALOG(view.dialog));
 g_signal_handlers_disconnect_by_data(view.search,&view);g_signal_handlers_disconnect_by_data(view.filter,&view);
 g_signal_handlers_disconnect_by_data(view.list,&view);gui_widget_destroy(view.dialog);
 if(response==GTK_RESPONSE_ACCEPT && view.selected.book>0){
  for(const auto &book:canonicalBibleBooks())if(book.bookId==view.selected.book){
   const std::string key=std::string(book.osis)+" "+std::to_string(view.selected.chapter)+":"+std::to_string(view.selected.verse);
   main_display_bible(module.c_str(),key.c_str());break;
  }
 }
}

void browseChanges(GtkButton *,gpointer data)
{
 auto *editor=static_cast<Editor*>(data);
 if(!requireSavedChanges(editor))return;
 ChangesView view;view.editor=editor;EditableEditionMetadata metadata;std::string error;
 if(!EditableEdition::comparisons(modulePath(editor->edition->metadata().projectId),metadata,view.items,error)){
  gtk_label_set_text(GTK_LABEL(editor->status),error.c_str());return;
 }
 view.dialog=gtk_dialog_new_with_buttons(_("Cambios respecto a la base"),GTK_WINDOW(editor->window),GTK_DIALOG_MODAL,_("Cerrar"),GTK_RESPONSE_CLOSE,NULL);
 gtk_widget_set_name(view.dialog,"edition-changes-dialog");gtk_window_set_default_size(GTK_WINDOW(view.dialog),600,440);
 GtkWidget *box=gtk_dialog_get_content_area(GTK_DIALOG(view.dialog));gtk_box_set_spacing(GTK_BOX(box),12);
 for(auto set:{gtk_widget_set_margin_start,gtk_widget_set_margin_end,gtk_widget_set_margin_top,gtk_widget_set_margin_bottom})set(box,16);
 gtk_box_append(GTK_BOX(box),gtk_label_new((metadata.name+" · "+_("Basada en: ")+metadata.sourceName).c_str()));
 const char *choices[]={_("Textos modificados"),_("Con comentarios"),_("Propuestas pendientes"),nullptr};
 view.filter=gtk_drop_down_new_from_strings(choices);gtk_widget_set_name(view.filter,"edition-change-filter");gtk_box_append(GTK_BOX(box),view.filter);
 view.count=gtk_label_new("");gtk_widget_set_name(view.count,"edition-change-count");gtk_box_append(GTK_BOX(box),view.count);
 view.list=gtk_list_box_new();gtk_widget_set_name(view.list,"edition-change-list");GtkWidget *scroll=gtk_scrolled_window_new();gtk_widget_set_vexpand(scroll,TRUE);gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),view.list);gtk_box_append(GTK_BOX(box),scroll);
 g_signal_connect(view.filter,"notify::selected",G_CALLBACK(populateChanges),&view);
 g_signal_connect(view.list,"row-activated",G_CALLBACK(+[](GtkListBox*,GtkListBoxRow *row,gpointer data){
  auto *v=static_cast<ChangesView*>(data);const guint i=GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row),"edition-index"));
  if(!i || i>v->items.size())return;
  const auto r=v->items[i-1].reference;
  v->editor->loading=true;
  for(guint b=0;b<v->editor->books.size();++b)if(v->editor->books[b].id==r.book)gtk_drop_down_set_selected(GTK_DROP_DOWN(v->editor->book),b);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->editor->chapter),r.chapter);gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->editor->verse),r.verse);
  v->editor->loading=false;loadSelection(v->editor);loadParallel(v->editor);
  gtk_dialog_response(GTK_DIALOG(v->dialog),GTK_RESPONSE_CLOSE);
 }),&view);
 populateChanges(nullptr,nullptr,&view);gtk_widget_show(view.dialog);gui_dialog_run(GTK_DIALOG(view.dialog));gui_widget_destroy(view.dialog);
}

void sourceItemSetup(GtkSignalListItemFactory *, GtkListItem *item, gpointer)
{
	GtkWidget *label = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 48);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_list_item_set_child(item, label);
}

void sourceItemBind(GtkSignalListItemFactory *, GtkListItem *item, gpointer)
{
	GtkStringObject *source = GTK_STRING_OBJECT(gtk_list_item_get_item(item));
	GtkWidget *label = gtk_list_item_get_child(item);
	gtk_label_set_text(GTK_LABEL(label), gtk_string_object_get_string(source));
}

void createProject(GtkButton *, gpointer data)
{
	auto *state = static_cast<std::pair<GtkWindow *, GtkWidget *> *>(data);
	std::vector<SqliteManagedModule> eligible;
	for (const auto &module : listSqliteModules(moduleDirectory())) {
		const std::string license = sqliteModuleMetadata(module.path, "license");
		if (EditableEdition::sourceMayBeDerived(module.info.id, license))
			eligible.push_back(module);
	}
	if (eligible.empty()) {
		gtk_label_set_text(GTK_LABEL(state->second),
			_("No hay Biblias instaladas que permitan crear derivados.")); return;
	}
	GtkWidget *d = gtk_dialog_new_with_buttons(_("Crear Biblia de estudio"), state->first,
		GTK_DIALOG_MODAL, _("Cancelar"), GTK_RESPONSE_CANCEL,
		_("Crear"), GTK_RESPONSE_ACCEPT, NULL);
	gtk_widget_set_name(d, "editable-edition-create");
	GtkWidget *createButton = gtk_dialog_get_widget_for_response(GTK_DIALOG(d),
								 GTK_RESPONSE_ACCEPT);
	gtk_widget_set_margin_start(createButton, 10);
	gtk_window_set_default_size(GTK_WINDOW(d), 520, -1);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d));
	gtk_orientable_set_orientation(GTK_ORIENTABLE(box), GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing(GTK_BOX(box), 8);
	gtk_widget_set_margin_start(box, 20);
	gtk_widget_set_margin_end(box, 20);
	gtk_widget_set_margin_top(box, 16);
	gtk_widget_set_margin_bottom(box, 12);
	GtkStringList *names = gtk_string_list_new(nullptr);
	for (const auto &m : eligible) gtk_string_list_append(names, m.info.description.c_str());
	GtkWidget *source = gtk_drop_down_new(G_LIST_MODEL(names), nullptr);
	gtk_widget_set_name(source, "editable-edition-source");
	gtk_widget_set_hexpand(source, TRUE);
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(sourceItemSetup), nullptr);
	g_signal_connect(factory, "bind", G_CALLBACK(sourceItemBind), nullptr);
	gtk_drop_down_set_factory(GTK_DROP_DOWN(source), factory);
	gtk_drop_down_set_list_factory(GTK_DROP_DOWN(source), factory);
	g_object_unref(factory);
	GtkWidget *name = gtk_entry_new();
	gtk_editable_set_text(GTK_EDITABLE(name), _("Mi Biblia de estudio"));
	GtkWidget *sourceLabel = gtk_label_new(_("Biblia base"));
	gtk_label_set_xalign(GTK_LABEL(sourceLabel), 0.0f);
	gtk_box_append(GTK_BOX(box), sourceLabel);
	gtk_box_append(GTK_BOX(box), source);
	GtkWidget *nameLabel = gtk_label_new(_("Nombre de la nueva Biblia"));
	gtk_label_set_xalign(GTK_LABEL(nameLabel), 0.0f);
	gtk_box_append(GTK_BOX(box), nameLabel);
	gtk_box_append(GTK_BOX(box), name);
	gtk_widget_show(d);
	if (gui_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
		const guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(source));
		const std::string title = gtk_editable_get_text(GTK_EDITABLE(name));
		if (i < eligible.size() && !title.empty()) {
			gchar *uuid = g_uuid_string_random();
			std::string id = std::string("elim-") + std::string(uuid, 12); g_free(uuid);
			const std::string path = moduleDirectory() + "/" + id + ".sqlite";
			std::string error;
			if (EditableEdition::createFromModule(eligible[i].path, path, id, title, error)) {
				main_update_module_lists();
				gui_set_statusbar(_("Biblia de estudio creada y guardada."));
				gui_widget_destroy(d);
				showEditor(state->first, path); return;
			} else gtk_label_set_text(GTK_LABEL(state->second), error.c_str());
		}
	}
	gui_widget_destroy(d);
}
}

void gui_open_editable_edition_dialog(GtkWindow *parent)
{
	GtkWidget *d = gtk_dialog_new_with_buttons(_("Biblias de estudio"), parent,
		GTK_DIALOG_MODAL, _("Cerrar"), GTK_RESPONSE_CLOSE, NULL);
	gtk_widget_set_name(d, "editable-edition-manager");
	gtk_window_set_default_size(GTK_WINDOW(d), 600, -1);
	GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(d));
	gtk_orientable_set_orientation(GTK_ORIENTABLE(box), GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing(GTK_BOX(box), 14);
	gtk_widget_set_margin_start(box, 22);
	gtk_widget_set_margin_end(box, 22);
	gtk_widget_set_margin_top(box, 16);
	gtk_widget_set_margin_bottom(box, 16);
	GtkWidget *label = gtk_label_new(_("Crea una copia editable de una Biblia que permita obras derivadas, o continúa trabajando en un proyecto existente."));
	gtk_widget_set_name(label, "editable-edition-description");
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 66);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_box_append(GTK_BOX(box), label);
	GtkStringList *store = gtk_string_list_new(nullptr);
	std::vector<std::string> projectPaths;
	for (const auto &m : listSqliteModules(moduleDirectory())) {
		if (sqliteModuleMetadata(m.path, "edition_format") == "biblia-elim-edition") {
			gtk_string_list_append(GTK_STRING_LIST(store), m.info.description.c_str());
			projectPaths.push_back(m.path);
		}
	}
	GtkWidget *selector = gtk_drop_down_new(G_LIST_MODEL(store), nullptr);
	gtk_widget_set_sensitive(selector, !projectPaths.empty());
	GtkWidget *status = gtk_label_new(" "); gtk_label_set_wrap(GTK_LABEL(status), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(status), 72);
	gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
	GtkWidget *existing = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	GtkWidget *existingTitle = gtk_label_new(_("Proyectos existentes"));
	gtk_widget_add_css_class(existingTitle, "heading");
	gtk_label_set_xalign(GTK_LABEL(existingTitle), 0.0f);
	gtk_box_append(GTK_BOX(existing), existingTitle);
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_hexpand(selector, TRUE);
	GtkWidget *open = gtk_button_new_with_label(_("Abrir proyecto"));
	GtkWidget *exportPdf = gtk_button_new_with_label(_("Exportar PDF…"));
	gtk_widget_set_name(exportPdf,"editable-edition-export-pdf");
	gtk_widget_set_sensitive(exportPdf,!projectPaths.empty());
	gtk_box_append(GTK_BOX(row), selector); gtk_box_append(GTK_BOX(row), open);
	gtk_box_append(GTK_BOX(row),exportPdf);
	gtk_box_append(GTK_BOX(existing), row);
	gtk_box_append(GTK_BOX(box), existing);
	gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *createSection = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	GtkWidget *createCopy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	GtkWidget *createTitle = gtk_label_new(_("Empezar una Biblia nueva"));
	GtkWidget *createHint = gtk_label_new(_("La fuente se conserva intacta y solo aparecen aquí Biblias aptas para redistribuir."));
	gtk_widget_add_css_class(createTitle, "heading");
	gtk_label_set_xalign(GTK_LABEL(createTitle), 0.0f);
	gtk_label_set_xalign(GTK_LABEL(createHint), 0.0f);
	gtk_label_set_wrap(GTK_LABEL(createHint), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(createHint), 48);
	gtk_box_append(GTK_BOX(createCopy), createTitle);
	gtk_box_append(GTK_BOX(createCopy), createHint);
	gtk_widget_set_hexpand(createCopy, TRUE);
	GtkWidget *create = gtk_button_new_with_label(_("Crear Biblia…"));
	gtk_widget_set_name(create, "editable-edition-create-button");
	gtk_widget_set_valign(create, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(createSection), createCopy);
	gtk_box_append(GTK_BOX(createSection), create);
	gtk_box_append(GTK_BOX(box), createSection);
	gtk_box_append(GTK_BOX(box), status);
	std::pair<GtkWindow *, GtkWidget *> createState{parent, status};
	g_signal_connect(create, "clicked", G_CALLBACK(createProject), &createState);
	// The nested editor is modal; close the chooser before opening it.
	struct OpenState { GtkWidget *dialog; GtkWidget *selector; std::vector<std::string> *paths; } openState{d, selector, &projectPaths};
	g_signal_connect(exportPdf,"clicked",G_CALLBACK(+[](GtkButton*,gpointer data){
		auto *state=static_cast<OpenState*>(data);const guint i=gtk_drop_down_get_selected(GTK_DROP_DOWN(state->selector));
		if(i<state->paths->size())gui_export_study_bible_pdf_path(GTK_WINDOW(state->dialog),(*state->paths)[i].c_str());
	}),&openState);
	g_signal_connect(open, "clicked", G_CALLBACK(+[](GtkButton *, gpointer data) {
		auto *state = static_cast<OpenState *>(data);
		const guint index = gtk_drop_down_get_selected(GTK_DROP_DOWN(state->selector));
		if (index >= state->paths->size()) return;
		const std::string path = (*state->paths)[index];
		gtk_dialog_response(GTK_DIALOG(state->dialog), GTK_RESPONSE_ACCEPT);
		showEditor(GTK_WINDOW(gtk_window_get_transient_for(GTK_WINDOW(state->dialog))), path);
	}), &openState);
	gtk_widget_show(d); gui_dialog_run(GTK_DIALOG(d)); gui_widget_destroy(d);
}

extern "C" void gui_show_edition_comparison(GtkWindow *parent,const char *module,int book,int chapter,int verse)
{
 if(!module || std::string(module).find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)return;
 showComparison(parent,modulePath(module),{0,book,chapter,verse});
}

extern "C" void gui_open_edition_editor(GtkWindow *parent,const char *module)
{
 if(!module || std::string(module).find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)return;
 showEditor(parent,modulePath(module));
}

extern "C" void gui_open_edition_contributions(GtkWindow *parent,const char *module)
{
 if(!module || !*module || std::string(module).find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)return;
 showContributions(parent,module);
}

extern "C" GtkWidget *gui_edition_contributions_button_new(gboolean compact)
{
 GtkWidget *contributionsButton=compact?gtk_button_new_from_icon_name("document-edit-symbolic"):gtk_button_new_with_label(_("Aportes"));
 contributionsButtons.insert(contributionsButton);
 g_object_weak_ref(G_OBJECT(contributionsButton),+[](gpointer,GObject *object){contributionsButtons.erase(reinterpret_cast<GtkWidget*>(object));},nullptr);
 gtk_widget_set_name(contributionsButton,compact?"reading-edition-contributions-button":"edition-contributions-button");
 gtk_widget_add_css_class(contributionsButton,"elim-pill");
 gtk_widget_set_tooltip_text(contributionsButton,_("Aportes: buscar comentarios y cambios de esta Biblia de estudio"));
 gtk_widget_set_visible(contributionsButton,FALSE);
 g_signal_connect(contributionsButton,"clicked",G_CALLBACK(+[](GtkButton *button,gpointer){
  const char *id=static_cast<const char*>(g_object_get_data(G_OBJECT(button),"edition-module"));
  // Copy the ID: navigation can replace the button's captured module.
  const std::string module=id?id:"";
  gui_open_edition_contributions(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),module.c_str());
 }),nullptr);
 return contributionsButton;
}

extern "C" void gui_edition_contributions_button_sync(const char *module)
{
 for(GtkWidget *contributionsButton:contributionsButtons){
 const char *previous=static_cast<const char*>(g_object_get_data(G_OBJECT(contributionsButton),"edition-module"));
 if(!g_strcmp0(previous,module))continue;
 g_object_set_data_full(G_OBJECT(contributionsButton),"edition-module",g_strdup(module),g_free);
 const bool valid=module && *module && std::string(module).find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;
 gtk_widget_set_visible(contributionsButton,valid && sqliteModuleMetadata(modulePath(module),"edition_format")=="biblia-elim-edition");
 }
}
