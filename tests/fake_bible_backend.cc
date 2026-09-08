#include "fake_bible_backend.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {
const char *const kBibleModule = "FakeBible";
const char *const kDictionaryModule = "FakeDictionary";

std::string lower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		       [](unsigned char c) { return std::tolower(c); });
	return value;
}

bool same_reference(const BibleReference &a, const BibleReference &b)
{
	return a.testament == b.testament && a.book == b.book &&
	       a.chapter == b.chapter && a.verse == b.verse;
}
}

FakeBibleBackend::FakeBibleBackend()
{
	auto add = [this](BibleReference reference, const char *key,
			  const char *text) {
		VerseRecord record;
		record.reference = reference;
		record.key = key;
		record.content.reference = reference;
		record.content.plainText = text;
		record.content.renderedText = text;
		record.content.valid = true;
		verses_.push_back(record);
	};
	add({ 1, 1, 1, 1 }, "Genesis 1:1", "In the beginning God created.");
	add({ 2, 4, 3, 16 }, "John 3:16", "For God so loved the world.");
	add({ 2, 4, 3, 17 }, "John 3:17", "God sent his Son to save the world.");

	BibleWordInfo hebrew;
	hebrew.start = 17;
	hebrew.length = 3;
	hebrew.text = "God";
	hebrew.strong = "H430";
	hebrew.strongs.push_back({ StrongLanguage::Hebrew, 430 });
	verses_[0].content.words.push_back(hebrew);

	BibleWordInfo word;
	word.start = 11;
	word.length = 5;
	word.text = "loved";
	word.lemma = "agapao";
	word.strong = "G25";
	word.strongs.push_back({ StrongLanguage::Greek, 25 });
	word.morphology = "V-AAI-3S";
	word.gloss = "love";
	verses_[1].content.words.push_back(word);
	BibleWordInfo withoutStrong;
	withoutStrong.start = 21;
	withoutStrong.length = 5;
	withoutStrong.text = "world";
	verses_[1].content.words.push_back(withoutStrong);
	BibleWordInfo secondGreek;
	secondGreek.start = 0;
	secondGreek.length = 3;
	secondGreek.text = "God";
	secondGreek.strong = "G25";
	secondGreek.strongs.push_back({ StrongLanguage::Greek, 25 });
	verses_[2].content.words.push_back(secondGreek);
	BibleWordInfo multiple;
	multiple.start = 13;
	multiple.length = 3;
	multiple.text = "Son";
	multiple.strong = "G2424,G5547";
	multiple.strongs.push_back({ StrongLanguage::Greek, 2424 });
	multiple.strongs.push_back({ StrongLanguage::Greek, 5547 });
	verses_[2].content.words.push_back(multiple);
	dictionary_["G25"] = "to love";
}

StrongOccurrencePage FakeBibleBackend::findStrongOccurrencePage(
	const std::string &id, const StrongId &strong, std::size_t limit,
	std::size_t offset)
{
	std::vector<StrongOccurrence> all;
	StrongOccurrencePage page;
	if (id != kBibleModule) return page;
	for (const auto &record : verses_) for (const auto &word : record.content.words)
		for (const auto &candidate : word.strongs)
			if (candidate.language == strong.language && candidate.number == strong.number)
				all.push_back({record.reference, record.key, word.text,
					record.content.plainText, strong});
	if (offset >= all.size()) return page;
	const auto first = all.begin() + offset;
	const std::size_t available = all.size() - offset;
	const std::size_t count = std::min(limit, available);
	page.occurrences.assign(first, first + count);
	page.hasMore = available > count;
	return page;
}

std::vector<BibleModuleInfo> FakeBibleBackend::listModules() const
{
	return { { kBibleModule, "In-memory test Bible", "en",
		   BibleModuleType::Bible },
		 { kDictionaryModule, "In-memory test dictionary", "en",
		   BibleModuleType::Dictionary } };
}

bool FakeBibleBackend::hasModule(const std::string &id) const
{
	return id == kBibleModule || id == kDictionaryModule;
}

BibleModuleType FakeBibleBackend::moduleType(const std::string &id) const
{
	if (id == kBibleModule) return BibleModuleType::Bible;
	if (id == kDictionaryModule) return BibleModuleType::Dictionary;
	return BibleModuleType::Unknown;
}

BibleModuleCapabilities
FakeBibleBackend::moduleCapabilities(const std::string &id) const
{
	BibleModuleCapabilities result;
	if (id == kBibleModule) {
		result.verses = true;
		result.search = true;
		result.strongs = true;
		result.morphology = true;
	} else if (id == kDictionaryModule) {
		result.dictionaryLookup = true;
	}
	return result;
}

std::string FakeBibleBackend::moduleDescription(const std::string &id) const
{
	for (const auto &module : listModules())
		if (module.id == id) return module.description;
	return {};
}

std::string FakeBibleBackend::moduleLanguage(const std::string &id) const
{
	return hasModule(id) ? "en" : std::string();
}

bool FakeBibleBackend::resolveKey(const std::string &id, const std::string &key,
				  BibleKeyInfo &result)
{
	if (id != kBibleModule) return false;
	for (const auto &record : verses_) {
		if (record.key != key) continue;
		result = BibleKeyInfo();
		result.reference = record.reference;
		result.key = record.key;
		result.bookName = record.reference.testament == 1 ? "Genesis" : "John";
		result.bookIndex = record.reference.book;
		result.chapterCount = record.reference.testament == 1 ? 50 : 21;
		result.verseCount = record.reference.chapter == 3 ? 17 : 1;
		return true;
	}
	return false;
}

std::string FakeBibleBackend::osisRefFromKey(const std::string &id,
					      const std::string &key)
{
	BibleKeyInfo info;
	if (!resolveKey(id, key, info)) return {};
	return (info.reference.testament == 1 ? "Gen." : "John.") +
	       std::to_string(info.reference.chapter) + "." +
	       std::to_string(info.reference.verse);
}

std::vector<BibleVerse> FakeBibleBackend::getChapter(
	const std::string &id, const BibleReference &reference, bool rendered)
{
	std::vector<BibleVerse> result;
	if (id != kBibleModule) return result;
	for (const auto &record : verses_) {
		if (record.reference.testament != reference.testament ||
		    record.reference.book != reference.book ||
		    record.reference.chapter != reference.chapter) continue;
		BibleVerse verse;
		verse.reference = record.reference;
		verse.key = record.key;
		verse.text = rendered ? record.content.renderedText : record.content.plainText;
		verse.osisRef = osisRefFromKey(id, record.key);
		result.push_back(verse);
	}
	return result;
}

BibleVerseContent FakeBibleBackend::getVerseContent(
	const std::string &id, const BibleReference &reference, bool)
{
	if (id == kBibleModule)
		for (const auto &record : verses_)
			if (same_reference(record.reference, reference)) return record.content;
	return {};
}

DictionaryEntry FakeBibleBackend::lookupDictionary(const std::string &id,
						     const std::string &key)
{
	DictionaryEntry result;
	if (id != kDictionaryModule) return result;
	auto entry = dictionary_.find(key);
	if (entry == dictionary_.end()) return result;
	result.key = entry->first;
	result.text = entry->second;
	result.valid = true;
	return result;
}

std::string FakeBibleBackend::navigate(const std::string &id,
				       const std::string &key, int direction)
{
	if (id != kBibleModule) return {};
	for (std::size_t i = 0; i < verses_.size(); ++i) {
		if (verses_[i].key != key) continue;
		long target = static_cast<long>(i) + (direction < 0 ? -1 : 1);
		return target >= 0 && target < static_cast<long>(verses_.size())
			       ? verses_[target].key : key;
	}
	return {};
}

std::string FakeBibleBackend::setChapter(const std::string &id,
					  const std::string &key, int chapter)
{
	BibleKeyInfo info;
	if (!resolveKey(id, key, info)) return {};
	info.reference.chapter = chapter;
	info.reference.verse = 1;
	for (const auto &record : verses_)
		if (same_reference(record.reference, info.reference)) return record.key;
	return {};
}

std::string FakeBibleBackend::setVerse(const std::string &id,
					const std::string &key, int verse)
{
	BibleKeyInfo info;
	if (!resolveKey(id, key, info)) return {};
	info.reference.verse = verse;
	for (const auto &record : verses_)
		if (same_reference(record.reference, info.reference)) return record.key;
	return {};
}

std::string FakeBibleBackend::setBook(const std::string &id,
				       const std::string &, int testament, int book)
{
	if (id != kBibleModule) return {};
	for (const auto &record : verses_)
		if (record.reference.testament == testament && record.reference.book == book)
			return record.key;
	return {};
}

std::vector<std::string> FakeBibleBackend::bookNames(const std::string &id,
						      int testament) const
{
	if (id != kBibleModule) return {};
	if (testament == 1) return { "Genesis" };
	if (testament == 2) return { "John" };
	return {};
}

std::vector<BibleSearchResult> FakeBibleBackend::search(
	const std::string &id, const BibleSearchQuery &query)
{
	std::vector<BibleSearchResult> result;
	if (id != kBibleModule || query.text.empty()) return result;
	std::string needle = query.caseSensitive ? query.text : lower(query.text);
	for (const auto &record : verses_) {
		std::string haystack = query.caseSensitive ? record.content.plainText
						       : lower(record.content.plainText);
		if (haystack.find(needle) == std::string::npos) continue;
		BibleSearchResult item;
		item.module = id;
		item.key = record.key;
		item.reference = record.reference;
		item.text = record.content.plainText;
		item.osisRef = osisRefFromKey(id, record.key);
		result.push_back(item);
	}
	if (query.offset >= result.size()) return {};
	const auto first = result.begin() + query.offset;
	const auto last = query.limit >= result.size() - query.offset
		? result.end() : first + query.limit;
	return std::vector<BibleSearchResult>(first, last);
}
