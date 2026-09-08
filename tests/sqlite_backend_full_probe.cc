#include <chrono>
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include <regex>
#include <sstream>

#include "backend/sqlite/sqlite_bible_backend.h"

int main(int argc, char **argv)
{
	if (argc != 3) return 2;
	SqliteBibleBackend backend(argv[1]);
	const std::string module = argv[2];
	std::printf("modules=%zu\n", backend.listModules().size());
	const std::vector<std::string> references = {
		"Gen 1:1", "Ps 23:1", "Isa 53:5", "Matt 5:1",
		"John 3:16", "Rom 8:28", "Rev 22:21" };
	for (const auto &reference : references) {
		BibleKeyInfo info;
		const bool resolved = backend.resolveKey(module, reference, info);
		std::printf("reference=%s resolved=%d", reference.c_str(), resolved ? 1 : 0);
		if (resolved) {
			BibleVerseContent content = backend.getVerseContent(module, info.reference);
			std::printf(" osis=%s nonempty=%d next=%s prev=%s", backend.osisRefFromKey(module, reference).c_str(), content.valid && !content.plainText.empty() ? 1 : 0,
				backend.navigate(module, reference, 1).c_str(), backend.navigate(module, reference, -1).c_str());
		}
		std::printf("\n");
	}
	auto walk = [&](const std::string &start, int direction) {
		std::set<std::string> seen;
		std::string current = start;
		const auto begin = std::chrono::steady_clock::now();
		while (true) {
			if (!seen.insert(current).second) return std::make_pair(seen.size(), -1LL);
			std::string next = backend.navigate(module, current, direction);
			if (next == current || next.empty()) break;
			current = next;
			if (seen.size() > 40000) return std::make_pair(seen.size(), -2LL);
		}
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count();
		return std::make_pair(seen.size(), static_cast<long long>(elapsed));
	};
	auto forward = walk("Gen 1:1", 1);
	auto backward = walk("Rev 22:21", -1);
	std::printf("forward_count=%zu forward_us=%lld backward_count=%zu backward_us=%lld\n", forward.first, forward.second, backward.first, backward.second);
	std::string current = "Gen 1:1";
	std::size_t checked = 0, mismatches = 0, invalidRanges = 0, invalidStrong = 0, withStrong = 0, hebrew = 0, greek = 0, maxWords = 0;
	std::set<std::string> strongH, strongG;
	const std::regex strongPattern("^(H|G)[0-9]+$");
	while (true) {
		BibleKeyInfo info;
		if (!backend.resolveKey(module, current, info)) break;
		BibleVerseContent content = backend.getVerseContent(module, info.reference);
		maxWords = std::max(maxWords, content.words.size());
		for (const BibleWordInfo &word : content.words) {
			++checked;
			if (word.length == 0 || word.start + word.length > content.plainText.size()) ++invalidRanges;
			else if (content.plainText.substr(word.start, word.length) != word.text) ++mismatches;
			if (!word.strong.empty()) {
				++withStrong;
				std::stringstream values(word.strong); std::string value; bool valid = true;
				while (std::getline(values, value, ',')) {
					if (!std::regex_match(value, strongPattern)) { valid = false; break; }
					if (value[0] == 'H') { ++hebrew; strongH.insert(value); }
					else { ++greek; strongG.insert(value); }
				}
				if (!valid) ++invalidStrong;
			}
		}
		std::string next = backend.navigate(module, current, 1);
		if (next == current || next.empty()) break;
		current = next;
	}
	std::printf("words_checked=%zu offset_mismatches=%zu invalid_ranges=%zu invalid_strong=%zu words_with_strong=%zu strong_H=%zu strong_G=%zu unique_H=%zu unique_G=%zu max_words_per_verse=%zu\n", checked, mismatches, invalidRanges, invalidStrong, withStrong, hebrew, greek, strongH.size(), strongG.size(), maxWords);
	const std::vector<std::string> terms = {"Dios", "Señor", "Israel", "que", "de"};
	for (const auto &term : terms) {
		BibleSearchQuery query; query.mode = BibleSearchMode::MultiWord; query.text = term;
		const auto begin = std::chrono::steady_clock::now();
		auto results = backend.search(module, query);
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count();
		std::printf("search=%s results=%zu us=%lld\n", term.c_str(), results.size(), elapsed);
	}
	const std::vector<std::string> utf8Terms = {"á", "é", "í", "ó", "ú", "ñ", "ü", "¿", "¡"};
	for (const auto &term : utf8Terms) {
		BibleSearchQuery query; query.mode = BibleSearchMode::MultiWord; query.text = term;
		std::printf("utf8=%s results=%zu\n", term.c_str(), backend.search(module, query).size());
	}
	BibleKeyInfo utf8Key;
	bool utf8Content = backend.resolveKey(module, "Ps 23:1", utf8Key);
	if (utf8Content) {
		const std::string verse = backend.getVerseContent(module, utf8Key.reference).plainText;
		utf8Content = verse.find("JEHOVÁ") != std::string::npos;
	}
	std::printf("utf8_content_jehova=%d\n", utf8Content ? 1 : 0);
	const BibleReference reference = { 2, 43, 3, 16 };
	(void)backend.getChapter(module, reference, false);
	(void)backend.getVerseContent(module, reference);
	const auto warm = std::chrono::steady_clock::now();
	for (int i = 0; i < 100; ++i) (void)backend.getChapter(module, reference, false);
	const auto chapters = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - warm).count();
	const auto warm2 = std::chrono::steady_clock::now();
	for (int i = 0; i < 1000; ++i) (void)backend.getVerseContent(module, reference);
	const auto verses = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - warm2).count();
	const auto warm3 = std::chrono::steady_clock::now();
	for (const auto &term : terms) { BibleSearchQuery query; query.mode = BibleSearchMode::MultiWord; query.text = term; for (int i = 0; i < 20; ++i) (void)backend.search(module, query); }
	const auto searches = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - warm3).count();
	std::printf("100_chapters_us=%lld 1000_verses_us=%lld 100_searches_us=%lld\n", chapters, verses, searches);
	return 0;
}
