/* Exercise the actual first-run installer without network or user files.
 * Include the implementation to reach its private bootstrap function; the
 * target discards unrelated settings/UI sections at link time. */
#include "../src/main/settings.c"

#include <stdarg.h>

static GPtrArray *installed;
static int refresh_result, initialized, terminated, shut_down, sources;

gchar *XI_g_strdup_printf(const char *file, int line, const gchar *format, ...)
{
	(void)file;
	(void)line;
	va_list args;
	va_start(args, format);
	gchar *result = g_strdup_vprintf(format, args);
	va_end(args);
	return result;
}

void main_ensure_remote_sources(void) { sources++; }

void mod_mgr_init(const char *dir, gboolean augment, gboolean regular)
{
	g_assert_cmpstr(dir, ==, "/unused-korean-test/.sword");
	g_assert_true(augment);
	g_assert_true(regular);
	initialized++;
}

int mod_mgr_refresh_remote_source(const char *source)
{
	g_assert_cmpstr(source, ==, "CrossWire");
	return refresh_result;
}

int mod_mgr_remote_install(const char *dir, const char *source, const char *id)
{
	g_assert_cmpstr(dir, ==, "/unused-korean-test/.sword");
	g_assert_cmpstr(source, ==, "CrossWire");
	g_ptr_array_add(installed, g_strdup(id));
	/* Failure of one edition must not suppress the remaining defaults. */
	return !strcmp(id, "KorRV") ? -1 : 0;
}

void mod_mgr_terminate(void) { terminated++; }
void mod_mgr_shut_down(void) { shut_down++; }

int main(void)
{
	const char *expected[] = { "KJV", "SpaRV", "SpaPlatense", "SpaRVG",
		"KorRV", "KorHKJV", "WLC", "Tisch", "TR", "VulgClementine", "TSK" };
	settings.homedir = "/unused-korean-test";
	installed = g_ptr_array_new_with_free_func(g_free);
	main_bootstrap_default_modules();
	g_assert_cmpuint(installed->len, ==, G_N_ELEMENTS(expected));
	for (guint i = 0; i < G_N_ELEMENTS(expected); i++)
		g_assert_cmpstr(g_ptr_array_index(installed, i), ==, expected[i]);
	g_assert_cmpint(initialized, ==, 1);
	g_assert_cmpint(sources, ==, 1);
	g_assert_cmpint(terminated, ==, 1);
	g_assert_cmpint(shut_down, ==, 1);

	/* No catalog: no partial installs, with normal manager cleanup. */
	g_ptr_array_set_size(installed, 0);
	refresh_result = -1;
	main_bootstrap_default_modules();
	g_assert_cmpuint(installed->len, ==, 0);
	g_assert_cmpint(initialized, ==, 2);
	g_assert_cmpint(sources, ==, 2);
	g_assert_cmpint(terminated, ==, 2);
	g_assert_cmpint(shut_down, ==, 2);
	g_ptr_array_free(installed, TRUE);
	puts("default_bibles_test: PASS (11 modules; install/catalog failures)");
	return 0;
}
