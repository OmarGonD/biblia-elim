/*
 * SQLITE-REAL-101: SQLite modules declare their real versification and
 * convert references between versifications through SWORD's tables, as
 * SWORD modules do.
 *
 *   ./tests/sqlite_versification_test
 *
 * With BIBLIA_ELIM_SQLITE_COMPARE=<dir> it also converts every verse of
 * every pair of SQLite modules in <dir> and checks each against the SWORD
 * module of the same id (SQLite id "rv1909" = SWORD "SpaRV1909").
 */
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/sqlite/sqlite_module_writer.h"
#include "backend/sword/sword_backend.h"

#include "main/settings.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

SETTINGS settings = {};
char *sword_locale = nullptr;
extern "C" void main_dialog_search_percent_update(char, void *) {}
extern "C" void main_sidebar_search_percent_update(char, void *) {}
extern "C" void main_index_percent_update(char, void *) {}
extern "C" void main_setup_displays(void) {}
extern "C" void main_clear_abbreviations(void) {}
extern "C" void main_add_abbreviation(const char *, const char *) {}
extern "C" int main_is_module(char *) { return 0; }
extern "C" void gui_generic_warning(const char *) {}
extern "C" const char *main_get_language_map(const char *language) { return language; }
extern "C" char *main_get_mod_config_file(const char *, const char *) { return nullptr; }
extern "C" char *main_format_number(int value) { return g_strdup_printf("%d", value); }
extern "C" gchar *XI_g_strdup_printf(const char *, int, const gchar *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	gchar *result = g_strdup_vprintf(format, arguments);
	va_end(arguments);
	return result;
}

namespace {

int failures = 0;

void check(bool condition, const std::string &message)
{
	if (condition) return;
	++failures;
	std::fprintf(stderr, "FAIL: %s\n", message.c_str());
}

void writeModule(const std::string &directory, const std::string &id,
		 const std::string &versification,
		 const std::vector<std::pair<std::string, std::vector<int>>> &verses)
{
	SqliteModuleMetadata metadata;
	metadata.moduleId = id;
	metadata.name = id;
	metadata.language = "es";
	metadata.versification = versification;
	std::vector<SqliteImportBook> books;
	std::vector<SqliteImportVerse> rows;
	std::map<std::string, int> ids = {{"Ps", 19}, {"Tob", 67}, {"John", 43}};
	std::map<std::string, int> seen;
	for (const auto &verse : verses) {
		const std::string &osis = verse.first;
		if (!seen.count(osis)) {
			seen[osis] = 1;
			books.push_back({ids[osis], osis == "John" ? 2 : 1,
				static_cast<int>(books.size()) + 1, osis,
				osis == "Ps" ? "Salmos" : osis == "Tob" ? "Tobías" : "Juan",
				osis});
		}
		SqliteImportVerse row;
		row.reference = {osis == "John" ? 2 : 1, ids[osis], verse.second[0],
			verse.second[1]};
		row.text = osis + " " + std::to_string(verse.second[0]) + ":" +
			std::to_string(verse.second[1]);
		rows.push_back(row);
	}
	std::string error;
	check(SqliteModuleWriter().write(metadata, books, rows,
		directory + "/" + id + ".sqlite", error), "write " + id + ": " + error);
}

void synthetic()
{
	gchar *temporary = g_dir_make_tmp("sqlite-v11n-XXXXXX", nullptr);
	const std::string directory = temporary;
	/* A Vulgate Bible (with Tobit) and a KJV one (without). */
	writeModule(directory, "vulgata", "vulg",
		{{"Ps", {22, 1}}, {"Ps", {118, 1}}, {"Tob", {1, 1}}, {"John", {3, 16}}});
	writeModule(directory, "kjv", "kjv",
		{{"Ps", {23, 1}}, {"Ps", {119, 1}}, {"John", {3, 16}}});

	SqliteBibleBackend plain(directory);
	check(plain.versification("vulgata") == "Vulg" &&
		plain.versification("kjv") == "KJV", "declared versifications read");
	/* Without a mapper nothing crosses versifications (never reread). */
	check(plain.convertReference("vulgata", "Salmos 22:1", "kjv").status ==
		BibleReferenceMapping::Unmapped, "no mapper: Unmapped");

	SqliteBibleBackend backend(directory);
	backend.setVersificationMapper(makeSwordVersificationMapper());
	BibleReferenceConversion c = backend.convertReference("vulgata", "Salmos 22:1", "kjv");
	check(c.status == BibleReferenceMapping::Mapped && c.target.key == "Salmos 23:1",
		"Vulgate Ps 22:1 -> KJV Ps 23:1: " + c.target.key);
	c = backend.convertReference("kjv", "Salmos 119:1", "vulgata");
	check(c.status == BibleReferenceMapping::Mapped && c.target.key == "Salmos 118:1",
		"KJV Ps 119:1 -> Vulgate Ps 118:1: " + c.target.key);
	c = backend.convertReference("vulgata", "Juan 3:16", "kjv");
	check(c.status == BibleReferenceMapping::Mapped && c.target.key == "Juan 3:16",
		"John 3:16 identical");
	c = backend.convertReference("vulgata", "Tobías 1:1", "kjv");
	check(c.status == BibleReferenceMapping::Unmapped,
		"Tobit has no counterpart in a KJV Bible");
	c = backend.convertReferenceFromVersification("KJV", "Psalms 23:1", "vulgata");
	check(c.status == BibleReferenceMapping::Mapped && c.target.key == "Salmos 22:1",
		"from a remembered versification name: " + c.target.key);
	c = backend.convertReferenceFromVersification("NoSuchSystem", "Psalms 23:1", "vulgata");
	check(c.status != BibleReferenceMapping::Mapped, "unknown versification name");
	g_free(temporary);
}

/* Every verse of every pair: SQLite's conversion equals SWORD's. */
int compareWithSword(const char *directory)
{
	SqliteBibleBackend sqlite(directory);
	sqlite.setVersificationMapper(makeSwordVersificationMapper());
	SwordBackend sword;
	auto swordId = [](const std::string &id) {
		return id == "rv1909" ? std::string("SpaRV1909") : id;
	};
	std::vector<std::string> modules;
	for (const BibleModuleInfo &info : sqlite.listModules())
		if (sword.hasModule(swordId(info.id))) modules.push_back(info.id);
	long total = 0, same = 0, differ = 0, emptyTarget = 0;
	for (const std::string &from : modules) {
		for (const std::string &to : modules) {
			if (from == to) continue;
			long pairDiffer = 0, pairTotal = 0;
			for (int testament = 1; testament <= 2; ++testament) {
				for (int book = 1;; ++book) {
					const std::string first = sqlite.setBook(from, "", testament, book);
					if (first.empty()) break;
					BibleKeyInfo info;
					if (!sqlite.resolveKey(from, first, info)) continue;
					for (int chapter = 1; chapter <= info.chapterCount; ++chapter) {
						BibleReference ref = info.reference;
						ref.chapter = chapter;
						for (const BibleVerse &verse : sqlite.getChapter(from, ref, false)) {
							const std::string osis = info.osisBook + "." +
								std::to_string(chapter) + "." +
								std::to_string(verse.reference.verse);
							const BibleReferenceConversion a =
								sqlite.convertReference(from, verse.key, to);
							BibleKeyInfo swordInfo;
							const std::string swordKey = sword.resolveKey(
								swordId(from), info.osisBook + " " +
								std::to_string(chapter) + ":" +
								std::to_string(verse.reference.verse),
								swordInfo) ? swordInfo.key : std::string();
							const BibleReferenceConversion b = sword.convertReference(
								swordId(from), swordKey, swordId(to));
							const bool am = a.status == BibleReferenceMapping::Mapped;
							const bool bm = b.status == BibleReferenceMapping::Mapped &&
								b.target.reference.chapter > 0 &&
								b.target.reference.verse > 0;
							const std::string at = am ? a.target.osisBook + "." +
								std::to_string(a.target.reference.chapter) + "." +
								std::to_string(a.target.reference.verse) : "-";
							const std::string bt = bm ? b.target.osisBook + "." +
								std::to_string(b.target.reference.chapter) + "." +
								std::to_string(b.target.reference.verse) : "-";
							++pairTotal;
							if (at == bt) { ++same; continue; }
							/* SWORD maps onto a verse its module leaves empty
							 * (a Bible that numbers but does not carry it);
							 * SQLite has no row for it: no counterpart. */
							if (!am && bm && sword.getText(swordId(to), b.target.key,
									false).find_first_not_of(" \t\n\r") ==
								    std::string::npos) {
								++emptyTarget;
								continue;
							}
							if (++pairDiffer <= 3)
								std::printf("  %s -> %s %s: sqlite %s sword %s\n",
									from.c_str(), to.c_str(), osis.c_str(),
									at.c_str(), bt.c_str());
						}
					}
				}
			}
			total += pairTotal;
			differ += pairDiffer;
			std::printf("%s -> %s: %ld verses, %ld differ\n", from.c_str(),
				to.c_str(), pairTotal, pairDiffer);
		}
	}
	std::printf("compare_total=%ld same=%ld target_verse_empty_in_sword=%ld differ=%ld\n",
		total, same, emptyTarget, differ);
	return differ == 0 ? 0 : 1;
}

} // namespace

int main()
{
	synthetic();
	int rc = failures ? 1 : 0;
	if (const char *directory = g_getenv("BIBLIA_ELIM_SQLITE_COMPARE"))
		rc |= compareWithSword(directory);
	std::printf("sqlite_versification_failures=%d\n", failures);
	return rc;
}
