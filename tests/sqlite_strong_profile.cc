#include <sqlite3.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Row {
	int testament = 0, book = 0, chapter = 0, verse = 0;
	std::string context, word;
};

long long elapsedUs(const std::chrono::steady_clock::time_point &start)
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - start).count();
}

void profile(sqlite3 *db, const char *label, const char *sql, bool makeObjects,
	int repetitions = 1000)
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) {
		std::fprintf(stderr, "%s: %s\n", label, sqlite3_errmsg(db));
		std::exit(1);
	}
	int rows = 0;
	volatile std::size_t consumed = 0;
	const auto start = std::chrono::steady_clock::now();
	for (int repetition = 0; repetition < repetitions; ++repetition) {
		sqlite3_bind_text(statement, 1, "H430", -1, SQLITE_STATIC);
		std::vector<Row> objects;
		while (sqlite3_step(statement) == SQLITE_ROW) {
			if (repetition == 0) ++rows;
			if (makeObjects) {
				Row row;
				row.testament = sqlite3_column_int(statement, 0);
				row.book = sqlite3_column_int(statement, 1);
				row.chapter = sqlite3_column_int(statement, 2);
				row.verse = sqlite3_column_int(statement, 3);
				const unsigned char *context = sqlite3_column_text(statement, 4);
				const unsigned char *word = sqlite3_column_text(statement, 5);
				if (context) row.context = reinterpret_cast<const char *>(context);
				if (word) row.word = reinterpret_cast<const char *>(word);
				objects.push_back(std::move(row));
			} else {
				for (int column = 0; column < sqlite3_column_count(statement); ++column)
					consumed += static_cast<std::size_t>(sqlite3_column_bytes(statement, column));
			}
		}
		sqlite3_reset(statement);
		sqlite3_clear_bindings(statement);
		consumed += objects.size();
	}
	const long long elapsed = elapsedUs(start);
	std::printf("stage=%s rows=%d repetitions=%d total_us=%lld mean_us=%.3f objects=%s\n",
		label, rows, repetitions, elapsed,
		static_cast<double>(elapsed) / repetitions, makeObjects ? "yes" : "no");
	sqlite3_finalize(statement);
}
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::fprintf(stderr, "usage: %s DATABASE\n", argv[0]);
		return 2;
	}
	sqlite3 *db = nullptr;
	if (sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
		return 1;
	profile(db, "A_matches", "SELECT book_id,chapter,verse,sequence FROM verse_word_strongs WHERE strong=? LIMIT 100", false);
	profile(db, "B_words", "SELECT s.book_id,s.chapter,s.verse,s.sequence,vw.text FROM verse_word_strongs s JOIN verse_words vw USING(book_id,chapter,verse,sequence) WHERE s.strong=? LIMIT 100", false);
	profile(db, "C_verses_no_context", "SELECT b.testament,s.book_id,s.chapter,s.verse,vw.text FROM verse_word_strongs s JOIN verse_words vw USING(book_id,chapter,verse,sequence) JOIN verses v USING(book_id,chapter,verse) JOIN books b USING(book_id) WHERE s.strong=? LIMIT 100", false);
	profile(db, "E_context", "SELECT b.testament,s.book_id,s.chapter,s.verse,v.text,vw.text FROM verse_word_strongs s JOIN verse_words vw USING(book_id,chapter,verse,sequence) JOIN verses v USING(book_id,chapter,verse) JOIN books b USING(book_id) WHERE s.strong=? LIMIT 100", false);
	profile(db, "D_objects", "SELECT b.testament,s.book_id,s.chapter,s.verse,v.text,vw.text FROM verse_word_strongs s JOIN verse_words vw USING(book_id,chapter,verse,sequence) JOIN verses v USING(book_id,chapter,verse) JOIN books b USING(book_id) WHERE s.strong=? LIMIT 100", true);
	profile(db, "original_ordered", "SELECT b.testament,v.book_id,v.chapter,v.verse,v.text,vw.text FROM verse_word_strongs s JOIN verses v USING(book_id,chapter,verse) JOIN verse_words vw USING(book_id,chapter,verse,sequence) JOIN books b USING(book_id) WHERE s.strong=? ORDER BY b.position,v.chapter,v.verse,vw.sequence LIMIT 100 OFFSET 0", true, 100);
	profile(db, "final_ordered", "SELECT b.testament,s.book_id,s.chapter,s.verse,v.text,vw.text FROM books b CROSS JOIN verse_word_strongs s INDEXED BY verse_word_strongs_canonical JOIN verse_words vw USING(book_id,chapter,verse,sequence) JOIN verses v USING(book_id,chapter,verse) WHERE s.strong=? AND s.book_id=b.book_id ORDER BY b.position,s.chapter,s.verse,s.sequence LIMIT 100 OFFSET 0", true);
	sqlite3_close(db);
	return 0;
}
