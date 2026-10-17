#include "backend/editable_edition.h"
#include "backend/edition_diff.h"
#include "backend/sqlite/sqlite_bible_backend.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#include <cstdio>
#include <string>
#include <memory>
#include <vector>

namespace {
int failures = 0;
void check(bool condition, const char *message)
{
	if (!condition) { g_printerr("FAIL: %s\n", message); ++failures; }
}
}

int main()
{
	check(EditableEdition::sourceMayBeDerived("TorresAmat", "Public Domain"),
	      "bundled public-domain Spanish Bible should be eligible");
	check(EditableEdition::sourceMayBeDerived("SpaRV", "Public Domain"),
	      "public-domain Reina-Valera 1909 should be eligible");
	check(!EditableEdition::sourceMayBeDerived("SpaRVG", "CC BY-NC-ND 4.0"),
	      "no-derivatives source must be rejected");
	check(!EditableEdition::sourceMayBeDerived("SpaRV1909", "Copyrighted; Permission to distribute granted to CrossWire"),
	      "CrossWire-only permission must be rejected");
	check(!EditableEdition::sourceMayBeDerived("NacarColunga", "Copyrighted"),
	      "copyrighted source must be rejected");
	check(!EditableEdition::sourceMayBeDerived("SpaPlatense", "Public Domain"),
	      "source with unresolved rights status must not be allowlisted");
	check(!EditableEdition::sourceMayBeDerived("MadeUpBible", "Public Domain"),
	      "arbitrary self-declared public-domain module must be rejected");

	gchar *directory = g_dir_make_tmp("elim-edition-test-XXXXXX", nullptr);
	if (!directory) return 1;
	const std::string path = std::string(directory) + "/project.sqlite";
	const std::string sourcePath = std::string(directory) + "/SpaRV.sqlite";
	const std::string clonedPath = std::string(directory) + "/elim-clone.sqlite";
	const std::string deniedPath = std::string(directory) + "/denied.sqlite";
	EditableEditionMetadata metadata;
	metadata.projectId = "elim-study-1";
	metadata.name = "Biblia de estudio del equipo";
	metadata.language = "es";
	metadata.versification = "kjv";
	metadata.sourceModuleId = "SpaRV";
	metadata.sourceName = "Reina-Valera 1909";
	metadata.sourceLicense = "Public Domain";
	metadata.sourceAttribution = "Reina-Valera 1909; dominio público";
	std::vector<SqliteImportBook> books = {
		{1, 1, 1, "Gen", "Génesis", "Gn"},
	};
	BibleReference ref{1, 1, 1, 1};
	std::vector<SqliteImportVerse> verses = {
		{ref, "En el principio creó Dios los cielos y la tierra."},
		{{1, 1, 1, 2}, "Y la tierra estaba desordenada y vacía."},
	};
	verses[0].headings.push_back({"La creación"});
	BibleWordInfo word;
	word.start = 0; word.length = 2; word.text = "En"; word.strong = "H7225";
	word.strongs.push_back({StrongLanguage::Hebrew, 7225});
	verses[0].words.push_back(word);
	verses[0].spans.push_back({0, 2, BibleTextStyle::Added});
	BibleFootnote footnote;
	footnote.offset = 2; footnote.label = "a"; footnote.body = "Nota de prueba.";
	verses[0].footnotes.push_back(footnote);
	BibleCrossReference crossref;
	crossref.offset = 2; crossref.displayText = "Gn 1:2";
	crossref.references.push_back({1, 1, 1, 2});
	verses[0].crossReferences.push_back(crossref);
	std::string error, text;
	SqliteModuleMetadata sourceMetadata;
	sourceMetadata.moduleId = "SpaRV";
	sourceMetadata.name = "Reina-Valera 1909";
	sourceMetadata.language = "es";
	sourceMetadata.versification = "kjv";
	sourceMetadata.description = sourceMetadata.name;
	sourceMetadata.license = "Public Domain";
	sourceMetadata.source = "Reina-Valera 1909; dominio público";
	check(SqliteModuleWriter().write(sourceMetadata, books, verses, sourcePath, error),
	      "write installable source Bible fixture");
	check(EditableEdition::createFromModule(sourcePath, clonedPath, "elim-clone",
		"Biblia derivada", error),
	      "create editable edition as a full SQLite copy of an eligible source");
	if (auto clone = EditableEdition::open(clonedPath, error)) {
		check(clone->metadata().name == "Biblia derivada" &&
		      clone->metadata().sourceModuleId == "SpaRV" &&
		      clone->metadata().sourceLicense == "Public Domain" &&
		      clone->metadata().sourceFingerprint.size() == 64,
		      "cloned project records title, source, license, and source fingerprint");
		check(clone->verseText(ref, text, error) && text == verses[0].text,
		      "cloned project retains all source verse text");
	} else check(false, "open copied editable project");
	{
		SqliteBibleBackend backend(directory);
		const auto sourceContent = backend.getVerseContent("SpaRV", ref, true);
		const auto cloneContent = backend.getVerseContent("elim-clone", ref, true);
		check(sourceContent.valid && cloneContent.valid &&
		      sourceContent.plainText == cloneContent.plainText &&
		      cloneContent.headings.size() == 1 && cloneContent.words.size() == 1 &&
		      cloneContent.footnotes.size() == 1 && cloneContent.crossReferences.size() == 1,
		      "source and full editable clone both open with source apparatus preserved");
	}
	{
		sqlite3 *legacy = nullptr;
		check(sqlite3_open(clonedPath.c_str(), &legacy) == SQLITE_OK,
		      "open clone to emulate a compatible SQLite v1 module with optional tables absent");
		char *message = nullptr;
		const int dropped = legacy ? sqlite3_exec(legacy,
			"DROP TABLE cross_reference_range_targets; DROP TABLE edition_glossary; "
			"DROP TABLE editorial_decisions; DROP TABLE verse_revisions", nullptr, nullptr,
			&message) : SQLITE_ERROR;
		check(dropped == SQLITE_OK,
		      "emulate legacy SQLite modules without optional annotation and editorial tables");
		if (message) sqlite3_free(message);
		if (legacy) sqlite3_close(legacy);
	}
	{
		auto migrated=EditableEdition::open(clonedPath,error);
		check(migrated && migrated->setGlossaryEntry({"agape","amor","Prueba de migración"},error),
		      "opening an older editable project adds the editorial tools schema safely");
	}
	{
		SqliteBibleBackend reader(directory);
		BibleKeyInfo key;
		check(reader.resolveKey("elim-clone", "Gen 1:1", key),
		      "resolve a verse in the actively read edition");
		check(reader.getVerseContent("elim-clone", ref, true).valid,
		      "read the verse while its SQLite module remains open");
		check(!reader.setBook("elim-clone", "", 1, 1).empty(),
		      "read first book while edition is open");
		check(!reader.setChapter("elim-clone", "Gen 1:1", 1).empty(),
		      "read first chapter while edition is open");
		check(!reader.navigate("elim-clone", "Gen 1:1", 1).empty(),
		      "navigate while edition is open");
		check(!reader.osisRefFromKey("elim-clone", "Gen 1:1").empty(),
		      "read OSIS reference while edition is open");
		check(reader.getChapterContent("elim-clone", ref, 2, true).size() == 2 &&
		      reader.getChapter("elim-clone", ref, true).size() == 2 &&
		      !reader.bookNames("elim-clone", 1).empty(),
		      "read chapter and book lists while edition is open");
		auto liveEdit = EditableEdition::open(clonedPath, error);
		check(liveEdit && liveEdit->setName("Biblia derivada", error) &&
		      liveEdit->setVerseText(ref, verses[0].text, error) &&
		      liveEdit->setBookComment(ref.book, "Comentario del libro", error) &&
		      liveEdit->setVerseComment(ref, "Comentario del versículo", error),
		      "save an edition while the reader has recently read it without a database lock");
	}

	EditableEditionMetadata denied = metadata;
	denied.sourceModuleId = "NacarColunga";
	denied.sourceLicense = "Copyrighted";
	check(!EditableEdition::create(deniedPath, denied, books, verses, error),
	      "creation must reject a non-redistributable Bible source");
	check(!g_file_test(deniedPath.c_str(), G_FILE_TEST_EXISTS) &&
	      !g_file_test((deniedPath + ".tmp").c_str(), G_FILE_TEST_EXISTS),
	      "rejected source must leave no project or temporary file");

	error.clear();
	check(EditableEdition::create(path, metadata, books, verses, error),
	      "create complete project from an eligible source");
	if (failures) { g_free(directory); return 1; }
	EditableEditionMetadata replacement = metadata;
	replacement.name = "No debe sobrescribir";
	check(!EditableEdition::create(path, replacement, books, verses, error),
	      "creating a second project cannot overwrite an existing one");
	auto project = EditableEdition::open(path, error);
	check(project != nullptr, "open newly created project");
	if (!project) { g_printerr("open: %s\n", error.c_str()); g_free(directory); return 1; }
	check(project->verseText(ref, text, error) &&
	      text == "En el principio creó Dios los cielos y la tierra.",
	      "project starts with full source verse text");
	{
		SqliteBibleBackend backend(directory);
		const auto content = backend.getVerseContent(metadata.projectId, ref, true);
		check(content.valid && content.plainText == verses[0].text &&
		      content.headings.size() == 1 && content.words.size() == 1 &&
		      content.spans.size() == 1 && content.footnotes.size() == 1 &&
		      content.crossReferences.size() == 1,
		      "created project opens as a complete SQLite Bible with its source apparatus");
		check(backend.moduleDescription(metadata.projectId) == metadata.name,
		      "chosen project name is the displayed Bible name");
	}
	check(project->setName("Biblia Elim de estudio", error),
	      "author can choose and revise project name");
	check(project->setVerseText(ref, "Al principio, Dios creó cielo y tierra.", error),
	      "author can progressively revise an existing verse");
	check(project->setBookComment(1, "Criterios editoriales de Génesis.", error),
	      "author can add book-level comment");
	check(project->setVerseComment(ref, "Revisar esta traducción en equipo.", error),
	      "author can add verse-level comment");
	EditionGlossaryEntry glossary{"hesed", "misericordia leal", "Término de pacto; revisar por contexto."};
	check(project->setGlossaryEntry(glossary, error), "author can record a project glossary decision");
	EditionDecision decision;
	decision.reference = ref; decision.proposal = "Propuesta contextual";
	decision.rationale = "Respeta el uso del pasaje"; decision.evidence = "Léxico hebreo, entrada 1";
	decision.confidence = "media"; decision.status = "pendiente de revisión";
	check(project->setDecision(decision, error), "author can document proposal, evidence, confidence, and review status");
	check(!project->setVerseText({1, 1, 99, 1}, "verso inexistente", error),
	      "editing cannot create a verse outside the source Bible");
	project.reset();

	project = EditableEdition::open(path, error);
	check(project && project->metadata().name == "Biblia Elim de estudio",
	      "project title persists after close/reopen");
	check(project && project->metadata().sourceModuleId == "SpaRV" &&
	      project->metadata().sourceLicense == "Public Domain" &&
	      project->metadata().sourceAttribution == "Reina-Valera 1909; dominio público" &&
	      project->metadata().sourceFingerprint.size() == 64,
	      "project retains source identity, license, attribution, and SHA-256 fingerprint");
	check(project && project->verseText(ref, text, error) &&
	      text == "Al principio, Dios creó cielo y tierra.",
	      "progressive verse edits persist after close/reopen");
	std::vector<EditionRevision> revisions;
	check(project && project->revisions(ref, revisions, error) && revisions.size() == 1 &&
	      revisions[0].oldText == verses[0].text && revisions[0].newText == text,
	      "each verse revision preserves the previous and new wording");
	EditionDecision storedDecision;
	check(project && project->decision(ref, storedDecision, error) &&
	      storedDecision.evidence == "Léxico hebreo, entrada 1" && storedDecision.confidence == "media" &&
	      storedDecision.status == "pendiente de revisión",
	      "editorial decision evidence and review state persist");
	std::vector<EditionGlossaryEntry> glossaryEntries;
	check(project && project->glossaryEntries(glossaryEntries, error) && glossaryEntries.size() == 1 &&
	      glossaryEntries[0].targetTerm == "misericordia leal",
	      "project glossary persists across reopening");
	std::string comment;
	check(project && project->bookComment(1, comment, error) &&
	      comment == "Criterios editoriales de Génesis.",
	      "book comment persists after close/reopen");
	check(project && project->verseComment(ref, comment, error) &&
	      comment == "Revisar esta traducción en equipo.",
	      "verse comment persists after close/reopen");
	project.reset();
	{
		SqliteBibleBackend backend(directory);
		const auto content = backend.getVerseContent(metadata.projectId, ref, true);
		const auto capabilities = backend.moduleCapabilities(metadata.projectId);
		check(content.valid && content.plainText == "Al principio, Dios creó cielo y tierra." &&
		      content.headings.size() == 1 && content.words.empty() &&
		      content.spans.empty() && content.footnotes.empty() &&
		      content.crossReferences.empty(),
		      "edited verse remains readable and stale byte-ranged apparatus is removed");
		check(!capabilities.strongs && !capabilities.footnotes && !capabilities.crossrefs,
		      "module capabilities update when text editing removes source apparatus");
		check(backend.moduleDescription(metadata.projectId) == "Biblia Elim de estudio",
		      "renamed project appears under the new name when reopened");
		BibleSearchQuery query;
		query.mode = BibleSearchMode::Phrase;
		query.text = "Al principio";
		check(backend.search(metadata.projectId, query).size() == 1,
		      "search index follows the incrementally edited verse");
		query.text = "En el principio";
		check(backend.search(metadata.projectId, query).empty(),
		      "search index no longer returns the superseded verse text");
	}

 {
  project=EditableEdition::open(path,error);
  check(bool(project),"reopen project for comparison regressions");
  if(!project)return 1;
  EditableEditionMetadata m;std::vector<EditionComparison> comparisons;
  check(EditableEdition::comparisons(path,m,comparisons,error) && comparisons.size()==2,
        "read-only comparison enumerates every project verse");
  check(comparisons.size()==2 && comparisons[0].edited() && comparisons[0].commented() && comparisons[0].pending() &&
        !comparisons[1].edited() && !comparisons[1].commented() && !comparisons[1].pending(),
        "generic edit, comment and pending filters distinguish each kind of contribution");
  check(comparisons.size()==2 && comparisons[0].baseText==verses[0].text && m.sourceName==metadata.sourceName,
        "comparison retains original text and visible provenance");
  check(project->setVerseText(ref,verses[0].text,error),"restore exact original text");
  check(EditableEdition::comparisons(path,m,comparisons,error,1,1) && !comparisons[0].edited() && comparisons[0].commented(),
        "restoring original removes edited badge while retaining comment badge");
  check(project->revisions(ref,revisions,error) && revisions.size()==2,
        "restoration retains complete revision history");
  check(EditableEdition::comparisons(path,m,comparisons,error,1,99) && comparisons.empty(),
        "chapter filter does not fabricate references");
  check(!EditableEdition::comparisons(sourcePath,m,comparisons,error),
        "ordinary source modules do not acquire editorial markers");
  check(project->setVerseText(ref,"Texto nuevo con á y <etiqueta>.",error),"make another incremental change");
  // Simulate a pre-feature project and reconstruct its base from earliest history.
  sqlite3 *legacy=nullptr;sqlite3_open(path.c_str(),&legacy);
  check(sqlite3_exec(legacy,"DROP TABLE edition_base_verses",nullptr,nullptr,nullptr)==SQLITE_OK,"emulate legacy project without original snapshot");
  sqlite3_close(legacy);
  check(EditableEdition::comparisons(path,m,comparisons,error) && comparisons[0].baseText==verses[0].text && comparisons[0].edited(),
        "read-only legacy comparison uses earliest history, not last revision");
  project.reset();project=EditableEdition::open(path,error);
  check(project && EditableEdition::comparisons(path,m,comparisons,error) && comparisons[0].baseText==verses[0].text,
        "migration snapshots legacy original without changing current text");
  // A separately updated or removed installed source cannot change the snapshot.
  sqlite3 *source=nullptr;sqlite3_open(sourcePath.c_str(),&source);
  sqlite3_exec(source,"UPDATE verses SET text='Updated source module'",nullptr,nullptr,nullptr);sqlite3_close(source);
  check(EditableEdition::comparisons(path,m,comparisons,error) && comparisons[0].baseText==verses[0].text,
        "comparison is independent of updated installed source text");
  const auto diff=editionDifferenceMarkup("En á <b>","En é <b>");
  check(diff.first=="En <b><u>á</u></b> &lt;b&gt;" && diff.second=="En <b><u>é</u></b> &lt;b&gt;",
        "difference highlighting preserves UTF-8 boundaries and escapes markup");
  check(editionDifferenceMarkup("same","same").first=="same" &&
        editionDifferenceMarkup("","á").second=="<b><u>á</u></b>" &&
        editionDifferenceMarkup("á","").first=="<b><u>á</u></b>",
        "difference highlighting handles equality, insertion and deletion");
 }
	check(verses.front().text == "En el principio creó Dios los cielos y la tierra.",
	      "editing the project does not change the source data");

	project.reset();
	g_remove((std::string(directory) + "/elim-study-1.sqlite").c_str());
	g_remove(path.c_str());
	g_remove(sourcePath.c_str());
	g_remove(clonedPath.c_str());
	g_remove(deniedPath.c_str());
	g_rmdir(directory);
	g_free(directory);
	g_print("editable_edition_failures=%d\n", failures);
	return failures ? 1 : 0;
}
