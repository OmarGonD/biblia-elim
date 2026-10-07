/* Test the installer's private labels and search with the actual SWORD
 * language name (한국말), without a display or a downloaded Bible. */
#include "../src/gtk/instalar_biblias.c"

int main(void)
{
	const char *languages[] = { "한국말", "한국어", "조선말", "Korean",
		"korean", "Coreano", "coreano", "ko", "KO", "kor" };
	gchar *needle = g_utf8_casefold("COREANO", -1);
	for (guint i = 0; i < G_N_ELEMENTS(languages); i++) {
		g_assert_cmpstr(lang_label(languages[i]), ==, "Coreano");
		g_assert_true(utf_contains(lang_label(languages[i]), needle));
	}
	g_free(needle);
	g_assert_true(utf_contains("개역성경", "개역"));
	g_assert_false(utf_contains("개역성경", "없는문자"));
	g_assert_cmpstr(lang_label("Spanish"), ==, "Español");
	g_assert_cmpstr(lang_label("English"), ==, "Inglés");
	g_assert_cmpstr(lang_label("中文"), ==, "Chino");
	g_assert_cmpstr(lang_label(NULL), ==, "Desconocido");
	g_assert_cmpstr(lang_label(""), ==, "Desconocido");
	g_assert_cmpstr(lang_label("Unknown language"), ==, "Unknown language");
	g_assert_false(utf_contains(lang_label("Spanish"), "coreano"));
	g_print("korean_catalog_test: PASS (10 aliases; Korean/Spanish search)\n");
	return 0;
}
