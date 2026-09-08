#include "backend/strong_id.h"
#include "backend/sqlite/sqlite_strong_lexicon.h"

#include <sqlite3.h>

#include <cassert>
#include <cstdio>

int main()
{
	StrongId h430, g25;
	assert(parseStrongId("H430", h430));
	assert(parseStrongId("G25", g25));
	assert(formatStrongId(h430) == "H430");
	const std::vector<StrongId> ids = parseStrongIds("G2424,G5547");
	assert(ids.size() == 2);
	const StrongId g2424{StrongLanguage::Greek, 2424};
	const StrongId g5547{StrongLanguage::Greek, 5547};
	assert(ids[0] == g2424);
	assert(ids[1] == g5547);
	assert(!parseStrongId("X1", h430));

	const char *path = "/tmp/xiphos-strong-lexicon-test.sqlite";
	std::remove(path);
	sqlite3 *db = nullptr;
	assert(sqlite3_open(path, &db) == SQLITE_OK);
	assert(sqlite3_exec(db,
		"CREATE TABLE lexicon_entries("
		"strong TEXT PRIMARY KEY,lemma TEXT,transliteration TEXT,"
		"pronunciation TEXT,definition TEXT);"
		"INSERT INTO lexicon_entries VALUES("
		"'H430','elohim','elohim','el-o-heem','God');"
		"INSERT INTO lexicon_entries VALUES("
		"'G25','agapao','','','');",
		nullptr, nullptr, nullptr) == SQLITE_OK);
	sqlite3_close(db);

	SqliteStrongLexicon lexicon(path);
	LexiconEntry hebrew = lexicon.lookupStrong(h430);
	assert(hebrew.valid);
	assert(hebrew.lemma == "elohim");
	assert(hebrew.transliteration == "elohim");
	assert(hebrew.pronunciation == "el-o-heem");
	assert(hebrew.definition == "God");
	LexiconEntry incomplete = lexicon.lookupStrong(g25);
	assert(incomplete.valid);
	assert(incomplete.lemma == "agapao");
	assert(incomplete.transliteration.empty());
	assert(incomplete.pronunciation.empty());
	assert(incomplete.definition.empty());
	const StrongId missing{StrongLanguage::Greek, 99999};
	assert(!lexicon.lookupStrong(missing).valid);
	assert(!lexicon.contains(missing));
	std::remove(path);
	return 0;
}
