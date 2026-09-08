#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#include <string>
#include <sys/stat.h>

#include "backend/sqlite/sqlite_bible_backend.h"
#include "bible_backend_contract.h"

namespace {
std::string fixtureDirectory;

bool createDatabase(const std::string &path, const char *sql)
{
	sqlite3 *db = nullptr;
	if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
		if (db) sqlite3_close(db);
		return false;
	}
	char *message = nullptr;
	const bool ok = sqlite3_exec(db, sql, nullptr, nullptr, &message) == SQLITE_OK;
	sqlite3_free(message);
	sqlite3_close(db);
	return ok;
}

std::string validationSchema(const char *metadataRows, const char *rows = "")
{
	return std::string("PRAGMA user_version=1; PRAGMA foreign_keys=OFF;\n") +
		"CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);\n" +
		"CREATE TABLE books(book_id INTEGER PRIMARY KEY,osis TEXT UNIQUE,name TEXT,short_name TEXT,testament INTEGER,position INTEGER);\n" +
		"CREATE TABLE verses(book_id INTEGER,chapter INTEGER,verse INTEGER,text TEXT,PRIMARY KEY(book_id,chapter,verse),FOREIGN KEY(book_id) REFERENCES books(book_id));\n" +		"INSERT INTO metadata(key,value) VALUES " + metadataRows + ";\n" +
		"INSERT INTO books VALUES(40,'John','John','Jn',2,40);\n" + rows;
}

bool rejectedModule(const std::string &sql)
{
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-reject-XXXXXX", nullptr);
	if (!directory) return false;
	const std::string path = std::string(directory) + "/module.sqlite";
	const bool created = createDatabase(path, sql.c_str());
	SqliteBibleBackend backend(directory);
	const bool rejected = created && backend.listModules().empty();
	g_remove(path.c_str());
	g_rmdir(directory);
	g_free(directory);
	return rejected;
}

int strongCapability(const std::string &sql)
{
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-strong-cap-XXXXXX", nullptr);
	if (!directory) return -1;
	const std::string path = std::string(directory) + "/module.sqlite";
	const bool created = createDatabase(path, sql.c_str());
	SqliteBibleBackend backend(directory);
	const int capability = created && backend.hasModule("x")
		? (backend.moduleCapabilities("x").strongs ? 1 : 0) : -1;
	g_remove(path.c_str());
	g_rmdir(directory);
	g_free(directory);
	return capability;
}

void contractTest()
{
	SqliteBibleBackend backend(fixtureDirectory);
	BibleBackendContractFixture fixture;
	fixture.chapterSize = 3;
	fixture.enrichedWords = false;
	fixture.dictionaryModule.clear();
	fixture.bookId = 40;
	runBibleBackendContractTests(backend, fixture);
}

void metadataAndCapabilities()
{
	SqliteBibleBackend backend(fixtureDirectory);
	auto modules = backend.listModules();
	g_assert_cmpuint(modules.size(), ==, 1);
	g_assert_cmpstr(modules[0].id.c_str(), ==, "FakeBible");
	g_assert_cmpint(static_cast<int>(modules[0].type), ==,
		static_cast<int>(BibleModuleType::Bible));
	auto capabilities = backend.moduleCapabilities("FakeBible");
	g_assert_true(capabilities.verses);
	g_assert_true(capabilities.search);
	g_assert_false(capabilities.strongs);
	g_assert_false(capabilities.morphology);
	g_assert_false(capabilities.dictionaryLookup);
}

void navigationBoundaries()
{
	SqliteBibleBackend backend(fixtureDirectory);
	g_assert_cmpstr(backend.navigate("FakeBible", "Genesis 1:1", -1).c_str(),
		==, "Genesis 1:1");
	g_assert_cmpstr(backend.navigate("FakeBible", "Genesis 1:1", 1).c_str(),
		==, "John 3:16");
	g_assert_cmpstr(backend.navigate("FakeBible", "John 3:18", 1).c_str(),
		==, "John 4:1");
	g_assert_cmpstr(backend.navigate("FakeBible", "John 4:1", 1).c_str(),
		==, "John 4:1");
	g_assert_cmpstr(backend.setChapter("FakeBible", "John 3:16", 4).c_str(),
		==, "John 4:1");
	g_assert_true(backend.setChapter("FakeBible", "John 3:16", 99).empty());
	g_assert_true(backend.setVerse("FakeBible", "John 3:16", 99).empty());
}

void searchModes()
{
	SqliteBibleBackend backend(fixtureDirectory);
	BibleSearchQuery query;
	query.text = "God world";
	query.mode = BibleSearchMode::MultiWord;
	g_assert_cmpuint(backend.search("FakeBible", query).size(), ==, 2);
	query.text = "God so loved";
	query.mode = BibleSearchMode::Phrase;
	g_assert_cmpuint(backend.search("FakeBible", query).size(), ==, 1);
	query.mode = BibleSearchMode::Regex;
	g_assert_true(backend.search("FakeBible", query).empty());
	query.mode = BibleSearchMode::Attribute;
	g_assert_true(backend.search("FakeBible", query).empty());
	query.mode = BibleSearchMode::MultiWord;
	query.text = "God";
	query.limit = 1;
	query.offset = 1;
	g_assert_cmpuint(backend.search("FakeBible", query).size(), ==, 1);
}

void missingDatabase()
{
	SqliteBibleBackend backend(fixtureDirectory + "/does-not-exist");
	g_assert_true(backend.listModules().empty());
	g_assert_false(backend.hasModule("FakeBible"));
}

void invalidSchema()
{
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-invalid-XXXXXX", nullptr);
	g_assert_nonnull(directory);
	std::string path = std::string(directory) + "/invalid.sqlite";
	g_assert_true(createDatabase(path, "CREATE TABLE unrelated(value TEXT);"));
	SqliteBibleBackend backend(directory);
	g_assert_true(backend.listModules().empty());
	g_remove(path.c_str());
	g_rmdir(directory);
	g_free(directory);
}

void corruptDatabase()
{
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-corrupt-XXXXXX", nullptr);
	g_assert_nonnull(directory);
	std::string path = std::string(directory) + "/corrupt.sqlite";
	g_assert_true(g_file_set_contents(path.c_str(), "not sqlite", -1, nullptr));
	SqliteBibleBackend backend(directory);
	g_assert_true(backend.listModules().empty());
	g_remove(path.c_str());
	g_rmdir(directory);
	g_free(directory);
}

void schemaValidation()
{
	const char *valid = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','true'),('feature.strong','false'),('feature.morphology','false'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	g_assert_true(rejectedModule(validationSchema("('module_id','x')")));
	g_assert_true(rejectedModule(validationSchema("('schema_version','2')")));
	g_assert_true(rejectedModule(validationSchema("('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('feature.verses','true'),('feature.search','true'),('feature.strong','false'),('feature.morphology','false'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')")));
	g_assert_true(!rejectedModule(validationSchema(valid)));
	const char *legacy = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','true'),('feature.morphology','false'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	g_assert_cmpint(strongCapability(validationSchema(legacy)), ==, 0);
	const char *declaredWithoutData = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','true'),('feature.strong','true'),('feature.morphology','false'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	g_assert_cmpint(strongCapability(validationSchema(declaredWithoutData)), ==, 0);
}

void invalidReferencesAndCapabilities()
{
	const char *valid = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','true'),('feature.strong','false'),('feature.morphology','false'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	g_assert_true(rejectedModule(validationSchema(valid, "INSERT INTO verses VALUES(99,1,1,'bad');")));
	g_assert_true(rejectedModule(validationSchema(valid, "INSERT INTO verses VALUES(40,0,1,'bad');")));
	g_assert_true(rejectedModule(validationSchema(valid, "INSERT INTO verses VALUES(40,1,0,'bad');")));
	const char *noSearch = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','false'),('feature.strong','false'),('feature.morphology','false'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	g_assert_true(rejectedModule(validationSchema(noSearch)));
}

void opensReadOnly()
{
	struct stat before {}, after {};
	const std::string path = fixtureDirectory + "/test-bible.sqlite";
	g_assert_cmpint(stat(path.c_str(), &before), ==, 0);
	{ SqliteBibleBackend backend(fixtureDirectory); g_assert_true(backend.hasModule("FakeBible")); }
	g_assert_cmpint(stat(path.c_str(), &after), ==, 0);
	g_assert_cmpint(before.st_size, ==, after.st_size);
}
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, nullptr);
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-fixture-XXXXXX", nullptr);
	g_assert_nonnull(directory);
	fixtureDirectory = directory;
	gchar *sql = nullptr;
	gsize length = 0;
	const std::string sqlPath = std::string(SRCDIR) +
		"/tests/fixtures/sqlite/test-bible.sql";
	g_assert_true(g_file_get_contents(sqlPath.c_str(), &sql, &length, nullptr));
	const std::string database = fixtureDirectory + "/test-bible.sqlite";
	g_assert_true(createDatabase(database, sql));
	g_free(sql);

	g_test_add_func("/backend/sqlite/contract", contractTest);
	g_test_add_func("/backend/sqlite/metadata", metadataAndCapabilities);
	g_test_add_func("/backend/sqlite/navigation-boundaries", navigationBoundaries);
	g_test_add_func("/backend/sqlite/search-modes", searchModes);
	g_test_add_func("/backend/sqlite/missing-database", missingDatabase);
	g_test_add_func("/backend/sqlite/invalid-schema", invalidSchema);
	g_test_add_func("/backend/sqlite/corrupt-database", corruptDatabase);
	g_test_add_func("/backend/sqlite/schema-validation", schemaValidation);
	g_test_add_func("/backend/sqlite/invalid-references", invalidReferencesAndCapabilities);
	g_test_add_func("/backend/sqlite/read-only", opensReadOnly);
	const int result = g_test_run();
	g_remove(database.c_str());
	g_rmdir(fixtureDirectory.c_str());
	g_free(directory);
	return result;
}
