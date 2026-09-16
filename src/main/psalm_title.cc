/*
 * Biblia Elim - psalm titles numbered as their own verse.
 */
#include "main/psalm_title.h"

#include <cctype>
#include <cstring>

#include "backend/content_availability.h"

namespace {

const char TITLE_CLASS[] = "x-psalm-title";

bool
isNameChar(char c)
{
	return std::isalnum((unsigned char)c) || c == '-' || c == '_';
}

/* The value of class="…" in an opening tag, or "" without one. */
std::string
classAttribute(const std::string &tag)
{
	std::size_t at = 0;
	while ((at = tag.find("class", at)) != std::string::npos) {
		const bool starts = at > 0 && !isNameChar(tag[at - 1]);
		std::size_t eq = at + 5;
		while (eq < tag.size() && std::isspace((unsigned char)tag[eq]))
			eq++;
		if (!starts || eq >= tag.size() || tag[eq] != '=') {
			at += 5;
			continue;
		}
		std::size_t q = eq + 1;
		while (q < tag.size() && std::isspace((unsigned char)tag[q]))
			q++;
		if (q >= tag.size() || (tag[q] != '"' && tag[q] != '\''))
			return std::string();
		std::size_t end = tag.find(tag[q], q + 1);
		if (end == std::string::npos)
			return std::string();
		return tag.substr(q + 1, end - q - 1);
	}
	return std::string();
}

bool
hasClassToken(const std::string &classes, const char *token)
{
	const std::size_t len = std::strlen(token);
	std::size_t i = 0;
	while (i < classes.size()) {
		while (i < classes.size() &&
		       std::isspace((unsigned char)classes[i]))
			i++;
		std::size_t j = i;
		while (j < classes.size() &&
		       !std::isspace((unsigned char)classes[j]))
			j++;
		if (j - i == len && classes.compare(i, len, token) == 0)
			return true;
		i = j;
	}
	return false;
}

bool
isSpanOpen(const std::string &html, std::size_t lt)
{
	return html.compare(lt, 5, "<span") == 0 &&
	       lt + 5 < html.size() && !isNameChar(html[lt + 5]);
}

/* Index just past the </span> matching the <span> at lt. */
std::size_t
matchingSpanEnd(const std::string &html, std::size_t lt)
{
	int depth = 0;
	std::size_t i = lt;
	while (i < html.size()) {
		std::size_t next = html.find('<', i);
		if (next == std::string::npos)
			return std::string::npos;
		std::size_t gt = html.find('>', next);
		if (gt == std::string::npos)
			return std::string::npos;
		if (isSpanOpen(html, next)) {
			if (html[gt - 1] != '/')
				depth++;
		} else if (html.compare(next, 7, "</span>") == 0) {
			if (--depth == 0)
				return gt + 1;
		}
		i = gt + 1;
	}
	return std::string::npos;
}

} // namespace

PsalmTitleParts
splitPsalmTitle(const std::string &html)
{
	PsalmTitleParts parts;
	parts.body = html;

	std::size_t lt = 0;
	bool found = false;
	while ((lt = html.find("<span", lt)) != std::string::npos) {
		std::size_t gt = html.find('>', lt);
		if (gt == std::string::npos)
			break;
		if (isSpanOpen(html, lt) &&
		    hasClassToken(classAttribute(html.substr(lt, gt - lt + 1)),
				  TITLE_CLASS)) {
			found = true;
			break;
		}
		lt = gt + 1;
	}
	if (!found)
		return parts;

	const std::string before = html.substr(0, lt);
	if (hasUsableVerseBody(before))
		return parts;
	const std::size_t end = matchingSpanEnd(html, lt);
	if (end == std::string::npos)
		return parts;

	std::string after = html.substr(end);
	std::size_t first = 0;
	while (first < after.size() && std::isspace((unsigned char)after[first]))
		first++;
	after.erase(0, first);

	parts.hasTitle = true;
	parts.title = html.substr(lt, end - lt);
	parts.body = before + after;
	parts.hasBody = hasUsableVerseBody(parts.body);
	return parts;
}

std::string
psalmTitleVerseHtml(const PsalmTitleParts &parts)
{
	if (!parts.hasTitle)
		return parts.body;
	if (!parts.hasBody)
		return parts.title + parts.body;
	return parts.title + "<br/>" + parts.body;
}
