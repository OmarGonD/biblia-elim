#ifndef XIPHOS_SQLITE_STRONG_LEXICON_H
#define XIPHOS_SQLITE_STRONG_LEXICON_H
#include <memory>
#include "backend/bible_lexicon.h"
class SqliteStrongLexicon final : public BibleLexicon {
public:
	explicit SqliteStrongLexicon(const std::string &path);
	~SqliteStrongLexicon() override;
	LexiconEntry lookupStrong(const StrongId &) const override;
private: struct Impl; std::unique_ptr<Impl> impl_;
};
#endif
