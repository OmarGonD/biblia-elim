#include "main/verse_navigation.h"

#include "backend/bible_backend.h"

VerseNavigationResult prepareVerseNavigation(BibleBackend *backend,
	const std::string &module, const std::string &key, int direction,
	bool interlinear_locked)
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
	result.target = backend->navigate(module, info.key, direction < 0 ? -1 : 1);
	if (result.target.empty()) {
		result.status = VerseNavigationStatus::TargetUnavailable;
		return result;
	}
	result.status = VerseNavigationStatus::Ready;
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
	}
	return "unknown";
}
