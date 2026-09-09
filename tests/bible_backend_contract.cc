#include "bible_backend_contract.h"

#include <glib.h>

#include "backend/bible_backend.h"

void runBibleBackendContractTests(BibleBackend &backend,
				  const BibleBackendContractFixture &fixture)
{
	const std::vector<BibleModuleInfo> modules = backend.listModules();
	g_assert_cmpuint(modules.size(), >=, 1);
	g_assert_true(backend.hasModule(fixture.module));
	g_assert_false(backend.hasModule("Missing"));
	g_assert_cmpint(static_cast<int>(backend.moduleType(fixture.module)), ==,
			static_cast<int>(BibleModuleType::Bible));
	std::string description = backend.moduleDescription(fixture.module);
	std::string language = backend.moduleLanguage(fixture.module);
	g_assert_cmpstr(description.c_str(), ==, fixture.description.c_str());
	g_assert_cmpstr(language.c_str(), ==, fixture.language.c_str());

	BibleModuleCapabilities capabilities =
		backend.moduleCapabilities(fixture.module);
	g_assert_true(capabilities.verses);
	g_assert_true(capabilities.search);

	BibleKeyInfo key;
	g_assert_true(backend.resolveKey(fixture.module, fixture.reference, key));
	g_assert_cmpint(key.reference.testament, ==, fixture.testament);
	g_assert_cmpint(key.reference.book, ==, fixture.bookId);
	std::string osis = backend.osisRefFromKey(fixture.module, fixture.reference);
	g_assert_cmpstr(osis.c_str(), ==, "John.3.16");
	g_assert_false(backend.resolveKey(fixture.module, "Not a reference", key));
	g_assert_false(backend.resolveKey("Missing", "John 3:16", key));

	BibleReference chapter_reference = { fixture.testament, fixture.bookId, 3, 1 };
	std::vector<BibleVerse> chapter =
		backend.getChapter(fixture.module, chapter_reference, false);
	g_assert_cmpuint(chapter.size(), ==, fixture.chapterSize);
	g_assert_cmpstr(chapter.front().key.c_str(), ==, "John 3:16");

	BibleReference verse_reference = { fixture.testament, fixture.bookId, 3, 16 };
	BibleVerseContent content =
		backend.getVerseContent(fixture.module, verse_reference, true);
	g_assert_true(content.valid);
	g_assert_cmpstr(content.plainText.c_str(), ==, fixture.verseText.c_str());
	g_assert_cmpstr(content.renderedText.c_str(), ==, fixture.verseText.c_str());
	if (fixture.enrichedWords) {
		g_assert_true(capabilities.strongs);
		g_assert_true(capabilities.morphology);
		g_assert_cmpuint(content.words.size(), >=, 1);
		g_assert_cmpstr(content.words[0].strong.c_str(), ==, "G25");
		if (fixture.legacyWordFields) {
			g_assert_cmpstr(content.words[0].lemma.c_str(), ==, "agapao");
			g_assert_cmpstr(content.words[0].morphology.c_str(), ==, "V-AAI-3S");
		}
		g_assert_cmpuint(content.words[0].morphologyTags.size(), ==, 2);
		g_assert_true(content.words[0].morphologyTags[0] ==
			MorphologyTag({ "robinson", "V-AAI-3S" }));
		g_assert_true(content.words[0].morphologyTags[1] ==
			MorphologyTag({ "custom.alpha", "opaque/code" }));
		g_assert_cmpuint(content.words.size(), >=, 2);
		g_assert_true(content.words[1].strongs.empty());
		g_assert_cmpuint(content.words[1].morphologyTags.size(), ==, 1);
		g_assert_true(content.words[1].morphologyTags[0] ==
			MorphologyTag({ "", "HR/Ncfsa" }));

		const MorphologyTag exact{ "robinson", "V-AAI-3S" };
		MorphologyOccurrencePage first = backend.findMorphologyOccurrencePage(
			fixture.module, exact, 1, 0);
		g_assert_cmpuint(first.occurrences.size(), ==, 1);
		g_assert_true(first.hasMore);
		g_assert_true(first.occurrences[0].morphology == exact);
		g_assert_false(first.occurrences[0].key.empty());
		g_assert_false(first.occurrences[0].word.empty());
		g_assert_cmpstr(first.occurrences[0].context.substr(
			first.occurrences[0].start, first.occurrences[0].length).c_str(),
			==, first.occurrences[0].word.c_str());
		MorphologyOccurrencePage second = backend.findMorphologyOccurrencePage(
			fixture.module, exact, 1, 1);
		g_assert_cmpuint(second.occurrences.size(), ==, 1);
		g_assert_false(second.hasMore);
		const BibleReference &firstReference = first.occurrences[0].reference;
		const BibleReference &secondReference = second.occurrences[0].reference;
		g_assert_true(firstReference.book < secondReference.book ||
			(firstReference.book == secondReference.book &&
			 (firstReference.chapter < secondReference.chapter ||
			  (firstReference.chapter == secondReference.chapter &&
			   firstReference.verse < secondReference.verse))));
		g_assert_true(backend.findMorphologyOccurrencePage(
			fixture.module, exact, 0, 0).hasMore);
		g_assert_true(backend.findMorphologyOccurrences(
			fixture.module, exact, 10, 100).empty());
		g_assert_cmpuint(backend.findMorphologyOccurrences(fixture.module,
			{ "custom.alpha", "V-AAI-3S" }, 100, 0).size(), ==, 1);
		g_assert_true(backend.findMorphologyOccurrences(fixture.module,
			{ "robinson", "missing" }, 100, 0).empty());
		g_assert_true(backend.findMorphologyOccurrences("Missing",
			exact, 100, 0).empty());
	} else {
		g_assert_false(capabilities.strongs);
		g_assert_false(capabilities.morphology);
		g_assert_true(content.words.empty());
	}

	std::string next = backend.navigate(fixture.module, fixture.reference, 1);
	std::string verse17 = backend.setVerse(fixture.module, fixture.reference, 17);
	std::string genesis = backend.setBook(fixture.module, fixture.reference, 1, 1);
	g_assert_cmpstr(next.c_str(), ==, fixture.nextReference.c_str());
	g_assert_cmpstr(verse17.c_str(), ==, fixture.nextReference.c_str());
	g_assert_cmpstr(genesis.c_str(), ==, "Genesis 1:1");
	g_assert_cmpuint(backend.bookNames(fixture.module, 2).size(), ==, 1);

	BibleSearchQuery query;
	query.text = fixture.searchText;
	query.mode = BibleSearchMode::MultiWord;
	query.caseSensitive = false;
	std::vector<BibleSearchResult> results = backend.search(fixture.module, query);
	g_assert_cmpuint(results.size(), ==, fixture.searchResults);
	query.limit = 1;
	query.offset = 1;
	g_assert_cmpuint(backend.search(fixture.module, query).size(), <=, 1);
	g_assert_true(backend.search("Missing", query).empty());

	if (!fixture.dictionaryModule.empty()) {
		DictionaryEntry dictionary =
			backend.lookupDictionary(fixture.dictionaryModule, "G25");
		g_assert_true(dictionary.valid);
		g_assert_cmpstr(dictionary.text.c_str(), ==, "to love");
		g_assert_false(backend.lookupDictionary(fixture.dictionaryModule,
			"missing").valid);
	}

	BibleReference invalid = { 9, 9, 9, 9 };
	g_assert_false(backend.getVerseContent(fixture.module, invalid).valid);
}
