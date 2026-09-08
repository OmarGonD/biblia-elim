#include "backend/osis_importer.h"

#include "backend/bible_book_map.h"
#include "backend/sqlite/sqlite_module_writer.h"
#include "backend/strong_id.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cctype>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

namespace {

std::string property(xmlNode *node, const char *name)
{
	xmlChar *value = xmlGetProp(node, reinterpret_cast<const xmlChar *>(name));
	const std::string result = value ? reinterpret_cast<const char *>(value) : "";
	if (value) xmlFree(value);
	return result;
}

const BibleBookDefinition *findBook(const std::string &osis)
{
	for (const BibleBookDefinition &book : canonicalBibleBooks())
		if (osis == book.osis) return &book;
	return nullptr;
}

bool isClosingPunctuation(unsigned char value)
{
	return std::string(",.;:!?)]}").find(static_cast<char>(value)) !=
		std::string::npos;
}

/* The sole path by which visible OSIS character data enters a verse. */
class VisibleVerseText {
public:
	explicit VisibleVerseText(std::string &text) : text_(text) {}

	std::pair<std::size_t, std::size_t> append(const std::string &source)
	{
		std::size_t first = std::string::npos;
		std::size_t last = text_.size();
		for (unsigned char value : source) {
			if (std::isspace(value)) {
				if (!text_.empty()) pendingSpace_ = true;
				continue;
			}
			if (pendingSpace_) {
				if (!text_.empty() && text_.back() != ' ' &&
				    !isClosingPunctuation(value))
					text_.push_back(' ');
				pendingSpace_ = false;
			}
			if (first == std::string::npos) first = text_.size();
			text_.push_back(static_cast<char>(value));
			last = text_.size();
		}
		if (first == std::string::npos) return { text_.size(), 0 };
		return { first, last - first };
	}

	std::size_t offset() const { return text_.size(); }

private:
	std::string &text_;
	bool pendingSpace_ = false;
};

std::string descendantText(xmlNode *node)
{
	std::string result;
	for (; node; node = node->next) {
		if (node->type == XML_TEXT_NODE || node->type == XML_CDATA_SECTION_NODE)
			result += reinterpret_cast<const char *>(node->content);
		else if (node->type == XML_ELEMENT_NODE)
			result += descendantText(node->children);
	}
	return result;
}

std::string normalizedStandaloneText(const std::string &source)
{
	std::string result;
	VisibleVerseText normalized(result);
	normalized.append(source);
	return result;
}

std::vector<std::string> tokens(const std::string &source)
{
	std::vector<std::string> result;
	std::string token;
	for (unsigned char value : source) {
		if (std::isspace(value) || value == ',' || value == ';') {
			if (!token.empty()) result.push_back(std::move(token));
			token.clear();
		} else {
			token.push_back(static_cast<char>(value));
		}
	}
	if (!token.empty()) result.push_back(std::move(token));
	return result;
}

std::string scheme(const std::string &value)
{
	const std::size_t separator = value.find(':');
	return separator == std::string::npos ? "unqualified" :
		value.substr(0, separator);
}

std::set<std::string> supportedAttributes(const std::string &element)
{
	if (element == "verse") return { "osisID", "sID", "eID" };
	if (element == "chapter") return { "osisID", "sID", "eID" };
	if (element == "w") return { "lemma", "morph" };
	if (element == "note") return { "type", "n" };
	if (element == "reference") return { "osisRef" };
	return {};
}

void auditAttributes(UsfmImportStats &stats, xmlNode *node,
	const std::string &element)
{
	const std::set<std::string> supported = supportedAttributes(element);
	for (xmlAttr *attribute = node->properties; attribute; attribute = attribute->next) {
		const std::string name = reinterpret_cast<const char *>(attribute->name);
		if (supported.find(name) == supported.end())
			++stats.ignoredAttributes[element + "." + name];
	}
	if (element == "w" && xmlHasProp(node, BAD_CAST "morph")) {
		++stats.morphAttributes["w.morph"];
		for (const std::string &value : tokens(property(node, "morph")))
			++stats.morphSchemes[scheme(value)];
	}
}

bool parseReference(const std::string &source, BibleReference &reference)
{
	const std::size_t chapterDot = source.find('.');
	const std::size_t verseDot = source.find('.', chapterDot + 1);
	if (chapterDot == std::string::npos || verseDot == std::string::npos)
		return false;
	const BibleBookDefinition *book = findBook(source.substr(0, chapterDot));
	if (!book) return false;
	try {
		reference = { book->testament, book->bookId,
			std::stoi(source.substr(chapterDot + 1,
				verseDot - chapterDot - 1)),
			std::stoi(source.substr(verseDot + 1)) };
		return reference.chapter > 0 && reference.verse > 0;
	} catch (...) {
		return false;
	}
}

struct State {
	explicit State(UsfmImportStats &importStats) : stats(importStats) {}

	std::vector<SqliteImportBook> books;
	std::vector<SqliteImportVerse> verses;
	std::map<int, bool> seenBooks;
	std::vector<BibleHeading> pendingHeadings;
	bool pendingParagraph = false;
	bool verseActive = false;
	bool milestoneVerse = false;
	std::string activeVerseId;
	SqliteImportVerse verse;
	std::unique_ptr<VisibleVerseText> visible;
	UsfmImportStats &stats;
	std::string error;
};

void processChildren(State &state, xmlNode *node);

void registerBook(State &state, const std::string &chapterId)
{
	const std::size_t dot = chapterId.find('.');
	if (dot == std::string::npos) return;
	const BibleBookDefinition *book = findBook(chapterId.substr(0, dot));
	if (!book || state.seenBooks[book->bookId]) return;
	state.books.push_back({ book->bookId, book->testament, book->position,
		book->osis, book->name, book->shortName });
	state.seenBooks[book->bookId] = true;
}

void beginVerse(State &state, const BibleReference &reference)
{
	if (state.verseActive) {
		state.error = "nested OSIS verse";
		return;
	}
	state.verse = SqliteImportVerse();
	state.verse.reference = reference;
	state.verse.paragraphBreak = state.pendingParagraph;
	state.verse.headings = std::move(state.pendingHeadings);
	state.pendingHeadings.clear();
	state.pendingParagraph = false;
	state.verseActive = true;
	state.visible.reset(new VisibleVerseText(state.verse.text));
}

void finishVerse(State &state, bool milestone)
{
	state.verses.push_back(std::move(state.verse));
	state.visible.reset();
	state.verseActive = false;
	state.milestoneVerse = false;
	state.activeVerseId.clear();
	if (milestone) ++state.stats.milestoneVerses;
	else ++state.stats.containerVerses;
}

void addStrongWord(State &state, xmlNode *node)
{
	const std::string wordText = normalizedStandaloneText(descendantText(node->children));
	const auto range = state.visible->append(wordText);
	if (range.second == 0) return;

	BibleWordInfo word;
	word.start = range.first;
	word.length = range.second;
	word.text = state.verse.text.substr(word.start, word.length);
	for (const std::string &lemma : tokens(property(node, "lemma"))) {
		if (lemma.compare(0, 7, "strong:") == 0) {
			StrongId strong;
			if (parseStrongId(lemma.substr(7), strong))
				word.strongs.push_back(strong);
			else
				++state.stats.nonStrongLemmaAttributes["strong.invalid"];
		} else {
			++state.stats.nonStrongLemmaAttributes[scheme(lemma)];
		}
	}
	for (std::size_t index = 0; index < word.strongs.size(); ++index) {
		if (index) word.strong += ',';
		word.strong += formatStrongId(word.strongs[index]);
	}
	state.verse.words.push_back(std::move(word));
}

void addNote(State &state, xmlNode *node)
{
	const std::string body = normalizedStandaloneText(descendantText(node->children));
	if (property(node, "type") == "crossReference") {
		BibleCrossReference crossReference;
		crossReference.offset = state.visible->offset();
		crossReference.displayText = body;
		for (xmlNode *child = node->children; child; child = child->next) {
			if (child->type != XML_ELEMENT_NODE ||
			    std::string(reinterpret_cast<const char *>(child->name)) != "reference")
				continue;
			auditAttributes(state.stats, child, "reference");
			for (const std::string &osisRef : tokens(property(child, "osisRef"))) {
				if (osisRef.find('-') != std::string::npos) {
					++state.stats.rangeReferences[osisRef];
					continue;
				}
				BibleReference target;
				if (parseReference(osisRef, target)) {
					crossReference.references.push_back(target);
					++state.stats.crossrefTargetsResolved;
				} else {
					++state.stats.crossrefTargetsUnresolved;
					++state.stats.unresolvedReferences[osisRef.empty() ? "empty" : osisRef];
				}
			}
		}
		state.verse.crossReferences.push_back(std::move(crossReference));
		++state.stats.crossReferencesImported;
	} else {
		BibleFootnote footnote;
		footnote.offset = state.visible->offset();
		footnote.body = body;
		footnote.label = property(node, "n");
		state.verse.footnotes.push_back(std::move(footnote));
		++state.stats.footnotesImported;
	}
}

void processElement(State &state, xmlNode *node)
{
	const std::string name = reinterpret_cast<const char *>(node->name);
	auditAttributes(state.stats, node, name);
	if (name == "w" && state.verseActive) {
		addStrongWord(state, node);
		return;
	}
	if (name == "note" && state.verseActive) {
		addNote(state, node);
		return;
	}
	if (name == "title" && !state.verseActive) {
		const std::string title = normalizedStandaloneText(descendantText(node->children));
		if (!title.empty()) {
			state.pendingHeadings.push_back({ title });
			++state.stats.headingsImported;
		}
		return;
	}
	if (name == "p" && !state.verseActive) {
		state.pendingParagraph = true;
		++state.stats.paragraphMarkers;
		processChildren(state, node->children);
		return;
	}
	if (name == "verse") {
		const std::string startId = property(node, "sID");
		const std::string endId = property(node, "eID");
		const std::string containerId = property(node, "osisID");
		if (!startId.empty()) {
			BibleReference reference;
			if (!parseReference(startId, reference)) {
				state.error = "invalid OSIS verse sID: " + startId;
				return;
			}
			beginVerse(state, reference);
			state.milestoneVerse = true;
			state.activeVerseId = startId;
			processChildren(state, node->children);
			return;
		}
		if (!endId.empty()) {
			if (!state.verseActive || !state.milestoneVerse ||
			    endId != state.activeVerseId) {
				state.error = "invalid OSIS verse eID: " + endId;
				return;
			}
			finishVerse(state, true);
			return;
		}
		if (!containerId.empty()) {
			BibleReference reference;
			if (!parseReference(containerId, reference)) {
				state.error = "invalid OSIS verse osisID: " + containerId;
				return;
			}
			beginVerse(state, reference);
			state.milestoneVerse = false;
			processChildren(state, node->children);
			if (state.verseActive) finishVerse(state, false);
			return;
		}
		state.error = "verse missing OSIS id";
		return;
	}
	if (state.verseActive) {
		++state.stats.unknownInlineElements[name];
		processChildren(state, node->children);
		return;
	}
	if (name == "chapter") {
		std::string chapterId = property(node, "osisID");
		if (chapterId.empty()) chapterId = property(node, "sID");
		registerBook(state, chapterId);
	} else if (name != "osis" && name != "osisText" && name != "div") {
		++state.stats.unknownStructuralElements[name];
	}
	processChildren(state, node->children);
}

void processChildren(State &state, xmlNode *node)
{
	for (; node && state.error.empty(); node = node->next) {
		if ((node->type == XML_TEXT_NODE || node->type == XML_CDATA_SECTION_NODE) &&
		    state.verseActive)
			state.visible->append(reinterpret_cast<char *>(node->content));
		else if (node->type == XML_ELEMENT_NODE)
			processElement(state, node);
	}
}

} // namespace

bool importOsis(const std::string &input, const std::string &output,
	const UsfmImportOptions &options, UsfmImportStats &stats, std::string &error)
{
	xmlDocPtr document = xmlReadFile(input.c_str(), nullptr, XML_PARSE_NONET);
	if (!document) {
		error = "invalid OSIS XML";
		return false;
	}
	State state(stats);
	processChildren(state, xmlDocGetRootElement(document));
	xmlFreeDoc(document);
	if (state.error.empty() && state.verseActive)
		state.error = "EOF with open verse milestone";
	if (!state.error.empty()) {
		error = state.error;
		return false;
	}
	if (state.verses.empty()) {
		error = "no verses found";
		return false;
	}

	SqliteModuleMetadata metadata{ options.moduleId, options.name,
		options.language, options.versification, options.abbreviation,
		options.description, options.license, options.publisher, options.source,
		options.contentVersion, "osis" };
	SqliteModuleWriter writer;
	if (!writer.write(metadata, state.books, state.verses, output, error))
		return false;

	stats.books = state.books.size();
	stats.verses = state.verses.size();
	std::set<std::pair<int, int>> chapters;
	for (const SqliteImportVerse &verse : state.verses)
		chapters.insert({ verse.reference.book, verse.reference.chapter });
	stats.chapters = chapters.size();
	for (const SqliteImportVerse &verse : state.verses)
		for (const BibleWordInfo &word : verse.words) {
			++stats.wordsImported;
			if (!word.strongs.empty()) ++stats.wordsWithStrong;
			stats.strongIdsImported += word.strongs.size();
		}
	return true;
}
