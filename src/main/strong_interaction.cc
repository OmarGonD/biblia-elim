#include "main/strong_interaction.h"

#include <glib.h>

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

/* Small capitals as the Bible pane can draw them: upper case, and every
 * letter after a word's first one in <small> ("L<small>ORD</small>"). */
std::string smallCaps(const std::string &text, std::size_t start, std::size_t end)
{
	std::string result;
	bool small = false;
	const char *base = text.c_str();
	for (const char *p = base + start; p < base + end; p = g_utf8_next_char(p)) {
		const gunichar c = g_utf8_get_char(p);
		const bool afterLetter = p > base &&
			g_unichar_isalpha(g_utf8_get_char(g_utf8_prev_char(p)));
		const bool wantSmall = g_unichar_isalpha(c) && afterLetter;
		if (wantSmall != small) {
			result += wantSmall ? "<small>" : "</small>";
			small = wantSmall;
		}
		gchar *upper = g_utf8_strup(p, g_utf8_next_char(p) - p);
		result += htmlEscape(upper);
		g_free(upper);
	}
	if (small) result += "</small>";
	return result;
}

/* The verse text in [from, to) with its styled spans. Tags open and close
 * inside the range, so it nests in whatever the caller wraps around it
 * (an annotated word's link, a footnote marker's neighbours). With no spans
 * this is exactly htmlEscape(). */
std::string styledText(const BibleVerseContent &content, std::size_t from,
	std::size_t to, const VerseTextStyle &style)
{
	const std::string &text = content.plainText;
	to = std::min(to, text.size());
	if (from >= to) return std::string();
	std::vector<std::size_t> cuts = { from, to };
	for (const BibleTextSpan &span : content.spans) {
		if (span.start > text.size() || span.length > text.size() - span.start) continue;
		if (span.start > from && span.start < to) cuts.push_back(span.start);
		const std::size_t end = span.start + span.length;
		if (end > from && end < to) cuts.push_back(end);
	}
	std::sort(cuts.begin(), cuts.end());
	cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
	std::string result;
	for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
		const std::size_t a = cuts[i], b = cuts[i + 1];
		bool added = false, christ = false, divine = false, bold = false;
		for (const BibleTextSpan &span : content.spans) {
			if (span.start > text.size() || span.length > text.size() - span.start ||
			    span.start > a || span.start + span.length < b) continue;
			added |= span.style == BibleTextStyle::Added || span.style == BibleTextStyle::Italic;
			bold |= span.style == BibleTextStyle::Bold;
			christ |= span.style == BibleTextStyle::WordsOfChrist;
			divine |= span.style == BibleTextStyle::DivineName;
		}
		christ = christ && style.wordsOfChristInRed;
		if (bold) result += "<b>";
		if (added) result += "<i>";
		if (christ) result += "<font color=\"red\">";
		result += divine ? smallCaps(text, a, b) : htmlEscape(text.substr(a, b - a));
		if (christ) result += "</font>";
		if (added) result += "</i>";
		if (bold) result += "</b>";
	}
	return result;
}

bool containsStrong(const BibleAnnotatedWord &context, const StrongId &strong)
{
	return std::find(context.strongs.begin(), context.strongs.end(), strong) !=
		context.strongs.end();
}

/* MorphologyTag has value equality (backend/bible_types.h): scheme and
 * code both match, exactly what MORPH-107's occurrence lookup keys on. */
bool containsMorphology(const BibleAnnotatedWord &context,
	const MorphologyTag &morphology)
{
	return std::find(context.morphologyTags.begin(),
		context.morphologyTags.end(), morphology) !=
		context.morphologyTags.end();
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
	const std::string &module, const std::string &key, bool annotationsEnabled,
	const VerseTextStyle &style)
{
	auto text = [&](std::size_t from, std::size_t length) {
		return styledText(content, from,
			length == std::string::npos ? content.plainText.size() : from + length, style);
	};
	/* A note's marker: its label (or its number in the verse), or in the
	 * pane SWORD's raised "*n", followed by the label when the module
	 * numbers its notes, otherwise numbered by the pane. */
	auto noteLabel = [&](std::size_t i) {
		std::string label = content.footnotes[i].label;
		const bool numbered = !(label.empty() || label == "+" || label == "-");
		if (style.paneNoteMarkers)
			return "<small><sup>*n" + (numbered && content.footnotesHaveNumbers
				? htmlEscape(label) : std::string()) + "</sup></small>";
		return htmlEscape(numbered ? label : std::to_string(i + 1));
	};
	auto crossLabel = [&]() {
		return std::string(style.paneNoteMarkers ? "<small><sup>*x</sup></small>" : "↗");
	};
	const bool anyAnnotations = std::any_of(content.words.begin(),
		content.words.end(), [](const BibleWordInfo &word) {
			return !word.strongs.empty() || !word.morphologyTags.empty();
		});
	if ((!annotationsEnabled || !anyAnnotations) && content.footnotes.empty() && content.crossReferences.empty())
		return text(0, std::string::npos);
	if (!annotationsEnabled || !anyAnnotations) {
		std::ostringstream plain; std::size_t cursor=0;
		for (std::size_t i=0;i<content.footnotes.size();++i) { const auto &n=content.footnotes[i]; if(n.offset>cursor) plain<<text(cursor, n.offset-cursor); plain<<"<a class=\"bible-footnote\" href=\"passagestudy.jsp?action=showNeutralFootnote&amp;module="<<queryEscape(module)<<"&amp;passage="<<queryEscape(key)<<"&amp;value="<<i<<"\">"<<noteLabel(i)<<"</a>"; cursor=n.offset; }
		for (std::size_t i=0;i<content.crossReferences.size();++i) { const auto &x=content.crossReferences[i]; if(x.offset>cursor) plain<<text(cursor, x.offset-cursor); plain<<"<a class=\"bible-crossref\" href=\"passagestudy.jsp?action=showNeutralCrossref&amp;module="<<queryEscape(module)<<"&amp;passage="<<queryEscape(key)<<"&amp;value="<<i<<"\">"<<crossLabel()<<"</a>"; cursor=x.offset; }
		plain<<text(cursor, std::string::npos); return plain.str();
	}

	std::ostringstream result;
	std::size_t cursor = 0;
	std::size_t noteIndex = 0, crossIndex = 0;
	auto markers = [&](std::size_t limit) {
		while (noteIndex < content.footnotes.size() && content.footnotes[noteIndex].offset <= limit) {
			result << "<a class=\"bible-footnote\" data-sequence=\"" << noteIndex << "\" href=\"passagestudy.jsp?action=showNeutralFootnote&amp;module=" << queryEscape(module) << "&amp;passage=" << queryEscape(key) << "&amp;value=" << noteIndex << "\">" << noteLabel(noteIndex) << "</a>"; ++noteIndex;
		}
		while (crossIndex < content.crossReferences.size() && content.crossReferences[crossIndex].offset <= limit) {
			result << "<a class=\"bible-crossref\" data-sequence=\"" << crossIndex << "\" href=\"passagestudy.jsp?action=showNeutralCrossref&amp;module=" << queryEscape(module) << "&amp;passage=" << queryEscape(key) << "&amp;value=" << crossIndex << "\">" << crossLabel() << "</a>"; ++crossIndex;
		}
	};
	for (const BibleWordInfo &word : content.words) {
		if ((word.strongs.empty() && word.morphologyTags.empty()) ||
		    word.length == 0 || word.start < cursor ||
		    word.start > content.plainText.size() ||
		    word.length > content.plainText.size() - word.start)
			continue;
		result << text(cursor, word.start - cursor); markers(word.start);
		result << "<a class=\"annotated-word\" data-offset=\"" << word.start
		       << "\" href=\"passagestudy.jsp?action=showNeutralWord&amp;module="
		       << queryEscape(module) << "&amp;passage=" << queryEscape(key)
		       << "&amp;value=" << word.start << "\">"
		       << text(word.start, word.length)
		       << "</a>";
		cursor = word.start + word.length;
	}
	result << text(cursor, std::string::npos); markers(content.plainText.size());
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

bool StrongDetailSession::selectMorphology(const MorphologyTag &morphology)
{
	if (!containsMorphology(context_, morphology)) return false;
	morphologyState_ = {};
	morphologyState_.selected = morphology;
	morphologyState_.selectedValid = true;
	const auto pageStart = std::chrono::steady_clock::now();
	MorphologyOccurrencePage page = backend_.findMorphologyOccurrencePage(
		module_, morphology, pageSize_, 0);
	morphologyState_.pageMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - pageStart).count();
	morphologyState_.occurrences = std::move(page.occurrences);
	morphologyState_.hasMore = page.hasMore;
	return true;
}

bool StrongDetailSession::loadMoreMorphology()
{
	if (!morphologyState_.selectedValid || !morphologyState_.hasMore) return false;
	MorphologyOccurrencePage page = backend_.findMorphologyOccurrencePage(
		module_, morphologyState_.selected, pageSize_,
		morphologyState_.occurrences.size());
	morphologyState_.occurrences.insert(morphologyState_.occurrences.end(),
		std::make_move_iterator(page.occurrences.begin()),
		std::make_move_iterator(page.occurrences.end()));
	morphologyState_.hasMore = page.hasMore;
	return true;
}
