#include "gtk/zoom_indicator.h"

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

static void
check_indicator(ZoomState *state, ZoomSurface surface, gint expected_percent,
		const gchar *expected_text)
{
	gchar *text;

	zoom_state_set_active(state, surface);
	text = zoom_indicator_format(state,
				     zoom_indicator_surface_name(surface));
	CHECK(zoom_state_get(state, surface) == expected_percent);
	CHECK(g_strcmp0(text, expected_text) == 0);
	g_free(text);
}

int
main(void)
{
	ZoomState state;
	ZoomState restored;
	gchar *serialized;
	gchar *renderer = NULL;
	gchar *window = NULL;
	gint i;

	zoom_state_init(&state);
	zoom_state_set(&state, ZOOM_SURFACE_BIBLE_MAIN, 125);
	zoom_state_set(&state, ZOOM_SURFACE_BIBLE_PARALLEL, 95);
	zoom_state_set(&state, ZOOM_SURFACE_COMMENTARY, 110);
	zoom_state_set(&state, ZOOM_SURFACE_DICTIONARY, 90);

	check_indicator(&state, ZOOM_SURFACE_BIBLE_MAIN, 125,
			"Biblia principal · 125%");
	check_indicator(&state, ZOOM_SURFACE_BIBLE_PARALLEL, 95,
			"Biblia paralela · 95%");
	check_indicator(&state, ZOOM_SURFACE_COMMENTARY, 110,
			"Comentario · 110%");
	check_indicator(&state, ZOOM_SURFACE_DICTIONARY, 90,
			"Diccionario · 90%");

	/* Focusing is selection only; adjusting the focused commentary leaves
	 * every other percentage untouched and immediately changes its text. */
	zoom_state_set_active(&state, ZOOM_SURFACE_COMMENTARY);
	zoom_state_adjust(&state, zoom_state_active(&state), ZOOM_PERCENT_STEP);
	check_indicator(&state, ZOOM_SURFACE_COMMENTARY, 120,
			"Comentario · 120%");
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_BIBLE_MAIN) == 125);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_BIBLE_PARALLEL) == 95);
	CHECK(zoom_state_get(&state, ZOOM_SURFACE_DICTIONARY) == 90);

	serialized = zoom_state_serialize(&state);
	zoom_state_deserialize(&restored, serialized);
	check_indicator(&restored, ZOOM_SURFACE_BIBLE_MAIN, 125,
			"Biblia principal · 125%");
	check_indicator(&restored, ZOOM_SURFACE_COMMENTARY, 120,
			"Comentario · 120%");
	g_free(serialized);

	for (i = 0; i < ZOOM_SURFACE_COUNT; i++)
		CHECK(zoom_indicator_surface_name((ZoomSurface)i)[0] != '\0');
	CHECK(g_strcmp0(zoom_indicator_surface_name(ZOOM_SURFACE_INVALID),
			"Biblia principal") == 0);

	/* Headless integration contract: real renderer focus and every zoom path
	 * notify the one header indicator, while reading mode mirrors its text. */
	CHECK(g_file_get_contents(SRCDIR "/src/webkit/wk-html.c", &renderer,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/main_window.c", &window,
				  NULL, NULL));
	CHECK(renderer && strstr(renderer, "on_zoom_surface_focus") != NULL);
	CHECK(renderer && strstr(renderer, "notify_zoom_observer();") != NULL);
	CHECK(window && strstr(window,
			       "wk_html_set_zoom_observer(on_zoom_target_changed") != NULL);
	CHECK(window && strstr(window, "zoom_target_label") != NULL);
	CHECK(window && strstr(window, "reading_zoom_target_label") != NULL);
	/* A GtkHeaderBar side label must stay compressible.  width-chars creates
	 * a minimum request and can force GTK's centred title box negative while
	 * the fresh-profile window is receiving its initial allocation. */
	CHECK(window &&
	      strstr(window,
		     "gtk_label_set_width_chars(GTK_LABEL(zoom_target_label)") == NULL);
	g_free(renderer);
	g_free(window);

	printf("zoom_indicator_failures=%d\n", failures);
	return failures ? 1 : 0;
}
