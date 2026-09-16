/*
 * Biblia Elim - psalm titles numbered as their own verse.
 *
 * Vulgate-numbered editions (Torres Amat) count a psalm's superscription as
 * a verse: Vulg Ps 3:1 is «Salmo de David cuando temeroso…». Modules mark
 * that text <seg type="x-psalm-title">, which SWORD renders as
 * <span class="x-psalm-title">. The reference stays the native verse; only
 * the presentation changes. Detection is structural: the span's class,
 * never the words.
 */
#ifndef BIBLIA_ELIM_PSALM_TITLE_H
#define BIBLIA_ELIM_PSALM_TITLE_H

#include <string>

struct PsalmTitleParts {
	/* A leading x-psalm-title span was found. */
	bool hasTitle = false;
	/* Readable body text remains besides the title (Ps 52:1). */
	bool hasBody = false;
	/* The title element, markup included. Empty unless hasTitle. */
	std::string title;
	/* Everything else, in order; the whole input when !hasTitle. */
	std::string body;
};

/* Splits the rendered HTML of one verse. The title must come before any
 * readable body text; a span found later, or one that is not closed, is
 * left in place and reported as no title. */
PsalmTitleParts splitPsalmTitle(const std::string &html);

/* The verse text to draw after its (always shown) native number: the
 * title, and when the slot also holds body (Ps 52:1), a line break and
 * the body. An ordinary verse comes back unchanged. */
std::string psalmTitleVerseHtml(const PsalmTitleParts &parts);

#endif /* BIBLIA_ELIM_PSALM_TITLE_H */
