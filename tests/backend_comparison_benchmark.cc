#include <glib.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <sys/resource.h>
#include <thread>
#include <utility>
#include <vector>

#include "backend/bible_backend.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/sword/sword_backend.h"
#include "main/busqueda_tildes.h"
#include "main/settings.h"

SETTINGS settings = {};
char *sword_locale = nullptr;

extern "C" void main_dialog_search_percent_update(char, void *) {}
extern "C" void main_sidebar_search_percent_update(char, void *) {}
extern "C" void main_index_percent_update(char, void *) {}
extern "C" void main_setup_displays(void) {}
extern "C" void main_clear_abbreviations(void) {}
extern "C" void main_add_abbreviation(const char *, const char *) {}
extern "C" int main_is_module(char *) { return 0; }
extern "C" void gui_generic_warning(const char *) {}
extern "C" const char *main_get_language_map(const char *language)
{
	return language;
}
extern "C" char *main_get_mod_config_file(const char *, const char *)
{
	return nullptr;
}
extern "C" char *main_format_number(int value)
{
	return g_strdup_printf("%d", value);
}
extern "C" gchar *XI_g_strdup_printf(const char *, int, const gchar *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	gchar *result = g_strdup_vprintf(format, arguments);
	va_end(arguments);
	return result;
}

namespace {
using Clock = std::chrono::steady_clock;

long long elapsedUs(Clock::time_point start)
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
		Clock::now() - start).count();
}

template <class Function>
long long measure(Function function)
{
	const auto start = Clock::now();
	function();
	return elapsedUs(start);
}

long long cpuUs(const rusage &before, const rusage &after)
{
	auto micros = [](const timeval &value) {
		return static_cast<long long>(value.tv_sec) * 1000000 + value.tv_usec;
	};
	return micros(after.ru_utime) + micros(after.ru_stime) -
		micros(before.ru_utime) - micros(before.ru_stime);
}

BibleReference resolve(BibleBackend &backend, const std::string &module,
	const std::string &key)
{
	BibleKeyInfo info;
	if (!backend.resolveKey(module, key, info)) {
		std::fprintf(stderr, "cannot resolve %s\n", key.c_str());
		std::exit(3);
	}
	return info.reference;
}
}

int main(int argc, char **argv)
{
	if (argc < 2) return 2;
	const std::string choice = argv[1];
	int holdMs = 0;
	for (int i = 2; i < argc; ++i) {
		const std::string option = argv[i];
		if (option.rfind("--hold-ms=", 0) == 0)
			holdMs = std::atoi(option.c_str() + 10);
	}
	std::unique_ptr<BibleBackend> backend;
	std::string module;
	const auto initStart = Clock::now();
	if (choice == "--backend=sword") {
		backend.reset(new SwordBackend());
		module = "SpaRV1909";
	} else if (choice == "--backend=sqlite" && argc >= 3) {
		backend.reset(new SqliteBibleBackend(argv[2]));
		module = "rv1909";
	} else {
		return 2;
	}
	const long long init = elapsedUs(initStart);
	if (!backend->hasModule(module)) {
		std::fprintf(stderr, "module %s unavailable\n", module.c_str());
		return 4;
	}

	const std::string genesis = choice == "--backend=sword"
		? "Genesis 1:1" : "Génesis 1:1";
	const std::string psalms = choice == "--backend=sword"
		? "Psalms 23:1" : "Salmos 23:1";
	const std::string matthew = choice == "--backend=sword"
		? "Matthew 5:1" : "San Mateo 5:1";
	const std::string john = choice == "--backend=sword"
		? "John 3:16" : "Juan 3:16";
	const std::vector<BibleReference> references = {
		resolve(*backend, module, genesis), resolve(*backend, module, psalms),
		resolve(*backend, module, matthew), resolve(*backend, module, john)};
	volatile std::size_t sink = 0;
	rusage cpuBefore{}, cpuAfter{};
	getrusage(RUSAGE_SELF, &cpuBefore);

	const long long chapters = measure([&] {
		for (int i = 0; i < 100; ++i)
			sink += backend->getChapter(module, references[i % 4], true).size();
	});
	const long long verses = measure([&] {
		for (int i = 0; i < 1000; ++i) {
			BibleReference ref = references[i % 4];
			ref.verse = 1 + (i % 5);
			sink += backend->getVerseContent(module, ref).renderedText.size();
		}
	});
	const long long navigation = measure([&] {
		std::string key = genesis;
		for (int i = 0; i < 1000; ++i) {
			key = backend->navigate(module, key, 1);
			sink += key.size();
		}
	});
	const long long chapterChanges = measure([&] {
		for (int i = 0; i < 100; ++i) {
			const int chapter = 1 + (i % 50);
			const std::string key = backend->setChapter(
				module, genesis, chapter);
			BibleReference ref = resolve(*backend, module, key);
			sink += backend->getChapter(module, ref, true).size();
		}
	});

	const char *terms[] = {"Dios", "Señor", "Israel", "que", "de"};
	long long searches[5] = {};
	std::size_t searchCounts[5] = {};
	for (int i = 0; i < 5; ++i) {
		BibleSearchQuery query;
		query.text = terms[i];
		query.mode = BibleSearchMode::Phrase;
		query.limit = 100;
		searches[i] = measure([&] {
			auto results = backend->search(module, query);
			searchCounts[i] = results.size();
			sink += results.size();
		});
	}
	getrusage(RUSAGE_SELF, &cpuAfter);

	BibleAnnotatedWord strong;
	const bool strongResolved = backend->resolveAnnotatedWord(module,
		references[0], 24, strong) && !strong.strongs.empty();
	const BibleModuleCapabilities capabilities = backend->moduleCapabilities(module);
	std::printf("backend=%s module=%s init_us=%lld chapters100_us=%lld "
		"verses1000_us=%lld navigate1000_us=%lld chapter_changes100_us=%lld "
		"cpu_workload_us=%lld strong_capability=%d strong_resolved=%d sink=%zu\n",
		choice == "--backend=sword" ? "sword" : "sqlite", module.c_str(),
		init, chapters, verses, navigation, chapterChanges,
		cpuUs(cpuBefore, cpuAfter), capabilities.strongs ? 1 : 0,
		strongResolved ? 1 : 0, static_cast<std::size_t>(sink));
	for (int i = 0; i < 5; ++i)
		std::printf("search_%s_us=%lld search_%s_count=%zu%c", terms[i],
			searches[i], terms[i], searchCounts[i], i == 4 ? '\n' : ' ');
	if (holdMs > 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
	return 0;
}
