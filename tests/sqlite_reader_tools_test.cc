#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include "backend/sqlite/sqlite_bible_backend.h"
#include "backend/sqlite/sqlite_module_writer.h"
#include "main/backend_access.h"
#include "main/nube_palabras.h"
#include "main/export_passage.h"
#include "main/settings.h"
#include <cassert>
#include <string>

BibleBackend *bible_backend = nullptr;
SETTINGS settings = {};
BibleBackend &main_backend_for(const char *) { return *bible_backend; }
extern "C" void _set_global_textual(const char *, const char *) {}

static EXPORT_DATA options(const std::string &file, int scope) {
    EXPORT_DATA d = {};
    d.filename = g_strdup(file.c_str()); d.passage_type = scope;
    d.start_verse = 16; d.end_verse = 17; d.verse_num = 1; d.version = 1;
    d.bookheader = g_strdup("%s%s %d ");
    d.chapterheader_book = g_strdup("Chapter %d ");
    d.chapterheader_chapter = g_strdup("%s%s %s %d ");
    d.versenumber = g_strdup("%d. ");
    d.verselayout_single_verse_ref_last = g_strdup("%s%s %s %d:%d%s");
    d.verselayout_single_verse_ref_first = g_strdup("%s%s %d:%d%s %s");
    d.verse_range_ref_first = (char *)"%s %d:%d-%d%s ";
    d.verse_range_ref_last = (char *)"%s%s %d:%d-%d%s";
    d.plain_bookheader = (char *)"%s %d ";
    d.plain_chapterheader_book = (char *)"Chapter %d ";
    d.plain_chapterheader_chapter = (char *)"%s %s %d ";
    d.plain_versenumber = (char *)"%d. ";
    d.plain_verselayout_single_verse_ref_last = (char *)"%s %s %d:%d%s";
    d.plain_verselayout_single_verse_ref_first = (char *)"%s %d:%d%s %s";
    d.plain_verse_range_ref_first = d.verse_range_ref_first;
    d.plain_verse_range_ref_last = d.verse_range_ref_last;
    d.verse_range_verse = g_strdup("%s%s");
    return d;
}
int main() {
    gchar *temp = g_dir_make_tmp("sqlite-reader-tools-XXXXXX", nullptr);
    assert(temp);
    const std::string root(temp), file = root + "/out.txt";
    SqliteModuleMetadata metadata; metadata.moduleId = "SQLiteOnly";
    metadata.name = "SQLite tools"; metadata.language = "es"; metadata.versification = "kjv";
    std::vector<SqliteImportBook> books = {{1,1,1,"Gen","Génesis","Gen"},{43,2,2,"John","Juan","Jn"}};
    std::vector<SqliteImportVerse> verses;
    for (int v : {16,17,18}) {
        SqliteImportVerse row; row.reference = {2,43,3,v};
        row.text = v == 16 ? "Dios amó al mundo." : v == 17 ? "Dios envió a Jesús." : "Jesús es luz.";
        verses.push_back(row);
    }
    SqliteImportVerse genesis; genesis.reference = {1,1,1,1}; genesis.text = "Dios creó."; verses.push_back(genesis);
    std::string error;
    assert(SqliteModuleWriter().write(metadata,books,verses,root+"/SQLiteOnly.sqlite",error));
    SqliteBibleBackend sqlite(root); bible_backend = &sqlite;
    settings.MainWindowModule = g_strdup("SQLiteOnly"); settings.currentverse = g_strdup("Juan 3:16");
    GList *list = main_nube_lista_libros("SQLiteOnly"); assert(g_list_length(list) == 2);
    assert(std::string(static_cast<NUBE_LIBRO *>(list->next->data)->osis) == "John");
    main_nube_lista_libros_free(list);
    gchar *book = main_nube_libro_de_clave("SQLiteOnly","John 3:16"); assert(book && std::string(book)=="Juan"); g_free(book);
    NUBE_CONTEO *count = main_nube_contar("SQLiteOnly","Juan",nullptr,20);
    assert(count && count->palabras->len > 0);
    int god = 0;
    for (guint i = 0; i < count->palabras->len; ++i) {
        auto *word = static_cast<NUBE_PALABRA *>(g_ptr_array_index(count->palabras,i));
        if (std::string(word->palabra)=="dios") god = word->cuenta;
    }
    assert(god == 2); main_nube_conteo_free(count);
    assert(main_get_max_verses("SQLiteOnly") == 18 && main_get_current_verse("SQLiteOnly") == 16);
    int exports = 0;
    for (int scope : {BIBLE,BOOK,CHAPTER,VERSE,VERSE_RANGE}) for (int html : {0,1}) for (int last : {0,1}) {
        auto data = options(file,scope); data.reference_last = last;
        main_export_content(data,html);
        gchar *text = nullptr; assert(g_file_get_contents(file.c_str(),&text,nullptr,nullptr));
        assert(std::string(text).find("Dios amó al mundo.") != std::string::npos);
        assert((std::string(text).find("Dios envió a Jesús.") != std::string::npos) == (scope != VERSE));
        assert((std::string(text).find("Dios creó.") != std::string::npos) == (scope == BIBLE));
        g_free(text); ++exports;
    }
    auto range = options(file,VERSE_RANGE); range.start_verse = range.end_verse = 17;
    main_export_content(range,0);
    gchar *text = nullptr; assert(g_file_get_contents(file.c_str(),&text,nullptr,nullptr));
    assert(std::string(text).find("Dios envió a Jesús.") != std::string::npos);
    assert(std::string(text).find("Dios amó al mundo.") == std::string::npos); g_free(text);
    g_print("sqlite_reader_tools cloud_count=2 exports=%d single_range=ok failures=0\n", exports);
    g_remove(file.c_str()); g_remove((root+"/SQLiteOnly.sqlite").c_str()); g_rmdir(temp); g_free(temp);
    g_free(settings.MainWindowModule); g_free(settings.currentverse);
}
