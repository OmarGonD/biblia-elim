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
	gchar *commentary = NULL;
	gchar *notes = NULL;

	CHECK(g_file_get_contents(SRCDIR "/src/gtk/main_window.c", &window,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/commentary.c", &commentary,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/gtk/notas_verso.c", &notes,
				  NULL, NULL));

	if (window) {
		CHECK(strstr(window, "void gui_close_comms_panel(void)") != NULL);
		CHECK(strstr(window, "gtk_notebook_set_action_widget") != NULL);
		CHECK(strstr(window, "Cerrar panel de comentarios y notas") !=
		      NULL);
		CHECK(strstr(window, "comms_panel_close_button") != NULL);
		CHECK(strstr(window, "gui_schedule_bible_text_reflow") != NULL);
		CHECK(strstr(window, "main_study_hpaned_available_width") !=
		      NULL);
		CHECK(strstr(window, "gtk_widget_set_no_show_all(widgets.vpaned2") !=
		      NULL);
	}
	if (commentary) {
		CHECK(strstr(commentary, "window-close-symbolic") != NULL);
		CHECK(strstr(commentary, "gui_close_comms_panel") != NULL);
		CHECK(strstr(commentary, "comm-panel-close") != NULL);
	}
	if (notes) {
		CHECK(strstr(notes, "gui_close_comms_panel") != NULL);
		CHECK(strstr(notes, "Cerrar panel de notas") != NULL);
	}

	g_free(window);
	g_free(commentary);
	g_free(notes);
	printf("comm_panel_close_failures=%d\n", failures);
	return failures ? 1 : 0;
}
