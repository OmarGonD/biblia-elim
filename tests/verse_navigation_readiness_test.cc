#include "main/verse_navigation.h"
#include "gui/panel_load_state.h"
#include "fake_bible_backend.h"

#include <iostream>

namespace {
int failures;

#define CHECK(condition) do { \
	if (!(condition)) { \
		std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
			<< ": " #condition "\n"; \
		++failures; \
	} \
} while (0)

void checkReadyAcrossRendererStates(FakeBibleBackend &backend)
{
	PanelLoadModel renderer;
	panel_load_model_init(&renderer);

	auto check = [&] {
		const VerseNavigationResult result = prepareVerseNavigation(
			&backend, "FakeBible", "John 3:16", 1, false);
		CHECK(result.status == VerseNavigationStatus::Ready);
		CHECK(result.source == "John 3:16");
		CHECK(result.target == "John 3:17");
	};

	check(); /* EMPTY */
	PanelLoadToken token = panel_load_model_begin(&renderer);
	check(); /* LOADING */
	CHECK(panel_load_model_ready(&renderer, token));
	check(); /* READY */
	token = panel_load_model_begin(&renderer);
	CHECK(panel_load_model_error(&renderer, token));
	check(); /* ERROR */
}
}

int main()
{
	FakeBibleBackend backend;
	checkReadyAcrossRendererStates(backend);

	CHECK(prepareVerseNavigation(nullptr, "FakeBible", "John 3:16", 1,
		false).status == VerseNavigationStatus::BackendUnavailable);
	CHECK(prepareVerseNavigation(&backend, "", "John 3:16", 1,
		false).status == VerseNavigationStatus::ModuleUnavailable);
	CHECK(prepareVerseNavigation(&backend, "FakeDictionary", "John 3:16", 1,
		false).status == VerseNavigationStatus::ModuleUnavailable);
	CHECK(prepareVerseNavigation(&backend, "FakeBible", "invalid", 1,
		false).status == VerseNavigationStatus::ReferenceInvalid);
	CHECK(prepareVerseNavigation(&backend, "FakeBible", "John 3:16", 1,
		true).status == VerseNavigationStatus::InterlinearLocked);

	/* Three immediate NEXT requests use the accepted target as the next
	 * source and never consult or wait for renderer completion. The
	 * fixture has three verses, so the third press reports the end of
	 * the Bible instead of "moving" to the verse it is already on. */
	std::string key = "Genesis 1:1";
	for (int click = 0; click < 3; ++click) {
		const VerseNavigationResult result = prepareVerseNavigation(
			&backend, "FakeBible", key, 1, false);
		if (click < 2) {
			CHECK(result.status == VerseNavigationStatus::Ready);
			CHECK(!result.target.empty());
			key = result.target;
		} else {
			CHECK(result.status == VerseNavigationStatus::EndOfBible);
			CHECK(result.target.empty());
		}
	}
	CHECK(key == "John 3:17");

	/* stepVerse: one native slot, or the next one the predicate accepts;
	 * the ends of the versification are explicit, not "same key". */
	VerseStep step = stepVerse(backend, "FakeBible", "Genesis 1:1", 1);
	CHECK(step.status == VerseStepStatus::Valid);
	CHECK(step.key == "John 3:16");
	step = stepVerse(backend, "FakeBible", "John 3:16", -1);
	CHECK(step.status == VerseStepStatus::Valid);
	CHECK(step.key == "Genesis 1:1");
	step = stepVerse(backend, "FakeBible", "Genesis 1:1", -1);
	CHECK(step.status == VerseStepStatus::BeginningOfBible);
	CHECK(step.key.empty());
	step = stepVerse(backend, "FakeBible", "John 3:17", 1);
	CHECK(step.status == VerseStepStatus::EndOfBible);
	CHECK(step.key.empty());
	CHECK(stepVerse(backend, "FakeBible", "invalid", 1).status ==
		VerseStepStatus::Invalid);
	CHECK(stepVerse(backend, "FakeBible", "John 3:16", 0).status ==
		VerseStepStatus::Invalid);

	const VerseSlotNavigable skip_john_3_16 = [](const BibleReference &ref) {
		return !(ref.chapter == 3 && ref.verse == 16);
	};
	step = stepVerse(backend, "FakeBible", "Genesis 1:1", 1, skip_john_3_16);
	CHECK(step.status == VerseStepStatus::Valid);
	CHECK(step.key == "John 3:17");
	step = stepVerse(backend, "FakeBible", "John 3:17", -1, skip_john_3_16);
	CHECK(step.key == "Genesis 1:1");
	const VerseSlotNavigable nothing = [](const BibleReference &) {
		return false;
	};
	CHECK(stepVerse(backend, "FakeBible", "Genesis 1:1", 1, nothing).status ==
		VerseStepStatus::EndOfBible);

	CHECK(prepareVerseNavigation(&backend, "FakeBible", "Genesis 1:1", -1,
		false).status == VerseNavigationStatus::BeginningOfBible);
	CHECK(prepareVerseNavigation(&backend, "FakeBible", "John 3:17", 1,
		false).status == VerseNavigationStatus::EndOfBible);
	const VerseNavigationResult skipped = prepareVerseNavigation(&backend,
		"FakeBible", "Genesis 1:1", 1, false, skip_john_3_16);
	CHECK(skipped.status == VerseNavigationStatus::Ready);
	CHECK(skipped.target == "John 3:17");

	std::cout << "verse_navigation_readiness_failures=" << failures << '\n';
	return failures ? 1 : 0;
}
