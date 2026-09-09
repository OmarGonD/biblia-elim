#ifndef XIPHOS_USFM_IMPORTER_H
#define XIPHOS_USFM_IMPORTER_H

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "backend/bible_types.h"

struct UsfmImportOptions {
	std::string moduleId, name, language, versification;
	std::string abbreviation, description, license, publisher, source, contentVersion;
};

struct UsfmImportStats {
	std::size_t books = 0, chapters = 0, verses = 0;
	std::size_t footnotesImported = 0, crossReferencesImported = 0;
	std::size_t crossrefTargetsResolved = 0, crossrefTargetsUnresolved = 0;
	std::size_t footnotesSkipped = 0, crossReferencesSkipped = 0;
	std::size_t unsupportedMarkers = 0;
	std::size_t paragraphMarkers = 0, headingsImported = 0, addedSpans = 0;
	std::size_t wordsImported = 0, wordsWithStrong = 0;
	std::size_t strongIdsImported = 0;
	std::size_t morphologyBearingTokens = 0, morphologyTagsParsed = 0;
	std::size_t multiMorphologyTokens = 0, malformedMorphologyValues = 0;
	std::size_t maxMorphologyTagsPerToken = 0;
	std::size_t containerVerses = 0, milestoneVerses = 0;
	std::map<std::string, std::size_t> unknownInlineElements;
	std::map<std::string, std::size_t> unknownStructuralElements;
	std::map<std::string, std::size_t> ignoredAttributes;
	std::map<std::string, std::size_t> morphAttributes;
	std::map<std::string, std::size_t> morphSchemes;
	std::map<std::string, std::size_t> morphCodes;
	std::map<std::string, std::size_t> malformedMorphValues;
	std::vector<std::vector<MorphologyTag>> morphologyWordTags;
	std::map<std::string, std::size_t> nonStrongLemmaAttributes;
	std::map<std::string, std::size_t> rangeReferences;
	std::map<std::string, std::size_t> unresolvedReferences;
};

bool importUsfm(const std::vector<std::string> &inputs,
		const std::string &output, const UsfmImportOptions &options,
		UsfmImportStats &stats, std::string &error);

#endif
