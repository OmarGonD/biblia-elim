#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#include <string>
#include <vector>
#include <cstring>

#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/usfm_importer.h"
#include "strong_backend_contract.h"

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, nullptr);
	gchar *directory = g_dir_make_tmp("xiphos-usfm-import-XXXXXX", nullptr);
	g_assert_nonnull(directory);
	const std::string output = std::string(directory) + "/fixture.sqlite";
	const std::string root = SRCDIR "/tests/fixtures/sqlite/";
	UsfmImportOptions options;
	options.moduleId = "usfm-fixture";
	options.name = "USFM Fixture";
	options.language = "en";
	options.versification = "custom";
	options.source = "test";
	UsfmImportStats stats;
	std::string error;
	g_assert_true(importUsfm({root + "GEN.usfm", root + "JHN.usfm"}, output,
		options, stats, error));
	g_assert_cmpuint(stats.books, ==, 2);
	g_assert_cmpuint(stats.verses, ==, 6);
	g_assert_cmpuint(stats.footnotesSkipped, ==, 1);
	g_assert_cmpuint(stats.crossReferencesSkipped, ==, 1);
	SqliteBibleBackend backend(directory);
	g_assert_true(backend.hasModule("usfm-fixture"));
	g_assert_true(backend.moduleCapabilities("usfm-fixture").strongs);
	g_assert_false(backend.moduleCapabilities("usfm-fixture").morphology);
	BibleKeyInfo key;
	g_assert_true(backend.resolveKey("usfm-fixture", "Genesis 1:1", key));
	BibleVerseContent content = backend.getVerseContent("usfm-fixture", key.reference);
	g_assert_true(content.valid);
	g_assert_nonnull(strstr(content.plainText.c_str(), "continuation"));
	g_assert_null(strstr(content.plainText.c_str(), "strong="));
	g_assert_true(content.paragraphBreak);
	g_assert_cmpuint(content.headings.size(), ==, 1);
	g_assert_cmpstr(content.headings[0].text.c_str(), ==, "Genesis title");
	g_assert_cmpuint(content.spans.size(), ==, 1);
	g_assert_cmpstr(content.plainText.substr(content.spans[0].start, content.spans[0].length).c_str(), ==, "created");
	g_assert_cmpuint(content.words.size(), ==, 1);
	g_assert_cmpstr(content.words[0].strong.c_str(), ==, "H0430,H01234");
	g_assert_cmpuint(content.words[0].strongs.size(), ==, 2);
	g_assert_cmpstr(content.plainText.substr(content.words[0].start, content.words[0].length).c_str(), ==, "God");
	StrongId h430{StrongLanguage::Hebrew, 430};
	const auto occurrences = backend.findStrongOccurrences("usfm-fixture", h430, 100, 0);
	g_assert_cmpuint(occurrences.size(), ==, 2);
	g_assert_cmpstr(occurrences[0].word.c_str(), ==, "God");
	g_assert_cmpstr(occurrences[0].context.c_str(), ==, content.plainText.c_str());
	const auto secondPage = backend.findStrongOccurrences("usfm-fixture", h430, 1, 1);
	g_assert_cmpuint(secondPage.size(), ==, 1);
	g_assert_cmpint(secondPage[0].reference.book, ==, 43);
	g_assert_cmpstr(secondPage[0].word.c_str(), ==, "world");
	StrongId h1234{StrongLanguage::Hebrew, 1234};
	const auto secondStrong = backend.findStrongOccurrences("usfm-fixture", h1234, 10, 0);
	g_assert_cmpuint(secondStrong.size(), ==, 1);
	g_assert_cmpstr(secondStrong[0].word.c_str(), ==, "God");
	g_assert_null(strstr(content.plainText.c_str(), "A note"));
	BibleSearchQuery query; query.mode = BibleSearchMode::Phrase; query.text = "world";
	g_assert_cmpuint(backend.search("usfm-fixture", query).size(), ==, 1);
	g_assert_cmpstr(backend.navigate("usfm-fixture", "Genesis 1:1", 1).c_str(), ==, "Genesis 1:2");
	StrongBackendContractFixture strongFixture;
	strongFixture.module = "usfm-fixture";
	strongFixture.noStrongReference = "John 4:1";
	strongFixture.noStrongWord = "traveler";
	runStrongBackendContractTests(backend, strongFixture);
	gchar *legacyDirectory = g_dir_make_tmp("xiphos-usfm-legacy-XXXXXX", nullptr);
	g_assert_nonnull(legacyDirectory);
	const std::string legacyOutput = std::string(legacyDirectory) + "/fixture.sqlite";
	gchar *databaseBytes = nullptr;
	gsize databaseSize = 0;
	g_assert_true(g_file_get_contents(output.c_str(), &databaseBytes,
		&databaseSize, nullptr));
	g_assert_true(g_file_set_contents(legacyOutput.c_str(), databaseBytes,
		databaseSize, nullptr));
	g_free(databaseBytes);
	sqlite3 *legacyDatabase = nullptr;
	g_assert_cmpint(sqlite3_open(legacyOutput.c_str(), &legacyDatabase), ==, SQLITE_OK);
	g_assert_cmpint(sqlite3_exec(legacyDatabase,
		"DROP INDEX verse_word_strongs_canonical; "
		"CREATE INDEX verse_word_strongs_id ON verse_word_strongs(strong); "
		"UPDATE metadata SET value='false' WHERE key='feature.strong'",
		nullptr, nullptr, nullptr), ==, SQLITE_OK);
	sqlite3_close(legacyDatabase);
	SqliteBibleBackend legacyBackend(legacyDirectory);
	g_assert_false(legacyBackend.moduleCapabilities("usfm-fixture").strongs);
	g_assert_cmpuint(legacyBackend.findStrongOccurrences(
		"usfm-fixture", h430, 100, 0).size(), ==, 2);
	g_remove(legacyOutput.c_str());
	g_rmdir(legacyDirectory);
	g_free(legacyDirectory);
	gchar *badBook = g_build_filename(directory, "BAD.usfm", nullptr);
	g_file_set_contents(badBook, "\\id XXX\n\\c 1\n\\v 1 bad\n", -1, nullptr);
	UsfmImportStats badStats; std::string badError;
	g_assert_false(importUsfm({badBook}, std::string(directory) + "/bad.sqlite", options, badStats, badError));
	gchar *duplicate = g_build_filename(directory, "DUP.usfm", nullptr);
	g_file_set_contents(duplicate, "\\id GEN\n\\c 1\n\\v 1 one\n\\v 1 two\n", -1, nullptr);
	g_assert_false(importUsfm({duplicate}, std::string(directory) + "/dup.sqlite", options, badStats, badError));
	g_remove(output.c_str());
	g_remove(badBook); g_remove(duplicate); g_free(badBook); g_free(duplicate);
	g_rmdir(directory);
	g_free(directory);
	return 0;
}
