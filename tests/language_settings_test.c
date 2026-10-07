#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "main/app_language.h"
#include "main/xml.h"
#include <glib/gstdio.h>
#include <stdarg.h>
#include <stdio.h>

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
	gchar *directory = g_dir_make_tmp("elim-language-settings-XXXXXX", NULL);
	gchar *path = g_build_filename(directory, "settings.xml", NULL);
	gchar *current = g_strdup("fr_FR.UTF-8");
	const char *initial = "<Xiphos><locale><special>fr_FR.UTF-8</special></locale>"
		"<keys><verse>Juan 3:16</verse></keys><misc><reading_mode>1</reading_mode>"
		"</misc></Xiphos>";
	g_assert_true(g_file_set_contents(path, initial, -1, NULL));
	g_assert_true(xml_parse_settings_file(path));
	g_assert_false(app_language_save_choice("fr", &current, path));
	gchar *unchanged = NULL;
	g_assert_true(g_file_get_contents(path, &unchanged, NULL, NULL));
	g_assert_cmpstr(unchanged, ==, initial);
	g_free(unchanged);
	const char *choices[] = { "en_GB", "es", "fr", "ko_KR", "zh_CN", "pt_BR", "system" };
	for (guint i = 0; i < G_N_ELEMENTS(choices); i++) {
		g_assert_true(app_language_save_choice(choices[i], &current, path));
		g_assert_false(app_language_save_choice(choices[i], &current, path));
		gchar *saved = app_language_read_settings(path);
		g_assert_cmpstr(saved, ==, choices[i]);
		g_free(saved);
		gchar *verse = xml_get_value("keys", "verse");
		gchar *mode = xml_get_value("misc", "reading_mode");
		g_assert_cmpstr(verse, ==, "Juan 3:16");
		g_assert_cmpstr(mode, ==, "1");
		g_free(verse); g_free(mode);
	}
	g_assert_cmpstr(current, ==, "None");
	xml_free_settings_doc();
	gchar *backup = g_strconcat(path, ".SAVE", NULL);
	g_remove(backup); g_remove(path); g_rmdir(directory);
	g_free(backup); g_free(directory); g_free(path); g_free(current);
	puts("language_settings_test: PASS (7 persisted choices; legacy aliases; unrelated settings intact)");
	return 0;
}
