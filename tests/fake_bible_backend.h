#ifndef XIPHOS_TESTS_FAKE_BIBLE_BACKEND_H
#define XIPHOS_TESTS_FAKE_BIBLE_BACKEND_H

#include <map>

#include "backend/bible_backend.h"

/* Small deterministic implementation used to exercise the common contract.
 * It deliberately has no SWORD includes and performs no filesystem access. */
class FakeBibleBackend final : public BibleBackend
{
public:
	FakeBibleBackend();

	std::vector<BibleModuleInfo> listModules() const override;
	bool hasModule(const std::string &module_id) const override;
	BibleModuleType moduleType(const std::string &module_id) const override;
	BibleModuleCapabilities moduleCapabilities(
		const std::string &module_id) const override;
	std::string moduleDescription(const std::string &module_id) const override;
	std::string moduleLanguage(const std::string &module_id) const override;
	std::string osisRefFromKey(const std::string &module_id,
				  const std::string &key) override;
	bool resolveKey(const std::string &module_id, const std::string &key,
			BibleKeyInfo &result) override;
	std::vector<BibleVerse> getChapter(const std::string &module_id,
					   const BibleReference &reference,
					   bool rendered) override;
	BibleVerseContent getVerseContent(const std::string &module_id,
					 const BibleReference &reference,
					 bool include_plain_text = false) override;
	DictionaryEntry lookupDictionary(const std::string &module_id,
					 const std::string &key) override;
	std::string navigate(const std::string &module_id, const std::string &key,
			     int direction) override;
	std::string setChapter(const std::string &module_id, const std::string &key,
			       int chapter) override;
	std::string setVerse(const std::string &module_id, const std::string &key,
			     int verse) override;
	std::string setBook(const std::string &module_id, const std::string &key,
			    int testament, int book) override;
	std::vector<std::string> bookNames(const std::string &module_id,
					   int testament) const override;
	std::vector<BibleSearchResult> search(const std::string &module_id,
					      const BibleSearchQuery &query) override;
	StrongOccurrencePage findStrongOccurrencePage(const std::string &,
		const StrongId &, std::size_t, std::size_t) override;
	MorphologyOccurrencePage findMorphologyOccurrencePage(const std::string &,
		const MorphologyTag &, std::size_t, std::size_t) override;

private:
	struct VerseRecord {
		BibleReference reference;
		std::string key;
		BibleVerseContent content;
	};
	std::vector<VerseRecord> verses_;
	std::map<std::string, std::string> dictionary_;
};

#endif
