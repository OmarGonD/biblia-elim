#include "main/strong_interaction.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <utility>

namespace {
std::string htmlEscape(const std::string &value)
{
	std::string result;
	result.reserve(value.size());
	for (char c : value) {
		switch (c) {
		case '&': result += "&amp;"; break;
		case '<': result += "&lt;"; break;
		case '>': result += "&gt;"; break;
		case '"': result += "&quot;"; break;
		case '\'': result += "&#39;"; break;
		default: result += c; break;
		}
	}
	return result;
}

std::string queryEscape(const std::string &value)
{
	static const char hex[] = "0123456789ABCDEF";
	std::string result;
	for (unsigned char c : value) {
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			result += static_cast<char>(c);
		} else {
			result += '%';
			result += hex[c >> 4];
			result += hex[c & 15];
		}
	}
	return result;
}

bool containsStrong(const StrongWordContext &context, const StrongId &strong)
{
	return std::find(context.strongs.begin(), context.strongs.end(), strong) !=
		context.strongs.end();
}
}

StrongWordResolution resolveStrongInteraction(BibleBackend &backend,
	const std::string &module, const BibleReference &reference,
	std::size_t byteOffset)
{
	StrongWordResolution result;
	if (!backend.moduleCapabilities(module).strongs)
		return result;
	if (!backend.resolveStrongWord(module, reference, byteOffset,
		result.context) || result.context.strongs.empty())
		return result;
	result.action = result.context.strongs.size() == 1
		? StrongWordAction::OpenDetail : StrongWordAction::ChooseStrong;
	return result;
}

std::string renderStrongVerseText(const BibleVerseContent &content,
	const std::string &module, const std::string &key, bool strongEnabled)
{
	if (!strongEnabled)
		return htmlEscape(content.plainText);

	std::ostringstream result;
	std::size_t cursor = 0;
	for (const BibleWordInfo &word : content.words) {
		if (word.strongs.empty() || word.length == 0 || word.start < cursor ||
		    word.start > content.plainText.size() ||
		    word.length > content.plainText.size() - word.start)
			continue;
		result << htmlEscape(content.plainText.substr(cursor, word.start - cursor));
		result << "<a class=\"strong-word\" data-offset=\"" << word.start
		       << "\" href=\"passagestudy.jsp?action=showNeutralStrong&amp;module="
		       << queryEscape(module) << "&amp;passage=" << queryEscape(key)
		       << "&amp;value=" << word.start << "\">"
		       << htmlEscape(content.plainText.substr(word.start, word.length))
		       << "</a>";
		cursor = word.start + word.length;
	}
	result << htmlEscape(content.plainText.substr(cursor));
	return result.str();
}

StrongDetailSession::StrongDetailSession(BibleBackend &backend,
	const BibleApplicationResources &resources, std::string module,
	StrongWordContext context, std::size_t pageSize)
	: backend_(backend), resources_(resources), module_(std::move(module)),
	  context_(std::move(context)), pageSize_(pageSize)
{
}

bool StrongDetailSession::selectStrong(const StrongId &strong)
{
	if (!containsStrong(context_, strong)) return false;
	state_ = {};
	state_.selected = strong;
	state_.selectedValid = true;
	const auto lexiconStart = std::chrono::steady_clock::now();
	state_.lexicon = resources_.lookupStrong(strong);
	state_.lexiconMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - lexiconStart).count();
	const auto pageStart = std::chrono::steady_clock::now();
	StrongOccurrencePage page = backend_.findStrongOccurrencePage(
		module_, strong, pageSize_, 0);
	state_.pageMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - pageStart).count();
	state_.occurrences = std::move(page.occurrences);
	state_.hasMore = page.hasMore;
	return true;
}

bool StrongDetailSession::loadMore()
{
	if (!state_.selectedValid || !state_.hasMore) return false;
	StrongOccurrencePage page = backend_.findStrongOccurrencePage(
		module_, state_.selected, pageSize_, state_.occurrences.size());
	state_.occurrences.insert(state_.occurrences.end(),
		std::make_move_iterator(page.occurrences.begin()),
		std::make_move_iterator(page.occurrences.end()));
	state_.hasMore = page.hasMore;
	return true;
}
