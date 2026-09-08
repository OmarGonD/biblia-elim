#ifndef XIPHOS_TESTS_NEUTRAL_MODULE_COMPARATOR_H
#define XIPHOS_TESTS_NEUTRAL_MODULE_COMPARATOR_H

#include <cstddef>
#include <string>
#include <vector>

#include "backend/bible_backend.h"

struct NeutralComparisonResult {
	std::size_t mismatches = 0;
	std::vector<BibleReference> references;
};

struct NeutralOffsetValidationResult {
	std::size_t rowsChecked = 0;
	std::size_t invalidOffsets = 0;
	std::size_t substringMismatches = 0;
};

NeutralComparisonResult compareNeutralModules(
	BibleBackend &expectedBackend, const std::string &expectedModule,
	BibleBackend &actualBackend, const std::string &actualModule);

NeutralOffsetValidationResult validateNeutralOffsets(
	BibleBackend &backend, const std::string &module);

#endif
