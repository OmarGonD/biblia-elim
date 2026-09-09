#include <glib.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <utility>

#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/bible_resources.h"
#include "main/strong_interaction.h"

static long long elapsed_us(const std::chrono::steady_clock::time_point &start)
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - start).count();
}

template <typename Function>
static long long benchmark(Function function, int repetitions = 1)
{
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < repetitions; ++i) function();
	return elapsed_us(start);
}

int main(int argc, char **argv)
{
	if (argc != 3 || std::string(argv[1]) != "--backend=sqlite") {
		std::fprintf(stderr, "usage: %s --backend=sqlite MODULE_DIRECTORY\n", argv[0]);
		return 2;
	}
	auto startup_start = std::chrono::steady_clock::now();
	SqliteBibleBackend backend(argv[2]);
	const long long startup = elapsed_us(startup_start);
	const std::string module = backend.listModules().empty()
		? std::string() : backend.listModules().front().id;
	if (module.empty()) {
		std::fprintf(stderr, "no valid SQLite module found\n");
		return 1;
	}
	const BibleModuleCapabilities capabilities = backend.moduleCapabilities(module);
	const BibleReference reference = { 2, 40, 3, 16 };
	BibleSearchQuery query;
	query.mode = BibleSearchMode::Phrase;
	query.text = "world";

	const long long chapters = benchmark([&] {
		(void)backend.getChapter(module, reference, false);
	}, 100);
	const long long verses = benchmark([&] {
		(void)backend.getVerseContent(module, reference);
	}, 1000);
	const long long search = benchmark([&] { (void)backend.search(module, query); });
	StrongId h430{StrongLanguage::Hebrew, 430};
	StrongId g25{StrongLanguage::Greek, 25};
	StrongId mostFrequent{StrongLanguage::Greek, 3588};
	StrongId rare{StrongLanguage::Greek, 100};
	auto strongBenchmark = [&](const StrongId &strong, std::size_t limit,
		std::size_t offset, int repetitions = 1) {
		return benchmark([&] {
			(void)backend.findStrongOccurrences(module, strong, limit, offset);
		}, repetitions);
	};

	std::printf("backend=sqlite startup_us=%lld 100_chapters_us=%lld "
		"1000_verses_us=%lld search_us=%lld feature_strong=%d "
		"feature_morphology=%d\n", startup, chapters, verses, search,
		capabilities.strongs ? 1 : 0, capabilities.morphology ? 1 : 0);
	std::printf("h430_limit_10_us=%lld h430_limit_100_us=%lld "
		"h430_offset_100_limit_100_us=%lld\n",
		strongBenchmark(h430, 10, 0), strongBenchmark(h430, 100, 0),
		strongBenchmark(h430, 100, 100));
	std::printf("g25_limit_10_us=%lld g25_limit_100_us=%lld "
		"most_frequent_limit_100_us=%lld rare_limit_100_us=%lld\n",
		strongBenchmark(g25, 10, 0), strongBenchmark(g25, 100, 0),
		strongBenchmark(mostFrequent, 100, 0), strongBenchmark(rare, 100, 0));
	std::printf("100_h430_limit_100_us=%lld 100_g25_limit_100_us=%lld\n",
		strongBenchmark(h430, 100, 0, 100),
		strongBenchmark(g25, 100, 0, 100));
	BibleApplicationResources resources;
	resources.bible = &backend;
	const BibleReference h430Reference{1, 1, 1, 1};
	const BibleReference g25Reference{2, 40, 5, 43};
	const long long resolveH430 = benchmark([&] {
		(void)resolveAnnotatedWordInteraction(backend, module, h430Reference, 24);
	}, 1000);
	const long long resolveG25 = benchmark([&] {
		(void)resolveAnnotatedWordInteraction(backend, module, g25Reference, 28);
	}, 1000);
	const long long lexiconMissing = benchmark([&] {
		(void)resources.lookupStrong(h430);
	}, 1000);
	const long long firstPage50 = benchmark([&] {
		BibleAnnotatedWord context;
		context.reference = h430Reference;
		context.word = "Dios";
		context.strongs.push_back(h430);
		StrongDetailSession session(backend, resources, module,
			std::move(context), 50);
		(void)session.selectStrong(h430);
	}, 100);
	std::printf("interaction_resolve_h430_1000_us=%lld "
		"interaction_resolve_g25_1000_us=%lld "
		"interaction_missing_lexicon_1000_us=%lld "
		"interaction_first_page_50_100_us=%lld\n",
		resolveH430, resolveG25, lexiconMissing, firstPage50);
	return 0;
}
