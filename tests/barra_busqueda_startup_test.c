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
	gchar *source = NULL;

	CHECK(g_file_get_contents(SRCDIR "/src/gtk/barra_busqueda.c", &source,
				  NULL, NULL));
	if (source) {
		/* Collapsed in-chapter search must stay out of startup show_all
		 * and the first GTK event drain; Ctrl-F shows it explicitly. */
		CHECK(strstr(source,
			     "gtk_widget_set_no_show_all(barra, TRUE);") != NULL);
		CHECK(count_occurrences(source, "gtk_widget_hide(barra);") == 2);
		CHECK(count_occurrences(source, "gtk_widget_show(barra);") == 1);
		CHECK(strstr(source,
			     "gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(barra), TRUE);") !=
		      NULL);
		CHECK(strstr(source,
			     "gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(barra), FALSE);") !=
		      NULL);
		/* Create must not map the bar into the startup tree. */
		CHECK(strstr(source,
			     "gtk_widget_show_all(caja);\n"
			     "\tgtk_widget_show(barra);") == NULL);
	}

	g_free(source);
	printf("barra_busqueda_startup_failures=%d\n", failures);
	return failures ? 1 : 0;
}
