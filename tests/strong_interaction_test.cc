#include <glib.h>

#include "fake_bible_backend.h"
#include "main/strong_interaction.h"

namespace {
class TestLexicon final : public BibleLexicon
{
public:
	LexiconEntry lookupStrong(const StrongId &id) const override
	{
		LexiconEntry result;
		result.id = id;
		if (id == StrongId{StrongLanguage::Greek, 25}) {
			result.lemma = "agapao";
			result.transliteration = "agapaō";
			result.pronunciation = "ag-ap-ah-o";
			result.definition = "to love";
			result.valid = true;
		}
		return result;
	}
};

BibleReference referenceFor(FakeBibleBackend &backend, const char *key)
{
	BibleKeyInfo info;
	g_assert_true(backend.resolveKey("FakeBible", key, info));
	return info.reference;
}

void testResolution()
{
	FakeBibleBackend backend;
	StrongWordResolution none = resolveStrongInteraction(backend,
		"FakeDictionary", {}, 0);
	g_assert_true(none.action == StrongWordAction::None);

	const BibleReference john316 = referenceFor(backend, "John 3:16");
	StrongWordResolution single = resolveStrongInteraction(backend,
		"FakeBible", john316, 11);
	g_assert_true(single.action == StrongWordAction::OpenDetail);
	g_assert_cmpstr(single.context.word.c_str(), ==, "loved");
	g_assert_cmpuint(single.context.strongs.size(), ==, 1);

	StrongWordResolution noStrong = resolveStrongInteraction(backend,
		"FakeBible", john316, 21);
	g_assert_true(noStrong.action == StrongWordAction::None);

	const BibleReference john317 = referenceFor(backend, "John 3:17");
	StrongWordResolution multiple = resolveStrongInteraction(backend,
		"FakeBible", john317, 13);
	g_assert_true(multiple.action == StrongWordAction::ChooseStrong);
	g_assert_cmpuint(multiple.context.strongs.size(), ==, 2);
	const StrongId g2424{StrongLanguage::Greek, 2424};
	const StrongId g5547{StrongLanguage::Greek, 5547};
	g_assert_true(multiple.context.strongs[0] == g2424);
	g_assert_true(multiple.context.strongs[1] == g5547);
}

void testMarkupKeepsUtf8ByteOffsets()
{
	BibleVerseContent content;
	content.plainText = "Él amó";
	content.valid = true;
	BibleWordInfo word;
	word.start = 4; /* Él is three UTF-8 bytes, then one space. */
	word.length = 3;
	word.text = "amó";
	word.strongs.push_back({StrongLanguage::Greek, 25});
	content.words.push_back(word);
	const std::string markup = renderStrongVerseText(content, "RV 1909",
		"Juan 3:16", true);
	g_assert_nonnull(strstr(markup.c_str(), "data-offset=\"4\""));
	g_assert_nonnull(strstr(markup.c_str(), "module=RV%201909"));
	g_assert_nonnull(strstr(markup.c_str(), "passage=Juan%203%3A16"));
	g_assert_null(strstr(markup.c_str(), "G25"));
	g_assert_cmpstr(renderStrongVerseText(content, "RV 1909", "Juan 3:16",
		false).c_str(), ==, "Él amó");
}

void testDetailAndPagination()
{
	FakeBibleBackend backend;
	BibleApplicationResources resources;
	resources.bible = &backend;
	StrongWordResolution resolution = resolveStrongInteraction(backend,
		"FakeBible", referenceFor(backend, "John 3:16"), 11);
	StrongDetailSession withoutLexicon(backend, resources, "FakeBible",
		resolution.context, 1);
	g_assert_true(withoutLexicon.selectStrong(
		{StrongLanguage::Greek, 25}));
	g_assert_false(withoutLexicon.state().lexicon.valid);
	g_assert_cmpuint(withoutLexicon.state().occurrences.size(), ==, 1);
	g_assert_true(withoutLexicon.state().hasMore);
	g_assert_cmpstr(withoutLexicon.state().occurrences[0].key.c_str(), ==,
		"John 3:16");
	g_assert_true(withoutLexicon.loadMore());
	g_assert_cmpuint(withoutLexicon.state().occurrences.size(), ==, 2);
	g_assert_false(withoutLexicon.state().hasMore);
	g_assert_cmpstr(withoutLexicon.state().occurrences[1].key.c_str(), ==,
		"John 3:17");
	g_assert_false(withoutLexicon.loadMore());

	TestLexicon lexicon;
	resources.strongLexicon = &lexicon;
	StrongDetailSession withLexicon(backend, resources, "FakeBible",
		resolution.context, 50);
	g_assert_true(withLexicon.selectStrong({StrongLanguage::Greek, 25}));
	g_assert_true(withLexicon.state().lexicon.valid);
	g_assert_cmpstr(withLexicon.state().lexicon.lemma.c_str(), ==, "agapao");
	g_assert_false(withLexicon.selectStrong(
		{StrongLanguage::Hebrew, 430}));
}
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/strong-ui/resolution", testResolution);
	g_test_add_func("/strong-ui/utf8-markup", testMarkupKeepsUtf8ByteOffsets);
	g_test_add_func("/strong-ui/detail-pagination", testDetailAndPagination);
	return g_test_run();
}
