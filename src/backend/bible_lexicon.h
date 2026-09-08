#ifndef XIPHOS_BIBLE_LEXICON_H
#define XIPHOS_BIBLE_LEXICON_H

#include "backend/bible_types.h"

struct LexiconEntry {
	StrongId id;
	std::string lemma;
	std::string transliteration;
	std::string pronunciation;
	std::string definition;
	bool valid = false;
};

class BibleLexicon
{
public:
	virtual ~BibleLexicon() = default;
	virtual LexiconEntry lookupStrong(const StrongId &) const = 0;
	bool contains(const StrongId &id) const { return lookupStrong(id).valid; }
};

#endif /* XIPHOS_BIBLE_LEXICON_H */
