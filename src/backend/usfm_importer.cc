#include "backend/usfm_importer.h"
#include "backend/strong_id.h"
#include "backend/morphology.h"
#include "backend/bible_book_map.h"
#include "backend/sqlite/sqlite_module_writer.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <tuple>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>

namespace {
struct Verse { int book = 0, chapter = 0, verse = 0; std::string text; bool paragraph = false; std::string heading; std::vector<BibleTextSpan> spans; std::vector<BibleWordInfo> words; std::vector<BibleFootnote> footnotes; std::vector<BibleCrossReference> crossReferences; };
struct BookState { const BibleBookDefinition *book = nullptr; std::string title; std::string heading; };

std::string trim(const std::string &s)
{
	const std::size_t first = s.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return {};
	const std::size_t last = s.find_last_not_of(" \t\r\n");
	return s.substr(first, last - first + 1);
}

std::string inlineText(const std::string &s, std::string *field = nullptr)
{
	std::string out;
	for (std::size_t i=0;i<s.size();) {
		if (s[i]=='\\') { std::size_t e=i+1; while(e<s.size() && std::isalpha((unsigned char)s[e])) ++e; std::string m=s.substr(i+1,e-i-1); if(e<s.size()&&s[e]=='*') ++e; std::size_t n=s.find('\\',e); std::string part=s.substr(e,n==std::string::npos?s.size()-e:n-e); if(m=="ft"||m=="fq"||m=="fqa"||m=="fk"||m=="fl"||m=="fw"||m=="fp"||m=="fv"||m=="xt"||m=="xq") out += part; i=n==std::string::npos?s.size():n; continue; }
		out+=s[i++];
	}
	if (field) *field=trim(out);
	return trim(out);
}

bool parseTarget(const std::string &raw, BibleReference &out)
{
	std::string s=trim(raw); std::size_t p=s.find_last_of(' '); if(p==std::string::npos) return false;
	std::string book=s.substr(0,p), cv=s.substr(p+1); std::size_t c=cv.find(':'); if(c==std::string::npos) return false;
	try { int ch=std::stoi(cv.substr(0,c)), v=std::stoi(cv.substr(c+1)); for(const auto &b:canonicalBibleBooks()) if(book==b.osis || book==b.shortName || book==b.name) { out={b.testament,b.bookId,ch,v}; return ch>0&&v>0; } } catch(...) {}
	return false;
}

std::string markerField(const std::string &body, const char *marker)
{
	const std::string needle = std::string("\\") + marker;
	std::size_t p = body.find(needle); if (p == std::string::npos) return {};
	p += needle.size(); if (p < body.size() && body[p] == ' ') ++p;
	std::size_t e = body.find('\\', p); return trim(body.substr(p, e == std::string::npos ? std::string::npos : e-p));
}

std::string attributeValue(const std::string &attributes, const std::string &name,
	bool &present)
{
	present = false;
	const std::string key = name + "=\"";
	std::size_t start = 0;
	while ((start = attributes.find(key, start)) != std::string::npos) {
		if (start == 0 || std::isspace(static_cast<unsigned char>(attributes[start - 1]))) {
			present = true;
			const std::size_t valueStart = start + key.size();
			const std::size_t valueEnd = attributes.find('"', valueStart);
			return attributes.substr(valueStart,
				valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
		}
		start += key.size();
	}
	return {};
}

void auditMorphology(const std::string &attribute, const std::string &value,
	UsfmImportStats &stats, BibleWordInfo &word)
{
	++stats.morphAttributes[attribute];
	const MorphologyParseResult parsed = parseMorphology(value);
	word.morphologyTags.insert(word.morphologyTags.end(), parsed.tags.begin(), parsed.tags.end());
	for (const MorphologyTag &tag : parsed.tags) {
		++stats.morphSchemes[tag.scheme.empty() ? "unqualified" : tag.scheme];
		++stats.morphCodes[tag.code];
	}
	for (const std::string &malformed : parsed.malformedValues)
		++stats.malformedMorphValues[malformed];
	stats.morphologyTagsParsed += parsed.tags.size();
	stats.malformedMorphologyValues += parsed.malformedValues.size();
}

std::string cleanInline(const std::string &source, UsfmImportStats &stats,
	std::vector<BibleTextSpan> &spans, std::vector<BibleWordInfo> &words,
	std::vector<BibleFootnote> &footnotes, std::vector<BibleCrossReference> &crossReferences)
{
	std::string out;
	std::vector<std::size_t> added;
	for (std::size_t i = 0; i < source.size();) {
		if (source[i] != '\\') { out += source[i++]; continue; }
		std::size_t end = i + 1;
		if (end < source.size() && source[end] == '+') ++end;
		while (end < source.size() && std::isalpha(static_cast<unsigned char>(source[end]))) ++end;
		const std::string marker = source.substr(i + 1, end - i - 1);
		if (marker == "w" || marker == "+w") {
			const std::string close = "\\" + marker + "*";
			const std::size_t finish = source.find(close, end);
			const std::string word = source.substr(end, finish == std::string::npos ? source.size() - end : finish - end);
			const std::size_t attributes = word.find('|');
			const std::string visible = attributes == std::string::npos ? word : word.substr(0, attributes);
			const std::size_t leading = visible.find_first_not_of(" \t\r\n");
			const std::size_t first = leading == std::string::npos ? visible.size() : leading;
			const std::size_t trailing = visible.find_last_not_of(" \t\r\n");
			out += visible;
			if (leading != std::string::npos && trailing != std::string::npos) {
				BibleWordInfo info;
				info.start = out.size() - visible.size() + first;
				info.length = trailing - first + 1;
				info.text = visible.substr(first, info.length);
				if (attributes != std::string::npos) {
					const std::string attributeText = word.substr(attributes + 1);
					bool hasMorphAttribute = false;
					bool strongPresent = false;
					info.strong = attributeValue(attributeText, "strong", strongPresent);
					if (strongPresent)
						info.strongs = parseStrongIds(info.strong);
					for (const std::string &name : { std::string("x-morph"), std::string("morph") }) {
						bool present = false;
						const std::string value = attributeValue(attributeText, name, present);
						if (present) {
							hasMorphAttribute = true;
							auditMorphology("w." + name, value, stats, info);
						}
					}
					if (hasMorphAttribute) stats.morphologyWordTags.push_back(info.morphologyTags);
				}
				if (!info.morphologyTags.empty()) {
					++stats.morphologyBearingTokens;
					if (info.morphologyTags.size() > 1) ++stats.multiMorphologyTokens;
					stats.maxMorphologyTagsPerToken = std::max(
						stats.maxMorphologyTagsPerToken, info.morphologyTags.size());
				}
				words.push_back(info);
			}
			i = finish == std::string::npos ? source.size() : finish + close.size();
			continue;
		}
		if (marker == "add") {
			if (end < source.size() && source[end] == '*') {
				if (!added.empty()) {
					std::size_t start = added.back(); added.pop_back();
					std::size_t finish = out.size();
					while (start < finish && std::isspace(static_cast<unsigned char>(out[start]))) ++start;
					while (finish > start && std::isspace(static_cast<unsigned char>(out[finish - 1]))) --finish;
					spans.push_back({ start, finish - start, BibleTextStyle::Added });
				}
			} else {
				added.push_back(out.size());
			}
			i = end < source.size() && source[end] == '*' ? end + 1 : end;
			continue;
		}
		if (marker == "f" || marker == "x") {
			const std::string close = "\\" + marker + "*";
			const std::size_t finish = source.find(close, end);
			const std::string body = source.substr(end, finish == std::string::npos ? source.size()-end : finish-end);
			const std::size_t offset = out.size();
			if (marker == "f") { BibleFootnote n; n.offset=offset; n.body=markerField(body,"ft"); if(n.body.empty()) n.body=inlineText(body); std::string lead=trim(body); if(!lead.empty() && (lead[0]=='+'||lead[0]=='-')) n.label=lead.substr(0,1); if (!n.body.empty()) { footnotes.push_back(n); ++stats.footnotesImported; } }
			else { BibleCrossReference x; x.offset=offset; x.displayText=markerField(body,"xt"); if(x.displayText.empty()) x.displayText=inlineText(body); std::size_t pos=0; while(pos<x.displayText.size()) { std::size_t end=x.displayText.find(';',pos); BibleReference target; if(parseTarget(x.displayText.substr(pos,end==std::string::npos?std::string::npos:end-pos),target)) { x.references.push_back(target); ++stats.crossrefTargetsResolved; } else if(!trim(x.displayText.substr(pos,end==std::string::npos?std::string::npos:end-pos)).empty()) ++stats.crossrefTargetsUnresolved; if(end==std::string::npos) break; pos=end+1; } if (!x.displayText.empty()) { crossReferences.push_back(x); ++stats.crossReferencesImported; } }
			i = finish == std::string::npos ? source.size() : finish + close.size();
			continue;
		}
		if (!marker.empty()) {
			// Character styles are visual only in v1: retain their content.
			i = end;
			if (i < source.size() && source[i] == '*') ++i;
			continue;
		}
		out += source[i++];
	}
	const std::size_t leading = out.find_first_not_of(" \t\r\n");
	const std::size_t removed = leading == std::string::npos ? out.size() : leading;
	for (BibleTextSpan &span : spans) {
		if (span.start >= removed) span.start -= removed;
		else span.start = 0;
	}
	for (BibleWordInfo &word : words) {
		if (word.start >= removed) word.start -= removed;
		else word.start = 0;
	}
	for (BibleFootnote &note : footnotes) {
		if (note.offset >= removed) note.offset -= removed;
		else note.offset = 0;
	}
	for (BibleCrossReference &xref : crossReferences) {
		if (xref.offset >= removed) xref.offset -= removed;
		else xref.offset = 0;
	}
	const std::string normalized = trim(out);
	for (auto &note : footnotes) if (note.offset > normalized.size()) note.offset = normalized.size();
	for (auto &xref : crossReferences) if (xref.offset > normalized.size()) xref.offset = normalized.size();
	return normalized;
}

bool parseFile(const std::string &path, std::map<int, BookState> &books,
		std::vector<Verse> &verses, UsfmImportStats &stats, std::string &error)
{
	std::ifstream input(path);
	if (!input) { error = "cannot open " + path; return false; }
	const BibleBookDefinition *book = nullptr;
	int chapter = 0, verseNumber = 0;
	bool paragraphPending = false;
	Verse current;
	std::string line;
	auto flush = [&]() {
		if (!verseNumber) return;
		current.text = cleanInline(current.text, stats, current.spans, current.words, current.footnotes, current.crossReferences);
		verses.push_back(current);
		verseNumber = 0; current = Verse();
	};
	while (std::getline(input, line)) {
		std::string value = trim(line);
		if (value.empty()) continue;
		if (value[0] != '\\') {
			if (verseNumber) { if (!current.text.empty()) current.text += ' '; current.text += value; }
			continue;
		}
		std::size_t markerEnd = value.find_first_of(" \t");
		if (markerEnd == std::string::npos) markerEnd = value.size();
		const std::string marker = value.substr(1, markerEnd - 1);
		const std::string content = trim(value.substr(markerEnd));
		if (marker == "id") {
			flush();
			std::istringstream parts(content); std::string code; parts >> code;
			book = findBibleBookByUsfm(code);
			if (!book) { error = "unknown USFM book in " + path + ": " + code; return false; }
			books[book->bookId] = { book, {} };
			chapter = 0;
		} else if (marker == "h" || marker == "toc1" || marker == "toc2" || marker == "toc3") {
			if (book && !content.empty() && (books[book->bookId].title.empty() || marker == "h" || marker == "toc1"))
				books[book->bookId].title = content;
		} else if (marker == "mt1") {
			if (book && !content.empty()) { books[book->bookId].heading = content; ++stats.headingsImported; }
		} else if (marker == "c") {
			flush();
			try { chapter = std::stoi(content); } catch (...) { chapter = 0; }
			if (!book || chapter <= 0) { error = "invalid chapter before book in " + path; return false; }
		} else if (marker == "v") {
			flush();
			std::istringstream parts(content); std::string number; parts >> number;
			try { verseNumber = std::stoi(number); } catch (...) { verseNumber = 0; }
			if (!book || chapter <= 0 || verseNumber <= 0) { error = "invalid verse reference in " + path; return false; }
			current = { book->bookId, chapter, verseNumber, trim(content.substr(number.size())), paragraphPending, books[book->bookId].heading, {}, {}, {}, {} };
			paragraphPending = false;
			books[book->bookId].heading.clear();
		} else if (marker == "p") {
			paragraphPending = true;
			++stats.paragraphMarkers;
		} else if (marker == "f" || marker == "x") {
			/* Inline blocks are parsed when the verse is flushed. */
		} else {
			++stats.unsupportedMarkers;
		}
	}
	flush();
	return true;
}

}

bool importUsfm(const std::vector<std::string> &inputs, const std::string &output,
	const UsfmImportOptions &options, UsfmImportStats &stats, std::string &error)
{
	if (options.moduleId.empty() || options.name.empty() || options.language.empty() ||
		(options.versification != "kjv" && options.versification != "custom")) {
		error = "module-id, name, language and versification (kjv/custom) are required"; return false;
	}
	std::vector<std::string> files;
	for (const std::string &input : inputs) {
		struct stat st {};
		if (stat(input.c_str(), &st) != 0) { error = "cannot stat " + input; return false; }
		if (S_ISREG(st.st_mode)) files.push_back(input);
		else if (S_ISDIR(st.st_mode)) {
			DIR *dir = opendir(input.c_str()); if (!dir) { error = "cannot read " + input; return false; }
			while (dirent *entry = readdir(dir)) {
				std::string name = entry->d_name;
				if (name == "." || name == "..") continue;
				if ((name.size() >= 5 && name.substr(name.size()-5) == ".usfm") ||
					(name.size() >= 4 && name.substr(name.size()-4) == ".sfm"))
					files.push_back(input + "/" + name);
			}
			closedir(dir);
		} else { error = "input is not a file or directory: " + input; return false; }
	}
	std::sort(files.begin(), files.end());
	if (files.empty()) { error = "no USFM files found"; return false; }
	std::map<int, BookState> books; std::vector<Verse> verses;
	for (const auto &file : files) if (!parseFile(file, books, verses, stats, error)) return false;
	std::set<std::tuple<int,int,int>> seen;
	for (const Verse &verse : verses) if (!seen.insert({verse.book,verse.chapter,verse.verse}).second) { error = "duplicate verse reference"; return false; }
	if (verses.empty()) { error = "no verses found"; return false; }
	std::vector<SqliteImportBook> outBooks;
	for (const auto &item : books) { const auto &b = *item.second.book; outBooks.push_back({b.bookId,b.testament,b.position,b.osis,item.second.title.empty()?b.name:item.second.title,b.shortName}); }
	std::vector<SqliteImportVerse> outVerses;
	for (const Verse &v : verses) { SqliteImportVerse x; x.reference={v.book, v.book, v.chapter, v.verse}; x.text=v.text; x.paragraphBreak=v.paragraph; if(!v.heading.empty()) x.headings.push_back({v.heading}); x.spans=v.spans; x.words=v.words; x.footnotes=v.footnotes; x.crossReferences=v.crossReferences; outVerses.push_back(std::move(x)); }
	SqliteModuleMetadata metadata{options.moduleId, options.name, options.language, options.versification, options.abbreviation, options.description, options.license, options.publisher, options.source, options.contentVersion, "usfm"};
	SqliteModuleWriter writer;
	if (!writer.write(metadata, outBooks, outVerses, output, error)) return false;
	stats.books = books.size(); stats.verses = verses.size();
	for (const Verse &v : verses) stats.addedSpans += v.spans.size();
	for (const Verse &v : verses) for (const BibleWordInfo &word : v.words) {
		++stats.wordsImported;
		if (!word.strongs.empty()) ++stats.wordsWithStrong;
		stats.strongIdsImported += word.strongs.size();
	}
	std::set<std::pair<int,int>> chapters;
	for (const Verse &v : verses) chapters.insert({v.book, v.chapter});
	stats.chapters = chapters.size();
	return true;
}
