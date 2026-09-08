#include "backend/osis_importer.h"

#include <chrono>
#include <cstdio>
#include <map>
#include <string>

namespace {

std::size_t total(const std::map<std::string, std::size_t> &counts)
{
	std::size_t result = 0;
	for (const auto &entry : counts) result += entry.second;
	return result;
}

void printDetails(const char *title,
	const std::map<std::string, std::size_t> &counts)
{
	if (counts.empty()) return;
	std::printf("%s:\n", title);
	std::size_t shown = 0;
	for (const auto &entry : counts) {
		if (shown++ == 10) {
			std::printf("  ... %zu more keys\n", counts.size() - 10);
			break;
		}
		std::printf("  %s = %zu\n", entry.first.c_str(), entry.second);
	}
}

} // namespace

int main(int argc, char **argv)
{
	if (argc < 10) {
		std::fprintf(stderr, "Usage: %s bible.xml --module-id ID --name NAME "
			"--language LANG --versification kjv|custom --output FILE\n", argv[0]);
		return 2;
	}
	UsfmImportOptions options;
	std::string output;
	for (int index = 2; index + 1 < argc; index += 2) {
		const std::string key = argv[index];
		const std::string value = argv[index + 1];
		if (key == "--module-id") options.moduleId = value;
		else if (key == "--name") options.name = value;
		else if (key == "--language") options.language = value;
		else if (key == "--versification") options.versification = value;
		else if (key == "--output") output = value;
	}
	UsfmImportStats stats;
	std::string error;
	const auto started = std::chrono::steady_clock::now();
	if (!importOsis(argv[1], output, options, stats, error)) {
		std::fprintf(stderr, "OSIS import failed: %s\n", error.c_str());
		return 1;
	}
	const double seconds = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - started).count();

	std::printf("Module: %s\nBooks: %zu\nChapters: %zu\nVerses: %zu\n\n"
		"Container verses: %zu\nMilestone verses: %zu\n\n"
		"Strong words: %zu\nStrong IDs: %zu\nFootnotes: %zu\nCross references: %zu\n\n"
		"Unknown inline elements: %zu\nUnknown structural elements: %zu\n"
		"Ignored attributes: %zu\nMorph attributes: %zu\nNon-Strong lemmas: %zu\n"
		"Range references: %zu\nUnresolved targets: %zu\n\nImport time: %.3f s\nOutput: %s\n",
		options.moduleId.c_str(), stats.books, stats.chapters, stats.verses,
		stats.containerVerses, stats.milestoneVerses, stats.wordsWithStrong,
		stats.strongIdsImported, stats.footnotesImported,
		stats.crossReferencesImported, total(stats.unknownInlineElements),
		total(stats.unknownStructuralElements), total(stats.ignoredAttributes),
		total(stats.morphAttributes), total(stats.nonStrongLemmaAttributes),
		total(stats.rangeReferences), total(stats.unresolvedReferences), seconds,
		output.c_str());
	printDetails("Unknown inline element keys", stats.unknownInlineElements);
	printDetails("Unknown structural element keys", stats.unknownStructuralElements);
	printDetails("Ignored attribute keys", stats.ignoredAttributes);
	printDetails("Morph schemes", stats.morphSchemes);
	printDetails("Non-Strong lemma schemes", stats.nonStrongLemmaAttributes);
	printDetails("Range keys", stats.rangeReferences);
	printDetails("Unresolved target keys", stats.unresolvedReferences);
	return 0;
}
