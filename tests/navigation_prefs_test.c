/*
 * The reader's wheel and reading-focus preferences through settings.xml,
 * with the app's own xml.c on a temporary file: never the real settings.
 */
#include "main/navigation_prefs.h"
#include "main/wheel_scroll.h"
#include "main/xml.h"

#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                 \
	} while (0)

/* A settings.xml from before these settings existed. */
static const char *old_settings =
    "<?xml version=\"1.0\"?>\n"
    "<Xiphos>\n"
    "  <misc><reading_mode>0</reading_mode><reading_mode_window>5</reading_mode_window></misc>\n"
    "  <keys><verse>Mateo 5:22</verse></keys>\n"
    "</Xiphos>\n";

static void
test_parse(void)
{
	NavigationPrefs p;

	p = navigation_prefs_parse(NULL, NULL);
	CHECK(p.wheel_percent == 50 && p.focus_mode == READING_FOCUS_BALANCED);
	p = navigation_prefs_parse("35", "stable");
	CHECK(p.wheel_percent == 35 && p.focus_mode == READING_FOCUS_STABLE);
	p = navigation_prefs_parse("100", "immediate");
	CHECK(p.wheel_percent == 100 && p.focus_mode == READING_FOCUS_IMMEDIATE);
	/* hand-edited or damaged values */
	CHECK(navigation_prefs_parse("37", NULL).wheel_percent == 35);
	CHECK(navigation_prefs_parse("38", NULL).wheel_percent == 40);
	CHECK(navigation_prefs_parse("5", NULL).wheel_percent == 25);
	CHECK(navigation_prefs_parse("500", NULL).wheel_percent == 100);
	CHECK(navigation_prefs_parse("abc", NULL).wheel_percent == 50);
	CHECK(navigation_prefs_parse("0,5", NULL).wheel_percent == 50);
	CHECK(navigation_prefs_parse("", "").focus_mode == READING_FOCUS_BALANCED);
	CHECK(navigation_prefs_parse(NULL, "weird").focus_mode == READING_FOCUS_BALANCED);
	CHECK(!strcmp(navigation_prefs_focus_mode_name(READING_FOCUS_STABLE), "stable"));
}

int
main(void)
{
	gchar *dir = g_dir_make_tmp("navigation-prefs-XXXXXX", NULL);
	gchar *path = g_build_filename(dir, "settings.xml", NULL);
	gchar *before = NULL, *after = NULL;
	NavigationPrefs loaded, wanted = { 35, READING_FOCUS_STABLE };

	test_parse();

	CHECK(g_file_set_contents(path, old_settings, -1, NULL));

	/* no settings yet: exactly the behaviour of before */
	CHECK(xml_parse_settings_file(path)); /* TRUE when parsed */
	loaded = navigation_prefs_load_settings();
	CHECK(loaded.wheel_percent == 50);
	CHECK(loaded.focus_mode == READING_FOCUS_BALANCED);
	CHECK(wheel_scroll_get_distance_scale() == 0.50);
	CHECK(reading_focus_get_mode() == READING_FOCUS_BALANCED);
	/* and loading writes nothing */
	CHECK(g_file_get_contents(path, &before, NULL, NULL));
	CHECK(before && !strcmp(before, old_settings));

	/* save 35 % / stable, reload from the file */
	navigation_prefs_save_settings(&wanted, path);
	xml_free_settings_doc();
	CHECK(g_file_get_contents(path, &after, NULL, NULL));
	CHECK(after && strstr(after, "<wheel_scroll_percent>35</wheel_scroll_percent>"));
	CHECK(after && strstr(after, "<reading_focus_mode>stable</reading_focus_mode>"));
	/* the rest of the file is still there */
	CHECK(after && strstr(after, "<reading_mode_window>5</reading_mode_window>"));

	/* as at the next start: defaults in memory, then the file */
	{
		NavigationPrefs defaults = navigation_prefs_defaults();
		navigation_prefs_apply(&defaults);
	}
	CHECK(xml_parse_settings_file(path)); /* TRUE when parsed */
	loaded = navigation_prefs_load_settings();
	CHECK(loaded.wheel_percent == 35);
	CHECK(loaded.focus_mode == READING_FOCUS_STABLE);
	CHECK(ABS(wheel_scroll_get_distance_scale() - 0.35) < 1e-9);
	CHECK(reading_focus_get_mode() == READING_FOCUS_STABLE);
	CHECK(navigation_prefs_current().wheel_percent == 35);

	/* saved again with the same values: one entry each, not duplicates */
	navigation_prefs_save_settings(&wanted, path);
	xml_free_settings_doc();
	g_free(after);
	CHECK(g_file_get_contents(path, &after, NULL, NULL));
	{
		const char *first = after ? strstr(after, "<wheel_scroll_percent>") : NULL;
		CHECK(first && !strstr(first + 1, "<wheel_scroll_percent>"));
	}

	g_free(before);
	g_free(after);
	g_unlink(path);
	g_rmdir(dir);
	g_free(path);
	g_free(dir);
	printf("navigation_prefs_failures=%d\n", failures);
	return failures ? 1 : 0;
}
