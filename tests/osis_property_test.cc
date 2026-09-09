#include "backend/osis_importer.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "neutral_module_comparator.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kSeed = 0x05A5202u;
constexpr std::size_t kCaseCount = 24;
constexpr double kTimeLimitSeconds = 20.0;

struct Atom {
	std::string xml;
	bool punctuation = false;
};

struct GeneratedCase {
	std::string compact;
	std::string pretty;
	std::string malformed;
};

bool writeFile(const std::string &path, const std::string &contents)
{
	return g_file_set_contents(path.c_str(), contents.c_str(),
		static_cast<gssize>(contents.size()), nullptr);
}

UsfmImportOptions options(const std::string &module)
{
	UsfmImportOptions result;
	result.moduleId = module;
	result.name = "Generated OSIS property fixture";
	result.language = "und";
	result.versification = "custom";
	result.description = "Fixed-seed generated mixed content";
	return result;
}

std::size_t pick(std::mt19937 &random, std::size_t upperBound)
{
	return static_cast<std::size_t>(random()) % upperBound;
}

Atom generateAtom(std::mt19937 &random, int forcedKind = -1)
{
	static const std::vector<std::string> words = {
		"amor", "Jesús", "λόγος", "שָׁלוֹם", "世界", "नमस्ते", "é"
	};
	static const std::vector<std::string> punctuation = {
		",", ".", ";", "?", "!", "؟", "。"
	};
	const std::string &word = words[pick(random, words.size())];
	const std::size_t kind = forcedKind < 0 ? pick(random, 7) :
		static_cast<std::size_t>(forcedKind);
	switch (kind) {
	case 0:
		return { word, false };
	case 1:
		return { "<w lemma=\"strong:G25\">" + word + "</w>", false };
	case 2:
		return { "<hi>" + word + "</hi>", false };
	case 3:
		return { "<![CDATA[" + word + "]]>", false };
	case 4:
		return { "<note type=\"footnote\" n=\"*\">Nota <hi>" + word +
			"</hi>.</note>", false };
	case 5:
		return { "<note type=\"crossReference\">Véase <hi><reference "
			"osisRef=\"Gen.1.1 John.3.16\">Génesis 1:1; Juan 3:16</reference>"
			"</hi>.</note>", false };
	default:
		return { punctuation[pick(random, punctuation.size())], true };
	}
}

std::string renderBody(const std::vector<Atom> &atoms, bool pretty)
{
	std::string result;
	for (std::size_t index = 0; index < atoms.size(); ++index) {
		if (index != 0) {
			if (pretty) result += "\n        ";
			else if (!atoms[index].punctuation) result += ' ';
		}
		result += atoms[index].xml;
	}
	return result;
}

std::string containerDocument(const std::string &body, bool pretty)
{
	if (!pretty)
		return "<osis><osisText><chapter osisID=\"John.3\"><verse "
			"osisID=\"John.3.16\">" + body +
			"</verse></chapter></osisText></osis>";
	return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<osis>\n  <osisText>\n    <chapter osisID=\"John.3\">\n"
		"      <verse osisID=\"John.3.16\">\n        " + body +
		"\n      </verse>\n    </chapter>\n  </osisText>\n</osis>\n";
}

std::string malformedDocument(const std::string &body)
{
	return "<osis><osisText><chapter sID=\"John.3\"/><verse "
		"sID=\"John.3.16\"/>" + body +
		"<chapter eID=\"John.3\"/></osisText></osis>";
}

GeneratedCase generateCase(std::mt19937 &random, std::size_t caseIndex)
{
	const std::size_t count = 5 + pick(random, 5);
	const std::size_t forcedWord = pick(random, count);
	const std::size_t forcedKind = (forcedWord + 1) % count;
	std::vector<Atom> atoms;
	for (std::size_t index = 0; index < count; ++index) {
		int kind = -1;
		if (index == forcedWord) kind = 1;
		else if (index == forcedKind) kind = static_cast<int>(caseIndex % 7);
		atoms.push_back(generateAtom(random, kind));
	}
	const std::string compactBody = renderBody(atoms, false);
	const std::string prettyBody = renderBody(atoms, true);
	return { containerDocument(compactBody, false),
		containerDocument(prettyBody, true), malformedDocument(compactBody) };
}

void printReproducer(std::size_t index, const GeneratedCase &test,
	const std::string &reason)
{
	std::cerr << "property_failure=" << reason << '\n'
		<< "seed=" << kSeed << '\n'
		<< "case_index=" << index << '\n'
		<< "compact_xml=" << test.compact << '\n'
		<< "pretty_xml=" << test.pretty << '\n'
		<< "malformed_xml=" << test.malformed << '\n';
}

} // namespace

int main()
{
	const auto started = std::chrono::steady_clock::now();
	gchar *temporary = g_dir_make_tmp("osis-property-XXXXXX", nullptr);
	if (!temporary) return 2;
	const std::string directory = temporary;
	std::mt19937 random(kSeed);
	std::size_t failures = 0;
	std::size_t validImports = 0;
	std::size_t rejectedImports = 0;
	std::size_t offsetRows = 0;

	for (std::size_t index = 0; index < kCaseCount; ++index) {
		const GeneratedCase test = generateCase(random, index);
		const std::string prefix = "property-" + std::to_string(index);
		const std::string compactInput = directory + "/" + prefix + "-compact.xml";
		const std::string prettyInput = directory + "/" + prefix + "-pretty.xml";
		const std::string malformedInput = directory + "/" + prefix + "-malformed.xml";
		bool caseValid = writeFile(compactInput, test.compact) &&
			writeFile(prettyInput, test.pretty) && writeFile(malformedInput, test.malformed);
		if (!caseValid) {
			printReproducer(index, test, "input write failed");
			++failures;
			continue;
		}

		const std::vector<std::pair<std::string, std::string>> valid = {
			{ compactInput, prefix + "-compact-a" },
			{ compactInput, prefix + "-compact-b" },
			{ prettyInput, prefix + "-pretty" },
		};
		for (const auto &inputAndModule : valid) {
			UsfmImportStats stats;
			std::string error;
			const std::string output = directory + "/" + inputAndModule.second + ".sqlite";
			if (!importOsis(inputAndModule.first, output,
				options(inputAndModule.second), stats, error)) {
				caseValid = false;
				printReproducer(index, test, "valid import failed: " + error);
			} else {
				++validImports;
			}
		}

		if (caseValid) {
			SqliteBibleBackend backend(directory);
			const std::string compactA = prefix + "-compact-a";
			const std::string compactB = prefix + "-compact-b";
			const std::string pretty = prefix + "-pretty";
			const std::size_t mismatches =
				compareNeutralModules(backend, compactA, backend, compactB).mismatches +
				compareNeutralModules(backend, compactA, backend, pretty).mismatches;
			std::size_t invalidOffsets = 0;
			std::size_t substringMismatches = 0;
			for (const std::string &module : { compactA, compactB, pretty }) {
				const NeutralOffsetValidationResult validation =
					validateNeutralOffsets(backend, module);
				offsetRows += validation.rowsChecked;
				invalidOffsets += validation.invalidOffsets;
				substringMismatches += validation.substringMismatches;
			}
			if (mismatches != 0 || invalidOffsets != 0 || substringMismatches != 0) {
				caseValid = false;
				printReproducer(index, test, "determinism/equivalence/offset mismatch");
			}
		}

		for (int attempt = 0; attempt < 2; ++attempt) {
			const std::string module = prefix + "-rejected-" + std::to_string(attempt);
			const std::string output = directory + "/" + module + ".sqlite";
			UsfmImportStats stats;
			std::string error;
			const bool accepted = importOsis(malformedInput, output,
				options(module), stats, error);
			const bool atomic = !g_file_test(output.c_str(), G_FILE_TEST_EXISTS) &&
				!g_file_test((output + ".tmp").c_str(), G_FILE_TEST_EXISTS);
			if (accepted || error != "EOF with open verse milestone" || !atomic) {
				caseValid = false;
				printReproducer(index, test, "malformed import was not deterministic and atomic");
			} else {
				++rejectedImports;
			}
		}
		if (!caseValid) ++failures;
	}

	const double elapsedSeconds = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - started).count();
	if (elapsedSeconds > kTimeLimitSeconds) {
		++failures;
		std::cerr << "property_failure=time bound exceeded\nseed=" << kSeed
			<< "\nelapsed_seconds=" << elapsedSeconds
			<< "\ntime_limit_seconds=" << kTimeLimitSeconds << '\n';
	}

	std::cout << "property_seed=" << kSeed << '\n'
		<< "property_cases=" << kCaseCount << '\n'
		<< "covered_atom_kinds=7\n"
		<< "valid_imports=" << validImports << '\n'
		<< "rejected_imports=" << rejectedImports << '\n'
		<< "offset_rows_checked=" << offsetRows << '\n'
		<< "time_limit_seconds=" << kTimeLimitSeconds << '\n'
		<< "elapsed_seconds=" << elapsedSeconds << '\n'
		<< "property_failures=" << failures << '\n';
	g_free(temporary);
	return failures == 0 ? 0 : 1;
}
