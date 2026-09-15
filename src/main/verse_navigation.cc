#include "main/verse_navigation.h"

#include "backend/bible_backend.h"
#include "backend/content_availability.h"
#include "backend/content_resolver.h"

namespace {

/* Past the last navigable slot of a module there can be whole books the
 * versification lists and the module lacks (Vulg's appendix after
 * Revelation, NRSVA's deuterocanon in a Protestant Bible). Walking them
 * is bounded by the versification itself; this is only a guard against a
 * backend that never reports its end. */
const int kMaxSlotsWalked = 40000;

bool sameReference(const BibleReference &a, const BibleReference &b)
{
	return a.testament == b.testament && a.book == b.book &&
	       a.chapter == b.chapter && a.verse == b.verse;
}

}

VerseStep stepVerse(BibleBackend &backend, const std::string &module,
	const std::string &key, int direction, const VerseSlotNavigable &navigable)
{
	VerseStep step;
	BibleKeyInfo current;
	if (direction == 0 || module.empty() || key.empty() ||
	    !backend.resolveKey(module, key, current))
		return step;

	const int sign = direction < 0 ? -1 : 1;
	const VerseStepStatus edge = sign < 0 ? VerseStepStatus::BeginningOfBible
					      : VerseStepStatus::EndOfBible;
	for (int walked = 0; walked < kMaxSlotsWalked; ++walked) {
		/* Step from the resolved position, not by re-reading key text. */
		BibleKeyInfo info;
		if (!backend.navigateFrom(module, current, sign, info))
			return step;
		/* Backends answer the same slot at either end. */
		if (sameReference(info.reference, current.reference)) {
			step.status = edge;
			return step;
		}
		current = info;
		if (current.reference.chapter < 1 || current.reference.verse < 1)
			continue;
		if (!navigable || navigable(current.reference)) {
			/* The key goes on as text -- navbar, display, settings
			 * -- so it has to name this very slot. A key that reads
			 * back as another verse would take the reader to
			 * another book; better not to move. */
			BibleKeyInfo check;
			if (!backend.resolveKey(module, current.key, check) ||
			    !sameReference(check.reference, current.reference))
				return step;
			step.status = VerseStepStatus::Valid;
			step.key = current.key;
			step.reference = current.reference;
			return step;
		}
	}
	return step;
}

bool verseSlotNavigable(BibleBackend &backend, const std::string &module,
	const BibleReference &reference)
{
	if (reference.chapter < 1 || reference.verse < 1)
		return false;
	const BibleVerseContent content =
		resolveVerseContent(backend, module, reference);
	return classifyVerseContent(content, reference) ==
	       ContentAvailability::Available;
}

VerseNavigationResult prepareVerseNavigation(BibleBackend *backend,
	const std::string &module, const std::string &key, int direction,
	bool interlinear_locked, const VerseSlotNavigable &navigable)
{
	VerseNavigationResult result;
	if (interlinear_locked) {
		result.status = VerseNavigationStatus::InterlinearLocked;
		return result;
	}
	if (!backend) {
		result.status = VerseNavigationStatus::BackendUnavailable;
		return result;
	}
	if (module.empty() || !backend->hasModule(module) ||
	    backend->moduleType(module) != BibleModuleType::Bible) {
		result.status = VerseNavigationStatus::ModuleUnavailable;
		return result;
	}

	BibleKeyInfo info;
	if (key.empty() || !backend->resolveKey(module, key, info)) {
		result.status = VerseNavigationStatus::ReferenceInvalid;
		return result;
	}
	result.source = info.key;
	const VerseStep step = stepVerse(*backend, module, info.key,
		direction < 0 ? -1 : 1, navigable);
	switch (step.status) {
	case VerseStepStatus::Valid:
		result.target = step.key;
		result.status = VerseNavigationStatus::Ready;
		break;
	case VerseStepStatus::BeginningOfBible:
		result.status = VerseNavigationStatus::BeginningOfBible;
		break;
	case VerseStepStatus::EndOfBible:
		result.status = VerseNavigationStatus::EndOfBible;
		break;
	case VerseStepStatus::Invalid:
		result.status = VerseNavigationStatus::TargetUnavailable;
		break;
	}
	return result;
}

const char *verseNavigationStatusName(VerseNavigationStatus status)
{
	switch (status) {
	case VerseNavigationStatus::Ready:
		return "ready";
	case VerseNavigationStatus::InterlinearLocked:
		return "interlinear-lock";
	case VerseNavigationStatus::BackendUnavailable:
		return "backend-unavailable";
	case VerseNavigationStatus::ModuleUnavailable:
		return "module-unavailable";
	case VerseNavigationStatus::ReferenceInvalid:
		return "reference-invalid";
	case VerseNavigationStatus::TargetUnavailable:
		return "target-unavailable";
	case VerseNavigationStatus::BeginningOfBible:
		return "beginning-of-bible";
	case VerseNavigationStatus::EndOfBible:
		return "end-of-bible";
	}
	return "unknown";
}
