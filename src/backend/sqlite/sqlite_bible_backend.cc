#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/strong_id.h"

#include <sqlite3.h>

#include <cctype>
#include <dirent.h>
#include <limits>
#include <map>
#include <thread>
#include <utility>

namespace {

struct Statement {
	sqlite3_stmt *value = nullptr;
	~Statement() { if (value) sqlite3_finalize(value); }
};

struct Module {
	sqlite3 *db = nullptr;
	BibleModuleInfo info;
	std::string name;
	std::string versification;
	BibleModuleCapabilities capabilities;
	bool hasFts = false;
	Statement verse, chapter, bookByName, bookInfo, books, counts;
	Statement annotation, headings, spans;
	Statement words;
	Statement strongOccurrences;
	Statement next, previous, firstInBook, firstInChapter;
	Statement searchFts, searchText, searchTextCase;
	/* close_v2 permits the statement members to finalize after this body; the
	 * connection is then released automatically without leaking on errors. */
	~Module() { if (db) sqlite3_close_v2(db); }
};

bool prepare(sqlite3 *db, const char *sql, Statement &out)
{
	return sqlite3_prepare_v2(db, sql, -1, &out.value, nullptr) == SQLITE_OK;
}

void reset(Statement &statement)
{
	sqlite3_reset(statement.value);
	sqlite3_clear_bindings(statement.value);
}

std::string text(sqlite3_stmt *statement, int column)
{
	const unsigned char *value = sqlite3_column_text(statement, column);
	return value ? reinterpret_cast<const char *>(value) : std::string();
}

bool tableExists(sqlite3 *db, const char *name)
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(db,
		"SELECT 1 FROM sqlite_master WHERE name=? LIMIT 1", -1,
		&statement, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_text(statement, 1, name, -1, SQLITE_STATIC);
	const bool found = sqlite3_step(statement) == SQLITE_ROW;
	sqlite3_finalize(statement);
	return found;
}

bool indexExists(sqlite3 *db, const char *name)
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(db,
		"SELECT 1 FROM sqlite_master WHERE type='index' AND name=? LIMIT 1", -1,
		&statement, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_text(statement, 1, name, -1, SQLITE_STATIC);
	const bool found = sqlite3_step(statement) == SQLITE_ROW;
	sqlite3_finalize(statement);
	return found;
}

bool hasValidStrongData(sqlite3 *db)
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(db,
		"SELECT s.strong,vw.strong FROM verse_word_strongs s "
		"JOIN verse_words vw USING(book_id,chapter,verse,sequence)",
		-1, &statement, nullptr) != SQLITE_OK) return false;
	bool found = false;
	while (!found && sqlite3_step(statement) == SQLITE_ROW) {
		StrongId relation;
		if (!parseStrongId(text(statement, 0), relation)) continue;
		for (const StrongId &word : parseStrongIds(text(statement, 1))) {
			if (word == relation) {
				found = true;
				break;
			}
		}
	}
	sqlite3_finalize(statement);
	return found;
}

std::map<std::string, std::string> metadata(sqlite3 *db)
{
	std::map<std::string, std::string> result;
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(db, "SELECT key,value FROM metadata", -1,
		&statement, nullptr) != SQLITE_OK) return result;
	while (sqlite3_step(statement) == SQLITE_ROW)
		result[text(statement, 0)] = text(statement, 1);
	sqlite3_finalize(statement);
	return result;
}

bool hasSuffix(const std::string &value, const char *suffix)
{
	const std::size_t size = std::char_traits<char>::length(suffix);
	return value.size() >= size &&
	       value.compare(value.size() - size, size, suffix) == 0;
}

bool positiveNumber(const std::string &source, int &value)
{
	if (source.empty()) return false;
	int number = 0;
	for (char c : source) {
		if (!std::isdigit(static_cast<unsigned char>(c))) return false;
		number = number * 10 + c - '0';
	}
	if (number <= 0) return false;
	value = number;
	return true;
}

bool metadataBool(const std::map<std::string, std::string> &values,
			 const std::string &key, bool &result)
{
	auto found = values.find(key);
	if (found == values.end()) return false;
	if (found->second == "true" || found->second == "1") {
		result = true;
		return true;
	}
	if (found->second == "false" || found->second == "0") {
		result = false;
		return true;
	}
	return false;
}

bool pragmaUserVersionIsOne(sqlite3 *db)
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1,
		&statement, nullptr) != SQLITE_OK) return false;
	const bool valid = sqlite3_step(statement) == SQLITE_ROW &&
		sqlite3_column_int(statement, 0) == 1;
	sqlite3_finalize(statement);
	return valid;
}

bool hasInvalidReferences(sqlite3 *db)
{
	const char *queries[] = {
		"SELECT 1 FROM verses WHERE chapter<=0 OR verse<=0 LIMIT 1",
		"SELECT 1 FROM verses v LEFT JOIN books b ON b.book_id=v.book_id WHERE b.book_id IS NULL LIMIT 1",
		"PRAGMA foreign_key_check(verses)"
	};
	for (const char *query : queries) {
		sqlite3_stmt *statement = nullptr;
		if (sqlite3_prepare_v2(db, query, -1, &statement, nullptr) != SQLITE_OK)
			return true;
		const bool invalid = sqlite3_step(statement) == SQLITE_ROW;
		sqlite3_finalize(statement);
		if (invalid) return true;
	}
	return false;
}

bool splitReference(const std::string &key, std::string &book,
			    int &chapter, int &verse)
{
	const std::size_t space = key.find_last_of(' ');
	const std::size_t colon = space == std::string::npos
		? std::string::npos : key.find(':', space + 1);
	if (space == std::string::npos || colon == std::string::npos) return false;
	book = key.substr(0, space);
	return !book.empty() &&
	       positiveNumber(key.substr(space + 1, colon - space - 1), chapter) &&
	       positiveNumber(key.substr(colon + 1), verse);
}

std::string keyFor(const std::string &book, int chapter, int verse)
{
	return book + " " + std::to_string(chapter) + ":" + std::to_string(verse);
}

std::string osisFor(const std::string &book, int chapter, int verse)
{
	return book + "." + std::to_string(chapter) + "." + std::to_string(verse);
}

std::string ftsPhrase(const std::string &source)
{
	std::string result = "\"";
	for (char c : source) {
		if (c == '"') result += '"';
		result += c;
	}
	return result + "\"";
}

std::unique_ptr<Module> openModule(const std::string &path)
{
	std::unique_ptr<Module> module(new Module());
	if (sqlite3_open_v2(path.c_str(), &module->db,
		SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK)
		return nullptr;
	if (sqlite3_exec(module->db, "PRAGMA foreign_keys=ON", nullptr, nullptr,
		nullptr) != SQLITE_OK)
		return nullptr;
	if (!tableExists(module->db, "metadata") ||
	    !tableExists(module->db, "books") ||
	    !tableExists(module->db, "verses") ||
	    !pragmaUserVersionIsOne(module->db)) return nullptr;

	const auto values = metadata(module->db);
	auto schema = values.find("schema_version"), id = values.find("module_id");
	auto name = values.find("name"), language = values.find("language");
	auto type = values.find("module_type"), versification = values.find("versification");
	if (schema == values.end() || schema->second != "1" ||
	    id == values.end() || id->second.empty() || name == values.end() ||
	    language == values.end() || language->second.empty() ||
	    type == values.end() || type->second != "bible" ||
	    versification == values.end() ||
	    (versification->second != "kjv" && versification->second != "custom"))
		return nullptr;
	if (hasInvalidReferences(module->db)) return nullptr;
	module->info = { id->second,
		values.count("description") ? values.at("description") : std::string(),
		language->second,
			 BibleModuleType::Bible };
	module->name = name->second;
	module->versification = versification->second;
	const char *features[] = { "verses", "search", "strong", "morphology",
		"headings", "footnotes", "crossrefs", "dictionary" };
	bool *capabilities[] = { &module->capabilities.verses,
		&module->capabilities.search, &module->capabilities.strongs,
		&module->capabilities.morphology, &module->capabilities.headings,
		&module->capabilities.footnotes, &module->capabilities.crossrefs,
		&module->capabilities.dictionaryLookup };
	for (std::size_t i = 0; i < sizeof(features) / sizeof(features[0]); ++i) {
		bool value = false;
		if (!metadataBool(values, std::string("feature.") + features[i], value)) {
			/* Early v1 files may not have declared Strong. Never infer it from
			 * optional tables; an absent or malformed declaration is false. */
			if (std::string(features[i]) != "strong") return nullptr;
			value = false;
		}
		if (capabilities[i]) *capabilities[i] = value;
	}
	if (!module->capabilities.verses || !module->capabilities.search)
		return nullptr;
	module->hasFts = tableExists(module->db, "verses_fts");
	const bool hasAnnotations = tableExists(module->db, "verse_annotations");
	const bool hasHeadings = tableExists(module->db, "headings");
	const bool hasSpans = tableExists(module->db, "verse_spans");
	const bool hasWords = tableExists(module->db, "verse_words");
	const bool hasStrongIndex = tableExists(module->db, "verse_word_strongs");
	const bool hasCanonicalStrongIndex = indexExists(module->db,
		"verse_word_strongs_canonical");
	const bool declaredStrong = module->capabilities.strongs;
	module->capabilities.strongs = false;

	const bool valid =
		prepare(module->db, "SELECT text FROM verses WHERE book_id=? AND chapter=? AND verse=?", module->verse) &&
		prepare(module->db, "SELECT v.verse,v.text,b.name,b.osis FROM verses v JOIN books b USING(book_id) WHERE v.book_id=? AND v.chapter=? ORDER BY v.verse", module->chapter) &&
		prepare(module->db, "SELECT book_id,name,osis,testament FROM books WHERE name=? COLLATE NOCASE OR osis=? COLLATE NOCASE LIMIT 1", module->bookByName) &&
		prepare(module->db, "SELECT name,osis FROM books WHERE book_id=?", module->bookInfo) &&
		prepare(module->db, "SELECT name FROM books WHERE testament=? ORDER BY position", module->books) &&
		prepare(module->db, "SELECT max(chapter),max(CASE WHEN chapter=? THEN verse ELSE 0 END) FROM verses WHERE book_id=?", module->counts) &&
		prepare(module->db, "SELECT v.book_id,v.chapter,v.verse,b.name FROM verses v JOIN books b USING(book_id) WHERE (b.position,v.chapter,v.verse)>(?,?,?) ORDER BY b.position,v.chapter,v.verse LIMIT 1", module->next) &&
		prepare(module->db, "SELECT v.book_id,v.chapter,v.verse,b.name FROM verses v JOIN books b USING(book_id) WHERE (b.position,v.chapter,v.verse)<(?,?,?) ORDER BY b.position DESC,v.chapter DESC,v.verse DESC LIMIT 1", module->previous) &&
		prepare(module->db, "SELECT v.chapter,v.verse,b.name FROM verses v JOIN books b USING(book_id) WHERE v.book_id=? ORDER BY v.chapter,v.verse LIMIT 1", module->firstInBook) &&
		prepare(module->db, "SELECT v.verse,b.name FROM verses v JOIN books b USING(book_id) WHERE v.book_id=? AND v.chapter=? ORDER BY v.verse LIMIT 1", module->firstInChapter) &&
		prepare(module->db, "SELECT b.testament,v.book_id,v.chapter,v.verse,v.text,b.name,b.osis FROM verses v JOIN books b USING(book_id) WHERE instr(lower(v.text),lower(?))>0 ORDER BY b.position,v.chapter,v.verse LIMIT ? OFFSET ?", module->searchText) &&
		prepare(module->db, "SELECT b.testament,v.book_id,v.chapter,v.verse,v.text,b.name,b.osis FROM verses v JOIN books b USING(book_id) WHERE instr(v.text,?)>0 ORDER BY b.position,v.chapter,v.verse LIMIT ? OFFSET ?", module->searchTextCase);
	if (!valid) return nullptr;
	if (module->hasFts && !prepare(module->db,
		"SELECT b.testament,v.book_id,v.chapter,v.verse,v.text,b.name,b.osis FROM verses_fts f JOIN verses v ON v.rowid=f.rowid JOIN books b USING(book_id) WHERE verses_fts MATCH ? ORDER BY b.position,v.chapter,v.verse LIMIT ? OFFSET ?",
		module->searchFts)) module->hasFts = false;
	if (hasAnnotations) prepare(module->db, "SELECT paragraph_break FROM verse_annotations WHERE book_id=? AND chapter=? AND verse=?", module->annotation);
	if (hasHeadings) prepare(module->db, "SELECT text FROM headings WHERE book_id=? AND chapter=? AND verse=? ORDER BY sequence", module->headings);
	if (hasSpans) prepare(module->db, "SELECT start,length,style FROM verse_spans WHERE book_id=? AND chapter=? AND verse=? ORDER BY start", module->spans);
	if (hasWords) prepare(module->db, "SELECT start,length,text,strong FROM verse_words WHERE book_id=? AND chapter=? AND verse=? ORDER BY sequence", module->words);
	if (hasStrongIndex && hasCanonicalStrongIndex) {
		if (!prepare(module->db, "SELECT b.testament,s.book_id,s.chapter,s.verse,v.text,vw.text,b.name FROM books b CROSS JOIN verse_word_strongs s INDEXED BY verse_word_strongs_canonical JOIN verse_words vw USING(book_id,chapter,verse,sequence) JOIN verses v USING(book_id,chapter,verse) WHERE s.strong=? AND s.book_id=b.book_id ORDER BY b.position,s.chapter,s.verse,s.sequence LIMIT ? OFFSET ?", module->strongOccurrences))
			prepare(module->db, "SELECT b.testament,v.book_id,v.chapter,v.verse,v.text,vw.text,b.name FROM verse_word_strongs s JOIN verses v USING(book_id,chapter,verse) JOIN verse_words vw USING(book_id,chapter,verse,sequence) JOIN books b USING(book_id) WHERE s.strong=? ORDER BY b.position,v.chapter,v.verse,vw.sequence LIMIT ? OFFSET ?", module->strongOccurrences);
	} else if (hasStrongIndex)
		prepare(module->db, "SELECT b.testament,v.book_id,v.chapter,v.verse,v.text,vw.text,b.name FROM verse_word_strongs s JOIN verses v USING(book_id,chapter,verse) JOIN verse_words vw USING(book_id,chapter,verse,sequence) JOIN books b USING(book_id) WHERE s.strong=? ORDER BY b.position,v.chapter,v.verse,vw.sequence LIMIT ? OFFSET ?", module->strongOccurrences);
	module->capabilities.strongs = declaredStrong && hasWords && hasStrongIndex &&
		module->words.value && module->strongOccurrences.value &&
		hasValidStrongData(module->db);
	return module;
}

} // namespace

struct SqliteBibleBackend::Impl {
	std::map<std::string, std::unique_ptr<Module>> modules;
	std::thread::id owner = std::this_thread::get_id();
	bool sameThread() const { return owner == std::this_thread::get_id(); }
	Module *find(const std::string &id) const
	{
		if (!sameThread()) return nullptr;
		auto found = modules.find(id);
		return found == modules.end() ? nullptr : found->second.get();
	}
};

SqliteBibleBackend::SqliteBibleBackend(const std::string &directory)
	: impl_(new Impl())
{
	DIR *dir = opendir(directory.c_str());
	if (!dir) return;
	while (dirent *entry = readdir(dir)) {
		const std::string filename = entry->d_name;
		if (!hasSuffix(filename, ".sqlite")) continue;
		std::string path = directory;
		if (!path.empty() && path.back() != '/') path += '/';
		std::unique_ptr<Module> module = openModule(path + filename);
		if (module && impl_->modules.find(module->info.id) == impl_->modules.end())
			impl_->modules[module->info.id] = std::move(module);
	}
	closedir(dir);
}

SqliteBibleBackend::~SqliteBibleBackend() = default;

std::vector<BibleModuleInfo> SqliteBibleBackend::listModules() const
{
	std::vector<BibleModuleInfo> result;
	if (!impl_->sameThread()) return result;
	for (const auto &item : impl_->modules) result.push_back(item.second->info);
	return result;
}

bool SqliteBibleBackend::hasModule(const std::string &id) const { return impl_->find(id); }
BibleModuleType SqliteBibleBackend::moduleType(const std::string &id) const
{
	Module *module = impl_->find(id);
	return module ? BibleModuleType::Bible : BibleModuleType::Unknown;
}
BibleModuleCapabilities SqliteBibleBackend::moduleCapabilities(const std::string &id) const
{
	BibleModuleCapabilities result;
	Module *module = impl_->find(id);
	if (module) result = module->capabilities;
	return result;
}
std::string SqliteBibleBackend::moduleDescription(const std::string &id) const
{
	Module *module = impl_->find(id);
	return module ? module->info.description : std::string();
}
std::string SqliteBibleBackend::moduleLanguage(const std::string &id) const
{
	Module *module = impl_->find(id);
	return module ? module->info.language : std::string();
}

bool SqliteBibleBackend::resolveKey(const std::string &id, const std::string &key,
				    BibleKeyInfo &result)
{
	Module *module = impl_->find(id);
	std::string bookName;
	int chapter = 0, verse = 0;
	if (!module || !splitReference(key, bookName, chapter, verse)) return false;
	reset(module->bookByName);
	sqlite3_bind_text(module->bookByName.value, 1, bookName.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(module->bookByName.value, 2, bookName.c_str(), -1, SQLITE_TRANSIENT);
	if (sqlite3_step(module->bookByName.value) != SQLITE_ROW) return false;
	const int bookId = sqlite3_column_int(module->bookByName.value, 0);
	const std::string canonical = text(module->bookByName.value, 1);
	const int testament = sqlite3_column_int(module->bookByName.value, 3);
	reset(module->verse);
	sqlite3_bind_int(module->verse.value, 1, bookId);
	sqlite3_bind_int(module->verse.value, 2, chapter);
	sqlite3_bind_int(module->verse.value, 3, verse);
	if (sqlite3_step(module->verse.value) != SQLITE_ROW) return false;
	result = BibleKeyInfo();
	result.reference = { testament, bookId, chapter, verse };
	result.key = keyFor(canonical, chapter, verse);
	result.bookName = canonical;
	result.bookIndex = bookId;
	reset(module->counts);
	sqlite3_bind_int(module->counts.value, 1, chapter);
	sqlite3_bind_int(module->counts.value, 2, bookId);
	if (sqlite3_step(module->counts.value) == SQLITE_ROW) {
		result.chapterCount = sqlite3_column_int(module->counts.value, 0);
		result.verseCount = sqlite3_column_int(module->counts.value, 1);
	}
	return true;
}

std::string SqliteBibleBackend::osisRefFromKey(const std::string &id,
						const std::string &key)
{
	BibleKeyInfo info;
	if (!resolveKey(id, key, info)) return {};
	Module *module = impl_->find(id);
	reset(module->bookInfo);
	sqlite3_bind_int(module->bookInfo.value, 1, info.reference.book);
	if (sqlite3_step(module->bookInfo.value) != SQLITE_ROW) return {};
	return osisFor(text(module->bookInfo.value, 1),
		info.reference.chapter, info.reference.verse);
}

BibleVerseContent SqliteBibleBackend::getVerseContent(
	const std::string &id, const BibleReference &reference, bool)
{
	BibleVerseContent result;
	Module *module = impl_->find(id);
	if (!module) return result;
	reset(module->verse);
	sqlite3_bind_int(module->verse.value, 1, reference.book);
	sqlite3_bind_int(module->verse.value, 2, reference.chapter);
	sqlite3_bind_int(module->verse.value, 3, reference.verse);
	if (sqlite3_step(module->verse.value) != SQLITE_ROW) return result;
	result.reference = reference;
	result.plainText = text(module->verse.value, 0);
	result.renderedText = result.plainText;
	if (module->annotation.value) {
		reset(module->annotation);
		sqlite3_bind_int(module->annotation.value, 1, reference.book);
		sqlite3_bind_int(module->annotation.value, 2, reference.chapter);
		sqlite3_bind_int(module->annotation.value, 3, reference.verse);
		if (sqlite3_step(module->annotation.value) == SQLITE_ROW)
			result.paragraphBreak = sqlite3_column_int(module->annotation.value, 0) != 0;
	}
	if (module->headings.value) {
		reset(module->headings);
		sqlite3_bind_int(module->headings.value, 1, reference.book);
		sqlite3_bind_int(module->headings.value, 2, reference.chapter);
		sqlite3_bind_int(module->headings.value, 3, reference.verse);
		while (sqlite3_step(module->headings.value) == SQLITE_ROW)
			result.headings.push_back({ text(module->headings.value, 0) });
	}
	if (module->spans.value) {
		reset(module->spans);
		sqlite3_bind_int(module->spans.value, 1, reference.book);
		sqlite3_bind_int(module->spans.value, 2, reference.chapter);
		sqlite3_bind_int(module->spans.value, 3, reference.verse);
		while (sqlite3_step(module->spans.value) == SQLITE_ROW) {
			BibleTextSpan span;
			span.start = sqlite3_column_int(module->spans.value, 0);
			span.length = sqlite3_column_int(module->spans.value, 1);
			if (text(module->spans.value, 2) == "added") result.spans.push_back(span);
		}
	}
	if (module->words.value) {
		reset(module->words);
		sqlite3_bind_int(module->words.value, 1, reference.book);
		sqlite3_bind_int(module->words.value, 2, reference.chapter);
		sqlite3_bind_int(module->words.value, 3, reference.verse);
		while (sqlite3_step(module->words.value) == SQLITE_ROW) {
			BibleWordInfo word;
			word.start = sqlite3_column_int(module->words.value, 0);
			word.length = sqlite3_column_int(module->words.value, 1);
			word.text = text(module->words.value, 2);
			word.strong = text(module->words.value, 3);
			word.strongs = parseStrongIds(word.strong);
			result.words.push_back(std::move(word));
		}
	}
	result.valid = true;
	return result;
}

std::vector<BibleVerse> SqliteBibleBackend::getChapter(
	const std::string &id, const BibleReference &reference, bool)
{
	std::vector<BibleVerse> result;
	Module *module = impl_->find(id);
	if (!module) return result;
	reset(module->chapter);
	sqlite3_bind_int(module->chapter.value, 1, reference.book);
	sqlite3_bind_int(module->chapter.value, 2, reference.chapter);
	while (sqlite3_step(module->chapter.value) == SQLITE_ROW) {
		BibleVerse verse;
		verse.reference = { reference.testament, reference.book, reference.chapter,
			sqlite3_column_int(module->chapter.value, 0) };
		verse.text = text(module->chapter.value, 1);
		verse.key = keyFor(text(module->chapter.value, 2), reference.chapter,
			verse.reference.verse);
		verse.osisRef = osisFor(text(module->chapter.value, 3), reference.chapter,
			verse.reference.verse);
		result.push_back(std::move(verse));
	}
	return result;
}

std::string navigationResult(Statement &statement, const BibleReference &ref,
				     const std::string &fallback)
{
	reset(statement);
	sqlite3_bind_int(statement.value, 1, ref.book);
	sqlite3_bind_int(statement.value, 2, ref.chapter);
	sqlite3_bind_int(statement.value, 3, ref.verse);
	if (sqlite3_step(statement.value) != SQLITE_ROW) return fallback;
	return keyFor(text(statement.value, 3), sqlite3_column_int(statement.value, 1),
		sqlite3_column_int(statement.value, 2));
}

std::string SqliteBibleBackend::navigate(const std::string &id,
					 const std::string &key, int direction)
{
	BibleKeyInfo info;
	Module *module = impl_->find(id);
	if (!module || direction == 0 || !resolveKey(id, key, info)) return {};
	return navigationResult(direction < 0 ? module->previous : module->next,
		info.reference, info.key);
}

std::string SqliteBibleBackend::setBook(const std::string &id,
					const std::string &, int testament, int book)
{
	Module *module = impl_->find(id);
	if (!module) return {};
	reset(module->firstInBook);
	sqlite3_bind_int(module->firstInBook.value, 1, book);
	if (sqlite3_step(module->firstInBook.value) != SQLITE_ROW) return {};
	return keyFor(text(module->firstInBook.value, 2),
		sqlite3_column_int(module->firstInBook.value, 0),
		sqlite3_column_int(module->firstInBook.value, 1));
}

std::string SqliteBibleBackend::setChapter(const std::string &id,
					   const std::string &key, int chapter)
{
	BibleKeyInfo info;
	if (chapter <= 0 || !resolveKey(id, key, info)) return {};
	Module *module = impl_->find(id);
	reset(module->firstInChapter);
	sqlite3_bind_int(module->firstInChapter.value, 1, info.reference.book);
	sqlite3_bind_int(module->firstInChapter.value, 2, chapter);
	std::string result;
	if (sqlite3_step(module->firstInChapter.value) == SQLITE_ROW)
		result = keyFor(text(module->firstInChapter.value, 1), chapter,
			sqlite3_column_int(module->firstInChapter.value, 0));
	return result;
}

std::string SqliteBibleBackend::setVerse(const std::string &id,
					 const std::string &key, int verse)
{
	BibleKeyInfo info;
	if (verse <= 0 || !resolveKey(id, key, info)) return {};
	BibleReference target = info.reference;
	target.verse = verse;
	if (!getVerseContent(id, target).valid) return {};
	return keyFor(info.bookName, target.chapter, verse);
}

std::vector<std::string> SqliteBibleBackend::bookNames(
	const std::string &id, int testament) const
{
	std::vector<std::string> result;
	Module *module = impl_->find(id);
	if (!module) return result;
	reset(module->books);
	sqlite3_bind_int(module->books.value, 1, testament);
	while (sqlite3_step(module->books.value) == SQLITE_ROW)
		result.push_back(text(module->books.value, 0));
	return result;
}

std::vector<BibleSearchResult> SqliteBibleBackend::search(
	const std::string &id, const BibleSearchQuery &query)
{
	std::vector<BibleSearchResult> result;
	Module *module = impl_->find(id);
	if (!module || query.text.empty() || query.mode == BibleSearchMode::Regex ||
	    query.mode == BibleSearchMode::Attribute) return result;
	Statement *statement;
	std::string queryText = query.text;
	if (!query.caseSensitive && module->hasFts &&
	    (query.mode == BibleSearchMode::MultiWord ||
	     query.mode == BibleSearchMode::Indexed)) {
		statement = &module->searchFts;
	} else if (!query.caseSensitive && module->hasFts &&
		   query.mode == BibleSearchMode::Phrase) {
		statement = &module->searchFts;
		queryText = ftsPhrase(query.text);
	} else {
		statement = query.caseSensitive ? &module->searchTextCase
						: &module->searchText;
	}
	reset(*statement);
	sqlite3_bind_text(statement->value, 1, queryText.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(statement->value, 2, static_cast<sqlite3_int64>(query.limit));
	sqlite3_bind_int64(statement->value, 3, static_cast<sqlite3_int64>(query.offset));
	while (sqlite3_step(statement->value) == SQLITE_ROW) {
		BibleSearchResult item;
		item.module = id;
		item.reference = { sqlite3_column_int(statement->value, 0),
			sqlite3_column_int(statement->value, 1),
			sqlite3_column_int(statement->value, 2),
			sqlite3_column_int(statement->value, 3) };
		item.text = text(statement->value, 4);
		item.key = keyFor(text(statement->value, 5), item.reference.chapter,
			item.reference.verse);
		item.osisRef = osisFor(text(statement->value, 6), item.reference.chapter,
			item.reference.verse);
		result.push_back(std::move(item));
	}
	return result;
}

DictionaryEntry SqliteBibleBackend::lookupDictionary(const std::string &,
						       const std::string &)
{
	return {};
}

StrongOccurrencePage SqliteBibleBackend::findStrongOccurrencePage(
	const std::string &id, const StrongId &strong, std::size_t limit, std::size_t offset)
{
	StrongOccurrencePage result;
	Module *module = impl_->find(id);
	if (!module || !module->strongOccurrences.value) return result;
	const std::string value = formatStrongId(strong);
	const std::size_t fetchLimit = limit == std::numeric_limits<std::size_t>::max()
		? limit : limit + 1;
	reset(module->strongOccurrences);
	sqlite3_bind_text(module->strongOccurrences.value, 1, value.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(module->strongOccurrences.value, 2, static_cast<sqlite3_int64>(fetchLimit));
	sqlite3_bind_int64(module->strongOccurrences.value, 3, static_cast<sqlite3_int64>(offset));
	while (sqlite3_step(module->strongOccurrences.value) == SQLITE_ROW) {
		StrongOccurrence occurrence;
		occurrence.reference = {sqlite3_column_int(module->strongOccurrences.value, 0), sqlite3_column_int(module->strongOccurrences.value, 1),
			sqlite3_column_int(module->strongOccurrences.value, 2),
			sqlite3_column_int(module->strongOccurrences.value, 3)};
		occurrence.context = text(module->strongOccurrences.value, 4);
		occurrence.word = text(module->strongOccurrences.value, 5);
		occurrence.key = keyFor(text(module->strongOccurrences.value, 6),
			occurrence.reference.chapter, occurrence.reference.verse);
		occurrence.strong = strong;
		result.occurrences.push_back(std::move(occurrence));
	}
	result.hasMore = result.occurrences.size() > limit;
	if (result.hasMore) result.occurrences.resize(limit);
	return result;
}
