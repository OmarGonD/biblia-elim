#ifndef XIPHOS_USFM_IMPORTER_H
#define XIPHOS_USFM_IMPORTER_H

#include <cstddef>
#include <string>
#include <vector>

#include "backend/bible_types.h"

struct UsfmImportOptions {
	std::string moduleId, name, language, versification;
	std::string abbreviation, description, license, publisher, source, contentVersion;
};

struct UsfmImportStats {
	std::size_t books = 0, chapters = 0, verses = 0;
	std::size_t footnotesSkipped = 0, crossReferencesSkipped = 0;
	std::size_t unsupportedMarkers = 0;
	std::size_t paragraphMarkers = 0, headingsImported = 0, addedSpans = 0;
	std::size_t wordsImported = 0, wordsWithStrong = 0;
};

bool importUsfm(const std::vector<std::string> &inputs,
		const std::string &output, const UsfmImportOptions &options,
		UsfmImportStats &stats, std::string &error);

#endif
