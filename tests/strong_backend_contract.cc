#include "strong_backend_contract.h"

#include <glib.h>

#include "backend/bible_resources.h"

namespace {
const BibleWordInfo *wordNamed(const BibleVerseContent &content,
	const std::string &name)
{
	for (const BibleWordInfo &word : content.words)
		if (word.text == name) return &word;
	return nullptr;
}

class ContractLexicon final : public BibleLexicon
{
public:
	LexiconEntry lookupStrong(const StrongId &id) const override
	{
		LexiconEntry entry;
		entry.id = id;
		const StrongId h430{StrongLanguage::Hebrew, 430};
		const StrongId g25{StrongLanguage::Greek, 25};
		if (id == h430) {
			entry.lemma = "elohim";
			entry.transliteration = "elohim";
			entry.pronunciation = "el-o-heem";
			entry.definition = "God";
			entry.valid = true;
		} else if (id == g25) {
			/* A present entry remains valid when optional fields are empty. */
			entry.lemma = "agapao";
			entry.valid = true;
		}
		return entry;
	}
};

BibleVerseContent contentAt(BibleBackend &backend, const std::string &module,
	const std::string &key, BibleReference &reference)
{
	BibleKeyInfo info;
	g_assert_true(backend.resolveKey(module, key, info));
	reference = info.reference;
	return backend.getVerseContent(module, reference);
}
}

void runStrongBackendContractTests(BibleBackend &backend,
	const StrongBackendContractFixture &fixture)
{
	const StrongId h430{StrongLanguage::Hebrew, 430};
	const StrongId h1234{StrongLanguage::Hebrew, 1234};
	const StrongId g25{StrongLanguage::Greek, 25};
	const StrongId g2424{StrongLanguage::Greek, 2424};
	const StrongId g5547{StrongLanguage::Greek, 5547};
	const StrongId missing{StrongLanguage::Hebrew, 99999};
	g_assert_true(backend.moduleCapabilities(fixture.module).strongs);

	BibleReference reference;
	BibleVerseContent content = contentAt(backend, fixture.module,
		fixture.hebrewReference, reference);
	const BibleWordInfo *word = wordNamed(content, fixture.hebrewWord);
	g_assert_nonnull(word);
	g_assert_false(word->strongs.empty());
	g_assert_true(word->strongs.front() == h430);

	content = contentAt(backend, fixture.module, fixture.greekReference, reference);
	word = wordNamed(content, fixture.greekWord);
	g_assert_nonnull(word);
	g_assert_cmpuint(word->strongs.size(), ==, 1);
	g_assert_true(word->strongs[0] == g25);

	content = contentAt(backend, fixture.module, fixture.multipleReference, reference);
	word = wordNamed(content, fixture.multipleWord);
	g_assert_nonnull(word);
	g_assert_cmpuint(word->strongs.size(), ==, 2);
	g_assert_true(word->strongs[0] == g2424);
	g_assert_true(word->strongs[1] == g5547);

	content = contentAt(backend, fixture.module, fixture.noStrongReference, reference);
	word = wordNamed(content, fixture.noStrongWord);
	g_assert_nonnull(word);
	g_assert_true(word->strongs.empty());
	BibleAnnotatedWord context;
	g_assert_true(backend.resolveAnnotatedWord(fixture.module, reference,
		word->start, context));
	g_assert_cmpstr(context.word.c_str(), ==, fixture.noStrongWord.c_str());
	g_assert_true(context.strongs.empty());
	if (backend.moduleCapabilities(fixture.module).morphology) {
		g_assert_cmpuint(context.morphologyTags.size(), ==, 1);
		g_assert_true(context.morphologyTags[0] ==
			MorphologyTag({"", "HR/Ncfsa"}));
	} else {
		g_assert_true(context.morphologyTags.empty());
	}
	g_assert_cmpuint(context.start, ==, word->start);
	g_assert_cmpuint(context.length, ==, word->length);

	StrongOccurrencePage first = backend.findStrongOccurrencePage(
		fixture.module, g25, 1, 0);
	g_assert_cmpuint(first.occurrences.size(), ==, 1);
	g_assert_true(first.hasMore);
	g_assert_true(first.occurrences[0].strong == g25);
	g_assert_false(first.occurrences[0].key.empty());
	g_assert_false(first.occurrences[0].word.empty());
	g_assert_false(first.occurrences[0].context.empty());
	StrongOccurrencePage second = backend.findStrongOccurrencePage(
		fixture.module, g25, 1, 1);
	g_assert_cmpuint(second.occurrences.size(), ==, 1);
	g_assert_false(second.hasMore);
	const BibleReference &firstReference = first.occurrences[0].reference;
	const BibleReference &secondReference = second.occurrences[0].reference;
	g_assert_true(firstReference.book < secondReference.book ||
		(firstReference.book == secondReference.book &&
		 (firstReference.chapter < secondReference.chapter ||
		  (firstReference.chapter == secondReference.chapter &&
		   firstReference.verse < secondReference.verse))));
	g_assert_true(backend.findStrongOccurrencePage(
		fixture.module, g25, 0, 0).hasMore);
	g_assert_true(backend.findStrongOccurrencePage(
		fixture.module, missing, 100, 0).occurrences.empty());
	g_assert_cmpuint(backend.findStrongOccurrences(
		fixture.module, h430, 100, 0).size(), >=, 1);
	g_assert_cmpuint(backend.findStrongOccurrences(
		fixture.module, h1234, 100, 0).size(), <=, 1);

	BibleApplicationResources resources;
	resources.bible = &backend;
	g_assert_false(resources.capabilities().strongLexicon);
	g_assert_false(resources.lookupStrong(g25).valid);
	g_assert_cmpuint(backend.findStrongOccurrences(
		fixture.module, g25, 100, 0).size(), ==, 2);
	ContractLexicon lexicon;
	resources.strongLexicon = &lexicon;
	g_assert_true(resources.capabilities().strongLexicon);
	g_assert_true(resources.lookupStrong(h430).valid);
	g_assert_true(resources.lookupStrong(g25).valid);
	g_assert_false(resources.lookupStrong(missing).valid);
}
