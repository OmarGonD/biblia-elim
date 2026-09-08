/*
 * The existing BackEnd owns the long-lived SWMgr and contains the mature
 * SWORD implementation.  SwordBackend is intentionally a thin concrete
 * adapter in this first step, so there is one manager and no duplicated
 * loading or caching logic.
 */
#include "backend/sword/sword_backend.h"

#include <regex.h>
#include <string.h>
#include <sstream>
#include <utility>

#include <gbfxhtml.h>
#include <osisxhtml.h>
#include <swmodule.h>
#include <teixhtml.h>
#include <thmlxhtml.h>
#include <utf8html.h>
#include <versekey.h>

#include "main/search_dialog.h"
#include "main/search_sidebar.h"

namespace {

static sword::VerseKey *verseKeyFor(sword::SWModule *module,
					    const std::string &key)
{
	if (!module)
		return NULL;
	sword::VerseKey *verse_key =
		dynamic_cast<sword::VerseKey *>(module->createKey());
	if (!verse_key)
		return NULL;
	verse_key->setAutoNormalize(1);
	verse_key->setText(key.c_str());
	return verse_key;
}

static BibleReference referenceFor(const sword::VerseKey &key)
{
	BibleReference reference;
	reference.testament = key.getTestament();
	reference.book = key.getBook();
	reference.chapter = key.getChapter();
	reference.verse = key.getVerse();
	return reference;
}

static int swordSearchType(BibleSearchMode mode)
{
	switch (mode) {
	case BibleSearchMode::Phrase:
		return -1;
	case BibleSearchMode::MultiWord:
		return -2;
	case BibleSearchMode::Indexed:
		return -4;
	case BibleSearchMode::Attribute:
		return -3;
	case BibleSearchMode::Regex:
	default:
		return 0;
	}
}

static const sword::SWBuf *attributeValue(
	const sword::AttributeValue &values, const char *name)
{
	sword::AttributeValue::const_iterator value = values.find(name);
	return value == values.end() ? NULL : &value->second;
}

static std::string attributeText(const sword::AttributeValue &values,
				 const char *name)
{
	const sword::SWBuf *value = attributeValue(values, name);
	return value ? std::string(value->c_str()) : std::string();
}

static void splitLemmaAndStrong(const std::string &source,
				BibleWordInfo &word)
{
	std::istringstream tokens(source);
	std::string token;
	while (tokens >> token) {
		std::string *destination = &word.lemma;
		std::string value = token;
		if (token.compare(0, 7, "strong:") == 0 ||
		    token.compare(0, 7, "Strong:") == 0) {
			destination = &word.strong;
			value.erase(0, 7);
		} else if (token.compare(0, 6, "lemma:") == 0 ||
			   token.compare(0, 6, "Lemma:") == 0) {
			value.erase(0, 6);
		}
		if (!destination->empty())
			destination->append(" ");
		destination->append(value);
	}
}

static const char *swordOptionName(BibleOption option)
{
	switch (option) {
	case BibleOption::Strongs: return "Strong's Numbers";
	case BibleOption::Morphology: return "Morphological Tags";
	case BibleOption::Footnotes: return "Footnotes";
	case BibleOption::Headings: return "Headings";
	case BibleOption::Lemmas: return "Lemmas";
	case BibleOption::Glosses: return "Glosses";
	case BibleOption::GreekAccents: return "Greek Accents";
	case BibleOption::ArabicVowelPoints: return "Arabic Vowel Points";
	case BibleOption::CrossReferences: return "Cross-references";
	case BibleOption::HebrewVowelPoints: return "Hebrew Vowel Points";
	case BibleOption::HebrewCantillation: return "Hebrew Cantillation";
	case BibleOption::ItalicHeadings: return "Italic Headings";
	case BibleOption::WordsOfChristInRed: return "Words of Christ in Red";
	case BibleOption::TransliteratedForms: return "Transliterated Forms";
	case BibleOption::Enumerations: return "Enumerations";
	case BibleOption::MorphemeSegmentation: return "Morpheme Segmentation";
	}
	return NULL;
}

} // namespace

BibleModuleType BackEnd::moduleType(const std::string &module_id) const
{
	sword::SWModule *module =
		const_cast<BackEnd *>(this)->get_SWModule(module_id.c_str());
	if (!module) return BibleModuleType::Unknown;
	if (!strcmp(module->getType(), "Biblical Texts"))
		return BibleModuleType::Bible;
	if (!strcmp(module->getType(), "Commentaries"))
		return !strcmp(module->getConfigEntry("ModDrv"), "RawFiles")
			       ? BibleModuleType::PersonalCommentary
			       : BibleModuleType::Commentary;
	if (!strcmp(module->getType(), "Lexicons / Dictionaries")) {
		const char *feature = module->getConfigEntry("Feature");
		return feature && (!strcmp(feature, "GreekDef") ||
				   !strcmp(feature, "HebrewDef"))
			       ? BibleModuleType::Lexicon
			       : BibleModuleType::Dictionary;
	}
	if (!strcmp(module->getType(), "Generic Books"))
		return module->getConfigEntry("GSType") &&
			       !strcmp(module->getConfigEntry("GSType"), "PrayerList")
			       ? BibleModuleType::PrayerList
			       : BibleModuleType::GeneralBook;
	return BibleModuleType::Unknown;
}

BibleModuleCapabilities BackEnd::moduleCapabilities(
	const std::string &module_id) const
{
	BibleModuleCapabilities capabilities;
	BackEnd *self = const_cast<BackEnd *>(this);
	BibleModuleType type = moduleType(module_id);
	capabilities.verses = type == BibleModuleType::Bible ||
		type == BibleModuleType::Commentary ||
		type == BibleModuleType::PersonalCommentary;
	capabilities.search = capabilities.verses;
	capabilities.dictionaryLookup = type == BibleModuleType::Dictionary ||
		type == BibleModuleType::Lexicon;
	capabilities.editable = type == BibleModuleType::PersonalCommentary;
	capabilities.strongs =
		self->has_global_option(module_id.c_str(), "GBFStrongs") ||
		self->has_global_option(module_id.c_str(), "ThMLStrongs") ||
		self->has_global_option(module_id.c_str(), "OSISStrongs");
	capabilities.morphology =
		self->has_global_option(module_id.c_str(), "GBFMorph") ||
		self->has_global_option(module_id.c_str(), "ThMLMorph") ||
		self->has_global_option(module_id.c_str(), "OSISMorph");
	return capabilities;
}

bool BackEnd::resolveKey(const std::string &module_id,
				 const std::string &key,
				 BibleKeyInfo &result)
{
	sword::SWModule *module = get_SWModule(module_id.c_str());
	sword::VerseKey *verse_key = verseKeyFor(module, key);
	if (!verse_key)
		return false;

	result.reference = referenceFor(*verse_key);
	result.key = verse_key->getText();
	result.bookName = verse_key->getBookName();
	result.bookIndex = result.reference.book;
	if (result.reference.testament == 2)
		result.bookIndex += verse_key->BMAX[0];
	result.chapterCount = verse_key->getChapterMax();
	result.verseCount = verse_key->getVerseMax();
	delete verse_key;
	return true;
}

std::vector<BibleVerse> BackEnd::getChapter(const std::string &module_id,
						    const BibleReference &reference,
						    bool rendered)
{
	std::vector<BibleVerse> verses;
	sword::SWModule *module = get_SWModule(module_id.c_str());
	if (!module)
		return verses;
	sword::VerseKey *verse_key = dynamic_cast<sword::VerseKey *>(module->createKey());
	if (!verse_key)
		return verses;

	verse_key->setAutoNormalize(1);
	if (reference.testament > 0)
		verse_key->setTestament(reference.testament);
	if (reference.book > 0)
		verse_key->setBook(reference.book);
	if (reference.chapter > 0)
		verse_key->setChapter(reference.chapter);
	int last_verse = verse_key->getVerseMax();
	for (int verse = 1; verse <= last_verse; ++verse) {
		verse_key->setVerse(verse);
		module->setKey(verse_key);
		BibleVerse item;
		item.reference = referenceFor(*verse_key);
		item.key = verse_key->getText();
		item.osisRef = verse_key->getOSISRef();
		if (rendered)
			item.text = module->renderText().c_str();
		else
			item.text = module->getRawEntry();
		verses.push_back(item);
	}
	delete verse_key;
	return verses;
}

BibleVerseContent BackEnd::getVerseContent(
	const std::string &module_id, const BibleReference &reference,
	bool include_plain_text)
{
	BibleVerseContent content;
	content.reference = reference;
	sword::SWModule *module = get_SWModule(module_id.c_str());
	if (!module)
		return content;

	sword::VerseKey *verse_key =
		dynamic_cast<sword::VerseKey *>(module->createKey());
	if (!verse_key)
		return content;
	if (reference.testament < 1 || reference.testament > 2 ||
	    reference.book < 1 || reference.chapter < 0 || reference.verse < 0) {
		delete verse_key;
		return content;
	}
	verse_key->setAutoNormalize(0);
	verse_key->setTestament(reference.testament);
	if (reference.book > verse_key->getBookMax()) {
		delete verse_key;
		return content;
	}
	verse_key->setBook(reference.book);
	if (reference.chapter > verse_key->getChapterMax() ||
	    (reference.chapter == 0 && reference.verse != 0)) {
		delete verse_key;
		return content;
	}
	verse_key->setChapter(reference.chapter);
	if (reference.chapter > 0 && reference.verse > verse_key->getVerseMax()) {
		delete verse_key;
		return content;
	}
	verse_key->setVerse(reference.verse);
	if (verse_key->popError()) {
		delete verse_key;
		return content;
	}

	module->setKey(verse_key);
	delete verse_key;
	if (include_plain_text) {
		const char *plain = module->stripText();
		if (plain)
			content.plainText = plain;
	}
	content.renderedText = module->renderText().c_str();
	content.valid = !module->popError();

	sword::AttributeTypeList &attributes = module->getEntryAttributes();
	sword::AttributeTypeList::const_iterator word_type = attributes.find("Word");
	if (word_type != attributes.end()) {
		for (sword::AttributeList::const_iterator item =
			     word_type->second.begin();
		     item != word_type->second.end(); ++item) {
			BibleWordInfo word;
			word.text = attributeText(item->second, "Text");
			std::string lemma = attributeText(item->second, "Lemma");
			splitLemmaAndStrong(lemma, word);
			std::string explicit_strong =
				attributeText(item->second, "Strong");
			if (!explicit_strong.empty())
				word.strong = explicit_strong;
			word.morphology = attributeText(item->second, "Morph");
			word.gloss = attributeText(item->second, "Gloss");
			content.words.push_back(std::move(word));
		}
	}

	sword::AttributeTypeList::const_iterator footnote_type =
		attributes.find("Footnote");
	if (footnote_type != attributes.end()) {
		sword::AttributeList::const_iterator first =
			footnote_type->second.find("1");
		content.footnotesHaveNumbers =
			first != footnote_type->second.end() &&
			attributeValue(first->second, "n") != NULL;
	}

	sword::AttributeTypeList::const_iterator heading_type =
		attributes.find("Heading");
	std::vector<std::string> raw_headings;
	if (heading_type != attributes.end()) {
		sword::AttributeList::const_iterator preverse =
			heading_type->second.find("Preverse");
		if (preverse != heading_type->second.end()) {
			for (int index = 0;; ++index) {
				std::ostringstream key;
				key << index;
				const sword::SWBuf *raw =
					attributeValue(preverse->second, key.str().c_str());
				if (!raw)
					break;
				sword::SWBuf heading_text = raw->c_str();
				sword::UTF8HTML utf8_html;
				utf8_html.processText(heading_text);
				raw_headings.push_back(heading_text.c_str());
			}
		}
	}
	/* Rendering an isolated heading may replace EntryAttributes, so only do
	 * it after all attribute iterators have gone out of use. */
	for (std::vector<std::string>::const_iterator raw = raw_headings.begin();
	     raw != raw_headings.end(); ++raw) {
		BibleHeading heading;
		heading.text = module->renderText(raw->c_str()).c_str();
		content.headings.push_back(std::move(heading));
	}
	return content;
}

bool BackEnd::currentEntryFootnotesHaveNumbers(
	const std::string &module_id) const
{
	sword::SWModule *module =
		const_cast<BackEnd *>(this)->get_SWModule(module_id.c_str());
	if (!module)
		return false;
	sword::AttributeTypeList &attributes = module->getEntryAttributes();
	sword::AttributeTypeList::const_iterator footnote_type =
		attributes.find("Footnote");
	if (footnote_type == attributes.end())
		return false;
	sword::AttributeList::const_iterator first =
		footnote_type->second.find("1");
	return first != footnote_type->second.end() &&
	       attributeValue(first->second, "n") != NULL;
}

bool BackEnd::getCurrentEntryFootnote(const std::string &module_id,
				      const std::string &id,
				      BibleFootnote &footnote)
{
	sword::SWModule *module = get_SWModule(module_id.c_str());
	if (!module || id.empty())
		return false;
	module->renderText();
	sword::AttributeTypeList &attributes = module->getEntryAttributes();
	sword::AttributeTypeList::const_iterator footnote_type =
		attributes.find("Footnote");
	if (footnote_type == attributes.end())
		return false;
	sword::AttributeList::const_iterator item = footnote_type->second.find(id.c_str());
	if (item == footnote_type->second.end())
		return false;
	footnote.id = id;
	footnote.body = attributeText(item->second, "body");
	footnote.referenceList = attributeText(item->second, "refList");
	footnote.label = attributeText(item->second, "n");
	sword::UTF8HTML utf8_html;
	sword::SWBuf body = footnote.body.c_str();
	sword::SWBuf references = footnote.referenceList.c_str();
	utf8_html.processText(body);
	utf8_html.processText(references);
	footnote.body = body.length()
		? std::string(module->renderText(body).c_str()) : std::string();
	footnote.referenceList = references.c_str();
	return true;
}

std::vector<std::string> BackEnd::getCurrentEntryCrossReferences(
	const std::string &module_id)
{
	std::vector<std::string> references;
	sword::SWModule *module = get_SWModule(module_id.c_str());
	if (!module)
		return references;
	module->renderText();
	sword::AttributeTypeList &attributes = module->getEntryAttributes();
	sword::AttributeTypeList::const_iterator footnote_type =
		attributes.find("Footnote");
	if (footnote_type == attributes.end())
		return references;
	for (int index = 0; index < 24; ++index) {
		std::ostringstream id;
		id << index;
		sword::AttributeList::const_iterator item =
			footnote_type->second.find(id.str().c_str());
		if (item == footnote_type->second.end()) {
			if (index == 0)
				continue;
			break;
		}
		std::string value = attributeText(item->second, "refList");
		if (value.empty()) {
			if (index == 0)
				continue;
			break;
		}
		sword::SWBuf html = value.c_str();
		sword::UTF8HTML utf8_html;
		utf8_html.processText(html);
		references.push_back(html.c_str());
	}
	return references;
}

DictionaryEntry BackEnd::lookupDictionary(const std::string &module_id,
					   const std::string &key)
{
	DictionaryEntry entry;
	entry.key = key;
	sword::SWModule *module = get_SWModule(module_id.c_str());
	if (!module || key.empty())
		return entry;
	module->setKey(key.c_str());
	const char *raw = module->getRawEntry();
	if (!raw || module->popError())
		return entry;
	entry.key = module->getKeyText();
	entry.text = module->renderText().c_str();
	entry.valid = true;
	return entry;
}

std::string BackEnd::navigate(const std::string &module_id,
				      const std::string &key,
				      int direction)
{
	sword::SWModule *module = get_SWModule(module_id.c_str());
	sword::VerseKey *verse_key = verseKeyFor(module, key);
	if (!verse_key)
		return std::string();
	if (direction < 0)
		--(*verse_key);
	else if (direction > 0)
		++(*verse_key);
	/* Preserve the existing navbar behavior for modules with OCR gaps: keep
	 * moving in the requested direction until a readable verse is found, but
	 * stop at the same key or after the historical safety bound. */
	module->setKey(verse_key);
	for (int n = 0; n < 40 && direction != 0; ++n) {
		const char *raw = module->getRawEntry();
		if (raw && *raw)
			break;
		BibleReference before = referenceFor(*verse_key);
		if (direction < 0)
			--(*verse_key);
		else
			++(*verse_key);
		BibleReference after = referenceFor(*verse_key);
		if (before.testament == after.testament &&
		    before.book == after.book && before.chapter == after.chapter &&
		    before.verse == after.verse)
			break;
		module->setKey(verse_key);
	}
	std::string result(verse_key->getText());
	delete verse_key;
	return result;
}

std::string BackEnd::setChapter(const std::string &module_id,
					const std::string &key,
					int chapter)
{
	sword::VerseKey *verse_key = verseKeyFor(get_SWModule(module_id.c_str()), key);
	if (!verse_key)
		return std::string();
	verse_key->setChapter(chapter);
	std::string result(verse_key->getText());
	delete verse_key;
	return result;
}

std::string BackEnd::setVerse(const std::string &module_id,
				      const std::string &key,
				      int verse)
{
	sword::VerseKey *verse_key = verseKeyFor(get_SWModule(module_id.c_str()), key);
	if (!verse_key)
		return std::string();
	verse_key->setVerse(verse);
	std::string result(verse_key->getText());
	delete verse_key;
	return result;
}

std::string BackEnd::setBook(const std::string &module_id,
				     const std::string &key,
				     int testament,
				     int book)
{
	sword::VerseKey *verse_key = verseKeyFor(get_SWModule(module_id.c_str()), key);
	if (!verse_key)
		return std::string();
	verse_key->setTestament(testament);
	verse_key->setBook(book);
	std::string result(verse_key->getText());
	delete verse_key;
	return result;
}

std::vector<std::string> BackEnd::bookNames(const std::string &module_id,
						    int testament) const
{
	std::vector<std::string> names;
	sword::SWModule *module =
		const_cast<BackEnd *>(this)->get_SWModule(module_id.c_str());
	if (!module)
		return names;
	sword::VerseKey *verse_key =
		dynamic_cast<sword::VerseKey *>(module->createKey());
	if (!verse_key)
		return names;
	int count = verse_key->BMAX[testament - 1];
	for (int book = 1; book <= count; ++book) {
		verse_key->setTestament(testament);
		verse_key->setBook(book);
		names.push_back(verse_key->getBookName());
	}
	delete verse_key;
	return names;
}

std::vector<BibleSearchResult> BackEnd::search(
	const std::string &module_id, const BibleSearchQuery &query)
{
	std::vector<BibleSearchResult> matches;
	sword::SWModule *module = get_SWModule(module_id.c_str());
	if (!module || query.text.empty())
		return matches;

	const char *stripped = module->stripText(query.text.c_str());
	int params = query.caseSensitive ? 0 : REG_ICASE;
	if (query.matchWholeEntry)
		params |= 4096; /* SWORD SEARCHFLAG_MATCHWHOLEENTRY */
	int count = do_module_search((char *)module_id.c_str(),
					     stripped ? stripped : query.text.c_str(),
					     swordSearchType(query.mode), params,
					     query.fromDialog ? 1 : 0);
	if (count <= 0)
		return matches;

	const char *key = NULL;
	while ((key = get_next_listkey()) != NULL) {
		BibleSearchResult result;
		result.module = module_id;
		result.key = key;
		BibleKeyInfo info;
		if (resolveKey(module_id, result.key, info))
			result.reference = info.reference;
		char *stripped_text = get_strip_text(module_id.c_str(), result.key.c_str());
		if (stripped_text) {
			result.text = stripped_text;
			free(stripped_text);
		}
		result.osisRef = osisRefFromKey(module_id, result.key);
		matches.push_back(result);
	}
	if (query.offset >= matches.size()) return {};
	const auto first = matches.begin() + query.offset;
	const auto last = query.limit >= matches.size() - query.offset
		? matches.end() : first + query.limit;
	return std::vector<BibleSearchResult>(first, last);
}

void BackEnd::setGlobalOption(const std::string &option, bool enabled)
{
	if (get_mgr())
		get_mgr()->setGlobalOption(option.c_str(), enabled ? "On" : "Off");
}

void BackEnd::setOption(BibleOption option, bool enabled)
{
	const char *name = swordOptionName(option);
	if (name && get_mgr())
		get_mgr()->setGlobalOption(name, enabled ? "On" : "Off");
}

void BackEnd::configureRendering(const std::string &module_id,
				 bool morphology_first,
				 bool render_note_numbers)
{
	sword::SWModule *module = get_SWModule(module_id.c_str());
	if (!module)
		return;
	for (sword::FilterList::const_iterator it =
		     module->getRenderFilters().begin();
	     it != module->getRenderFilters().end(); ++it) {
		sword::OSISXHTML *osis = dynamic_cast<sword::OSISXHTML *>(*it);
		if (osis) {
			if (morphology_first)
				osis->setMorphFirst();
			osis->setRenderNoteNumbers(render_note_numbers);
		}
		sword::ThMLXHTML *thml = dynamic_cast<sword::ThMLXHTML *>(*it);
		if (thml)
			thml->setRenderNoteNumbers(render_note_numbers);
		sword::GBFXHTML *gbf = dynamic_cast<sword::GBFXHTML *>(*it);
		if (gbf)
			gbf->setRenderNoteNumbers(render_note_numbers);
		sword::TEIXHTML *tei = dynamic_cast<sword::TEIXHTML *>(*it);
		if (tei)
			tei->setRenderNoteNumbers(render_note_numbers);
	}
}

char *BackEnd::get_entry_attribute(const char *level1,
				   const char *level2,
				   const char *level3,
				   bool render)
{
	if (!display_mod)
		return NULL;
	if (render)
		display_mod->renderText();
	sword::SWBuf attribute =
		display_mod->getEntryAttributes()[level1][level2][level3].c_str();
	sword::UTF8HTML utf8_html;
	utf8_html.processText(attribute);
	return attribute.length() ? strdup(attribute.c_str()) : NULL;
}
