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

static guint
count_occurrences(const gchar *text, const gchar *needle)
{
	guint count = 0;

	while ((text = strstr(text, needle)) != NULL) {
		count++;
		text += strlen(needle);
	}

	return count;
}

int
main(void)
{
	gchar *gui = NULL;
	gchar *main_window = NULL;

	CHECK(g_file_get_contents(SRCDIR "/src/gtk/gui.c", &gui, NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/main_window.c", &main_window,
				  NULL, NULL));

	if (gui) {
		/* Process identity belongs to the one-time pre-GTK initializer. */
		CHECK(count_occurrences(gui,
					"g_set_application_name(\"Biblia Elim\");") == 1);
		CHECK(count_occurrences(gui,
					"g_set_prgname(\"biblia-elim\");") == 1);
	}
	if (main_window) {
		/* Window construction must not reset global process identity. */
		CHECK(strstr(main_window, "g_set_application_name(") == NULL);
		CHECK(strstr(main_window, "g_set_prgname(") == NULL);
		CHECK(strstr(main_window,
			     "gtk_window_set_title(GTK_WINDOW(widgets.app), "
			     "_(\"Biblia Elim\"));") != NULL);
		CHECK(strstr(main_window,
			     "gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "
			     "_(\"Biblia Elim\"));") != NULL);
	}

	g_free(gui);
	g_free(main_window);
	printf("application_name_startup_failures=%d\n", failures);
	return failures ? 1 : 0;
}
