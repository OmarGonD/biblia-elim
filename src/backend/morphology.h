#ifndef XIPHOS_MORPHOLOGY_H
#define XIPHOS_MORPHOLOGY_H

#include "backend/bible_types.h"

#include <string>
#include <vector>

struct MorphologyParseResult {
	std::vector<MorphologyTag> tags;
	std::vector<std::string> malformedValues;
};

/* Validate the neutral, already-tokenized form. This is shared by parsers and
 * persistence so malformed tags cannot bypass source-level parsing. */
bool isValidMorphologyTag(const MorphologyTag &tag);

/* Parse a source attribute containing whitespace-separated morphology values.
 * Commas, slashes and punctuation in codes remain opaque and are not
 * separators. Qualified values use "scheme:code"; unqualified values are
 * retained with an empty scheme. */
MorphologyParseResult parseMorphology(const std::string &source);

#endif
