#include "backend/morphology.h"
#include "backend/osis_importer.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/usfm_importer.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#include <iostream>
#include <string>
#include <tuple>

namespace {

std::size_t failures = 0;

void check(bool condition, const std::string &message)
{
	if (condition) return;
	++failures;
	std::cerr << "FAIL: " << message << '\n';
}

UsfmImportOptions options(const std::string &module)
{
	UsfmImportOptions result;
	result.moduleId = module;
	result.name = "Morphology equivalence";
	result.language = "mul";
	result.versification = "custom";
	result.description = "Synthetic equivalent morphology fixture";
	return result;
}

void checkParser()
{
	const MorphologyParseResult parsed = parseMorphology(
		"robinson:N-NSM oshb:He,Ncmsa custom.alpha:opaque/code HR/Ncfsa");
	check(parsed.malformedValues.empty(), "valid opaque values parse");
	check(parsed.tags.size() == 4, "four morphology tags parse");
	if (parsed.tags.size() == 4) {
		check(parsed.tags[0] == MorphologyTag{"robinson", "N-NSM"},
			"qualified Robinson tag");
		check(parsed.tags[1] == MorphologyTag{"oshb", "He,Ncmsa"},
			"comma remains inside opaque code");
		check(parsed.tags[2] == MorphologyTag{"custom.alpha", "opaque/code"},
			"unknown valid scheme is preserved");
		check(parsed.tags[3] == MorphologyTag{"", "HR/Ncfsa"},
			"unqualified MorphHB form is preserved");
	}
	const MorphologyParseResult malformed = parseMorphology("good:V-PAI bad: :orphan");
	check(malformed.tags.size() == 1 &&
		malformed.tags[0] == MorphologyTag{"good", "V-PAI"},
		"valid tag survives malformed neighbors");
	check(malformed.malformedValues.size() == 2 &&
		malformed.malformedValues[0] == "bad:" &&
		malformed.malformedValues[1] == ":orphan",
		"malformed qualified values are audited in order");
	check(parseMorphology("").malformedValues == std::vector<std::string>{"<empty>"},
		"empty morphology is unusable");
}

void checkAudit(const UsfmImportStats &stats, const std::string &source)
{
	check(stats.wordsImported == 3, source + " word count");
	check(stats.wordsWithStrong == 2 && stats.strongIdsImported == 2,
		source + " Strong remains independent");
	check(stats.morphologyBearingTokens == 2, source + " morphology-bearing words");
	check(stats.morphologyTagsParsed == 4, source + " morphology tag count");
	check(stats.multiMorphologyTokens == 2, source + " multi-morph word count");
	check(stats.maxMorphologyTagsPerToken == 2, source + " maximum tags per word");
	check(stats.malformedMorphologyValues == 2, source + " malformed count");
	check(stats.morphSchemes.size() == 3 && stats.morphSchemes.at("robinson") == 1 &&
		stats.morphSchemes.at("custom.alpha") == 1 && stats.morphSchemes.at("oshb") == 2,
		source + " schemes and namespaces");
	check(stats.morphCodes.size() == 4 && stats.morphCodes.at("N-NSM") == 1 &&
		stats.morphCodes.at("opaque/code") == 1 && stats.morphCodes.at("HNcmpa") == 1 &&
		stats.morphCodes.at("HR/Ncfsa") == 1, source + " unique opaque codes");
	check(stats.malformedMorphValues.size() == 2 &&
		stats.malformedMorphValues.at("bad:") == 1 &&
		stats.malformedMorphValues.at(":orphan") == 1,
		source + " malformed values");
}

void checkPersistedRegression(const std::string &directory)
{
	using Row = std::tuple<int, int, std::string, std::string>;
	const std::vector<Row> expectedRows = {
		{ 0, 0, "robinson", "N-NSM" },
		{ 0, 1, "custom.alpha", "opaque/code" },
		{ 1, 0, "oshb", "HNcmpa" },
		{ 1, 1, "oshb", "HR/Ncfsa" }
	};
	SqliteBibleBackend backend(directory);
	for (const std::string &module : {std::string("morph-usfm"), std::string("morph-osis")}) {
		check(backend.hasModule(module), module + " opens");
		check(backend.moduleCapabilities(module).morphology,
			module + " feature.morphology follows persisted rows");
		sqlite3 *database = nullptr;
		std::vector<Row> rows;
		const std::string path = directory + "/" + module + ".sqlite";
		if (sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) ==
		    SQLITE_OK) {
			sqlite3_stmt *statement = nullptr;
			if (sqlite3_prepare_v2(database,
				"SELECT word_sequence,morphology_sequence,scheme,code "
				"FROM verse_word_morphology ORDER BY word_sequence,morphology_sequence",
				-1, &statement, nullptr) == SQLITE_OK) {
				while (sqlite3_step(statement) == SQLITE_ROW) {
					const char *scheme = reinterpret_cast<const char *>(sqlite3_column_text(statement, 2));
					const char *code = reinterpret_cast<const char *>(sqlite3_column_text(statement, 3));
					rows.emplace_back(sqlite3_column_int(statement, 0),
						sqlite3_column_int(statement, 1), scheme ? scheme : "", code ? code : "");
				}
			}
			if (statement) sqlite3_finalize(statement);
			sqlite3_close(database);
		} else if (database) {
			sqlite3_close(database);
		}
		check(rows == expectedRows,
			module + " persists ordered equivalent neutral morphology");
		BibleKeyInfo key;
		check(backend.resolveKey(module, "John 1:1", key), module + " reference resolves");
		const BibleVerseContent verse = backend.getVerseContent(module, key.reference);
		check(verse.valid && verse.plainText == "λόγος אֱלֹהִים intact.",
			module + " visible UTF-8 text is intact: [" + verse.plainText + "]");
		check(verse.words.size() == 3, module + " persisted word count");
		for (const BibleWordInfo &word : verse.words) {
			check(word.start + word.length <= verse.plainText.size() &&
				verse.plainText.substr(word.start, word.length) == word.text,
				module + " UTF-8 byte range");
		}
		if (verse.words.size() == 3) {
			check(verse.words[0].morphologyTags ==
				std::vector<MorphologyTag>({ { "robinson", "N-NSM" },
					{ "custom.alpha", "opaque/code" } }),
				module + " ordered multiple morphology tags round-trip");
			check(verse.words[0].strongs.size() == 1 &&
				verse.words[0].strongs[0] == StrongId{StrongLanguage::Greek, 3056},
				module + " Strong with morphology remains valid");
			check(verse.words[1].strongs.empty(),
				module + " morphology without Strong remains independent");
			check(verse.words[1].morphologyTags ==
				std::vector<MorphologyTag>({ { "oshb", "HNcmpa" },
					{ "oshb", "HR/Ncfsa" } }),
				module + " morphology without Strong round-trips");
			check(verse.words[2].strongs.size() == 1 &&
				verse.words[2].strongs[0] == StrongId{StrongLanguage::Hebrew, 430},
				module + " malformed morphology does not corrupt Strong");
			check(verse.words[2].morphologyTags.empty(),
				module + " malformed morphology remains excluded");
		}
	}
}

} // namespace

int main()
{
	checkParser();
	gchar *temporary = g_dir_make_tmp("morphology-audit-XXXXXX", nullptr);
	check(temporary != nullptr, "temporary directory");
	if (!temporary) return 1;
	const std::string directory = temporary;
	const std::string fixtureRoot = SRCDIR "/tests/fixtures/morphology/";
	const std::string usfmOutput = directory + "/morph-usfm.sqlite";
	const std::string osisOutput = directory + "/morph-osis.sqlite";
	std::string error;
	UsfmImportStats usfmStats;
	check(importUsfm({fixtureRoot + "equivalent.usfm"}, usfmOutput,
		options("morph-usfm"), usfmStats, error), "USFM morphology fixture imports: " + error);
	error.clear();
	UsfmImportStats osisStats;
	check(importOsis(fixtureRoot + "equivalent.xml", osisOutput,
		options("morph-osis"), osisStats, error), "OSIS morphology fixture imports: " + error);
	checkAudit(usfmStats, "USFM");
	checkAudit(osisStats, "OSIS");
	check(usfmStats.morphSchemes == osisStats.morphSchemes &&
		usfmStats.morphCodes == osisStats.morphCodes &&
		usfmStats.malformedMorphValues == osisStats.malformedMorphValues &&
		usfmStats.morphologyWordTags == osisStats.morphologyWordTags &&
		usfmStats.morphologyTagsParsed == osisStats.morphologyTagsParsed &&
		usfmStats.multiMorphologyTokens == osisStats.multiMorphologyTokens,
		"equivalent USFM and OSIS yield equal neutral morphology audit");
	checkPersistedRegression(directory);
	g_remove(usfmOutput.c_str());
	g_remove(osisOutput.c_str());
	g_rmdir(directory.c_str());
	g_free(temporary);
	std::cout << "morphology_audit_failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
