#ifndef XIPHOS_BIBLE_BOOK_MAP_H
#define XIPHOS_BIBLE_BOOK_MAP_H

#include <string>
#include <vector>

struct BibleBookDefinition {
	int bookId;
	const char *usfm;
	const char *osis;
	const char *name;
	const char *shortName;
	int testament;
	int position;
};

const BibleBookDefinition *findBibleBookByUsfm(const std::string &code);
/* The 66 books, then the deuterocanonical ones (ids 67-73); a book's id
 * is its index + 1. */
const std::vector<BibleBookDefinition> &canonicalBibleBooks();
/* The book's name in `language` ("es", "es-ES", "spa": Spanish, the
 * spelling SWORD's Spanish locale uses); English otherwise. */
const char *bibleBookName(const BibleBookDefinition &book,
			  const std::string &language);
/* A book by its OSIS id, English, short or Spanish name, in any case:
 * «Luke», «LUKE», «Lucas», «GÉNESIS». */
const BibleBookDefinition *findBibleBookByAnyName(const std::string &name);
/* The versification a module declares, as stored in its metadata
 * (lowercase: "kjv", "vulg", "nrsva", ...), by its SWORD name ("KJV",
 * "Vulg", "NRSVA"). "custom" is read as KJV, as before. nullptr for a
 * value no versification is known by. */
const char *versificationSystemName(const std::string &stored);

#endif
