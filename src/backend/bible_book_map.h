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
const std::vector<BibleBookDefinition> &canonicalBibleBooks();

#endif
