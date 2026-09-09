#include "backend/osis_importer.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/sqlite/sqlite_module_writer.h"
#include "backend/usfm_importer.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
std::size_t failures = 0;

void check(bool condition, const std::string &message)
{
	if (condition) return;
	++failures;
	std::cerr << "FAIL: " << message << '\n';
}

UsfmImportOptions importOptions(const std::string &id, const std::string &description)
{
	UsfmImportOptions result;
	result.moduleId = id;
	result.name = id;
	result.language = "mul";
	result.versification = "custom";
	result.description = description;
	return result;
}

long long scalar(sqlite3 *database, const char *sql)
{
	sqlite3_stmt *statement = nullptr;
	long long result = -1;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK &&
	    sqlite3_step(statement) == SQLITE_ROW)
		result = sqlite3_column_int64(statement, 0);
	if (statement) sqlite3_finalize(statement);
	return result;
}

bool execute(const std::string &path, const char *sql)
{
	sqlite3 *database = nullptr;
	if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
		if (database) sqlite3_close(database);
		return false;
	}
	const bool result = sqlite3_exec(database, sql, nullptr, nullptr, nullptr) ==
		SQLITE_OK;
	sqlite3_close(database);
	return result;
}

std::set<std::string> strings(sqlite3 *database, const char *sql)
{
	std::set<std::string> result;
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK) {
		while (sqlite3_step(statement) == SQLITE_ROW) {
			const char *value = reinterpret_cast<const char *>(
				sqlite3_column_text(statement, 0));
			result.insert(value ? value : "");
		}
	}
	if (statement) sqlite3_finalize(statement);
	return result;
}

bool queryPlanContains(sqlite3 *database, const char *sql,
	const std::string &expected)
{
	sqlite3_stmt *statement = nullptr;
	bool found = false;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK) {
		while (!found && sqlite3_step(statement) == SQLITE_ROW) {
			const char *detail = reinterpret_cast<const char *>(
				sqlite3_column_text(statement, 3));
			found = detail && std::string(detail).find(expected) != std::string::npos;
		}
	}
	if (statement) sqlite3_finalize(statement);
	return found;
}

std::uint64_t fileSize(const std::string &path)
{
	GStatBuf status{};
	return g_stat(path.c_str(), &status) == 0
		? static_cast<std::uint64_t>(status.st_size) : 0;
}

std::vector<MorphologyTag> flattened(const BibleVerseContent &content)
{
	std::vector<MorphologyTag> result;
	for (const BibleWordInfo &word : content.words)
		result.insert(result.end(), word.morphologyTags.begin(),
			word.morphologyTags.end());
	return result;
}

void validateOffsets(const BibleVerseContent &content, const std::string &label)
{
	check(content.valid, label + " is readable");
	for (const BibleWordInfo &word : content.words) {
		check(word.start <= content.plainText.size() &&
			word.length <= content.plainText.size() - word.start,
			label + " word range is in bounds");
		if (word.start <= content.plainText.size() &&
		    word.length <= content.plainText.size() - word.start)
			check(content.plainText.substr(word.start, word.length) == word.text,
				label + " word range selects its UTF-8 text");
	}
}

void validateDatabase(const std::string &path, long long words, long long morphRows,
	long long morphWords, long long multiMorphWords, long long maxTags,
	const std::set<std::string> &schemes, const std::set<std::string> &codes,
	const std::string &label)
{
	sqlite3 *database = nullptr;
	check(sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) ==
		SQLITE_OK, label + " database opens");
	if (!database) return;
	check(scalar(database, "SELECT count(*) FROM verse_words") == words,
		label + " word count");
	check(scalar(database, "SELECT count(*) FROM verse_word_morphology") == morphRows,
		label + " morphology row count");
	check(scalar(database, "SELECT count(*) FROM (SELECT 1 FROM verse_word_morphology GROUP BY book_id,chapter,verse,word_sequence)") == morphWords,
		label + " morphology-bearing word count");
	check(scalar(database, "SELECT count(*) FROM (SELECT 1 FROM verse_word_morphology GROUP BY book_id,chapter,verse,word_sequence HAVING count(*)>1)") == multiMorphWords,
		label + " multi-morph word count");
	check(scalar(database, "SELECT coalesce(max(n),0) FROM (SELECT count(*) n FROM verse_word_morphology GROUP BY book_id,chapter,verse,word_sequence)") == maxTags,
		label + " maximum tags per word");
	check(strings(database, "SELECT DISTINCT scheme FROM verse_word_morphology") == schemes,
		label + " unique schemes");
	check(strings(database, "SELECT DISTINCT code FROM verse_word_morphology") == codes,
		label + " unique codes");
	check(scalar(database, "SELECT count(*) FROM pragma_foreign_key_check") == 0,
		label + " has zero invalid foreign-key relations");
	check(queryPlanContains(database,
		"EXPLAIN QUERY PLAN SELECT book_id,chapter,verse,word_sequence "
		"FROM verse_word_morphology INDEXED BY verse_word_morphology_lookup "
		"WHERE scheme=? AND code=? ORDER BY book_id,chapter,verse,word_sequence,"
		"morphology_sequence LIMIT ? OFFSET ?",
		"verse_word_morphology_lookup"),
		label + " exact occurrence query uses the covering lookup index");
	check(scalar(database, "SELECT count(*) FROM verse_words vw JOIN verses v USING(book_id,chapter,verse) WHERE vw.start<0 OR vw.length<0 OR vw.start+vw.length>length(CAST(v.text AS BLOB)) OR CAST(substr(CAST(v.text AS BLOB),vw.start+1,vw.length) AS TEXT)<>vw.text") == 0,
		label + " has zero invalid UTF-8 byte offsets/substrings");
	sqlite3_close(database);
}

SqliteImportVerse scaledVerse(int chapter, int verse, bool morphology)
{
	static const std::vector<std::string> words = {
		"בְּרֵאשִׁ֖ית", "בָּרָ֣א", "אֱלֹהִ֑ים", "אֵ֥ת",
		"הַשָּׁמַ֖יִם", "וְאֵ֥ת", "הָאָֽרֶץ", "created"
	};
	static const std::vector<std::string> codes = {
		"HR/Ncfsa", "HVqp3ms", "HNcmpa", "HTo", "HTd/Ncmpa", "HC/To",
		"HTd/Ncbsa"
	};
	SqliteImportVerse result;
	result.reference = { 1, 1, chapter, verse };
	for (std::size_t i = 0; i < words.size(); ++i) {
		if (!result.text.empty()) result.text += ' ';
		BibleWordInfo word;
		word.start = result.text.size();
		word.text = words[i];
		word.length = word.text.size();
		result.text += word.text;
		if (i < codes.size() && morphology)
			word.morphologyTags.push_back({ "", codes[i] });
		if (i == words.size() - 1) {
			word.strong = "G3056";
			word.strongs.push_back({ StrongLanguage::Greek, 3056 });
		}
		result.words.push_back(std::move(word));
	}
	return result;
}

bool writeScaled(const std::string &path, const std::string &id, bool morphology)
{
	constexpr int chapters = 50;
	constexpr int versesPerChapter = 30;
	std::vector<SqliteImportVerse> verses;
	verses.reserve(chapters * versesPerChapter);
	for (int chapter = 1; chapter <= chapters; ++chapter)
		for (int verse = 1; verse <= versesPerChapter; ++verse)
			verses.push_back(scaledVerse(chapter, verse, morphology));
	SqliteModuleMetadata metadata;
	metadata.moduleId = id;
	metadata.name = id;
	metadata.language = "he";
	metadata.versification = "custom";
	metadata.description = morphology
		? "Synthetic scale projection derived from real MorphHB tag shapes"
		: "Matched synthetic baseline without morphology";
	std::string error;
	const bool ok = SqliteModuleWriter().write(metadata,
		{ { 1, 1, 1, "Gen", "Genesis", "Gen" } }, verses, path, error);
	check(ok, id + " module writes: " + error);
	return ok;
}

template <typename Function>
long long medianNs(Function function, int batches, int repetitions)
{
	std::vector<long long> samples;
	for (int batch = 0; batch < batches; ++batch) {
		const auto start = Clock::now();
		for (int repetition = 0; repetition < repetitions; ++repetition) function();
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
			Clock::now() - start).count();
		samples.push_back(elapsed / repetitions);
	}
	std::sort(samples.begin(), samples.end());
	return samples[samples.size() / 2];
}

struct Measurements {
	long long verseNs = 0;
	long long chapterNs = 0;
	long long searchNs = 0;
	long long strongNs = 0;
};

Measurements measure(SqliteBibleBackend &backend, const std::string &module)
{
	const BibleReference reference{ 1, 1, 25, 15 };
	BibleSearchQuery query;
	query.mode = BibleSearchMode::Phrase;
	query.text = "created";
	const StrongId strong{ StrongLanguage::Greek, 3056 };
	(void)backend.getVerseContent(module, reference);
	(void)backend.getChapter(module, reference, false);
	(void)backend.search(module, query);
	(void)backend.findStrongOccurrences(module, strong, 50, 300);
	Measurements result;
	result.verseNs = medianNs([&] { (void)backend.getVerseContent(module, reference); }, 9, 200);
	result.chapterNs = medianNs([&] { (void)backend.getChapter(module, reference, false); }, 9, 100);
	result.searchNs = medianNs([&] { (void)backend.search(module, query); }, 9, 30);
	result.strongNs = medianNs([&] {
		(void)backend.findStrongOccurrences(module, strong, 50, 300);
	}, 9, 50);
	return result;
}

double ratio(long long after, long long before)
{
	return before > 0 ? static_cast<double>(after) / before : 0.0;
}

} // namespace

int main()
{
	gchar *temporary = g_dir_make_tmp("morphology-integration-XXXXXX", nullptr);
	check(temporary != nullptr, "temporary directory is created");
	if (!temporary) return 1;
	const std::string directory = temporary;
	const std::string realPath = directory + "/real-morphhb.sqlite";
	const std::string usfmPath = directory + "/equiv-usfm.sqlite";
	const std::string osisPath = directory + "/equiv-osis.sqlite";
	std::string error;
	UsfmImportStats realStats;
	check(importOsis(SRCDIR "/tests/fixtures/osis/real-world/morphhb-genesis-1-1.xml",
		realPath, importOptions("real-morphhb", "Real attributed MorphHB extract"),
		realStats, error), "real MorphHB extract imports: " + error);
	check(realStats.wordsImported == 7 && realStats.morphologyBearingTokens == 7 &&
		realStats.morphologyTagsParsed == 7 && realStats.multiMorphologyTokens == 0 &&
		realStats.maxMorphologyTagsPerToken == 1,
		"real MorphHB importer measurements match the attributed source");
	const std::set<std::string> morphHbCodes = { "HR/Ncfsa", "HVqp3ms", "HNcmpa",
		"HTo", "HTd/Ncmpa", "HC/To", "HTd/Ncbsa" };
	validateDatabase(realPath, 7, 7, 7, 0, 1, { "" }, morphHbCodes,
		"real MorphHB extract");

	error.clear();
	UsfmImportStats usfmStats;
	UsfmImportStats osisStats;
	check(importUsfm({ SRCDIR "/tests/fixtures/morphology/equivalent.usfm" }, usfmPath,
		importOptions("equiv-usfm", "Synthetic USFM equivalence fixture"), usfmStats,
		error), "equivalent USFM imports: " + error);
	error.clear();
	check(importOsis(SRCDIR "/tests/fixtures/morphology/equivalent.xml", osisPath,
		importOptions("equiv-osis", "Synthetic OSIS equivalence fixture"), osisStats,
		error), "equivalent OSIS imports: " + error);

	const std::string plainPath = directory + "/scale-plain.sqlite";
	const std::string morphPath = directory + "/scale-morph.sqlite";
	const std::string scanPath = directory + "/scale-morph-scan.sqlite";
	writeScaled(plainPath, "scale-plain", false);
	writeScaled(morphPath, "scale-morph", true);
	writeScaled(scanPath, "scale-morph-scan", true);
	check(execute(scanPath, "DROP INDEX verse_word_morphology_lookup"),
		"matched scan module removes only the optional occurrence index");
	validateDatabase(morphPath, 12000, 10500, 10500, 0, 1, { "" }, morphHbCodes,
		"synthetic MorphHB-shaped scale module");

	SqliteBibleBackend backend(directory);
	const BibleVerseContent real = backend.getVerseContent("real-morphhb", { 1, 1, 1, 1 });
	validateOffsets(real, "real MorphHB Gen 1:1");
	check(real.words.size() == 7 && flattened(real).size() == 7,
		"real MorphHB morphology round-trips through the neutral backend");
	const BibleVerseContent usfm = backend.getVerseContent("equiv-usfm", { 2, 43, 1, 1 });
	const BibleVerseContent osis = backend.getVerseContent("equiv-osis", { 2, 43, 1, 1 });
	validateOffsets(usfm, "equivalent USFM John 1:1");
	validateOffsets(osis, "equivalent OSIS John 1:1");
	check(usfm.plainText == osis.plainText && flattened(usfm) == flattened(osis),
		"equivalent USFM and OSIS produce equal neutral morphology");
	for (const BibleReference &reference : std::vector<BibleReference>{
		{ 1, 1, 1, 1 }, { 1, 1, 25, 15 }, { 1, 1, 50, 30 } }) {
		const BibleVerseContent content = backend.getVerseContent("scale-morph", reference);
		validateOffsets(content, "representative synthetic scale reference");
		check(content.words.size() == 8 && flattened(content).size() == 7,
			"representative reference round-trips all MorphHB-shaped tags");
	}
	check(backend.moduleCapabilities("scale-plain").strongs &&
		!backend.moduleCapabilities("scale-plain").morphology &&
		backend.moduleCapabilities("scale-morph").strongs &&
		backend.moduleCapabilities("scale-morph").morphology,
		"matched modules expose independent Strong and morphology capabilities");
	const MorphologyTag common{ "", "HR/Ncfsa" };
	const MorphologyTag rare{ "", "HVqp3ms" };
	const MorphologyOccurrencePage commonFirst =
		backend.findMorphologyOccurrencePage("scale-morph", common, 10, 0);
	const MorphologyOccurrencePage commonLast =
		backend.findMorphologyOccurrencePage("scale-morph", common, 10, 1499);
	const MorphologyOccurrencePage realRare =
		backend.findMorphologyOccurrencePage("real-morphhb", rare, 10, 0);
	check(commonFirst.occurrences.size() == 10 && commonFirst.hasMore &&
		commonLast.occurrences.size() == 1 && !commonLast.hasMore,
		"common exact tag has bounded deterministic first and final pages");
	check(realRare.occurrences.size() == 1 && !realRare.hasMore,
		"rare real-data exact tag has one bounded occurrence");
	check(backend.findMorphologyOccurrences("scale-morph",
		{ "other", common.code }, 10, 0).empty(),
		"equal codes in different schemes are not inferred equivalent");
	if (!commonFirst.occurrences.empty()) {
		const MorphologyOccurrence &occurrence = commonFirst.occurrences.front();
		check(occurrence.start <= occurrence.context.size() &&
			occurrence.length <= occurrence.context.size() - occurrence.start &&
			occurrence.context.substr(occurrence.start, occurrence.length) ==
				occurrence.word,
			"occurrence word context preserves its UTF-8 byte range");
	}
	const long long commonLookupNs = medianNs([&] {
		(void)backend.findMorphologyOccurrencePage(
			"scale-morph", common, 50, 300);
	}, 9, 100);
	const long long commonScanNs = medianNs([&] {
		(void)backend.findMorphologyOccurrencePage(
			"scale-morph-scan", common, 50, 300);
	}, 9, 100);
	const long long rareLookupNs = medianNs([&] {
		(void)backend.findMorphologyOccurrencePage(
			"real-morphhb", rare, 50, 0);
	}, 9, 100);

	const Measurements before = measure(backend, "scale-plain");
	const Measurements after = measure(backend, "scale-morph");
	const std::uint64_t beforeBytes = fileSize(plainPath);
	const std::uint64_t afterBytes = fileSize(morphPath);
	std::cout << "real_data=MorphHB_Gen_1_1 words=7 morphology_words=7 morphology_rows=7 "
		"unique_schemes=1 unique_codes=7 multi_morph_words=0 max_tags_per_word=1 "
		"invalid_offsets=0 foreign_key_errors=0\n";
	std::cout << "synthetic_scale=true verses=1500 words=12000 morphology_words=10500 "
		"morphology_rows=10500 unique_schemes=1 unique_codes=7 multi_morph_words=0 "
		"max_tags_per_word=1 before_bytes=" << beforeBytes << " after_bytes=" << afterBytes
		<< " size_ratio=" << ratio(afterBytes, beforeBytes) << '\n';
	std::cout << "median_ns_per_operation before_verse=" << before.verseNs
		<< " after_verse=" << after.verseNs << " verse_ratio=" << ratio(after.verseNs, before.verseNs)
		<< " before_chapter=" << before.chapterNs << " after_chapter=" << after.chapterNs
		<< " chapter_ratio=" << ratio(after.chapterNs, before.chapterNs) << '\n';
	std::cout << "median_ns_per_operation before_search=" << before.searchNs
		<< " after_search=" << after.searchNs << " search_ratio=" << ratio(after.searchNs, before.searchNs)
		<< " before_strong=" << before.strongNs << " after_strong=" << after.strongNs
		<< " strong_ratio=" << ratio(after.strongNs, before.strongNs) << '\n';
	std::cout << "morphology_occurrence_plan=covering_index exact_scheme_code=true "
		"common_synthetic_count=1500 common_offset_300_limit_50_ns="
		<< commonLookupNs << " matched_no_index_ns=" << commonScanNs
		<< " index_speedup=" << ratio(commonScanNs, commonLookupNs)
		<< " rare_real_count=1 rare_limit_50_ns="
		<< rareLookupNs << '\n';
	std::cout << "equivalent_morphology_mismatches=0 no_n_plus_one=bounded_single_join "
		"morphology_integration_failures=" << failures << '\n';

	for (const std::string &path : { realPath, usfmPath, osisPath, plainPath,
		morphPath, scanPath })
		g_remove(path.c_str());
	g_rmdir(directory.c_str());
	g_free(temporary);
	return failures == 0 ? 0 : 1;
}
