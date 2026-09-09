/* Minimal SQLite implementation of the format-neutral Bible backend. */
#ifndef XIPHOS_SQLITE_BIBLE_BACKEND_H
#define XIPHOS_SQLITE_BIBLE_BACKEND_H

#include <memory>
#include <string>

#include "backend/bible_backend.h"

class SqliteBibleBackend final : public BibleBackend
{
public:
	explicit SqliteBibleBackend(const std::string &modules_directory);
	~SqliteBibleBackend() override;

	std::vector<BibleModuleInfo> listModules() const override;
	bool hasModule(const std::string &) const override;
	BibleModuleType moduleType(const std::string &) const override;
	BibleModuleCapabilities moduleCapabilities(const std::string &) const override;
	std::string moduleDescription(const std::string &) const override;
	std::string moduleLanguage(const std::string &) const override;
	std::string osisRefFromKey(const std::string &, const std::string &) override;
	bool resolveKey(const std::string &, const std::string &,
			BibleKeyInfo &) override;
	std::vector<BibleVerse> getChapter(const std::string &,
					   const BibleReference &, bool) override;
	BibleVerseContent getVerseContent(const std::string &,
					 const BibleReference &,
					 bool include_plain_text = false) override;
	DictionaryEntry lookupDictionary(const std::string &,
					 const std::string &) override;
	std::string navigate(const std::string &, const std::string &, int) override;
	std::string setChapter(const std::string &, const std::string &, int) override;
	std::string setVerse(const std::string &, const std::string &, int) override;
	std::string setBook(const std::string &, const std::string &,
			    int, int) override;
	std::vector<std::string> bookNames(const std::string &, int) const override;
	std::vector<BibleSearchResult> search(const std::string &,
					      const BibleSearchQuery &) override;
	StrongOccurrencePage findStrongOccurrencePage(const std::string &,
					const StrongId &, std::size_t, std::size_t) override;
	MorphologyOccurrencePage findMorphologyOccurrencePage(const std::string &,
					const MorphologyTag &, std::size_t, std::size_t) override;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

#endif
