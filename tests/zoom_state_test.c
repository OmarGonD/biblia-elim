#include "main/zoom_state.h"

#include <gtk/gtk.h>
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

int
main(void)
{
	ZoomState state;
	ZoomState restored;
	gchar *serialized;
	gchar *renderer = NULL;
	gchar *window = NULL;
	gchar *bible = NULL;
	gchar *parallel = NULL;
	gchar *commentary = NULL;
	gchar *dictionary = NULL;
	gchar *search = NULL;
	gchar *settings_source = NULL;
	gchar *reading_start;
	gchar *reading_end;
	gint i;
	GtkCssProvider *provider;
	GError *css_error = NULL;

	zoom_state_init(&state);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_BIBLE_MAIN) == 100);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_COMMENTARY) == 100);
	CHECK(zoom_state_active(&state) == ZOOM_SURFACE_BIBLE_MAIN);
	for (i = 0; i < ZOOM_SURFACE_COUNT; i++)
		CHECK(zoom_surface_from_name(zoom_surface_name((ZoomSurface)i)) == i);

	zoom_state_set(&state, ZOOM_SURFACE_BIBLE_MAIN, 125);
	zoom_state_set(&state, ZOOM_SURFACE_BIBLE_PARALLEL, 100);
	zoom_state_set(&state, ZOOM_SURFACE_COMMENTARY, 110);
	zoom_state_set(&state, ZOOM_SURFACE_DICTIONARY, 95);
	zoom_state_set(&state, ZOOM_SURFACE_LOWER_PREVIEWER, 105);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_BIBLE_MAIN) == 125);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_BIBLE_PARALLEL) == 100);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_COMMENTARY) == 110);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_DICTIONARY) == 95);

	/* Navigation, reload and reading-mode transitions never reconstruct this
	 * application-owned state. Changing focus only selects the next target. */
	zoom_state_set_active(&state, ZOOM_SURFACE_COMMENTARY);
	zoom_state_adjust(&state, zoom_state_active(&state), ZOOM_PERCENT_STEP);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_COMMENTARY) == 120);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_DICTIONARY) == 95);
	zoom_state_set_active(&state, ZOOM_SURFACE_BIBLE_MAIN);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_BIBLE_MAIN) == 125);

	/* In-session bible-main may be 125, but persistence always records 100%
	 * so the next launch starts at the shared baseline. */
	CHECK(zoom_surface_session_local(ZOOM_SURFACE_BIBLE_MAIN));
	CHECK(!zoom_surface_session_local(ZOOM_SURFACE_COMMENTARY));
	serialized = zoom_state_serialize(&state);
	CHECK(strstr(serialized, "bible-main=100") != NULL);
	CHECK(strstr(serialized, "bible-main=125") == NULL);
	CHECK(strstr(serialized, "bible-parallel=100") != NULL);
	CHECK(strstr(serialized, "commentary=120") != NULL);
	CHECK(strstr(serialized, "dictionary=95") != NULL);
	CHECK(strstr(serialized, "lower-previewer=105") != NULL);
	/* Live session value is unchanged by serialize. */
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_BIBLE_MAIN) == 125);
	zoom_state_deserialize(&restored, serialized);
	CHECK(zoom_state_get(&restored, ZOOM_SURFACE_BIBLE_MAIN) == 100);
	CHECK(zoom_state_get(&restored, ZOOM_SURFACE_COMMENTARY) == 120);
	CHECK(zoom_state_get(&restored, ZOOM_SURFACE_DICTIONARY) == 95);
	g_free(serialized);

	/* Missing/old configuration uses safe defaults. Unknown future keys are
	 * ignored by this version; malformed values do not overwrite defaults.
	 * A persisted bible-main=999 must not survive deserialize. */
	zoom_state_deserialize(&restored, NULL);
	CHECK(zoom_state_get(&restored, ZOOM_SURFACE_BIBLE_MAIN) == 100);
	zoom_state_deserialize(&restored,
		"bible-main=999;commentary=-20;dictionary=nope;future=175");
	CHECK(zoom_state_get(&restored, ZOOM_SURFACE_BIBLE_MAIN) == 100);
	CHECK(zoom_state_get(&restored, ZOOM_SURFACE_COMMENTARY) == 50);
	CHECK(zoom_state_get(&restored, ZOOM_SURFACE_DICTIONARY) == 100);
	CHECK(zoom_surface_from_name("future") == ZOOM_SURFACE_INVALID);
	CHECK(zoom_state_set(&restored, ZOOM_SURFACE_INVALID, 200) == 100);
	provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data(
	    provider,
	    "textview { font-size: 125%; } textview text { color: #111111; }",
	    -1, &css_error);
	CHECK(css_error == NULL);
	g_clear_error(&css_error);
	g_object_unref(provider);

	/* Headless integration contract: renderer reloads recompute CSS from the
	 * persistent state, real focus-in selects the target, and the main UI no
	 * longer invokes a global rerender for +/-/0. */
	CHECK(g_file_get_contents(SRCDIR "/src/webkit/wk-html.c", &renderer,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/main_window.c", &window,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/bibletext.c", &bible,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/parallel_view.c", &parallel,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/commentary.c", &commentary,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/dictlex.c", &dictionary,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/search_dialog.c", &search,
				  NULL, NULL));
	CHECK(renderer && strstr(renderer, "focus-in-event") != NULL);
	CHECK(renderer && strstr(renderer, "on_zoom_surface_focus") != NULL);
	CHECK(renderer && strstr(renderer, "apply_body_colors(html") != NULL);
	CHECK(renderer && strstr(renderer, "apply_surface_style(html)") != NULL);
	CHECK(renderer && strstr(renderer, "font-size: %d%%") != NULL);
	CHECK(renderer && strstr(renderer, "main_settings_zoom_adjust") != NULL);
	CHECK(renderer && strstr(renderer, "redisplay_to_realign()") == NULL);
	CHECK(window && strstr(window, "wk_html_zoom_active(TRUE)") != NULL);
	CHECK(window && strstr(window, "wk_html_zoom_active_reset()") != NULL);
	CHECK(bible && strstr(bible, "\"bible-main\"") != NULL);
	CHECK(parallel && strstr(parallel, "\"bible-parallel\"") != NULL);
	CHECK(commentary && strstr(commentary, "\"commentary\"") != NULL);
	CHECK(dictionary && strstr(dictionary, "\"dictionary\"") != NULL);
	CHECK(dictionary && strstr(dictionary, "\"devotional\"") != NULL);
	CHECK(search && strstr(search, "\"search-previewer\"") != NULL);
	CHECK(g_file_get_contents(SRCDIR "/src/main/settings.c", &settings_source,
				  NULL, NULL));
	CHECK(settings_source && strstr(settings_source,
			       "xml_set_or_create_value(\"fontsize\", \"surfacezoom\"") != NULL);
	reading_start = window ? strstr(window, "void gui_toggle_reading_mode(") : NULL;
	reading_end = reading_start ? strstr(reading_start,
					     "static void\nreading_mode_settle(") : NULL;
	CHECK(reading_start != NULL);
	CHECK(reading_end != NULL);
	if (reading_start && reading_end) {
		gchar *body = g_strndup(reading_start, reading_end - reading_start);
		CHECK(strstr(body, "zoom_state_init") == NULL);
		CHECK(strstr(body, "main_settings_zoom_set") == NULL);
		CHECK(strstr(body, "main_settings_zoom_adjust") == NULL);
		g_free(body);
	}
	g_free(renderer);
	g_free(window);
	g_free(bible);
	g_free(parallel);
	g_free(commentary);
	g_free(dictionary);
	g_free(search);
	g_free(settings_source);

	printf("zoom_state_failures=%d\n", failures);
	return failures ? 1 : 0;
}
