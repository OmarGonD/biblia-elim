/*
 * Biblia Elim - carrying a reference from one module to another.
 */
#include "main/reference_transition.h"

#include <cstring>

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

/* Platense is normalized at install time to SpaPlatenseComentarios; the
 * two OCR editions publish their notes as separate zCom modules. Each
 * declares the same versification as its edition in its .conf; callers
 * still convert, so a mismatch would be mapped rather than misread. */
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
