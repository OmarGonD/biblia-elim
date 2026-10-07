#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "main/app_language.h"
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	/* Each language probe is a fresh process, as an app restart is. */
	if (argc >= 3 && !strcmp(argv[1], "--probe")) {
		app_language_initialize(!strcmp(argv[2], "saved") ? NULL : argv[2],
			argc == 4 ? argv[3] : NULL);
		g_print("%s\n%s\n%s\n", app_language_reader_locale(),
			_("Estudio bíblico"), _("Instalar Biblias"));
		return 0;
	}
	const struct { const char *input, *expected; } cases[] = {
		{ NULL, "system" }, { "", "system" }, { "None", "system" },
		{ "  None ", "system" }, { "en", "en_GB" },
		{ "en_US.UTF-8", "en_GB" }, { "es_PE.utf8", "es" },
		{ "fr_FR.UTF-8", "fr" }, { "ko-KR", "ko_KR" },
		{ "zh-Hans", "zh_CN" }, { "zh-Hant", "zh_TW" },
		{ "pt_BR.UTF-8", "pt_BR" }, { "pt_PT", "pt" },
		{ "C.UTF-8", "en_GB" }, { "de_DE.UTF-8", "de_DE" }
	};
	for (gsize i = 0; i < G_N_ELEMENTS(cases); i++) {
		gchar *value = app_language_normalize(cases[i].input);
		g_assert_cmpstr(value, ==, cases[i].expected);
		g_free(value);
	}
	gsize count;
	const AppLanguage *languages = app_languages(&count);
	g_assert_cmpuint(count, >=, 6);
	for (gsize i = 0; i < count; i++)
		g_assert_true(g_utf8_validate(languages[i].native_name, -1, NULL));

	char *arguments[] = { "biblia-elim", "--language=ko", "--backend=sqlite:/tmp/modules",
		"--language=fr", "sword://KJV/John.3.16", NULL };
	int n = 5;
	gchar *choice = NULL;
	GError *error = NULL;
	g_assert_true(app_language_parse_args(&n, arguments, &choice, &error));
	g_assert_no_error(error);
	g_assert_cmpstr(choice, ==, "fr");
	g_assert_cmpint(n, ==, 3);
	g_assert_cmpstr(arguments[1], ==, "--backend=sqlite:/tmp/modules");
	g_assert_cmpstr(arguments[2], ==, "sword://KJV/John.3.16");
	g_assert_null(arguments[3]);
	g_free(choice); choice = NULL;
	char *invalid[] = { "biblia-elim", "--language=not-a-language", NULL };
	n = 2;
	g_assert_false(app_language_parse_args(&n, invalid, &choice, &error));
	g_assert_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE);
	g_clear_error(&error);
	g_assert_null(choice);
	char *empty[] = { "biblia-elim", "--language=", NULL };
	n = 2;
	g_assert_false(app_language_parse_args(&n, empty, &choice, &error));
	g_clear_error(&error);

	gchar *directory = g_dir_make_tmp("elim-language-XXXXXX", NULL);
	gchar *path = g_build_filename(directory, "settings.xml", NULL);
	g_assert_null(app_language_read_settings(path));
	const char *xml = "<?xml version=\"1.0\"?><Xiphos><locale><special>ko_KR.UTF-8</special>"
		"</locale><keys><verse>Juan 3:16</verse></keys></Xiphos>";
	g_assert_true(g_file_set_contents(path, xml, -1, NULL));
	choice = app_language_read_settings(path);
	g_assert_cmpstr(choice, ==, "ko_KR");
	g_free(choice);
	gchar *contents = NULL;
	g_assert_true(g_file_get_contents(path, &contents, NULL, NULL));
	g_assert_cmpstr(contents, ==, xml);
	g_free(contents);
	g_assert_true(g_file_set_contents(path, "<Xiphos><locale>", -1, NULL));
	g_assert_null(app_language_read_settings(path));
	g_assert_true(g_file_set_contents(path, "<Other><locale><special>fr</special></locale></Other>", -1, NULL));
	g_assert_null(app_language_read_settings(path));
	g_remove(path); g_rmdir(directory); g_free(path); g_free(directory);
	puts("app_language_test: PASS (aliases, arguments, legacy preference, UTF-8)");
	return 0;
}
