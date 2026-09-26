/*
 * Biblia Elim - carrying a reference from one module to another.
 */
#include "main/reference_transition.h"

#include <cstring>
#include <vector>

static BibleModuleTransitionPlan planFromConversion(
	const BibleReferenceConversion &conversion)
{
	BibleModuleTransitionPlan plan;
	switch (conversion.status) {
	case BibleReferenceMapping::Mapped:
		plan.status = BibleModuleTransition::Converted;
		plan.key = conversion.target.key;
		break;
	case BibleReferenceMapping::Unmapped:
		plan.status = BibleModuleTransition::Unmapped;
		break;
	case BibleReferenceMapping::InvalidSource:
	case BibleReferenceMapping::InvalidTarget:
		plan.status = BibleModuleTransition::Invalid;
		break;
	}
	return plan;
}

BibleModuleTransitionPlan planBibleModuleTransition(
	BibleBackend &backend, const std::string &source_module,
	const std::string &source_key, const std::string &target_module)
{
	BibleModuleTransitionPlan plan;
	if (source_module.empty() || source_key.empty() ||
	    target_module.empty())
		return plan;
	if (source_module == target_module) {
		plan.status = BibleModuleTransition::SameModule;
		plan.key = source_key;
		return plan;
	}
	return planFromConversion(backend.convertReference(
		source_module, source_key, target_module));
}

BibleModuleTransitionPlan planBibleVersificationTransition(
	BibleBackend &backend, const std::string &source_versification,
	const std::string &source_key, const std::string &target_module)
{
	if (source_versification.empty() || source_key.empty() ||
	    target_module.empty())
		return BibleModuleTransitionPlan();
	return planFromConversion(backend.convertReferenceFromVersification(
		source_versification, source_key, target_module));
}

static bool isVerseKeyed(BibleBackend &backend, const std::string &module)
{
	switch (backend.moduleType(module)) {
	case BibleModuleType::Bible:
	case BibleModuleType::Commentary:
	case BibleModuleType::PersonalCommentary:
		return true;
	default:
		return false;
	}
}

BibleModuleTransitionPlan planUriKeyForMainBible(
	BibleBackend &backend, const std::string &source_module,
	const std::string &source_key, const std::string &main_module)
{
	BibleModuleTransitionPlan plan;
	if (source_key.empty())
		return plan;
	/* Nothing to carry the reference away from, or nowhere to carry
	 * it to: the key is used as given, which is what these routes
	 * always did. */
	if (source_module.empty() || main_module.empty() ||
	    source_module == main_module ||
	    !isVerseKeyed(backend, source_module)) {
		plan.status = BibleModuleTransition::SameModule;
		plan.key = source_key;
		return plan;
	}
	return planBibleModuleTransition(backend, source_module, source_key,
					 main_module);
}

const char *const kLegacyBookmarkVersification = "KJV";

BibleModuleTransitionPlan planLegacyBookmarkKey(
	BibleBackend &backend, const std::string &key,
	const std::string &target_module)
{
	BibleModuleTransitionPlan plan;
	if (key.empty() || target_module.empty())
		return plan;
	if (!isVerseKeyed(backend, target_module)) {
		plan.status = BibleModuleTransition::SameModule;
		plan.key = key;
		return plan;
	}
	return planBibleVersificationTransition(
		backend, kLegacyBookmarkVersification, key, target_module);
}

static std::string trim(const std::string &text)
{
	const size_t first = text.find_first_not_of(" \t");
	if (first == std::string::npos)
		return std::string();
	const size_t last = text.find_last_not_of(" \t");
	return text.substr(first, last - first + 1);
}

static std::vector<std::string> split(const std::string &text, char sep)
{
	std::vector<std::string> parts;
	size_t start = 0;
	for (;;) {
		const size_t at = text.find(sep, start);
		parts.push_back(trim(text.substr(start, at - start)));
		if (at == std::string::npos)
			return parts;
		start = at + 1;
	}
}

/* "Psalms 119:5" -> ("Psalms 119:", "5"): the part a bare verse number
 * after a comma or a dash is read against. */
static bool splitVerse(const std::string &ref, std::string &prefix,
		       std::string &verse)
{
	const size_t colon = ref.rfind(':');
	if (colon == std::string::npos)
		return false;
	prefix = ref.substr(0, colon + 1);
	verse = ref.substr(colon + 1);
	return !verse.empty();
}

static bool isNumber(const std::string &text)
{
	return !text.empty() &&
	       text.find_first_not_of("0123456789") == std::string::npos;
}

/* One reference of a legacy list, converted, with its target position. */
static bool convertLegacyRef(BibleBackend &backend, const std::string &ref,
			     const std::string &target, BibleKeyInfo &info)
{
	const BibleModuleTransitionPlan plan =
		planLegacyBookmarkKey(backend, ref, target);
	if (plan.status != BibleModuleTransition::SameModule &&
	    plan.status != BibleModuleTransition::Converted)
		return false;
	return backend.resolveKey(target, plan.key, info);
}

BibleModuleTransitionPlan planLegacyBookmarkKeyList(
	BibleBackend &backend, const std::string &key,
	const std::string &target_module)
{
	BibleModuleTransitionPlan plan;
	if (key.empty() || target_module.empty())
		return plan;
	if (!isVerseKeyed(backend, target_module)) {
		plan.status = BibleModuleTransition::SameModule;
		plan.key = key;
		return plan;
	}
	std::string out, context;
	for (const std::string &item : split(key, ';')) {
		if (item.empty())
			continue;
		for (const std::string &piece : split(item, ',')) {
			if (piece.empty())
				continue;
			const size_t dash = piece.find('-');
			std::string first = trim(piece.substr(0, dash));
			std::string prefix, verse;
			/* "8,9": a bare number continues the previous chapter. */
			if (isNumber(first) && !context.empty())
				first = context + first;
			BibleKeyInfo start;
			if (!convertLegacyRef(backend, first, target_module, start)) {
				plan.status = BibleModuleTransition::Unmapped;
				return plan;
			}
			if (splitVerse(first, prefix, verse))
				context = prefix;
			if (!out.empty())
				out += "; ";
			out += start.key;
			if (dash == std::string::npos)
				continue;
			/* Range end: "5" (same chapter), "120:2" or a full key. */
			std::string last = trim(piece.substr(dash + 1));
			if (isNumber(last))
				last = prefix + last;
			else if (last.find(' ') == std::string::npos &&
				 !prefix.empty()) {
				const size_t space = prefix.rfind(' ');
				last = prefix.substr(0, space + 1) + last;
			}
			BibleKeyInfo end;
			if (!convertLegacyRef(backend, last, target_module, end)) {
				plan.status = BibleModuleTransition::Unmapped;
				return plan;
			}
			out += "-";
			if (end.reference.book == start.reference.book &&
			    end.reference.chapter == start.reference.chapter)
				out += std::to_string(end.reference.verse);
			else if (end.reference.book == start.reference.book)
				out += std::to_string(end.reference.chapter) + ":" +
				       std::to_string(end.reference.verse);
			else
				out += end.key;
		}
	}
	if (out.empty())
		return plan;
	plan.status = BibleModuleTransition::Converted;
	plan.key = out;
	return plan;
}

/* Platense is normalized at install time to SpaPlatenseComentarios; the
 * two OCR editions publish their notes as separate zCom modules. Each
 * declares the same versification as its edition in its .conf; callers
 * still convert, so a mismatch would be mapped rather than misread. */
std::string planNoteVerseProjection(BibleBackend &backend,
				    const std::string &source_module,
				    const std::string &source_osisref,
				    const std::string &target_module)
{
	if (source_module.empty() || target_module.empty() ||
	    !backend.hasModule(source_module) ||
	    !backend.hasModule(target_module))
		return std::string();
	/* "Book.C.V" -> "Book C:V", the key form every backend parses. */
	const std::string::size_type verse_dot = source_osisref.rfind('.');
	if (verse_dot == std::string::npos || verse_dot == 0)
		return std::string();
	const std::string::size_type chapter_dot =
		source_osisref.rfind('.', verse_dot - 1);
	if (chapter_dot == std::string::npos || chapter_dot == 0)
		return std::string();
	const std::string key =
		source_osisref.substr(0, chapter_dot) + " " +
		source_osisref.substr(chapter_dot + 1,
				      verse_dot - chapter_dot - 1) +
		":" + source_osisref.substr(verse_dot + 1);
	const BibleModuleTransitionPlan plan = planBibleModuleTransition(
		backend, source_module, key, target_module);
	if (plan.status == BibleModuleTransition::SameModule)
		return source_osisref;
	if (plan.status != BibleModuleTransition::Converted)
		return std::string();
	return backend.osisRefFromKey(target_module, plan.key);
}

const char *authorCommentaryForBible(const char *bible)
{
	if (!bible)
		return NULL;
	if (!strcmp(bible, "SpaPlatense"))
		return "SpaPlatenseComentarios";
	if (!strcmp(bible, "NacarColunga"))
		return "NacarColungaNotas";
	if (!strcmp(bible, "TorresAmat"))
		return "TorresAmatNotas";
	return NULL;
}
