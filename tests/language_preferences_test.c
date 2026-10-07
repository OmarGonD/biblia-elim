/* The real preference callbacks and XML persistence, on temporary files.
 * No window is presented and the restart notice is captured by a stub. */
#include "../src/gtk/preferences_dialog.c"
#include <glib/gstdio.h>
#include <stdarg.h>

SETTINGS settings;
char *locale_set[] = { "en_GB", "en_US", "es_ES", "fr_FR", "ko_KR",
	"zh_CN", "pt_BR", "pt_PT", "zh_TW", "de_DE", NULL };
static int notices;

const char *main_get_language_map(const char *language) { return language; }
void gui_generic_warning(const char *message)
{
	g_assert_nonnull(message);
	notices++;
}
void gui_generic_warning_modal(const char *message) { g_error("%s", message); }
gchar *XI_g_strdup_printf(const char *file, int line, const gchar *format, ...)
{
	(void)file; (void)line;
	va_list args; va_start(args, format);
	gchar *result = g_strdup_vprintf(format, args);
	va_end(args); return result;
}

int main(void)
{
	app_language_initialize("fr", NULL);
	gtk_disable_setlocale();
	if (!gtk_init_check()) {
		puts("language_preferences_test skipped: no display");
		return 77;
	}
	gchar *directory = g_dir_make_tmp("elim-language-prefs-XXXXXX", NULL);
	settings.fnconfigure = g_build_filename(directory, "settings.xml", NULL);
	settings.special_locale = g_strdup("fr_FR.UTF-8");
	const char *initial = "<Xiphos><locale><special>fr_FR.UTF-8</special></locale>"
		"<keys><verse>Juan 3:16</verse></keys></Xiphos>";
	g_assert_true(g_file_set_contents(settings.fnconfigure, initial, -1, NULL));
	g_assert_true(xml_parse_settings_file(settings.fnconfigure));
	combo.special_locale = elim_dropdown_new();
	g_object_ref_sink(combo.special_locale);
	setup_locale_combobox();
	setup_locale_combobox();
	g_assert_cmpint(notices, ==, 0);
	g_assert_cmpstr(settings.special_locale, ==, "fr_FR.UTF-8");
	GtkDropDown *dropdown = GTK_DROP_DOWN(combo.special_locale);
	g_assert_cmpstr(elim_dropdown_get_active_id(dropdown), ==, "fr");
	GHashTable *ids = g_hash_table_new(g_str_hash, g_str_equal);
	GPtrArray *list = elim_dropdown_ids(dropdown);
	for (guint i = 0; i < list->len; i++) {
		const char *id = g_ptr_array_index(list, i);
		g_assert_false(g_hash_table_contains(ids, id));
		g_hash_table_add(ids, (gpointer)id);
	}
	const char *choices[] = { "en_GB", "es", "fr", "ko_KR", "zh_CN", "pt_BR", "system" };
	for (guint i = 0; i < G_N_ELEMENTS(choices); i++) {
		g_assert_true(elim_dropdown_set_active_id(dropdown, choices[i]));
		gchar *saved = app_language_read_settings(settings.fnconfigure);
		g_assert_cmpstr(saved, ==, choices[i]);
		g_free(saved);
		gchar *verse = xml_get_value("keys", "verse");
		g_assert_cmpstr(verse, ==, "Juan 3:16");
		g_free(verse);
	}
	g_assert_cmpint(notices, ==, G_N_ELEMENTS(choices));
	g_assert_cmpstr(settings.special_locale, ==, "None");
	g_hash_table_destroy(ids);
	g_object_unref(combo.special_locale);
	xml_free_settings_doc();
	gchar *backup = g_strconcat(settings.fnconfigure, ".SAVE", NULL);
	g_remove(backup); g_remove(settings.fnconfigure); g_rmdir(directory);
	g_free(backup); g_free(directory); g_free(settings.fnconfigure);
	g_free(settings.special_locale);
	puts("language_preferences_test: PASS (7 choices persisted; reader reference preserved)");
	return 0;
}
