#include "backend/editable_edition.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <tuple>
#include <fstream>
#include <sstream>

#include <glib.h>
#include <glib/gstdio.h>

namespace {
bool exec(sqlite3 *db, const char *sql, std::string &error)
{
	char *message = nullptr;
	if (sqlite3_exec(db, sql, nullptr, nullptr, &message) == SQLITE_OK)
		return true;
	error = message ? message : sqlite3_errmsg(db);
	sqlite3_free(message);
	return false;
}

bool bind(sqlite3_stmt *statement, int index, const std::string &value)
{
	return sqlite3_bind_text(statement, index, value.data(),
				 static_cast<int>(value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool validReference(const BibleReference &reference)
{
	return reference.book > 0 && reference.chapter > 0 && reference.verse > 0;
}

bool validUtf8(const std::string &value)
{
	return g_utf8_validate(value.data(), static_cast<gssize>(value.size()), nullptr);
}

std::string fingerprint(const std::vector<SqliteImportBook> &books,
			const std::vector<SqliteImportVerse> &verses)
{
	GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
	auto add = [checksum](const std::string &value) {
		guint64 length = value.size();
		guint8 prefix[8];
		for (int i = 7; i >= 0; --i) { prefix[i] = length & 0xff; length >>= 8; }
		g_checksum_update(checksum, prefix, sizeof(prefix));
		g_checksum_update(checksum,
			reinterpret_cast<const guchar *>(value.data()), value.size());
	};
	for (const auto &book : books) {
		add(std::to_string(book.id));
		add(std::to_string(book.position));
		add(book.osis);
		add(book.name);
	}
	for (const auto &verse : verses) {
		add(std::to_string(verse.reference.book));
		add(std::to_string(verse.reference.chapter));
		add(std::to_string(verse.reference.verse));
		add(verse.text);
	}
	const std::string result = g_checksum_get_string(checksum);
	g_checksum_free(checksum);
	return result;
}

bool readMetadata(sqlite3 *db, EditableEditionMetadata &m, std::string &error)
{
	static const char *sql = "SELECT key,value FROM metadata";
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) {
		error = sqlite3_errmsg(db);
		return false;
	}
	std::map<std::string, std::string> values;
	while (sqlite3_step(s) == SQLITE_ROW) {
		const auto *key = reinterpret_cast<const char *>(sqlite3_column_text(s, 0));
		const auto *value = reinterpret_cast<const char *>(sqlite3_column_text(s, 1));
		if (key && value) values[key] = value;
	}
	sqlite3_finalize(s);
	auto get = [&values](const char *key) { return values[key]; };
	if (get("edition_format") != "biblia-elim-edition" ||
	    get("edition_version") != "1") {
		error = "unsupported editable edition format";
		return false;
	}
	m.projectId = get("project_id");
	m.name = get("name");
	m.language = get("language");
	m.versification = get("versification");
	m.sourceModuleId = get("source_module_id");
	m.sourceName = get("source_name");
	m.sourceLicense = get("source_license");
	m.sourceAttribution = get("source_attribution");
	m.sourceFingerprint = get("source_fingerprint");
	if (m.projectId.empty() || m.name.empty() || m.language.empty() ||
	    m.sourceModuleId.empty() || m.sourceFingerprint.empty()) {
		error = "editable edition metadata is incomplete";
		return false;
	}
	return true;
}

bool readValue(sqlite3 *db, const char *key, std::string &value)
{
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db, "SELECT value FROM metadata WHERE key=?", -1,
			       &s, nullptr) != SQLITE_OK) return false;
	bind(s, 1, key);
	const int rc = sqlite3_step(s);
	if (rc == SQLITE_ROW) {
		const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(s, 0));
		value = text ? text : "";
	}
	sqlite3_finalize(s);
	return rc == SQLITE_ROW;
}

bool setValue(sqlite3 *db, const char *key, const std::string &value,
	      std::string &error)
{
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db, "INSERT INTO metadata(key,value) VALUES(?,?) "
			       "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
			       -1, &s, nullptr) != SQLITE_OK) {
		error = sqlite3_errmsg(db); return false;
	}
	const bool ok = bind(s, 1, key) && bind(s, 2, value) &&
		sqlite3_step(s) == SQLITE_DONE;
	if (!ok) error = sqlite3_errmsg(db);
	sqlite3_finalize(s);
	return ok;
}

bool tableExists(sqlite3 *db, const char *name)
{
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db,
		"SELECT 1 FROM sqlite_master WHERE type IN ('table','view') AND name=?",
		-1, &s, nullptr) != SQLITE_OK)
		return false;
	sqlite3_bind_text(s, 1, name, -1, SQLITE_STATIC);
	const bool found = sqlite3_step(s) == SQLITE_ROW;
	sqlite3_finalize(s);
	return found;
}

std::string originalTextExpression(sqlite3 *db)
{
 if (tableExists(db, "edition_base_verses"))
  return "(SELECT b.text FROM edition_base_verses b WHERE b.book_id=v.book_id AND b.chapter=v.chapter AND b.verse=v.verse)";
 if (tableExists(db, "verse_revisions"))
  return "COALESCE((SELECT r.old_text FROM verse_revisions r WHERE r.book_id=v.book_id AND r.chapter=v.chapter AND r.verse=v.verse ORDER BY r.revision_id LIMIT 1),v.text)";
 return "v.text";
}

// Called within the creator's transaction or a migration savepoint. The
// earliest revision reconstructs the original for pre-snapshot projects.
bool ensureOriginalText(sqlite3 *db, std::string &error)
{
 if (tableExists(db, "edition_base_verses")) return true;
 const std::string expression = originalTextExpression(db);
 const std::string sql = "SAVEPOINT edition_baseline;"
  "CREATE TABLE edition_base_verses(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,"
  "verse INTEGER NOT NULL,text TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse),"
  "FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse));"
  "INSERT INTO edition_base_verses SELECT v.book_id,v.chapter,v.verse," + expression + " FROM verses v;"
  "RELEASE edition_baseline;";
 if (exec(db, sql.c_str(), error)) return true;
 std::string ignored;
 exec(db, "ROLLBACK TO edition_baseline; RELEASE edition_baseline", ignored);
 return false;
}

const char *editorialSchema =
	"CREATE TABLE IF NOT EXISTS edition_glossary("
	"source_term TEXT PRIMARY KEY,target_term TEXT NOT NULL,note TEXT NOT NULL DEFAULT '');"
	"CREATE TABLE IF NOT EXISTS editorial_decisions("
	"book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,"
	"proposal TEXT NOT NULL,rationale TEXT NOT NULL,evidence TEXT NOT NULL,"
	"confidence TEXT NOT NULL,status TEXT NOT NULL,modified INTEGER NOT NULL,"
	"PRIMARY KEY(book_id,chapter,verse),"
	"FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse));"
	"CREATE TABLE IF NOT EXISTS verse_revisions("
	"revision_id INTEGER PRIMARY KEY,book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,"
	"verse INTEGER NOT NULL,old_text TEXT NOT NULL,new_text TEXT NOT NULL,modified INTEGER NOT NULL);"
	"CREATE INDEX IF NOT EXISTS verse_revisions_reference ON verse_revisions(book_id,chapter,verse,revision_id);";
}

bool EditableEdition::sourceMayBeDerived(const std::string &moduleId,
					 const std::string &license)
{
	/* Eligibility is intentionally an exact allowlist. An arbitrary module's
	 * self-reported license string is not sufficient proof of redistribution. */
	static const std::map<std::string, std::set<std::string>> approved = {
		{"TorresAmat", {"Public Domain"}},
		{"SpaRV", {"Public Domain"}},
		{"KJV", {"GPL"}},
		{"KorRV", {"Public Domain"}},
		{"ChiUns", {"Public Domain"}},
		{"PorAlmeida1911", {"GPL"}},
	};
	auto found = approved.find(moduleId);
	return found != approved.end() && found->second.count(license) != 0;
}

bool EditableEdition::create(const std::string &path,
			     const EditableEditionMetadata &input,
			     const std::vector<SqliteImportBook> &books,
			     const std::vector<SqliteImportVerse> &verses,
			     std::string &error)
{
	if (path.empty() || input.projectId.empty() || input.name.empty() ||
	    input.language.empty() || input.sourceModuleId.empty() ||
	    input.sourceName.empty() || input.sourceAttribution.empty() ||
	    !sourceMayBeDerived(input.sourceModuleId, input.sourceLicense) ||
	    books.empty() || verses.empty()) {
		error = "incomplete project or source is not approved for redistribution";
		return false;
	}
	if (!validUtf8(input.name) || !validUtf8(input.sourceName) ||
	    !validUtf8(input.sourceAttribution)) {
		error = "project metadata must be valid UTF-8";
		return false;
	}
	if (g_file_test(path.c_str(), G_FILE_TEST_EXISTS)) {
		error = "editable edition already exists";
		return false;
	}
	std::set<int> bookIds;
	for (const auto &book : books) {
		if (book.id <= 0 || book.osis.empty() || book.name.empty() ||
		    !validUtf8(book.osis) || !validUtf8(book.name) ||
		    !bookIds.insert(book.id).second) {
			error = "invalid or duplicate book in source Bible";
			return false;
		}
	}
	std::set<std::tuple<int, int, int>> refs;
	for (const auto &verse : verses) {
		if (!validReference(verse.reference) ||
		    !validUtf8(verse.text) ||
		    !bookIds.count(verse.reference.book) ||
		    !refs.emplace(verse.reference.book, verse.reference.chapter,
				  verse.reference.verse).second) {
			error = "invalid or duplicate verse in source Bible";
			return false;
		}
	}

	EditableEditionMetadata metadata = input;
	if (metadata.sourceFingerprint.empty())
		metadata.sourceFingerprint = fingerprint(books, verses);
	const std::string temporary = path + ".tmp";
	std::remove(temporary.c_str());
	SqliteModuleMetadata module;
	module.moduleId = metadata.projectId;
	module.name = metadata.name;
	module.language = metadata.language;
	module.versification = metadata.versification;
	module.description = metadata.name;
	module.license = metadata.sourceLicense;
	module.source = metadata.sourceAttribution + " | Source module: " +
		metadata.sourceModuleId + " | Source fingerprint: " +
		metadata.sourceFingerprint;
	module.contentVersion = "1";
	module.sourceFormat = "editable-edition";
	if (!SqliteModuleWriter().write(module, books, verses, temporary, error))
		return false;

	sqlite3 *db = nullptr;
	if (sqlite3_open(temporary.c_str(), &db) != SQLITE_OK) {
		error = db ? sqlite3_errmsg(db) : "cannot reopen editable edition";
		if (db) sqlite3_close(db);
		std::remove(temporary.c_str());
		return false;
	}
	bool ok = exec(db, "PRAGMA foreign_keys=ON; BEGIN IMMEDIATE;"
		"CREATE TABLE book_comments(book_id INTEGER PRIMARY KEY,comment TEXT NOT NULL,"
		"modified INTEGER NOT NULL,FOREIGN KEY(book_id) REFERENCES books(book_id));"
		"CREATE TABLE verse_comments(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,"
		"verse INTEGER NOT NULL,comment TEXT NOT NULL,modified INTEGER NOT NULL,"
		"PRIMARY KEY(book_id,chapter,verse),"
		"FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse));",
		error);
	if (ok) ok = exec(db, editorialSchema, error);
	if (ok) ok = ensureOriginalText(db, error);
	const std::vector<std::pair<std::string, std::string>> values = {
		{"edition_format", "biblia-elim-edition"}, {"edition_version", "1"},
		{"project_id", metadata.projectId}, {"source_module_id", metadata.sourceModuleId},
		{"source_name", metadata.sourceName}, {"source_license", metadata.sourceLicense},
		{"source_attribution", metadata.sourceAttribution},
		{"source_fingerprint", metadata.sourceFingerprint},
	};
	sqlite3_stmt *statement = nullptr;
	if (ok && sqlite3_prepare_v2(db, "INSERT INTO metadata(key,value) VALUES(?,?)",
				     -1, &statement, nullptr) == SQLITE_OK) {
		for (const auto &value : values) {
			if (!bind(statement, 1, value.first) || !bind(statement, 2, value.second) ||
			    sqlite3_step(statement) != SQLITE_DONE) { ok = false; error = sqlite3_errmsg(db); break; }
			sqlite3_reset(statement); sqlite3_clear_bindings(statement);
		}
	} else if (ok) { ok = false; error = sqlite3_errmsg(db); }
	if (statement) sqlite3_finalize(statement);
	if (ok) ok = exec(db, "COMMIT", error);
	else { std::string ignored; exec(db, "ROLLBACK", ignored); }
	sqlite3_close(db);
	if (!ok) { std::remove(temporary.c_str()); return false; }
	if (std::rename(temporary.c_str(), path.c_str()) != 0) {
		error = "cannot finalize editable edition: " + std::string(std::strerror(errno));
		std::remove(temporary.c_str());
		return false;
	}
	return true;
}

bool EditableEdition::createFromModule(const std::string &sourcePath,
				       const std::string &path,
				       const std::string &projectId,
				       const std::string &name,
				       std::string &error)
{
	if (sourcePath.empty() || path.empty() || projectId.empty() || name.empty() ||
	    !validUtf8(name) || projectId.find_first_not_of(
		    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" ) != std::string::npos) {
		error = "invalid editable edition name or ID"; return false;
	}
	if (g_file_test(path.c_str(), G_FILE_TEST_EXISTS) ||
	    g_file_test((path + ".tmp").c_str(), G_FILE_TEST_EXISTS)) {
		error = "editable edition already exists"; return false;
	}
	sqlite3 *source = nullptr;
	if (sqlite3_open_v2(sourcePath.c_str(), &source, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
		error = source ? sqlite3_errmsg(source) : "cannot open source Bible";
		if (source) sqlite3_close(source); return false;
	}
	std::string moduleId, license, sourceName, language, versification, attribution;
	bool ok = readValue(source, "module_id", moduleId) &&
		readValue(source, "license", license) && readValue(source, "name", sourceName) &&
		readValue(source, "language", language) && readValue(source, "versification", versification);
	readValue(source, "source", attribution);
	std::string editionFormat;
	const bool isProject = readValue(source, "edition_format", editionFormat);
	EditableEditionMetadata original;
	if (ok && isProject) {
		ok = readMetadata(source, original, error);
		if (ok) {
			moduleId = original.sourceModuleId;
			license = original.sourceLicense;
			sourceName = original.sourceName;
			attribution = original.sourceAttribution;
		}
	}
	if (!ok || !sourceMayBeDerived(moduleId, license)) {
		error = "source Bible is not approved for redistribution";
		sqlite3_close(source); return false;
	}
	sqlite3 *destination = nullptr;
	const std::string temporary = path + ".tmp";
	if (sqlite3_open(temporary.c_str(), &destination) != SQLITE_OK) {
		error = destination ? sqlite3_errmsg(destination) : "cannot create editable edition";
		if (destination) sqlite3_close(destination); sqlite3_close(source);
		g_unlink(temporary.c_str()); return false;
	}
	sqlite3_backup *backup = sqlite3_backup_init(destination, "main", source, "main");
	ok = backup && sqlite3_backup_step(backup, -1) == SQLITE_DONE;
	if (backup && sqlite3_backup_finish(backup) != SQLITE_OK) ok = false;
	if (!ok) error = sqlite3_errmsg(destination);
	sqlite3_close(source);
	if (ok) ok = exec(destination, "PRAGMA foreign_keys=ON; BEGIN IMMEDIATE;"
		"CREATE TABLE IF NOT EXISTS book_comments(book_id INTEGER PRIMARY KEY,comment TEXT NOT NULL,modified INTEGER NOT NULL,FOREIGN KEY(book_id) REFERENCES books(book_id));"
		"CREATE TABLE IF NOT EXISTS verse_comments(book_id INTEGER NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,comment TEXT NOT NULL,modified INTEGER NOT NULL,PRIMARY KEY(book_id,chapter,verse),FOREIGN KEY(book_id,chapter,verse) REFERENCES verses(book_id,chapter,verse));", error);
	if (ok) ok = exec(destination, editorialSchema, error);
	if (ok) ok = ensureOriginalText(destination, error);
	if (ok) ok = setValue(destination, "module_id", projectId, error) &&
		setValue(destination, "name", name, error) &&
		setValue(destination, "description", name, error) &&
		setValue(destination, "source_format", "editable-edition", error) &&
		setValue(destination, "edition_format", "biblia-elim-edition", error) &&
		setValue(destination, "edition_version", "1", error) &&
		setValue(destination, "project_id", projectId, error) &&
		setValue(destination, "source_module_id", moduleId, error) &&
		setValue(destination, "source_name", sourceName, error) &&
		setValue(destination, "source_license", license, error) &&
		setValue(destination, "source_attribution", attribution, error);
	if (ok && isProject)
		ok = setValue(destination, "source_fingerprint", original.sourceFingerprint, error);
	if (ok && !isProject) {
		std::ifstream input(sourcePath, std::ios::binary);
		GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA256);
		char buffer[8192];
		while (input.good()) {
			input.read(buffer, sizeof(buffer));
			const auto count = input.gcount();
			if (count > 0) g_checksum_update(sum, reinterpret_cast<const guchar *>(buffer), count);
		}
		ok = setValue(destination, "source_fingerprint", g_checksum_get_string(sum), error);
		g_checksum_free(sum);
	}
	if (ok) ok = exec(destination, "COMMIT", error);
	else { std::string ignored; exec(destination, "ROLLBACK", ignored); }
	sqlite3_close(destination);
	if (!ok) { g_unlink(temporary.c_str()); return false; }
	if (g_rename(temporary.c_str(), path.c_str()) != 0) {
		error = "cannot finalize editable edition"; g_unlink(temporary.c_str()); return false;
	}
	return true;
}

std::unique_ptr<EditableEdition> EditableEdition::open(const std::string &path,
							       std::string &error)
{
	sqlite3 *db = nullptr;
	if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
		error = db ? sqlite3_errmsg(db) : "cannot open editable edition";
		if (db) sqlite3_close(db);
		return nullptr;
	}
	if (!exec(db, "PRAGMA foreign_keys=ON", error) || !exec(db, editorialSchema, error)) { sqlite3_close(db); return nullptr; }
	EditableEditionMetadata metadata;
	if (!readMetadata(db, metadata, error) || !ensureOriginalText(db, error)) { sqlite3_close(db); return nullptr; }
	return std::unique_ptr<EditableEdition>(new EditableEdition(db, std::move(metadata)));
}

EditableEdition::~EditableEdition() { if (db_) sqlite3_close(db_); }

bool EditableEdition::verseText(const BibleReference &r, std::string &text,
				std::string &error) const
{
	if (!validReference(r)) { error = "invalid verse reference"; return false; }
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db_, "SELECT text FROM verses WHERE book_id=? AND chapter=? AND verse=?",
			       -1, &s, nullptr) != SQLITE_OK) { error = sqlite3_errmsg(db_); return false; }
	sqlite3_bind_int(s, 1, r.book); sqlite3_bind_int(s, 2, r.chapter); sqlite3_bind_int(s, 3, r.verse);
	const int rc = sqlite3_step(s);
	if (rc == SQLITE_ROW) {
		const auto *value = reinterpret_cast<const char *>(sqlite3_column_text(s, 0));
		text.assign(value ? value : "", value ? sqlite3_column_bytes(s, 0) : 0);
		sqlite3_finalize(s); return true;
	}
	error = rc == SQLITE_DONE ? "verse is not in this edition" : sqlite3_errmsg(db_);
	sqlite3_finalize(s); return false;
}

bool EditableEdition::setName(const std::string &name, std::string &error)
{
	if (name.empty() || !validUtf8(name)) { error = "edition name must be non-empty UTF-8"; return false; }
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db_, "UPDATE metadata SET value=? WHERE key IN ('name','description')", -1, &s, nullptr) != SQLITE_OK) {
		error = sqlite3_errmsg(db_); return false;
	}
	bind(s, 1, name);
	const bool ok = sqlite3_step(s) == SQLITE_DONE && sqlite3_changes(db_) == 2;
	if (ok) metadata_.name = name; else error = sqlite3_errmsg(db_);
	sqlite3_finalize(s); return ok;
}

bool EditableEdition::setVerseText(const BibleReference &r, const std::string &text,
				   std::string &error)
{
	if (!validReference(r) || !validUtf8(text)) { error = "verse reference or UTF-8 text is invalid"; return false; }
	if (!exec(db_, "BEGIN IMMEDIATE", error)) return false;
	sqlite3_stmt *s = nullptr;
	const char *lookup = "SELECT rowid,text FROM verses WHERE book_id=? AND chapter=? AND verse=?";
	if (sqlite3_prepare_v2(db_, lookup, -1, &s, nullptr) != SQLITE_OK) {
		error = sqlite3_errmsg(db_); exec(db_, "ROLLBACK", error); return false;
	}
	sqlite3_bind_int(s, 1, r.book); sqlite3_bind_int(s, 2, r.chapter); sqlite3_bind_int(s, 3, r.verse);
	const int found = sqlite3_step(s);
	if (found != SQLITE_ROW) {
		error = found == SQLITE_DONE ? "verse is not in this edition" : sqlite3_errmsg(db_);
		sqlite3_finalize(s); std::string ignored; exec(db_, "ROLLBACK", ignored); return false;
	}
	const sqlite3_int64 rowid = sqlite3_column_int64(s, 0);
	const auto *oldValue = reinterpret_cast<const char *>(sqlite3_column_text(s, 1));
	const int oldBytes = sqlite3_column_bytes(s, 1);
	const std::string oldText(oldValue ? oldValue : "", oldValue ? oldBytes : 0);
	sqlite3_finalize(s);
	if (oldText == text) { std::string ignored; exec(db_, "ROLLBACK", ignored); return true; }
	if (sqlite3_prepare_v2(db_, "INSERT INTO verse_revisions(book_id,chapter,verse,old_text,new_text,modified) VALUES(?,?,?,?,?,?)", -1, &s, nullptr) != SQLITE_OK) {
		error = sqlite3_errmsg(db_); std::string ignored; exec(db_, "ROLLBACK", ignored); return false;
	}
	bool audited = sqlite3_bind_int(s,1,r.book)==SQLITE_OK && sqlite3_bind_int(s,2,r.chapter)==SQLITE_OK && sqlite3_bind_int(s,3,r.verse)==SQLITE_OK && bind(s,4,oldText) && bind(s,5,text) && sqlite3_bind_int64(s,6,g_get_real_time()/G_USEC_PER_SEC)==SQLITE_OK && sqlite3_step(s)==SQLITE_DONE;
	if (!audited) error = sqlite3_errmsg(db_);
	sqlite3_finalize(s);
	if (!audited) { std::string ignored; exec(db_, "ROLLBACK", ignored); return false; }
	auto run = [&](const char *sql, const std::function<bool(sqlite3_stmt *)> &bindValues) {
		sqlite3_stmt *statement = nullptr;
		if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
			error = sqlite3_errmsg(db_); return false;
		}
		bool ok = bindValues(statement) && sqlite3_step(statement) == SQLITE_DONE;
		if (!ok && error.empty()) error = sqlite3_errmsg(db_);
		sqlite3_finalize(statement); return ok;
	};
	bool ok = true;
	const bool hasFts = tableExists(db_, "verses_fts");
	if (hasFts)
		ok = run("INSERT INTO verses_fts(verses_fts,rowid,text) VALUES('delete',?,?)",
			[&](sqlite3_stmt *q) { return sqlite3_bind_int64(q, 1, rowid) == SQLITE_OK && bind(q, 2, oldText); });
	ok = ok && run("UPDATE verses SET text=? WHERE rowid=?",
		[&](sqlite3_stmt *q) { return bind(q, 1, text) && sqlite3_bind_int64(q, 2, rowid) == SQLITE_OK; });
	struct CleanupTable { const char *name; const char *sql; };
	const CleanupTable removeRows[] = {
		{"verse_word_morphology", "DELETE FROM verse_word_morphology WHERE book_id=? AND chapter=? AND verse=?"},
		{"verse_word_strongs", "DELETE FROM verse_word_strongs WHERE book_id=? AND chapter=? AND verse=?"},
		{"verse_words", "DELETE FROM verse_words WHERE book_id=? AND chapter=? AND verse=?"},
		{"verse_spans", "DELETE FROM verse_spans WHERE book_id=? AND chapter=? AND verse=?"},
		{"footnotes", "DELETE FROM footnotes WHERE book_id=? AND chapter=? AND verse=?"},
		{"cross_reference_range_targets", "DELETE FROM cross_reference_range_targets WHERE book_id=? AND chapter=? AND verse=?"},
		{"cross_reference_ranges", "DELETE FROM cross_reference_ranges WHERE book_id=? AND chapter=? AND verse=?"},
		{"cross_reference_targets", "DELETE FROM cross_reference_targets WHERE book_id=? AND chapter=? AND verse=?"},
		{"cross_references", "DELETE FROM cross_references WHERE book_id=? AND chapter=? AND verse=?"},
	};
	for (const auto &table : removeRows) {
		if (!tableExists(db_, table.name)) continue;
		ok = ok && run(table.sql, [&](sqlite3_stmt *q) {
			return sqlite3_bind_int(q, 1, r.book) == SQLITE_OK &&
			       sqlite3_bind_int(q, 2, r.chapter) == SQLITE_OK &&
			       sqlite3_bind_int(q, 3, r.verse) == SQLITE_OK;
		});
	}
	if (hasFts)
		ok = ok && run("INSERT INTO verses_fts(rowid,text) VALUES(?,?)",
			[&](sqlite3_stmt *q) { return sqlite3_bind_int64(q, 1, rowid) == SQLITE_OK && bind(q, 2, text); });
	struct FeatureTable { const char *table; const char *key; const char *sql; };
	const FeatureTable featureUpdates[] = {
		{"verse_word_strongs", "feature.strong", "UPDATE metadata SET value=CASE WHEN EXISTS(SELECT 1 FROM verse_word_strongs) THEN 'true' ELSE 'false' END WHERE key='feature.strong'"},
		{"verse_word_morphology", "feature.morphology", "UPDATE metadata SET value=CASE WHEN EXISTS(SELECT 1 FROM verse_word_morphology) THEN 'true' ELSE 'false' END WHERE key='feature.morphology'"},
		{"footnotes", "feature.footnotes", "UPDATE metadata SET value=CASE WHEN EXISTS(SELECT 1 FROM footnotes) THEN 'true' ELSE 'false' END WHERE key='feature.footnotes'"},
		{"cross_references", "feature.crossrefs", "UPDATE metadata SET value=CASE WHEN EXISTS(SELECT 1 FROM cross_references) THEN 'true' ELSE 'false' END WHERE key='feature.crossrefs'"},
	};
	for (const auto &feature : featureUpdates) {
		if (tableExists(db_, feature.table))
			ok = ok && exec(db_, feature.sql, error);
		else
			ok = ok && setValue(db_, feature.key, "false", error);
	}
	if (ok) ok = exec(db_, "COMMIT", error);
	else { std::string ignored; exec(db_, "ROLLBACK", ignored); }
	return ok;
}

bool EditableEdition::setComment(int bookId, int chapter, int verse,
				 const std::string &comment,
				 std::string &error)
{
	if (!validUtf8(comment)) { error = "comment must be valid UTF-8"; return false; }
	const char *sql = verse == 0
		? "INSERT INTO book_comments VALUES(?,?,?) ON CONFLICT(book_id) DO UPDATE SET comment=excluded.comment,modified=excluded.modified"
		: "INSERT INTO verse_comments VALUES(?,?,?,?,?) ON CONFLICT(book_id,chapter,verse) DO UPDATE SET comment=excluded.comment,modified=excluded.modified";
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db_, sql, -1, &s, nullptr) != SQLITE_OK) { error = sqlite3_errmsg(db_); return false; }
	int i = 1;
	sqlite3_bind_int(s, i++, bookId);
	if (verse != 0) { sqlite3_bind_int(s, i++, chapter); sqlite3_bind_int(s, i++, verse); }
	bind(s, i++, comment);
	sqlite3_bind_int64(s, i, g_get_real_time() / G_USEC_PER_SEC);
	const bool ok = sqlite3_step(s) == SQLITE_DONE;
	if (!ok) error = sqlite3_errmsg(db_);
	sqlite3_finalize(s); return ok;
}

bool EditableEdition::getComment(const char *sql, int bookId, int chapter,
				int verse, std::string &comment,
				std::string &error) const
{
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db_, sql, -1, &s, nullptr) != SQLITE_OK) { error = sqlite3_errmsg(db_); return false; }
	sqlite3_bind_int(s, 1, bookId);
	if (verse != 0) { sqlite3_bind_int(s, 2, chapter); sqlite3_bind_int(s, 3, verse); }
	const int rc = sqlite3_step(s);
	if (rc == SQLITE_ROW) {
		const auto *value = reinterpret_cast<const char *>(sqlite3_column_text(s, 0));
		comment.assign(value ? value : "", value ? sqlite3_column_bytes(s, 0) : 0);
		sqlite3_finalize(s); return true;
	}
	if (rc == SQLITE_DONE) { comment.clear(); sqlite3_finalize(s); return true; }
	error = sqlite3_errmsg(db_); sqlite3_finalize(s); return false;
}

bool EditableEdition::setBookComment(int bookId, const std::string &comment,
				     std::string &error)
{
	return bookId > 0 ? setComment(bookId, 0, 0, comment, error)
			  : (error = "invalid book reference", false);
}
bool EditableEdition::bookComment(int bookId, std::string &comment,
				  std::string &error) const
{
	return bookId > 0 ? getComment("SELECT comment FROM book_comments WHERE book_id=?", bookId, 0, 0, comment, error)
			  : (error = "invalid book reference", false);
}
bool EditableEdition::setVerseComment(const BibleReference &r,
				      const std::string &comment,
				      std::string &error)
{
	return validReference(r) ? setComment(r.book, r.chapter, r.verse, comment, error)
				 : (error = "invalid verse reference", false);
}
bool EditableEdition::verseComment(const BibleReference &r,
				   std::string &comment,
				   std::string &error) const
{
	return validReference(r) ? getComment("SELECT comment FROM verse_comments WHERE book_id=? AND chapter=? AND verse=?", r.book, r.chapter, r.verse, comment, error)
				 : (error = "invalid verse reference", false);
}

bool EditableEdition::setGlossaryEntry(const EditionGlossaryEntry &e, std::string &error)
{
	if (e.sourceTerm.empty() || e.targetTerm.empty() || !validUtf8(e.sourceTerm) || !validUtf8(e.targetTerm) || !validUtf8(e.note)) { error = "glossary entry requires valid non-empty terms"; return false; }
	sqlite3_stmt *s = nullptr;
	if (sqlite3_prepare_v2(db_, "INSERT INTO edition_glossary(source_term,target_term,note) VALUES(?,?,?) ON CONFLICT(source_term) DO UPDATE SET target_term=excluded.target_term,note=excluded.note", -1, &s, nullptr) != SQLITE_OK) { error = sqlite3_errmsg(db_); return false; }
	const bool ok = bind(s,1,e.sourceTerm) && bind(s,2,e.targetTerm) && bind(s,3,e.note) && sqlite3_step(s)==SQLITE_DONE;
	if (!ok) error=sqlite3_errmsg(db_); sqlite3_finalize(s); return ok;
}

bool EditableEdition::glossaryEntries(std::vector<EditionGlossaryEntry>& out, std::string& error) const
{
	out.clear(); sqlite3_stmt*s=nullptr;
	if(sqlite3_prepare_v2(db_,"SELECT source_term,target_term,note FROM edition_glossary ORDER BY source_term",-1,&s,nullptr)!=SQLITE_OK){error=sqlite3_errmsg(db_);return false;}
	while(sqlite3_step(s)==SQLITE_ROW){auto v=[s](int i){const auto*p=reinterpret_cast<const char*>(sqlite3_column_text(s,i));return std::string(p?p:"",p?sqlite3_column_bytes(s,i):0);};out.push_back({v(0),v(1),v(2)});}
	sqlite3_finalize(s);return true;
}

bool EditableEdition::setDecision(const EditionDecision&d,std::string&error)
{
	if(!validReference(d.reference)||!validUtf8(d.proposal)||!validUtf8(d.rationale)||!validUtf8(d.evidence)||!validUtf8(d.confidence)||!validUtf8(d.status)){error="editorial decision has invalid reference or encoding";return false;}
	sqlite3_stmt*s=nullptr;const char*sql="INSERT INTO editorial_decisions VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(book_id,chapter,verse) DO UPDATE SET proposal=excluded.proposal,rationale=excluded.rationale,evidence=excluded.evidence,confidence=excluded.confidence,status=excluded.status,modified=excluded.modified";
	if(sqlite3_prepare_v2(db_,sql,-1,&s,nullptr)!=SQLITE_OK){error=sqlite3_errmsg(db_);return false;}
	bool ok=sqlite3_bind_int(s,1,d.reference.book)==SQLITE_OK&&sqlite3_bind_int(s,2,d.reference.chapter)==SQLITE_OK&&sqlite3_bind_int(s,3,d.reference.verse)==SQLITE_OK&&bind(s,4,d.proposal)&&bind(s,5,d.rationale)&&bind(s,6,d.evidence)&&bind(s,7,d.confidence)&&bind(s,8,d.status)&&sqlite3_bind_int64(s,9,g_get_real_time()/G_USEC_PER_SEC)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE;
	if(!ok)error=sqlite3_errmsg(db_);sqlite3_finalize(s);return ok;
}

bool EditableEdition::decision(const BibleReference&r,EditionDecision&d,std::string&error)const
{
	if(!validReference(r)){error="invalid verse reference";return false;}sqlite3_stmt*s=nullptr;
	if(sqlite3_prepare_v2(db_,"SELECT proposal,rationale,evidence,confidence,status FROM editorial_decisions WHERE book_id=? AND chapter=? AND verse=?",-1,&s,nullptr)!=SQLITE_OK){error=sqlite3_errmsg(db_);return false;}
	sqlite3_bind_int(s,1,r.book);sqlite3_bind_int(s,2,r.chapter);sqlite3_bind_int(s,3,r.verse);int rc=sqlite3_step(s);
	if(rc==SQLITE_ROW){auto v=[s](int i){const auto*p=reinterpret_cast<const char*>(sqlite3_column_text(s,i));return std::string(p?p:"",p?sqlite3_column_bytes(s,i):0);};d.reference=r;d.proposal=v(0);d.rationale=v(1);d.evidence=v(2);d.confidence=v(3);d.status=v(4);}
	sqlite3_finalize(s);if(rc!=SQLITE_ROW&&rc!=SQLITE_DONE){error=sqlite3_errmsg(db_);return false;}return true;
}

bool EditableEdition::revisions(const BibleReference&r,std::vector<EditionRevision>&out,std::string&error)const
{
	if(!validReference(r)){error="invalid verse reference";return false;}out.clear();sqlite3_stmt*s=nullptr;
	if(sqlite3_prepare_v2(db_,"SELECT old_text,new_text,modified FROM verse_revisions WHERE book_id=? AND chapter=? AND verse=? ORDER BY revision_id DESC",-1,&s,nullptr)!=SQLITE_OK){error=sqlite3_errmsg(db_);return false;}
	sqlite3_bind_int(s,1,r.book);sqlite3_bind_int(s,2,r.chapter);sqlite3_bind_int(s,3,r.verse);
	while(sqlite3_step(s)==SQLITE_ROW){auto v=[s](int i){const auto*p=reinterpret_cast<const char*>(sqlite3_column_text(s,i));return std::string(p?p:"",p?sqlite3_column_bytes(s,i):0);};out.push_back({r.book,r.chapter,r.verse,v(0),v(1),sqlite3_column_int64(s,2)});}
	sqlite3_finalize(s);return true;
}


bool EditableEdition::comparisons(const std::string &path, EditableEditionMetadata &metadata,
 std::vector<EditionComparison> &verses, std::string &error, int book, int chapter)
{
 verses.clear();
 sqlite3 *raw = nullptr;
 if (sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
  error = raw ? sqlite3_errmsg(raw) : "cannot open edition";
  if (raw) sqlite3_close(raw);
  return false;
 }
 std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
 if (!readMetadata(raw, metadata, error)) return false;
 const bool comments = tableExists(raw, "verse_comments");
 const bool decisions = tableExists(raw, "editorial_decisions");
 std::string sql = "SELECT v.book_id,v.chapter,v.verse,b.name," + originalTextExpression(raw) + ",v.text,";
 sql += comments ? "COALESCE(c.comment,'')" : "''";
 sql += decisions ? ",COALESCE(d.proposal,''),COALESCE(d.rationale,''),COALESCE(d.evidence,''),COALESCE(d.status,'')" : ", '','','',''";
 sql += " FROM verses v JOIN books b ON b.book_id=v.book_id";
 if (comments) sql += " LEFT JOIN verse_comments c USING(book_id,chapter,verse)";
 if (decisions) sql += " LEFT JOIN editorial_decisions d USING(book_id,chapter,verse)";
 sql += " WHERE (?=0 OR v.book_id=?) AND (?=0 OR v.chapter=?) ORDER BY b.position,v.chapter,v.verse";
 sqlite3_stmt *stmt = nullptr;
 if (sqlite3_prepare_v2(raw, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
  error = sqlite3_errmsg(raw); return false;
 }
 std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> guard(stmt, sqlite3_finalize);
 sqlite3_bind_int(stmt,1,book);sqlite3_bind_int(stmt,2,book);
 sqlite3_bind_int(stmt,3,chapter);sqlite3_bind_int(stmt,4,chapter);
 auto text=[stmt](int i){const auto *v=reinterpret_cast<const char*>(sqlite3_column_text(stmt,i));return std::string(v?v:"");};
 int rc;
 while ((rc=sqlite3_step(stmt))==SQLITE_ROW) {
  if(sqlite3_column_type(stmt,4)==SQLITE_NULL){verses.clear();error="original verse snapshot is incomplete";return false;}
  EditionComparison item;
  item.reference={0,sqlite3_column_int(stmt,0),sqlite3_column_int(stmt,1),sqlite3_column_int(stmt,2)};
  item.bookName=text(3);item.baseText=text(4);item.currentText=text(5);
  item.comment=text(6);item.proposal=text(7);item.rationale=text(8);item.evidence=text(9);item.status=text(10);
  verses.push_back(std::move(item));
 }
 if (rc != SQLITE_DONE) { verses.clear();error=sqlite3_errmsg(raw);return false; }
 return true;
}
