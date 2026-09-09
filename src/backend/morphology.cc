#include "backend/morphology.h"

#include <cctype>
#include <utility>

namespace {

bool validScheme(const std::string &value)
{
	if (value.empty()) return false;
	for (unsigned char character : value)
		if (!std::isalnum(character) && character != '-' && character != '_' &&
		    character != '.')
			return false;
	return true;
}

bool containsWhitespace(const std::string &value)
{
	for (unsigned char character : value)
		if (std::isspace(character)) return true;
	return false;
}

void appendValue(const std::string &value, MorphologyParseResult &result)
{
	const std::size_t separator = value.find(':');
	MorphologyTag tag;
	if (separator == std::string::npos)
		tag.code = value;
	else {
		tag.scheme = value.substr(0, separator);
		tag.code = value.substr(separator + 1);
	}
	if ((separator != std::string::npos && tag.scheme.empty()) ||
	    !isValidMorphologyTag(tag)) {
		result.malformedValues.push_back(value);
		return;
	}
	result.tags.push_back(std::move(tag));
}

} // namespace

bool isValidMorphologyTag(const MorphologyTag &tag)
{
	return !tag.code.empty() && !containsWhitespace(tag.code) &&
		(tag.scheme.empty() || validScheme(tag.scheme)) &&
		(!tag.scheme.empty() || tag.code.find(':') == std::string::npos);
}

MorphologyParseResult parseMorphology(const std::string &source)
{
	MorphologyParseResult result;
	std::string value;
	for (unsigned char character : source) {
		if (std::isspace(character)) {
			if (!value.empty()) {
				appendValue(value, result);
				value.clear();
			}
		} else {
			value.push_back(static_cast<char>(character));
		}
	}
	if (!value.empty()) appendValue(value, result);
	if (result.tags.empty() && result.malformedValues.empty())
		result.malformedValues.push_back("<empty>");
	return result;
}
