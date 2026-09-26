#include "backend/osis_importer.h"
#include "backend/sqlite/sqlite_bible_backend.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

std::size_t failures = 0;

void check(bool condition, const std::string &message)
{
	if (condition) return;
	++failures;
	std::cerr << "FAIL: " << message << '\n';
}

UsfmImportOptions options(const std::string &module, const std::string &name,
	const std::string &language, const std::string &description,
	const std::string &license, const std::string &source)
{
	UsfmImportOptions result;
	result.moduleId = module;
	result.name = name;
	result.language = language;
	result.versification = "custom";
	result.description = description;
	result.license = license;
	result.source = source;
	result.contentVersion = "regression-extract-1";
	return result;
}

bool importFixture(const std::string &fixture, const std::string &output,
	const UsfmImportOptions &metadata, UsfmImportStats &stats)
{
	std::string error;
	const bool imported = importOsis(std::string(SRCDIR) +
		"/tests/fixtures/osis/" + fixture, output, metadata, stats, error);
	if (!imported)
		std::cerr << "FAIL: import " << fixture << ": " << error << '\n';
	return imported;
}

std::map<std::string, std::string> readMetadata(const std::string &path)
{
	std::map<std::string, std::string> result;
	sqlite3 *database = nullptr;
	if (sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) !=
	    SQLITE_OK) {
		if (database) sqlite3_close(database);
		return result;
	}
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(database, "SELECT key,value FROM metadata", -1,
		&statement, nullptr) == SQLITE_OK) {
		while (sqlite3_step(statement) == SQLITE_ROW) {
			const char *key = reinterpret_cast<const char *>(
				sqlite3_column_text(statement, 0));
			const char *value = reinterpret_cast<const char *>(
				sqlite3_column_text(statement, 1));
			result[key ? key : ""] = value ? value : "";
		}
	}
	if (statement) sqlite3_finalize(statement);
	sqlite3_close(database);
	return result;
}

void checkBaseCapabilities(const BibleModuleCapabilities &capabilities,
	bool strongs, bool morphology, const std::string &module)
{
	check(capabilities.verses, module + " verses capability");
	check(capabilities.search, module + " search capability");
	check(capabilities.strongs == strongs, module + " strong capability");
	check(capabilities.morphology == morphology, module + " morphology capability");
	check(!capabilities.headings, module + " headings capability");
	check(!capabilities.footnotes, module + " footnotes capability");
	check(!capabilities.crossrefs, module + " crossrefs capability");
	check(!capabilities.dictionaryLookup, module + " dictionary capability");
}

void checkMetadata(const std::string &path, const UsfmImportOptions &expected)
{
	const std::map<std::string, std::string> values = readMetadata(path);
	auto equals = [&](const std::string &key, const std::string &value) {
		auto found = values.find(key);
		check(found != values.end() && found->second == value,
			expected.moduleId + " metadata " + key);
	};
	equals("schema_version", "1");
	equals("module_id", expected.moduleId);
	equals("name", expected.name);
	equals("language", expected.language);
	equals("module_type", "bible");
	equals("source_format", "osis");
	equals("description", expected.description);
	equals("license", expected.license);
	equals("source", expected.source);
	equals("content_version", expected.contentVersion);
}

void checkMorphHebrew(SqliteBibleBackend &backend, const UsfmImportStats &stats)
{
	const std::string module = "morphhb-extract";
	check(backend.hasModule(module), "MorphHB module opens through backend");
	check(backend.moduleType(module) == BibleModuleType::Bible,
		"MorphHB backend module type");
	check(backend.moduleLanguage(module) == "he", "MorphHB backend language");
	check(backend.moduleDescription(module) == "Open Scriptures Hebrew Bible extract",
		"MorphHB backend description");
	checkBaseCapabilities(backend.moduleCapabilities(module), false, true, module);

	BibleKeyInfo key;
	check(backend.resolveKey(module, "Genesis 1:1", key),
		"MorphHB Genesis 1:1 resolves");
	const BibleVerseContent verse = backend.getVerseContent(module, key.reference);
	const std::string expected =
		"בְּרֵאשִׁ֖ית בָּרָ֣א אֱלֹהִ֑ים אֵ֥ת הַשָּׁמַ֖יִם וְאֵ֥ת הָאָֽרֶץ׃";
	check(verse.valid && verse.plainText == expected, "MorphHB neutral text");
	const std::vector<std::string> words = {
		"בְּרֵאשִׁ֖ית", "בָּרָ֣א", "אֱלֹהִ֑ים", "אֵ֥ת", "הַשָּׁמַ֖יִם", "וְאֵ֥ת", "הָאָֽרֶץ"
	};
	check(verse.words.size() == words.size(), "MorphHB word count");
	for (std::size_t index = 0; index < verse.words.size(); ++index) {
		const BibleWordInfo &word = verse.words[index];
		check(index < words.size() && word.text == words[index],
			"MorphHB word text " + std::to_string(index));
		check(word.start <= verse.plainText.size() &&
			word.length <= verse.plainText.size() - word.start,
			"MorphHB word byte bounds " + std::to_string(index));
		check(word.start <= verse.plainText.size() &&
			word.length <= verse.plainText.size() - word.start &&
			verse.plainText.substr(word.start, word.length) == word.text,
			"MorphHB word byte substring " + std::to_string(index));
	}
	check(stats.books == 1 && stats.chapters == 1 && stats.verses == 1,
		"MorphHB import counts");
	check(stats.wordsImported == 7 && stats.wordsWithStrong == 0 &&
		stats.strongIdsImported == 0, "MorphHB non-Strong lemma audit");
	check(stats.morphAttributes.count("w.morph") &&
		stats.morphAttributes.at("w.morph") == 7,
		"MorphHB morphology attributes audited");
	check(stats.morphologyBearingTokens == 7 && stats.morphologyTagsParsed == 7 &&
		stats.multiMorphologyTokens == 0 && stats.maxMorphologyTagsPerToken == 1 &&
		stats.malformedMorphologyValues == 0,
		"MorphHB real morphology totals audited");
	check(stats.morphSchemes.size() == 1 &&
		stats.morphSchemes.count("unqualified") &&
		stats.morphSchemes.at("unqualified") == 7,
		"MorphHB unqualified morphology scheme audited");
	check(stats.morphCodes.size() == 7 && stats.morphCodes.count("HR/Ncfsa") &&
		stats.morphCodes.count("HVqp3ms") && stats.morphCodes.count("HNcmpa") &&
		stats.morphCodes.count("HTo") && stats.morphCodes.count("HTd/Ncmpa") &&
		stats.morphCodes.count("HC/To") && stats.morphCodes.count("HTd/Ncbsa"),
		"MorphHB opaque morphology codes audited");
	check(stats.nonStrongLemmaAttributes.count("unqualified") &&
		stats.nonStrongLemmaAttributes.at("unqualified") == 7,
		"MorphHB lemma scheme audited");
}

void checkTorresAmat(SqliteBibleBackend &backend)
{
	const std::string module = "torres-amat-extract";
	check(backend.hasModule(module), "Torres Amat module opens through backend");
	check(backend.moduleType(module) == BibleModuleType::Bible,
		"Torres Amat backend module type");
	check(backend.moduleLanguage(module) == "es", "Torres Amat backend language");
	check(backend.moduleDescription(module) == "Public-domain 1882 fixture extract",
		"Torres Amat backend description");
	checkBaseCapabilities(backend.moduleCapabilities(module), false, false, module);

	BibleKeyInfo key;
	check(backend.resolveKey(module, "Psalms 1:1", key),
		"Torres Amat Psalm 1:1 resolves");
	const BibleVerseContent verse = backend.getVerseContent(module, key.reference);
	check(verse.valid && verse.plainText ==
		"Dichoso aquel varon que no se deja llevar de los consejos de los malos, "
		"ni se detiene en el camino de los pecadores, ni se asienta en la cátedra "
		"pestilencial de los libertinos", "Torres Amat neutral text");
	check(verse.words.empty() && verse.footnotes.empty() &&
		verse.crossReferences.empty() && verse.spans.empty(),
		"Torres Amat neutral annotations");
	const std::vector<BibleVerse> chapter = backend.getChapter(module,
		key.reference, false);
	check(chapter.size() == 2 && chapter[0].osisRef == "Ps.1.1" &&
		chapter[1].osisRef == "Ps.1.2", "Torres Amat chapter through backend");
}

} // namespace

int main()
{
	gchar *temporary = g_dir_make_tmp("osis-test-XXXXXX", nullptr);
	if (!temporary) {
		std::cerr << "FAIL: cannot create temporary directory\n";
		return 1;
	}
	const std::string directory = temporary;
	const std::string basicOutput = directory + "/basic.sqlite";
	const std::string milestoneOutput = directory + "/milestone.sqlite";
	const std::string morphOutput = directory + "/morphhb.sqlite";
	const std::string torresOutput = directory + "/torres.sqlite";
	const std::string unsupportedOutput = directory + "/unsupported.sqlite";

	UsfmImportStats basicStats;
	check(importFixture("basic.xml", basicOutput,
		options("osis", "OSIS", "en", "Basic regression", "fixture", "local"),
		basicStats), "basic fixture imports");
	UsfmImportStats milestoneStats;
	check(importFixture("milestone.xml", milestoneOutput,
		options("milestone", "Milestone", "en", "Milestone regression", "fixture", "local"),
		milestoneStats), "milestone fixture imports");

	const UsfmImportOptions morphOptions = options("morphhb-extract",
		"Open Scriptures Hebrew Bible", "he", "Open Scriptures Hebrew Bible extract",
		"WLC public domain; annotations CC BY 4.0",
		"https://github.com/openscriptures/morphhb/blob/master/wlc/Gen.xml");
	UsfmImportStats morphStats;
	check(importFixture("real-world/morphhb-genesis-1-1.xml", morphOutput,
		morphOptions, morphStats), "MorphHB fixture imports");

	const UsfmImportOptions torresOptions = options("torres-amat-extract",
		"Torres Amat 1882", "es", "Public-domain 1882 fixture extract",
		"Public Domain",
		"https://archive.org/details/la-sagrada-biblia-vulgata-tomo-iiv_202111");
	UsfmImportStats torresStats;
	check(importFixture("real-world/torres-amat-1882-psalm-1.xml", torresOutput,
		torresOptions, torresStats), "Torres Amat fixture imports");

	/* A book outside the supported set (the Vulgate appendix's 3 Esdras)
	 * is still rejected, never mapped into another book. */
	UsfmImportStats unsupportedStats;
	std::string unsupportedError;
	const bool unsupportedAccepted = importOsis(std::string(SRCDIR) +
		"/tests/fixtures/osis/real-world/unsupported-esdras-structure.xml",
		unsupportedOutput, torresOptions, unsupportedStats, unsupportedError);
	check(!unsupportedAccepted &&
		unsupportedError == "invalid OSIS verse osisID: 1Esd.1.1",
		"unsupported 1 Esdras is rejected explicitly");
	check(!g_file_test(unsupportedOutput.c_str(), G_FILE_TEST_EXISTS) &&
		!g_file_test((unsupportedOutput + ".tmp").c_str(), G_FILE_TEST_EXISTS),
		"unsupported 1 Esdras leaves no output artifacts");

	/* Deuterocanonical books (Catholic Bibles), in the module's own
	 * (Vulgate) order, named in Spanish. */
	const std::string deuteroDirectory = directory + "/deutero";
	g_mkdir(deuteroDirectory.c_str(), 0700);
	UsfmImportStats deuteroStats;
	check(importFixture("deuterocanon-vulgate-order.xml",
		deuteroDirectory + "/vulgata.sqlite",
		options("vulgata", "Vulgata", "es", "Deuterocanon order", "fixture", "local"),
		deuteroStats), "deuterocanonical fixture imports");
	check(deuteroStats.books == 6 && deuteroStats.verses == 8,
		"deuterocanonical fixture counts");
	{
		SqliteBibleBackend backend(deuteroDirectory);
		const std::string m = "vulgata";
		const std::vector<std::string> ot = backend.bookNames(m, 1);
		const std::vector<std::string> nt = backend.bookNames(m, 2);
		check(ot == std::vector<std::string>({"Nehemías", "Tobías", "Judit", "Ester"}),
			"Old Testament in the module's order, in Spanish");
		check(nt == std::vector<std::string>({"Mateo", "Lucas"}),
			"New Testament in Spanish");
		BibleKeyInfo tob;
		check(backend.resolveKey(m, "Tobías 1:1", tob) && tob.key == "Tobías 1:1" &&
			tob.osisBook == "Tob" && tob.reference.book == 67 &&
			tob.bookIndex == 2 && tob.chapterCount == 2,
			"Tobit resolves as itself, second in the list");
		BibleKeyInfo alias;
		check(backend.resolveKey(m, "Tobit 2:1", alias) && alias.key == "Tobías 2:1",
			"English name resolves in a Spanish module");
		check(backend.resolveKey(m, "Tob 1:1", alias) && alias.key == "Tobías 1:1",
			"OSIS id resolves");
		check(backend.resolveKey(m, "lucas 23:33", alias) &&
			alias.key == "Lucas 23:33" && alias.bookIndex == 6,
			"a saved Spanish reference resolves, any case");
		check(backend.resolveKey(m, "Luke 23:33", alias) && alias.key == "Lucas 23:33",
			"English reference resolves");
		check(!backend.resolveKey(m, "Génesis 1:1", alias),
			"a book the module lacks does not resolve");
		check(backend.navigate(m, "Nehemías 1:2", 1) == "Tobías 1:1",
			"next verse follows the module's order into Tobit");
		check(backend.navigate(m, "Tobías 1:1", -1) == "Nehemías 1:2",
			"previous verse goes back to Nehemiah");
		check(backend.navigate(m, "Ester 1:1", 1) == "Mateo 1:1",
			"next verse crosses into the New Testament");
		check(backend.setBook(m, "Tobías 1:1", 1, 3) == "Judit 1:1",
			"setBook: third Old Testament book");
		check(backend.setBook(m, "Tobías 1:1", 2, 1) == "Mateo 1:1",
			"setBook: first New Testament book, not book id 1");
		check(backend.setBook(m, "Tobías 1:1", 2, 3).empty() &&
			backend.setBook(m, "Tobías 1:1", 1, 0).empty(),
			"setBook out of range");
	}
	/* What SWORD's mod2osis writes (SQLITE-REAL-101): pre-verse titles
	 * and text, section and acrostic titles inside a verse, "Strong:"
	 * lemmas, a space at the edge of <w>. */
	{
		const std::string exportDirectory = directory + "/export";
		g_mkdir(exportDirectory.c_str(), 0700);
		UsfmImportStats exportStats;
		check(importFixture("mod2osis-structure.xml",
			exportDirectory + "/export.sqlite",
			options("export", "Export", "es", "mod2osis structures", "fixture", "local"),
			exportStats), "mod2osis structure fixture imports");
		SqliteBibleBackend backend(exportDirectory);
		auto content = [&](const char *key) {
			BibleKeyInfo info;
			if (!backend.resolveKey("export", key, info)) return BibleVerseContent();
			return backend.getVerseContent("export", info.reference);
		};
		auto headings = [](const BibleVerseContent &c) {
			std::string out;
			for (const BibleHeading &h : c.headings) out += (out.empty() ? "" : "|") + h.text;
			return out;
		};
		const BibleVerseContent gen1 = content("Génesis 1:1");
		check(gen1.plainText == "Al principio creó Dios.",
			"pre-verse titles, note tail and paragraph are not verse text: " + gen1.plainText);
		check(headings(gen1) == "I. PRIMERA PARTE|Sección uno.",
			"pre-verse titles are headings: " + headings(gen1));
		check(gen1.footnotes.size() == 1, "pre-verse note kept");
		check(gen1.words.size() == 2 && gen1.words[0].strongs.size() == 1 &&
			gen1.words[1].strongs.size() == 1,
			"\"Strong:\" and \"strong:\" lemmas both import");
		const BibleVerseContent gen2 = content("Génesis 1:2");
		check(gen2.plainText == "Texto antes. Texto después." &&
			headings(gen2) == "Título en medio",
			"a section title inside a verse is a heading: " + gen2.plainText);
		const BibleVerseContent gen3 = content("Génesis 1:3");
		check(gen3.plainText == "y se humillarán delante de él.",
			"a space at the edge of <w> separates words: " + gen3.plainText);
		for (const BibleVerseContent *c : {&gen1, &gen3})
			for (const BibleWordInfo &w : c->words)
				check(w.start + w.length <= c->plainText.size() &&
					c->plainText.substr(w.start, w.length) == w.text,
					"word offsets exact: " + w.text);
		const BibleVerseContent ps = content("Salmos 3:1");
		check(ps.plainText == "Salmo de David. ¡Oh Señor!" && ps.headings.empty(),
			"an untyped psalm title inside the verse stays text: " + ps.plainText);
		const BibleVerseContent lam = content("Lamentaciones 1:1");
		check(lam.plainText == "¡Cómo ha quedado solitaria!" && headings(lam) == "ALEF.",
			"an acrostic letter is a heading: " + lam.plainText);
	}
	{
		/* An English module still answers to Spanish names. */
		SqliteBibleBackend backend(directory);
		BibleKeyInfo john;
		check(backend.resolveKey("osis", "Juan 3:16", john) &&
			john.key == "John 3:16", "Spanish name resolves in an English module");
	}

	{
		SqliteBibleBackend backend(directory);
		BibleKeyInfo basicKey;
		check(backend.hasModule("osis") &&
			backend.resolveKey("osis", "John 3:16", basicKey),
			"basic importer regression through backend");
		if (backend.hasModule("osis") && basicKey.reference.book != 0) {
			const BibleVerseContent content = backend.getVerseContent(
				"osis", basicKey.reference);
			check(content.valid && !content.plainText.empty() &&
				content.footnotes.size() == 1, "basic enriched content regression");
		}
		check(backend.hasModule("milestone"), "milestone importer regression");
		checkMorphHebrew(backend, morphStats);
		checkTorresAmat(backend);
	}

	checkMetadata(morphOutput, morphOptions);
	checkMetadata(torresOutput, torresOptions);

	for (const std::string &path : { basicOutput, milestoneOutput, morphOutput,
		torresOutput, unsupportedOutput, unsupportedOutput + ".tmp" })
		g_remove(path.c_str());
	g_rmdir(directory.c_str());
	g_free(temporary);

	std::cout << "real_world_fixture_failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
