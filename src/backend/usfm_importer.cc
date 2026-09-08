#include "backend/usfm_importer.h"
#include "backend/strong_id.h"
#include "backend/bible_book_map.h"

#include <sqlite3.h>

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
struct Verse { int book = 0, chapter = 0, verse = 0; std::string text; bool paragraph = false; std::string heading; std::vector<BibleTextSpan> spans; std::vector<BibleWordInfo> words; };
struct BookState { const BibleBookDefinition *book = nullptr; std::string title; std::string heading; };

std::string trim(const std::string &s)
{
	const std::size_t first = s.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return {};
	const std::size_t last = s.find_last_not_of(" \t\r\n");
	return s.substr(first, last - first + 1);
}

std::string cleanInline(const std::string &source, UsfmImportStats &stats,
	std::vector<BibleTextSpan> &spans, std::vector<BibleWordInfo> &words)
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
					const std::string key = "strong=\"";
					const std::size_t strongStart = attributeText.find(key);
					if (strongStart != std::string::npos) {
						const std::size_t valueStart = strongStart + key.size();
						const std::size_t valueEnd = attributeText.find('"', valueStart);
						info.strong = attributeText.substr(valueStart, valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
						info.strongs = parseStrongIds(info.strong);
					}
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
			if (marker == "f") ++stats.footnotesSkipped; else ++stats.crossReferencesSkipped;
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
	return trim(out);
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
		current.text = cleanInline(current.text, stats, current.spans, current.words);
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
			current = { book->bookId, chapter, verseNumber, trim(content.substr(number.size())), paragraphPending, books[book->bookId].heading, {}, {} };
			paragraphPending = false;
			books[book->bookId].heading.clear();
		} else if (marker == "p") {
			paragraphPending = true;
			++stats.paragraphMarkers;
		} else if (marker == "f" || marker == "x") {
			if (marker == "f") ++stats.footnotesSkipped; else ++stats.crossReferencesSkipped;
		} else {
			++stats.unsupportedMarkers;
		}
	}
	flush();
	return true;
}

bool exec(sqlite3 *db, const std::string &sql, std::string &error)
{
	char *message = nullptr;
	if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &message) != SQLITE_OK) {
		error = message ? message : "SQLite error"; sqlite3_free(message); return false;
	}
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
	bool hasStrong = false;
	for (const Verse &verse : verses) for (const BibleWordInfo &word : verse.words)
		if (!word.strongs.empty()) { hasStrong = true; break; }
	const std::string temporary = output + ".tmp";
	std::remove(temporary.c_str());
	sqlite3 *db = nullptr;
	if (sqlite3_open(temporary.c_str(), &db) != SQLITE_OK) { error = "cannot create output"; if (db) sqlite3_close(db); return false; }
	bool ok = exec(db, "PRAGMA user_version=1; PRAGMA foreign_keys=ON; BEGIN IMMEDIATE;", error);
	ok = ok && exec(db, "CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL); CREATE TABLE books(book_id INTEGER PRIMARY KEY,osis TEXT NOT NULL UNIQUE,name TEXT NOT NULL,short_name TEXT,testament INTEGER NOT NULL,position INTEGER NOT NULL UNIQUE); CREATE TABLE verses(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL CHECK(chapter>0),verse INTEGER NOT NULL CHECK(verse>0),text TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse),FOREIGN KEY(book_id) REFERENCES books(book_id)); CREATE INDEX verses_book_chapter ON verses(book_id,chapter,verse); CREATE TABLE verse_annotations(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,paragraph_break INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(book_id,chapter,verse),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse)); CREATE TABLE headings(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,text TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,sequence),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse)); CREATE TABLE verse_spans(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,start INTEGER NOT NULL,length INTEGER NOT NULL,style TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,start,style),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse)); CREATE TABLE verse_words(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,start INTEGER NOT NULL,length INTEGER NOT NULL,text TEXT NOT NULL,strong TEXT,PRIMARY KEY(book_id,chapter,verse,sequence),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse)); CREATE TABLE verse_word_strongs(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,sequence INTEGER NOT NULL,strong TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,sequence,strong),FOREIGN KEY(book_id,chapter,verse,sequence) REFERENCES verse_words(book_id,chapter,verse,sequence)); CREATE INDEX verse_word_strongs_canonical ON verse_word_strongs(strong,book_id,chapter,verse,sequence); CREATE VIRTUAL TABLE verses_fts USING fts5(text,content='verses',content_rowid='rowid');", error);
	sqlite3_stmt *statement = nullptr;
	if (ok && sqlite3_prepare_v2(db, "INSERT INTO metadata(key,value) VALUES(?,?)", -1, &statement, nullptr) == SQLITE_OK) {
		std::map<std::string,std::string> metadata = {{"schema_version","1"},{"module_id",options.moduleId},{"name",options.name},{"language",options.language},{"module_type","bible"},{"versification",options.versification},{"source_format","usfm"},{"feature.verses","true"},{"feature.search","true"},{"feature.strong",hasStrong ? "true" : "false"},{"feature.morphology","false"},{"feature.headings","false"},{"feature.footnotes","false"},{"feature.crossrefs","false"},{"feature.dictionary","false"}};
		if (!options.abbreviation.empty()) metadata["abbreviation"] = options.abbreviation;
		if (!options.description.empty()) metadata["description"] = options.description;
		if (!options.license.empty()) metadata["license"] = options.license;
		if (!options.publisher.empty()) metadata["publisher"] = options.publisher;
		if (!options.source.empty()) metadata["source"] = options.source;
		if (!options.contentVersion.empty()) metadata["content_version"] = options.contentVersion;
		metadata["feature.headings"] = stats.headingsImported ? "true" : "false";
		for (const auto &item : metadata) { sqlite3_bind_text(statement,1,item.first.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(statement,2,item.second.c_str(),-1,SQLITE_TRANSIENT); if (sqlite3_step(statement)!=SQLITE_DONE) { ok=false; break; } sqlite3_reset(statement); sqlite3_clear_bindings(statement); }
	} else ok = false;
	if (statement) sqlite3_finalize(statement);
	if (ok && sqlite3_prepare_v2(db, "INSERT INTO books VALUES(?,?,?,?,?,?)", -1, &statement, nullptr) == SQLITE_OK) {
		for (const auto &item : books) { const auto &b=*item.second.book; sqlite3_bind_int(statement,1,b.bookId); sqlite3_bind_text(statement,2,b.osis,-1,SQLITE_STATIC); sqlite3_bind_text(statement,3,(item.second.title.empty()?b.name:item.second.title).c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(statement,4,b.shortName,-1,SQLITE_STATIC); sqlite3_bind_int(statement,5,b.testament); sqlite3_bind_int(statement,6,b.position); if (sqlite3_step(statement)!=SQLITE_DONE) { ok=false; break; } sqlite3_reset(statement); sqlite3_clear_bindings(statement); }
	} else ok = false;
	if (statement) sqlite3_finalize(statement);
	if (ok && sqlite3_prepare_v2(db, "INSERT INTO verses VALUES(?,?,?,?)", -1, &statement, nullptr) == SQLITE_OK) {
		for (const Verse &v : verses) { sqlite3_bind_int(statement,1,v.book); sqlite3_bind_int(statement,2,v.chapter); sqlite3_bind_int(statement,3,v.verse); sqlite3_bind_text(statement,4,v.text.c_str(),-1,SQLITE_TRANSIENT); if (sqlite3_step(statement)!=SQLITE_DONE) { ok=false; break; } sqlite3_reset(statement); sqlite3_clear_bindings(statement); }
	} else ok = false;
	if (statement) sqlite3_finalize(statement);
	if (ok && sqlite3_prepare_v2(db, "INSERT INTO verse_annotations VALUES(?,?,?,?)", -1, &statement, nullptr) == SQLITE_OK) {
		for (const Verse &v : verses) if (v.paragraph) { sqlite3_bind_int(statement,1,v.book); sqlite3_bind_int(statement,2,v.chapter); sqlite3_bind_int(statement,3,v.verse); sqlite3_bind_int(statement,4,1); if (sqlite3_step(statement)!=SQLITE_DONE) { ok=false; break; } sqlite3_reset(statement); sqlite3_clear_bindings(statement); }
	} else ok = false;
	if (statement) sqlite3_finalize(statement);
	if (ok && sqlite3_prepare_v2(db, "INSERT INTO headings VALUES(?,?,?,?,?)", -1, &statement, nullptr) == SQLITE_OK) {
		for (const Verse &v : verses) if (!v.heading.empty()) { sqlite3_bind_int(statement,1,v.book); sqlite3_bind_int(statement,2,v.chapter); sqlite3_bind_int(statement,3,v.verse); sqlite3_bind_int(statement,4,0); sqlite3_bind_text(statement,5,v.heading.c_str(),-1,SQLITE_TRANSIENT); if (sqlite3_step(statement)!=SQLITE_DONE) { ok=false; break; } sqlite3_reset(statement); sqlite3_clear_bindings(statement); }
	} else ok = false;
	if (statement) sqlite3_finalize(statement);
	if (ok && sqlite3_prepare_v2(db, "INSERT INTO verse_spans VALUES(?,?,?,?,?,?)", -1, &statement, nullptr) == SQLITE_OK) {
		for (const Verse &v : verses) for (const BibleTextSpan &span : v.spans) { sqlite3_bind_int(statement,1,v.book); sqlite3_bind_int(statement,2,v.chapter); sqlite3_bind_int(statement,3,v.verse); sqlite3_bind_int(statement,4,span.start); sqlite3_bind_int(statement,5,span.length); sqlite3_bind_text(statement,6,"added",-1,SQLITE_STATIC); if (sqlite3_step(statement)!=SQLITE_DONE) { ok=false; break; } sqlite3_reset(statement); sqlite3_clear_bindings(statement); }
	} else ok = false;
	if (statement) sqlite3_finalize(statement);
	if (ok && sqlite3_prepare_v2(db, "INSERT INTO verse_words VALUES(?,?,?,?,?,?,?,?)", -1, &statement, nullptr) == SQLITE_OK) {
		for (const Verse &v : verses) { int sequence = 0; for (const BibleWordInfo &word : v.words) { sqlite3_bind_int(statement,1,v.book); sqlite3_bind_int(statement,2,v.chapter); sqlite3_bind_int(statement,3,v.verse); sqlite3_bind_int(statement,4,sequence++); sqlite3_bind_int(statement,5,word.start); sqlite3_bind_int(statement,6,word.length); sqlite3_bind_text(statement,7,word.text.c_str(),-1,SQLITE_TRANSIENT); if (word.strong.empty()) sqlite3_bind_null(statement,8); else sqlite3_bind_text(statement,8,word.strong.c_str(),-1,SQLITE_TRANSIENT); if (sqlite3_step(statement)!=SQLITE_DONE) { ok=false; break; } sqlite3_reset(statement); sqlite3_clear_bindings(statement); } }
	} else ok = false;
	if (statement) sqlite3_finalize(statement);
	if (ok && sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO verse_word_strongs VALUES(?,?,?,?,?)", -1, &statement, nullptr) == SQLITE_OK) {
		for (const Verse &v : verses) { int sequence = 0; for (const BibleWordInfo &word : v.words) { for (const StrongId &id : word.strongs) { sqlite3_bind_int(statement,1,v.book); sqlite3_bind_int(statement,2,v.chapter); sqlite3_bind_int(statement,3,v.verse); sqlite3_bind_int(statement,4,sequence); std::string strong = formatStrongId(id); sqlite3_bind_text(statement,5,strong.c_str(),-1,SQLITE_TRANSIENT); if (sqlite3_step(statement)!=SQLITE_DONE) { ok=false; error=sqlite3_errmsg(db); break; } sqlite3_reset(statement); sqlite3_clear_bindings(statement); } ++sequence; } }
	} else ok = false;
	if (statement) sqlite3_finalize(statement);
	if (ok) ok = exec(db, "INSERT INTO verses_fts(rowid,text) SELECT rowid,text FROM verses; COMMIT;", error); else exec(db, "ROLLBACK", error);
	sqlite3_close(db);
	if (!ok) { std::remove(temporary.c_str()); if (error.empty()) error="failed writing SQLite module"; return false; }
	if (std::rename(temporary.c_str(), output.c_str()) != 0) { error = "cannot finalize output: " + std::string(std::strerror(errno)); std::remove(temporary.c_str()); return false; }
	stats.books = books.size(); stats.verses = verses.size();
	for (const Verse &v : verses) stats.addedSpans += v.spans.size();
	for (const Verse &v : verses) for (const BibleWordInfo &word : v.words) {
		++stats.wordsImported;
		if (!word.strong.empty()) ++stats.wordsWithStrong;
	}
	std::set<std::pair<int,int>> chapters;
	for (const Verse &v : verses) chapters.insert({v.book, v.chapter});
	stats.chapters = chapters.size();
	return true;
}
