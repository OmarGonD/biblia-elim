#include "backend/morphology.h"
#include "backend/sqlite/sqlite_module_writer.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace {

std::size_t failures = 0;

void check(bool condition, const std::string &message)
{
	if (condition) return;
	++failures;
	std::cerr << "FAIL: " << message << '\n';
}

std::string scalar(sqlite3 *database, const char *sql)
{
	sqlite3_stmt *statement = nullptr;
	std::string result;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK &&
	    sqlite3_step(statement) == SQLITE_ROW) {
		const char *value = reinterpret_cast<const char *>(
			sqlite3_column_text(statement, 0));
		result = value ? value : "";
	}
	if (statement) sqlite3_finalize(statement);
	return result;
}

std::vector<std::tuple<int, int, std::string, std::string>> morphologyRows(
	sqlite3 *database)
{
	std::vector<std::tuple<int, int, std::string, std::string>> result;
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(database,
		"SELECT word_sequence,morphology_sequence,scheme,code "
		"FROM verse_word_morphology ORDER BY word_sequence,morphology_sequence",
		-1, &statement, nullptr) == SQLITE_OK) {
		while (sqlite3_step(statement) == SQLITE_ROW) {
			const char *scheme = reinterpret_cast<const char *>(sqlite3_column_text(statement, 2));
			const char *code = reinterpret_cast<const char *>(sqlite3_column_text(statement, 3));
			result.emplace_back(sqlite3_column_int(statement, 0),
				sqlite3_column_int(statement, 1), scheme ? scheme : "", code ? code : "");
		}
	}
	if (statement) sqlite3_finalize(statement);
	return result;
}

SqliteModuleMetadata metadata(const std::string &module)
{
	SqliteModuleMetadata result;
	result.moduleId = module;
	result.name = "Writer morphology test";
	result.language = "mul";
	result.versification = "custom";
	return result;
}

SqliteImportVerse verseWithMorphology()
{
	SqliteImportVerse verse;
	verse.reference = { 2, 43, 1, 1 };
	verse.text = "alpha beta gamma";
	BibleWordInfo alpha;
	alpha.start = 0;
	alpha.length = 5;
	alpha.text = "alpha";
	alpha.strong = "G3056";
	alpha.strongs = { { StrongLanguage::Greek, 3056 } };
	alpha.morphologyTags = {
		{ "robinson", "N-NSM" },
		{ "custom.alpha", "opaque/code" },
		{ "robinson", "N-NSM" }
	};
	BibleWordInfo beta;
	beta.start = 6;
	beta.length = 4;
	beta.text = "beta";
	beta.morphologyTags = { { "", "HR/Ncfsa" } };
	BibleWordInfo gamma;
	gamma.start = 11;
	gamma.length = 5;
	gamma.text = "gamma";
	gamma.strong = "H430";
	gamma.strongs = { { StrongLanguage::Hebrew, 430 } };
	verse.words = { alpha, beta, gamma };
	return verse;
}

} // namespace

int main()
{
	gchar *temporary = g_dir_make_tmp("sqlite-writer-morphology-XXXXXX", nullptr);
	if (!temporary) return 1;
	const std::string directory = temporary;
	const std::string output = directory + "/morph.sqlite";
	const std::vector<SqliteImportBook> books = {
		{ 43, 2, 43, "John", "John", "John" }
	};
	SqliteModuleWriter writer;
	std::string error;
	check(writer.write(metadata("writer-morph"), books, { verseWithMorphology() },
		output, error), "writer persists valid morphology: " + error);

	sqlite3 *database = nullptr;
	check(sqlite3_open_v2(output.c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) ==
		SQLITE_OK, "persisted database opens");
	if (database) {
		check(scalar(database, "PRAGMA user_version") == "1", "user_version remains 1");
		check(scalar(database,
			"SELECT value FROM metadata WHERE key='schema_version'") == "1",
			"schema_version remains 1");
		check(scalar(database,
			"SELECT value FROM metadata WHERE key='feature.morphology'") == "true",
			"morphology capability follows valid persisted rows");
		check(scalar(database,
			"SELECT name FROM sqlite_master WHERE type='index' AND "
			"name='verse_word_morphology_lookup'") ==
				"verse_word_morphology_lookup",
			"writer creates the exact-tag occurrence lookup index");
		const auto expected = std::vector<std::tuple<int, int, std::string, std::string>>{
			{ 0, 0, "robinson", "N-NSM" },
			{ 0, 1, "custom.alpha", "opaque/code" },
			{ 0, 2, "robinson", "N-NSM" },
			{ 1, 0, "", "HR/Ncfsa" }
		};
		check(morphologyRows(database) == expected,
			"multiple, duplicate, unknown and unqualified tags round-trip in order");
		check(scalar(database, "PRAGMA integrity_check") == "ok",
			"SQLite integrity check passes");
		check(scalar(database, "PRAGMA foreign_key_check").empty(),
			"persisted morphology foreign keys are valid");
		check(sqlite3_exec(database, "PRAGMA foreign_keys=ON", nullptr, nullptr, nullptr) ==
			SQLITE_OK, "foreign keys enabled for relationship test");
		check(sqlite3_exec(database,
			"INSERT INTO verse_word_morphology VALUES(43,1,1,99,0,'x','code')",
			nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT,
			"orphan morphology row is rejected by the owning-word foreign key");
		sqlite3_close(database);
	}

	SqliteImportVerse plain = verseWithMorphology();
	for (BibleWordInfo &word : plain.words) word.morphologyTags.clear();
	const std::string plainOutput = directory + "/plain.sqlite";
	error.clear();
	check(writer.write(metadata("writer-plain"), books, { plain }, plainOutput, error),
		"module without morphology remains writable: " + error);
	database = nullptr;
	if (sqlite3_open_v2(plainOutput.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) ==
	    SQLITE_OK) {
		check(scalar(database,
			"SELECT value FROM metadata WHERE key='feature.morphology'") == "false",
			"empty morphology table does not enable capability");
		check(scalar(database, "SELECT count(*) FROM verse_word_morphology") == "0",
			"module without morphology has no rows");
		sqlite3_close(database);
	} else {
		check(false, "module without morphology opens");
		if (database) sqlite3_close(database);
	}

	SqliteImportVerse malformed = verseWithMorphology();
	malformed.words[0].morphologyTags.push_back({ "bad scheme", "code" });
	const std::string failedOutput = directory + "/failed.sqlite";
	error.clear();
	check(!writer.write(metadata("writer-failed"), books, { malformed }, failedOutput,
		error) && error == "invalid morphology tag",
		"malformed neutral morphology is rejected deterministically");
	check(!g_file_test(failedOutput.c_str(), G_FILE_TEST_EXISTS),
		"failed morphology write creates no final output");
	check(!g_file_test((failedOutput + ".tmp").c_str(), G_FILE_TEST_EXISTS),
		"failed morphology write leaves no temporary output");

	SqliteImportVerse missingBook = verseWithMorphology();
	missingBook.reference.book = 44;
	const std::string relationalOutput = directory + "/relational-failure.sqlite";
	error.clear();
	check(!writer.write(metadata("writer-relational-failure"), books,
		{ missingBook }, relationalOutput, error),
		"writer rejects a verse whose book relationship is invalid");
	check(!g_file_test(relationalOutput.c_str(), G_FILE_TEST_EXISTS) &&
		!g_file_test((relationalOutput + ".tmp").c_str(), G_FILE_TEST_EXISTS),
		"transactional writer failure leaves no final or temporary output");

	g_remove(output.c_str());
	g_remove(plainOutput.c_str());
	g_rmdir(directory.c_str());
	g_free(temporary);
	std::cout << "sqlite_module_writer_morphology_failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
