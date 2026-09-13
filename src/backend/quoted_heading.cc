#include "backend/quoted_heading.h"

#include <cctype>
#include <cstring>
#include <string>

static const char *
skip_ws_and_tags(const char *p)
{
	for (;;) {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if (*p != '<')
			return p;
		const char *gt = std::strchr(p, '>');
		if (!gt)
			return p;
		p = gt + 1;
	}
}

static bool
open_quote(const char *p, std::size_t *n)
{
	if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xAB) {
		*n = 2;
		return true;
	}
	if (std::strncmp(p, "&laquo;", 7) == 0) {
		*n = 7;
		return true;
	}
	if (std::strncmp(p, "&#171;", 6) == 0 ||
	    std::strncmp(p, "&#xAB;", 6) == 0 ||
	    std::strncmp(p, "&#xab;", 6) == 0) {
		*n = 6;
		return true;
	}
	return false;
}

static bool
close_quote(const char *p, std::size_t *n)
{
	if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xBB) {
		*n = 2;
		return true;
	}
	if (std::strncmp(p, "&raquo;", 7) == 0) {
		*n = 7;
		return true;
	}
	if (std::strncmp(p, "&#187;", 6) == 0 ||
	    std::strncmp(p, "&#xBB;", 6) == 0 ||
	    std::strncmp(p, "&#xbb;", 6) == 0) {
		*n = 6;
		return true;
	}
	return false;
}

static bool
remainder_has_letter(const char *p)
{
	p = skip_ws_and_tags(p);
	if (!*p)
		return false;
	unsigned char c = (unsigned char)*p;
	if (c < 0x80)
		return std::isalpha(c) != 0;
	/* Any non-ASCII start after the title is verse body (UTF-8 letter
	 * or punctuation that begins a sentence). Require a UTF-8 lead. */
	return c >= 0xC0;
}

static bool
split_leading_quote(const std::string &text, std::string &inner,
		    std::string &rest)
{
	const char *p = skip_ws_and_tags(text.c_str());
	std::size_t open_n = 0;
	if (!open_quote(p, &open_n))
		return false;
	const char *start = p + open_n;
	const char *q = start;
	std::size_t close_n = 0;
	int depth = 0;
	while (*q) {
		if (*q == '<') {
			const char *gt = std::strchr(q, '>');
			if (!gt)
				return false;
			q = gt + 1;
			continue;
		}
		if (open_quote(q, &open_n) && depth >= 0) {
			depth++;
			q += open_n;
			continue;
		}
		if (close_quote(q, &close_n)) {
			if (depth == 0)
				break;
			depth--;
			q += close_n;
			continue;
		}
		q++;
	}
	if (!*q || !close_quote(q, &close_n))
		return false;
	inner.assign(start, q);
	while (!inner.empty() &&
	       (inner.back() == ' ' || inner.back() == '\n' ||
		inner.back() == '\t'))
		inner.pop_back();
	if (inner.size() < 3 || inner.size() > 800)
		return false;
	const char *after = q + close_n;
	if (!remainder_has_letter(after))
		return false;
	after = skip_ws_and_tags(after);
	rest = after;
	return true;
}

static bool
heading_already_has(const BibleVerseContent &content, const std::string &inner)
{
	for (std::size_t i = 0; i < content.headings.size(); i++) {
		if (content.headings[i].text.find(inner) != std::string::npos)
			return true;
	}
	return false;
}

void
promoteLeadingQuotedHeading(BibleVerseContent &content)
{
	std::string inner, rest;
	if (!split_leading_quote(content.renderedText, inner, rest)) {
		if (!split_leading_quote(content.plainText, inner, rest))
			return;
		content.plainText = rest;
	} else {
		content.renderedText = rest;
		std::string plain_inner, plain_rest;
		if (split_leading_quote(content.plainText, plain_inner,
					plain_rest))
			content.plainText = plain_rest;
	}
	if (heading_already_has(content, inner))
		return;
	BibleHeading heading;
	heading.text = "<h3>" + inner + "</h3>";
	content.headings.insert(content.headings.begin(), heading);
}
