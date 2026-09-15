/*
 * Minimal backend API used by the core for module discovery and text access.
 *
 * Implementations are free to use a native Bible format internally.  No
 * native backend types may cross this interface.
 */
#ifndef XIPHOS_BIBLE_BACKEND_H
#define XIPHOS_BIBLE_BACKEND_H

#include <string>
#include <vector>

#include "backend/bible_types.h"

class BibleBackend
{
public:
	virtual ~BibleBackend() = default;

	virtual std::vector<BibleModuleInfo> listModules() const = 0;
	virtual bool hasModule(const std::string &module_id) const = 0;
	/* Legacy string-key text accessor. New display code should use
	 * getVerseContent() with a BibleReference. */
	virtual std::string getText(const std::string &module_id,
					    const std::string &key,
					    bool rendered)
	{
		BibleKeyInfo key_info;
		if (!resolveKey(module_id, key, key_info))
			return std::string();
		BibleVerseContent content = getVerseContent(
			module_id, key_info.reference, !rendered);
		return rendered ? content.renderedText : content.plainText;
	}
	virtual BibleModuleType moduleType(
		const std::string &module_id) const = 0;
	virtual BibleModuleCapabilities moduleCapabilities(
		const std::string &module_id) const = 0;
	virtual std::string moduleDescription(const std::string &module_id) const = 0;
	virtual std::string moduleLanguage(const std::string &module_id) const = 0;
	/* SWORD versification system name (KJV, NRSVA, Vulg, ...). Used
	 * only by the missing-content resolver to map citations before
	 * reading the fallback module. Default is KJV identity. */
	virtual std::string versification(const std::string &module_id) const
	{
		(void)module_id;
		return "KJV";
	}
	virtual std::string osisRefFromKey(const std::string &module_id,
						  const std::string &key) = 0;
	virtual bool resolveKey(const std::string &module_id,
				       const std::string &key,
				       BibleKeyInfo &result) = 0;
	/* Carries source_key, native to source_module, to the reference
	 * native to target_module. References are never reread as text in
	 * another versification. Without a real mapper only identical
	 * versifications are accepted; everything else is Unmapped. */
	virtual BibleReferenceConversion convertReference(
		const std::string &source_module,
		const std::string &source_key,
		const std::string &target_module)
	{
		BibleReferenceConversion result;
		BibleKeyInfo source;
		if (!resolveKey(source_module, source_key, source))
			return result;
		if (!hasModule(target_module)) {
			result.status = BibleReferenceMapping::InvalidTarget;
			return result;
		}
		if (versification(source_module) != versification(target_module)) {
			result.status = BibleReferenceMapping::Unmapped;
			return result;
		}
		if (!resolveKey(target_module, source.key, result.target)) {
			result.status = BibleReferenceMapping::InvalidTarget;
			return result;
		}
		result.status = BibleReferenceMapping::Mapped;
		return result;
	}
	/* Same, when only the source's versification name survives (the
	 * module itself is gone, e.g. just uninstalled). */
	virtual BibleReferenceConversion convertReferenceFromVersification(
		const std::string &source_versification,
		const std::string &source_key,
		const std::string &target_module)
	{
		BibleReferenceConversion result;
		if (source_versification.empty() || source_key.empty())
			return result;
		if (!hasModule(target_module)) {
			result.status = BibleReferenceMapping::InvalidTarget;
			return result;
		}
		if (versification(target_module) != source_versification) {
			result.status = BibleReferenceMapping::Unmapped;
			return result;
		}
		if (!resolveKey(target_module, source_key, result.target)) {
			result.status = BibleReferenceMapping::InvalidTarget;
			return result;
		}
		result.status = BibleReferenceMapping::Mapped;
		return result;
	}
	virtual std::vector<BibleVerse> getChapter(
		const std::string &module_id,
		const BibleReference &reference,
		bool rendered) = 0;
	virtual BibleVerseContent getVerseContent(
		const std::string &module_id,
		const BibleReference &reference,
		bool include_plain_text = false) = 0;
	/* Compatibility helper for non-VerseKey displays which still render the
	 * module's current entry. */
	virtual bool currentEntryFootnotesHaveNumbers(
		const std::string &module_id) const
	{
		(void)module_id;
		return false;
	}
	virtual bool getCurrentEntryFootnote(const std::string &module_id,
					     const std::string &id,
					     BibleFootnote &footnote)
	{
		(void)module_id;
		(void)id;
		(void)footnote;
		return false;
	}
	virtual std::vector<std::string> getCurrentEntryCrossReferences(
		const std::string &module_id)
	{
		(void)module_id;
		return std::vector<std::string>();
	}
	virtual DictionaryEntry lookupDictionary(
		const std::string &module_id,
		const std::string &key) = 0;
	virtual std::string navigate(const std::string &module_id,
					 const std::string &key,
					 int direction) = 0;
	/* One native slot from a position already resolved, in the module's
	 * versification. At either end result is the same slot. The default
	 * goes through the key text; a backend whose key text can fail to
	 * identify its slot steps on the structural position instead. */
	virtual bool navigateFrom(const std::string &module_id,
				  const BibleKeyInfo &from, int direction,
				  BibleKeyInfo &result)
	{
		const std::string next = navigate(module_id, from.key, direction);
		return !next.empty() && resolveKey(module_id, next, result);
	}
	virtual std::string setChapter(const std::string &module_id,
					   const std::string &key,
					   int chapter) = 0;
	virtual std::string setVerse(const std::string &module_id,
					 const std::string &key,
					 int verse) = 0;
	virtual std::string setBook(const std::string &module_id,
					const std::string &key,
					int testament,
					int book) = 0;
	virtual std::vector<std::string> bookNames(
		const std::string &module_id, int testament) const = 0;
	virtual std::vector<BibleSearchResult> search(
		const std::string &module_id,
		const BibleSearchQuery &query) = 0;
	virtual std::vector<StrongOccurrence> findStrongOccurrences(
		const std::string &module_id, const StrongId &strong,
		std::size_t limit, std::size_t offset)
	{
		return findStrongOccurrencePage(module_id, strong, limit, offset)
			.occurrences;
	}
	virtual StrongOccurrencePage findStrongOccurrencePage(
		const std::string &, const StrongId &, std::size_t, std::size_t)
	{ return {}; }
	virtual std::vector<MorphologyOccurrence> findMorphologyOccurrences(
		const std::string &module_id, const MorphologyTag &morphology,
		std::size_t limit, std::size_t offset)
	{
		return findMorphologyOccurrencePage(module_id, morphology, limit, offset)
			.occurrences;
	}
	virtual MorphologyOccurrencePage findMorphologyOccurrencePage(
		const std::string &, const MorphologyTag &, std::size_t, std::size_t)
	{ return {}; }
	virtual bool resolveAnnotatedWord(const std::string &module_id,
		const BibleReference &reference, std::size_t byte_offset,
		BibleAnnotatedWord &result)
	{
		const BibleVerseContent content = getVerseContent(module_id, reference);
		if (!content.valid) return false;
		for (const BibleWordInfo &word : content.words) {
			if (word.length == 0 || byte_offset < word.start ||
			    byte_offset - word.start >= word.length) continue;
			result.reference = reference;
			result.start = word.start;
			result.length = word.length;
			result.word = word.text;
			result.strongs = word.strongs;
			result.morphologyTags = word.morphologyTags;
			return true;
		}
		return false;
	}
	virtual void setGlobalOption(const std::string &option,
					     bool enabled)
	{
		(void)option;
		(void)enabled;
	}
	virtual void setOption(BibleOption option, bool enabled)
	{
		(void)option;
		(void)enabled;
	}
	virtual void configureRendering(const std::string &module_id,
					bool morphology_first,
					bool render_note_numbers)
	{
		(void)module_id;
		(void)morphology_first;
		(void)render_note_numbers;
	}
};

/* Non-owning view of the single backend owned by the application. */
extern BibleBackend *bible_backend;

#endif /* XIPHOS_BIBLE_BACKEND_H */
