#include "backend/content_availability.h"

#include <cctype>
#include <cstring>
#include <string>

namespace {

void
lowerAsciiInPlace(std::string &s)
{
	for (std::size_t i = 0; i < s.size(); i++) {
		if (s[i] >= 'A' && s[i] <= 'Z')
			s[i] = (char)(s[i] - 'A' + 'a');
	}
}

bool
headingTagName(const std::string &name)
{
	return name == "h1" || name == "h2" || name == "h3" || name == "h4" ||
	       name == "h5" || name == "h6" || name == "title";
}

std::string
tagNameAt(const std::string &html, std::size_t lt)
{
	if (lt >= html.size() || html[lt] != '<')
		return std::string();
	std::size_t i = lt + 1;
	if (i < html.size() && html[i] == '/')
		i++;
	std::size_t start = i;
	while (i < html.size() &&
	       (std::isalpha((unsigned char)html[i]) ||
		std::isdigit((unsigned char)html[i])))
		i++;
	std::string name = html.substr(start, i - start);
	lowerAsciiInPlace(name);
	return name;
}

std::size_t
findCloseTag(const std::string &html, std::size_t from, const std::string &name)
{
	std::string close = "</" + name + ">";
	std::string lower = html;
	lowerAsciiInPlace(lower);
	std::size_t at = lower.find(close, from);
	if (at == std::string::npos)
		return std::string::npos;
	return at + close.size();
}

bool
attrHasClass(const std::string &open_tag, const char *token)
{
	std::string lower = open_tag;
	lowerAsciiInPlace(lower);
	std::size_t cls = lower.find("class=");
	if (cls == std::string::npos)
		return false;
	return lower.find(token, cls) != std::string::npos;
}

bool
attrHasType(const std::string &open_tag, const char *token)
{
	std::string lower = open_tag;
	lowerAsciiInPlace(lower);
	std::size_t type = lower.find("type=");
	if (type == std::string::npos)
		return false;
	return lower.find(token, type) != std::string::npos;
}

std::string
stripMarkup(const std::string &text)
{
	std::string out;
	bool in_tag = false;
	bool in_entity = false;
	for (std::size_t i = 0; i < text.size(); i++) {
		unsigned char c = (unsigned char)text[i];
		if (in_tag) {
			if (c == '>')
				in_tag = false;
			continue;
		}
		if (in_entity) {
			if (c == ';' || c == ' ')
				in_entity = false;
			continue;
		}
		if (c == '<') {
			in_tag = true;
			continue;
		}
		if (c == '&') {
			in_entity = true;
			continue;
		}
		out.push_back((char)c);
	}
	return out;
}

bool
looksLikeIntroDump(const std::string &plain)
{
	/* A verse slot filled with a page of apparatus, not a biblical
	 * verse. Real reconstructed verses in this corpus sit well below
	 * this size; dumps of book introductions do not. */
	if (plain.size() < 700)
		return false;
	int stops = 0;
	for (std::size_t i = 0; i < plain.size(); i++) {
		char c = plain[i];
		if (c == '.' || c == '?' || c == '!')
			stops++;
	}
	return stops >= 4;
}

bool
wholeEntryIsReconstructionSeg(const std::string &html)
{
	std::size_t i = 0;
	while (i < html.size() &&
	       std::isspace((unsigned char)html[i]))
		i++;
	if (i >= html.size() || html[i] != '<')
		return false;
	std::string name = tagNameAt(html, i);
	if (name != "span" && name != "seg")
		return false;
	std::size_t gt = html.find('>', i);
	if (gt == std::string::npos)
		return false;
	std::string open = html.substr(i, gt - i + 1);
	if (!attrHasClass(open, "ocr-facs") && !attrHasType(open, "ocr-facs") &&
	    !attrHasClass(open, "reconstruido-testigos") &&
	    !attrHasType(open, "reconstruido-testigos"))
		return false;
	std::size_t end = findCloseTag(html, gt, name);
	if (end == std::string::npos)
		return false;
	while (end < html.size() &&
	       std::isspace((unsigned char)html[end]))
		end++;
	return end == html.size();
}

void
trimHtml(std::string &html)
{
	std::size_t a = 0;
	while (a < html.size() &&
	       std::isspace((unsigned char)html[a]))
		a++;
	std::size_t b = html.size();
	while (b > a && std::isspace((unsigned char)html[b - 1]))
		b--;
	html = html.substr(a, b - a);
}

} // namespace

bool
hasUsableVerseBody(const std::string &text)
{
	bool in_tag = false;
	bool in_entity = false;
	for (std::size_t i = 0; i < text.size(); i++) {
		unsigned char c = (unsigned char)text[i];
		if (in_tag) {
			if (c == '>')
				in_tag = false;
			continue;
		}
		if (in_entity) {
			if (c == ';' || c == ' ')
				in_entity = false;
			continue;
		}
		if (c == '<') {
			in_tag = true;
			continue;
		}
		if (c == '&') {
			in_entity = true;
			continue;
		}
		if (std::isalnum(c) || c >= 0xC0)
			return true;
	}
	return false;
}

void
splitIntroductoryMaterial(BibleVerseContent &content)
{
	std::string body = content.renderedText.empty() ? content.plainText
							: content.renderedText;

	for (std::size_t h = 0; h < content.headings.size(); h++) {
		const std::string &heading = content.headings[h].text;
		if (heading.empty() || body.empty())
			continue;
		if (body.compare(0, heading.size(), heading) == 0) {
			body.erase(0, heading.size());
			trimHtml(body);
		}
	}

	std::string extracted;
	std::size_t i = 0;
	while (i < body.size()) {
		if (body[i] != '<') {
			extracted.push_back(body[i]);
			i++;
			continue;
		}
		std::string name = tagNameAt(body, i);
		std::size_t gt = body.find('>', i);
		if (gt == std::string::npos) {
			extracted.append(body.substr(i));
			break;
		}
		std::string open = body.substr(i, gt - i + 1);
		bool intro_div =
			(name == "div" || name == "span") &&
			attrHasClass(open, "intromaterial");
		if (headingTagName(name) || intro_div) {
			std::size_t end = findCloseTag(body, gt, name);
			if (end == std::string::npos)
				end = body.size();
			BibleHeading heading;
			heading.text = body.substr(i, end - i);
			content.headings.push_back(heading);
			i = end;
			continue;
		}
		extracted.append(body.substr(i, gt - i + 1));
		i = gt + 1;
	}
	trimHtml(extracted);

	if (wholeEntryIsReconstructionSeg(extracted) &&
	    looksLikeIntroDump(stripMarkup(extracted))) {
		BibleHeading heading;
		heading.text = extracted;
		content.headings.push_back(heading);
		extracted.clear();
	}

	content.renderedText = extracted;
	if (!content.plainText.empty() || extracted.empty())
		content.plainText = stripMarkup(extracted);
}

ContentAvailability
classifyVerseContent(const BibleVerseContent &content,
		     const BibleReference &reference)
{
	if (reference.chapter <= 0 || reference.verse <= 0)
		return ContentAvailability::NotApplicable;
	BibleVerseContent split = content;
	splitIntroductoryMaterial(split);
	if (hasUsableVerseBody(split.renderedText) ||
	    hasUsableVerseBody(split.plainText))
		return ContentAvailability::Available;
	return ContentAvailability::Missing;
}
