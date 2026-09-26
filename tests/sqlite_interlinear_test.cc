#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include <swmodule.h>
#include <versekey.h>

#include "backend/sword/sword_backend.h"
#include "main/interlineal.h"
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/sqlite/sqlite_module_writer.h"
#include "main/pulpito.h"
#include <glib/gstdio.h>
#include "main/settings.h"

SETTINGS settings = {};
char *sword_locale = nullptr;
BibleBackend *bible_backend = nullptr;
BibleBackend &main_backend_for(const char *) { return *bible_backend; }
extern "C" gchar *main_reference_for_module(const char *source,
                                           const char *key,
                                           const char *target)
{
	if (!source || !key || !target || !bible_backend) return nullptr;
	auto mapped = bible_backend->convertReference(source, key, target);
	return mapped.status == BibleReferenceMapping::Mapped
		? g_strdup(mapped.target.key.c_str()) : nullptr;
}

extern "C" void main_dialog_search_percent_update(char, void *) {}
extern "C" void main_sidebar_search_percent_update(char, void *) {}
extern "C" void main_index_percent_update(char, void *) {}
extern "C" void main_setup_displays(void) {}
extern "C" void main_clear_abbreviations(void) {}
extern "C" void main_add_abbreviation(const char *, const char *) {}
extern "C" int main_is_module(char *) { return 0; }
extern "C" void gui_generic_warning(const char *) {}
extern "C" const char *main_get_language_map(const char *language)
{
	return language;
}
extern "C" char *main_get_mod_config_file(const char *, const char *)
{
	return nullptr;
}
extern "C" char *main_format_number(int value)
{
	return g_strdup_printf("%d", value);
}
/* interlineal.cc is linked in whole for main_interlineal_cita_es(); the
 * rest of that file reaches into the GTK layer, which has no place in a
 * headless test. None of these is on the citation's path. */
extern "C" char *xml_get_value(const char *, const char *) { return nullptr; }
extern "C" void xml_set_or_create_value(const char *, const char *,
					const char *) {}
extern "C" void gui_interlineal_rellenar(void) {}
extern "C" void gui_lectura_sync_ficha_clear(void) {}
extern "C" gchar *main_morf_codigo(const char *) { return nullptr; }
extern "C" gchar *main_morf_es(const char *) { return nullptr; }
extern "C" gchar *main_morf_corto(const char *) { return nullptr; }
extern "C" void main_display_verse_list_in_sidebar(gchar *, gchar *, gchar *) {}

extern "C" gchar *XI_g_strdup_printf(const char *, int, const gchar *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	gchar *result = g_strdup_vprintf(format, arguments);
	va_end(arguments);
	return result;
}

extern "C" GList *get_list(int) { return nullptr; }
extern "C" char *xml_get_list_from_label(const char *, const char *, const char *) { return nullptr; }
extern "C" void xml_set_list_item(const char *, const char *, const char *, const char *) {}

int main()
{
    gchar *directory = g_dir_make_tmp("sqlite-interlinear-XXXXXX", nullptr);
    g_assert_nonnull(directory);
    std::string root(directory), error;
    SqliteModuleMetadata metadata;
    metadata.moduleId = "Tisch"; metadata.name = "SQLite-only original";
    metadata.language = "el"; metadata.versification = "kjv";
    std::vector<SqliteImportBook> books = {{43,2,1,"John","Juan","Jn"}};
    SqliteImportVerse verse; verse.reference = {2,43,3,16}; verse.text = "θεός";
    BibleWordInfo word; word.start = 0; word.length = verse.text.size();
    word.text = verse.text; word.strong = "G2316"; word.strongs.push_back({StrongLanguage::Greek,2316});
    verse.words.push_back(word);
    std::vector<SqliteImportVerse> verses = {verse};
    verse.reference.verse = 17; verses.push_back(verse);
    g_assert_true(SqliteModuleWriter().write(metadata,books,verses,root+"/Tisch.sqlite",error));
    metadata.moduleId = "VulgSQLite"; metadata.versification = "vulg";
    SqliteImportVerse psalm; psalm.reference = {1,19,118,176}; psalm.text = "tu ley";
    std::vector<SqliteImportBook> psalmBooks = {{19,1,1,"Ps","Salmos","Sal"}};
    g_assert_true(SqliteModuleWriter().write(metadata,psalmBooks,{psalm},root+"/VulgSQLite.sqlite",error));
    SqliteBibleBackend sqlite(root); bible_backend = &sqlite;
    settings.MainWindowModule = g_strdup("Tisch");
    settings.currentverse = g_strdup("John 3:16");
    // The legacy global backend stays NULL: no SWORD Bible can satisfy these reads.
    GList *tokens = main_interlineal_versiculo("John 3:16");
    g_assert_cmpuint(g_list_length(tokens), ==, 1);
    auto *token = static_cast<InterlTok *>(tokens->data);
    g_assert_cmpstr(token->forma, ==, "θεός");
    g_assert_cmpstr(token->strong, ==, "G2316");
    main_interlineal_tokens_free(tokens);
    GList *occurrences = main_interlineal_ocurrencias("G2316", 10);
    g_assert_cmpuint(g_list_length(occurrences), ==, 2);
    g_list_free_full(occurrences, g_free);
    gchar *text = main_pulpito_texto("Tisch", "John 3:16-17");
    g_assert_nonnull(text);
    g_assert_nonnull(g_strstr_len(text,-1,"θεός"));
    g_free(text);
    text = main_pulpito_texto("VulgSQLite", "Ps 118:176");
    g_assert_nonnull(text); g_assert_nonnull(g_strstr_len(text,-1,"tu ley")); g_free(text);
    GList *refs = BackEnd::parse_reference_list("vulg","Ps 118:176","Ps 118:1");
    g_assert_cmpuint(g_list_length(refs), ==, 1);
    g_assert_nonnull(g_strstr_len(static_cast<char *>(refs->data),-1,"176"));
    g_list_free_full(refs,g_free);
    // BibleBackend::versification() and VerseKey report SWORD's name ("Vulg");
    // it must select Vulgate numbering too, not fall back to KJV.
    g_assert_cmpstr(sqlite.versification("VulgSQLite").c_str(), ==, "Vulg");
    refs = BackEnd::parse_reference_list(sqlite.versification("VulgSQLite").c_str(), "Ps 118:176", "Ps 118:1");
    g_assert_cmpuint(g_list_length(refs), ==, 1);
    g_assert_nonnull(g_strstr_len(static_cast<char *>(refs->data),-1,"118:176"));
    g_list_free_full(refs,g_free);
    g_print("sqlite_interlinear tokens=1 strong_occurrences=2 pulpit=ok vulg_range=ok legacy_backend=null failures=0\n");
    main_interlineal_shutdown();
    g_free(settings.MainWindowModule); g_free(settings.currentverse);
    g_remove((root+"/VulgSQLite.sqlite").c_str());
    g_remove((root+"/Tisch.sqlite").c_str()); g_rmdir(directory); g_free(directory);
}
