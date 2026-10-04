#include <glib.h>
#include <json-glib/json-glib.h>

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

static void study_summary_test()
{
    InterlFila row = {};
    row.forma = g_strdup("θεοῦ");
    row.raiz = g_strdup("θεός");
    row.translit = g_strdup("theós");
    row.es = g_strdup("de Dios");
    row.strong = g_strdup("G2316");
    row.strongs = g_strdup("G2316 · G2532");
    row.morph = g_strdup("N-GSM");
    row.morph_es = g_strdup("Sustantivo · genitivo · singular · masculino");
    gchar *text = main_interlineal_ficha_texto(&row, "Juan 1:1");
    g_assert_true(g_utf8_validate(text, -1, nullptr));
    g_assert_cmpstr(text, ==,
        "Pasaje: Juan 1:1\nForma en el versículo: θεοῦ\nLema: θεός\n"
        "Transliteración: theós\nEspañol / glosa: de Dios\n"
        "Strong: G2316 · G2532\n"
        "Análisis gramatical: Sustantivo · genitivo · singular · masculino\n"
        "Código morfológico: N-GSM");
    g_free(text);
    g_free(row.forma); g_free(row.raiz); g_free(row.translit); g_free(row.es);
    g_free(row.strong); g_free(row.strongs); g_free(row.morph); g_free(row.morph_es);

    // Unrecognized morphology remains visible; missing lexical fields stay absent.
    row = {};
    row.forma = g_strdup("θεός"); // decomposed accent must survive verbatim
    row.strong = g_strdup("G2316");
    row.strongs = g_strdup("");
    row.morph = g_strdup("custom:XYZ");
    text = main_interlineal_ficha_texto(&row, nullptr);
    g_assert_cmpstr(text, ==,
        "Forma en el versículo: θεός\nStrong: G2316\nCódigo morfológico: custom:XYZ");
    g_free(text);
    g_free(row.forma); g_free(row.strong); g_free(row.strongs); g_free(row.morph);
    row = {};
    text = main_interlineal_ficha_texto(&row, "");
    g_assert_cmpstr(text, ==, "");
    g_free(text);
    text = main_interlineal_ficha_texto(nullptr, "Juan 1:1");
    g_assert_cmpstr(text, ==, "");
    g_free(text);
    g_print("interlinear_study_summary_failures=0\n");
}

static void hebrew_study_test()
{
    const char *pointed = "בְּרֵאשִׁ֖ית";
    g_assert_true(main_interlineal_es_hebreo(pointed));
    g_assert_true(main_interlineal_es_hebreo("(שָׁלוֹם)"));
    g_assert_false(main_interlineal_es_hebreo("θεός"));
    g_assert_false(main_interlineal_es_hebreo("H7225"));
    g_assert_false(main_interlineal_es_hebreo(nullptr));
    g_assert_false(main_interlineal_es_hebreo(""));
    const struct { const char *input; const char *expected; } cases[] = {
        {pointed, "בראשית"},
        {"שָׁלוֹם־לְךָ׃", "שלום־לך׃"},
        {"בְּ/רֵאשִׁית", "ב/ראשית"},
        {"שׁ שׂ", "ש ש"},
        {"θεός café שָׁלוֹם 123", "θεός café שלום 123"},
        {"שלום", "שלום"},
        {"", ""}, {nullptr, ""}
    };
    for (const auto &item : cases) {
        gchar *text = main_interlineal_sin_signos_hebreos(item.input);
        g_assert_cmpstr(text, ==, item.expected);
        g_assert_true(g_utf8_validate(text, -1, nullptr));
        g_free(text);
    }
    const char invalid[] = {char(0xff), 0};
    g_assert_false(main_interlineal_es_hebreo(invalid));
    gchar *text = main_interlineal_sin_signos_hebreos(invalid);
    g_assert_cmpstr(text, ==, invalid);
    g_free(text);

    // Hebrew without Strong metadata still gets its study variant.
    InterlFila row = {};
    row.forma = g_strdup(pointed);
    row.raiz = g_strdup("רֵאשִׁית");
    text = main_interlineal_ficha_texto(&row, "Génesis 1:1");
    g_assert_cmpstr(text, ==,
        "Pasaje: Génesis 1:1\nForma en el versículo: בְּרֵאשִׁ֖ית\n"
        "Hebreo sin signos: בראשית\nLema: רֵאשִׁית");
    g_assert_cmpstr(row.forma, ==, pointed);
    g_free(text); g_free(row.forma); g_free(row.raiz);
    row = {};
    row.forma = g_strdup("שלום");
    text = main_interlineal_ficha_texto(&row, nullptr);
    g_assert_cmpstr(text, ==, "Forma en el versículo: שלום");
    g_free(text); g_free(row.forma);
    g_print("hebrew_study_failures=0\n");
}

/* Sin signos de puntuación (el C guarda la forma así: strip_punct). */
static std::string only_letters(const char *text)
{
    std::string out;
    for (const char *p = text ? text : ""; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (g_unichar_isalnum(c)) out.append(p, g_utf8_next_char(p) - p);
    }
    return out;
}

/* La posición de la ficha (ref_tisch + posición + Strong) es la del alineamiento en Python
 * (tools/tagnt/alinear.py). tests/data/posiciones_tisch.json trae entradas crudas de Tisch con las posiciones
 * que da Python: Jn 1:1, Jn 1:4 (lectura propia de Tisch), Mt 5:4 y 5:5 (reordenados), 36 versículos del NT
 * muestreados con semilla fija y un caso sintético con un <w> vacío y otro sin Strong, que el HTML omite hoy. */
static void tisch_position_parity_test()
{
    JsonParser *parser = json_parser_new();
    GError *error = nullptr;
    g_assert_true(json_parser_load_from_file(parser, TISCH_POSITIONS_FIXTURE, &error));
    JsonArray *items = json_node_get_array(json_parser_get_root(parser));
    guint checked = 0;
    for (guint i = 0; i < json_array_get_length(items); i++) {
        JsonObject *item = json_array_get_object_element(items, i);
        const char *ref = json_object_get_string_member(item, "ref");
        JsonArray *words = json_object_get_array_member(item, "palabras");
        GList *tokens = main_interlineal_tokens_de_crudo(json_object_get_string_member(item, "raw"));
        GList *cursor = tokens;
        guint expectedTokens = 0;
        for (guint w = 0; w < json_array_get_length(words); w++) {
            JsonObject *word = json_array_get_object_element(words, w);
            const std::string form = only_letters(json_object_get_string_member(word, "forma"));
            gint pos = (gint)json_object_get_int_member(word, "pos");
            if (form.empty()) continue;            // el C no genera token para un <w> vacío, pero su posición cuenta
            expectedTokens++;
            g_assert_nonnull(cursor);
            auto *token = static_cast<InterlTok *>(cursor->data);
            if (token->pos != pos) g_error("%s: posición C %d != Python %d (%s)", ref, token->pos, pos, form.c_str());
            const std::string tokenForm = only_letters(token->forma);
            g_assert_cmpstr(tokenForm.c_str(), ==, form.c_str());
            g_assert_cmpstr(token->strong ? token->strong : "", ==, json_object_get_string_member(word, "strong"));
            cursor = cursor->next;
            checked++;
        }
        g_assert_null(cursor);
        g_assert_cmpuint(g_list_length(tokens), ==, expectedTokens);
        main_interlineal_tokens_free(tokens);
    }
    g_assert_cmpuint(json_array_get_length(items), >=, 40);
    g_assert_cmpuint(checked, >, 600);
    g_object_unref(parser);
}

int main()
{
    tisch_position_parity_test();
    study_summary_test();
    hebrew_study_test();
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
    // La puntuación pegada a la palabra («θεός,») no forma parte de la forma que se muestra.
    SqliteImportVerse punctuated; punctuated.reference = {2,43,3,18}; punctuated.text = "θεός, δι’";
    BibleWordInfo comma = word; comma.text = "θεός,"; comma.length = comma.text.size();
    BibleWordInfo elided = word; elided.start = 10; elided.text = "δι’"; elided.length = elided.text.size();
    elided.strong = "G1223"; elided.strongs = {{StrongLanguage::Greek,1223}};
    punctuated.words = {comma, elided};
    verses.push_back(punctuated);
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
    tokens = main_interlineal_versiculo("John 3:18");
    g_assert_cmpuint(g_list_length(tokens), ==, 2);
    g_assert_cmpstr(static_cast<InterlTok *>(tokens->data)->forma, ==, "θεός");
    g_assert_cmpstr(static_cast<InterlTok *>(tokens->next->data)->forma, ==, "δι’");
    main_interlineal_tokens_free(tokens);
    GList *occurrences = main_interlineal_ocurrencias("G2316", 10);
    g_assert_cmpuint(g_list_length(occurrences), ==, 3);
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
