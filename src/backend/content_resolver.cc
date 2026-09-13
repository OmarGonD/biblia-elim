#include "backend/content_resolver.h"

#include <map>
#include <string>
#include <vector>

#include <versificationmgr.h>

namespace {

struct BookPresenceKey {
	std::string module;
	int testament = 0;
	int book = 0;

	bool operator<(const BookPresenceKey &other) const
	{
		if (module != other.module)
			return module < other.module;
		if (testament != other.testament)
			return testament < other.testament;
		return book < other.book;
	}
};

std::map<BookPresenceKey, bool> g_book_presence;

void
stamp(BibleVerseContent &content, const std::string &requested,
      const std::string &source, bool fallback,
      const std::string &heading_source)
{
	content.requestedModuleId = requested;
	content.sourceModuleId = source;
	content.headingSourceModuleId = heading_source;
	content.isFallback = fallback;
}

void
stampOriginal(BibleVerseContent &content, const std::string &module_id)
{
	stamp(content, module_id, module_id, false, module_id);
}

std::string
verseKeyFor(BibleBackend &backend, const std::string &module_id,
	    const BibleReference &reference)
{
	const std::vector<std::string> names =
		backend.bookNames(module_id, reference.testament);
	if (reference.book < 1 ||
	    (std::size_t)reference.book > names.size())
		return std::string();
	return names[(std::size_t)reference.book - 1] + " " +
	       std::to_string(reference.chapter) + ":" +
	       std::to_string(reference.verse);
}

bool
chapterHasUsableVerse(BibleBackend &backend, const std::string &module_id,
		      const BibleReference &book_ref, int chapter)
{
	if (chapter < 1)
		return false;
	BibleReference probe = book_ref;
	probe.chapter = chapter;
	probe.verse = 1;
	const std::vector<BibleVerse> verses =
		backend.getChapter(module_id, probe, false);
	for (std::size_t i = 0; i < verses.size(); i++) {
		if (hasUsableVerseBody(verses[i].text))
			return true;
	}
	return false;
}

/* True when the selected module actually contains this book, not merely
 * an empty versification slot (NT-only modules listing Genesis). A book
 * is in-corpus if any chapter we probe has usable body. */
bool
moduleContainsBookUncached(BibleBackend &backend, const std::string &module_id,
			   const BibleReference &reference)
{
	const std::string key = verseKeyFor(backend, module_id, reference);
	BibleKeyInfo info;
	int chapters = 1;
	if (backend.resolveKey(module_id, key, info) && info.chapterCount > 0)
		chapters = info.chapterCount;
	if (chapterHasUsableVerse(backend, module_id, reference,
				  reference.chapter))
		return true;
	if (reference.chapter != 1 &&
	    chapterHasUsableVerse(backend, module_id, reference, 1))
		return true;
	if (reference.chapter != 2 &&
	    chapterHasUsableVerse(backend, module_id, reference, 2))
		return true;
	if (chapters > 2 && reference.chapter != chapters &&
	    chapterHasUsableVerse(backend, module_id, reference, chapters))
		return true;
	return false;
}

bool
moduleContainsBook(BibleBackend &backend, const std::string &module_id,
		   const BibleReference &reference)
{
	BookPresenceKey key;
	key.module = module_id;
	key.testament = reference.testament;
	key.book = reference.book;
	std::map<BookPresenceKey, bool>::iterator it = g_book_presence.find(key);
	if (it != g_book_presence.end())
		return it->second;
	const bool present =
		moduleContainsBookUncached(backend, module_id, reference);
	g_book_presence[key] = present;
	return present;
}

bool
sameOsisBook(const BibleKeyInfo &requested, const BibleKeyInfo &fallback)
{
	if (!requested.osisBook.empty() && !fallback.osisBook.empty())
		return requested.osisBook == fallback.osisBook;
	if (!requested.bookName.empty() && !fallback.bookName.empty())
		return requested.bookName == fallback.bookName;
	return false;
}

bool
shouldAttemptFallback(BibleBackend &backend, const std::string &module_id,
		      const FallbackPolicy &policy)
{
	if (policy.fallbackModuleId.empty())
		return false;
	if (policy.fallbackModuleId == module_id)
		return false;
	if (backend.moduleType(module_id) != BibleModuleType::Bible)
		return false;
	if (!backend.hasModule(policy.fallbackModuleId))
		return false;
	if (backend.moduleType(policy.fallbackModuleId) !=
	    BibleModuleType::Bible)
		return false;
	return true;
}

bool
bookIdentityHolds(const std::string &asked_key, const BibleKeyInfo &requested,
		  const BibleKeyInfo &mapped)
{
	if (sameOsisBook(requested, mapped))
		return true;
	if (!mapped.osisBook.empty() &&
	    asked_key.compare(0, mapped.osisBook.size(), mapped.osisBook) == 0)
		return true;
	if (!mapped.bookName.empty() &&
	    asked_key.compare(0, mapped.bookName.size(), mapped.bookName) == 0)
		return true;
	return false;
}

/* Single allow-list for cross-v11n fallback. Same-system is always
 * VerifiedIdentity. Add a row here to enable a new pair. */
const char *const kVerifiedIdentity[][2] = {
	{ "NRSVA", "KJV" },
	{ "NRSVA", "KJVA" },
	{ "KJVA", "KJV" },
	{ "KJV", "KJVA" },
};

const char *const kSwordMapped[][2] = {
	{ "Vulg", "KJV" },
	{ "Vulg", "KJVA" },
};

bool
pairListed(const char *const table[][2], unsigned count, const std::string &from,
	   const std::string &to)
{
	for (unsigned i = 0; i < count; i++) {
		if (from == table[i][0] && to == table[i][1])
			return true;
	}
	return false;
}

} // namespace

FallbackMappingStatus
classifyFallbackVersification(const std::string &source_v11n,
			      const std::string &fallback_v11n)
{
	const std::string from = source_v11n.empty() ? "KJV" : source_v11n;
	const std::string to = fallback_v11n.empty() ? "KJV" : fallback_v11n;
	if (from == to)
		return FallbackMappingStatus::VerifiedIdentity;
	if (pairListed(kVerifiedIdentity,
		       sizeof(kVerifiedIdentity) / sizeof(kVerifiedIdentity[0]),
		       from, to))
		return FallbackMappingStatus::VerifiedIdentity;
	if (pairListed(kSwordMapped,
		       sizeof(kSwordMapped) / sizeof(kSwordMapped[0]), from,
		       to))
		return FallbackMappingStatus::Mapped;
	return FallbackMappingStatus::Unsupported;
}

namespace {

bool
translateCitation(const std::string &src_v11n, const std::string &dst_v11n,
		  std::string &osis_book, int &chapter, int &verse)
{
	const FallbackMappingStatus status =
		classifyFallbackVersification(src_v11n, dst_v11n);
	if (status == FallbackMappingStatus::Unsupported)
		return false;
	if (status == FallbackMappingStatus::VerifiedIdentity)
		return true;
	if (osis_book.empty() || chapter < 1 || verse < 1)
		return false;
	sword::VersificationMgr *mgr =
		sword::VersificationMgr::getSystemVersificationMgr();
	const sword::VersificationMgr::System *src =
		mgr->getVersificationSystem(src_v11n.c_str());
	const sword::VersificationMgr::System *dst =
		mgr->getVersificationSystem(dst_v11n.c_str());
	if (!src || !dst)
		return false;
	if (src->getBookNumberByOSISName(osis_book.c_str()) < 1)
		return false;
	const char *book = osis_book.c_str();
	int ch = chapter;
	int vs = verse;
	int ve = verse;
	src->translateVerse(dst, &book, &ch, &vs, &ve);
	if (!book || ch < 1 || vs < 1)
		return false;
	osis_book = book;
	chapter = ch;
	verse = vs;
	return true;
}

bool
mapFallbackKey(BibleBackend &backend, const std::string &requested,
	       const std::string &fallback, const BibleReference &reference,
	       BibleKeyInfo &mapped)
{
	const std::string key = verseKeyFor(backend, requested, reference);
	if (key.empty())
		return false;

	BibleKeyInfo requested_info;
	const bool have_requested = backend.resolveKey(requested, key,
						       requested_info);

	std::string osis_book = have_requested ? requested_info.osisBook
					       : std::string();
	int chapter = reference.chapter;
	int verse = reference.verse;
	if (!translateCitation(backend.versification(requested),
			       backend.versification(fallback), osis_book,
			       chapter, verse))
		return false;

	const bool numbers_unchanged = chapter == reference.chapter &&
				       verse == reference.verse;

	bool have_mapped = false;
	if (numbers_unchanged)
		have_mapped = backend.resolveKey(fallback, key, mapped);
	if (!have_mapped && !osis_book.empty()) {
		const std::string osis_key = osis_book + " " +
					     std::to_string(chapter) + ":" +
					     std::to_string(verse);
		have_mapped = backend.resolveKey(fallback, osis_key, mapped);
	}
	if (!have_mapped && have_requested &&
	    !requested_info.bookName.empty() &&
	    (osis_book.empty() || osis_book == requested_info.osisBook)) {
		const std::string name_key = requested_info.bookName + " " +
					     std::to_string(chapter) + ":" +
					     std::to_string(verse);
		have_mapped = backend.resolveKey(fallback, name_key, mapped);
	}
	if (!have_mapped)
		return false;
	if (mapped.reference.chapter != chapter ||
	    mapped.reference.verse != verse)
		return false;
	if (!osis_book.empty() && !mapped.osisBook.empty())
		return mapped.osisBook == osis_book;
	if (have_requested)
		return bookIdentityHolds(key, requested_info, mapped);
	if (!mapped.osisBook.empty() &&
	    key.compare(0, mapped.osisBook.size(), mapped.osisBook) == 0)
		return true;
	if (!mapped.bookName.empty() &&
	    key.compare(0, mapped.bookName.size(), mapped.bookName) == 0)
		return true;
	return false;
}

BibleVerseContent
fetchFallback(BibleBackend &backend, const std::string &requested,
	      const std::string &fallback, const BibleReference &reference,
	      bool include_plain_text, const BibleVerseContent &original)
{
	BibleVerseContent failed = original;
	stampOriginal(failed, requested);
	if (fallback.empty() || fallback == requested)
		return failed;

	BibleKeyInfo mapped;
	if (!mapFallbackKey(backend, requested, fallback, reference, mapped))
		return failed;
	if (mapped.reference.chapter <= 0 || mapped.reference.verse <= 0)
		return failed;

	BibleVerseContent supplied = backend.getVerseContent(
		fallback, mapped.reference, include_plain_text);
	if (classifyVerseContent(supplied, mapped.reference) !=
	    ContentAvailability::Available)
		return failed;

	std::string heading_source = fallback;
	if (!original.headings.empty()) {
		supplied.headings = original.headings;
		heading_source = requested;
	}
	stamp(supplied, requested, fallback, true, heading_source);
	supplied.reference = reference;
	return supplied;
}

} // namespace

FallbackPolicy
defaultFallbackPolicy()
{
	FallbackPolicy policy;
	policy.fallbackModuleId = "SpaRV1909";
	policy.fallbackDisplayName = "Reina-Valera 1909";
	return policy;
}

void
resetContentResolverCache()
{
	g_book_presence.clear();
}

std::string
missingContentFallbackNotice(const BibleVerseContent &content,
			     const FallbackPolicy &policy)
{
	if (!content.isFallback)
		return std::string();
	std::string name = policy.fallbackDisplayName;
	if (name.empty() ||
	    (!content.sourceModuleId.empty() &&
	     content.sourceModuleId != policy.fallbackModuleId))
		name = content.sourceModuleId;
	if (name.empty())
		return std::string();
	return "Texto suplido desde " + name;
}

BibleVerseContent
resolveVerseContent(BibleBackend &backend, const std::string &module_id,
		    const BibleReference &reference, bool include_plain_text,
		    const FallbackPolicy &policy)
{
	BibleVerseContent original =
		backend.getVerseContent(module_id, reference, include_plain_text);
	splitIntroductoryMaterial(original);
	stampOriginal(original, module_id);

	const ContentAvailability availability =
		classifyVerseContent(original, reference);
	if (availability != ContentAvailability::Missing)
		return original;
	if (!shouldAttemptFallback(backend, module_id, policy))
		return original;
	if (!moduleContainsBook(backend, module_id, reference))
		return original;
	return fetchFallback(backend, module_id, policy.fallbackModuleId,
			     reference, include_plain_text, original);
}

std::vector<BibleVerseContent>
resolveChapterContent(BibleBackend &backend, const std::string &module_id,
		      const BibleReference &chapter_ref, int verse_count,
		      bool include_plain_text, const FallbackPolicy &policy)
{
	std::vector<BibleVerseContent> resolved;
	if (verse_count < 1)
		return resolved;

	bool any_usable = false;
	const std::vector<BibleVerse> chapter =
		backend.getChapter(module_id, chapter_ref, false);
	for (std::size_t i = 0; i < chapter.size(); i++) {
		if (hasUsableVerseBody(chapter[i].text)) {
			any_usable = true;
			break;
		}
	}

	const bool try_bulk =
		!any_usable &&
		shouldAttemptFallback(backend, module_id, policy) &&
		moduleContainsBook(backend, module_id, chapter_ref);

	resolved.reserve((std::size_t)verse_count);
	for (int verse = 1; verse <= verse_count; verse++) {
		BibleReference reference = chapter_ref;
		reference.verse = verse;
		if (!try_bulk) {
			resolved.push_back(resolveVerseContent(
				backend, module_id, reference,
				include_plain_text, policy));
			continue;
		}
		BibleVerseContent original = backend.getVerseContent(
			module_id, reference, include_plain_text);
		splitIntroductoryMaterial(original);
		stampOriginal(original, module_id);
		if (classifyVerseContent(original, reference) !=
		    ContentAvailability::Missing) {
			resolved.push_back(original);
			continue;
		}
		resolved.push_back(fetchFallback(
			backend, module_id, policy.fallbackModuleId, reference,
			include_plain_text, original));
	}
	return resolved;
}
