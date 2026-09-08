#include "backend/osis_importer.h"
#include "backend/sqlite/sqlite_bible_backend.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <sqlite3.h>

#include <fstream>
#include <iostream>
#include <map>
#include <string>

namespace {

std::size_t networkLoaderCalls = 0;
std::size_t fileEntityLoaderCalls = 0;
xmlExternalEntityLoader delegatedLoader = nullptr;
std::string controlledFileEntityUrl;

xmlParserInputPtr rejectingNetworkLoader(const char *url, const char *identifier,
	xmlParserCtxtPtr context)
{
	if (url && controlledFileEntityUrl == url) {
		++fileEntityLoaderCalls;
		return nullptr;
	}
	if (url && (std::string(url).compare(0, 7, "http://") == 0 ||
	    std::string(url).compare(0, 8, "https://") == 0)) {
		++networkLoaderCalls;
		return nullptr;
	}
	return delegatedLoader ? delegatedLoader(url, identifier, context) : nullptr;
}

bool writeFile(const std::string &path, const std::string &contents)
{
	return g_file_set_contents(path.c_str(), contents.c_str(),
		static_cast<gssize>(contents.size()), nullptr);
}

UsfmImportOptions options(const std::string &module)
{
	UsfmImportOptions result;
	result.moduleId = module;
	result.name = "OSIS operational fixture";
	result.language = "es";
	result.versification = "custom";
	return result;
}

bool import(const std::string &input, const std::string &output,
	const std::string &module, UsfmImportStats &stats)
{
	std::string error;
	if (importOsis(input, output, options(module), stats, error)) return true;
	std::cerr << "import failed for " << module << ": " << error << '\n';
	return false;
}

std::size_t total(const std::map<std::string, std::size_t> &values)
{
	std::size_t result = 0;
	for (const auto &value : values) result += value.second;
	return result;
}

bool databaseIntegrity(const std::string &path)
{
	sqlite3 *database = nullptr;
	if (sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
		return false;
	sqlite3_stmt *statement = nullptr;
	bool valid = sqlite3_prepare_v2(database, "PRAGMA integrity_check", -1,
		&statement, nullptr) == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW &&
		std::string(reinterpret_cast<const char *>(sqlite3_column_text(statement, 0))) == "ok";
	sqlite3_finalize(statement);
	statement = nullptr;
	valid = valid && sqlite3_prepare_v2(database, "PRAGMA foreign_key_check", -1,
		&statement, nullptr) == SQLITE_OK && sqlite3_step(statement) == SQLITE_DONE;
	sqlite3_finalize(statement);
	sqlite3_close(database);
	return valid;
}

void generateLarge(const std::string &path, std::size_t verses, bool malformed)
{
	std::ofstream output(path);
	output << "<osis><osisText><chapter osisID=\"Gen.1\"><p>\n";
	for (std::size_t verse = 1; verse <= verses; ++verse)
		output << "<verse osisID=\"Gen.1." << verse
			<< "\">Texto operacional " << verse << " variante " << verse % 97
			<< ".</verse>\n";
	if (malformed) output << "<broken>";
	else output << "</p></chapter></osisText></osis>\n";
}

} // namespace

int main(int argc, char **argv)
{
	if (argc == 4) {
		SqliteBibleBackend backend(argv[1]);
		const std::string module = argv[2];
		const std::size_t expected = std::stoul(argv[3]);
		BibleKeyInfo key;
		bool valid = backend.resolveKey(module, "Genesis 1:1", key);
		valid = valid && backend.getChapter(module, key.reference, false).size() == expected;
		BibleReference last = key.reference;
		last.verse = static_cast<int>(expected);
		valid = valid && backend.getVerseContent(module, last).valid;
		BibleSearchQuery query;
		query.mode = BibleSearchMode::Phrase;
		query.text = "variante 42";
		valid = valid && !backend.search(module, query).empty();
		std::cout << "external_large_backend_sanity="
			<< (valid ? "true" : "false") << '\n';
		return valid ? 0 : 1;
	}
	gchar *temporary = g_dir_make_tmp("osis-operational-XXXXXX", nullptr);
	if (!temporary) return 2;
	const std::string directory = temporary;
	std::size_t failures = 0;

	const std::string secretPath = directory + "/secret.txt";
	const std::string secret = "OSIS_XXE_SECRET_SENTINEL";
	controlledFileEntityUrl = "file://" + secretPath;
	writeFile(secretPath, secret);
	const std::string fileEntityInput = directory + "/file-entity.xml";
	writeFile(fileEntityInput,
		"<!DOCTYPE foo [<!ENTITY ext SYSTEM \"" + controlledFileEntityUrl + "\">]>"
		"<osis><osisText><chapter osisID=\"John.1\"><verse osisID=\"John.1.1\">"
		"Antes &ext; después.</verse></chapter></osisText></osis>");
	xmlExternalEntityLoader previousLoader = xmlGetExternalEntityLoader();
	delegatedLoader = previousLoader;
	xmlSetExternalEntityLoader(rejectingNetworkLoader);
	UsfmImportStats fileStats;
	if (!import(fileEntityInput, directory + "/file-entity.sqlite", "file-entity", fileStats))
		++failures;
	xmlSetExternalEntityLoader(previousLoader);

	const std::string networkEntityInput = directory + "/network-entity.xml";
	writeFile(networkEntityInput,
		"<!DOCTYPE foo [<!ENTITY ext SYSTEM \"http://127.0.0.1:9/osis-entity\">]>"
		"<osis><osisText><chapter osisID=\"John.1\"><verse osisID=\"John.1.1\">"
		"Red &ext; bloqueada.</verse></chapter></osisText></osis>");
	delegatedLoader = previousLoader;
	xmlSetExternalEntityLoader(rejectingNetworkLoader);
	UsfmImportStats networkStats;
	if (!import(networkEntityInput, directory + "/network-entity.sqlite",
		"network-entity", networkStats)) ++failures;
	xmlSetExternalEntityLoader(previousLoader);

	const std::string internalEntityInput = directory + "/internal-entity.xml";
	writeFile(internalEntityInput,
		"<!DOCTYPE foo [<!ENTITY internal \"INTERNAL_ENTITY_SENTINEL\">]>"
		"<osis><osisText><chapter osisID=\"John.1\"><verse osisID=\"John.1.1\">"
		"Antes &internal; después.</verse></chapter></osisText></osis>");
	UsfmImportStats internalStats;
	if (!import(internalEntityInput, directory + "/internal-entity.sqlite",
		"internal-entity", internalStats)) ++failures;

	const std::string auditInput = directory + "/audit.xml";
	writeFile(auditInput,
		"<osis><osisText><unknownSection><chapter osisID=\"John.1\">"
		"<title foo=\"x\">Título auditado</title><p><verse osisID=\"John.1.1\" foo=\"x\">"
		"Texto <foo>preservado</foo> aquí "
		"<w lemma=\"lemma:λόγος strong:G25\" morph=\"robinson:N-NSM oshm:He,Ncmsa\" foo=\"x\">Palabra</w>"
		"<note type=\"footnote\" n=\"+\" foo=\"x\">Nota.</note> continúa "
		"<note type=\"crossReference\" foo=\"x\"><reference foo=\"x\" "
		"osisRef=\"John.3.16 John.3.16-John.3.18 Bogus.1.1 Rom.8.28 Gen.1.1-Gen.1.3\">"
		"John 3:16; John 3:16-18; inválida; Romans 8:28; Genesis 1:1-3</reference>"
		"</note> termina.</verse></p></chapter></unknownSection></osisText></osis>");
	UsfmImportStats auditStats;
	if (!import(auditInput, directory + "/audit.sqlite", "audit", auditStats))
		++failures;

	const std::string largeInput = directory + "/large.xml";
	generateLarge(largeInput, 10000, false);
	UsfmImportStats largeStats;
	if (!import(largeInput, directory + "/large.sqlite", "large", largeStats))
		++failures;
	const std::string malformedInput = directory + "/malformed.xml";
	const std::string malformedOutput = directory + "/malformed.sqlite";
	generateLarge(malformedInput, 10000, true);
	UsfmImportStats malformedStats;
	std::string malformedError;
	const bool malformedAccepted = importOsis(malformedInput, malformedOutput,
		options("malformed"), malformedStats, malformedError);
	const bool rollbackClean = !g_file_test(malformedOutput.c_str(), G_FILE_TEST_EXISTS) &&
		!g_file_test((malformedOutput + ".tmp").c_str(), G_FILE_TEST_EXISTS);
	if (malformedAccepted || !rollbackClean) ++failures;

	SqliteBibleBackend backend(directory);
	BibleKeyInfo key;
	bool sentinelInOutput = true;
	if (backend.resolveKey("file-entity", "John 1:1", key))
		sentinelInOutput = backend.getVerseContent("file-entity", key.reference)
			.plainText.find(secret) != std::string::npos;
	else ++failures;
	if (sentinelInOutput || fileEntityLoaderCalls != 0) ++failures;
	bool networkEntityLoaded = networkLoaderCalls != 0;
	if (backend.resolveKey("network-entity", "John 1:1", key))
		networkEntityLoaded = networkEntityLoaded ||
			backend.getVerseContent("network-entity", key.reference).plainText
				.find("OSIS_NETWORK_SENTINEL") != std::string::npos;
	else ++failures;
	if (networkEntityLoaded) ++failures;
	bool internalEntityExpanded = true;
	if (backend.resolveKey("internal-entity", "John 1:1", key))
		internalEntityExpanded = backend.getVerseContent("internal-entity", key.reference)
			.plainText.find("INTERNAL_ENTITY_SENTINEL") != std::string::npos;
	else ++failures;
	if (internalEntityExpanded) ++failures;

	if (auditStats.unknownInlineElements["foo"] != 1 ||
	    auditStats.unknownStructuralElements["unknownSection"] != 1 ||
	    auditStats.ignoredAttributes["verse.foo"] != 1 ||
	    auditStats.ignoredAttributes["w.foo"] != 1 ||
	    auditStats.ignoredAttributes["note.foo"] != 2 ||
	    auditStats.ignoredAttributes["reference.foo"] != 1 ||
	    auditStats.ignoredAttributes["title.foo"] != 1 ||
	    total(auditStats.morphAttributes) != 1 ||
	    auditStats.morphSchemes["robinson"] != 1 ||
	    auditStats.morphSchemes["oshm"] != 1 ||
	    auditStats.nonStrongLemmaAttributes["lemma"] != 1 ||
	    total(auditStats.rangeReferences) != 2 ||
	    auditStats.unresolvedReferences["Bogus.1.1"] != 1)
		++failures;
	if (!backend.resolveKey("audit", "John 1:1", key)) {
		++failures;
	} else {
		const BibleVerseContent content = backend.getVerseContent("audit", key.reference);
		if (content.plainText.find("Texto preservado aquí Palabra") == std::string::npos ||
		    content.words.size() != 1 || content.words[0].strongs.size() != 1 ||
		    content.words[0].strongs[0].number != 25 ||
		    content.crossReferences.size() != 1 ||
		    content.crossReferences[0].displayText !=
			"John 3:16; John 3:16-18; inválida; Romans 8:28; Genesis 1:1-3" ||
		    content.crossReferences[0].references.size() != 2 ||
		    content.crossReferences[0].references[0].book != 43 ||
		    content.crossReferences[0].references[1].book != 45 ||
		    backend.moduleCapabilities("audit").morphology)
			++failures;
	}

	bool backendSanity = backend.resolveKey("large", "Genesis 1:1", key);
	if (backendSanity) {
		backendSanity = backend.getChapter("large", key.reference, false).size() == 10000;
		BibleReference last = key.reference;
		last.verse = 10000;
		backendSanity = backendSanity && backend.getVerseContent("large", last).valid;
		BibleSearchQuery query;
		query.mode = BibleSearchMode::Phrase;
		query.text = "variante 42";
		backendSanity = backendSanity && !backend.search("large", query).empty();
	}
	backendSanity = backendSanity && largeStats.verses == 10000 &&
		databaseIntegrity(directory + "/large.sqlite") &&
		!g_file_test((directory + "/large.sqlite.tmp").c_str(), G_FILE_TEST_EXISTS);
	if (!backendSanity) ++failures;

	std::cout << "sentinel_in_output=" << (sentinelInOutput ? "true" : "false") << '\n'
		<< "file_external_entity_loader_calls=" << fileEntityLoaderCalls << '\n'
		<< "network_external_entity_loaded=" << (networkEntityLoaded ? "true" : "false") << '\n'
		<< "internal_entity_expanded=" << (internalEntityExpanded ? "true" : "false") << '\n'
		<< "unknown_inline_foo=" << auditStats.unknownInlineElements["foo"] << '\n'
		<< "unknown_structural_unknownSection="
		<< auditStats.unknownStructuralElements["unknownSection"] << '\n'
		<< "ignored_attributes=" << total(auditStats.ignoredAttributes) << '\n'
		<< "morph_attributes=" << total(auditStats.morphAttributes) << '\n'
		<< "non_strong_lemmas=" << total(auditStats.nonStrongLemmaAttributes) << '\n'
		<< "ranges_detected=" << total(auditStats.rangeReferences) << '\n'
		<< "unresolved_targets=" << total(auditStats.unresolvedReferences) << '\n'
		<< "near_eof_import_failed=" << (!malformedAccepted ? "true" : "false") << '\n'
		<< "near_eof_rollback_clean=" << (rollbackClean ? "true" : "false") << '\n'
		<< "large_backend_sanity=" << (backendSanity ? "true" : "false") << '\n'
		<< "operational_failures=" << failures << '\n';
	g_free(temporary);
	return failures == 0 ? 0 : 1;
}
