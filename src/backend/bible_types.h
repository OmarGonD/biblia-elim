/*
 * Neutral data types shared by the application and Bible backends.
 *
 * This header deliberately does not include any SWORD headers.  A backend
 * adapter is responsible for translating its native module metadata into
 * these small value types.
 */
#ifndef XIPHOS_BIBLE_TYPES_H
#define XIPHOS_BIBLE_TYPES_H

#include <string>
#include <vector>

enum class StrongLanguage { Hebrew, Greek };
struct StrongId { StrongLanguage language = StrongLanguage::Hebrew; int number = 0; };

inline bool operator==(const StrongId &left, const StrongId &right)
{
	return left.language == right.language && left.number == right.number;
}

inline bool operator!=(const StrongId &left, const StrongId &right)
{
	return !(left == right);
}

/* Morphology identifiers are deliberately opaque. A scheme names the grammar
 * system when the source supplies one; code is interpreted only by that
 * system. An empty scheme represents a valid unqualified source value. */
struct MorphologyTag {
	std::string scheme;
	std::string code;
};

inline bool operator==(const MorphologyTag &left, const MorphologyTag &right)
{
	return left.scheme == right.scheme && left.code == right.code;
}

inline bool operator!=(const MorphologyTag &left, const MorphologyTag &right)
{
	return !(left == right);
}

struct BibleReference {
	int testament = 0;
	int book = 0;
	int chapter = 0;
	int verse = 0;
};

enum class BibleModuleType {
	Bible,
	Commentary,
	Dictionary,
	Lexicon,
	GeneralBook,
	PersonalCommentary,
	PrayerList,
	Unknown
};

struct BibleModuleCapabilities {
	bool verses = false;
	bool search = false;
	bool strongs = false;
	bool morphology = false;
	bool headings = false;
	bool footnotes = false;
	bool crossrefs = false;
	bool dictionaryLookup = false;
	bool editable = false;
};

struct BibleModuleInfo {
	std::string id;
	std::string description;
	std::string language;
	BibleModuleType type = BibleModuleType::Unknown;
};

struct BibleKeyInfo {
	BibleReference reference;
	std::string key;
	std::string bookName;
	std::string osisBook;
	int bookIndex = 0;
	int chapterCount = 0;
	int verseCount = 0;
};

struct BibleVerse {
	BibleReference reference;
	std::string key;
	std::string text;
	std::string osisRef;
};

/* Enriched verse data exposed without leaking a backend's attribute model. */
struct BibleWordInfo {
	std::size_t start = 0;
	std::size_t length = 0;
	std::string text;
	std::string lemma;
	std::string strong;
	std::vector<StrongId> strongs;
	std::vector<MorphologyTag> morphologyTags;
	/* Legacy backend display value. New neutral import code uses morphologyTags. */
	std::string morphology;
	std::string gloss;
};

struct BibleHeading {
	std::string text;
};

enum class BibleTextStyle {
	Added
};

struct BibleTextSpan {
	std::size_t start = 0;
	std::size_t length = 0;
	BibleTextStyle style = BibleTextStyle::Added;
};

struct BibleFootnote {
	std::string id;
	std::string body;
	std::string referenceList;
	std::string label;
	std::size_t offset = 0;
};

struct BibleCrossReference {
	std::string label;
	std::vector<BibleReference> references;
	std::string displayText;
	std::size_t offset = 0;
};

struct BibleVerseContent {
	BibleReference reference;
	std::string plainText;
	std::string renderedText;
	std::vector<BibleWordInfo> words;
	std::vector<BibleHeading> headings;
	std::vector<BibleTextSpan> spans;
	std::vector<BibleFootnote> footnotes;
	std::vector<BibleCrossReference> crossReferences;
	bool paragraphBreak = false;
	bool footnotesHaveNumbers = false;
	bool valid = false;
	/* Provenance after resolution. requested is what the user asked
	 * for; sourceModuleId is who supplied the verse body. isFallback
	 * is true only when that body came from the fallback module.
	 * headingSourceModuleId names who supplied headings: the requested
	 * module when it had any, otherwise the same as sourceModuleId. */
	std::string requestedModuleId;
	std::string sourceModuleId;
	std::string headingSourceModuleId;
	bool isFallback = false;
};

enum class ContentAvailability {
	Available,
	Missing,
	NotApplicable
};

struct FallbackPolicy {
	std::string fallbackModuleId;
	std::string fallbackDisplayName;
};

struct StrongOccurrence {
	BibleReference reference;
	std::string key;
	std::string word;
	std::string context;
	StrongId strong;
};

struct StrongOccurrencePage {
	std::vector<StrongOccurrence> occurrences;
	bool hasMore = false;
};

struct MorphologyOccurrence {
	BibleReference reference;
	std::string key;
	std::string word;
	std::string context;
	std::size_t start = 0;
	std::size_t length = 0;
	MorphologyTag morphology;
};

struct MorphologyOccurrencePage {
	std::vector<MorphologyOccurrence> occurrences;
	bool hasMore = false;
};

/* A backend-neutral identity and annotation snapshot for an interacted word.
 * start/length are UTF-8 byte offsets into the owning verse plain text. */
struct BibleAnnotatedWord {
	BibleReference reference;
	std::size_t start = 0;
	std::size_t length = 0;
	std::string word;
	std::vector<StrongId> strongs;
	std::vector<MorphologyTag> morphologyTags;
};

struct DictionaryEntry {
	std::string key;
	std::string text;
	bool valid = false;
};

enum class BibleOption {
	Strongs,
	Morphology,
	Footnotes,
	Headings,
	Lemmas,
	Glosses,
	GreekAccents,
	ArabicVowelPoints,
	CrossReferences,
	HebrewVowelPoints,
	HebrewCantillation,
	ItalicHeadings,
	WordsOfChristInRed,
	TransliteratedForms,
	Enumerations,
	MorphemeSegmentation
};

enum class BibleSearchMode {
	Regex,
	Phrase,
	MultiWord,
	Indexed,
	Attribute
};

struct BibleSearchQuery {
	std::string text;
	BibleSearchMode mode = BibleSearchMode::Regex;
	bool caseSensitive = false;
	bool fromDialog = false;
	bool matchWholeEntry = false;
	std::size_t limit = 100;
	std::size_t offset = 0;
};

struct BibleSearchResult {
	std::string module;
	std::string key;
	BibleReference reference;
	std::string text;
	std::string osisRef;
};

#endif /* XIPHOS_BIBLE_TYPES_H */
