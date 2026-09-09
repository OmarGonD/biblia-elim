#include "backend/osis_importer.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/usfm_importer.h"
#include "neutral_module_comparator.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <iostream>
#include <map>
#include <string>
#include <utility>
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
	const std::string &module, UsfmImportStats *resultStats = nullptr)
{
	UsfmImportStats stats;
	std::string error;
	if (importOsis(input, output, options(module), stats, error)) {
		if (resultStats) *resultStats = std::move(stats);
		return true;
	}
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

struct UnicodeCase {
	const char *name;
	const char *compact;
	const char *pretty;
	const char *expected;
	std::vector<std::string> words;
};

struct BoundaryCase {
	const char *reference;
	const char *text;
	bool paragraphBreak;
	std::vector<std::string> headings;
};

std::size_t boundaryFailures(BibleBackend &backend, const std::string &module)
{
	std::size_t failures = 0;
	if (!backend.moduleCapabilities(module).headings) {
		++failures;
		std::cerr << "module=" << module
			<< "\nfield=capabilities.headings\nexpected=true\nactual=false\n";
	}
	const BoundaryCase cases[] = {
		{ "Genesis 1:1", "One.", true,
			{ "Before first chapter", "Wrapped heading" } },
		{ "Genesis 1:2", "Two.", false, {} },
		{ "Genesis 1:3", "Three.", false, {} },
		{ "Genesis 1:4", "Four.", true,
			{ "First consecutive", "Second consecutive" } },
		{ "Genesis 2:1", "Five.", false, { "Between chapters" } },
		{ "Genesis 2:2", "Six.", true, { "Nested heading" } },
		{ "Genesis 2:3", "Seven.", false, {} },
		{ "Genesis 3:1", "Eight.", true, { "Third chapter" } },
	};
	for (const BoundaryCase &expected : cases) {
		BibleKeyInfo key;
		if (!backend.resolveKey(module, expected.reference, key)) {
			++failures;
			std::cerr << "module=" << module << "\nreference=" << expected.reference
				<< "\nfield=reference\nexpected=present\nactual=missing\n";
			continue;
		}
		const BibleVerseContent actual = backend.getVerseContent(module, key.reference);
		bool matches = actual.valid && actual.plainText == expected.text &&
			actual.paragraphBreak == expected.paragraphBreak &&
			actual.headings.size() == expected.headings.size();
		for (std::size_t index = 0;
		     matches && index < expected.headings.size(); ++index)
			matches = actual.headings[index].text == expected.headings[index];
		if (!matches) {
			++failures;
			std::cerr << "module=" << module << "\nreference=" << expected.reference
				<< "\nfield=boundaryState\nexpected=" << expected.text << '|'
				<< expected.paragraphBreak << '|' << expected.headings.size()
				<< " headings\nactual=" << actual.plainText << '|'
				<< actual.paragraphBreak << '|' << actual.headings.size()
				<< " headings\n";
		}
	}
	return failures;
}

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
	UsfmImportStats boundaryContainerStats;
	UsfmImportStats boundaryMilestoneStats;
	if (!importOsisFixture(fixtureRoot + "boundaries-container.xml",
		directory + "/boundaries-container.sqlite", "boundaries-container",
		&boundaryContainerStats))
		++importFailures;
	if (!importOsisFixture(fixtureRoot + "boundaries-milestone.xml",
		directory + "/boundaries-milestone.sqlite", "boundaries-milestone",
		&boundaryMilestoneStats))
		++importFailures;

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
	/* XML formatting whitespace is collapsed to an ASCII separator. Unicode
	 * spacing, normalization form, CDATA text, and UTF-8 punctuation are
	 * canonical content and therefore remain byte-for-byte intact. */
	const UnicodeCase unicodeCases[] = {
		{ "scripts",
			"<w>λόγος</w> <w>שָׁלוֹם</w> <w>世界</w>。",
			"<w>λόγος</w>\n        <w>שָׁלוֹם</w>\n        <w>世界</w>\n        。",
			"λόγος שָׁלוֹם 世界。", { "λόγος", "שָׁלוֹם", "世界" } },
		{ "combining",
			"Caf<w>é</w>, <w>नमस्ते</w>!",
			"Caf<w>é</w>\n        ,\n        <w>नमस्ते</w>\n        !",
			"Café, नमस्ते!", { "é", "नमस्ते" } },
		{ "unicode-spacing",
			"<w>uno</w>&#xA0;<w>dos</w>&#x2003;<w>три</w>",
			"<w>uno</w>\n        &#xA0;\n        <w>dos</w>\n        &#x2003;\n        <w>три</w>",
			"uno dos три", { "uno", "dos", "три" } },
		{ "cdata",
			"<w>ca<![CDATA[fé]]></w>. <![CDATA[λέξη]]> <w>終</w>",
			"<w>ca<![CDATA[fé]]></w>\n        .\n        <![CDATA[λέξη]]>\n        <w>終</w>",
			"café. λέξη 終", { "café", "終" } },
		{ "unicode-punctuation",
			"<w>سلام</w>؟ <w>世界</w>。",
			"<w>سلام</w>\n        ؟\n        <w>世界</w>\n        。",
			"سلام؟ 世界。", { "سلام", "世界" } },
	};
	const std::string nestedNoteCompact =
		"Inicio<note type=\"footnote\" n=\"*\">Nota <hi>muy <w>clara</w></hi> "
		"con <reference osisRef=\"John.3.16\"><hi>Juan</hi> 3:16</reference>."
		"</note> continúa<note type=\"crossReference\">Véase <hi><reference "
		"osisRef=\"Gen.1.1\">Génesis <w>1:1</w></reference></hi>; <reference "
		"osisRef=\"Rom.8.28 John.3.16\"><hi>Romanos</hi> 8:28 y <w>Juan 3:16</w>"
		"</reference>.</note> fin.";
	const std::string nestedNotePretty =
		"Inicio\n        <note type=\"footnote\" n=\"*\">\n          Nota\n"
		"          <hi>muy <w>clara</w></hi>\n          con\n"
		"          <reference osisRef=\"John.3.16\"><hi>Juan</hi> 3:16</reference>.\n"
		"        </note>\n        continúa\n        <note type=\"crossReference\">\n"
		"          Véase\n          <hi><reference osisRef=\"Gen.1.1\">Génesis "
		"<w>1:1</w></reference></hi>;\n          <reference "
		"osisRef=\"Rom.8.28 John.3.16\"><hi>Romanos</hi> 8:28 y "
		"<w>Juan 3:16</w></reference>.\n        </note>\n        fin.";
	const std::string rangeNoteCompact =
		"Antes<note type=\"crossReference\">Véase <reference "
		"osisRef=\"Gen.1.1 Exod.2.1-Exod.2.3 John.3.16\">Génesis 1:1; "
		"Éxodo 2:1-3; Juan 3:16</reference>; <reference "
		"osisRef=\"Rom.8.28-Rom.8.30 Ps.23.1\">Romanos 8:28-30; Salmo 23:1"
		"</reference>.</note> después.";
	const std::string rangeNotePretty =
		"Antes\n        <note type=\"crossReference\">\n          Véase\n"
		"          <reference osisRef=\"Gen.1.1 Exod.2.1-Exod.2.3 John.3.16\">"
		"Génesis 1:1; Éxodo 2:1-3; Juan 3:16</reference>;\n"
		"          <reference osisRef=\"Rom.8.28-Rom.8.30 Ps.23.1\">"
		"Romanos 8:28-30; Salmo 23:1</reference>.\n        </note>\n"
		"        después.";

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
	for (const UnicodeCase &unicodeCase : unicodeCases) {
		const std::string prefix = "unicode-" + std::string(unicodeCase.name);
		const std::string compactInput = directory + "/" + prefix + "-compact.xml";
		const std::string prettyInput = directory + "/" + prefix + "-pretty.xml";
		if (!writeFile(compactInput, mixedDocument(unicodeCase.compact, false)) ||
		    !writeFile(prettyInput, mixedDocument(unicodeCase.pretty, true))) {
			++importFailures;
			continue;
		}
		if (!importOsisFixture(compactInput, directory + "/" + prefix + "-compact.sqlite",
			prefix + "-compact")) ++importFailures;
		if (!importOsisFixture(prettyInput, directory + "/" + prefix + "-pretty.sqlite",
			prefix + "-pretty")) ++importFailures;
	}
	const std::string nestedCompactInput = directory + "/nested-note-compact.xml";
	const std::string nestedPrettyInput = directory + "/nested-note-pretty.xml";
	if (!writeFile(nestedCompactInput, mixedDocument(nestedNoteCompact, false)) ||
	    !writeFile(nestedPrettyInput, mixedDocument(nestedNotePretty, true))) {
		++importFailures;
	} else {
		if (!importOsisFixture(nestedCompactInput, directory + "/nested-note-compact.sqlite",
			"nested-note-compact")) ++importFailures;
		if (!importOsisFixture(nestedPrettyInput, directory + "/nested-note-pretty.sqlite",
			"nested-note-pretty")) ++importFailures;
	}
	UsfmImportStats rangeCompactStats;
	UsfmImportStats rangePrettyStats;
	const std::string rangeCompactInput = directory + "/range-note-compact.xml";
	const std::string rangePrettyInput = directory + "/range-note-pretty.xml";
	if (!writeFile(rangeCompactInput, mixedDocument(rangeNoteCompact, false)) ||
	    !writeFile(rangePrettyInput, mixedDocument(rangeNotePretty, true))) {
		++importFailures;
	} else {
		if (!importOsisFixture(rangeCompactInput, directory + "/range-note-compact.sqlite",
			"range-note-compact", &rangeCompactStats)) ++importFailures;
		if (!importOsisFixture(rangePrettyInput, directory + "/range-note-pretty.sqlite",
			"range-note-pretty", &rangePrettyStats)) ++importFailures;
	}

	std::size_t containerMilestoneMismatches = importFailures;
	std::size_t usfmOsisMismatches = importFailures;
	std::size_t mixedFailures = importFailures;
	std::size_t unicodeFailures = importFailures;
	std::size_t nestedNoteFailures = importFailures;
	std::size_t headingParagraphFailures = importFailures;
	std::size_t rangeReferenceFailures = importFailures;
	if (boundaryContainerStats.headingsImported != 7 ||
	    boundaryMilestoneStats.headingsImported != 7 ||
	    boundaryContainerStats.paragraphMarkers != 6 ||
	    boundaryMilestoneStats.paragraphMarkers != 6) {
		++headingParagraphFailures;
		std::cerr << "field=boundaryStats\nexpected=headings 7|7, paragraphs 6|6\n"
			<< "actual=headings "
			<< boundaryContainerStats.headingsImported << '|'
			<< boundaryMilestoneStats.headingsImported << ", paragraphs "
			<< boundaryContainerStats.paragraphMarkers << '|'
			<< boundaryMilestoneStats.paragraphMarkers << '\n';
	}
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
		headingParagraphFailures += compareNeutralModules(backend,
			"boundaries-container", backend, "boundaries-milestone").mismatches;
		headingParagraphFailures += boundaryFailures(backend, "boundaries-container");
		headingParagraphFailures += boundaryFailures(backend, "boundaries-milestone");

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

		for (const UnicodeCase &unicodeCase : unicodeCases) {
			const std::string prefix = "unicode-" + std::string(unicodeCase.name);
			const std::string compactModule = prefix + "-compact";
			const std::string prettyModule = prefix + "-pretty";
			unicodeFailures += compareNeutralModules(backend, compactModule,
				backend, prettyModule).mismatches;
			BibleKeyInfo key;
			if (!backend.resolveKey(compactModule, "John 3:16", key)) {
				++unicodeFailures;
				continue;
			}
			const BibleVerseContent content = backend.getVerseContent(
				compactModule, key.reference);
			if (content.plainText != unicodeCase.expected) {
				++unicodeFailures;
				std::cerr << "case=" << unicodeCase.name
					<< "\nfield=plainText\nexpected=" << unicodeCase.expected
					<< "\nactual=" << content.plainText << '\n';
			}
			if (content.words.size() != unicodeCase.words.size()) {
				++unicodeFailures;
				std::cerr << "case=" << unicodeCase.name
					<< "\nfield=words.size\nexpected=" << unicodeCase.words.size()
					<< "\nactual=" << content.words.size() << '\n';
				continue;
			}
			std::size_t searchFrom = 0;
			for (std::size_t index = 0; index < unicodeCase.words.size(); ++index) {
				const std::string &expectedWord = unicodeCase.words[index];
				const std::size_t expectedStart = std::string(unicodeCase.expected).find(
					expectedWord, searchFrom);
				const BibleWordInfo &actualWord = content.words[index];
				if (expectedStart == std::string::npos ||
				    actualWord.start != expectedStart ||
				    actualWord.length != expectedWord.size() ||
				    actualWord.text != expectedWord ||
				    content.plainText.substr(actualWord.start, actualWord.length) != expectedWord) {
					++unicodeFailures;
					std::cerr << "case=" << unicodeCase.name << "\nfield=words[" << index
						<< "].byteRange\nexpected=" << expectedStart << '+'
						<< expectedWord.size() << '|' << expectedWord << "\nactual="
						<< actualWord.start << '+' << actualWord.length << '|'
						<< actualWord.text << '\n';
				}
				if (expectedStart != std::string::npos)
					searchFrom = expectedStart + expectedWord.size();
			}
		}

		nestedNoteFailures += compareNeutralModules(backend, "nested-note-compact",
			backend, "nested-note-pretty").mismatches;
		BibleKeyInfo nestedKey;
		if (!backend.resolveKey("nested-note-compact", "John 3:16", nestedKey)) {
			++nestedNoteFailures;
		} else {
			const BibleVerseContent nested = backend.getVerseContent(
				"nested-note-compact", nestedKey.reference);
			const std::string footnotePrefix = "Inicio";
			const std::string crossReferencePrefix = "Inicio continúa";
			if (nested.plainText != "Inicio continúa fin." || nested.footnotes.size() != 1 ||
			    nested.footnotes[0].label != "*" ||
			    nested.footnotes[0].body != "Nota muy clara con Juan 3:16." ||
			    nested.footnotes[0].offset > nested.plainText.size() ||
			    nested.plainText.substr(0, nested.footnotes[0].offset) != footnotePrefix) {
				++nestedNoteFailures;
				std::cerr << "reference=John.3.16\nfield=nestedFootnote\n"
					<< "expected=*|Nota muy clara con Juan 3:16.|" << footnotePrefix.size()
					<< "\nactual=" << (nested.footnotes.empty() ? "missing" :
						nested.footnotes[0].label + "|" + nested.footnotes[0].body + "|" +
						std::to_string(nested.footnotes[0].offset)) << '\n';
			}
			if (nested.crossReferences.size() != 1 ||
			    nested.crossReferences[0].displayText !=
				    "Véase Génesis 1:1; Romanos 8:28 y Juan 3:16." ||
			    nested.crossReferences[0].offset > nested.plainText.size() ||
			    nested.plainText.substr(0, nested.crossReferences[0].offset) !=
				    crossReferencePrefix ||
			    nested.crossReferences[0].references.size() != 3 ||
			    nested.crossReferences[0].references[0].book != 1 ||
			    nested.crossReferences[0].references[1].book != 45 ||
			    nested.crossReferences[0].references[2].book != 43) {
				++nestedNoteFailures;
				std::cerr << "reference=John.3.16\nfield=nestedCrossReference\n"
					<< "expected=ordered Gen.1.1,Rom.8.28,John.3.16 at "
					<< crossReferencePrefix.size() << "\nactual=unexpected\n";
			}
			if (nested.plainText.find("Nota") != std::string::npos ||
			    nested.plainText.find("Véase") != std::string::npos) {
				++nestedNoteFailures;
				std::cerr << "reference=John.3.16\nfield=plainText.noteLeak\n"
					<< "expected=no note text\nactual=" << nested.plainText << '\n';
			}
		}

		rangeReferenceFailures += compareNeutralModules(backend, "range-note-compact",
			backend, "range-note-pretty").mismatches;
		for (const UsfmImportStats *stats : { &rangeCompactStats, &rangePrettyStats }) {
			if (stats->crossReferencesImported != 1 ||
			    stats->crossrefTargetsResolved != 3 ||
			    stats->crossrefTargetsUnresolved != 0 ||
			    !stats->unresolvedReferences.empty() ||
			    stats->rangeReferences !=
				std::map<std::string, std::size_t>{
					{ "Exod.2.1-Exod.2.3", 1 },
					{ "Rom.8.28-Rom.8.30", 1 } }) {
				++rangeReferenceFailures;
				std::cerr << "field=rangeAudit\nexpected=1 cross-reference, 3 simple "
					"targets, 2 exact ranges, 0 unresolved\nactual=unexpected\n";
			}
		}
		BibleKeyInfo rangeKey;
		if (!backend.resolveKey("range-note-compact", "John 3:16", rangeKey)) {
			++rangeReferenceFailures;
		} else {
			const BibleVerseContent range = backend.getVerseContent(
				"range-note-compact", rangeKey.reference);
			const std::string expectedDisplay =
				"Véase Génesis 1:1; Éxodo 2:1-3; Juan 3:16; "
				"Romanos 8:28-30; Salmo 23:1.";
			const bool orderedSimpleTargets = range.crossReferences.size() == 1 &&
				range.crossReferences[0].references.size() == 3 &&
				range.crossReferences[0].references[0].book == 1 &&
				range.crossReferences[0].references[0].chapter == 1 &&
				range.crossReferences[0].references[0].verse == 1 &&
				range.crossReferences[0].references[1].book == 43 &&
				range.crossReferences[0].references[1].chapter == 3 &&
				range.crossReferences[0].references[1].verse == 16 &&
				range.crossReferences[0].references[2].book == 19 &&
				range.crossReferences[0].references[2].chapter == 23 &&
				range.crossReferences[0].references[2].verse == 1;
			if (range.plainText != "Antes después." || !orderedSimpleTargets ||
			    range.crossReferences[0].displayText != expectedDisplay ||
			    range.crossReferences[0].offset > range.plainText.size() ||
			    range.plainText.substr(0, range.crossReferences[0].offset) != "Antes") {
				++rangeReferenceFailures;
				std::cerr << "reference=John.3.16\nfield=partialRangeCrossReference\n"
					<< "expected=display preserved|Gen.1.1,John.3.16,Ps.23.1|offset 5\n"
					<< "actual=unexpected\n";
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

		for (const std::string &module : { "container", "milestone", "usfm", "osis",
			"nested-note-compact", "nested-note-pretty", "boundaries-container",
			"boundaries-milestone", "range-note-compact", "range-note-pretty" }) {
			const NeutralOffsetValidationResult validation = validateNeutralOffsets(backend, module);
			offsetRows += validation.rowsChecked;
			invalidOffsets += validation.invalidOffsets;
			substringMismatches += validation.substringMismatches;
		}
		for (const UnicodeCase &unicodeCase : unicodeCases) {
			const std::string prefix = "unicode-" + std::string(unicodeCase.name);
			for (const std::string &suffix : { "-compact", "-pretty" }) {
				const NeutralOffsetValidationResult validation = validateNeutralOffsets(
					backend, prefix + suffix);
				offsetRows += validation.rowsChecked;
				invalidOffsets += validation.invalidOffsets;
				substringMismatches += validation.substringMismatches;
			}
		}
	}

	std::cout << "container_milestone_mismatches=" << containerMilestoneMismatches << '\n'
		<< "usfm_osis_mismatches=" << usfmOsisMismatches << '\n'
		<< "mixed_content_cases=" << sizeof(mixedCases) / sizeof(mixedCases[0]) << '\n'
		<< "mixed_content_failures=" << mixedFailures << '\n'
		<< "unicode_edge_cases=" << sizeof(unicodeCases) / sizeof(unicodeCases[0]) << '\n'
		<< "unicode_edge_failures=" << unicodeFailures << '\n'
		<< "nested_note_failures=" << nestedNoteFailures << '\n'
		<< "heading_paragraph_boundary_failures=" << headingParagraphFailures << '\n'
		<< "partial_range_failures=" << rangeReferenceFailures << '\n'
		<< "offset_rows_checked=" << offsetRows << '\n'
		<< "invalid_offsets=" << invalidOffsets << '\n'
		<< "substring_mismatches=" << substringMismatches << '\n';

	const bool success = containerMilestoneMismatches == 0 &&
		usfmOsisMismatches == 0 && mixedFailures == 0 && unicodeFailures == 0 &&
		nestedNoteFailures == 0 && headingParagraphFailures == 0 &&
		rangeReferenceFailures == 0 &&
		invalidOffsets == 0 && substringMismatches == 0;
	g_free(temporary);
	return success ? 0 : 1;
}
