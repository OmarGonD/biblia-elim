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
	g_remove((std::string(directory) + "/.validation-cache").c_str());
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
	g_remove((std::string(directory) + "/.validation-cache").c_str());
	g_rmdir(directory);
	g_free(directory);
	return capability;
}

int morphologyCapability(const std::string &sql)
{
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-morph-cap-XXXXXX", nullptr);
	if (!directory) return -1;
	const std::string path = std::string(directory) + "/module.sqlite";
	const bool created = createDatabase(path, sql.c_str());
	SqliteBibleBackend backend(directory);
	const int capability = created && backend.hasModule("x")
		? (backend.moduleCapabilities("x").morphology ? 1 : 0) : -1;
	g_remove(path.c_str());
	g_remove((std::string(directory) + "/.validation-cache").c_str());
	g_rmdir(directory);
	g_free(directory);
	return capability;
}

bool sameReference(const BibleReference &left, const BibleReference &right)
{
	return left.testament == right.testament && left.book == right.book &&
		left.chapter == right.chapter && left.verse == right.verse;
}

void assertSameContent(const BibleVerseContent &expected,
	const BibleVerseContent &actual)
{
	g_assert_true(sameReference(expected.reference, actual.reference));
	g_assert_cmpstr(expected.plainText.c_str(), ==, actual.plainText.c_str());
	g_assert_cmpstr(expected.renderedText.c_str(), ==, actual.renderedText.c_str());
	g_assert_cmpint(expected.paragraphBreak, ==, actual.paragraphBreak);
	g_assert_cmpint(expected.valid, ==, actual.valid);
	g_assert_cmpuint(expected.headings.size(), ==, actual.headings.size());
	for (std::size_t i = 0; i < expected.headings.size(); ++i)
		g_assert_cmpstr(expected.headings[i].text.c_str(), ==,
			actual.headings[i].text.c_str());
	g_assert_cmpuint(expected.spans.size(), ==, actual.spans.size());
	for (std::size_t i = 0; i < expected.spans.size(); ++i) {
		g_assert_cmpuint(expected.spans[i].start, ==, actual.spans[i].start);
		g_assert_cmpuint(expected.spans[i].length, ==, actual.spans[i].length);
		g_assert_cmpint(static_cast<int>(expected.spans[i].style), ==,
			static_cast<int>(actual.spans[i].style));
	}
	g_assert_cmpuint(expected.words.size(), ==, actual.words.size());
	for (std::size_t i = 0; i < expected.words.size(); ++i) {
		const BibleWordInfo &a = expected.words[i], &b = actual.words[i];
		g_assert_cmpuint(a.start, ==, b.start);
		g_assert_cmpuint(a.length, ==, b.length);
		g_assert_cmpstr(a.text.c_str(), ==, b.text.c_str());
		g_assert_cmpstr(a.strong.c_str(), ==, b.strong.c_str());
		g_assert_cmpuint(a.morphologyTags.size(), ==, b.morphologyTags.size());
		for (std::size_t j = 0; j < a.morphologyTags.size(); ++j) {
			g_assert_cmpstr(a.morphologyTags[j].scheme.c_str(), ==,
				b.morphologyTags[j].scheme.c_str());
			g_assert_cmpstr(a.morphologyTags[j].code.c_str(), ==,
				b.morphologyTags[j].code.c_str());
		}
	}
	g_assert_cmpuint(expected.footnotes.size(), ==, actual.footnotes.size());
	for (std::size_t i = 0; i < expected.footnotes.size(); ++i) {
		g_assert_cmpuint(expected.footnotes[i].offset, ==,
			actual.footnotes[i].offset);
		g_assert_cmpstr(expected.footnotes[i].id.c_str(), ==,
			actual.footnotes[i].id.c_str());
		g_assert_cmpstr(expected.footnotes[i].body.c_str(), ==,
			actual.footnotes[i].body.c_str());
	}
	g_assert_cmpuint(expected.crossReferences.size(), ==,
		actual.crossReferences.size());
	for (std::size_t i = 0; i < expected.crossReferences.size(); ++i) {
		const BibleCrossReference &a = expected.crossReferences[i];
		const BibleCrossReference &b = actual.crossReferences[i];
		g_assert_cmpuint(a.offset, ==, b.offset);
		g_assert_cmpstr(a.displayText.c_str(), ==, b.displayText.c_str());
		g_assert_cmpuint(a.references.size(), ==, b.references.size());
		for (std::size_t j = 0; j < a.references.size(); ++j)
			g_assert_true(sameReference(a.references[j], b.references[j]));
		g_assert_cmpuint(a.ranges.size(), ==, b.ranges.size());
		for (std::size_t j = 0; j < a.ranges.size(); ++j) {
			g_assert_true(sameReference(a.ranges[j].start, b.ranges[j].start));
			g_assert_true(sameReference(a.ranges[j].end, b.ranges[j].end));
			g_assert_cmpuint(a.ranges[j].expandedReferences.size(), ==,
				b.ranges[j].expandedReferences.size());
			for (std::size_t k = 0;
			     k < a.ranges[j].expandedReferences.size(); ++k)
				g_assert_true(sameReference(
					a.ranges[j].expandedReferences[k],
					b.ranges[j].expandedReferences[k]));
		}
	}
}

void chapterContentMatchesVerseReads()
{
	SqliteBibleBackend backend(fixtureDirectory);
	const BibleReference chapter{2, 40, 3, 1};
	const std::vector<BibleVerseContent> contents =
		backend.getChapterContent("FakeBible", chapter, 18);
	g_assert_cmpuint(contents.size(), ==, 18);
	for (int verse = 1; verse <= 18; ++verse) {
		BibleReference reference = chapter;
		reference.verse = verse;
		assertSameContent(backend.getVerseContent("FakeBible", reference),
			contents[static_cast<std::size_t>(verse - 1)]);
	}
}

void contractTest()
{
	SqliteBibleBackend backend(fixtureDirectory);
	BibleBackendContractFixture fixture;
	fixture.chapterSize = 3;
	fixture.dictionaryModule.clear();
	fixture.bookId = 40;
	fixture.legacyWordFields = false;
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
	g_assert_true(capabilities.strongs);
	g_assert_true(capabilities.morphology);
	g_assert_false(capabilities.dictionaryLookup);
}

void navigationBoundaries()
{
	SqliteBibleBackend backend(fixtureDirectory);
	/* A bare chapter, as reading plans give it, means its first verse. */
	BibleKeyInfo chapterOnly;
	g_assert_true(backend.resolveKey("FakeBible", "Genesis 1", chapterOnly));
	g_assert_cmpstr(chapterOnly.key.c_str(), ==, "Genesis 1:1");
	g_assert_false(backend.resolveKey("FakeBible", "Genesis", chapterOnly));
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
	g_remove((std::string(directory) + "/.validation-cache").c_str());
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
	g_remove((std::string(directory) + "/.validation-cache").c_str());
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

void morphologyCapabilitiesRequireReadableValidData()
{
	const char *declared = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','true'),('feature.strong','false'),('feature.morphology','true'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	g_assert_cmpint(morphologyCapability(validationSchema(declared)), ==, 0);
	const std::string tables =
		"CREATE TABLE verse_words(book_id INTEGER,chapter INTEGER,verse INTEGER,"
		"sequence INTEGER,start INTEGER,length INTEGER,text TEXT,strong TEXT,"
		"PRIMARY KEY(book_id,chapter,verse,sequence));"
		"CREATE TABLE verse_word_morphology(book_id INTEGER,chapter INTEGER,"
		"verse INTEGER,word_sequence INTEGER,morphology_sequence INTEGER,"
		"scheme TEXT NOT NULL,code TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,"
		"word_sequence,morphology_sequence));";
	g_assert_cmpint(morphologyCapability(validationSchema(declared, tables.c_str())), ==, 0);
	const std::string word = tables +
		"INSERT INTO verses VALUES(40,1,1,'word');"
		"INSERT INTO verse_words VALUES(40,1,1,0,0,4,'word',NULL);";
	const std::string malformed = word +
		"INSERT INTO verse_word_morphology VALUES(40,1,1,0,0,'bad scheme','code');";
	g_assert_cmpint(morphologyCapability(validationSchema(declared, malformed.c_str())), ==, 0);
	const std::string orphan = word +
		"INSERT INTO verse_word_morphology VALUES(40,1,1,9,0,'unknown','code');";
	g_assert_cmpint(morphologyCapability(validationSchema(declared, orphan.c_str())), ==, 0);
	const std::string valid = word +
		"INSERT INTO verse_word_morphology VALUES(40,1,1,0,0,'unknown','opaque/code');";
	g_assert_cmpint(morphologyCapability(validationSchema(declared, valid.c_str())), ==, 1);
	const char *notDeclared = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','true'),('feature.strong','false'),('feature.morphology','false'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	g_assert_cmpint(morphologyCapability(validationSchema(notDeclared, valid.c_str())), ==, 0);
}

void legacyMorphologyLookupWithoutIndex()
{
	const char *declared = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','true'),('feature.strong','false'),('feature.morphology','true'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	const std::string rows =
		"CREATE TABLE verse_words(book_id INTEGER,chapter INTEGER,verse INTEGER,"
		"sequence INTEGER,start INTEGER,length INTEGER,text TEXT,strong TEXT,"
		"PRIMARY KEY(book_id,chapter,verse,sequence));"
		"CREATE TABLE verse_word_morphology(book_id INTEGER,chapter INTEGER,"
		"verse INTEGER,word_sequence INTEGER,morphology_sequence INTEGER,"
		"scheme TEXT NOT NULL,code TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,"
		"word_sequence,morphology_sequence));"
		"INSERT INTO verses VALUES(40,1,1,'legacy');"
		"INSERT INTO verse_words VALUES(40,1,1,0,0,6,'legacy',NULL);"
		"INSERT INTO verse_word_morphology VALUES(40,1,1,0,0,'old','code');";
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-old-morph-XXXXXX", nullptr);
	g_assert_nonnull(directory);
	const std::string path = std::string(directory) + "/module.sqlite";
	g_assert_true(createDatabase(path, validationSchema(declared, rows.c_str()).c_str()));
	SqliteBibleBackend backend(directory);
	const MorphologyOccurrencePage page = backend.findMorphologyOccurrencePage(
		"x", { "old", "code" }, 10, 0);
	g_assert_cmpuint(page.occurrences.size(), ==, 1);
	g_assert_false(page.hasMore);
	g_assert_cmpstr(page.occurrences[0].word.c_str(), ==, "legacy");
	g_assert_cmpstr(page.occurrences[0].key.c_str(), ==, "John 1:1");
	g_remove(path.c_str());
	g_remove((std::string(directory) + "/.validation-cache").c_str());
	g_rmdir(directory);
	g_free(directory);
}

/* The whole-module checks are remembered per file (.validation-cache), and a
 * remembered verdict never outlives its file: the same module stays valid
 * from the cache, a file replaced with invalid data is checked again. */
void validationVerdictsFollowTheFile()
{
	const char *declared = "('schema_version','1'),('module_id','x'),('name','X'),('language','en'),('module_type','bible'),('versification','custom'),('feature.verses','true'),('feature.search','true'),('feature.strong','false'),('feature.morphology','true'),('feature.headings','false'),('feature.footnotes','false'),('feature.crossrefs','false'),('feature.dictionary','false')";
	const std::string word =
		"CREATE TABLE verse_words(book_id INTEGER,chapter INTEGER,verse INTEGER,"
		"sequence INTEGER,start INTEGER,length INTEGER,text TEXT,strong TEXT,"
		"PRIMARY KEY(book_id,chapter,verse,sequence));"
		"CREATE TABLE verse_word_morphology(book_id INTEGER,chapter INTEGER,"
		"verse INTEGER,word_sequence INTEGER,morphology_sequence INTEGER,"
		"scheme TEXT NOT NULL,code TEXT NOT NULL,PRIMARY KEY(book_id,chapter,verse,"
		"word_sequence,morphology_sequence));"
		"INSERT INTO verses VALUES(40,1,1,'word');"
		"INSERT INTO verse_words VALUES(40,1,1,0,0,4,'word',NULL);";
	const std::string valid = validationSchema(declared, (word +
		"INSERT INTO verse_word_morphology VALUES(40,1,1,0,0,'robinson','N-NSM');").c_str());
	const std::string malformed = validationSchema(declared, (word +
		"INSERT INTO verse_word_morphology VALUES(40,1,1,0,0,'bad scheme','N-NSM');").c_str());
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-verdicts-XXXXXX", nullptr);
	g_assert_nonnull(directory);
	const std::string path = std::string(directory) + "/module.sqlite";
	const std::string cache = std::string(directory) + "/.validation-cache";
	const std::string staged = std::string(directory) + "/staged.tmp";
	g_assert_true(createDatabase(path, valid.c_str()));
	{
		SqliteBibleBackend backend(directory);
		g_assert_true(backend.moduleCapabilities("x").morphology);
	}
	g_assert_true(g_file_test(cache.c_str(), G_FILE_TEST_EXISTS));
	{
		SqliteBibleBackend backend(directory);
		g_assert_true(backend.moduleCapabilities("x").morphology);
		g_assert_cmpuint(backend.listModules().size(), ==, 1);
	}
	/* Replaced the way a reinstall does it: a new file renamed over. */
	g_assert_true(createDatabase(staged, malformed.c_str()));
	g_assert_cmpint(g_rename(staged.c_str(), path.c_str()), ==, 0);
	{
		SqliteBibleBackend backend(directory);
		g_assert_true(backend.hasModule("x"));
		g_assert_false(backend.moduleCapabilities("x").morphology);
	}
	/* And rewritten in place, as fast as possible. */
	g_remove(path.c_str());
	g_assert_true(createDatabase(path, valid.c_str()));
	{
		SqliteBibleBackend backend(directory);
		g_assert_true(backend.moduleCapabilities("x").morphology);
	}
	g_remove(path.c_str());
	g_assert_true(createDatabase(path, malformed.c_str()));
	{
		SqliteBibleBackend backend(directory);
		g_assert_false(backend.moduleCapabilities("x").morphology);
	}
	g_remove(path.c_str());
	g_remove(cache.c_str());
	g_rmdir(directory);
	g_free(directory);
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

void opensCrossReferencesWithoutRangeTable()
{
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-old-crossref-XXXXXX", nullptr);
	g_assert_nonnull(directory);
	const std::string path = std::string(directory) + "/Legacy.sqlite";
	const char *sql =
		"PRAGMA user_version=1;"
		"CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
		"INSERT INTO metadata VALUES('schema_version','1'),('module_id','Legacy'),"
		"('name','Legacy'),('language','en'),('module_type','bible'),"
		"('versification','custom'),('feature.verses','true'),('feature.search','true'),"
		"('feature.strong','false'),('feature.morphology','false'),"
		"('feature.headings','false'),('feature.footnotes','false'),"
		"('feature.crossrefs','true'),('feature.dictionary','false');"
		"CREATE TABLE books(book_id INTEGER PRIMARY KEY,osis TEXT UNIQUE,name TEXT,"
		"short_name TEXT,testament INTEGER,position INTEGER UNIQUE);"
		"INSERT INTO books VALUES(40,'John','John','Jn',2,1);"
		"CREATE TABLE verses(book_id INTEGER,chapter INTEGER,verse INTEGER,text TEXT,"
		"PRIMARY KEY(book_id,chapter,verse));"
		"INSERT INTO verses VALUES(40,3,16,'one'),(40,3,17,'two');"
		"CREATE TABLE cross_references(book_id INTEGER,chapter INTEGER,verse INTEGER,"
		"sequence INTEGER,offset INTEGER,display_text TEXT,PRIMARY KEY(book_id,chapter,verse,sequence));"
		"CREATE TABLE cross_reference_targets(book_id INTEGER,chapter INTEGER,verse INTEGER,"
		"sequence INTEGER,target_sequence INTEGER,target_book_id INTEGER,target_chapter INTEGER,"
		"target_verse INTEGER,PRIMARY KEY(book_id,chapter,verse,sequence,target_sequence));"
		"INSERT INTO cross_references VALUES(40,3,16,0,0,'John 3:17');"
		"INSERT INTO cross_reference_targets VALUES(40,3,16,0,0,40,3,17);";
	g_assert_true(createDatabase(path, sql));
	{
		SqliteBibleBackend backend(directory);
		BibleKeyInfo key;
		g_assert_true(backend.hasModule("Legacy"));
		g_assert_true(backend.resolveKey("Legacy", "John 3:16", key));
		const BibleVerseContent content = backend.getVerseContent("Legacy", key.reference);
		const std::vector<BibleVerseContent> chapter =
			backend.getChapterContent("Legacy", key.reference, 17);
		g_assert_cmpuint(chapter.size(), ==, 17);
		assertSameContent(content, chapter[15]);
		g_assert_true(content.valid);
		g_assert_cmpuint(content.crossReferences.size(), ==, 1);
		g_assert_cmpuint(content.crossReferences[0].references.size(), ==, 1);
		g_assert_true(content.crossReferences[0].ranges.empty());
	}
	g_remove(path.c_str());
	g_remove((std::string(directory) + "/.validation-cache").c_str());
	g_rmdir(directory);
	g_free(directory);
}

void loadsExpandedCrossReferenceRanges()
{
	gchar *directory = g_dir_make_tmp("xiphos-sqlite-range-targets-XXXXXX", nullptr);
	g_assert_nonnull(directory);
	const std::string path = std::string(directory) + "/Ranges.sqlite";
	const char *sql =
		"PRAGMA user_version=1;"
		"CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
		"INSERT INTO metadata VALUES('schema_version','1'),('module_id','Ranges'),"
		"('name','Ranges'),('language','en'),('module_type','bible'),"
		"('versification','custom'),('feature.verses','true'),('feature.search','true'),"
		"('feature.strong','false'),('feature.morphology','false'),"
		"('feature.headings','false'),('feature.footnotes','false'),"
		"('feature.crossrefs','true'),('feature.dictionary','false');"
		"CREATE TABLE books(book_id INTEGER PRIMARY KEY,osis TEXT UNIQUE,name TEXT,"
		"short_name TEXT,testament INTEGER,position INTEGER UNIQUE);"
		"INSERT INTO books VALUES(2,'Exod','Exodus','Ex',1,1),"
		"(43,'John','John','Jn',2,2);"
		"CREATE TABLE verses(book_id INTEGER,chapter INTEGER,verse INTEGER,text TEXT,"
		"PRIMARY KEY(book_id,chapter,verse));"
		"INSERT INTO verses VALUES(2,2,1,'one'),(2,2,3,'three'),"
		"(43,3,16,'source');"
		"CREATE TABLE cross_references(book_id INTEGER,chapter INTEGER,verse INTEGER,"
		"sequence INTEGER,offset INTEGER,display_text TEXT,"
		"PRIMARY KEY(book_id,chapter,verse,sequence));"
		"INSERT INTO cross_references VALUES(43,3,16,0,0,'Exodus 2:1-3');"
		"CREATE TABLE cross_reference_ranges(book_id INTEGER,chapter INTEGER,verse INTEGER,"
		"sequence INTEGER,range_sequence INTEGER,start_testament INTEGER,start_book_id INTEGER,"
		"start_chapter INTEGER,start_verse INTEGER,end_testament INTEGER,end_book_id INTEGER,"
		"end_chapter INTEGER,end_verse INTEGER,"
		"PRIMARY KEY(book_id,chapter,verse,sequence,range_sequence));"
		"INSERT INTO cross_reference_ranges VALUES(43,3,16,0,0,1,2,2,1,1,2,2,3);"
		"CREATE TABLE cross_reference_range_targets(book_id INTEGER,chapter INTEGER,"
		"verse INTEGER,sequence INTEGER,range_sequence INTEGER,target_sequence INTEGER,"
		"target_testament INTEGER,target_book_id INTEGER,target_chapter INTEGER,"
		"target_verse INTEGER,"
		"PRIMARY KEY(book_id,chapter,verse,sequence,range_sequence,target_sequence));"
		"INSERT INTO cross_reference_range_targets VALUES"
		"(43,3,16,0,0,0,1,2,2,1),(43,3,16,0,0,1,1,2,2,3);";
	g_assert_true(createDatabase(path, sql));
	{
		SqliteBibleBackend backend(directory);
		BibleKeyInfo key;
		g_assert_true(backend.resolveKey("Ranges", "John 3:16", key));
		const BibleVerseContent content = backend.getVerseContent("Ranges", key.reference);
		const std::vector<BibleVerseContent> chapter =
			backend.getChapterContent("Ranges", key.reference, 16);
		g_assert_cmpuint(chapter.size(), ==, 16);
		assertSameContent(content, chapter[15]);
		g_assert_true(content.valid);
		g_assert_cmpuint(content.crossReferences.size(), ==, 1);
		g_assert_cmpstr(content.crossReferences[0].displayText.c_str(), ==, "Exodus 2:1-3");
		g_assert_cmpuint(content.crossReferences[0].ranges.size(), ==, 1);
		const BibleCrossReference::Range &range = content.crossReferences[0].ranges[0];
		g_assert_cmpint(range.start.testament, ==, 1);
		g_assert_cmpint(range.start.book, ==, 2);
		g_assert_cmpint(range.start.verse, ==, 1);
		g_assert_cmpint(range.end.testament, ==, 1);
		g_assert_cmpint(range.end.book, ==, 2);
		g_assert_cmpint(range.end.verse, ==, 3);
		g_assert_cmpuint(range.expandedReferences.size(), ==, 2);
		g_assert_cmpint(range.expandedReferences[0].testament, ==, 1);
		g_assert_cmpint(range.expandedReferences[0].book, ==, 2);
		g_assert_cmpint(range.expandedReferences[0].verse, ==, 1);
		g_assert_cmpint(range.expandedReferences[1].testament, ==, 1);
		g_assert_cmpint(range.expandedReferences[1].book, ==, 2);
		g_assert_cmpint(range.expandedReferences[1].verse, ==, 3);
	}
	g_remove(path.c_str());
	g_remove((std::string(directory) + "/.validation-cache").c_str());
	g_rmdir(directory);
	g_free(directory);
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
	g_test_add_func("/backend/sqlite/chapter-content", chapterContentMatchesVerseReads);
	g_test_add_func("/backend/sqlite/metadata", metadataAndCapabilities);
	g_test_add_func("/backend/sqlite/navigation-boundaries", navigationBoundaries);
	g_test_add_func("/backend/sqlite/search-modes", searchModes);
	g_test_add_func("/backend/sqlite/missing-database", missingDatabase);
	g_test_add_func("/backend/sqlite/invalid-schema", invalidSchema);
	g_test_add_func("/backend/sqlite/corrupt-database", corruptDatabase);
	g_test_add_func("/backend/sqlite/schema-validation", schemaValidation);
	g_test_add_func("/backend/sqlite/invalid-references", invalidReferencesAndCapabilities);
	g_test_add_func("/backend/sqlite/morphology-capabilities",
		morphologyCapabilitiesRequireReadableValidData);
	g_test_add_func("/backend/sqlite/morphology-legacy-lookup",
		legacyMorphologyLookupWithoutIndex);
	g_test_add_func("/backend/sqlite/validation-verdicts", validationVerdictsFollowTheFile);
	g_test_add_func("/backend/sqlite/read-only", opensReadOnly);
	g_test_add_func("/backend/sqlite/crossrefs-no-range-extension",
		opensCrossReferencesWithoutRangeTable);
	g_test_add_func("/backend/sqlite/crossref-range-targets",
		loadsExpandedCrossReferenceRanges);
	const int result = g_test_run();
	g_remove(database.c_str());
	g_rmdir(fixtureDirectory.c_str());
	g_free(directory);
	return result;
}
