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
	 * source and never consult or wait for renderer completion. */
	std::string key = "Genesis 1:1";
	for (int click = 0; click < 3; ++click) {
		const VerseNavigationResult result = prepareVerseNavigation(
			&backend, "FakeBible", key, 1, false);
		CHECK(result.status == VerseNavigationStatus::Ready);
		CHECK(!result.target.empty());
		key = result.target;
	}
	CHECK(key == "John 3:17");

	std::cout << "verse_navigation_readiness_failures=" << failures << '\n';
	return failures ? 1 : 0;
}
