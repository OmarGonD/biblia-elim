/* Backend-ready verse navigation, independent of renderer lifecycle state. */
#ifndef XIPHOS_VERSE_NAVIGATION_H
#define XIPHOS_VERSE_NAVIGATION_H

#include <string>

class BibleBackend;

enum class VerseNavigationStatus {
	Ready,
	InterlinearLocked,
	BackendUnavailable,
	ModuleUnavailable,
	ReferenceInvalid,
	TargetUnavailable
};

struct VerseNavigationResult {
	VerseNavigationStatus status = VerseNavigationStatus::BackendUnavailable;
	std::string source;
	std::string target;

	explicit operator bool() const
	{
		return status == VerseNavigationStatus::Ready;
	}
};

VerseNavigationResult prepareVerseNavigation(BibleBackend *backend,
	const std::string &module, const std::string &key, int direction,
	bool interlinear_locked);
const char *verseNavigationStatusName(VerseNavigationStatus status);

#endif /* XIPHOS_VERSE_NAVIGATION_H */
