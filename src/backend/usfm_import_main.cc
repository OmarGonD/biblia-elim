#include "backend/usfm_importer.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

static void usage(const char *program)
{
	std::fprintf(stderr, "Usage: %s INPUT... --module-id ID --name NAME --language LANG --versification kjv|custom --output FILE [options]\n", program);
}

int main(int argc, char **argv)
{
	UsfmImportOptions options;
	std::vector<std::string> inputs;
	std::string output;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		auto value = [&](std::string &target) { if (i + 1 >= argc) return false; target = argv[++i]; return true; };
		if (arg == "--module-id") { if (!value(options.moduleId)) return 2; }
		else if (arg == "--name") { if (!value(options.name)) return 2; }
		else if (arg == "--language") { if (!value(options.language)) return 2; }
		else if (arg == "--versification") { if (!value(options.versification)) return 2; }
		else if (arg == "--output") { if (!value(output)) return 2; }
		else if (arg == "--abbreviation") { if (!value(options.abbreviation)) return 2; }
		else if (arg == "--description") { if (!value(options.description)) return 2; }
		else if (arg == "--license") { if (!value(options.license)) return 2; }
		else if (arg == "--publisher") { if (!value(options.publisher)) return 2; }
		else if (arg == "--source") { if (!value(options.source)) return 2; }
		else if (arg == "--content-version") { if (!value(options.contentVersion)) return 2; }
		else if (arg == "--help") { usage(argv[0]); return 0; }
		else if (arg.rfind("--", 0) == 0) { std::fprintf(stderr, "Unknown option: %s\n", arg.c_str()); return 2; }
		else inputs.push_back(arg);
	}
	if (inputs.empty() || output.empty()) { usage(argv[0]); return 2; }
	UsfmImportStats stats; std::string error;
	if (!importUsfm(inputs, output, options, stats, error)) { std::fprintf(stderr, "Import failed: %s\n", error.c_str()); return 1; }
	std::printf("Module: %s\nBooks: %zu\nChapters: %zu\nVerses: %zu\nParagraph markers: %zu\nHeadings imported: %zu\nAdded spans: %zu\nWords imported: %zu\nWords with Strong: %zu\nFootnotes skipped: %zu\nCross references skipped: %zu\nUnsupported markers: %zu\nOutput: %s\n", options.moduleId.c_str(), stats.books, stats.chapters, stats.verses, stats.paragraphMarkers, stats.headingsImported, stats.addedSpans, stats.wordsImported, stats.wordsWithStrong, stats.footnotesSkipped, stats.crossReferencesSkipped, stats.unsupportedMarkers, output.c_str());
	return 0;
}
