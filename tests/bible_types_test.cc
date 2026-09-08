/* Tests for the backend-neutral enriched-content value types. */
#include <glib.h>

#include <utility>

#include "backend/bible_types.h"

static void verse_content_keeps_enriched_data()
{
	BibleVerseContent content;
	content.reference.testament = 2;
	content.reference.book = 4;
	content.reference.chapter = 3;
	content.reference.verse = 16;
	content.plainText = "texto";
	content.renderedText = "<span>texto</span>";
	content.valid = true;
	content.footnotesHaveNumbers = true;

	BibleWordInfo word;
	word.text = "logos";
	word.lemma = "logos";
	word.strong = "G3056";
	word.morphology = "robinson:N-NSM";
	word.gloss = "palabra";
	content.words.push_back(word);
	BibleHeading heading;
	heading.text = "<h3>Encabezado</h3>";
	content.headings.push_back(heading);

	BibleVerseContent moved = std::move(content);
	g_assert_true(moved.valid);
	g_assert_cmpstr(moved.words[0].strong.c_str(), ==, "G3056");
	g_assert_cmpstr(moved.words[0].morphology.c_str(), ==,
			"robinson:N-NSM");
	g_assert_cmpstr(moved.headings[0].text.c_str(), ==,
			"<h3>Encabezado</h3>");
}

static void content_without_attributes_is_empty()
{
	BibleVerseContent content;
	g_assert_false(content.valid);
	g_assert_false(content.footnotesHaveNumbers);
	g_assert_true(content.words.empty());
	g_assert_true(content.headings.empty());
}

static void invalid_reference_stays_explicit()
{
	BibleReference reference;
	g_assert_cmpint(reference.testament, ==, 0);
	g_assert_cmpint(reference.book, ==, 0);
	g_assert_cmpint(reference.chapter, ==, 0);
	g_assert_cmpint(reference.verse, ==, 0);
}

static void dictionary_entry_has_explicit_validity()
{
	DictionaryEntry entry;
	g_assert_false(entry.valid);
	entry.key = "G3056";
	entry.text = "palabra";
	entry.valid = true;
	g_assert_true(entry.valid);
	g_assert_cmpstr(entry.key.c_str(), ==, "G3056");
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/backend/verse-content/enriched",
			verse_content_keeps_enriched_data);
	g_test_add_func("/backend/verse-content/no-attributes",
			content_without_attributes_is_empty);
	g_test_add_func("/backend/reference/invalid-default",
			invalid_reference_stays_explicit);
	g_test_add_func("/backend/dictionary/validity",
			dictionary_entry_has_explicit_validity);
	return g_test_run();
}
