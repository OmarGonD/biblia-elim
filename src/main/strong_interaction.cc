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

bool containsStrong(const BibleAnnotatedWord &context, const StrongId &strong)
{
	return std::find(context.strongs.begin(), context.strongs.end(), strong) !=
		context.strongs.end();
}
}

AnnotatedWordResolution resolveAnnotatedWordInteraction(BibleBackend &backend,
	const std::string &module, const BibleReference &reference,
	std::size_t byteOffset)
{
	AnnotatedWordResolution result;
	const BibleModuleCapabilities capabilities =
		backend.moduleCapabilities(module);
	if (!capabilities.strongs && !capabilities.morphology)
		return result;
	if (!backend.resolveAnnotatedWord(module, reference, byteOffset,
		result.context) || (result.context.strongs.empty() &&
		result.context.morphologyTags.empty()))
		return result;
	result.action = result.context.strongs.size() > 1
		? AnnotatedWordAction::ChooseStrong
		: AnnotatedWordAction::OpenDetail;
	return result;
}

std::string renderAnnotatedVerseText(const BibleVerseContent &content,
	const std::string &module, const std::string &key, bool annotationsEnabled)
{
	const bool anyAnnotations = std::any_of(content.words.begin(),
		content.words.end(), [](const BibleWordInfo &word) {
			return !word.strongs.empty() || !word.morphologyTags.empty();
		});
	if ((!annotationsEnabled || !anyAnnotations) && content.footnotes.empty() && content.crossReferences.empty())
		return htmlEscape(content.plainText);
	if (!annotationsEnabled || !anyAnnotations) {
		std::ostringstream plain; std::size_t cursor=0;
		for (std::size_t i=0;i<content.footnotes.size();++i) { const auto &n=content.footnotes[i]; if(n.offset>cursor) plain<<htmlEscape(content.plainText.substr(cursor,n.offset-cursor)); std::string l=n.label; if(l.empty()||l=="+"||l=="-") l=std::to_string(i+1); plain<<"<a class=\"bible-footnote\" href=\"passagestudy.jsp?action=showNeutralFootnote&amp;module="<<queryEscape(module)<<"&amp;passage="<<queryEscape(key)<<"&amp;value="<<i<<"\">"<<htmlEscape(l)<<"</a>"; cursor=n.offset; }
		for (std::size_t i=0;i<content.crossReferences.size();++i) { const auto &x=content.crossReferences[i]; if(x.offset>cursor) plain<<htmlEscape(content.plainText.substr(cursor,x.offset-cursor)); plain<<"<a class=\"bible-crossref\" href=\"passagestudy.jsp?action=showNeutralCrossref&amp;module="<<queryEscape(module)<<"&amp;passage="<<queryEscape(key)<<"&amp;value="<<i<<"\">↗</a>"; cursor=x.offset; }
		plain<<htmlEscape(content.plainText.substr(cursor)); return plain.str();
	}

	std::ostringstream result;
	std::size_t cursor = 0;
	std::size_t noteIndex = 0, crossIndex = 0;
	auto markers = [&](std::size_t limit) {
		while (noteIndex < content.footnotes.size() && content.footnotes[noteIndex].offset <= limit) {
			const auto &n=content.footnotes[noteIndex]; std::string label=n.label;
			if (label.empty() || label=="+" || label=="-") label=std::to_string(noteIndex+1);
			result << "<a class=\"bible-footnote\" data-sequence=\"" << noteIndex << "\" href=\"passagestudy.jsp?action=showNeutralFootnote&amp;module=" << queryEscape(module) << "&amp;passage=" << queryEscape(key) << "&amp;value=" << noteIndex << "\">" << htmlEscape(label) << "</a>"; ++noteIndex;
		}
		while (crossIndex < content.crossReferences.size() && content.crossReferences[crossIndex].offset <= limit) {
			result << "<a class=\"bible-crossref\" data-sequence=\"" << crossIndex << "\" href=\"passagestudy.jsp?action=showNeutralCrossref&amp;module=" << queryEscape(module) << "&amp;passage=" << queryEscape(key) << "&amp;value=" << crossIndex << "\">↗</a>"; ++crossIndex;
		}
	};
	for (const BibleWordInfo &word : content.words) {
		if ((word.strongs.empty() && word.morphologyTags.empty()) ||
		    word.length == 0 || word.start < cursor ||
		    word.start > content.plainText.size() ||
		    word.length > content.plainText.size() - word.start)
			continue;
		result << htmlEscape(content.plainText.substr(cursor, word.start - cursor)); markers(word.start);
		result << "<a class=\"annotated-word\" data-offset=\"" << word.start
		       << "\" href=\"passagestudy.jsp?action=showNeutralWord&amp;module="
		       << queryEscape(module) << "&amp;passage=" << queryEscape(key)
		       << "&amp;value=" << word.start << "\">"
		       << htmlEscape(content.plainText.substr(word.start, word.length))
		       << "</a>";
		cursor = word.start + word.length;
	}
	result << htmlEscape(content.plainText.substr(cursor)); markers(content.plainText.size());
	return result.str();
}

StrongDetailSession::StrongDetailSession(BibleBackend &backend,
	const BibleApplicationResources &resources, std::string module,
	BibleAnnotatedWord context, std::size_t pageSize)
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
