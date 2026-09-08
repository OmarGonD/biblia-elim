#include "backend/osis_importer.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/usfm_importer.h"
#include "neutral_module_comparator.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <iostream>
#include <string>
#include <vector>

namespace {

bool writeFile(const std::string &path, const std::string &contents)
{
	GError *error = nullptr;
	const bool result = g_file_set_contents(path.c_str(), contents.c_str(),
		static_cast<gssize>(contents.size()), &error);
	if (!result) {
		std::cerr << "cannot write " << path << ": "
			<< (error ? error->message : "unknown error") << '\n';
		if (error) g_error_free(error);
	}
	return result;
}

UsfmImportOptions options(const std::string &module)
{
	UsfmImportOptions result;
	result.moduleId = module;
	result.name = "Semantic equivalence fixture";
	result.language = "es";
	result.versification = "custom";
	result.description = "Neutral semantic fixture";
	return result;
}

bool importOsisFixture(const std::string &input, const std::string &output,
	const std::string &module)
{
	UsfmImportStats stats;
	std::string error;
	if (importOsis(input, output, options(module), stats, error)) return true;
	std::cerr << "OSIS import failed: " << input << ": " << error << '\n';
	return false;
}

std::string mixedDocument(const std::string &body, bool pretty)
{
	if (!pretty)
		return "<osis><osisText><chapter osisID=\"John.3\"><verse osisID=\"John.3.16\">" +
			body + "</verse></chapter></osisText></osis>";
	return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<osis>\n  <osisText>\n    <chapter osisID=\"John.3\">\n"
		"      <verse osisID=\"John.3.16\">\n        " + body +
		"\n      </verse>\n    </chapter>\n  </osisText>\n</osis>\n";
}

struct MixedCase {
	const char *compact;
	const char *pretty;
	const char *expected;
};

std::size_t fixtureSemanticFailures(BibleBackend &backend,
	const std::string &module)
{
	std::size_t failures = 0;
	auto fail = [&](const std::string &reference, const std::string &field,
		const std::string &expected, const std::string &actual) {
		++failures;
		std::cerr << "reference=" << reference << "\nfield=" << field
			<< "\nexpected=" << expected << "\nactual=" << actual << '\n';
	};
	const BibleModuleCapabilities capabilities = backend.moduleCapabilities(module);
	const struct { const char *name; bool value; } expectedCapabilities[] = {
		{ "verses", capabilities.verses }, { "search", capabilities.search },
		{ "strong", capabilities.strongs }, { "headings", capabilities.headings },
		{ "footnotes", capabilities.footnotes }, { "crossrefs", capabilities.crossrefs },
	};
	for (const auto &capability : expectedCapabilities)
		if (!capability.value) fail("module", std::string("capabilities.") + capability.name,
			"true", "false");

	auto content = [&](const std::string &key) {
		BibleKeyInfo resolved;
		if (!backend.resolveKey(module, key, resolved)) {
			fail(key, "reference", "present", "missing");
			return BibleVerseContent();
		}
		return backend.getVerseContent(module, resolved.reference);
	};
	const BibleVerseContent gen11 = content("Genesis 1:1");
	const BibleVerseContent gen12 = content("Genesis 1:2");
	const BibleVerseContent gen21 = content("Genesis 2:1");
	const BibleVerseContent john316 = content("John 3:16");
	const BibleVerseContent john317 = content("John 3:17");
	const BibleVerseContent john41 = content("John 4:1");
	if (!gen11.paragraphBreak) fail("Genesis 1:1", "paragraphBreak", "true", "false");
	if (gen12.paragraphBreak) fail("Genesis 1:2", "paragraphBreak", "false", "true");
	if (gen11.headings.size() != 1 || gen11.headings[0].text != "El principio")
		fail("Genesis 1:1", "headings", "[El principio]", "unexpected");
	if (gen21.headings.size() != 1 || gen21.headings[0].text != "Otro capítulo")
		fail("Genesis 2:1", "headings", "[Otro capítulo]", "unexpected");
	if (john316.headings.size() != 1 || john316.headings[0].text != "El amor")
		fail("John 3:16", "headings", "[El amor]", "unexpected");

	auto checkWord = [&](const char *reference, const BibleVerseContent &verse,
		const char *text, StrongLanguage language, const std::vector<int> &numbers) {
		if (verse.words.size() != 1 || verse.words[0].text != text ||
		    verse.words[0].strongs.size() != numbers.size()) {
			fail(reference, "words", text, "unexpected");
			return;
		}
		for (std::size_t index = 0; index < numbers.size(); ++index)
			if (verse.words[0].strongs[index].language != language ||
			    verse.words[0].strongs[index].number != numbers[index])
				fail(reference, "words.strongs[" + std::to_string(index) + "]",
					std::to_string(numbers[index]),
					std::to_string(verse.words[0].strongs[index].number));
	};
	checkWord("Genesis 1:1", gen11, "Dios", StrongLanguage::Hebrew, { 430 });
	checkWord("Genesis 1:2", gen12, "Jehová", StrongLanguage::Hebrew, { 430 });
	checkWord("John 3:16", john316, "amó", StrongLanguage::Greek, { 25 });
	checkWord("John 3:17", john317, "SEÑOR", StrongLanguage::Greek, { 2424, 5547 });

	if (john316.plainText != "Jesús amó al mundo." || john316.footnotes.size() != 1 ||
	    john316.footnotes[0].label != "+" || john316.footnotes[0].body != "Nota de amor." ||
	    john316.footnotes[0].offset != std::string("Jesús amó").size())
		fail("John 3:16", "footnotes", "+|Nota de amor.|11", "unexpected");
	if (john316.plainText.find("Nota de amor.") != std::string::npos)
		fail("John 3:16", "plainText", "footnote body excluded", john316.plainText);
	if (john317.plainText != "SEÑOR vino." || john317.crossReferences.size() != 1 ||
	    john317.crossReferences[0].displayText != "Genesis 1:1; John 3:16" ||
	    john317.crossReferences[0].offset != std::string("SEÑOR").size() ||
	    john317.crossReferences[0].references.size() != 2 ||
	    john317.crossReferences[0].references[0].book != 1 ||
	    john317.crossReferences[0].references[1].book != 43)
		fail("John 3:17", "crossReferences",
			"Genesis 1:1; John 3:16|ordered targets|6", "unexpected");
	if (gen11.plainText.find("corazón") == std::string::npos ||
	    gen12.plainText.find("Moisés") == std::string::npos ||
	    gen12.plainText.find("Jehová") == std::string::npos ||
	    john316.plainText.find("Jesús") == std::string::npos ||
	    john317.plainText.find("SEÑOR") == std::string::npos ||
	    john41.plainText.find("Moisés") == std::string::npos)
		fail("fixture", "UTF-8 corpus", "all required words", "missing word");
	return failures;
}

} // namespace

int main()
{
	gchar *temporary = g_dir_make_tmp("osis-semantics-XXXXXX", nullptr);
	if (!temporary) return 2;
	const std::string directory = temporary;
	const std::string fixtureRoot = SRCDIR "/tests/fixtures/osis/";

	std::size_t importFailures = 0;
	if (!importOsisFixture(fixtureRoot + "semantic-container.xml",
		directory + "/container.sqlite", "container")) ++importFailures;
	if (!importOsisFixture(fixtureRoot + "semantic-milestone.xml",
		directory + "/milestone.sqlite", "milestone")) ++importFailures;
	if (!importOsisFixture(fixtureRoot + "semantic-container.xml",
		directory + "/osis.sqlite", "osis")) ++importFailures;

	UsfmImportStats usfmStats;
	std::string usfmError;
	if (!importUsfm({ fixtureRoot + "semantic-GEN.usfm",
		fixtureRoot + "semantic-JHN.usfm" }, directory + "/usfm.sqlite",
		options("usfm"), usfmStats, usfmError)) {
		std::cerr << "USFM import failed: " << usfmError << '\n';
		++importFailures;
	}

	const MixedCase mixedCases[] = {
		{ "<w>Jesús</w>, dijo.", "<w>Jesús</w>\n        , dijo.", "Jesús, dijo." },
		{ "En el <w>principio</w>.", "En el\n        <w>principio</w>\n        .", "En el principio." },
		{ "<w>En</w> <w>el</w> <w>principio</w>.",
			"<w>En</w>\n        <w>el</w>\n        <w>principio</w>\n        .", "En el principio." },
		{ "Jesús <w>dijo</w>:", "Jesús\n        <w>dijo</w>\n        :", "Jesús dijo:" },
		{ "<w>corazón</w>, <w>SEÑOR</w>",
			"<w>corazón</w>\n        ,\n        <w>SEÑOR</w>", "corazón, SEÑOR" },
		{ "En el <w>principio</w> creó Dios.",
			"En el\n        <w>principio</w>\n        creó Dios.", "En el principio creó Dios." },
		{ "<w>palabra</w>; sigue", "<w>palabra</w>\n        ; sigue", "palabra; sigue" },
		{ "<w>palabra</w>? sigue", "<w>palabra</w>\n        ? sigue", "palabra? sigue" },
		{ "<w>palabra</w>! sigue", "<w>palabra</w>\n        ! sigue", "palabra! sigue" },
		{ "Jesús <w lemma=\"strong:G25\">amó</w> al mundo.",
			"Jesús\n        <w lemma=\"strong:G25\">amó</w>\n        al mundo.",
			"Jesús amó al mundo." },
	};

	for (std::size_t index = 0; index < sizeof(mixedCases) / sizeof(mixedCases[0]); ++index) {
		const std::string compactInput = directory + "/mixed-" + std::to_string(index) + "-compact.xml";
		const std::string prettyInput = directory + "/mixed-" + std::to_string(index) + "-pretty.xml";
		if (!writeFile(compactInput, mixedDocument(mixedCases[index].compact, false)) ||
		    !writeFile(prettyInput, mixedDocument(mixedCases[index].pretty, true))) {
			++importFailures;
			continue;
		}
		if (!importOsisFixture(compactInput,
			directory + "/mixed-" + std::to_string(index) + "-compact.sqlite",
			"mixed-" + std::to_string(index) + "-compact")) ++importFailures;
		if (!importOsisFixture(prettyInput,
			directory + "/mixed-" + std::to_string(index) + "-pretty.sqlite",
			"mixed-" + std::to_string(index) + "-pretty")) ++importFailures;
	}

	std::size_t containerMilestoneMismatches = importFailures;
	std::size_t usfmOsisMismatches = importFailures;
	std::size_t mixedFailures = importFailures;
	std::size_t offsetRows = 0;
	std::size_t invalidOffsets = 0;
	std::size_t substringMismatches = 0;

	SqliteBibleBackend backend(directory);
	if (importFailures == 0) {
		containerMilestoneMismatches = compareNeutralModules(
			backend, "container", backend, "milestone").mismatches;
		containerMilestoneMismatches += fixtureSemanticFailures(backend, "container");
		containerMilestoneMismatches += fixtureSemanticFailures(backend, "milestone");
		usfmOsisMismatches = compareNeutralModules(
			backend, "usfm", backend, "osis").mismatches;
		usfmOsisMismatches += fixtureSemanticFailures(backend, "usfm");
		usfmOsisMismatches += fixtureSemanticFailures(backend, "osis");

		for (std::size_t index = 0; index < sizeof(mixedCases) / sizeof(mixedCases[0]); ++index) {
			const std::string compactModule = "mixed-" + std::to_string(index) + "-compact";
			const std::string prettyModule = "mixed-" + std::to_string(index) + "-pretty";
			mixedFailures += compareNeutralModules(backend, compactModule,
				backend, prettyModule).mismatches;
			BibleKeyInfo key;
			if (!backend.resolveKey(compactModule, "John 3:16", key)) {
				++mixedFailures;
				continue;
			}
			const BibleVerseContent compact = backend.getVerseContent(compactModule, key.reference);
			if (compact.plainText != mixedCases[index].expected) {
				++mixedFailures;
				std::cerr << "reference=John.3.16\nfield=plainText\nexpected="
					<< mixedCases[index].expected << "\nactual=" << compact.plainText << '\n';
			}
		}

		BibleKeyInfo strongKey;
		if (!backend.resolveKey("osis", "John 3:16", strongKey)) {
			++mixedFailures;
		} else {
			const BibleVerseContent strong = backend.getVerseContent("osis", strongKey.reference);
			if (strong.words.size() != 1 || strong.words[0].text != "amó" ||
			    strong.plainText.substr(strong.words[0].start, strong.words[0].length) != "amó")
				++mixedFailures;
			if (strong.plainText != "Jesús amó al mundo." || strong.footnotes.size() != 1 ||
			    strong.footnotes[0].offset != std::string("Jesús amó").size())
				++mixedFailures;
		}
		BibleKeyInfo crossKey;
		if (!backend.resolveKey("osis", "John 3:17", crossKey)) {
			++mixedFailures;
		} else {
			const BibleVerseContent cross = backend.getVerseContent("osis", crossKey.reference);
			if (cross.plainText != "SEÑOR vino." || cross.crossReferences.size() != 1 ||
			    cross.crossReferences[0].offset != std::string("SEÑOR").size())
				++mixedFailures;
		}

		for (const std::string &module : { "container", "milestone", "usfm", "osis" }) {
			const NeutralOffsetValidationResult validation = validateNeutralOffsets(backend, module);
			offsetRows += validation.rowsChecked;
			invalidOffsets += validation.invalidOffsets;
			substringMismatches += validation.substringMismatches;
		}
	}

	std::cout << "container_milestone_mismatches=" << containerMilestoneMismatches << '\n'
		<< "usfm_osis_mismatches=" << usfmOsisMismatches << '\n'
		<< "mixed_content_cases=" << sizeof(mixedCases) / sizeof(mixedCases[0]) << '\n'
		<< "mixed_content_failures=" << mixedFailures << '\n'
		<< "offset_rows_checked=" << offsetRows << '\n'
		<< "invalid_offsets=" << invalidOffsets << '\n'
		<< "substring_mismatches=" << substringMismatches << '\n';

	const bool success = containerMilestoneMismatches == 0 &&
		usfmOsisMismatches == 0 && mixedFailures == 0 &&
		invalidOffsets == 0 && substringMismatches == 0;
	g_free(temporary);
	return success ? 0 : 1;
}
