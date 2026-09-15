/* Backend-ready verse navigation, independent of renderer lifecycle state. */
#ifndef XIPHOS_VERSE_NAVIGATION_H
#define XIPHOS_VERSE_NAVIGATION_H

#include <functional>
#include <string>

#include "backend/bible_types.h"

class BibleBackend;

enum class VerseNavigationStatus {
	Ready,
	InterlinearLocked,
	BackendUnavailable,
	ModuleUnavailable,
	ReferenceInvalid,
	TargetUnavailable,
	BeginningOfBible,
	EndOfBible
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

/* Whether a slot, native to the module, is one the reader can land on. */
using VerseSlotNavigable = std::function<bool(const BibleReference &)>;

enum class VerseStepStatus {
	Valid,
	BeginningOfBible,
	EndOfBible,
	Invalid
};

struct VerseStep {
	VerseStepStatus status = VerseStepStatus::Invalid;
	std::string key;
	BibleReference reference;
};

/* The single next/previous verse operation. Walks the module's own
 * versification one slot at a time -- across chapters and books, never
 * assuming KJV -- and stops at the first slot with chapter and verse >= 1
 * that `navigable` accepts (all of them when empty). Verse 0 (chapter
 * headings and intros) is never a stop. At either end of the
 * versification, or past the last navigable slot, the result is
 * BeginningOfBible / EndOfBible and no key. */
VerseStep stepVerse(BibleBackend &backend, const std::string &module,
	const std::string &key, int direction,
	const VerseSlotNavigable &navigable = VerseSlotNavigable());

/* The pane's own rule: a slot is navigable when the content resolver
 * gives it a usable body -- its own text or the fallback's. A slot filled
 * by fallback is as navigable as any other; one that stays empty is not
 * rendered, so there is nothing to land on. */
bool verseSlotNavigable(BibleBackend &backend, const std::string &module,
	const BibleReference &reference);

VerseNavigationResult prepareVerseNavigation(BibleBackend *backend,
	const std::string &module, const std::string &key, int direction,
	bool interlinear_locked,
	const VerseSlotNavigable &navigable = VerseSlotNavigable());
const char *verseNavigationStatusName(VerseNavigationStatus status);

#endif /* XIPHOS_VERSE_NAVIGATION_H */
