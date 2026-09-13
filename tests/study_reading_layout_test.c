/*
 * Standard-view reading column is a capped, centred block inside the
 * Bible text view. Parallel view is a different widget and must not
 * inherit that cap.
 */
#include <glib.h>
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
	gchar *window = NULL;
	gchar *parallel = NULL;

	CHECK(g_file_get_contents(SRCDIR "/src/gtk/main_window.c", &window,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/parallel_view.c", &parallel,
				  NULL, NULL));

	if (window) {
		CHECK(strstr(window, "study_reading_apply_measure") != NULL);
		CHECK(strstr(window, "main_study_reading_column") != NULL);
		CHECK(strstr(window, "bible_text_on_size_allocate") != NULL);
		CHECK(strstr(window, "bible_text_bind_measure") != NULL);
		/* Leaving reading mode must not drop the allocation handler;
		 * standard view also recentres on panel open/close. */
		CHECK(strstr(window,
			     "g_signal_handler_disconnect(view, reading_mode_alloc_id)") ==
		      NULL);
		/* Verses stay left-aligned; we only set symmetric margins. */
		CHECK(strstr(window, "gtk_text_view_set_justification") == NULL);
	}
	if (parallel) {
		CHECK(strstr(parallel, "study_reading_apply_measure") == NULL);
		CHECK(strstr(parallel, "STUDY_READING_COLUMN_MAX") == NULL);
		CHECK(strstr(parallel, "\"bible-parallel\"") != NULL);
	}

	g_free(window);
	g_free(parallel);
	printf("study_reading_layout_failures=%d\n", failures);
	return failures ? 1 : 0;
}
